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

#include "signalsdef.h"

void print_help(const char *cmd) {
	fprintf(stderr, "%s <pid> (stop|pause|play)\n", cmd);
	fprintf(stderr, "%s <pid> seek [<seconds>]\n", cmd);
	fprintf(stderr, "%s <pid> skip [<tracks>]\n", cmd);
	fprintf(stderr, "%s <pid> gain [<dB>]\n", cmd);
	fprintf(stderr, "%s <pid> gaintype [header|album|track|absolute]\n", cmd);
	fprintf(stderr, "%s <pid> absgain [<dB>]\n", cmd);
	fprintf(stderr, "%s <pid> playmode [playlist|repeat|repeatone|single]\n", cmd);
}

int main(int argc, char *argv[]) {
	if (argc-1 < 1) {
		print_help(argv[0]);
		return -1;
	}
	if (argc-1 < 2) {
		char path[32] = {0};
		sprintf(path, "/proc/%s/stat", argv[1]);
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
			if (kill(atoi(argv[1]), SIGSTOP) != 0) {
				perror("kill");
				return -1;
			}
			return 0;
		case 'T':
			if (kill(atoi(argv[1]), SIGCONT) != 0) {
				perror("kill");
				return -1;
			}
			return 0;
		default: return -1;
		}
	}
	union sigval val;
	if (!strcmp(argv[2], "stop")) {
		if (kill(atoi(argv[1]), SIGTERM) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "pause")) {
		if (kill(atoi(argv[1]), SIGSTOP) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "play")) {
		if (kill(atoi(argv[1]), SIGCONT) != 0) {
			perror("kill");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "seek")) {
		if (argc-1 < 3) return 0;
		double offset = strtof(argv[3], NULL);
		val.sival_int = offset * 1e3;
		if (sigqueue(atoi(argv[1]), SIGSEEK, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "skip")) {
		val.sival_int = 0;
		if (argc-1 >= 3) {
			val.sival_int = strtol(argv[3], NULL, 0);
		}
		if (sigqueue(atoi(argv[1]), SIGSKIP, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "gain")) {
		if (argc-1 < 3) return 0;
		float gain = strtof(argv[3], NULL);
		val.sival_int = gain * 256;
		if (sigqueue(atoi(argv[1]), SIGGAIN, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "gaintype")) {
		val.sival_int =
			  (argc-1 < 3)                   ? header
			: (!strcmp(argv[3], "header"))   ? header
			: (!strcmp(argv[3], "album"))    ? album
			: (!strcmp(argv[3], "track"))    ? track
			: (!strcmp(argv[3], "absolute")) ? absolute
			: (!strcmp(argv[3], "abs"))      ? absolute
			: -1;
		if (val.sival_int < 0) {
			fputs("header album track absolute\n", stderr);
			return -1;
		}
		if (sigqueue(atoi(argv[1]), SIGGAINTYPE, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "absgain")) {
		float gain = 0;
		if (argc-1 >= 3) {
			gain = strtof(argv[3], NULL);
		}
		val.sival_int = gain * 256;
		if (sigqueue(atoi(argv[1]), SIGABSGAIN, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	if (!strcmp(argv[2], "playmode")) {
		val.sival_int =
			  (argc-1 < 3)                    ? playlist
			: (!strcmp(argv[3], "playlist"))  ? playlist
			: (!strcmp(argv[3], "list"))      ? playlist
			: (!strcmp(argv[3], "repeat"))    ? repeat
			: (!strcmp(argv[3], "rep"))       ? repeat
			: (!strcmp(argv[3], "repeatone")) ? repeat_one
			: (!strcmp(argv[3], "repeat1"))   ? repeat_one
			: (!strcmp(argv[3], "rep1"))      ? repeat_one
			: (!strcmp(argv[3], "single"))    ? single
			: -1;
		if (val.sival_int < 0) {
			fputs("playlist repeat repeatone single\n", stderr);
			return -1;
		}
		if (sigqueue(atoi(argv[1]), SIGPLAYMODE, val) != 0) {
			perror("sigqueue");
			return -1;
		}
		return 0;
	}
	print_help(argv[0]);
	return -1;
}
