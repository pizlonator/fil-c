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

/**
 * SECTION:gstplayer
 * @title: GstPlayer
 * @short_description: Player
 * @symbols:
 * - GstPlayer
 *
 * Starting from GStreamer 1.20, application developers are strongly advised to migrate to #GstPlay.
 * #GstPlayer will be deprecated in 1.20 and most likely removed by 1.24.
 */

/* TODO:
 *
 * - Equalizer
 * - Gapless playback
 * - Frame stepping
 * - Subtitle font, connection speed
 * - Deinterlacing
 * - Buffering control (-> progressive downloading)
 * - Playlist/queue object
 * - Custom video sink (e.g. embed in GL scene)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplayer.h"
#include "gstplayer-signal-dispatcher-private.h"
#include "gstplayer-video-renderer-private.h"
#include "gstplayer-media-info-private.h"
#include "gstplayer-wrapped-video-renderer-private.h"

#include <gst/gst.h>
#include <gst/play/play.h>
#include <gst/video/video.h>
#include <gst/video/colorbalance.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/descriptions.h>

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_player_debug);
#define GST_CAT_DEFAULT gst_player_debug

#define DEFAULT_URI NULL
#define DEFAULT_POSITION GST_CLOCK_TIME_NONE
#define DEFAULT_DURATION GST_CLOCK_TIME_NONE
#define DEFAULT_VOLUME 1.0
#define DEFAULT_MUTE FALSE
#define DEFAULT_RATE 1.0
#define DEFAULT_POSITION_UPDATE_INTERVAL_MS 100
#define DEFAULT_AUDIO_VIDEO_OFFSET 0
#define DEFAULT_SUBTITLE_VIDEO_OFFSET 0

/**
 * gst_player_error_quark:
 */
GQuark
gst_player_error_quark (void)
{
  return g_quark_from_static_string ("gst-player-error-quark");
}

static GQuark QUARK_CONFIG;

/* Keep ConfigQuarkId and _config_quark_strings ordered and synced */
typedef enum
{
  CONFIG_QUARK_USER_AGENT = 0,
  CONFIG_QUARK_POSITION_INTERVAL_UPDATE,
  CONFIG_QUARK_ACCURATE_SEEK,

  CONFIG_QUARK_MAX
} ConfigQuarkId;

static const gchar *_config_quark_strings[] = {
  "user-agent",
  "position-interval-update",
  "accurate-seek",
};

static GQuark _config_quark_table[CONFIG_QUARK_MAX];

#define CONFIG_QUARK(q) _config_quark_table[CONFIG_QUARK_##q]

enum
{
  PROP_0,
  PROP_VIDEO_RENDERER,
  PROP_SIGNAL_DISPATCHER,
  PROP_URI,
  PROP_SUBURI,
  PROP_POSITION,
  PROP_DURATION,
  PROP_MEDIA_INFO,
  PROP_CURRENT_AUDIO_TRACK,
  PROP_CURRENT_VIDEO_TRACK,
  PROP_CURRENT_SUBTITLE_TRACK,
  PROP_VOLUME,
  PROP_MUTE,
  PROP_RATE,
  PROP_PIPELINE,
  PROP_VIDEO_MULTIVIEW_MODE,
  PROP_VIDEO_MULTIVIEW_FLAGS,
  PROP_AUDIO_VIDEO_OFFSET,
  PROP_SUBTITLE_VIDEO_OFFSET,
  PROP_LAST
};

enum
{
  SIGNAL_URI_LOADED,
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_BUFFERING,
  SIGNAL_END_OF_STREAM,
  SIGNAL_ERROR,
  SIGNAL_WARNING,
  SIGNAL_VIDEO_DIMENSIONS_CHANGED,
  SIGNAL_MEDIA_INFO_UPDATED,
  SIGNAL_VOLUME_CHANGED,
  SIGNAL_MUTE_CHANGED,
  SIGNAL_SEEK_DONE,
  SIGNAL_LAST
};

struct _GstPlayer
{
  GstObject parent;

  GstPlay *play;
  GstPlaySignalAdapter *signal_adapter;

  /* legacy */
  GstPlayerSignalDispatcher *signal_dispatcher;
  GstPlayerVideoRenderer *video_renderer;
};

struct _GstPlayerClass
{
  GstObjectClass parent_class;
};

