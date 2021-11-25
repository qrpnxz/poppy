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

void print_mapping_family(int family, int channels) {
	switch (family) {
	case 0:
		printf("RTP mapping: ");
		switch (channels) {
		case 1: puts("1 channel: monophonic (mono)"); break;
		case 2: puts("2 channels: stereo (left, right)"); break;
		default: printf("%d channels: invalid\n", channels); break;
		}
		break;
	case 1:
		printf("Vorbis mapping: ");
		switch (channels) {
		case 1: puts("1 channel: monophonic (mono)"); break;
		case 2: puts("2 channels: stereo (left, right)"); break;
		case 3: puts("3 channels: linear surround (left, center, right)"); break;
		case 4: puts("4 channels: quadraphonic "
				"(front left, front right, "
				"rear left, rear right)"); break;
		case 5: puts("5 channels: 5.0 surround "
				"(front left, front center, front right, "
				"rear left, rear right)"); break;
		case 6: puts("6 channels: 5.1 surround "
				"(front left, front center, front right, "
				"rear left, rear right, LFE)"); break;
		case 7: puts("7 channels: 6.1 surround "
				"(front left, front center, front right, "
				"side left, side right, "
				"rear center, LFE)"); break;
		case 8: puts("8 channels: 7.1 surround "
				"(front left, front center, front right, "
				"side left, side right, "
				"rear left, rear right, LFE)"); break;
		default: printf("%d channels: invalid\n", channels); break;
		}
		break;
	case 2: printf("Ambisonic: %d channels\n", channels); break;
	case 3: printf("Ambisonic: %d channels\n", channels); break;
	default:
	case 255: printf("%d channels: undefined\n", channels); break;
	}
}

void print_opusfile_head(const OpusHead *head) {
	printf("version: %d\n", head->version);
	printf("input_sample_rate: %.3g khz\n", head->input_sample_rate / 1e3);
	printf("mapping_family: %d: ", head->mapping_family);
	printf("output_gain: %+g dB\n", head->output_gain / 256.);
	print_mapping_family(head->mapping_family, head->channel_count);
}

void print_opusfile_tags(const OpusTags *tags) {
	printf("vendor: %s\n", tags->vendor);
	for (int cn = 0; cn < tags->comments; cn++) {
		int cl = tags->comment_lengths[cn];
		fwrite(tags->user_comments[cn], 1, cl, stdout);
		fputc('\n', stdout);
	}
}

void print_opusfile_info(OggOpusFile *opusfile) {
	printf("bitrate: %.3g kbps\n", op_bitrate(opusfile, -1) / 1e3);
	printf("serialno: %d\n", op_serialno(opusfile, -1));
	print_opusfile_head(op_head(opusfile, -1));
	print_opusfile_tags(op_tags(opusfile, -1));
}

void print_metadata(OggOpusFile *opusfile) {
	const OpusHead *head = op_head(opusfile, -1);
	const OpusTags *tags = op_tags(opusfile, -1);
	const char *value = NULL;

	printf("Stream serial number: %d\n", op_serialno(opusfile, -1));
	printf("Codec version: %d\n", head->version);
	printf("Vendor: %s\n", tags->vendor);
	puts("");

	printf("Original sample rate: %.3g khz\n", head->input_sample_rate / 1e3);
	printf("Channel mapping family: %d: ", head->mapping_family);
	print_mapping_family(head->mapping_family, head->channel_count);
	printf("Default gain: %+g dB\n", head->output_gain / 256.);
	int album_gain = 0, track_gain = 0;
	opus_tags_get_album_gain(tags, &album_gain);
	opus_tags_get_track_gain(tags, &track_gain);
	printf("Additional album gain: %+g dB (total: %+g dB)\n",
		album_gain/256., (head->output_gain + album_gain)/256.);
	printf("Additional track gain: %+g dB (total: %+g dB)\n",
		track_gain/256., (head->output_gain + track_gain)/256.);
	puts("");

	if ((value = opus_tags_query(tags, "artist", 0))) {
		printf("Artist:\t%s", value);
		for (int i = 1; (value = opus_tags_query(tags, "artist", 1)); i++) {
			printf(", %s", value);
		}
		//for (int i = 1; i < op_tags_query_count(tags, "artist"); i++) {
		//	value = opus_tags_query(tags, "artist", 0);
		//	printf(", %s", value);
		puts("");
	}
	if ((value = opus_tags_query(tags, "album", 0))) {
		printf("Album:\t%s\n", value);
	}
	if ((value = opus_tags_query(tags, "title", 0))) {
		printf("Title:\t%s\n", value);
	}
}
