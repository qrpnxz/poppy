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

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <vorbis/vorbisfile.h>

#include "track.h"
#include "vorbis_track.h"
#include "def.h"
#include "ch_map.h"
#include "poppy.h"

const char *strvorbiserror(int err) {
	static const char *table[] = {
		[-OV_FALSE     ] = "OV_FALSE",
		[-OV_EOF       ] = "OV_EOF",
		[-OV_HOLE      ] = "There was an interruption in the data.",
		[-OV_EREAD     ] = "A read from media returned an error.",
		[-OV_EFAULT    ] = "Internal logic fault; indicates a bug or heap/stack corruption.",
		[-OV_EIMPL     ] = "OV_EIMPL",
		[-OV_EINVAL    ] = "OV_EINVAL",
		[-OV_ENOTVORBIS] = "Bitstream does not contain any Vorbis data.",
		[-OV_EBADHEADER] = "OV_EBADHEADER",
		[-OV_EVERSION  ] = "Vorbis version mismatch.",
		[-OV_ENOTAUDIO ] = "OV_ENOTAUDIO",
		[-OV_EBADPACKET] = "Invalid Vorbis bitstream packet.",
		[-OV_EBADHEADER] = "Invalid Vorbis bitstream header.",
		[-OV_EBADLINK  ] = "Invalid stream section was supplied to libvorbisfile, or the requested link is corrupt.",
		[-OV_ENOSEEK   ] = "OV_ENOSEEK",
	};
	return table[-err];
}

size_t vorbis_read_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
	vorbis_stream *stream = datasource;
	if (stream->index >= stream->length) {
		return 0;
	}
	if (stream->index + size*nmemb > stream->length) {
		nmemb = stream->length - stream->index;
		nmemb /= size;
	}
	for (;;) {
		int n = fread(ptr, size, nmemb, stream->file);
		if (n > 0) {
			stream->index += n;
			return n;
		}
		if (feof(stream->file)) return 0;
		if (ferror(stream->file)) return -1;
	}
}