#define parent_class gst_player_parent_class
G_DEFINE_TYPE (GstPlayer, gst_player, GST_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_player_finalize (GObject * object);
static void gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_player_constructed (GObject * object);

static void
gst_player_init (GstPlayer * self)
{
  self->play = gst_play_new (NULL);
}

static void
config_quark_initialize (void)
{
  gint i;

  QUARK_CONFIG = g_quark_from_static_string ("player-config");

  if (G_N_ELEMENTS (_config_quark_strings) != CONFIG_QUARK_MAX)
    g_warning ("the quark table is not consistent! %d != %d",
        (int) G_N_ELEMENTS (_config_quark_strings), CONFIG_QUARK_MAX);

  for (i = 0; i < CONFIG_QUARK_MAX; i++) {
    _config_quark_table[i] =
        g_quark_from_static_string (_config_quark_strings[i]);
  }
}

static void
gst_player_class_init (GstPlayerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_player_debug, "gst-player", 0, "GstPlayer");
  gst_player_error_quark ();

  gobject_class->set_property = gst_player_set_property;
  gobject_class->get_property = gst_player_get_property;
  gobject_class->finalize = gst_player_finalize;
  gobject_class->constructed = gst_player_constructed;

  param_specs[PROP_VIDEO_RENDERER] =
      g_param_spec_object ("video-renderer",
      "Video Renderer", "Video renderer to use for rendering videos",
      GST_TYPE_PLAYER_VIDEO_RENDERER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SIGNAL_DISPATCHER] =
      g_param_spec_object ("signal-dispatcher",
      "Signal Dispatcher", "Dispatcher for the signals to e.g. event loops",
      GST_TYPE_PLAYER_SIGNAL_DISPATCHER,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_URI] = g_param_spec_string ("uri", "URI", "Current URI",
      DEFAULT_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SUBURI] = g_param_spec_string ("suburi", "Subtitle URI",
      "Current Subtitle URI", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION] =
      g_param_spec_uint64 ("position", "Position", "Current Position",
      0, G_MAXUINT64, DEFAULT_POSITION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MEDIA_INFO] =
      g_param_spec_object ("media-info", "Media Info",
      "Current media information", GST_TYPE_PLAYER_MEDIA_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_AUDIO_TRACK] =
      g_param_spec_object ("current-audio-track", "Current Audio Track",
      "Current audio track information", GST_TYPE_PLAYER_AUDIO_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_VIDEO_TRACK] =
      g_param_spec_object ("current-video-track", "Current Video Track",
      "Current video track information", GST_TYPE_PLAYER_VIDEO_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_SUBTITLE_TRACK] =
      g_param_spec_object ("current-subtitle-track", "Current Subtitle Track",
      "Current audio subtitle information", GST_TYPE_PLAYER_SUBTITLE_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] =
      g_param_spec_uint64 ("duration", "Duration", "Duration",
      0, G_MAXUINT64, DEFAULT_DURATION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VOLUME] =
      g_param_spec_double ("volume", "Volume", "Volume",
      0, 10.0, DEFAULT_VOLUME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MUTE] =
      g_param_spec_boolean ("mute", "Mute", "Mute",
      DEFAULT_MUTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PIPELINE] =
      g_param_spec_object ("pipeline", "Pipeline",
      "GStreamer pipeline that is used",
      GST_TYPE_ELEMENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_RATE] =
      g_param_spec_double ("rate", "rate", "Playback rate",
      -64.0, 64.0, DEFAULT_RATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VIDEO_MULTIVIEW_MODE] =
      g_param_spec_enum ("video-multiview-mode",
      "Multiview Mode Override",
      "Re-interpret a video stream as one of several frame-packed stereoscopic modes.",
      GST_TYPE_VIDEO_MULTIVIEW_FRAME_PACKING,
      GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VIDEO_MULTIVIEW_FLAGS] =
      g_param_spec_flags ("video-multiview-flags",
      "Multiview Flags Override",
      "Override details of the multiview frame layout",
      GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_AUDIO_VIDEO_OFFSET] =
      g_param_spec_int64 ("audio-video-offset", "Audio Video Offset",
      "The synchronisation offset between audio and video in nanoseconds",
      G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SUBTITLE_VIDEO_OFFSET] =
      g_param_spec_int64 ("subtitle-video-offset", "Text Video Offset",
      "The synchronisation offset between text and video in nanoseconds",
      G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  signals[SIGNAL_URI_LOADED] =
      g_signal_new ("uri-loaded", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_STATE_CHANGED] =
      g_signal_new ("state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAYER_STATE);

  signals[SIGNAL_BUFFERING] =
      g_signal_new ("buffering", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_END_OF_STREAM] =
      g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED] =
      g_signal_new ("video-dimensions-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  signals[SIGNAL_MEDIA_INFO_UPDATED] =
      g_signal_new ("media-info-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAYER_MEDIA_INFO);

  signals[SIGNAL_VOLUME_CHANGED] =
      g_signal_new ("volume-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_MUTE_CHANGED] =
      g_signal_new ("mute-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_WARNING] =
      g_signal_new ("warning", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_SEEK_DONE] =
      g_signal_new ("seek-done", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  config_quark_initialize ();
}

static void
gst_player_finalize (GObject * object)
{
  GstPlayer *self = GST_PLAYER (object);

  GST_TRACE_OBJECT (self, "Finalizing");

  if (self->signal_dispatcher)
    g_object_unref (self->signal_dispatcher);
  if (self->video_renderer)
    g_object_unref (self->video_renderer);
  if (self->signal_adapter)
    g_object_unref (self->signal_adapter);
  if (self->play)
    gst_object_unref (self->play);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPlayer *self = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_SIGNAL_DISPATCHER:
      self->signal_dispatcher = g_value_dup_object (value);
      break;
    case PROP_VIDEO_RENDERER:
      self->video_renderer = g_value_dup_object (value);
      break;
    default:
      g_object_set_property (G_OBJECT (self->play),
          g_param_spec_get_name (pspec), value);
      break;
  }
}

static void
gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPlayer *self = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_VIDEO_RENDERER:
      g_value_set_object (value, self->video_renderer);
      break;
    case PROP_MEDIA_INFO:
      g_value_take_object (value, gst_player_get_media_info (self));
      break;
    case PROP_CURRENT_AUDIO_TRACK:
      g_value_take_object (value, gst_player_get_current_audio_track (self));
      break;
    case PROP_CURRENT_VIDEO_TRACK:
      g_value_take_object (value, gst_player_get_current_video_track (self));
      break;
    case PROP_CURRENT_SUBTITLE_TRACK:
      g_value_take_object (value, gst_player_get_current_subtitle_track (self));
      break;
    default:
      g_object_get_property (G_OBJECT (self->play),
          g_param_spec_get_name (pspec), value);
      break;
  }
}

static gpointer
gst_player_init_once (G_GNUC_UNUSED gpointer user_data)
{
  gst_init (NULL, NULL);

  return NULL;
}

static void
uri_loaded_cb (GstPlaySignalAdapter * adapter, const gchar * uri,
    GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_URI_LOADED], 0, uri);
}

static void
position_updated_cb (GstPlaySignalAdapter * adapter, GstClockTime position,
    GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_POSITION_UPDATED], 0, position);
}

