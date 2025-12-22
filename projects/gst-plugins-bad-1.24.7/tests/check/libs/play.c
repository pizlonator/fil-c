/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* TODO:
 * - start with pause, go to playing
 * - play, pause, play
 * - set uri in play/pause
 * - play/pause after eos
 * - seek in play/pause/stopped, after eos, back to 0, after duration
 * - http buffering
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#define SOUP_VERSION_MIN_REQUIRED (SOUP_VERSION_2_40)
#include <libsoup/soup.h>
#if !defined(SOUP_MINOR_VERSION) || SOUP_MINOR_VERSION < 44
#define SoupStatus SoupKnownStatusCode
#endif

#include <gst/check/gstcheck.h>

#define fail_unless_equals_int(a, b)                                    \
G_STMT_START {                                                          \
  int first = a;                                                        \
  int second = b;                                                       \
  fail_unless(first == second,                                          \
    "'" #a "' (%d) is not equal to '" #b"' (%d)", first, second);       \
} G_STMT_END;

#define fail_unless_equals_uint64(a, b)                                 \
G_STMT_START {                                                          \
  guint64 first = a;                                                    \
  guint64 second = b;                                                   \
  fail_unless(first == second,                                          \
    "'" #a "' (%" G_GUINT64_FORMAT ") is not equal to '" #b"' (%"       \
    G_GUINT64_FORMAT ")", first, second);                               \
} G_STMT_END;

#define fail_unless_equals_double(a, b)                                 \
G_STMT_START {                                                          \
  double first = a;                                                     \
  double second = b;                                                    \
  fail_unless(first == second,                                          \
    "'" #a "' (%lf) is not equal to '" #b"' (%lf)", first, second);     \
} G_STMT_END;

#include <gst/play/play.h>

START_TEST (test_create_and_free)
{
  GstPlay *player;

  player = gst_play_new (NULL);
  fail_unless (player != NULL);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_set_and_get_uri)
{
  GstPlay *player;
  gchar *uri;

  player = gst_play_new (NULL);

  fail_unless (player != NULL);

  gst_play_set_uri (player, "file:///path/to/a/file");
  uri = gst_play_get_uri (player);

  fail_unless (g_strcmp0 (uri, "file:///path/to/a/file") == 0);

  g_free (uri);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_set_and_get_position_update_interval)
{
  GstPlay *player;
  guint interval = 0;
  GstStructure *config;

  player = gst_play_new (NULL);

  fail_unless (player != NULL);

  config = gst_play_get_config (player);
  gst_play_config_set_position_update_interval (config, 500);
  interval = gst_play_config_get_position_update_interval (config);
  fail_unless (interval == 500);
  gst_play_set_config (player, config);

  g_object_unref (player);
}

END_TEST;

typedef enum
{
  STATE_CHANGE_BUFFERING,
  STATE_CHANGE_DURATION_CHANGED,
  STATE_CHANGE_END_OF_STREAM,
  STATE_CHANGE_ERROR,
  STATE_CHANGE_WARNING,
  STATE_CHANGE_POSITION_UPDATED,
  STATE_CHANGE_STATE_CHANGED,
  STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED,
  STATE_CHANGE_MEDIA_INFO_UPDATED,
  STATE_CHANGE_SEEK_DONE,
  STATE_CHANGE_URI_LOADED,
} TestPlayerStateChange;

static const gchar *
test_play_state_change_get_name (TestPlayerStateChange change)
{
  switch (change) {
    case STATE_CHANGE_BUFFERING:
      return "buffering";
    case STATE_CHANGE_DURATION_CHANGED:
      return "duration-changed";
    case STATE_CHANGE_END_OF_STREAM:
      return "end-of-stream";
    case STATE_CHANGE_WARNING:
      return "warning";
    case STATE_CHANGE_ERROR:
      return "error";
    case STATE_CHANGE_POSITION_UPDATED:
      return "position-updated";
    case STATE_CHANGE_STATE_CHANGED:
      return "state-changed";
    case STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED:
      return "video-dimensions-changed";
    case STATE_CHANGE_MEDIA_INFO_UPDATED:
      return "media-info-updated";
    case STATE_CHANGE_SEEK_DONE:
      return "seek-done";
    case STATE_CHANGE_URI_LOADED:
      return "uri-loaded";
    default:
      g_assert_not_reached ();
      break;
  }
}

typedef struct _TestPlayerState TestPlayerState;
struct _TestPlayerState
{
  gint buffering_percent;
  guint64 position, duration, seek_done_position;
  gboolean end_of_stream, is_error, is_warning, seek_done;
  GstPlayState state;
  guint width, height;
  GstPlayMediaInfo *media_info;
  gchar *uri_loaded;
  GstClockTime last_position;
  gboolean done;
  GError *error;
  GstStructure *error_details;

  void (*test_callback) (GstPlay * player, TestPlayerStateChange change,
      TestPlayerState * old_state, TestPlayerState * new_state);
  gpointer test_data;
};

static void process_play_messages (GstPlay * player, TestPlayerState * state);

static void
test_play_state_change_debug (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  GST_DEBUG_OBJECT (player, "Changed %s:\n"
      "\tbuffering %d%% -> %d%%\n"
      "\tposition %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "\n"
      "\tduration %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "\n"
      "\tseek position %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "\n"
      "\tend-of-stream %d -> %d\n"
      "\terror %d -> %d\n"
      "\tseek_done %d -> %d\n"
      "\tstate %s -> %s\n"
      "\twidth/height %d/%d -> %d/%d\n"
      "\tmedia_info %p -> %p\n"
      "\turi_loaded %s -> %s",
      test_play_state_change_get_name (change),
      old_state->buffering_percent, new_state->buffering_percent,
      GST_TIME_ARGS (old_state->position), GST_TIME_ARGS (new_state->position),
      GST_TIME_ARGS (old_state->duration), GST_TIME_ARGS (new_state->duration),
      GST_TIME_ARGS (old_state->seek_done_position),
      GST_TIME_ARGS (new_state->seek_done_position), old_state->end_of_stream,
      new_state->end_of_stream, old_state->is_error, new_state->is_error,
      old_state->seek_done, new_state->seek_done,
      gst_play_state_get_name (old_state->state),
      gst_play_state_get_name (new_state->state), old_state->width,
      old_state->height, new_state->width, new_state->height,
      old_state->media_info, new_state->media_info,
      old_state->uri_loaded, new_state->uri_loaded);
}

static void
test_play_state_reset (GstPlay * player, TestPlayerState * state)
{
  state->buffering_percent = 100;
  state->position = state->duration = state->seek_done_position = -1;
  state->end_of_stream = state->is_error = state->seek_done = FALSE;
  state->state = GST_PLAY_STATE_STOPPED;
  state->width = state->height = 0;
  state->media_info = NULL;
  state->last_position = GST_CLOCK_TIME_NONE;
  state->done = FALSE;
  g_clear_pointer (&state->uri_loaded, g_free);
  g_clear_error (&state->error);
  gst_clear_structure (&state->error_details);
}

static GstPlay *
test_play_new (TestPlayerState * state)
{
  GstPlay *player;
  GstElement *playbin, *fakesink;

  player = gst_play_new (NULL);
  fail_unless (player != NULL);

  test_play_state_reset (player, state);

  playbin = gst_play_get_pipeline (player);
  fakesink = gst_element_factory_make ("fakesink", "audio-sink");
  g_object_set (fakesink, "sync", TRUE, NULL);
  g_object_set (playbin, "audio-sink", fakesink, NULL);
  fakesink = gst_element_factory_make ("fakesink", "video-sink");
  g_object_set (fakesink, "sync", TRUE, NULL);
  g_object_set (playbin, "video-sink", fakesink, NULL);
  gst_object_unref (playbin);

  return player;
}

static void
test_play_stopped_cb (GstPlay * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  if (new_state->state == GST_PLAY_STATE_STOPPED) {
    new_state->done = TRUE;
  }
}

static void
stop_player (GstPlay * player, TestPlayerState * state)
{
  if (state->state != GST_PLAY_STATE_STOPPED) {
    /* Make sure all pending operations are finished so the player won't be
     * appear as 'leaked' to leak detection tools. */
    state->test_callback = test_play_stopped_cb;
    gst_play_stop (player);
    state->done = FALSE;
    process_play_messages (player, state);
  }
  test_play_state_reset (player, state);
}

static void
test_play_audio_video_eos_cb (GstPlay * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);
  gboolean video;

  video = !!(step & 0x10);
  step = (step & (~0x10));

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_URI_LOADED);
      if (video)
        fail_unless (g_str_has_suffix (new_state->uri_loaded,
                "audio-video-short.ogg"));
      else
        fail_unless (g_str_has_suffix (new_state->uri_loaded,
                "audio-short.ogg"));
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_BUFFERING);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 3:
      fail_unless_equals_int (change, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED);
      if (video) {
        fail_unless_equals_int (new_state->width, 320);
        fail_unless_equals_int (new_state->height, 240);
      } else {
        fail_unless_equals_int (new_state->width, 0);
        fail_unless_equals_int (new_state->height, 0);
      }
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 4:
      fail_unless_equals_int (change, STATE_CHANGE_DURATION_CHANGED);
      fail_unless_equals_uint64 (new_state->duration,
          G_GUINT64_CONSTANT (464399092));
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 5:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 6:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_PLAYING);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 7:
      fail_unless_equals_int (change, STATE_CHANGE_POSITION_UPDATED);
      g_assert (new_state->position <= old_state->duration);
      if (new_state->position == old_state->duration)
        new_state->test_data =
            GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 8:
      fail_unless_equals_int (change, STATE_CHANGE_END_OF_STREAM);
      fail_unless_equals_uint64 (new_state->position, old_state->duration);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 9:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_PLAYING);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_STOPPED);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      new_state->done = TRUE;
      break;
    default:
      fail ();
      break;
  }
}

