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

#include <signal.h>

#include <unistd.h>

#include <opusfile.h>
#include <portaudio.h>

#include "poppy.h"
#include "signalsdef.h"

void sigseek(int sig, siginfo_t *siginfo, void *ctx) {
	ogg_int64_t seek_offset, pcm_tell, target, pcm_total;
	seek_offset = siginfo->si_value.sival_int * 48;
	pcm_tell = op_pcm_tell(chains[current_chain]);
	target = pcm_tell + seek_offset;
retry_seek:
	if (target < 0) {
		if (current_chain > 0) {
			op_pcm_seek(chains[current_chain], 0);
			current_chain--;
			target += op_pcm_total(chains[current_chain], -1)-1;
			goto retry_seek;
		}
		target = 0;
	} else if (target >= (pcm_total = op_pcm_total(chains[current_chain], -1))) {
		if (current_chain < chain_count-1) {
			op_pcm_seek(chains[current_chain], 0);
			current_chain++;
			target -= pcm_total;
			goto retry_seek;
		}
		target = pcm_total-1;
	}
	op_pcm_seek(chains[current_chain], target);
}

void sigskip(int sig, siginfo_t *siginfo, void *ctx) {
	int skip_offset, current_link, target, link_count;
	skip_offset = siginfo->si_value.sival_int;
	current_link = op_current_link(chains[current_chain]);
	target = current_link + skip_offset;
retry_skip:
	if (target < 0) {
		if (current_chain > 0) {
			op_pcm_seek(chains[current_chain], 0);
			current_chain--;
			target += op_link_count(chains[current_chain])-1;
			goto retry_skip;
		}
		target = 0;
	} else if (target >= (link_count = op_link_count(chains[current_chain]))) {
		if (current_chain < chain_count-1) {
			op_pcm_seek(chains[current_chain], 0);
			current_chain++;
			target -= link_count;
			goto retry_skip;
		}
		target = link_count-1;
	}
	if (target == 0) {
		op_pcm_seek(chains[current_chain], 0);
		return;
	}
	ogg_int64_t start = 0;
	for (int i = 0; i < target; i++) {
		start += op_pcm_total(chains[current_chain], i);
	}
	op_pcm_seek(chains[current_chain], start);
}

void siggain(int sig, siginfo_t *siginfo, void *ctx) {
	gain += siginfo->si_value.sival_int;
}

void siggaintype(int sig, siginfo_t *siginfo, void *ctx) {
	switch (siginfo->si_value.sival_int) {
	default:
	case header: gain_type = OP_HEADER_GAIN; break;
	case album: gain_type = OP_ALBUM_GAIN; break;
	case track: gain_type = OP_TRACK_GAIN; break;
	case absolute: gain_type = OP_ABSOLUTE_GAIN; break;
	}
}

void sigabsgain(int sig, siginfo_t *siginfo, void *ctx) {
	gain = siginfo->si_value.sival_int;
}

void sigplaymode(int sig, siginfo_t *siginfo, void *ctx) {
	play_mode = siginfo->si_value.sival_int;
}

void set_signal_handlers(void) {
	struct sigaction act = {0};
	act.sa_sigaction = sigseek;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(SIGSEEK, &act, NULL);
	act.sa_sigaction = sigskip;
	sigaction(SIGSKIP, &act, NULL);
	act.sa_sigaction = siggain;
	sigaction(SIGGAIN, &act, NULL);
	act.sa_sigaction = siggaintype;
	sigaction(SIGGAINTYPE, &act, NULL);
	act.sa_sigaction = sigabsgain;
	sigaction(SIGABSGAIN, &act, NULL);
	act.sa_sigaction = sigplaymode;
	sigaction(SIGPLAYMODE, &act, NULL);
}

