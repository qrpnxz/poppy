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

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <opusfile.h>
#include <portaudio.h>

#include "poppy.h"
#include "signals.h"
#include "print_meta.h"
#include "pid.h"

OggOpusFile **chains = NULL;
int chain_count = 0;
int current_chain = 0;
opus_int32 gain = 0;
int gain_type = OP_HEADER_GAIN;
enum play_mode play_mode = playlist;

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

int play_opus(
	const void *input,
	void *output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData
) {
	(void) input;
	(void) timeInfo;
	(void) statusFlags;
	float *pcm = output;
	OggOpusFile *chain;
chain:
	if (current_chain >= chain_count) return paComplete;
	chain = chains[current_chain];
	op_set_gain_offset(chain, gain_type, gain);
	int pre_link = op_current_link(chain);
	int ret = op_read_float_stereo(chain, pcm, 2*frameCount);
	if (ret < 0) {
		fprintf(stderr, "op_read_float_stereo: %s\n",
			stropuserror(ret));
	}
	int post_link = op_current_link(chain);
	ogg_int64_t pcm_tell  = op_pcm_tell(chain);
	ogg_int64_t pcm_total = op_pcm_total(chain, -1);
	if (post_link > pre_link || pcm_tell >= pcm_total) {
		ogg_int64_t last_length;
		switch (play_mode) {
		case repeat_one:
			last_length = op_pcm_total(chain, pre_link);
			op_pcm_seek(chain, pcm_tell - last_length);
			break;
		case single:
			return paComplete;
		default: break;
		}
	}
	if (pcm_tell >= pcm_total) {
		switch (play_mode) {
		case playlist:
			op_pcm_seek(chain, 0);
			current_chain++;
			break;
		case repeat:
			op_pcm_seek(chain, 0);
			current_chain++;
			current_chain %= chain_count;
			break;
		default: break;
		}
	}
	if (ret < frameCount) {
		pcm += 2*ret;
		frameCount -= ret;
		goto chain;
	}
	return paContinue;
}

void print_playback_details(PaStream *stream) {
	pid_t pid = getpid();
	int prev_chain = -1, prev_link = -1;
	const OpusTags *tags = NULL;
	const char *tracknumber = NULL;
	const char *tracktotal = NULL;
	int paerr = 0;
	while ((paerr = Pa_IsStreamActive(stream))) {
		if (current_chain >= chain_count) continue;

		OggOpusFile *chain = chains[current_chain];
		int current_link = op_current_link(chain);

		if (prev_chain != current_chain ||
			prev_link != current_link) {
			prev_chain = current_chain;
			prev_link  = current_link;
			tags = op_tags(chain, -1);
			tracknumber = opus_tags_query(tags, "tracknumber", 0);
			tracktotal  = opus_tags_query(tags, "tracktotal", 0);

			puts("");
			print_metadata(chain);
		}

		printf("\r%d ", pid);
		printf("[%0*d/%d] ",
			(int) log10(chain_count)+1,
			current_chain+1,
			chain_count);
		int link_count = op_link_count(chain);
		printf("[%0*d/%d] ",
			(int) log10(link_count)+1,
			current_link+1,
			link_count);
		if (tracknumber) {
			printf("[%s", tracknumber);
			if (tracktotal) printf("/%s", tracktotal);
			fputs("] ", stdout);
		}
		double length = op_pcm_total(chain, current_link) / 48e3;
		double offset = 0;
		for (int i = 0; i < current_link; i++) {
			offset += op_pcm_total(chain, i);
		}
		offset /= 48e3;
		double now = op_pcm_tell(chain)/48e3 - offset;
		double remaining = length - now;
		double min, sec;
		sec = modf(now/60, &min)*60;
		printf("[%02.0f:%05.2f/", min, sec);
		sec = modf(length/60, &min)*60;
		printf("%02.0f:%05.2f/", min, sec);
		remaining = length - now;
		sec = modf(remaining/60, &min)*60;
		printf("%02.0f:%05.2f]", min, sec);
		fflush(stdout);
		Pa_Sleep(10);
	}
	if (paerr < 0) {
		fprintf(stderr, "Pa_IsStreamActive: %d\n", paerr);
		exit(1);
	}
}