void
process_play_messages (GstPlay * player, TestPlayerState * state)
{
  GstBus *bus = gst_play_get_message_bus (player);
  do {
    GstMessage *msg =
        gst_bus_timed_pop_filtered (bus, -1, GST_MESSAGE_APPLICATION);
    GST_INFO ("message: %" GST_PTR_FORMAT, msg);

    if (gst_play_is_play_message (msg)) {
      TestPlayerState old_state = *state;
      GstPlayMessage type;

      gst_play_message_parse_type (msg, &type);
      switch (type) {
        case GST_PLAY_MESSAGE_URI_LOADED:
          state->uri_loaded = gst_play_get_uri (player);
          state->test_callback (player, STATE_CHANGE_URI_LOADED, &old_state,
              state);
          break;
        case GST_PLAY_MESSAGE_POSITION_UPDATED:
          gst_play_message_parse_position_updated (msg, &state->position);
          test_play_state_change_debug (player, STATE_CHANGE_POSITION_UPDATED,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_POSITION_UPDATED,
              &old_state, state);
          break;
        case GST_PLAY_MESSAGE_DURATION_CHANGED:{
          GstClockTime duration;
          gst_play_message_parse_duration_updated (msg, &duration);
          state->duration = duration;
          test_play_state_change_debug (player, STATE_CHANGE_DURATION_CHANGED,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_DURATION_CHANGED,
              &old_state, state);
          break;
        }
        case GST_PLAY_MESSAGE_STATE_CHANGED:{
          GstPlayState play_state;
          gst_play_message_parse_state_changed (msg, &play_state);

          state->state = play_state;

          if (play_state == GST_PLAY_STATE_STOPPED)
            test_play_state_reset (player, state);

          test_play_state_change_debug (player, STATE_CHANGE_STATE_CHANGED,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_STATE_CHANGED, &old_state,
              state);
          break;
        }
        case GST_PLAY_MESSAGE_BUFFERING:{
          guint percent;
          gst_play_message_parse_buffering_percent (msg, &percent);

          state->buffering_percent = percent;
          test_play_state_change_debug (player, STATE_CHANGE_BUFFERING,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_BUFFERING, &old_state,
              state);
          break;
        }
        case GST_PLAY_MESSAGE_END_OF_STREAM:
          state->end_of_stream = TRUE;
          test_play_state_change_debug (player, STATE_CHANGE_END_OF_STREAM,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_END_OF_STREAM, &old_state,
              state);
          break;
        case GST_PLAY_MESSAGE_ERROR:{
          gst_play_message_parse_error (msg, &state->error,
              &state->error_details);
          GST_DEBUG ("error: %s details: %" GST_PTR_FORMAT,
              state->error ? state->error->message : "", state->error_details);
          state->is_error = TRUE;
          test_play_state_change_debug (player, STATE_CHANGE_ERROR,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_ERROR, &old_state, state);
          break;
        }
        case GST_PLAY_MESSAGE_WARNING:{
          gst_play_message_parse_error (msg, &state->error,
              &state->error_details);
          GST_DEBUG ("error: %s details: %" GST_PTR_FORMAT,
              state->error ? state->error->message : "", state->error_details);
          state->is_warning = TRUE;
          test_play_state_change_debug (player, STATE_CHANGE_WARNING,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_WARNING, &old_state,
              state);
          break;
        }
        case GST_PLAY_MESSAGE_VIDEO_DIMENSIONS_CHANGED:{
          guint width, height;
          gst_play_message_parse_video_dimensions_changed (msg, &width,
              &height);

          state->width = width;
          state->height = height;
          test_play_state_change_debug (player,
              STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED, &old_state, state);
          state->test_callback (player, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED,
              &old_state, state);
          break;
        }
        case GST_PLAY_MESSAGE_MEDIA_INFO_UPDATED:{
          GstPlayMediaInfo *media_info;
          gst_play_message_parse_media_info_updated (msg, &media_info);

          state->media_info = media_info;
          test_play_state_change_debug (player,
              STATE_CHANGE_MEDIA_INFO_UPDATED, &old_state, state);
          state->test_callback (player, STATE_CHANGE_MEDIA_INFO_UPDATED,
              &old_state, state);
          break;
        }
        case GST_PLAY_MESSAGE_VOLUME_CHANGED:{
          gdouble volume;
          gst_play_message_parse_volume_changed (msg, &volume);
          break;
        }
        case GST_PLAY_MESSAGE_MUTE_CHANGED:{
          gboolean is_muted;
          gst_play_message_parse_muted_changed (msg, &is_muted);
          break;
        }
        case GST_PLAY_MESSAGE_SEEK_DONE:
          state->seek_done = TRUE;
          state->seek_done_position = gst_play_get_position (player);
          test_play_state_change_debug (player, STATE_CHANGE_SEEK_DONE,
              &old_state, state);
          state->test_callback (player, STATE_CHANGE_SEEK_DONE, &old_state,
              state);
          break;
      }
    }
    gst_message_unref (msg);
    if (state->done)
      break;
  } while (1);
  gst_object_unref (bus);
}

