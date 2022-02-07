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

#include <stdbool.h>
#include <threads.h>

#include <pulse/pulseaudio.h>
#include <dbus/dbus.h>

#include "def.h"
#include "track.h"

extern const int stream_sample_rate;
extern const int stream_channel_cnt;

struct playlist {
	track_i **track;
	int curr;
	int size;
};

struct player {
	struct playlist pl;
	double gain;
	enum gain_type gain_type;
	enum play_mode play_mode;
	pa_stream *stream;
	DBusConnection *conn;
	mtx_t lock;
};
