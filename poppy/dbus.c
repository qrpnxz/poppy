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
#include <threads.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <dbus/dbus.h>
#include <pulse/pulseaudio.h>

#include "track.h"
#include "poppy.h"

static const char *PlaybackPlaying = "Playing";
static const char *PlaybackPaused  = "Paused";
static const char *PlaybackStopped = "Stopped";

static const char *LoopNone     = "None";
static const char *LoopTrack    = "Track";
static const char *LoopPlaylist = "Playlist";

void unregister_noop(DBusConnection *_conn, void *_user_data) {}

void iter_init_dict(
	DBusMessageIter *iter,
	DBusMessageIter *dict
) {
	dbus_message_iter_open_container(
		iter, DBUS_TYPE_ARRAY, "{sv}", dict);
}

void iter_close_dict(
	DBusMessageIter *iter,
	DBusMessageIter *dict
) {
	dbus_message_iter_close_container(iter, dict);
}

void iter_dict_open_entry(
	DBusMessageIter *dict,
	DBusMessageIter *entry,
	DBusMessageIter *variant,
	const char *key,
	const char *sig
) {
	dbus_message_iter_open_container(
		dict, DBUS_TYPE_DICT_ENTRY, NULL, entry);
	dbus_message_iter_append_basic(entry,
		DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(
		entry, DBUS_TYPE_VARIANT, sig, variant);
}

void iter_dict_close_entry(
	DBusMessageIter *dict,
	DBusMessageIter *entry,
	DBusMessageIter *variant
) {
	dbus_message_iter_close_container(entry, variant);
	dbus_message_iter_close_container(dict, entry);
}

void iter_dict_append_basic(
	DBusMessageIter *dict,
	const char *key,
	int type,
	void *value
) {
	DBusMessageIter entry, variant;
	char sig[2] = { (char) type, 0 };
	iter_dict_open_entry(dict, &entry, &variant, key, sig);
	dbus_message_iter_append_basic(&variant, type, value);
	iter_dict_close_entry(dict, &entry, &variant);
}

void iter_dict_append_callback(
	DBusMessageIter *dict,
	const char *key,
	const char *sig,
	void (*value_cb)(DBusMessageIter*, void*),
	void *ud
) {
	DBusMessageIter entry, variant;
	iter_dict_open_entry(dict, &entry, &variant, key, sig);
	value_cb(&variant, ud);
	iter_dict_close_entry(dict, &entry, &variant);
}

void reply_nothing(
	DBusConnection *conn,
	DBusMessage *msg
) {
	DBusMessage *reply = dbus_message_new_method_return(msg);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
}

DBusMessage *signal_prop_change(
	const char *interface
) {
	DBusMessage *signal = dbus_message_new_signal(
		"/org/mpris/MediaPlayer2",
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged"
	);
	dbus_message_append_args(signal,
		DBUS_TYPE_STRING, &interface,
		DBUS_TYPE_INVALID
	);
	return signal;
}

void signal_prop_change_one_basic(
	DBusConnection *conn,
	const char *interface,
	const char *key,
	int type,
	void *value
) {
	DBusMessage *signal = signal_prop_change(interface);
	DBusMessageIter iter, dict, array;
	dbus_message_iter_init_append(signal, &iter);
	iter_init_dict(&iter, &dict);
	iter_dict_append_basic(&dict, key, type, value);
	iter_close_dict(&iter, &dict);
	dbus_message_iter_open_container(&iter,
		DBUS_TYPE_ARRAY, "s", &array);
	dbus_message_iter_close_container(&iter, &array);
	dbus_connection_send(conn, signal, NULL);
	dbus_message_unref(signal);
}

void signal_prop_change_one_callback(
	DBusConnection *conn,
	const char *interface,
	const char *key,
	const char *sig,
	void (*value_cb)(DBusMessageIter*, void*),
	void *ud
) {
	DBusMessage *signal = signal_prop_change(interface);
	DBusMessageIter iter, dict, array;
	dbus_message_iter_init_append(signal, &iter);
	iter_init_dict(&iter, &dict);
	iter_dict_append_callback(&dict, key, sig, value_cb, ud);
	iter_close_dict(&iter, &dict);
	dbus_message_iter_open_container(&iter,
		DBUS_TYPE_ARRAY, "s", &array);
	dbus_message_iter_close_container(&iter, &array);
	dbus_connection_send(conn, signal, NULL);
	dbus_message_unref(signal);
}

struct {
	dbus_bool_t CanQuit;
	dbus_bool_t CanRaise;
	dbus_bool_t HasTrackList;
	const char *Identity;
	const char **SupportedUriSchemes;
	dbus_int32_t SupportedUriSchemesSize;
	const char **SupportedMimeTypes;
	dbus_int32_t SupportedMimeTypesSize;
} mp2_props = {
	.CanQuit = true,
	.CanRaise = false,
	.HasTrackList = false,
	.Identity = "Poppy Music Player",
	.SupportedUriSchemes = NULL,
	.SupportedUriSchemesSize = 0,
	.SupportedMimeTypes = (const char*[]) {
		"audio/ogg; codecs=\"flac, opus, vorbis\"",
		"audio/flac", "audio/x-flac",
		"audio/opus",
	},
	.SupportedMimeTypesSize = 4,
};

DBusHandlerResult mp2_msg(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	if (dbus_message_has_member(msg, "Raise")) {
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "Quit")) {
		reply_nothing(conn, msg);
		exit(0);
	}
	DBusMessage *reply;
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidMethod",
		"Invalid method"
	);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_prop_get(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data,
	const char *property
) {
	DBusMessage *reply = dbus_message_new_method_return(msg);
	if (!strcmp(property, "CanQuit")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_props.CanQuit,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	if (!strcmp(property, "CanRaise")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_props.CanRaise,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	if (!strcmp(property, "HasTrackList")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_props.HasTrackList,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	if (!strcmp(property, "Identity")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_props.Identity,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	if (!strcmp(property, "SupportedUriSchemes")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&mp2_props.SupportedUriSchemes,
				mp2_props.SupportedUriSchemesSize,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	if (!strcmp(property, "SupportedMimeTypes")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&mp2_props.SupportedMimeTypes,
				mp2_props.SupportedMimeTypesSize,
			DBUS_TYPE_INVALID
		);
		goto send_reply;
	}
	dbus_message_unref(reply);
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidProperty",
		"Invalid property"
	);
	goto send_reply;
send_reply:
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_prop_set(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data,
	const char *property
) {
	DBusMessage *reply;
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidProperty",
		"Invalid property"
	);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_prop_getall(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	DBusMessage *reply;
	reply = dbus_message_new_method_return(msg);
	DBusMessageIter iter, dict, entry, variant, array;
	dbus_message_iter_init_append(reply, &iter);
	iter_init_dict(&iter, &dict);

	iter_dict_append_basic(&dict,
		"CanQuit", DBUS_TYPE_BOOLEAN, &mp2_props.CanQuit);

	iter_dict_append_basic(&dict,
		"CanRaise", DBUS_TYPE_BOOLEAN, &mp2_props.CanRaise);

	iter_dict_append_basic(&dict,
		"HasTrackList", DBUS_TYPE_BOOLEAN, &mp2_props.HasTrackList);

	iter_dict_append_basic(&dict,
		"Identity", DBUS_TYPE_STRING, &mp2_props.Identity);

	iter_dict_open_entry(&dict, &entry, &variant,
		"SupportedUriSchemes", "as");
	dbus_message_iter_open_container(&variant,
		DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &array);
	for (int i = 0; i < mp2_props.SupportedUriSchemesSize; i++) {
		dbus_message_iter_append_basic(&array,
			DBUS_TYPE_STRING, &mp2_props.SupportedUriSchemes[i]);
	}
	dbus_message_iter_close_container(&variant, &array);
	iter_dict_close_entry(&dict, &entry, &variant);

	iter_dict_open_entry(&dict, &entry, &variant,
		"SupportedMimeTypes", "as");
	dbus_message_iter_open_container(&variant,
		DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &array);
	for (int i = 0; i < mp2_props.SupportedMimeTypesSize; i++) {
		dbus_message_iter_append_basic(&array,
			DBUS_TYPE_STRING, &mp2_props.SupportedMimeTypes[i]);
	}
	dbus_message_iter_close_container(&variant, &array);
	iter_dict_close_entry(&dict, &entry, &variant);

	iter_close_dict(&iter, &dict);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

struct {
	const char *PlaybackStatus;
	const char *LoopStatus;
	dbus_bool_t Shuffle;
	double Volume;
	dbus_int64_t Position;
	double Rate;
	double MinimumRate;
	double MaximumRate;
	dbus_bool_t CanGoNext;
	dbus_bool_t CanGoPrevious;
	dbus_bool_t CanPlay;
	dbus_bool_t CanPause;
	dbus_bool_t CanSeek;
	dbus_bool_t CanControl;
} mp2_player_props = {
	.PlaybackStatus = "Playing",
	.LoopStatus     = "None",
	.Shuffle        = false,
	.Volume      = 1,
	.Position    = 0,
	.Rate        = 1,
	.MinimumRate = 1,
	.MaximumRate = 1,
	.CanGoNext     = true,
	.CanGoPrevious = true,
	.CanPlay       = true,
	.CanPause      = true,
	.CanSeek       = true,
	.CanControl    = true,
};

void signal_seeked(
	DBusConnection *conn,
	dbus_int64_t position
) {
	DBusMessage *signal = dbus_message_new_signal(
		"/org/mpris/MediaPlayer2",
		"org.mpris.MediaPlayer2.Player",
		"Seeked"
	);
	dbus_message_append_args(signal,
		DBUS_TYPE_INT64, &position,
		DBUS_TYPE_INVALID
	);
	dbus_connection_send(conn, signal, NULL);
	dbus_message_unref(signal);
}

const char *player_playback_status(struct player *player) {
	struct playlist *pl = &player->pl;
	track_i *track = pl->track[pl->curr];
	track_state state = track->state(track);
	return
		(!pa_stream_is_corked(player->stream)) ? PlaybackPlaying
		: (pl->curr == 0 && state.time == 0) ? PlaybackStopped
		: PlaybackPaused;
}

void mp2_player_prop_get_playback_status(
	DBusMessageIter *iter,
	struct player *player
) {
	const char *playback_status =
		player_playback_status(player);
	dbus_message_iter_append_basic(iter,
		DBUS_TYPE_STRING, &playback_status);
}

const char *player_loop_status(struct player *player) {
	return 
		(player->play_mode == repeat) ? LoopPlaylist
		: (player->play_mode == repeat_one) ? LoopTrack
		: LoopNone;
}

void mp2_player_prop_get_loop_status(
	DBusMessageIter *iter,
	struct player *player
) {
	const char *loop_status =
		player_loop_status(player);
	dbus_message_iter_append_basic(iter,
		DBUS_TYPE_STRING, &loop_status);
}

void mp2_player_prop_get_volume(
	DBusMessageIter *iter,
	struct player *player
) {
	double volume = pow(10, player->gain / 20);
	dbus_message_iter_append_basic(iter,
		DBUS_TYPE_DOUBLE, &volume);
}

void mp2_player_prop_get_position(
	DBusMessageIter *iter,
	struct player *player
) {
	struct playlist *pl = &player->pl;
	track_i *track = pl->track[pl->curr];
	track_state state = track->state(track);
	dbus_int64_t position = state.time * 1e6;
	dbus_message_iter_append_basic(iter,
		DBUS_TYPE_INT64, &position);
}

void mp2_player_prop_get_metadata(
	DBusMessageIter *iter,
	struct player *player
) {
	struct playlist *pl = &player->pl;
	track_i *track = pl->track[pl->curr];
	track_meta meta = track->meta(track);
	dbus_int64_t length = meta.length * 1e6;

	DBusMessageIter dict;
	iter_init_dict(iter, &dict);

	char buf[] = "/org/mpris/MediaPlayer2/track/??????";
	snprintf(buf, sizeof buf,
		"/org/mpris/MediaPlayer2/track/%d", pl->curr);
	const char *obj = buf;
	iter_dict_append_basic(&dict,
		"mpris:trackid", DBUS_TYPE_STRING, &obj);

	iter_dict_append_basic(&dict,
		"mpris:length", DBUS_TYPE_INT64, &length);
	
	if (meta.album) {
		iter_dict_append_basic(&dict,
			"xesam:album", DBUS_TYPE_STRING, &meta.album);
	}
	
	if (meta.artist) {
		DBusMessageIter entry, variant, array;
		iter_dict_open_entry(&dict, &entry, &variant,
			"xesam:artist", "as");
		dbus_message_iter_open_container(&variant,
			DBUS_TYPE_ARRAY, "s", &array);
		dbus_message_iter_append_basic(&array,
			DBUS_TYPE_STRING, &meta.artist);
		dbus_message_iter_close_container(&variant, &array);
		iter_dict_close_entry(&dict, &entry, &variant);
	}
	
	if (meta.title) {
		iter_dict_append_basic(&dict,
			"xesam:title", DBUS_TYPE_STRING, &meta.title);
	}
	
	if (meta.tracknumber) {
		dbus_int32_t track_number = atoi(meta.tracknumber);
		iter_dict_append_basic(&dict,
			"xesam:trackNumber", DBUS_TYPE_INT32, &track_number);
	}

	iter_close_dict(iter, &dict);
}

void signal_metadata_update(
	DBusConnection *conn,
	struct player *player
) {
	signal_prop_change_one_callback(conn,
		"org.mpris.MediaPlayer2.Player",
		"Metadata", "a{sv}",
		mp2_player_prop_get_metadata, player
	);
}

DBusHandlerResult mp2_player_msg(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	struct player *player = user_data;
	if (dbus_message_has_member(msg, "Next")) {
		mtx_lock(&player->lock);
		struct playlist *pl = &player->pl;
		track_i *track;
		track = pl->track[pl->curr];
		track->seek(track, 0, SEEK_SET);
		switch (player->play_mode) {
		case playlist:
		case single:
			pl->curr++;
			if (pl->curr >= pl->size) {
				pl->curr = 0;
				pa_operation *op = pa_stream_cork(player->stream,
					1, NULL, NULL);
				pa_operation_unref(op);
				const char *playback_status = "Stopped";
				signal_prop_change_one_basic(conn,
					"org.mpris.MediaPlayer2.Player",
					"PlaybackStatus",
					DBUS_TYPE_STRING, &playback_status
				);
			}
			signal_metadata_update(conn, player);
			break;
		case repeat:
			pl->curr++;
			pl->curr %= pl->size;
			signal_metadata_update(conn, player);
			break;
		case repeat_one: break;
		}
		mtx_unlock(&player->lock);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "Previous")) {
		mtx_lock(&player->lock);
		struct playlist *pl = &player->pl;
		track_i *track;
		track = pl->track[pl->curr];
		track->seek(track, 0, SEEK_SET);
		switch (player->play_mode) {
		case playlist:
		case single:
			pl->curr--;
			if (pl->curr < 0) {
				pl->curr = 0;
				pa_operation *op = pa_stream_cork(player->stream,
					1, NULL, NULL);
				pa_operation_unref(op);
				const char *playback_status = "Stopped";
				signal_prop_change_one_basic(conn,
					"org.mpris.MediaPlayer2.Player",
					"PlaybackStatus",
					DBUS_TYPE_STRING, &playback_status
				);
			}
			signal_metadata_update(conn, player);
			break;
		case repeat:
			pl->curr--;
			pl->curr %= pl->size;
			signal_metadata_update(conn, player);
			break;
		case repeat_one: break;
		}
		mtx_unlock(&player->lock);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "Pause")) {
		const char *status = player_playback_status(player);
		if (status == PlaybackPaused) {
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		pa_operation *op = pa_stream_cork(player->stream, 1, NULL, NULL);
		pa_operation_unref(op);
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"PlaybackStatus",
			DBUS_TYPE_STRING, &PlaybackPaused
		);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "PlayPause")) {
		const char *status = player_playback_status(player);
		pa_operation *op = pa_stream_cork(player->stream,
			(status == PlaybackPlaying) ? 1 : 0, NULL, NULL);
		pa_operation_unref(op);
		const char *new_status =
			(status == PlaybackPlaying) ? PlaybackPaused
			: PlaybackPlaying;
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"PlaybackStatus",
			DBUS_TYPE_STRING, &new_status
		);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "Stop")) {
		const char *status = player_playback_status(player);
		if (status == PlaybackStopped) {
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		mtx_lock(&player->lock);
		struct playlist *pl = &player->pl;
		track_i *track = pl->track[pl->curr];
		track->seek(track, 0, SEEK_SET);
		pl->curr = 0;
		pa_operation *op = pa_stream_cork(player->stream, 1, NULL, NULL);
		pa_operation_unref(op);
		mtx_unlock(&player->lock);
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"PlaybackStatus",
			DBUS_TYPE_STRING, &PlaybackStopped
		);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "Play")) {
		const char *status = player_playback_status(player);
		if (status == PlaybackPlaying) {
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		pa_operation *op = pa_stream_cork(player->stream, 0, NULL, NULL);
		pa_operation_unref(op);
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"PlaybackStatus",
			DBUS_TYPE_STRING, &PlaybackPlaying
		);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	DBusMessage *reply;
	if (dbus_message_has_member(msg, "Seek")) {
		dbus_int64_t offset;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_INT64, &offset,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		mtx_lock(&player->lock);
		struct playlist *pl = &player->pl;
		track_i *track = pl->track[pl->curr];
		track->seek(track, offset / 1e6, SEEK_CUR);
		track_state state = track->state(track);
		dbus_int64_t position = state.time * 1e6;
		mtx_unlock(&player->lock);

		signal_seeked(conn, position);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "SetPosition")) {
		const char *trackid;
		dbus_int64_t position;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_STRING, &trackid,
			DBUS_TYPE_INT64, &position,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		struct player *player = user_data;
		struct playlist *pl = &player->pl;
		mtx_lock(&player->lock);
		track_i *track = pl->track[pl->curr];
		char buf[] = "/org/mpris/MediaPlayer2/track/??????";
		snprintf(buf, sizeof buf,
			"/org/mpris/MediaPlayer2/track/%d", pl->curr);
		if (!strcmp(buf, trackid)) {
			track->seek(track, position / 1e6, SEEK_SET);
		}
		mtx_unlock(&player->lock);

		signal_seeked(conn, position);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_has_member(msg, "OpenUri")) {
		const char *uri;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_STRING, &uri,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.UnsupportedUriScheme",
			"Unsupported URI scheme"
		);
		goto send_reply;
	}
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidMethod",
		"Invalid method"
	);