START_TEST (test_play_audio_eos)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_audio_video_eos_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-short.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 10);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_audio_info (GstPlayMediaInfo * media_info)
{
  gint i = 0;
  GList *list;

  for (list = gst_play_media_info_get_audio_streams (media_info);
      list != NULL; list = list->next) {
    GstPlayStreamInfo *stream = (GstPlayStreamInfo *) list->data;
    GstPlayAudioInfo *audio_info = (GstPlayAudioInfo *) stream;

    fail_unless (gst_play_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_play_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_string (gst_play_stream_info_get_stream_type (stream),
        "audio");

    if (i == 0) {
      fail_unless_equals_string (gst_play_stream_info_get_codec (stream),
          "MPEG-1 Layer 3 (MP3)");
      fail_unless_equals_int (gst_play_audio_info_get_sample_rate
          (audio_info), 48000);
      fail_unless_equals_int (gst_play_audio_info_get_channels (audio_info), 2);
      fail_unless_equals_int (gst_play_audio_info_get_max_bitrate
          (audio_info), 192000);
      fail_unless (gst_play_audio_info_get_language (audio_info) != NULL);
    } else {
      fail_unless_equals_string (gst_play_stream_info_get_codec (stream),
          "MPEG-4 AAC");
      fail_unless_equals_int (gst_play_audio_info_get_sample_rate
          (audio_info), 48000);
      fail_unless_equals_int (gst_play_audio_info_get_channels (audio_info), 6);
      fail_unless (gst_play_audio_info_get_language (audio_info) != NULL);
    }

    i++;
  }
}

static void
test_video_info (GstPlayMediaInfo * media_info)
{
  GList *list;

  for (list = gst_play_media_info_get_video_streams (media_info);
      list != NULL; list = list->next) {
    gint fps_d, fps_n;
    guint par_d, par_n;
    GstPlayStreamInfo *stream = (GstPlayStreamInfo *) list->data;
    GstPlayVideoInfo *video_info = (GstPlayVideoInfo *) stream;

    fail_unless (gst_play_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_play_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_int (gst_play_stream_info_get_index (stream), 0);
    fail_unless (strstr (gst_play_stream_info_get_codec (stream),
            "H.264") != NULL
        || strstr (gst_play_stream_info_get_codec (stream), "H264") != NULL);
    fail_unless_equals_int (gst_play_video_info_get_width (video_info), 320);
    fail_unless_equals_int (gst_play_video_info_get_height (video_info), 240);
    gst_play_video_info_get_framerate (video_info, &fps_n, &fps_d);
    fail_unless_equals_int (fps_n, 24);
    fail_unless_equals_int (fps_d, 1);
    gst_play_video_info_get_pixel_aspect_ratio (video_info, &par_n, &par_d);
    fail_unless_equals_int (par_n, 33);
    fail_unless_equals_int (par_d, 20);
  }
}

static void
test_subtitle_info (GstPlayMediaInfo * media_info)
{
  GList *list;

  for (list = gst_play_media_info_get_subtitle_streams (media_info);
      list != NULL; list = list->next) {
    GstPlayStreamInfo *stream = (GstPlayStreamInfo *) list->data;
    GstPlaySubtitleInfo *sub = (GstPlaySubtitleInfo *) stream;

    fail_unless_equals_string (gst_play_stream_info_get_stream_type (stream),
        "subtitle");
    fail_unless (gst_play_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_play_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_string (gst_play_stream_info_get_codec (stream),
        "Timed Text");
    fail_unless (gst_play_subtitle_info_get_language (sub) != NULL);
  }
}

static void
test_media_info_object (GstPlay * player, GstPlayMediaInfo * media_info)
{
  GList *list;

  /* global tag */
  fail_unless (gst_play_media_info_is_seekable (media_info) == TRUE);
  fail_unless (gst_play_media_info_get_tags (media_info) != NULL);
  fail_unless_equals_string (gst_play_media_info_get_title (media_info),
      "Sintel");
  fail_unless_equals_string (gst_play_media_info_get_container_format
      (media_info), "Matroska");
  fail_unless (gst_play_media_info_get_image_sample (media_info) == NULL);
  fail_unless (strstr (gst_play_media_info_get_uri (media_info),
          "sintel.mkv") != NULL);

  /* number of streams */
  list = gst_play_media_info_get_stream_list (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 10);

  list = gst_play_media_info_get_video_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 1);

  list = gst_play_media_info_get_audio_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 2);

  list = gst_play_media_info_get_subtitle_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 7);

  /* test subtitle */
  test_subtitle_info (media_info);

  /* test audio */
  test_audio_info (media_info);

  /* test video */
  test_video_info (media_info);
}

