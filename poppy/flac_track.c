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
#include <stdbool.h>
#include <string.h>
#include <threads.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#include <FLAC/stream_decoder.h>
#include <libswresample/swresample.h>
#include <speex/speex_resampler.h>

#include "poppy.h"
#include "track.h"
#include "flac_track.h"
#include "def.h"
#include "ch_map.h"

FLAC__StreamDecoderReadStatus flac_read_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__byte buffer[],
	size_t *bytes,
	void *client_data
) {
	flac_track *track = client_data;
	flac_stream *stream = &track->stream;
	if (stream->index >= stream->length) {
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}
	if (stream->index + *bytes > stream->length) {
		*bytes = stream->length - stream->index;
	}
	for (;;) {
		int n = fread(buffer, 1, *bytes, stream->file);
		if (n > 0) {
			stream->index += n;
			*bytes = n;
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
		if (feof(stream->file)) {
			*bytes = 0;
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}
		if (ferror(stream->file)) {
			*bytes = 0;
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		}
	}
}

FLAC__StreamDecoderSeekStatus flac_seek_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 absolute_byte_offset,
	void *client_data
) {
	flac_track *track = client_data;
	flac_stream *stream = &track->stream;
	FLAC__uint64 offset = absolute_byte_offset;
	long real_offset;
	real_offset = offset - stream->index;
	stream->index = offset;
	if (real_offset < -stream->length) {
		real_offset = -stream->length;
		stream->index = 0;
	} else if (real_offset > stream->length) {
		real_offset = stream->length - stream->index;
		stream->index = stream->length;
	}
	if (!fseek(stream->file, real_offset, SEEK_CUR)) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
	} else {
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	}
}