static void
duration_changed_cb (GstPlaySignalAdapter * adapter, GstClockTime duraton,
    GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_DURATION_CHANGED], 0, duraton);
}

static void
state_changed_cb (GstPlaySignalAdapter * adapter, GstPlayState state,
    GstPlayer * self)
{
  GstPlayerState s = GST_PLAYER_STATE_BUFFERING;
  switch (state) {
    case GST_PLAY_STATE_BUFFERING:
      s = GST_PLAYER_STATE_BUFFERING;
      break;
    case GST_PLAY_STATE_PAUSED:
      s = GST_PLAYER_STATE_PAUSED;
      break;
    case GST_PLAY_STATE_PLAYING:
      s = GST_PLAYER_STATE_PLAYING;
      break;
    case GST_PLAY_STATE_STOPPED:
      s = GST_PLAYER_STATE_STOPPED;
      break;
  }
  g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, s);
}

static void
buffering_cb (GstPlaySignalAdapter * adapter, gint buffering_percent,
    GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_BUFFERING], 0, buffering_percent);
}

static void
end_of_stream_cb (GstPlaySignalAdapter * adapter, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_END_OF_STREAM], 0, NULL);
}

static void
error_cb (GstPlaySignalAdapter * adapter, GError * error,
    GstStructure * details, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_ERROR], 0, error);
}

static void
dimensions_changed_cb (GstPlaySignalAdapter * adapter, guint width,
    guint height, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0, width,
      height);
}

static void
media_info_cb (GstPlaySignalAdapter * adapter, GstPlayMediaInfo * info,
    GstPlayer * self)
{
  GstPlayerMediaInfo *i = gst_player_media_info_wrapped (info);
  g_signal_emit (self, signals[SIGNAL_MEDIA_INFO_UPDATED], 0, i);
  g_object_unref (i);
}

static void
volume_cb (GstPlaySignalAdapter * adapter, gdouble volume, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_VOLUME_CHANGED], 0, NULL);
}


static void
mute_cb (GstPlaySignalAdapter * adapter, gboolean muted, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_MUTE_CHANGED], 0, NULL);
}

static void
warning_cb (GstPlaySignalAdapter * adapter, GError * warning,
    GstStructure * details, GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_WARNING], 0, warning);
}

static void
seek_done_cb (GstPlaySignalAdapter * adapter, GstClockTime time,
    GstPlayer * self)
{
  g_signal_emit (self, signals[SIGNAL_SEEK_DONE], 0, time);
}

static void
gst_player_constructed (GObject * object)
{
  GstPlayer *self = GST_PLAYER (object);
  GstPlayerVideoRenderer *renderer = NULL;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  if (self->video_renderer != NULL) {
    renderer =
        gst_player_wrapped_video_renderer_new (self->video_renderer, self);
    g_object_set (self->play, "video-renderer",
        GST_PLAY_VIDEO_RENDERER (renderer), NULL);
    g_object_unref (renderer);
  }

  if (self->signal_dispatcher != NULL) {
    GMainContext *context = NULL;

    g_object_get (self->signal_dispatcher, "application-context", &context,
        NULL);
    self->signal_adapter =
        gst_play_signal_adapter_new_with_main_context (self->play, context);
    g_main_context_unref (context);
  } else {
    self->signal_adapter = gst_play_signal_adapter_new_sync_emit (self->play);
  }

  g_signal_connect (self->signal_adapter, "uri-loaded",
      G_CALLBACK (uri_loaded_cb), self);
  g_signal_connect (self->signal_adapter, "position-updated",
      G_CALLBACK (position_updated_cb), self);
  g_signal_connect (self->signal_adapter, "duration-changed",
      G_CALLBACK (duration_changed_cb), self);
  g_signal_connect (self->signal_adapter, "state-changed",
      G_CALLBACK (state_changed_cb), self);
  g_signal_connect (self->signal_adapter, "buffering",
      G_CALLBACK (buffering_cb), self);
  g_signal_connect (self->signal_adapter, "end-of-stream",
      G_CALLBACK (end_of_stream_cb), self);
  g_signal_connect (self->signal_adapter, "error", G_CALLBACK (error_cb), self);
  g_signal_connect (self->signal_adapter, "video-dimensions-changed",
      G_CALLBACK (dimensions_changed_cb), self);
  g_signal_connect (self->signal_adapter, "media-info-updated",
      G_CALLBACK (media_info_cb), self);
  g_signal_connect (self->signal_adapter, "volume-changed",
      G_CALLBACK (volume_cb), self);
  g_signal_connect (self->signal_adapter, "mute-changed", G_CALLBACK (mute_cb),
      self);
  g_signal_connect (self->signal_adapter, "warning", G_CALLBACK (warning_cb),
      self);
  g_signal_connect (self->signal_adapter, "seek-done",
      G_CALLBACK (seek_done_cb), self);
}

