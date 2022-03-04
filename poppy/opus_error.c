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

#include <opusfile.h>

const char *stropuserror(int err) {
	static const char *table[] = {
		[-OP_FALSE        ] = "OP_FALSE",
		[-OP_EOF          ] = "OP_EOF",
		[-OP_HOLE         ] = "OP_HOLE",
		[-OP_EREAD        ] = "OP_EREAD",
		[-OP_EFAULT       ] = "OP_EFAULT",
		[-OP_EIMPL        ] = "OP_EIMPL",
		[-OP_EINVAL       ] = "OP_EINVAL",
		[-OP_ENOTFORMAT   ] = "OP_ENOTFORMAT",
		[-OP_EBADHEADER   ] = "OP_EBADHEADER",
		[-OP_EVERSION     ] = "OP_EVERSION",
		[-OP_ENOTAUDIO    ] = "OP_ENOTAUDIO",
		[-OP_EBADPACKET   ] = "OP_EBADPACKET",
		[-OP_EBADLINK     ] = "OP_EBADLINK",
		[-OP_ENOSEEK      ] = "OP_ENOSEEK",
		[-OP_EBADTIMESTAMP] = "OP_EBADTIMESTAMP",
	};
	return table[-err];
}
