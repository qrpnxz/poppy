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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>
#include <string.h>

#include <ogg/ogg.h>

#include "track.h"
#include "opus_track.h"
#include "flac_track.h"
#include "vorbis_track.h"

int ogg_tracks_from_file(track_i ***tracks, const char *filename) {
	FILE *file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "fopen: %s: ", filename);
		perror("");
		return -1;
	}
	track_i **ogg_tracks = NULL;
	int num_tracks = 0;
	int link = 0;
	long link_start = 0;
	ogg_sync_state oy;
	ogg_sync_init(&oy);
	ogg_page og;
	while (ogg_sync_pageout(&oy, &og) != 1) {
		char *buf = ogg_sync_buffer(&oy, 0x1000);
		int n = fread(buf, 1, 0x1000, file);
		if (ferror(file)) {
			fprintf(stderr, "error parsing ogg stream: %s\n",
				filename);
			if (ogg_tracks) free(ogg_tracks);
			return -1;
		}
		ogg_sync_wrote(&oy, n);
	}
	ogg_stream_state os;
link:
	ogg_stream_init(&os, ogg_page_serialno(&og));
	if (ogg_stream_pagein(&os, &og)) {
		fprintf(stderr, "error parsing ogg stream: %s\n",
			filename);
		if (ogg_tracks) free(ogg_tracks);
		return -1;
	}
	ogg_packet op;
	if (ogg_stream_packetout(&os, &op) != 1) {
		fprintf(stderr, "error parsing ogg stream: %s\n",
			filename);
		if (ogg_tracks) free(ogg_tracks);
		return -1;
	}

	const char magic_opus[] = "OpusHead\x01";
	const char magic_vorbis[] = "\x01vorbis\0\0\0\0";
	const char magic_flac[] = "\177FLAC\x01\0";
	enum codec {unknown, opus, vorbis, flac} codec;
	if (!memcmp(op.packet, magic_opus, sizeof magic_opus - 1)) {
		//fprintf(stderr, "DEBUG: detected OPUS\n");
		codec = opus;
	}
	else if (!memcmp(op.packet, magic_vorbis, sizeof magic_vorbis - 1)) {
		//fprintf(stderr, "DEBUG: detected VORBIS\n");
		codec = vorbis;
	}
	else if (!memcmp(op.packet, magic_flac, sizeof magic_flac - 1)) {
		//fprintf(stderr, "DEBUG: detected FLAC\n");
		codec = flac;
	}
	else {
		codec = unknown;
	}

	bool last_link = false;
	do {
		while (ogg_sync_pageout(&oy, &og) != 1) {
			char *buf = ogg_sync_buffer(&oy, 0x1000);
			int n = fread(buf, 1, 0x1000, file);
			if (feof(file)) {
				last_link = true;
				goto fin;
			}
			if (ferror(file)) {
				fprintf(stderr, "unable to read file: %s\n",
					filename);
				if (ogg_tracks) free(ogg_tracks);
				return -1;
			}
			ogg_sync_wrote(&oy, n);
		}
	} while (ogg_page_bos(&og));
	do {
		while (ogg_sync_pageout(&oy, &og) != 1) {
			char *buf = ogg_sync_buffer(&oy, 0x1000);
			int n = fread(buf, 1, 0x1000, file);
			if (feof(file)) {
				last_link = true;
				goto fin;
			}
			if (ferror(file)) {
				fprintf(stderr, "unable to read file: %s\n",
					filename);
				if (ogg_tracks) free(ogg_tracks);
				return -1;
			}
			ogg_sync_wrote(&oy, n);
		}
	} while (!ogg_page_bos(&og));
	long link_end;
fin:
	link_end = -1;
	if (!last_link) {
		long page_size = og.header_len + og.body_len;
		long buffered  = oy.fill - oy.returned;
		link_end = ftell(file) - (page_size + buffered);
	}
	switch (codec) {
	case opus: {
		opus_track *track = calloc(1, sizeof *track);
		int ret = opus_track_from_file(track, filename, link_start, link_end);
		if (ret == 0) {
			ogg_tracks = realloc(ogg_tracks,
				(num_tracks+1) * sizeof *ogg_tracks);
			ogg_tracks[num_tracks++] = (track_i*) track;
		}
		break;
	}
	case vorbis: {
		vorbis_track *track = calloc(1, sizeof *track);
		int ret = vorbis_track_from_file(track, filename, link_start, link_end);
		if (ret == 0) {
			ogg_tracks = realloc(ogg_tracks,
				(num_tracks+1) * sizeof *ogg_tracks);
			ogg_tracks[num_tracks++] = (track_i*) track;
		}
		break;
	}
	case flac: {
		flac_track *track = calloc(1, sizeof *track);
		int ret = flac_track_from_file(track, filename, true, link_start, link_end);
		if (ret == 0) {
			ogg_tracks = realloc(ogg_tracks,
				(num_tracks+1) * sizeof *ogg_tracks);
			ogg_tracks[num_tracks++] = (track_i*) track;
		}
		break;
	}
	default:
		fprintf(stderr, "unknown codec in link: %d: %s\n",
			link, filename);
		break;
	}
	if (last_link) {
		ogg_stream_clear(&os);
		ogg_sync_clear(&oy);
		fclose(file);
		*tracks = ogg_tracks;
		return num_tracks;
	}
	link++;
	goto link;
}

int tracks_from_file(
	track_i ***tracks,
	const char *filename
) {
	char head[4];
	FILE *soundfile = fopen(filename, "r");
	if (!soundfile) {
		fprintf(stderr, "fopen: %s: ", filename);
		perror("");
		return -1;
	}
	int tries;
	size_t n = 0;
	for (tries = 0; tries < 10; tries++) {
		n += fread(head+n, 1, sizeof head, soundfile);
		if (n >= sizeof head) break;
		if (ferror(soundfile)) {
			fprintf(stderr, "unable to read file: %s\n", filename);
			return -1;
		}
	}
	if (tries >= 10) {
		fprintf(stderr, "no progress on file: %s\n", filename);
		return -1;
	}
	if (!memcmp(head, "fLaC", 4)) {
		//fprintf(stderr, "DEBUG: detected FLAC\n");
		fclose(soundfile);
		flac_track *track = calloc(1, sizeof *track);
		int ret = flac_track_from_file(track, filename, false, 0, 0);
		if (ret < 0) return ret;
		*tracks = calloc(1, sizeof **tracks);
		**tracks = (track_i*) track;
		return 1;
	} else if (!memcmp(head, "OggS", 4)) {
		//fprintf(stderr, "DEBUG: detected OGG\n");
		fclose(soundfile);
		return ogg_tracks_from_file(tracks, filename);
	} else {
		fprintf(stderr, "unsupported file: %s\n", filename);
		return -1;
	}
}