/**
 * gst_player_new:
 * @video_renderer: (transfer full) (allow-none): GstPlayerVideoRenderer to use
 * @signal_dispatcher: (transfer full) (allow-none): GstPlayerSignalDispatcher to use
 *
 * Creates a new #GstPlayer instance that uses @signal_dispatcher to dispatch
 * signals to some event loop system, or emits signals directly if NULL is
 * passed. See gst_player_g_main_context_signal_dispatcher_new().
 *
 * Video is going to be rendered by @video_renderer, or if %NULL is provided
 * no special video set up will be done and some default handling will be
 * performed.
 *
 * This also initializes GStreamer via `gst_init()` on the first call if this
 * didn't happen before.
 *
 * Returns: (transfer full): a new #GstPlayer instance
 */
GstPlayer *
gst_player_new (GstPlayerVideoRenderer * video_renderer,
    GstPlayerSignalDispatcher * signal_dispatcher)
{
  static GOnce once = G_ONCE_INIT;
  GstPlayer *self;

  g_once (&once, gst_player_init_once, NULL);

  self =
      g_object_new (GST_TYPE_PLAYER, "signal-dispatcher", signal_dispatcher,
      "video-renderer", video_renderer, NULL);

  gst_object_ref_sink (self);

  if (video_renderer)
    g_object_unref (video_renderer);
  if (signal_dispatcher)
    g_object_unref (signal_dispatcher);

  return self;
}

/**
 * gst_player_play:
 * @player: #GstPlayer instance
 *
 * Request to play the loaded stream.
 */
void
gst_player_play (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_play (self->play);
}

/**
 * gst_player_pause:
 * @player: #GstPlayer instance
 *
 * Pauses the current stream.
 */
void
gst_player_pause (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_pause (self->play);
}

/**
 * gst_player_stop:
 * @player: #GstPlayer instance
 *
 * Stops playing the current stream and resets to the first position
 * in the stream.
 */
void
gst_player_stop (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_stop (self->play);
}

/**
 * gst_player_set_rate:
 * @player: #GstPlayer instance
 * @rate: playback rate
 *
 * Playback at specified rate
 */
void
gst_player_set_rate (GstPlayer * self, gdouble rate)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  g_return_if_fail (rate != 0.0);

  g_object_set (self, "rate", rate, NULL);
}

/**
 * gst_player_get_rate:
 * @player: #GstPlayer instance
 *
 * Returns: current playback rate
 */
gdouble
gst_player_get_rate (GstPlayer * self)
{
  gdouble val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_RATE);

  g_object_get (self, "rate", &val, NULL);

  return val;
}

/**
 * gst_player_seek:
 * @player: #GstPlayer instance
 * @position: position to seek in nanoseconds
 *
 * Seeks the currently-playing stream to the absolute @position time
 * in nanoseconds.
 */
void
gst_player_seek (GstPlayer * self, GstClockTime position)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (position));

  gst_play_seek (self->play, position);
}

/**
 * gst_player_get_uri:
 * @player: #GstPlayer instance
 *
 * Gets the URI of the currently-playing stream.
 *
 * Returns: (transfer full) (nullable): a string containing the URI of the
 * currently-playing stream. g_free() after usage.
 */
gchar *
gst_player_get_uri (GstPlayer * self)
{
  gchar *val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_URI);

  g_object_get (self, "uri", &val, NULL);

  return val;
}

/**
 * gst_player_set_uri:
 * @player: #GstPlayer instance
 * @uri: (nullable): next URI to play.
 *
 * Sets the next URI to play.
 */
void
gst_player_set_uri (GstPlayer * self, const gchar * val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "uri", val, NULL);
}

/**
 * gst_player_set_subtitle_uri:
 * @player: #GstPlayer instance
 * @uri: (nullable): subtitle URI
 *
 * Sets the external subtitle URI. This should be combined with a call to
 * gst_player_set_subtitle_track_enabled(@player, TRUE) so the subtitles are actually
 * rendered.
 */
void
gst_player_set_subtitle_uri (GstPlayer * self, const gchar * suburi)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "suburi", suburi, NULL);
}

/**
 * gst_player_get_subtitle_uri:
 * @player: #GstPlayer instance
 *
 * current subtitle URI
 *
 * Returns: (transfer full) (nullable): URI of the current external subtitle.
 *   g_free() after usage.
 */
gchar *
gst_player_get_subtitle_uri (GstPlayer * self)
{
  gchar *val = NULL;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  g_object_get (self, "suburi", &val, NULL);

  return val;
}

/**
 * gst_player_get_position:
 * @player: #GstPlayer instance
 *
 * Returns: the absolute position time, in nanoseconds, of the
 * currently-playing stream.
 */
GstClockTime
gst_player_get_position (GstPlayer * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_POSITION);

  g_object_get (self, "position", &val, NULL);

  return val;
}

/**
 * gst_player_get_duration:
 * @player: #GstPlayer instance
 *
 * Retrieves the duration of the media stream that self represents.
 *
 * Returns: the duration of the currently-playing media stream, in
 * nanoseconds.
 */