static void
test_play_media_info_cb (GstPlay * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  gint completed = GPOINTER_TO_INT (new_state->test_data);

  if (change == STATE_CHANGE_MEDIA_INFO_UPDATED) {
    test_media_info_object (player, new_state->media_info);
    new_state->test_data = GINT_TO_POINTER (completed + 1);
    new_state->done = TRUE;
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  }
}

START_TEST (test_play_media_info)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_media_info_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 1);
  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_stream_disable_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data) & 0xf;
  gint mask = GPOINTER_TO_INT (new_state->test_data) & 0xf0;

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    new_state->test_data = GINT_TO_POINTER (0x10 + steps + 1);
    gst_play_set_audio_track_enabled (player, FALSE);

  } else if (mask == 0x10 && change == STATE_CHANGE_POSITION_UPDATED) {
    GstPlayAudioInfo *audio;

    audio = gst_play_get_current_audio_track (player);
    fail_unless (audio == NULL);
    new_state->test_data = GINT_TO_POINTER (0x20 + steps + 1);
    gst_play_set_subtitle_track_enabled (player, FALSE);

  } else if (mask == 0x20 && change == STATE_CHANGE_POSITION_UPDATED) {
    GstPlaySubtitleInfo *sub;

    sub = gst_play_get_current_subtitle_track (player);
    fail_unless (sub == NULL);
    new_state->test_data = GINT_TO_POINTER (0x30 + steps + 1);
    new_state->done = TRUE;

  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  }
}