send_reply:
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_player_prop_get(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data,
	const char *property
) {
	struct player *player = user_data;
	mtx_lock(&player->lock);
	DBusMessage *reply = dbus_message_new_method_return(msg);
	DBusMessageIter iter;
	dbus_message_iter_init_append(reply, &iter);
	if (!strcmp(property, "PlaybackStatus")) {
		mp2_player_prop_get_playback_status(&iter, player);
	}
	else if (!strcmp(property, "LoopStatus")) {
		mp2_player_prop_get_loop_status(&iter, player);
	}
	// else if (!strcmp(property, "Shuffle")) {
	// 	dbus_message_append_args(reply,
	// 		DBUS_TYPE_BOOLEAN, &mp2_player_props.Shuffle,
	// 		DBUS_TYPE_INVALID
	// 	);
	// 	goto send_reply;
	// }
	else if (!strcmp(property, "Volume")) {
		mp2_player_prop_get_volume(&iter, player);
	}
	else if (!strcmp(property, "Position")) {
		mp2_player_prop_get_position(&iter, player);
	}
	else if (!strcmp(property, "Rate")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_DOUBLE, &mp2_player_props.Rate,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "MinimumRate")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_DOUBLE, &mp2_player_props.MinimumRate,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "MaximumRate")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_DOUBLE, &mp2_player_props.MaximumRate,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanGoNext")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanGoNext,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanGoPrevious")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanGoPrevious,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanPlay")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanPlay,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanPause")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanPause,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanSeek")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanSeek,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "CanControl")) {
		dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &mp2_player_props.CanControl,
			DBUS_TYPE_INVALID
		);
	}
	else if (!strcmp(property, "Metadata")) {
		mp2_player_prop_get_metadata(&iter, player);
	}
	else {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.InvalidProperty",
			"Invalid property"
		);
	}
	mtx_unlock(&player->lock);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_player_prop_set(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data,
	const char *property
) {
	struct player *player = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_next(&iter);
	dbus_message_iter_next(&iter);
	if (!strcmp(property, "LoopStatus")) {
		if (dbus_message_iter_get_arg_type(&iter) !=
			DBUS_TYPE_VARIANT) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		DBusMessageIter variant;
		dbus_message_iter_recurse(&iter, &variant);
		if (dbus_message_iter_get_arg_type(&variant) !=
			DBUS_TYPE_STRING) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		const char *new_status;
		dbus_message_iter_get_basic(&variant, &new_status);
		mtx_lock(&player->lock);
		const char *status = player_loop_status(player);
		if (!strcmp(status, new_status)) {
			mtx_unlock(&player->lock);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"LoopStatus",
			DBUS_TYPE_STRING, &PlaybackPlaying
		);
		if (!strcmp(new_status, LoopNone)) {
			player->play_mode = playlist;
			mtx_unlock(&player->lock);
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		if (!strcmp(new_status, LoopTrack)) {
			player->play_mode = repeat_one;
			mtx_unlock(&player->lock);
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		if (!strcmp(new_status, LoopPlaylist)) {
			player->play_mode = repeat;
			mtx_unlock(&player->lock);
			reply_nothing(conn, msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		mtx_unlock(&player->lock);
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.InvalidArgument",
			"Invalid argument"
		);
		fprintf(stderr, "%s\n", new_status);
		goto send_reply;
	}
	// if (!strcmp(property, "Shuffle")) {
	// }
	if (!strcmp(property, "Volume")) {
		if (dbus_message_iter_get_arg_type(&iter) !=
			DBUS_TYPE_VARIANT) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		DBusMessageIter variant;
		dbus_message_iter_recurse(&iter, &variant);
		if (dbus_message_iter_get_arg_type(&variant) !=
			DBUS_TYPE_DOUBLE) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		double new_volume;
		dbus_message_iter_get_basic(&variant, &new_volume);
		float new_gain = 20*log(new_volume);
		mtx_lock(&player->lock);
		player->gain = new_gain;
		signal_prop_change_one_basic(conn,
			"org.mpris.MediaPlayer2.Player",
			"Volume",
			DBUS_TYPE_DOUBLE, &new_volume
		);
		mtx_unlock(&player->lock);
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (!strcmp(property, "Rate")) {
		if (dbus_message_iter_get_arg_type(&iter) !=
			DBUS_TYPE_VARIANT) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		DBusMessageIter variant;
		dbus_message_iter_recurse(&iter, &variant);
		if (dbus_message_iter_get_arg_type(&variant) !=
			DBUS_TYPE_DOUBLE) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		double new_rate;
		dbus_message_iter_get_basic(&variant, &new_rate);
		if (new_rate != 1) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidArgument",
				"Invalid argument"
			);
			goto send_reply;
		}
		reply_nothing(conn, msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidProperty",
		"Invalid property"
	);
send_reply:
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult mp2_player_prop_getall(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	struct player *player = user_data;
	mtx_lock(&player->lock);
	DBusMessage *reply;
	reply = dbus_message_new_method_return(msg);
	DBusMessageIter iter, dict, entry, variant;
	dbus_message_iter_init_append(reply, &iter);
	iter_init_dict(&iter, &dict);

	iter_dict_append_callback(&dict,
		"PlaybackStatus", "s",
		mp2_player_prop_get_playback_status, player
	);

	iter_dict_append_callback(&dict,
		"LoopStatus", "s",
		mp2_player_prop_get_loop_status, player
	);

	// iter_dict_append_basic(&dict,
	// 	"Shuffle", DBUS_TYPE_BOOLEAN, &mp2_player_props.Shuffle);

	iter_dict_append_callback(&dict,
		"Volume", "d",
		mp2_player_prop_get_volume, player
	);

	iter_dict_append_callback(&dict,
		"Position", "x",
		mp2_player_prop_get_position, player
	);

	iter_dict_append_basic(&dict,
		"Rate", DBUS_TYPE_DOUBLE, &mp2_player_props.Rate);

	iter_dict_append_basic(&dict,
		"MinimumRate", DBUS_TYPE_DOUBLE, &mp2_player_props.MinimumRate);

	iter_dict_append_basic(&dict,
		"MaximumRate", DBUS_TYPE_DOUBLE, &mp2_player_props.MaximumRate);

	iter_dict_append_basic(&dict,
		"CanGoNext", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanGoNext);

	iter_dict_append_basic(&dict,
		"CanGoPrevious", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanGoPrevious);

	iter_dict_append_basic(&dict,
		"CanPlay", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanPlay);

	iter_dict_append_basic(&dict,
		"CanPause", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanPause);

	iter_dict_append_basic(&dict,
		"CanSeek", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanSeek);

	iter_dict_append_basic(&dict,
		"CanControl", DBUS_TYPE_BOOLEAN, &mp2_player_props.CanControl);

	iter_dict_append_callback(&dict,
		"Metadata", "a{sv}",
		mp2_player_prop_get_metadata, player
	);

	iter_close_dict(&iter, &dict);
	mtx_unlock(&player->lock);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult prop_msg(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	DBusMessage *reply;
	if (dbus_message_has_member(msg, "Get")) {
		const char *interface, *property;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_STRING, &interface,
			DBUS_TYPE_STRING, &property,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2")) {
			return mp2_prop_get(conn, msg, user_data, property);
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2.Player")) {
			return mp2_player_prop_get(conn, msg, user_data, property);
		}
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.InvalidInterface",
			"Invalid interface"
		);
		goto send_reply;
	}
	if (dbus_message_has_member(msg, "Set")) {
		const char *interface, *property;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_STRING, &interface,
			DBUS_TYPE_STRING, &property,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2")) {
			return mp2_prop_set(conn, msg, user_data, property);
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2.Player")) {
			return mp2_player_prop_set(conn, msg, user_data, property);
		}
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.InvalidInterface",
			"Invalid interface"
		);
		goto send_reply;
	}
	if (dbus_message_has_member(msg, "GetAll")) {
		const char *interface;
		DBusError dbuserr = {};
		dbus_bool_t ok = dbus_message_get_args(msg, &dbuserr,
			DBUS_TYPE_STRING, &interface,
			DBUS_TYPE_INVALID
		);
		if (!ok) {
			reply = dbus_message_new_error(msg,
				"org.mpris.MediaPlayer2.poppy.Error.InvalidCall",
				"Malformed method call"
			);
			goto send_reply;
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2")) {
			return mp2_prop_getall(conn, msg, user_data);
		}
		if (!strcmp(interface, "org.mpris.MediaPlayer2.Player")) {
			return mp2_player_prop_getall(conn, msg, user_data);
		}
		reply = dbus_message_new_error(msg,
			"org.mpris.MediaPlayer2.poppy.Error.InvalidInterface",
			"Invalid interface"
		);
		goto send_reply;
	}
	reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.UnsupportedMethod",
		"Unsupported method"
	);
send_reply:
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult obj_msg(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	if (dbus_message_has_interface(msg,
		"org.mpris.MediaPlayer2"))
		return mp2_msg(conn, msg, user_data);
	if (dbus_message_has_interface(msg,
		"org.mpris.MediaPlayer2.Player"))
		return mp2_player_msg(conn, msg, user_data);
	if (dbus_message_has_interface(msg,
		"org.freedesktop.DBus.Properties"))
		return prop_msg(conn, msg, user_data);
	if (dbus_message_get_no_reply(msg))
		return DBUS_HANDLER_RESULT_HANDLED;
	DBusMessage *reply = dbus_message_new_error(msg,
		"org.mpris.MediaPlayer2.poppy.Error.InvalidInterface",
		"Invalid interface"
	);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusObjectPathVTable mp2_vtable = (DBusObjectPathVTable) {
	.unregister_function = unregister_noop,
	.message_function    = obj_msg,
};

int dbus_main(void *_player) {
	struct player *player = _player;
	dbus_threads_init_default();

	DBusError dbuserr = {};
	DBusConnection *conn =
		dbus_bus_get_private(DBUS_BUS_SESSION, &dbuserr);
	if (!conn) {
		fprintf(stderr,
			"dbus_bus_get_private: %s: %s\n",
			dbuserr.name,
			dbuserr.message
		);
		return -1;
	}

	int ret = dbus_bus_request_name(
		conn,
		"org.mpris.MediaPlayer2.poppy",
		DBUS_NAME_FLAG_DO_NOT_QUEUE,
		&dbuserr
	);
	if (ret == -1) {
		fprintf(stderr,
			"dbus_bus_request_name: "
			"unable to register \"%s\": "
			"%s: "
			"%s\n",
			"org.mpris.MediaPlayer2.poppy",
			dbuserr.name,
			dbuserr.message
		);
		goto close_conn;
	}

	// switch (ret) {
	// case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
	// 	fprintf(stderr, "dbus_bus_request_name: "
	// 		"DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER\n");
	// 	break;
	// case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
	// 	fprintf(stderr, "dbus_bus_request_name: "
	// 		"DBUS_REQUEST_NAME_REPLY_IN_QUEUE\n");
	// 	break;
	// case DBUS_REQUEST_NAME_REPLY_EXISTS:
	// 	fprintf(stderr, "dbus_bus_request_name: "
	// 		"DBUS_REQUEST_NAME_REPLY_EXISTS\n");
	// 	break;
	// case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
	// 	fprintf(stderr, "dbus_bus_request_name: "
	// 		"DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER\n");
	// 	break;
	// }

	ret = dbus_connection_register_object_path(
		conn,
		"/org/mpris/MediaPlayer2",
		&mp2_vtable,
		player
	);
	if (ret == -1) {
		fprintf(stderr,
			"dbus_connection_register_object_path: "
			"unable to register \"%s\"\n",
			"/org/mpris/MediaPlayer2"
		);
		goto release_name;
	}

	player->conn = conn;
	while (!player->stream);

	while (dbus_connection_read_write_dispatch(conn, 1));

release_name:
	dbus_bus_release_name(conn,
		"org.mpris.MediaPlayer2.poppy",
		&dbuserr);
close_conn:
	dbus_connection_close(conn);
	return 0;
}