GstClockTime
gst_player_get_duration (GstPlayer * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_DURATION);

  g_object_get (self, "duration", &val, NULL);

  return val;
}

/**
 * gst_player_get_volume:
 * @player: #GstPlayer instance
 *
 * Returns the current volume level, as a percentage between 0 and 1.
 *
 * Returns: the volume as percentage between 0 and 1.
 */
gdouble
gst_player_get_volume (GstPlayer * self)
{
  gdouble val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_VOLUME);

  g_object_get (self, "volume", &val, NULL);

  return val;
}

/**
 * gst_player_set_volume:
 * @player: #GstPlayer instance
 * @val: the new volume level, as a percentage between 0 and 1
 *
 * Sets the volume level of the stream as a percentage between 0 and 1.
 *
 * This volume is a linear factor. For showing the volume in a GUI it
 * might make sense to first convert from a different format. Volume sliders
 * should usually use a cubic volume. See gst_stream_volume_convert_volume().
 */
void
gst_player_set_volume (GstPlayer * self, gdouble val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "volume", val, NULL);
}

/**
 * gst_player_get_mute:
 * @player: #GstPlayer instance
 *
 * Returns: %TRUE if the currently-playing stream is muted.
 */
gboolean
gst_player_get_mute (GstPlayer * self)
{
  gboolean val;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_MUTE);

  g_object_get (self, "mute", &val, NULL);

  return val;
}

/**
 * gst_player_set_mute:
 * @player: #GstPlayer instance
 * @val: Mute state the should be set
 *
 * %TRUE if the currently-playing stream should be muted.
 */
void
gst_player_set_mute (GstPlayer * self, gboolean val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "mute", val, NULL);
}

/**
 * gst_player_get_pipeline:
 * @player: #GstPlayer instance
 *
 * Returns: (transfer full): The internal playbin instance.
 *
 * The caller should free it with g_object_unref()
 */
GstElement *
gst_player_get_pipeline (GstPlayer * self)
{
  GstElement *val;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  g_object_get (self, "pipeline", &val, NULL);

  return val;
}

/**
 * gst_player_get_media_info:
 * @player: #GstPlayer instance
 *
 * A Function to get the current media info #GstPlayerMediaInfo instance.
 *
 * Returns: (transfer full) (nullable): media info instance.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerMediaInfo *
gst_player_get_media_info (GstPlayer * self)
{
  GstPlayMediaInfo *info;
  GstPlayerMediaInfo *ret;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  info = gst_play_get_media_info (self->play);
  if (!info)
    return NULL;

  ret = gst_player_media_info_wrapped (info);
  g_object_unref (info);
  return ret;
}

/**
 * gst_player_get_current_audio_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current audio #GstPlayerAudioInfo instance.
 *
 * Returns: (transfer full) (nullable): current audio track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerAudioInfo *
gst_player_get_current_audio_track (GstPlayer * self)
{
  GstPlayAudioInfo *info;
  GstPlayerAudioInfo *ret = NULL;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  info = gst_play_get_current_audio_track (self->play);
  if (info != NULL) {
    ret = gst_player_audio_info_wrapped (info);
    g_object_unref (info);
  }
  return ret;
}

/**
 * gst_player_get_current_video_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current video #GstPlayerVideoInfo instance.
 *
 * Returns: (transfer full) (nullable): current video track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerVideoInfo *
gst_player_get_current_video_track (GstPlayer * self)
{
  GstPlayVideoInfo *info;
  GstPlayerVideoInfo *ret = NULL;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  info = gst_play_get_current_video_track (self->play);
  if (info != NULL) {
    ret = gst_player_video_info_wrapped (info);
    g_object_unref (info);
  }
  return ret;
}

/**
 * gst_player_get_current_subtitle_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current subtitle #GstPlayerSubtitleInfo instance.
 *
 * Returns: (transfer full) (nullable): current subtitle track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerSubtitleInfo *
gst_player_get_current_subtitle_track (GstPlayer * self)
{
  GstPlaySubtitleInfo *info;
  GstPlayerSubtitleInfo *ret = NULL;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  info = gst_play_get_current_subtitle_track (self->play);
  if (info != NULL) {
    ret = gst_player_subtitle_info_wrapped (info);
    g_object_unref (info);
  }
  return ret;
}

/**
 * gst_player_set_audio_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the audio track @stream_idex.
 */
gboolean
gst_player_set_audio_track (GstPlayer * self, gint stream_index)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  return gst_play_set_audio_track (self->play, stream_index);
}

/**
 * gst_player_set_video_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the video track @stream_index.
 */
gboolean
gst_player_set_video_track (GstPlayer * self, gint stream_index)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  return gst_play_set_video_track (self->play, stream_index);
}

/**
 * gst_player_set_subtitle_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the subtitle stack @stream_index.
 */
gboolean
gst_player_set_subtitle_track (GstPlayer * self, gint stream_index)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  return gst_play_set_subtitle_track (self->play, stream_index);
}

/**
 * gst_player_set_audio_track_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current audio track.
 */
void
gst_player_set_audio_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_set_audio_track_enabled (self->play, enabled);
}

/**
 * gst_player_set_video_track_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current video track.
 */
void
gst_player_set_video_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_set_video_track_enabled (self->play, enabled);
}