START_TEST (test_play_stream_disable)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_stream_disable_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 0x33);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_stream_switch_audio_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    gint ret;

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    ret = gst_play_set_audio_track (player, 1);
    fail_unless_equals_int (ret, 1);

  } else if (steps && change == STATE_CHANGE_POSITION_UPDATED) {
    gint index;
    GstPlayAudioInfo *audio;

    audio = gst_play_get_current_audio_track (player);
    fail_unless (audio != NULL);
    index = gst_play_stream_info_get_index ((GstPlayStreamInfo *) audio);
    fail_unless_equals_int (index, 1);
    g_object_unref (audio);

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;

  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  }
}

START_TEST (test_play_stream_switch_audio)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_stream_switch_audio_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_stream_switch_subtitle_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    gint ret;

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    ret = gst_play_set_subtitle_track (player, 5);
    fail_unless_equals_int (ret, 1);

  } else if (steps && change == STATE_CHANGE_POSITION_UPDATED) {
    gint index;
    GstPlaySubtitleInfo *sub;

    sub = gst_play_get_current_subtitle_track (player);
    fail_unless (sub != NULL);
    index = gst_play_stream_info_get_index ((GstPlayStreamInfo *) sub);
    fail_unless_equals_int (index, 5);
    g_object_unref (sub);

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  }
}

START_TEST (test_play_stream_switch_subtitle)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_stream_switch_subtitle_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_error_invalid_external_suburi_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    gchar *suburi;

    suburi = gst_filename_to_uri (TEST_PATH "/foo.srt", NULL);
    fail_unless (suburi != NULL);

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    /* load invalid suburi */
    gst_play_set_subtitle_uri (player, suburi);
    g_free (suburi);

  } else if (steps && change == STATE_CHANGE_WARNING) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;
  }
}

START_TEST (test_play_error_invalid_external_suburi)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_error_invalid_external_suburi_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static gboolean
has_subtitle_stream (TestPlayerState * new_state)
{
  if (gst_play_media_info_get_subtitle_streams (new_state->media_info))
    return TRUE;

  return FALSE;
}

static void
test_play_external_suburi_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    gchar *suburi;

    suburi = gst_filename_to_uri (TEST_PATH "/test_sub.srt", NULL);
    fail_unless (suburi != NULL);

    gst_play_set_subtitle_uri (player, suburi);
    g_free (suburi);
    new_state->test_data = GINT_TO_POINTER (steps + 1);

  } else if (change == STATE_CHANGE_MEDIA_INFO_UPDATED &&
      has_subtitle_stream (new_state)) {
    gchar *current_suburi, *suburi;

    current_suburi = gst_play_get_subtitle_uri (player);
    fail_unless (current_suburi != NULL);
    suburi = gst_filename_to_uri (TEST_PATH "/test_sub.srt", NULL);
    fail_unless (suburi != NULL);

    fail_unless_equals_int (g_strcmp0 (current_suburi, suburi), 0);

    g_free (current_suburi);
    g_free (suburi);
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;

  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR)
    new_state->done = TRUE;
}

