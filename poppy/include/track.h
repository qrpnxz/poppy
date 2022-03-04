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

#include "def.h"

typedef struct track_state {
	float time;
	float gain;
	enum gain_type gain_type;
} track_state;

enum codec {
	OPUS,
	VORBIS,
	FLAC,
};

typedef struct track_meta {
	enum codec codec;
	int channels;
	int bit_depth;
	int sample_rate;
	float length;
	int bit_rate;
	const char *artist;
	const char *album;
	const char *title;
	const char *tracknumber;
	const char *tracktotal;
} track_meta;

typedef struct track_i {
	track_state (*state)(struct track_i *this);
	track_meta (*meta)(struct track_i *this);
	int (*dec)(struct track_i *this, float *pcm, int samples);
	int (*seek)(struct track_i *this, float offset, int whence);
	int (*gain)(struct track_i *this, float gain, int whence);
	int (*gain_type)(struct track_i *this, enum gain_type gain_type);
	int (*close)(struct track_i *this);
} track_i;

int tracks_from_file(
	track_i ***tracks,
	const char *filename
);
