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

#include <stdlib.h>
#include <string.h>

#include <opusfile.h>

#include "poppy.h"
#include "track.h"
#include "opus_track.h"
#include "def.h"
#include "opus_error.h"
#include "ch_map.h"

int opus_read_callback (void *_stream, unsigned char *ptr, int nbytes) {
	opus_stream *stream = _stream;
	if (stream->index >= stream->length) {
		return 0;
	}
	if (stream->index + nbytes > stream->length) {
		nbytes = stream->length - stream->index;
	}
	for (;;) {
		int n = fread(ptr, 1, nbytes, stream->file);
		if (n > 0) {
			stream->index += n;
			return n;
		}
		if (feof(stream->file)) return 0;
		if (ferror(stream->file)) return -1;
	}
}
 
int opus_seek_callback (void *_stream, opus_int64 offset, int whence) {
	opus_stream *stream = _stream;
	long real_offset;
	long from_zero;
	switch (whence) {
	case SEEK_SET:
		real_offset = offset - stream->index;
		stream->index = offset;
		break;
	case SEEK_CUR:
		real_offset = offset;
		stream->index += offset;
		break;
	case SEEK_END:
		from_zero = stream->length + offset;
		real_offset = from_zero - stream->index;
		stream->index = from_zero;
		break;
	}
	if (real_offset < -stream->length) {
		real_offset = -stream->length;
		stream->index = 0;
	} else if (real_offset > stream->length) {
		real_offset = stream->length - stream->index;
		stream->index = stream->length;
	}
	return fseek(stream->file, real_offset, SEEK_CUR);
}
 
opus_int64 opus_tell_callback (void *_stream) {
	opus_stream *stream = _stream;
	return stream->index;
}
 
int opus_close_callback (void *_stream) {
	opus_stream *stream = _stream;
	return fclose(stream->file);
}

OpusFileCallbacks opus_file_callbacks = {
	.read  = opus_read_callback,
	.seek  = opus_seek_callback,
	.tell  = opus_tell_callback,
	.close = opus_close_callback,
};

track_state opus_track_state(track_i *this) {
	opus_track *track = (opus_track*) this;
	return track->state;
}

track_meta opus_track_meta(track_i *this) {
	opus_track *track = (opus_track*) this;
	return track->meta;
}

int opus_track_dec(track_i *this, float *pcm, int samples) {
	opus_track *track = (opus_track*) this;
	int opus_chn = track->meta.channels;
	int ret;
retry:
	ret = op_read_float(track->file, pcm, opus_chn*samples, NULL);
	if (ret < 0) {
		fprintf(stderr, "op_read_float: %s\n",
			stropuserror(ret));
		switch (ret) {
		case OP_HOLE: case OP_EREAD: case OP_EBADPACKET:
			goto retry;
		case OP_EINVAL:
			ret = op_test_open(track->file);
			fprintf(stderr, "op_test_open: %s\n",
				stropuserror(ret));
		default:
			return -1;
		}
	}
	for (int os = ret-1; os >= 0; os--) {
		float frame[9] = { 0 };
		for (int ch = 0; ch < opus_chn; ch++) {
			frame[vorbis_vorbis81_ch_map[opus_chn][ch]] = pcm[opus_chn*os+ch];
		}
		memcpy(&pcm[stream_channel_cnt*os], frame, sizeof frame);
	}
	ogg_int64_t pcm_tell = op_pcm_tell(track->file);
	track->state.time = pcm_tell / 48e3;
	return ret;
}

int opus_track_seek(track_i *this, float offset, int whence) {
	opus_track *track = (opus_track*) this;
	float real_offset;
	switch (whence) {
	case SEEK_SET:
		track->state.time = offset;
		real_offset = offset;
		break;
	case SEEK_CUR:
		track->state.time += offset;
		real_offset = track->state.time;
		break;
	case SEEK_END:
		track->state.time = track->meta.length + offset;
		real_offset = track->state.time;
		break;
	}
	if (real_offset < 0) {
		track->state.time = 0;
		real_offset = 0;
	} else if (real_offset >= track->meta.length) {
		track->state.time = track->meta.length;
		real_offset = track->meta.length;
	}
	return op_pcm_seek(track->file, real_offset * 48e3);
}