FLAC__StreamDecoderTellStatus flac_tell_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *absolute_byte_offset,
	void *client_data
) {
	flac_track *track = client_data;
	flac_stream *stream = &track->stream;
	*absolute_byte_offset = stream->index;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus flac_length_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *stream_length,
	void *client_data
) {
	flac_track *track = client_data;
	flac_stream *stream = &track->stream;
	*stream_length = stream->length;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flac_eof_callback(
	const FLAC__StreamDecoder *decoder,
	void *client_data
) {
	flac_track *track = client_data;
	flac_stream *stream = &track->stream;
	return stream->index >= stream->length;
}


FLAC__StreamDecoderWriteStatus flac_write_callback(
	const FLAC__StreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 *const buffer[],
	void *client_data
) {
	flac_track *track = client_data;

	int chn = track->meta.channels;
	int sn = frame->header.blocksize;
	int depth = 1-frame->header.bits_per_sample;
	track->frame.buffer = realloc(track->frame.buffer, chn*sn * sizeof (float));
	for (int ch = 0; ch < chn; ch++) {
		for (int s = 0; s < sn; s++) {
			track->frame.buffer[chn*s+ch] = ldexpf(buffer[ch][s], depth);
		}
	}
	track->frame.samples = sn;
	track->frame.consumed = 0;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void copy_tag(
	const char **dest,
	FLAC__StreamMetadata_VorbisComment tags,
	const char *tag
) {
	if (*dest) return;
	int tag_len = strlen(tag);
	const char *value = NULL;
	for (int i = 0; i < tags.num_comments; i++) {
		int comment_len = tags.comments[i].length;
		if (comment_len < tag_len+1) continue;
		const char *comment = (char*) tags.comments[i].entry;
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

void flac_metadata_callback(
	const FLAC__StreamDecoder *decoder,
	const FLAC__StreamMetadata *metadata,
	void *client_data
) {
	flac_track *track = client_data;
	switch (metadata->type) {
	case FLAC__METADATA_TYPE_STREAMINFO: {
		FLAC__StreamMetadata_StreamInfo stream_info =
			metadata->data.stream_info;
		track->meta.codec = FLAC;
		track->meta.sample_rate = stream_info.sample_rate;
		track->meta.channels    = stream_info.channels;
		track->meta.bit_depth   = stream_info.bits_per_sample;
		track->meta.length =
			(float) stream_info.total_samples /
				stream_info.sample_rate;
		track->meta.bit_rate =
			track->stream.length / 8 /
				track->meta.length;
		break;
	}
	case FLAC__METADATA_TYPE_VORBIS_COMMENT: {
		FLAC__StreamMetadata_VorbisComment tags =
			metadata->data.vorbis_comment;
		copy_tag(&track->meta.artist, tags, "artist");
		copy_tag(&track->meta.album, tags, "album");
		copy_tag(&track->meta.title, tags, "title");
		copy_tag(&track->meta.tracknumber, tags, "tracknumber");
		copy_tag(&track->meta.tracktotal, tags, "tracktotal");
		break;
	}
	default: break;
	}
}

void flac_error_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__StreamDecoderErrorStatus status,
	void *client_data
) {
	fprintf(stderr, "%s\n", FLAC__StreamDecoderErrorStatusString[status]);
	fflush(stderr);
}

track_state flac_track_state(track_i *this) {
	flac_track *track = (flac_track*) this;
	return track->state;
}

track_meta flac_track_meta(track_i *this) {
	flac_track *track = (flac_track*) this;
	return track->meta;
}

int flac_track_dec(track_i *this, float *pcm, int samples) {
	flac_track *track = (flac_track*) this;

	if (track->frame.consumed == track->frame.samples) {
		FLAC__stream_decoder_process_single(track->dec);
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
			&track->frame.buffer[chn*consumed+ch], &in_len,
			&pcm[vorbis_vorbis81_ch_map[chn][flac_vorbis_ch_map[chn][ch]]], &out_len
		);
	}
	float scale = powf(10, track->state.gain/20);
	switch (track->state.gain_type) {
	case album_gain: scale *= powf(10, track->album_gain/20); break;
	case track_gain: scale *= powf(10, track->track_gain/20); break;
	default: break;
	}
	for (int s = 0; s < stream_channel_cnt*out_len; s++) {
		pcm[s] *= scale;
	}
	track->state.time += out_len / (float) stream_sample_rate;
	track->frame.consumed += in_len;
	return out_len;
}

int flac_track_seek(track_i *this, float offset, int whence) {
	flac_track *track = (flac_track*) this;
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
	int ret = !FLAC__stream_decoder_seek_absolute(
		track->dec,
		real_offset * track->meta.sample_rate
	);
	return ret;
}

int flac_track_gain(track_i *this, float gain, int whence) {
	flac_track *track = (flac_track*) this;
	switch (whence) {
	case SEEK_SET: track->state.gain = gain; break;
	case SEEK_CUR: track->state.gain += gain; break;
	}
	return 0;
}

int flac_track_gain_type(track_i *this, enum gain_type gain_type) {
	flac_track *track = (flac_track*) this;
	track->state.gain_type = gain_type;
	return 0;
}

static void free_if_null(const void *ptr) {
	if (ptr) free((void *) ptr);
}

int flac_track_close(track_i *this) {
	flac_track *track = (flac_track*) this;
	free_if_null(track->meta.artist);
	free_if_null(track->meta.album);
	free_if_null(track->meta.title);
	free_if_null(track->meta.tracknumber);
	free_if_null(track->meta.tracktotal);
	FLAC__stream_decoder_delete(track->dec);
	return 0;
}

const track_i flac_track_vtable = {
	.state = flac_track_state,
	.meta  = flac_track_meta,
	.dec   = flac_track_dec,
	.seek  = flac_track_seek,
	.gain  = flac_track_gain,
	.gain_type = flac_track_gain_type,
	.close = flac_track_close,
};

int flac_track_from_file(
	flac_track *track,
	const char *filename,
	bool isogg,
	long link_start,
	long link_end
) {
	*track = (flac_track) { 0 };
	track->track_i = flac_track_vtable;
	track->dec = FLAC__stream_decoder_new();

	FLAC__stream_decoder_set_metadata_respond(
		track->dec,
		FLAC__METADATA_TYPE_VORBIS_COMMENT
	);

	FLAC__StreamDecoderInitStatus status;
	if (!isogg) {
		status = FLAC__stream_decoder_init_file(
			track->dec,
			filename,
			flac_write_callback,
			flac_metadata_callback,
			flac_error_callback,
			track
		);
	}
	else if (link_start == 0 && link_end == -1) {
		status = FLAC__stream_decoder_init_ogg_file(
			track->dec,
			filename,
			flac_write_callback,
			flac_metadata_callback,
			flac_error_callback,
			track
		);
	}
	else {
		FILE *file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "fopen: %s: ", filename);
			perror("");
			return -1;
		}
		fseek(file, link_start, SEEK_CUR);
		track->stream = (flac_stream) {
			.file   = file,
			.index  = 0,
			.length = link_end,
		};
		status = FLAC__stream_decoder_init_ogg_stream(
			track->dec,
			flac_read_callback,
			flac_seek_callback,
			flac_tell_callback,
			flac_length_callback,
			flac_eof_callback,
			flac_write_callback,
			flac_metadata_callback,
			flac_error_callback,
			track
		);
	}
	if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		abort(); //TODO: error
		return -1;
	}

	FLAC__stream_decoder_process_until_end_of_metadata(track->dec);
	
	int speex_err;
	track->resampler = speex_resampler_init(
		stream_channel_cnt,
		track->meta.sample_rate, stream_sample_rate,
		10, &speex_err
	);
	speex_resampler_set_input_stride(track->resampler, track->meta.channels);
	speex_resampler_set_output_stride(track->resampler, stream_channel_cnt);

	return 0;
}