START_TEST (test_play_external_suburi)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_external_suburi_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_rate_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data) & 0xf;
  gint mask = GPOINTER_TO_INT (new_state->test_data) & 0xf0;

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    guint64 dur = -1, pos = -1;

    g_object_get (player, "position", &pos, "duration", &dur, NULL);
    pos = pos + dur * 0.2;      /* seek 20% */
    gst_play_seek (player, pos);

    /* default rate should be 1.0 */
    fail_unless_equals_double (gst_play_get_rate (player), 1.0);
    new_state->test_data = GINT_TO_POINTER (mask + steps + 1);
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  } else if (steps == 1 && change == STATE_CHANGE_SEEK_DONE) {
    if (mask == 0x10)
      gst_play_set_rate (player, 1.5);
    else if (mask == 0x20)
      gst_play_set_rate (player, -1.0);

    new_state->test_data = GINT_TO_POINTER (mask + steps + 1);
  } else if (steps && (change == STATE_CHANGE_POSITION_UPDATED)) {
    if (steps == 10) {
      new_state->done = TRUE;
    } else {
      if (mask == 0x10 && (new_state->position > old_state->position))
        new_state->test_data = GINT_TO_POINTER (mask + steps + 1);
      else if (mask == 0x20 && (new_state->position < old_state->position))
        new_state->test_data = GINT_TO_POINTER (mask + steps + 1);
    }
  }
}

START_TEST (test_play_forward_rate)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_rate_cb;
  state.test_data = GINT_TO_POINTER (0x10);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data) & 0xf, 10);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_play_backward_rate)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_rate_cb;
  state.test_data = GINT_TO_POINTER (0x20);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data) & 0xf, 10);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_play_audio_video_eos)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_audio_video_eos_cb;
  state.test_data = GINT_TO_POINTER (0x10);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video-short.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data) & (~0x10), 10);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_error_invalid_uri_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_URI_LOADED);
      fail_unless_equals_string (new_state->uri_loaded, "foo://bar");
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_ERROR);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 3:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_STOPPED);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      new_state->done = TRUE;
      break;
    default:
      fail ();
      break;
  }
}

START_TEST (test_play_error_invalid_uri)
{
  GstPlay *player;
  TestPlayerState state;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_error_invalid_uri_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  gst_play_set_uri (player, "foo://bar");

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 4);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_error_invalid_uri_and_play_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);
  gchar *uri;

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_URI_LOADED);
      fail_unless_equals_string (new_state->uri_loaded, "foo://bar");
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_ERROR);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 3:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_STOPPED);
      new_state->test_data = GINT_TO_POINTER (step + 1);

      uri = gst_filename_to_uri (TEST_PATH "/audio-short.ogg", NULL);
      fail_unless (uri != NULL);
      gst_play_set_uri (player, uri);
      g_free (uri);

      gst_play_play (player);
      break;
    case 4:
      fail_unless_equals_int (change, STATE_CHANGE_URI_LOADED);
      fail_unless (g_str_has_suffix (new_state->uri_loaded, "audio-short.ogg"));
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 5:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 6:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 7:
      fail_unless_equals_int (change, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED);
      fail_unless_equals_int (new_state->width, 0);
      fail_unless_equals_int (new_state->height, 0);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 8:
      fail_unless_equals_int (change, STATE_CHANGE_DURATION_CHANGED);
      fail_unless_equals_uint64 (new_state->duration,
          G_GUINT64_CONSTANT (464399092));
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 9:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 10:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAY_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAY_STATE_PLAYING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      new_state->done = TRUE;
      break;
    default:
      fail ();
      break;
  }
}

START_TEST (test_play_error_invalid_uri_and_play)
{
  GstPlay *player;
  TestPlayerState state;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_error_invalid_uri_and_play_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  gst_play_set_uri (player, "foo://bar");

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 11);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_seek_done_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data) & (~0x10);

  if (new_state->state == GST_PLAY_STATE_PAUSED && !step) {
    gst_play_seek (player, 0);
    new_state->test_data = GINT_TO_POINTER (step + 1);
  } else if (change == STATE_CHANGE_SEEK_DONE && step == 1) {
    fail_unless_equals_int (change, STATE_CHANGE_SEEK_DONE);
    fail_unless_equals_uint64 (new_state->seek_done_position,
        G_GUINT64_CONSTANT (0));
    new_state->test_data = GINT_TO_POINTER (step + 1);
    new_state->done = TRUE;
  }
}

START_TEST (test_play_audio_video_seek_done)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_seek_done_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_pause (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data) & (~0x10), 2);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_play_position_update_interval_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAY_STATE_PLAYING && !steps) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
  } else if (steps && change == STATE_CHANGE_POSITION_UPDATED) {
    GstStructure *config = gst_play_get_config (player);
    guint update_interval =
        gst_play_config_get_position_update_interval (config);
    GstClockTime position = gst_play_get_position (player);
    new_state->test_data = GINT_TO_POINTER (steps + 1);

    gst_structure_free (config);
    if (GST_CLOCK_TIME_IS_VALID (old_state->last_position)) {
      GstClockTime delta = GST_CLOCK_DIFF (old_state->last_position, position);
      GST_DEBUG_OBJECT (player,
          "current delta: %" GST_TIME_FORMAT " interval: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (delta), GST_TIME_ARGS (update_interval));

      if (update_interval > 10) {
        fail_unless (delta > ((update_interval - 10) * GST_MSECOND)
            && delta < ((update_interval + 10) * GST_MSECOND));
      }
    }

    new_state->last_position = position;

    if (position >= 2000 * GST_MSECOND) {
      new_state->done = TRUE;
    }
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    new_state->done = TRUE;
  }
}