void print_help(const char *cmd) {
	fprintf(stderr, "%s [-h|-g|-a|-t|-b] <oggopus>+\n\n", cmd);
	fprintf(stderr, "\t-h\tprint this message\n");
	fprintf(stderr, "\t-g#\tset gain to # dB\n");
	fprintf(stderr, "\t-a\tadd album gain\n");
	fprintf(stderr, "\t-t\tadd track gain\n");
	fprintf(stderr, "\t-b\tremove default gain\n");
}

void parse_opt(const char *arg) {
	int len = strlen(arg);
	if (len < 2) return;
	switch (arg[1]) {
	default:
	case 'h': print_help("poppy"); exit(0);
	case 'g': gain = 256*strtof(&arg[2], NULL); break;
	case 'a': gain_type = OP_ALBUM_GAIN; break;
	case 't': gain_type = OP_TRACK_GAIN; break;
	case 'b': gain_type = OP_ABSOLUTE_GAIN; break;
	}
}

void open_playlist(int argc, char *argv[]) {
	chain_count = argc-1;
	chains = calloc(chain_count, sizeof *chains);
	current_chain = 0;
	int parsing_args = true;
	for (int i = 1; i < argc; i++) {
		if (parsing_args && argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				parsing_args = false;
			} else {
				chain_count--;
				parse_opt(argv[i]);
			}
			continue;
		}
		int operr;
		OggOpusFile *chain = op_open_file(argv[i], &operr);
		if (operr != 0) {
			fprintf(stderr,
				"op_open_file: %s: %s\n",
				argv[i], stropuserror(operr));
			exit(1);
		}
		chains[current_chain++] = chain;
	}
	current_chain = 0;
}

void exit_pa(void) {
	PaError err = Pa_Terminate();
	if (err != paNoError) {
		fprintf(stderr,
			"Error terminating portaudio: %s\n",
			Pa_GetErrorText(err));
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) return 0;
	set_signal_handlers();
	open_playlist(argc, argv);

	PaError paerr = Pa_Initialize();
	if (paerr != paNoError) {
		fprintf(stderr,
			"Pa_Initialize: %s\n",
			Pa_GetErrorText(paerr));
		exit(1);
	}
	atexit(exit_pa);

	fprintf(stderr,
		"portaudio successfully initialized: %s\n",
		Pa_GetVersionText());

	PaStream *stream;
	PaStreamParameters outParams = {
		.device = Pa_GetDefaultInputDevice(),
		.channelCount = 2,
		.sampleFormat = paFloat32,
		.suggestedLatency = 0.120,
		.hostApiSpecificStreamInfo = NULL,
	};
	paerr = Pa_OpenStream(
		&stream,
		NULL,
		&outParams,
		48000,
		48*outParams.channelCount,
		paNoFlag,
		play_opus,
		NULL
	);
	if (paerr != paNoError) {
		fprintf(stderr,
			"Pa_OpenStream: %s\n",
			Pa_GetErrorText(paerr));
		exit(1);
	}

	paerr = Pa_StartStream(stream);
	if (paerr != paNoError) {
		fprintf(stderr,
			"Pa_StartStream: %s\n",
			Pa_GetErrorText(paerr));
		exit(1);
	}

	write_pid_file();

	print_playback_details(stream);

	paerr = Pa_CloseStream(stream);
	if (paerr != paNoError) {
		fprintf(stderr,
			"Pa_CloseStream: %s\n",
			Pa_GetErrorText(paerr));
		exit(1);
	}

	for (int i = 0; i < chain_count; i++) {
		op_free(chains[i]);
	}
	free(chains);
	exit(0);
}