/**
 * gst_player_set_subtitle_track_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current subtitle track.
 */
void
gst_player_set_subtitle_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_set_subtitle_track_enabled (self->play, enabled);
}

/**
 * gst_player_set_visualization:
 * @player: #GstPlayer instance
 * @name: (nullable): visualization element obtained from
 * #gst_player_visualizations_get()
 *
 * Returns: %TRUE if the visualizations was set correctly. Otherwise,
 * %FALSE.
 */
gboolean
gst_player_set_visualization (GstPlayer * self, const gchar * name)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), FALSE);

  return gst_play_set_visualization (self->play, name);
}

/**
 * gst_player_get_current_visualization:
 * @player: #GstPlayer instance
 *
 * Returns: (transfer full) (nullable): Name of the currently enabled
 *   visualization.
 *   g_free() after usage.
 */
gchar *
gst_player_get_current_visualization (GstPlayer * self)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  return gst_play_get_current_visualization (self->play);
}

/**
 * gst_player_set_visualization_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the visualization.
 */
void
gst_player_set_visualization_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  gst_play_set_visualization_enabled (self->play, enabled);
}

struct CBChannelMap
{
  const gchar *label;           /* channel label name */
  const gchar *name;            /* get_name () */
};

static const struct CBChannelMap cb_channel_map[] = {
  /* GST_PLAYER_COLOR_BALANCE_BRIGHTNESS */ {"BRIGHTNESS", "brightness"},
  /* GST_PLAYER_COLOR_BALANCE_CONTRAST   */ {"CONTRAST", "contrast"},
  /* GST_PLAYER_COLOR_BALANCE_SATURATION */ {"SATURATION", "saturation"},
  /* GST_PLAYER_COLOR_BALANCE_HUE        */ {"HUE", "hue"},
};

/**
 * gst_player_has_color_balance:
 * @player:#GstPlayer instance
 *
 * Checks whether the @player has color balance support available.
 *
 * Returns: %TRUE if @player has color balance support. Otherwise,
 *   %FALSE.
 */
gboolean
gst_player_has_color_balance (GstPlayer * self)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), FALSE);

  return gst_play_has_color_balance (self->play);
}

/**
 * gst_player_set_color_balance:
 * @player: #GstPlayer instance
 * @type: #GstPlayerColorBalanceType
 * @value: The new value for the @type, ranged [0,1]
 *
 * Sets the current value of the indicated channel @type to the passed
 * value.
 */
void
gst_player_set_color_balance (GstPlayer * self, GstPlayerColorBalanceType type,
    gdouble value)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  g_return_if_fail (value >= 0.0 && value <= 1.0);

  gst_play_set_color_balance (self->play, (GstPlayColorBalanceType) type,
      value);
}

/**
 * gst_player_get_color_balance:
 * @player: #GstPlayer instance
 * @type: #GstPlayerColorBalanceType
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, between [0,1]. In case of
 *   error -1 is returned.
 */
gdouble
gst_player_get_color_balance (GstPlayer * self, GstPlayerColorBalanceType type)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), -1);

  return gst_play_get_color_balance (self->play,
      (GstPlayColorBalanceType) type);
}

/**
 * gst_player_get_multiview_mode:
 * @player: #GstPlayer instance
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, Default: -1 "none"
 *
 * Since: 1.10
 */
GstVideoMultiviewFramePacking
gst_player_get_multiview_mode (GstPlayer * self)
{
  GstVideoMultiviewFramePacking val = GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE;

  g_return_val_if_fail (GST_IS_PLAYER (self),
      GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE);

  g_object_get (self, "video-multiview-mode", &val, NULL);

  return val;
}

/**
 * gst_player_set_multiview_mode:
 * @player: #GstPlayer instance
 * @mode: The new value for the @type
 *
 * Sets the current value of the indicated mode @type to the passed
 * value.
 *
 * Since: 1.10
 */
void
gst_player_set_multiview_mode (GstPlayer * self,
    GstVideoMultiviewFramePacking mode)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "video-multiview-mode", mode, NULL);
}

/**
 * gst_player_get_multiview_flags:
 * @player: #GstPlayer instance
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, Default: 0x00000000 "none
 *
 * Since: 1.10
 */
GstVideoMultiviewFlags
gst_player_get_multiview_flags (GstPlayer * self)
{
  GstVideoMultiviewFlags val = GST_VIDEO_MULTIVIEW_FLAGS_NONE;

  g_return_val_if_fail (GST_IS_PLAYER (self), val);

  g_object_get (self, "video-multiview-flags", &val, NULL);

  return val;
}

/**
 * gst_player_set_multiview_flags:
 * @player: #GstPlayer instance
 * @flags: The new value for the @type
 *
 * Sets the current value of the indicated mode @type to the passed
 * value.
 *
 * Since: 1.10
 */
void
gst_player_set_multiview_flags (GstPlayer * self, GstVideoMultiviewFlags flags)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "video-multiview-flags", flags, NULL);
}

/**
 * gst_player_get_audio_video_offset:
 * @player: #GstPlayer instance
 *
 * Retrieve the current value of audio-video-offset property
 *
 * Returns: The current value of audio-video-offset in nanoseconds
 *
 * Since: 1.10
 */
gint64
gst_player_get_audio_video_offset (GstPlayer * self)
{
  gint64 val = 0;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_AUDIO_VIDEO_OFFSET);

  g_object_get (self, "audio-video-offset", &val, NULL);

  return val;
}

