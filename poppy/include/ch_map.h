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

#include <pulse/pulseaudio.h>

enum vorbis_ch_map {
	vorbis_silence,
	vorbis_mono,
	vorbis_stereo,
	vorbis_linear_surround,
	vorbis_quadraphonic,
	vorbis_5_0_surround,
	vorbis_5_1_surround,
	vorbis_6_1_surround,
	vorbis_7_1_surround,
	vorbis_8_1_surround,
};

enum flac_ch_map {
	flac_silence,
	flac_mono,
	flac_stereo,
	flac_linear_surround,
	flac_quadraphonic,
	flac_5_0_surround,
	flac_5_1_surround,
	flac_6_1_surround,
	flac_7_1_surround,
	flac_8_1_surround,
};

static const pa_channel_map vorbis_pa_ch_map[] = {
	[vorbis_silence] = { 0 },
	[vorbis_mono] = (pa_channel_map) {
		.channels = 1,
		.map = {
			[0] = PA_CHANNEL_POSITION_MONO,
		},
	},
	[vorbis_stereo] = (pa_channel_map) {
		.channels = 2,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_RIGHT,
		},
	},
	[vorbis_linear_surround] = (pa_channel_map) {
		.channels = 3,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
		},
	},
	[vorbis_quadraphonic] = (pa_channel_map) {
		.channels = 4,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[2] = PA_CHANNEL_POSITION_REAR_LEFT,
			[3] = PA_CHANNEL_POSITION_REAR_RIGHT,
		},
	},
	[vorbis_5_0_surround] = (pa_channel_map) {
		.channels = 5,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[3] = PA_CHANNEL_POSITION_REAR_LEFT,
			[4] = PA_CHANNEL_POSITION_REAR_RIGHT,
		},
	},
	[vorbis_5_1_surround] = (pa_channel_map) {
		.channels = 6,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[3] = PA_CHANNEL_POSITION_REAR_LEFT,
			[4] = PA_CHANNEL_POSITION_REAR_RIGHT,
			[5] = PA_CHANNEL_POSITION_LFE,
		},
	},
	[vorbis_6_1_surround] = (pa_channel_map) {
		.channels = 7,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[3] = PA_CHANNEL_POSITION_SIDE_LEFT,
			[4] = PA_CHANNEL_POSITION_SIDE_RIGHT,
			[5] = PA_CHANNEL_POSITION_REAR_CENTER,
			[6] = PA_CHANNEL_POSITION_LFE,
		},
	},
	[vorbis_7_1_surround] = (pa_channel_map) {
		.channels = 8,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[3] = PA_CHANNEL_POSITION_SIDE_LEFT,
			[4] = PA_CHANNEL_POSITION_SIDE_RIGHT,
			[5] = PA_CHANNEL_POSITION_REAR_LEFT,
			[6] = PA_CHANNEL_POSITION_REAR_RIGHT,
			[7] = PA_CHANNEL_POSITION_LFE,
		},
	},
	[vorbis_8_1_surround] = (pa_channel_map) {
		.channels = 9,
		.map = {
			[0] = PA_CHANNEL_POSITION_FRONT_LEFT,
			[1] = PA_CHANNEL_POSITION_FRONT_CENTER,
			[2] = PA_CHANNEL_POSITION_FRONT_RIGHT,
			[3] = PA_CHANNEL_POSITION_SIDE_LEFT,
			[4] = PA_CHANNEL_POSITION_SIDE_RIGHT,
			[5] = PA_CHANNEL_POSITION_REAR_LEFT,
			[6] = PA_CHANNEL_POSITION_REAR_CENTER,
			[7] = PA_CHANNEL_POSITION_REAR_RIGHT,
			[8] = PA_CHANNEL_POSITION_LFE,
		},
	},
};

static const int vorbis_flac_ch_map[9][8] = {
	[vorbis_silence]         = { 0 },
	[vorbis_mono]            = { 0 },
	[vorbis_stereo]          = { 0, 1 },
	[vorbis_linear_surround] = { 0, 2, 1 },
	[vorbis_quadraphonic]    = { 0, 1, 2, 3 },
	[vorbis_5_0_surround]    = { 0, 2, 1, 3, 4 },
	[vorbis_5_1_surround]    = { 0, 2, 1, 4, 5, 3 },
	[vorbis_6_1_surround]    = { 0, 2, 1, 5, 6, 4, 3 },
	[vorbis_7_1_surround]    = { 0, 2, 1, 6, 7, 4, 5, 3 },
};

static const int flac_vorbis_ch_map[9][8] = {
	[flac_silence]         = { 0 },
	[flac_mono]            = { 0 },
	[flac_stereo]          = { 0, 1 },
	[flac_linear_surround] = { 0, 2, 1 },
	[flac_quadraphonic]    = { 0, 1, 2, 3 },
	[flac_5_0_surround]    = { 0, 2, 1, 3, 4 },
	[flac_5_1_surround]    = { 0, 2, 1, 5, 3, 4 },
	[flac_6_1_surround]    = { 0, 2, 1, 6, 5, 3, 4 },
	[flac_7_1_surround]    = { 0, 2, 1, 7, 5, 6, 3, 4 },
};

static const int vorbis_vorbis8_ch_map[9][8] = {
	[vorbis_silence]         = { 0 },
	[vorbis_mono]            = { 1 },
	[vorbis_stereo]          = { 0, 2 },
	[vorbis_linear_surround] = { 0, 1, 2 },
	[vorbis_quadraphonic]    = { 0, 2, 5, 6 },
	[vorbis_5_0_surround]    = { 0, 1, 2, 5, 6 },
	[vorbis_5_1_surround]    = { 0, 1, 2, 5, 6, 7 },
	[vorbis_6_1_surround]    = { 0, 1, 2, 3, 4, 5, 7 },
	[vorbis_7_1_surround]    = { 0, 1, 2, 3, 4, 5, 6, 7 },
};

static const int vorbis_vorbis81_ch_map[10][9] = {
	[vorbis_silence]         = { 0 },
	[vorbis_mono]            = { 1 },
	[vorbis_stereo]          = { 0, 2 },
	[vorbis_linear_surround] = { 0, 1, 2 },
	[vorbis_quadraphonic]    = { 0, 2, 5, 7 },
	[vorbis_5_0_surround]    = { 0, 1, 2, 5, 7 },
	[vorbis_5_1_surround]    = { 0, 1, 2, 5, 7, 8 },
	[vorbis_6_1_surround]    = { 0, 1, 2, 3, 4, 6, 8 },
	[vorbis_7_1_surround]    = { 0, 1, 2, 3, 4, 5, 7, 8 },
	[vorbis_8_1_surround]    = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
};