START_TEST (test_play_position_update_interval)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;
  GstStructure *config;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_play_position_update_interval_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  config = gst_play_get_config (player);
  gst_play_config_set_position_update_interval (config, 600);
  gst_play_set_config (player, config);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 5);

  /* Disable position updates */
  gst_play_stop (player);

  config = gst_play_get_config (player);
  gst_play_config_set_position_update_interval (config, 0);
  gst_play_set_config (player, config);
  state.last_position = GST_CLOCK_TIME_NONE;

  gst_play_play (player);
  process_play_messages (player, &state);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 6);

  stop_player (player, &state);
  g_object_unref (player);
}

END_TEST;

static void
test_restart_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (!steps && change == STATE_CHANGE_URI_LOADED) {
    fail_unless (g_str_has_suffix (new_state->uri_loaded, "sintel.mkv"));
    new_state->test_data = GINT_TO_POINTER (steps + 1);
  } else if (change == STATE_CHANGE_STATE_CHANGED
      && new_state->state == GST_PLAY_STATE_BUFFERING) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;
  }
}

static void
test_restart_cb2 (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (!steps && change == STATE_CHANGE_URI_LOADED) {
    fail_unless (g_str_has_suffix (new_state->uri_loaded, "audio-short.ogg"));
    new_state->test_data = GINT_TO_POINTER (steps + 1);
  } else if (change == STATE_CHANGE_STATE_CHANGED
      && new_state->state == GST_PLAY_STATE_BUFFERING) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    new_state->done = TRUE;
  }
}


START_TEST (test_restart)
{
  GstPlay *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.test_callback = test_restart_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_play_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);
  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);
  stop_player (player, &state);

  /* Try again with another URI */
  state.test_data = GINT_TO_POINTER (0);
  state.test_callback = test_restart_cb2;

  uri = gst_filename_to_uri (TEST_PATH "/audio-short.ogg", NULL);
  fail_unless (uri != NULL);
  gst_play_set_uri (player, uri);
  g_free (uri);

  gst_play_play (player);
  process_play_messages (player, &state);
  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);
  stop_player (player, &state);

  g_object_unref (player);
}

END_TEST;

static void
do_get (SoupMessage * msg, const char *path)
{
  char *uri;
  SoupStatus status = SOUP_STATUS_OK;

  uri = soup_uri_to_string (soup_message_get_uri (msg), FALSE);
  GST_DEBUG ("request: \"%s\"", uri);

  if (status != (SoupStatus) SOUP_STATUS_OK)
    goto beach;

  if (msg->method == SOUP_METHOD_GET) {
    char *full_path = g_strconcat (TEST_PATH, path, NULL);
    char *buf;
    gsize buflen;

    if (!g_file_get_contents (full_path, &buf, &buflen, NULL)) {
      status = SOUP_STATUS_NOT_FOUND;
      g_free (full_path);
      goto beach;
    }

    g_free (full_path);
    soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE,
        buf, buflen);
  }

beach:
  soup_message_set_status (msg, status);
  g_free (uri);
}

static void
server_callback (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query,
    SoupClientContext * context, gpointer data)
{
  GST_DEBUG ("%s %s HTTP/1.%d", msg->method, path,
      soup_message_get_http_version (msg));
  if (msg->request_body->length)
    GST_DEBUG ("%s", msg->request_body->data);

  if (msg->method == SOUP_METHOD_GET)
    do_get (msg, path);
  else
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

  GST_DEBUG ("  -> %d %s", msg->status_code, msg->reason_phrase);
}

static guint
get_port_from_server (SoupServer * server)
{
  GSList *uris;
  guint port;

  uris = soup_server_get_uris (server);
  g_assert (g_slist_length (uris) == 1);
  port = soup_uri_get_port (uris->data);
  g_slist_free_full (uris, (GDestroyNotify) soup_uri_free);

  return port;
}

typedef struct
{
  GMainLoop *loop;
  GMainContext *ctx;
  GThread *thread;
  SoupServer *server;
  GMutex lock;
  GCond cond;
} ServerContext;