/**
 * gst_player_set_audio_video_offset:
 * @player: #GstPlayer instance
 * @offset: #gint64 in nanoseconds
 *
 * Sets audio-video-offset property by value of @offset
 *
 * Since: 1.10
 */
void
gst_player_set_audio_video_offset (GstPlayer * self, gint64 offset)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "audio-video-offset", offset, NULL);
}

/**
 * gst_player_get_subtitle_video_offset:
 * @player: #GstPlayer instance
 *
 * Retrieve the current value of subtitle-video-offset property
 *
 * Returns: The current value of subtitle-video-offset in nanoseconds
 *
 * Since: 1.16
 */
gint64
gst_player_get_subtitle_video_offset (GstPlayer * self)
{
  gint64 val = 0;

  g_return_val_if_fail (GST_IS_PLAYER (self), DEFAULT_SUBTITLE_VIDEO_OFFSET);

  g_object_get (self, "subtitle-video-offset", &val, NULL);

  return val;
}

/**
 * gst_player_set_subtitle_video_offset:
 * @player: #GstPlayer instance
 * @offset: #gint64 in nanoseconds
 *
 * Sets subtitle-video-offset property by value of @offset
 *
 * Since: 1.16
 */
void
gst_player_set_subtitle_video_offset (GstPlayer * self, gint64 offset)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "subtitle-video-offset", offset, NULL);
}


#define C_ENUM(v) ((gint) v)
#define C_FLAGS(v) ((guint) v)

GType
gst_player_color_balance_type_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_PLAYER_COLOR_BALANCE_HUE), "GST_PLAYER_COLOR_BALANCE_HUE",
        "hue"},
    {C_ENUM (GST_PLAYER_COLOR_BALANCE_BRIGHTNESS),
        "GST_PLAYER_COLOR_BALANCE_BRIGHTNESS", "brightness"},
    {C_ENUM (GST_PLAYER_COLOR_BALANCE_SATURATION),
        "GST_PLAYER_COLOR_BALANCE_SATURATION", "saturation"},
    {C_ENUM (GST_PLAYER_COLOR_BALANCE_CONTRAST),
        "GST_PLAYER_COLOR_BALANCE_CONTRAST", "contrast"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstPlayerColorBalanceType", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_player_color_balance_type_get_name:
 * @type: a #GstPlayerColorBalanceType
 *
 * Gets a string representing the given color balance type.
 *
 * Returns: (transfer none): a string with the name of the color
 *   balance type.
 */
const gchar *
gst_player_color_balance_type_get_name (GstPlayerColorBalanceType type)
{
  g_return_val_if_fail (type >= GST_PLAYER_COLOR_BALANCE_BRIGHTNESS &&
      type <= GST_PLAYER_COLOR_BALANCE_HUE, NULL);

  return cb_channel_map[type].name;
}

GType
gst_player_state_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_PLAYER_STATE_STOPPED), "GST_PLAYER_STATE_STOPPED", "stopped"},
    {C_ENUM (GST_PLAYER_STATE_BUFFERING), "GST_PLAYER_STATE_BUFFERING",
        "buffering"},
    {C_ENUM (GST_PLAYER_STATE_PAUSED), "GST_PLAYER_STATE_PAUSED", "paused"},
    {C_ENUM (GST_PLAYER_STATE_PLAYING), "GST_PLAYER_STATE_PLAYING", "playing"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstPlayerState", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_player_state_get_name:
 * @state: a #GstPlayerState
 *
 * Gets a string representing the given state.
 *
 * Returns: (transfer none): a string with the name of the state.
 */
const gchar *
gst_player_state_get_name (GstPlayerState state)
{
  switch (state) {
    case GST_PLAYER_STATE_STOPPED:
      return "stopped";
    case GST_PLAYER_STATE_BUFFERING:
      return "buffering";
    case GST_PLAYER_STATE_PAUSED:
      return "paused";
    case GST_PLAYER_STATE_PLAYING:
      return "playing";
  }

  g_assert_not_reached ();
  return NULL;
}

GType
gst_player_error_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_PLAYER_ERROR_FAILED), "GST_PLAYER_ERROR_FAILED", "failed"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstPlayerError", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_player_error_get_name:
 * @error: a #GstPlayerError
 *
 * Gets a string representing the given error.
 *
 * Returns: (transfer none): a string with the given error.
 */
const gchar *
gst_player_error_get_name (GstPlayerError error)
{
  switch (error) {
    case GST_PLAYER_ERROR_FAILED:
      return "failed";
  }

  g_assert_not_reached ();
  return NULL;
}

/**
 * gst_player_set_config:
 * @player: #GstPlayer instance
 * @config: (transfer full): a #GstStructure
 *
 * Set the configuration of the player. If the player is already configured, and
 * the configuration haven't change, this function will return %TRUE. If the
 * player is not in the GST_PLAYER_STATE_STOPPED, this method will return %FALSE
 * and active configuration will remain.
 *
 * @config is a #GstStructure that contains the configuration parameters for
 * the player.
 *
 * This function takes ownership of @config.
 *
 * Returns: %TRUE when the configuration could be set.
 * Since: 1.10
 */
gboolean
gst_player_set_config (GstPlayer * self, GstStructure * config)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (config != NULL, FALSE);

  return gst_play_set_config (self->play, config);
}