const int opus_gain_type_table[] = {
	[header_gain]   = OP_HEADER_GAIN,
	[album_gain]    = OP_ALBUM_GAIN,
	[track_gain]    = OP_TRACK_GAIN,
	[absolute_gain] = OP_ABSOLUTE_GAIN,
};

int opus_track_gain(track_i *this, float gain, int whence) {
	opus_track *track = (opus_track*) this;
	switch (whence) {
	case SEEK_SET: track->state.gain = gain; break;
	case SEEK_CUR: track->state.gain += gain; break;
	}
	return op_set_gain_offset(
		track->file,
		opus_gain_type_table[track->state.gain_type],
		track->state.gain * 256
	);
}

int opus_track_gain_type(track_i *this, enum gain_type gain_type) {
	opus_track *track = (opus_track*) this;
	track->state.gain_type = gain_type;
	return op_set_gain_offset(
		track->file,
		opus_gain_type_table[gain_type],
		track->state.gain
	);
}

static void free_if_null(const void *ptr) {
	if (ptr) free((void *) ptr);
}

int opus_track_close(track_i *this) {
	opus_track *track = (opus_track*) this;
	free_if_null(track->meta.artist);
	free_if_null(track->meta.album);
	free_if_null(track->meta.title);
	free_if_null(track->meta.tracknumber);
	free_if_null(track->meta.tracktotal);
	op_free(track->file);
	return 0;
}

const track_i opus_track_vtable = {
	.state = opus_track_state,
	.meta  = opus_track_meta,
	.dec   = opus_track_dec,
	.seek  = opus_track_seek,
	.gain  = opus_track_gain,
	.gain_type = opus_track_gain_type,
	.close = opus_track_close,
};

static void copy_tag(const char **dest, const OpusTags *tags, const char *tag) {
	if (*dest) return;
	const char *value = opus_tags_query(tags, tag, 0);
	if (!value) {
		*dest = NULL;
		return;
	}
	int len = strlen(value);
	char *copy = malloc(len+1);
	memcpy(copy, value, len);
	copy[len] = '\0';
	*dest = copy;
}

int opus_track_from_file(
	opus_track *track,
	const char *filename,
	long link_start,
	long link_end
) {
	*track = (opus_track) { 0 };
	track->track_i = opus_track_vtable;

	if (link_start == 0 && link_end == -1) {
		int operr;
		track->file = op_open_file(filename, &operr);
		if (operr != 0) {
			fprintf(stderr,
				"op_open_file: %s: %s\n",
				filename, stropuserror(operr));
			return -1;
		}
	}
	else {
		FILE *file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "fopen: %s: ", filename);
			perror("");
			return -1;
		}
		fseek(file, link_start, SEEK_CUR);
		track->stream = (opus_stream) {
			.file   = file,
			.index  = 0,
			.length = link_end,
		};
		int operr;
		track->file = op_open_callbacks(
			&track->stream,
			&opus_file_callbacks,
			NULL, 0,
			&operr
		);
		if (operr != 0) {
			fprintf(stderr,
				"op_open_callbacks: %s: %s\n",
				filename, stropuserror(operr));
			return -1;
		}
	}

	track->meta.codec = OPUS;

	const OpusHead *head = op_head(track->file, -1);
	track->meta.channels = head->channel_count;
	track->meta.sample_rate = head->input_sample_rate;

	ogg_int64_t pcm_total = op_pcm_total(track->file, -1);
	track->meta.length = pcm_total / 48e3;

	track->meta.bit_rate = op_bitrate(track->file, -1);

	const OpusTags *tags = op_tags(track->file, -1);
	copy_tag(&track->meta.artist, tags, "artist");
	copy_tag(&track->meta.album, tags, "album");
	copy_tag(&track->meta.title, tags, "title");
	copy_tag(&track->meta.tracknumber, tags, "tracknumber");
	copy_tag(&track->meta.tracktotal, tags, "tracktotal");

	return 0;
}
