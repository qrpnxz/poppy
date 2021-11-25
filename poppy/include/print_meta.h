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

void print_mapping_family(int family, int channels);

void print_opusfile_head(const OpusHead *head);

void print_opusfile_tags(const OpusTags *tags);

void print_opusfile_info(OggOpusFile *opusfile);

void print_metadata(OggOpusFile *opusfile);
