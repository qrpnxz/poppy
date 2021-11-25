# SPDX-License-Identifier: GPL-3.0-or-later

# Copyright 2021 Russell Hernandez Ruiz <qrpnxz@hyperlife.xyz>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

project(
	'poppy', 'c',
	version : '0.1.0',
	license : 'AGPL-3.0-or-later',
	default_options : [
		'c_std=c18',
		'optimization=s',
		'strip=true',
		'b_coverage=true',
		'b_lto=true',
	],
)

cc = meson.get_compiler('c')

subdir('poppy')
subdir('poppyctl')
