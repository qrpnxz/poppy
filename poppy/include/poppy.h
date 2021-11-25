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

#include "signalsdef.h"

extern const double sample_rate;
extern const double sample_time;

extern OggOpusFile **chains;
extern int chain_count;
extern int current_chain;
extern opus_int32 gain;
extern int gain_type;
extern enum play_mode play_mode;

void print_opus_read_error(int err);

int play_opus(
	const void *input,
	void *output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData
);

void parse_opt(const char *arg);
