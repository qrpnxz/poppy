/* SPDX-License-Identifier: GPL-3.0-or-later

Copyright 2021 Russell Hernandez Ruiz <qrpnxz@hyperlife.xyz>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <threads.h>

#include <unistd.h>

#include <pulse/pulseaudio.h>

#include "poppy.h"
#include "def.h"
#include "opus_error.h"

#include "track.h"
#include "opus_track.h"
#include "vorbis_track.h"
#include "flac_track.h"
#include "ch_map.h"

#include "dbus.h"

const int stream_sample_rate = 48000;
const int stream_channel_cnt = vorbis_8_1_surround;

typedef struct stream_ud {
	pa_mainloop_api *api;
	struct player *player;
} stream_ud;

void play(pa_stream *stream, size_t bytes, void *userdata) {
	stream_ud *ud = userdata;
	pa_mainloop_api *api = ud->api;
	struct player *player = ud->player;
	struct playlist *pl   = &player->pl;
	track_i *track;
	track_state state;
	track_meta meta;
cont:
	while (bytes > 0) {
		float *pcm;
		size_t buf_bytes = bytes;
		pa_stream_begin_write(stream, (void**) &pcm, &buf_bytes);
		memset(pcm, 0, buf_bytes);
		bool eot = false;
		int s = buf_bytes / sizeof (float);
		int spch = s / stream_channel_cnt;
		int ts = 0;
		do {
			mtx_lock(&player->lock);
			track_i *track = pl->track[pl->curr];
			track->gain(track, player->gain, SEEK_SET);
			track->gain_type(track, player->gain_type);
			int sd = track->dec(track, pcm+stream_channel_cnt*ts, spch-ts);
			mtx_unlock(&player->lock);
			if (sd < 0) api->quit(api, 1);
			if (sd == 0) eot = true;
			ts += sd;
		} while (ts < spch && !eot);
		pa_stream_write(
			stream,
			pcm, buf_bytes,
			NULL,
			0, PA_SEEK_RELATIVE
		);
		bytes -= buf_bytes;
		if (eot) break;
	}
	mtx_lock(&player->lock);
	track = pl->track[pl->curr];
	state = track->state(track);
	meta = track->meta(track);
	if (state.time < meta.length) {
		mtx_unlock(&player->lock);
		return;
	}
	track->seek(track, 0, SEEK_SET);
	switch (player->play_mode) {
	case playlist:
		pl->curr++;
		if (pl->curr >= pl->size) {
			api->quit(api, 0);
			mtx_unlock(&player->lock);
			return;
		}
		break;
	case repeat:
		pl->curr++;
		pl->curr %= pl->size;
		break;
	case repeat_one: break;
	case single:
		api->quit(api, 0);
		mtx_unlock(&player->lock);
		return;
	}
	mtx_unlock(&player->lock);
	if (bytes > 0) goto cont;
}

typedef struct ctx_ud {
	pa_mainloop_api *api;
	struct player *player;
} ctx_ud;

void ctx_state_cb(pa_context *ctx, void *userdata) {
	ctx_ud *ud = userdata;
	pa_mainloop_api *api = ud->api;
	switch (pa_context_get_state(ctx)) {
	case PA_CONTEXT_CONNECTING:   //puts("pa_context connecting"); return;
	case PA_CONTEXT_AUTHORIZING:  //puts("pa_context authenticating"); return;
	case PA_CONTEXT_SETTING_NAME: //puts("pa_context setting name"); return;
		return;
	case PA_CONTEXT_READY: {
		//puts("pa_context ready");
		pa_sample_spec spec = (pa_sample_spec) {
			.format   = PA_SAMPLE_FLOAT32LE,
			.rate     = stream_sample_rate,
			.channels = stream_channel_cnt,
		};
		assert(pa_sample_spec_valid(&spec));
		pa_stream *stream = pa_stream_new(
			ctx, "Poppy", &spec, &vorbis_pa_ch_map[stream_channel_cnt]);
		assert(stream != NULL);
		stream_ud *sud = calloc(1, sizeof *sud);
		sud->api    = api;
		sud->player = ud->player;
		sud->player->stream = stream;
		mtx_unlock(&sud->player->lock);
		pa_stream_set_write_callback(stream, play, sud);
		assert(pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL) == 0);
		return;
	}
	case PA_CONTEXT_TERMINATED: puts("pa_context terminated"); break;
	case PA_CONTEXT_FAILED:     puts("pa_context failed"); break;
	default: break;
    }
	pa_stream_unref(ud->player->stream);
	free(ud);
	api->quit(api, 0);
}

// void print_help(const char *cmd) {
// 	fprintf(stderr, "%s [-h|-g|-a|-t|-b] <oggopus>+\n\n", cmd);
// 	fprintf(stderr, "\t-h\tprint this message\n");
// 	fprintf(stderr, "\t-g#\tset gain to # dB\n");
// 	fprintf(stderr, "\t-a\tadd album gain\n");
// 	fprintf(stderr, "\t-t\tadd track gain\n");
// 	fprintf(stderr, "\t-b\tremove default gain\n");
// }

// void parse_opt(const char *arg) {
// 	int len = strlen(arg);
// 	if (len < 2) return;
// 	switch (arg[1]) {
// 	default:
// 	case 'h': print_help("poppy"); exit(0);
// 	case 'g': gain = 256*strtof(&arg[2], NULL); break;
// 	case 'a': gain_type = OP_ALBUM_GAIN; break;
// 	case 't': gain_type = OP_TRACK_GAIN; break;
// 	case 'b': gain_type = OP_ABSOLUTE_GAIN; break;
// 	}
// }

int main(int argc, char **argv) {
	track_i **all_tracks = NULL;
	int total_tracks = 0;
	for (int i = 1; i < argc; i++) {
		track_i **tracks = NULL;
		int n = tracks_from_file(&tracks, argv[i]);
		if (n <= 0) abort(); //TODO: proper error
		all_tracks = realloc(all_tracks,
			(total_tracks+n) * sizeof *all_tracks);
		memcpy(&all_tracks[total_tracks], tracks,
			n * sizeof *tracks);
		free(tracks);
		total_tracks += n;
	}

	struct player *player = calloc(1, sizeof *player);
	mtx_init(&player->lock, mtx_plain);
	mtx_lock(&player->lock);
	struct playlist *pl = &player->pl;
	pl->size  = total_tracks;
	pl->track = all_tracks;

	pa_mainloop *loop = pa_mainloop_new();
	pa_mainloop_api *api = pa_mainloop_get_api(loop);

	pa_context *ctx = pa_context_new(api, "poppy");
	ctx_ud *ud = calloc(1, sizeof *ud);
	ud->api    = api;
	ud->player = player;
	pa_context_set_state_callback(ctx, ctx_state_cb, ud);
	assert(pa_context_connect(ctx, NULL, 0, NULL) >= 0);

	thrd_t dbus;
	int ret = thrd_create(&dbus, dbus_main, player);
	if (ret != thrd_success) {
		fprintf(stderr, "unable to start dbus thread\n");
	} else {
		thrd_detach(dbus);
	}

	int runret;
	int curr_track = -1;
	while (pa_mainloop_iterate(loop, 1, &runret) >= 0) {
		track_i *track = pl->track[pl->curr];
		track_state state = track->state(track);
		track_meta meta = track->meta(track);
		if (curr_track != pl->curr) {
			curr_track = pl->curr;
			fputc('\n', stdout);
			printf(" Audio: %dch %dbit @ %gkhz @ %gkbps\n",
				meta.channels,
				meta.bit_depth,
				meta.sample_rate / 1e3,
				meta.bit_rate / 1e3
			);
			if (meta.artist) printf("Artist: %s\n", meta.artist);
			if (meta.album)  printf(" Album: %s\n", meta.album);
			if (meta.title)  printf(" Title: %s\n", meta.title);
		}
		fputc('\r', stdout);
		if (meta.tracknumber) {
			printf("%s", meta.tracknumber);
			if (meta.tracktotal) printf("/%s ", meta.tracktotal);
			else printf(" ");
		}
		double now = state.time;
		double remaining = meta.length - now;
		double min, sec;
		sec = modf(now/60, &min)*60;
		printf("[%02.0f:%05.2f/", min, sec);
		sec = modf(meta.length/60, &min)*60;
		printf("%02.0f:%05.2f/", min, sec);
		remaining = meta.length - now;
		sec = modf(remaining/60, &min)*60;
		printf("%02.0f:%05.2f]", min, sec);
		fflush(stdout);
	}
	fputc('\n', stdout);

	pa_context_unref(ctx);
	pa_mainloop_free(loop);
	return runret;
}