/**
 * gst_player_get_config:
 * @player: #GstPlayer instance
 *
 * Get a copy of the current configuration of the player. This configuration
 * can either be modified and used for the gst_player_set_config() call
 * or it must be freed after usage.
 *
 * Returns: (transfer full): a copy of the current configuration of @player. Use
 * gst_structure_free() after usage or gst_player_set_config().
 *
 * Since: 1.10
 */
GstStructure *
gst_player_get_config (GstPlayer * self)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  return gst_play_get_config (self->play);
}

/**
 * gst_player_config_set_user_agent:
 * @config: a #GstPlayer configuration
 * @agent: (nullable): the string to use as user agent
 *
 * Set the user agent to pass to the server if @player needs to connect
 * to a server during playback. This is typically used when playing HTTP
 * or RTSP streams.
 *
 * Since: 1.10
 */
void
gst_player_config_set_user_agent (GstStructure * config, const gchar * agent)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (agent != NULL);

  gst_structure_id_set (config,
      CONFIG_QUARK (USER_AGENT), G_TYPE_STRING, agent, NULL);
}

/**
 * gst_player_config_get_user_agent:
 * @config: a #GstPlayer configuration
 *
 * Return the user agent which has been configured using
 * gst_player_config_set_user_agent() if any.
 *
 * Returns: (transfer full) (nullable): the configured agent, or %NULL
 *
 * Since: 1.10
 */
gchar *
gst_player_config_get_user_agent (const GstStructure * config)
{
  gchar *agent = NULL;

  g_return_val_if_fail (config != NULL, NULL);

  gst_structure_id_get (config,
      CONFIG_QUARK (USER_AGENT), G_TYPE_STRING, &agent, NULL);

  return agent;
}

/**
 * gst_player_config_set_position_update_interval:
 * @config: a #GstPlayer configuration
 * @interval: interval in ms
 *
 * set interval in milliseconds between two position-updated signals.
 * pass 0 to stop updating the position.
 *
 * Since: 1.10
 */
void
gst_player_config_set_position_update_interval (GstStructure * config,
    guint interval)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (interval <= 10000);

  gst_structure_id_set (config,
      CONFIG_QUARK (POSITION_INTERVAL_UPDATE), G_TYPE_UINT, interval, NULL);
}

/**
 * gst_player_config_get_position_update_interval:
 * @config: a #GstPlayer configuration
 *
 * Returns: current position update interval in milliseconds
 *
 * Since: 1.10
 */
guint
gst_player_config_get_position_update_interval (const GstStructure * config)
{
  guint interval = DEFAULT_POSITION_UPDATE_INTERVAL_MS;

  g_return_val_if_fail (config != NULL, DEFAULT_POSITION_UPDATE_INTERVAL_MS);

  gst_structure_id_get (config,
      CONFIG_QUARK (POSITION_INTERVAL_UPDATE), G_TYPE_UINT, &interval, NULL);

  return interval;
}

/**
 * gst_player_config_set_seek_accurate:
 * @config: a #GstPlayer configuration
 * @accurate: accurate seek or not
 *
 * Enable or disable accurate seeking. When enabled, elements will try harder
 * to seek as accurately as possible to the requested seek position. Generally
 * it will be slower especially for formats that don't have any indexes or
 * timestamp markers in the stream.
 *
 * If accurate seeking is disabled, elements will seek as close as the request
 * position without slowing down seeking too much.
 *
 * Accurate seeking is disabled by default.
 *
 * Since: 1.12
 */
void
gst_player_config_set_seek_accurate (GstStructure * config, gboolean accurate)
{
  g_return_if_fail (config != NULL);

  gst_structure_id_set (config,
      CONFIG_QUARK (ACCURATE_SEEK), G_TYPE_BOOLEAN, accurate, NULL);
}

/**
 * gst_player_config_get_seek_accurate:
 * @config: a #GstPlayer configuration
 *
 * Returns: %TRUE if accurate seeking is enabled
 *
 * Since: 1.12
 */
gboolean
gst_player_config_get_seek_accurate (const GstStructure * config)
{
  gboolean accurate = FALSE;

  g_return_val_if_fail (config != NULL, FALSE);

  gst_structure_id_get (config,
      CONFIG_QUARK (ACCURATE_SEEK), G_TYPE_BOOLEAN, &accurate, NULL);

  return accurate;
}

/**
 * gst_player_get_video_snapshot:
 * @player: #GstPlayer instance
 * @format: output format of the video snapshot
 * @config: (allow-none): Additional configuration
 *
 * Get a snapshot of the currently selected video stream, if any. The format can be
 * selected with @format and optional configuration is possible with @config
 * Currently supported settings are:
 * - width, height of type G_TYPE_INT
 * - pixel-aspect-ratio of type GST_TYPE_FRACTION
 *  Except for GST_PLAYER_THUMBNAIL_RAW_NATIVE format, if no config is set, pixel-aspect-ratio would be 1/1
 *
 * Returns: (transfer full) (nullable):  Current video snapshot sample or %NULL on failure
 *
 * Since: 1.12
 */
GstSample *
gst_player_get_video_snapshot (GstPlayer * self,
    GstPlayerSnapshotFormat format, const GstStructure * config)
{
  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  return gst_play_get_video_snapshot (self->play,
      (GstPlaySnapshotFormat) format, config);
}
