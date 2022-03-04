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

#pragma once

#include <FLAC/stream_decoder.h>
#include <speex/speex_resampler.h>

typedef struct flac_stream {
	FILE *file;
	long index;
	long length;
} flac_stream;

typedef struct flac_frame {
	float *buffer;
	int samples;
	int consumed;
} flac_frame;

typedef struct flac_track {
	track_i track_i;
	track_state state;
	track_meta meta;
	float album_gain;
	float track_gain;
	flac_stream stream;
	FLAC__StreamDecoder *dec;
	flac_frame frame;
	SpeexResamplerState *resampler;
} flac_track;

int flac_track_from_file(
	flac_track *track,
	const char *filename,
	bool isogg,
	long link_start,
	long link_end
);
