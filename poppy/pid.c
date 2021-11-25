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
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

void xdg_state_home(char *path) {
	const char *xdgstatehome = getenv("XDG_STATE_HOME");
	if (xdgstatehome) {
		sprintf(path, xdgstatehome);
		return;
	}
	const char *home = getenv("HOME");
	if (!home) {
		struct passwd *passwd = getpwuid(getuid());
		home = passwd->pw_dir;
	}
	sprintf(path, "%s/.local/state", home);
	return;
}

int mkdirp(const char *path, mode_t mode) {
	int r = mkdir(path, mode);
	if (r && errno == ENOENT) {
		char pathcopy[4096] = {0};
		pathcopy[0] = path[0];
		for (int i = 1; path[i]; i++) {
			if (path[i] != '/') {
				pathcopy[i] = path[i];
				continue;
			}
			pathcopy[i] = '\0';
			r = mkdir(pathcopy, mode | 0200);
			perror(pathcopy);
			if (r && errno != EEXIST) return r;
			pathcopy[i] = '/';
		}
		return mkdir(path, mode);
	}
	return r;
}

void write_pid_file(void) {
	char path[4096] = {0};
	xdg_state_home(path);
	strcat(path, "/poppy");
	int r = mkdirp(path, 0777);
	if (r && errno != EEXIST) {
		fprintf(stderr, "mkdir %s: ", path);
		perror("");
		exit(1);
	}
	strcat(path, "/pid");
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "open %s: ", path);
		perror("");
		exit(1);
	}
	pid_t pid = getpid();
	fprintf(f, "%d\n", pid);
	fclose(f);
}
