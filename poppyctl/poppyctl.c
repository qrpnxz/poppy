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

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#include <unistd.h>
#include <pwd.h>

#include "def.h"

#define SIGSEEK     (SIGRTMIN)
#define SIGSKIP     (SIGRTMIN+1)
#define SIGGAIN     (SIGRTMIN+2)
#define SIGGAINTYPE (SIGRTMIN+3)
#define SIGABSGAIN  (SIGRTMIN+4)
#define SIGPLAYMODE (SIGRTMIN+5)

void print_help(const char *cmd) {
	fprintf(stderr, "%s (stop|pause|play)\n", cmd);
	fprintf(stderr, "%s seek [<seconds>]\n", cmd);
	fprintf(stderr, "%s skip [<tracks>]\n", cmd);
	fprintf(stderr, "%s gain [<dB>]\n", cmd);
	fprintf(stderr, "%s gaintype [header|album|track|absolute]\n", cmd);
	fprintf(stderr, "%s absgain [<dB>]\n", cmd);
	fprintf(stderr, "%s playmode [playlist|repeat|repeatone|single]\n", cmd);
}

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

int main(int argc, char *argv[]) {
	char path[4096] = {0};
	xdg_state_home(path);
	strcat(path, "/poppy/pid");
	FILE *pidfile = fopen(path, "r");
	if (!pidfile) {
		fprintf(stderr, "open %s: ", path);
		perror("");
		return -1;
	}
	pid_t pid;
	fscanf(pidfile, "%d", &pid);
	if (argc-1 < 1) {
		sprintf(path, "/proc/%d/stat", pid);
		FILE *stat = fopen(path, "r");
		if (!stat) {
			fprintf(stderr, "open %s: ", path);
			perror("");
			return -1;
		}
		char state;
		fscanf(stat, "%*d %*s %c", &state);
		fclose(stat);
		switch (state) {
		case 'R': case 'S':
			if (kill(pid, SIGSTOP) != 0) {
				perror("kill");
				return -1;
			}
			return 0;
		case 'T':
			if (kill(pid, SIGCONT) != 0) {
				perror("kill");
				return -1;
			}
			return 0;
		default: return -1;
		}
	}
	union sigval val;
	if (!strcmp(argv[1], "stop")) {
		if (kill(pid, SIGTERM) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "pause")) {
		if (kill(pid, SIGSTOP) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "play")) {
		if (kill(pid, SIGCONT) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "seek")) {
		if (argc-1 < 2) return 0;
		double offset = strtof(argv[2], NULL);
		val.sival_int = offset * 1e3;
		if (sigqueue(pid, SIGSEEK, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "skip")) {
		val.sival_int = 0;
		if (argc-1 >= 2) {
			val.sival_int = strtol(argv[2], NULL, 0);
		}
		if (sigqueue(pid, SIGSKIP, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "gain")) {
		if (argc-1 < 2) return 0;
		float gain = strtof(argv[2], NULL);
		val.sival_int = gain * 256;
		if (sigqueue(pid, SIGGAIN, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "gaintype")) {
		val.sival_int =
			  (argc-1 < 2)                   ? header_gain
			: (!strcmp(argv[2], "header"))   ? header_gain
			: (!strcmp(argv[2], "album"))    ? album_gain
			: (!strcmp(argv[2], "track"))    ? track_gain
			: (!strcmp(argv[2], "absolute")) ? absolute_gain
			: (!strcmp(argv[2], "abs"))      ? absolute_gain
			: -1;
		if (val.sival_int < 0) {
			fputs("header album track absolute\n", stderr);
			return -1;
		}
		if (sigqueue(pid, SIGGAINTYPE, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "absgain")) {
		float gain = 0;
		if (argc-1 >= 3) {
			gain = strtof(argv[2], NULL);
		}
		val.sival_int = gain * 256;
		if (sigqueue(pid, SIGABSGAIN, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[1], "playmode")) {
		val.sival_int =
			  (argc-1 < 2)                    ? playlist
			: (!strcmp(argv[2], "playlist"))  ? playlist
			: (!strcmp(argv[2], "list"))      ? playlist
			: (!strcmp(argv[2], "repeat"))    ? repeat
			: (!strcmp(argv[2], "rep"))       ? repeat
			: (!strcmp(argv[2], "repeatone")) ? repeat_one
			: (!strcmp(argv[2], "repeat1"))   ? repeat_one
			: (!strcmp(argv[2], "rep1"))      ? repeat_one
			: (!strcmp(argv[2], "single"))    ? single
			: -1;
		if (val.sival_int < 0) {
			fputs("playlist repeat repeatone single\n", stderr);
			return -1;
		}
		if (sigqueue(pid, SIGPLAYMODE, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	print_help(argv[0]);
	return -1;
}