static gboolean
main_loop_running_cb (gpointer data)
{
  ServerContext *context = (ServerContext *) data;

  g_mutex_lock (&context->lock);
  g_cond_signal (&context->cond);
  g_mutex_unlock (&context->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
http_main (gpointer data)
{
  ServerContext *context = (ServerContext *) data;
  GSource *source;

  context->server = soup_server_new (NULL, NULL);
  soup_server_add_handler (context->server, NULL, server_callback, NULL, NULL);

  g_main_context_push_thread_default (context->ctx);

  {
    GSocketAddress *address;
    GError *err = NULL;
    SoupServerListenOptions listen_flags = 0;

    address =
        g_inet_socket_address_new_from_string ("0.0.0.0",
        SOUP_ADDRESS_ANY_PORT);
    soup_server_listen (context->server, address, listen_flags, &err);
    g_object_unref (address);

    if (err) {
      GST_ERROR ("Failed to start HTTP server: %s", err->message);
      g_object_unref (context->server);
      g_error_free (err);
    }
  }

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, context,
      NULL);
  g_source_attach (source, context->ctx);
  g_source_unref (source);

  g_main_loop_run (context->loop);
  g_main_context_pop_thread_default (context->ctx);
  g_object_unref (context->server);
  return NULL;
}

#define TEST_USER_AGENT "test user agent"

static void
test_user_agent_cb (GstPlay * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  if (change == STATE_CHANGE_STATE_CHANGED
      && new_state->state == GST_PLAY_STATE_PAUSED) {
    GstElement *pipeline;
    GstElement *source;
    gchar *user_agent;

    pipeline = gst_play_get_pipeline (player);
    source = gst_bin_get_by_name (GST_BIN_CAST (pipeline), "source");
    g_object_get (source, "user-agent", &user_agent, NULL);
    fail_unless_equals_string (user_agent, TEST_USER_AGENT);
    g_free (user_agent);
    gst_object_unref (source);
    gst_object_unref (pipeline);
    new_state->done = TRUE;
  }
}

START_TEST (test_user_agent)
{
  GstPlay *player;
  GstStructure *config;
  gchar *user_agent;
  TestPlayerState state;
  guint port;
  gchar *url;
  ServerContext *context = g_new (ServerContext, 1);

  g_mutex_init (&context->lock);
  g_cond_init (&context->cond);
  context->ctx = g_main_context_new ();
  context->loop = g_main_loop_new (context->ctx, FALSE);
  context->server = NULL;

  g_mutex_lock (&context->lock);
  context->thread = g_thread_new ("HTTP Server", http_main, context);
  while (!g_main_loop_is_running (context->loop))
    g_cond_wait (&context->cond, &context->lock);
  g_mutex_unlock (&context->lock);

  if (context->server == NULL) {
    g_print ("Failed to start up HTTP server");
    /* skip this test */
    goto beach;
  }

  memset (&state, 0, sizeof (state));
  state.test_callback = test_user_agent_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = gst_play_new (NULL);
  fail_unless (player != NULL);

  port = get_port_from_server (context->server);
  url = g_strdup_printf ("http://127.0.0.1:%u/audio.ogg", port);
  fail_unless (url != NULL);

  gst_play_set_uri (player, url);
  g_free (url);

  config = gst_play_get_config (player);
  gst_play_config_set_user_agent (config, TEST_USER_AGENT);

  user_agent = gst_play_config_get_user_agent (config);
  fail_unless_equals_string (user_agent, TEST_USER_AGENT);
  g_free (user_agent);

  gst_play_set_config (player, config);

  gst_play_pause (player);
  process_play_messages (player, &state);

  stop_player (player, &state);
  g_object_unref (player);

beach:
  g_main_loop_quit (context->loop);
  g_thread_unref (context->thread);
  g_main_loop_unref (context->loop);
  context->loop = NULL;
  g_main_context_unref (context->ctx);
  context->ctx = NULL;
  g_free (context);
}

END_TEST;

static Suite *
play_suite (void)
{
  Suite *s = suite_create ("GstPlay");

  TCase *tc_general = tcase_create ("general");

  /* Use a longer timeout */
#ifdef HAVE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    tcase_set_timeout (tc_general, 5 * 60);
  } else
#endif
  {
    tcase_set_timeout (tc_general, 2 * 60);
  }

  tcase_add_test (tc_general, test_create_and_free);
  tcase_add_test (tc_general, test_set_and_get_uri);
  tcase_add_test (tc_general, test_set_and_get_position_update_interval);

#ifdef HAVE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
  } else
#endif
  {
    tcase_add_test (tc_general, test_play_position_update_interval);
  }
  tcase_add_test (tc_general, test_play_audio_eos);
  tcase_add_test (tc_general, test_play_audio_video_eos);
  tcase_add_test (tc_general, test_play_error_invalid_uri);
  tcase_add_test (tc_general, test_play_error_invalid_uri_and_play);
  tcase_add_test (tc_general, test_play_media_info);
  tcase_add_test (tc_general, test_play_stream_disable);
  tcase_add_test (tc_general, test_play_stream_switch_audio);
  tcase_add_test (tc_general, test_play_stream_switch_subtitle);
  tcase_add_test (tc_general, test_play_error_invalid_external_suburi);
  tcase_add_test (tc_general, test_play_external_suburi);
  tcase_add_test (tc_general, test_play_forward_rate);
  tcase_add_test (tc_general, test_play_backward_rate);
  tcase_add_test (tc_general, test_play_audio_video_seek_done);
  tcase_add_test (tc_general, test_restart);
  tcase_add_test (tc_general, test_user_agent);

  suite_add_tcase (s, tc_general);

  return s;
}

GST_CHECK_MAIN (play)