int vorbis_seek_callback(void *datasource, ogg_int64_t offset, int whence) {
	vorbis_stream *stream = datasource;
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

int vorbis_close_callback(void *datasource) {
	vorbis_stream *stream = datasource;
	return fclose(stream->file);
}

long vorbis_tell_callback(void *datasource) {
	vorbis_stream *stream = datasource;
	return stream->index;
}

ov_callbacks vorbis_file_callbacks = {
	.read_func = vorbis_read_callback,
	.seek_func = vorbis_seek_callback,
	.close_func = vorbis_close_callback,
	.tell_func = vorbis_tell_callback,
};

track_state vorbis_track_state(track_i *this) {
	vorbis_track *track = (vorbis_track*) this;
	return track->state;
}

track_meta vorbis_track_meta(track_i *this) {
	vorbis_track *track = (vorbis_track*) this;
	return track->meta;
}

int vorbis_track_dec(track_i *this, float *pcm, int samples) {
	vorbis_track *track = (vorbis_track*) this;
	float sample_ratio = (float) track->meta.sample_rate / stream_sample_rate;
	if (track->frame.consumed == track->frame.samples) {
		int ret;
	retry:
		ret = ov_read_float(&track->file, &track->frame.pcm, 1 + samples * sample_ratio, NULL);
		if (ret < 0) {
			fprintf(stderr, "ov_read_float: %s\n",
				strvorbiserror(ret));
			switch (ret) {
			case OV_HOLE: case OV_EREAD: case OV_EBADPACKET:
				goto retry;
			case OV_EINVAL:
				ret = ov_test_open(&track->file);
				fprintf(stderr, "ov_test_open: %s\n",
					strvorbiserror(ret));
			default:
				return -1;
			}
		}
		if (ret == 0) return 0;
		track->frame.samples = ret;
		track->frame.consumed = 0;
	}
	int chn = track->meta.channels;
	int consumed = track->frame.consumed;
	int available = track->frame.samples - consumed;
	spx_uint32_t in_len, out_len;
	for (int ch = 0; ch < chn; ch++) {
		in_len  = available;
		out_len = samples;
		speex_resampler_process_float(
			track->resampler, ch,
			&track->frame.pcm[ch][consumed], &in_len,
			&pcm[vorbis_vorbis81_ch_map[chn][ch]], &out_len
		);
	}
	float scale = powf(10, track->state.gain/20);
	switch (track->state.gain_type) {
	case album_gain: scale *= powf(10, track->album_gain/20); break;
	case track_gain: scale *= powf(10, track->track_gain/20); break;
	default: break;
	}
	int total_out_samples = stream_channel_cnt*out_len;
	for (int s = 0; s < total_out_samples; s++) {
		pcm[s] *= scale;
	}
	track->state.time = ov_time_tell(&track->file);
	track->frame.consumed += in_len;
	return out_len;
}

int vorbis_track_seek(track_i *this, float offset, int whence) {
	vorbis_track *track = (vorbis_track*) this;
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
	return ov_time_seek(&track->file, real_offset);
}

int vorbis_track_gain(track_i *this, float gain, int whence) {
	vorbis_track *track = (vorbis_track*) this;
	switch (whence) {
	case SEEK_SET: track->state.gain = gain; break;
	case SEEK_CUR: track->state.gain += gain; break;
	}
	return 0;
}

int vorbis_track_gain_type(track_i *this, enum gain_type gain_type) {
	vorbis_track *track = (vorbis_track*) this;
	track->state.gain_type = gain_type;
	return 0;
}

static void free_if_null(const void *ptr) {
	if (ptr) free((void *) ptr);
}

int vorbis_track_close(track_i *this) {
	vorbis_track *track = (vorbis_track*) this;
	free_if_null(track->meta.artist);
	free_if_null(track->meta.album);
	free_if_null(track->meta.title);
	free_if_null(track->meta.tracknumber);
	free_if_null(track->meta.tracktotal);
	ov_clear(&track->file);
	return 0;
}

const track_i vorbis_track_vtable = {
	.state = vorbis_track_state,
	.meta  = vorbis_track_meta,
	.dec   = vorbis_track_dec,
	.seek  = vorbis_track_seek,
	.gain  = vorbis_track_gain,
	.gain_type = vorbis_track_gain_type,
	.close = vorbis_track_close,
};

static void copy_tag(
	const char **dest,
	vorbis_comment *tags,
	const char *tag
) {
	if (*dest) return;
	int tag_len = strlen(tag);
	const char *value = NULL;
	for (int i = 0; i < tags->comments; i++) {
		int comment_len = tags->comment_lengths[i];
		if (comment_len < tag_len+1) continue;
		const char *comment = tags->user_comments[i];
		if (comment[tag_len] != '=') continue;
		value = comment+tag_len+1;
		for (int i = 0; i < tag_len; i++) {
			if (tolower(comment[i]) != tolower(tag[i])) {
				value = NULL;
				break;
			}
		}
		if (value) break;
	}
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

int vorbis_track_from_file(
	vorbis_track *track,
	const char *filename,
	long link_start,
	long link_end
) {
	*track = (vorbis_track) { 0 };
	track->track_i = vorbis_track_vtable;

	if (link_start == 0 && link_end == -1) {
		int overr = ov_fopen(filename, &track->file);
		if (overr != 0) {
			fprintf(stderr,
				"ov_fopen: %s: %s\n",
				filename, strvorbiserror(overr));
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
		track->stream = (vorbis_stream) {
			.file   = file,
			.index  = 0,
			.length = link_end,
		};
		int overr = ov_open_callbacks(
			&track->stream,
			&track->file,
			NULL, 0,
			vorbis_file_callbacks
		);
		if (overr != 0) {
			fprintf(stderr,
				"ov_open_callbacks: %s: %s\n",
				filename, strvorbiserror(overr));
			return -1;
		}
	}

	track->meta.codec = VORBIS;

	vorbis_info *info = ov_info(&track->file, -1);
	track->meta.sample_rate = info->rate;
	track->meta.channels = info->channels;

	track->meta.length = ov_time_total(&track->file, -1);

	track->meta.bit_rate = ov_bitrate(&track->file, -1);

	vorbis_comment *tags = ov_comment(&track->file, -1);
	copy_tag(&track->meta.artist, tags, "artist");
	copy_tag(&track->meta.album, tags, "album");
	copy_tag(&track->meta.title, tags, "title");
	copy_tag(&track->meta.tracknumber, tags, "tracknumber");
	copy_tag(&track->meta.tracktotal, tags, "tracktotal");
	
	int speex_err;
	track->resampler = speex_resampler_init(
		stream_channel_cnt,
		track->meta.sample_rate, stream_sample_rate,
		10, &speex_err
	);
	speex_resampler_set_input_stride(track->resampler, 1);
	speex_resampler_set_output_stride(track->resampler, stream_channel_cnt);

	return 0;
}
