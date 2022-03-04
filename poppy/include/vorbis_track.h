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

#include <vorbis/vorbisfile.h>
#include <speex/speex_resampler.h>

typedef struct vorbis_stream {
	FILE *file;
	long index;
	long length;
} vorbis_stream;

typedef struct vorbis_frame {
	float **pcm;
	int samples;
	int consumed;
} vorbis_frame;

typedef struct vorbis_track {
	track_i track_i;
	track_state state;
	track_meta meta;
	float album_gain;
	float track_gain;
	vorbis_stream stream;
	OggVorbis_File file;
	vorbis_frame frame;
	SpeexResamplerState *resampler;
} vorbis_track;

int vorbis_track_from_file(
	vorbis_track *track,
	const char *filename,
	long link_start,
	long link_end
);
