/* GStreamer
  * Copyright (C) 2019 Stéphane Cerveau <scerveau@collabora.com>
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
 * SECTION:element-dashsink
 * @title: dashsink
 *
 * Dynamic Adaptive Streaming over HTTP sink/server
 *
 * ## Example launch line
 * |[
  * gst-launch-1.0 dashsink name=dashsink audiotestsrc is-live=true ! avenc_aac ! dashsink.audio_0 videotestsrc is-live=true ! x264enc ! dashsink.video_0
 * ]|
 *
 */

/* Implementation notes:
 *
 * The following section describes how dashsink works internally.
 *
 * Introduction:
 *
 * This element aims to generate the Media Pressentation Description XML based file
 * used as DASH content in addition to the necessary media fragments.
 * Based on splitmuxsink branches to generate the media fragments,
 * the element will generate a new adaptation set for each media type (video/audio/test)
 * and a new representation for each additional stream for a media type.
 *                                    ,----------------dashsink------------------,
 *                                    ;  ,----------splitmuxsink--------------,  ;
 *    ,-videotestsrc-,  ,-x264enc-,   ;  ; ,-Queue-, ,-mpegtsmux-, ,-filesink-, ;  ;
 *    ;              o--o         o---o--o ;       o-o         o-o          , ;  ;
 *    '--------------'  '---------'   ;  ; '-------' '---------' '----------' ;  ;
 *                                    ;  '------------------------------------'  ;
 *                                    ;                                          ;
 *                                    ;  ,----------splitmuxsink--------------,  ;
 *    ,-audiotestsrc-,  ,-avenc_aac-, ;  ; ,-Queue-, ,-mpegtsmux-, ,-filesink-, ;  ;
 *    ;              o--o           o-o--o         o-o         o-o          ; ;  ;
 *    '--------------'  '-----------' ;  ; '-------' '---------' '----------' ;  ;
 *                                    ;  '------------------------------------'  ;
 *                                    ' -----------------------------------------'
 * * "DASH Sink"
 * |_ Period 1
 * |   |_ Video Adaptation Set
 * |   |   |_ Representation 1 - Container/Codec - bitrate X
 * |       |_ Representation 2 - Container/Codec - bitrate Y
 * |   |_ Audio Adaptation Set
 * |       |_ Representation 1 - Container/Codec - bitrate X
 * |       |_ Representation 2 - Container/Codec - bitrate Y
 *
 * This element is able to generate static or dynamic MPD with multiple adaptation sets,
 * multiple representations and multiple periods for three kind of
 * media streams (Video/Audio/Text).
 *
 * It supports any kind of stream input codec
 * which can be encapsulated in Transport Stream (MPEG-TS) or ISO media format (MP4).
 * The current implementation is generating compliant MPDs for both static and dynamic
 * profiles with  https://conformance.dashif.org/
 *
 * Limitations:
 *
 * The fragments during the DASH generation does not look reliable enough to be used as
 * a production solution. Some additional or fine tuning work needs to be performed to address
 * these issues, especially for MP4 fragments.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdashsink.h"
#include "gstmpdparser.h"
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <memory.h>


GST_DEBUG_CATEGORY_STATIC (gst_dash_sink_debug);
#define GST_CAT_DEFAULT gst_dash_sink_debug

/**
 * GstDashSinkMuxerType:
 * @GST_DASH_SINK_MUXER_TS: Use mpegtsmux
 * @GST_DASH_SINK_MUXER_MP4: Use mp4mux
 * @GST_DASH_SINK_MUXER_DASHMP4: Use dashmp4mux
 *
 * Muxer type
 */
typedef enum
{
  GST_DASH_SINK_MUXER_TS = 0,
  GST_DASH_SINK_MUXER_MP4 = 1,
  GST_DASH_SINK_MUXER_DASHMP4 = 2,
} GstDashSinkMuxerType;

typedef struct _DashSinkMuxer
{
  GstDashSinkMuxerType type;
  const gchar *element_name;
  const gchar *mimetype;
  const gchar *file_ext;
} DashSinkMuxer;

#define GST_TYPE_DASH_SINK_MUXER (gst_dash_sink_muxer_get_type())
static GType
gst_dash_sink_muxer_get_type (void)
{
  static GType dash_sink_muxer_type = 0;
  static const GEnumValue muxer_type[] = {
    {GST_DASH_SINK_MUXER_TS, "Use mpegtsmux", "ts"},
    {GST_DASH_SINK_MUXER_MP4, "Use mp4mux (deprecated, non-functional)", "mp4"},
    /**
     * GstDashSinkMuxerType::dashmp4
     *
     *
     * Since: 1.24
     */
    {GST_DASH_SINK_MUXER_DASHMP4, "Use dashmp4mux", "dashmp4"},
    {0, NULL, NULL},
  };

  if (!dash_sink_muxer_type) {
    dash_sink_muxer_type =
        g_enum_register_static ("GstDashSinkMuxerType", muxer_type);
  }
  return dash_sink_muxer_type;
}

static const DashSinkMuxer dash_muxer_list[] = {
  {
        GST_DASH_SINK_MUXER_TS,
        "mpegtsmux",
        "video/mp2t",
      "ts"},
  {
        GST_DASH_SINK_MUXER_MP4,
        "mp4mux",
        "video/mp4",
      "mp4"},
  {
        GST_DASH_SINK_MUXER_DASHMP4,
        "dashmp4mux",
        "video/mp4",
      "mp4"},
};

#define DEFAULT_SEGMENT_LIST_TPL "_%05d"
#define DEFAULT_SEGMENT_TEMPLATE_TPL "_%d"
#define DEFAULT_MPD_FILENAME "dash.mpd"
#define DEFAULT_MPD_ROOT_PATH NULL
#define DEFAULT_TARGET_DURATION 15
#define DEFAULT_SEND_KEYFRAME_REQUESTS TRUE
#define DEFAULT_MPD_NAMESPACE "urn:mpeg:dash:schema:mpd:2011"
#define DEFAULT_MPD_PROFILES "urn:mpeg:dash:profile:isoff-main:2011"
#define DEFAULT_MPD_SCHEMA_LOCATION "urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd"
#define DEFAULT_MPD_USE_SEGMENT_LIST FALSE
#define DEFAULT_MPD_MIN_BUFFER_TIME 2000
#define DEFAULT_MPD_PERIOD_DURATION GST_CLOCK_TIME_NONE
#define DEFAULT_MPD_SUGGESTED_PRESENTATION_DELAY 0

#define DEFAULT_DASH_SINK_MUXER GST_DASH_SINK_MUXER_TS

enum
{
  ADAPTATION_SET_ID_VIDEO = 1,
  ADAPTATION_SET_ID_AUDIO,
  ADAPTATION_SET_ID_SUBTITLE,
};

enum
{
  PROP_0,
  PROP_MPD_FILENAME,
  PROP_MPD_ROOT_PATH,
  PROP_TARGET_DURATION,
  PROP_SEND_KEYFRAME_REQUESTS,
  PROP_USE_SEGMENT_LIST,
  PROP_MPD_DYNAMIC,
  PROP_MUXER,
  PROP_MPD_MINIMUM_UPDATE_PERIOD,
  PROP_MPD_MIN_BUFFER_TIME,
  PROP_MPD_BASEURL,
  PROP_MPD_PERIOD_DURATION,
  PROP_MPD_SUGGESTED_PRESENTATION_DELAY,
};

enum
{
  SIGNAL_GET_PLAYLIST_STREAM,
  SIGNAL_GET_FRAGMENT_STREAM,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

typedef enum
{
  DASH_SINK_STREAM_TYPE_VIDEO = 0,
  DASH_SINK_STREAM_TYPE_AUDIO,
  DASH_SINK_STREAM_TYPE_SUBTITLE,
  DASH_SINK_STREAM_TYPE_UNKNOWN,
} GstDashSinkStreamType;

typedef struct _GstDashSinkStreamVideoInfo
{
  gint width;
  gint height;
} GstDashSinkStreamVideoInfo;

typedef struct _GstDashSinkStreamAudioInfo
{
  gint channels;
  gint rate;
} GstDashSinkStreamAudioInfo;

typedef struct GstDashSinkStreamSubtitleInfo
{
  gchar *codec;
} GstDashSinkStreamSubtitleInfo;

typedef union _GstDashSinkStreamInfo
{
  GstDashSinkStreamVideoInfo video;
  GstDashSinkStreamAudioInfo audio;
  GstDashSinkStreamSubtitleInfo subtitle;
} GstDashSinkStreamInfo;

struct _GstDashSink
{
  GstBin bin;
  GMutex mpd_lock;
  gchar *location;
  gchar *mpd_filename;
  gchar *mpd_root_path;
  gchar *mpd_profiles;
  gchar *mpd_baseurl;
  GstDashSinkMuxerType muxer;
  GstMPDClient *mpd_client;
  gchar *current_period_id;
  gint target_duration;
  GstClockTime running_time;
  gboolean send_keyframe_requests;
  gboolean use_segment_list;
  gboolean is_dynamic;
  gchar *segment_file_tpl;
  guint index;
  GList *streams;
  guint64 minimum_update_period;
  guint64 suggested_presentation_delay;
  guint64 min_buffer_time;
  gint64 period_duration;
};

typedef struct _GstDashSinkStream
{
  GstDashSink *sink;
  GstDashSinkStreamType type;
  GstPad *pad;
  gint buffer_probe;
  GstElement *splitmuxsink;
  gint adaptation_set_id;
  gchar *representation_id;
  gchar *current_segment_location;
  gint current_segment_id;
  gint next_segment_id;
  gchar *mimetype;
  gint bitrate;
  gchar *codec;
  GstClockTime current_running_time_start;
  GstDashSinkStreamInfo info;
  GstElement *giostreamsink;
} GstDashSinkStream;

static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);


static GstStaticPadTemplate audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate subtitle_sink_template =
GST_STATIC_PAD_TEMPLATE ("subtitle_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

#define gst_dash_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDashSink, gst_dash_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_dash_sink_debug, "dashsink", 0, "DashSink"));
GST_ELEMENT_REGISTER_DEFINE (dashsink, "dashsink", GST_RANK_NONE,
    gst_dash_sink_get_type ());

static void gst_dash_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_dash_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec);
static void gst_dash_sink_handle_message (GstBin * bin, GstMessage * message);
static void gst_dash_sink_reset (GstDashSink * sink);
static GstStateChangeReturn
gst_dash_sink_change_state (GstElement * element, GstStateChange trans);
static GstPad *gst_dash_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_dash_sink_release_pad (GstElement * element, GstPad * pad);


static GstDashSinkStream *
gst_dash_sink_stream_from_pad (GList * streams, GstPad * pad)
{
  GList *l;
  GstDashSinkStream *stream = NULL;
  for (l = streams; l != NULL; l = l->next) {
    stream = l->data;
    if (stream->pad == pad)
      return stream;
  }
  return NULL;
}

static GstDashSinkStream *
gst_dash_sink_stream_from_splitmuxsink (GList * streams, GstElement * element)
{
  GList *l;
  GstDashSinkStream *stream = NULL;
  for (l = streams; l != NULL; l = l->next) {
    stream = l->data;
    if (stream->splitmuxsink == element)
      return stream;
  }
  return NULL;
}

static gchar *
gst_dash_sink_stream_get_next_name (GList * streams, GstDashSinkStreamType type)
{
  GList *l;
  guint count = 0;
  GstDashSinkStream *stream = NULL;
  gchar *name = NULL;

  for (l = streams; l != NULL; l = l->next) {
    stream = l->data;
    if (stream->type == type)
      count++;
  }

  switch (type) {
    case DASH_SINK_STREAM_TYPE_VIDEO:
      name = g_strdup_printf ("video_%d", count);
      break;
    case DASH_SINK_STREAM_TYPE_AUDIO:
      name = g_strdup_printf ("audio_%d", count);
      break;
    case DASH_SINK_STREAM_TYPE_SUBTITLE:
      name = g_strdup_printf ("sub_%d", count);
      break;
    default:
      name = g_strdup_printf ("unknown_%d", count);
  }

  return name;
}

static void
gst_dash_sink_stream_free (gpointer s)
{
  GstDashSinkStream *stream = (GstDashSinkStream *) s;
  g_object_unref (stream->sink);
  g_free (stream->current_segment_location);
  g_free (stream->representation_id);
  g_free (stream->mimetype);
  g_free (stream->codec);

  g_free (stream);
}

static void
gst_dash_sink_dispose (GObject * object)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  G_OBJECT_CLASS (parent_class)->dispose ((GObject *) sink);
}

static void
gst_dash_sink_finalize (GObject * object)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  g_free (sink->mpd_filename);
  g_free (sink->mpd_root_path);
  g_free (sink->mpd_profiles);
  if (sink->mpd_client)
    gst_mpd_client_free (sink->mpd_client);
  g_mutex_clear (&sink->mpd_lock);

  g_list_free_full (sink->streams, gst_dash_sink_stream_free);

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) sink);
}

/* Default implementations for the signal handlers */
static GOutputStream *
gst_dash_sink_get_playlist_stream (GstDashSink * sink, const gchar * location)
{
  GFile *file = g_file_new_for_path (location);
  GOutputStream *ostream;
  GError *err = NULL;

  ostream =
      G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
          G_FILE_CREATE_REPLACE_DESTINATION, NULL, &err));
  if (!ostream) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (("Got no output stream for playlist '%s': %s."), location,
            err->message), (NULL));
    g_clear_error (&err);
  }

  g_object_unref (file);

  return ostream;
}

static GOutputStream *
gst_dash_sink_get_fragment_stream (GstDashSink * sink, const gchar * location)
{
  GFile *file = g_file_new_for_path (location);
  GOutputStream *ostream;
  GError *err = NULL;

  ostream =
      G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
          G_FILE_CREATE_REPLACE_DESTINATION, NULL, &err));
  if (!ostream) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (("Got no output stream for fragment '%s': %s."), location,
            err->message), (NULL));
    g_clear_error (&err);
  }

  g_object_unref (file);

  return ostream;
}

static void
gst_dash_sink_class_init (GstDashSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBinClass *bin_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  bin_class = GST_BIN_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &video_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &audio_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &subtitle_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "DASH Sink", "Sink",
      "Dynamic Adaptive Streaming over HTTP sink",
      "Stéphane Cerveau <scerveau@collabora.com>");

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_dash_sink_change_state);
  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_dash_sink_request_new_pad);
  element_class->release_pad = GST_DEBUG_FUNCPTR (gst_dash_sink_release_pad);

  bin_class->handle_message = gst_dash_sink_handle_message;

  gobject_class->dispose = gst_dash_sink_dispose;
  gobject_class->finalize = gst_dash_sink_finalize;
  gobject_class->set_property = gst_dash_sink_set_property;
  gobject_class->get_property = gst_dash_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_MPD_FILENAME,
      g_param_spec_string ("mpd-filename", "MPD filename",
          "filename of the mpd to write", DEFAULT_MPD_FILENAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MPD_ROOT_PATH,
      g_param_spec_string ("mpd-root-path", "MPD Root Path",
          "Path where the MPD and its fragents will be written",
          DEFAULT_MPD_ROOT_PATH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MPD_BASEURL,
      g_param_spec_string ("mpd-baseurl", "MPD BaseURL",
          "BaseURL to set in the MPD", DEFAULT_MPD_ROOT_PATH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TARGET_DURATION,
      g_param_spec_uint ("target-duration", "Target duration",
          "The target duration in seconds of a segment/file. "
          "(0 - disabled, useful for management of segment duration by the "
          "streaming server)", 0, G_MAXUINT, DEFAULT_TARGET_DURATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SEND_KEYFRAME_REQUESTS,
      g_param_spec_boolean ("send-keyframe-requests", "Send Keyframe Requests",
          "Send keyframe requests to ensure correct fragmentation. If this is disabled "
          "then the input must have keyframes in regular intervals",
          DEFAULT_SEND_KEYFRAME_REQUESTS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_SEGMENT_LIST,
      g_param_spec_boolean ("use-segment-list", "Use segment list",
          "Use segment list instead of segment template to create the segments",
          DEFAULT_MPD_USE_SEGMENT_LIST,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MPD_DYNAMIC,
      g_param_spec_boolean ("dynamic", "dynamic", "Provides a dynamic mpd",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MUXER,
      g_param_spec_enum ("muxer", "Muxer",
          "Muxer type to be used by dashsink to generate the fragment",
          GST_TYPE_DASH_SINK_MUXER, DEFAULT_DASH_SINK_MUXER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_MPD_MINIMUM_UPDATE_PERIOD,
      g_param_spec_uint64 ("minimum-update-period", "Minimum update period",
          "Provides to the manifest a minimum update period in milliseconds", 0,
          G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_MPD_MIN_BUFFER_TIME,
      g_param_spec_uint64 ("min-buffer-time", "Mininim buffer time",
          "Provides to the manifest a minimum buffer time in milliseconds", 0,
          G_MAXUINT64, DEFAULT_MPD_MIN_BUFFER_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_MPD_PERIOD_DURATION,
      g_param_spec_uint64 ("period-duration", "period duration",
          "Provides the explicit duration of a period in milliseconds", 0,
          G_MAXUINT64, DEFAULT_MPD_PERIOD_DURATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstDashSink:suggested-presentation-delay
   *
   * set suggested presentation delay of MPD file in milliseconds
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_MPD_SUGGESTED_PRESENTATION_DELAY,
      g_param_spec_uint64 ("suggested-presentation-delay",
          "suggested presentation delay",
          "Provides to the manifest a suggested presentation delay in milliseconds",
          0, G_MAXUINT64, DEFAULT_MPD_SUGGESTED_PRESENTATION_DELAY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstDashSink::get-playlist-stream:
   * @sink: the #GstDashSink
   * @location: Location for the playlist file
   *
   * Returns: #GOutputStream for writing the playlist file.
   *
   * Since: 1.20
   */
  signals[SIGNAL_GET_PLAYLIST_STREAM] =
      g_signal_new_class_handler ("get-playlist-stream",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_CALLBACK (gst_dash_sink_get_playlist_stream), NULL, NULL, NULL,
      G_TYPE_OUTPUT_STREAM, 1, G_TYPE_STRING);

  /**
   * GstDashSink::get-fragment-stream:
   * @sink: the #GstDashSink
   * @location: Location for the fragment file
   *
   * Returns: #GOutputStream for writing the fragment file.
   *
   * Since: 1.20
   */
  signals[SIGNAL_GET_FRAGMENT_STREAM] =
      g_signal_new_class_handler ("get-fragment-stream",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_CALLBACK (gst_dash_sink_get_fragment_stream), NULL, NULL, NULL,
      G_TYPE_OUTPUT_STREAM, 1, G_TYPE_STRING);

  gst_type_mark_as_plugin_api (GST_TYPE_DASH_SINK_MUXER, 0);
}

static gchar *
on_format_location (GstElement * splitmuxsink, guint fragment_id,
    GstDashSinkStream * dash_stream)
{
  GOutputStream *stream = NULL;
  GstDashSink *sink = dash_stream->sink;
  gchar *segment_tpl_path;

  dash_stream->current_segment_id = dash_stream->next_segment_id;
  g_free (dash_stream->current_segment_location);
  if (sink->use_segment_list)
    dash_stream->current_segment_location =
        g_strdup_printf ("%s" DEFAULT_SEGMENT_LIST_TPL ".%s",
        dash_stream->representation_id, dash_stream->current_segment_id,
        dash_muxer_list[sink->muxer].file_ext);
  else {
    dash_stream->current_segment_location =
        g_strdup_printf ("%s" DEFAULT_SEGMENT_TEMPLATE_TPL ".%s",
        dash_stream->representation_id, dash_stream->current_segment_id,
        dash_muxer_list[sink->muxer].file_ext);
  }
  dash_stream->next_segment_id++;

  if (sink->mpd_root_path)
    segment_tpl_path =
        g_build_path (G_DIR_SEPARATOR_S, sink->mpd_root_path,
        dash_stream->current_segment_location, NULL);
  else
    segment_tpl_path = g_strdup (dash_stream->current_segment_location);


  g_signal_emit (sink, signals[SIGNAL_GET_FRAGMENT_STREAM], 0, segment_tpl_path,
      &stream);

  if (!stream)
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (("Got no output stream for fragment '%s'."), segment_tpl_path),
        (NULL));
  else
    g_object_set (dash_stream->giostreamsink, "stream", stream, NULL);

  if (stream)
    g_object_unref (stream);

  g_free (segment_tpl_path);

  return NULL;
}

static gboolean
gst_dash_sink_add_splitmuxsink (GstDashSink * sink, GstDashSinkStream * stream)
{
  GstElement *mux =
      gst_element_factory_make (dash_muxer_list[sink->muxer].element_name,
      NULL);

  if (sink->muxer == GST_DASH_SINK_MUXER_MP4) {
    g_object_set (mux, "fragment-duration", sink->target_duration * GST_MSECOND,
        NULL);
  } else if (sink->muxer == GST_DASH_SINK_MUXER_DASHMP4) {
    g_object_set (mux, "fragment-duration", sink->target_duration * GST_SECOND,
        NULL);
  }

  g_return_val_if_fail (mux != NULL, FALSE);

  stream->splitmuxsink = gst_element_factory_make ("splitmuxsink", NULL);
  if (!stream->splitmuxsink) {
    gst_object_unref (mux);
    return FALSE;
  }
  stream->giostreamsink = gst_element_factory_make ("giostreamsink", NULL);
  if (!stream->giostreamsink) {
    gst_object_unref (stream->splitmuxsink);
    gst_object_unref (mux);
    return FALSE;
  }

  gst_bin_add (GST_BIN (sink), stream->splitmuxsink);

  if (!sink->use_segment_list)
    stream->current_segment_id = 1;
  else
    stream->current_segment_id = 0;
  stream->next_segment_id = stream->current_segment_id;

  g_object_set (stream->splitmuxsink, "location", NULL,
      "max-size-time", ((GstClockTime) sink->target_duration * GST_SECOND),
      "send-keyframe-requests", TRUE, "muxer", mux, "sink",
      stream->giostreamsink, "send-keyframe-requests",
      sink->send_keyframe_requests, NULL);

  if (sink->muxer == GST_DASH_SINK_MUXER_TS)
    g_object_set (stream->splitmuxsink, "reset-muxer", FALSE, NULL);

  g_signal_connect (stream->splitmuxsink, "format-location",
      G_CALLBACK (on_format_location), stream);

  return TRUE;
}

static void
gst_dash_sink_init (GstDashSink * sink)
{
  sink->mpd_filename = g_strdup (DEFAULT_MPD_FILENAME);
  sink->mpd_root_path = g_strdup (DEFAULT_MPD_ROOT_PATH);
  sink->mpd_client = NULL;

  sink->target_duration = DEFAULT_TARGET_DURATION;
  sink->send_keyframe_requests = DEFAULT_SEND_KEYFRAME_REQUESTS;
  sink->mpd_profiles = g_strdup (DEFAULT_MPD_PROFILES);
  sink->use_segment_list = DEFAULT_MPD_USE_SEGMENT_LIST;

  sink->min_buffer_time = DEFAULT_MPD_MIN_BUFFER_TIME;
  sink->period_duration = DEFAULT_MPD_PERIOD_DURATION;
  sink->suggested_presentation_delay = DEFAULT_MPD_SUGGESTED_PRESENTATION_DELAY;

  g_mutex_init (&sink->mpd_lock);

  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_FLAG_SINK);

  gst_dash_sink_reset (sink);
}

static void
gst_dash_sink_reset (GstDashSink * sink)
{
  sink->index = 0;
}

static void
gst_dash_sink_get_stream_metadata (GstDashSink * sink,
    GstDashSinkStream * stream)
{
  GstStructure *s;
  GstCaps *caps = gst_pad_get_current_caps (stream->pad);

  GST_DEBUG_OBJECT (sink, "stream caps %s", gst_caps_to_string (caps));
  s = gst_caps_get_structure (caps, 0);

  g_free (stream->codec);
  stream->codec = gst_codec_utils_caps_get_mime_codec (caps);

  switch (stream->type) {
    case DASH_SINK_STREAM_TYPE_VIDEO:
    {
      gst_structure_get_int (s, "width", &stream->info.video.width);
      gst_structure_get_int (s, "height", &stream->info.video.height);
      break;
    }
    case DASH_SINK_STREAM_TYPE_AUDIO:
    {
      gst_structure_get_int (s, "channels", &stream->info.audio.channels);
      gst_structure_get_int (s, "rate", &stream->info.audio.rate);
      break;
    }
    case DASH_SINK_STREAM_TYPE_SUBTITLE:
    {
      break;
    }
    default:
      break;
  }

  gst_caps_unref (caps);
}

static void
gst_dash_sink_generate_mpd_content (GstDashSink * sink,
    GstDashSinkStream * stream)
{
  if (!sink->mpd_client) {
    GList *l;
    sink->mpd_client = gst_mpd_client_new ();
    /* Add or set root node with stream ids */
    gst_mpd_client_set_root_node (sink->mpd_client,
        "profiles", sink->mpd_profiles,
        "default-namespace", DEFAULT_MPD_NAMESPACE,
        "min-buffer-time", sink->min_buffer_time, NULL);
    if (sink->is_dynamic) {
      GstDateTime *now = gst_date_time_new_now_utc ();
      gst_mpd_client_set_root_node (sink->mpd_client,
          "type", GST_MPD_FILE_TYPE_DYNAMIC,
          "availability-start-time", now, "publish-time", now, NULL);
      gst_date_time_unref (now);
    }
    if (sink->minimum_update_period)
      gst_mpd_client_set_root_node (sink->mpd_client,
          "minimum-update-period", sink->minimum_update_period, NULL);
    if (sink->suggested_presentation_delay)
      gst_mpd_client_set_root_node (sink->mpd_client,
          "suggested-presentation-delay", sink->suggested_presentation_delay,
          NULL);
    if (sink->mpd_baseurl)
      gst_mpd_client_add_baseurl_node (sink->mpd_client, "url",
          sink->mpd_baseurl, NULL);
    /* Add or set period node with stream ids
     * TODO support multiple period
     * */
    sink->current_period_id =
        gst_mpd_client_set_period_node (sink->mpd_client,
        sink->current_period_id, NULL);
    for (l = sink->streams; l != NULL; l = l->next) {
      GstDashSinkStream *stream = (GstDashSinkStream *) l->data;
      /* Add or set adaptation_set node with stream ids
       * AdaptationSet per stream type
       * */
      gst_mpd_client_set_adaptation_set_node (sink->mpd_client,
          sink->current_period_id, stream->adaptation_set_id, NULL);
      /* Add or set representation node with stream ids */
      gst_mpd_client_set_representation_node (sink->mpd_client,
          sink->current_period_id, stream->adaptation_set_id,
          stream->representation_id, "bandwidth", stream->bitrate, "mime-type",
          stream->mimetype, "codecs", stream->codec, NULL);
      /* Set specific to stream type */
      if (stream->type == DASH_SINK_STREAM_TYPE_VIDEO) {
        gst_mpd_client_set_adaptation_set_node (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id, "content-type",
            "video", NULL);
        gst_mpd_client_set_representation_node (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id,
            stream->representation_id, "width", stream->info.video.width,
            "height", stream->info.video.height, NULL);
      } else if (stream->type == DASH_SINK_STREAM_TYPE_AUDIO) {
        gst_mpd_client_set_adaptation_set_node (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id, "content-type",
            "audio", NULL);
        gst_mpd_client_set_representation_node (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id,
            stream->representation_id, "audio-sampling-rate",
            stream->info.audio.rate, NULL);
      }
      if (sink->use_segment_list) {
        /* Add a default segment list */
        gst_mpd_client_set_segment_list (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id,
            stream->representation_id, "duration", sink->target_duration, NULL);
      } else {
        gchar *media_segment_template =
            g_strconcat (stream->representation_id, "_$Number$",
            ".", dash_muxer_list[sink->muxer].file_ext, NULL);
        gst_mpd_client_set_segment_template (sink->mpd_client,
            sink->current_period_id, stream->adaptation_set_id,
            stream->representation_id, "media", media_segment_template,
            "duration", sink->target_duration, NULL);
        g_free (media_segment_template);
      }
    }
  }
  /* MPD updates */
  if (sink->use_segment_list) {
    GST_INFO_OBJECT (sink, "Add segment URL: %s",
        stream->current_segment_location);
    gst_mpd_client_add_segment_url (sink->mpd_client, sink->current_period_id,
        stream->adaptation_set_id, stream->representation_id, "media",
        stream->current_segment_location, NULL);
  } else {
    if (!sink->is_dynamic) {
      if (sink->period_duration != DEFAULT_MPD_PERIOD_DURATION)
        gst_mpd_client_set_period_node (sink->mpd_client,
            sink->current_period_id, "duration", sink->period_duration, NULL);
      else
        gst_mpd_client_set_period_node (sink->mpd_client,
            sink->current_period_id, "duration",
            gst_util_uint64_scale (sink->running_time, 1, GST_MSECOND), NULL);
    }
    if (!sink->minimum_update_period) {
      if (sink->period_duration != DEFAULT_MPD_PERIOD_DURATION)
        gst_mpd_client_set_root_node (sink->mpd_client,
            "media-presentation-duration", sink->period_duration, NULL);
      else
        gst_mpd_client_set_root_node (sink->mpd_client,
            "media-presentation-duration",
            gst_util_uint64_scale (sink->running_time, 1, GST_MSECOND), NULL);
    }
  }
}

static void
gst_dash_sink_write_mpd_file (GstDashSink * sink,
    GstDashSinkStream * current_stream)
{
  char *mpd_content = NULL;
  gint size;
  GError *error = NULL;
  gchar *mpd_filepath = NULL;
  GOutputStream *file_stream = NULL;
  gsize bytes_to_write;

  g_mutex_lock (&sink->mpd_lock);
  gst_dash_sink_generate_mpd_content (sink, current_stream);
  if (!gst_mpd_client_get_xml_content (sink->mpd_client, &mpd_content, &size)) {
    g_mutex_unlock (&sink->mpd_lock);
    return;
  }
  g_mutex_unlock (&sink->mpd_lock);

  if (sink->mpd_root_path)
    mpd_filepath =
        g_build_path (G_DIR_SEPARATOR_S, sink->mpd_root_path,
        sink->mpd_filename, NULL);
  else
    mpd_filepath = g_strdup (sink->mpd_filename);
  GST_DEBUG_OBJECT (sink, "a new mpd content is available: %s", mpd_content);
  GST_DEBUG_OBJECT (sink, "write mpd to %s", mpd_filepath);

  g_signal_emit (sink, signals[SIGNAL_GET_PLAYLIST_STREAM], 0, mpd_filepath,
      &file_stream);
  if (!file_stream) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (("Got no output stream for fragment '%s'."), mpd_filepath), (NULL));
  }

  bytes_to_write = strlen (mpd_content);
  if (!g_output_stream_write_all (file_stream, mpd_content, bytes_to_write,
          NULL, NULL, &error)) {
    GST_ERROR ("Failed to write mpd content: %s", error->message);
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (("Failed to write playlist '%s'."), error->message), (NULL));
    g_error_free (error);
    error = NULL;
  }

  g_free (mpd_content);
  g_free (mpd_filepath);
  g_object_unref (file_stream);
}

static void
gst_dash_sink_handle_message (GstBin * bin, GstMessage * message)
{
  GstDashSink *sink = GST_DASH_SINK (bin);
  GstDashSinkStream *stream = NULL;
  switch (message->type) {
    case GST_MESSAGE_ELEMENT:
    {
      const GstStructure *s = gst_message_get_structure (message);
      GST_DEBUG_OBJECT (sink, "Received message with name %s",
          gst_structure_get_name (s));
      stream =
          gst_dash_sink_stream_from_splitmuxsink (sink->streams,
          GST_ELEMENT (message->src));
      if (stream) {
        if (gst_structure_has_name (s, "splitmuxsink-fragment-opened")) {
          gst_dash_sink_get_stream_metadata (sink, stream);
          gst_structure_get_clock_time (s, "running-time",
              &stream->current_running_time_start);
        } else if (gst_structure_has_name (s, "splitmuxsink-fragment-closed")) {
          GstClockTime running_time;
          gst_structure_get_clock_time (s, "running-time", &running_time);
          if (sink->running_time < running_time)
            sink->running_time = running_time;
          gst_dash_sink_write_mpd_file (sink, stream);
        }
      }
      break;
    }
    case GST_MESSAGE_EOS:{
      gst_dash_sink_write_mpd_file (sink, NULL);
      break;
    }
    default:
      break;
  }

  GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}

static GstPadProbeReturn
_dash_sink_buffers_probe (GstPad * pad, GstPadProbeInfo * probe_info,
    gpointer user_data)
{
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (probe_info);
  GstDashSinkStream *stream = (GstDashSinkStream *) user_data;

  if (GST_BUFFER_DURATION (buffer))
    stream->bitrate =
        gst_buffer_get_size (buffer) * GST_SECOND /
        GST_BUFFER_DURATION (buffer);

  return GST_PAD_PROBE_OK;
}

static GstPad *
gst_dash_sink_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * pad_name, const GstCaps * caps)
{
  GstDashSink *sink = GST_DASH_SINK (element);
  GstDashSinkStream *stream = NULL;
  GstPad *pad = NULL;
  GstPad *peer = NULL;
  const gchar *split_pad_name = pad_name;

  stream = g_new0 (GstDashSinkStream, 1);
  stream->sink = g_object_ref (sink);
  if (g_str_has_prefix (templ->name_template, "video")) {
    stream->type = DASH_SINK_STREAM_TYPE_VIDEO;
    stream->adaptation_set_id = ADAPTATION_SET_ID_VIDEO;
    split_pad_name = "video";
  } else if (g_str_has_prefix (templ->name_template, "audio")) {
    stream->type = DASH_SINK_STREAM_TYPE_AUDIO;
    stream->adaptation_set_id = ADAPTATION_SET_ID_AUDIO;
  } else if (g_str_has_prefix (templ->name_template, "subtitle")) {
    stream->type = DASH_SINK_STREAM_TYPE_SUBTITLE;
    stream->adaptation_set_id = ADAPTATION_SET_ID_SUBTITLE;
  }

  if (pad_name)
    stream->representation_id = g_strdup (pad_name);
  else
    stream->representation_id =
        gst_dash_sink_stream_get_next_name (sink->streams, stream->type);


  stream->mimetype = g_strdup (dash_muxer_list[sink->muxer].mimetype);


  if (!gst_dash_sink_add_splitmuxsink (sink, stream)) {
    GST_ERROR_OBJECT (sink,
        "Unable to create splitmuxsink element for pad template name %s",
        templ->name_template);
    gst_dash_sink_stream_free (stream);
    goto done;
  }

  peer = gst_element_request_pad_simple (stream->splitmuxsink, split_pad_name);
  if (!peer) {
    GST_ERROR_OBJECT (sink, "Unable to request pad name %s", split_pad_name);
    return NULL;
  }

  pad = gst_ghost_pad_new_from_template (pad_name, peer, templ);
  gst_pad_set_active (pad, TRUE);
  gst_element_add_pad (element, pad);
  gst_object_unref (peer);

  stream->pad = pad;

  stream->buffer_probe = gst_pad_add_probe (stream->pad,
      GST_PAD_PROBE_TYPE_BUFFER, _dash_sink_buffers_probe, stream, NULL);

  sink->streams = g_list_append (sink->streams, stream);
  GST_DEBUG_OBJECT (sink, "Adding a new stream with id %s",
      stream->representation_id);

done:
  return pad;
}

static void
gst_dash_sink_release_pad (GstElement * element, GstPad * pad)
{
  GstDashSink *sink = GST_DASH_SINK (element);
  GstPad *peer;
  GstDashSinkStream *stream =
      gst_dash_sink_stream_from_pad (sink->streams, pad);

  g_return_if_fail (stream != NULL);

  peer = gst_pad_get_peer (pad);
  if (peer) {
    gst_element_release_request_pad (stream->splitmuxsink, pad);
    gst_object_unref (peer);
  }

  if (stream->buffer_probe > 0) {
    gst_pad_remove_probe (pad, stream->buffer_probe);
    stream->buffer_probe = 0;
  }

  gst_object_ref (pad);
  gst_element_remove_pad (element, pad);
  gst_pad_set_active (pad, FALSE);

  stream->pad = NULL;

  gst_object_unref (pad);
}

static GstStateChangeReturn
gst_dash_sink_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstDashSink *sink = GST_DASH_SINK (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!g_list_length (sink->streams)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_dash_sink_reset (sink);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_dash_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  switch (prop_id) {
    case PROP_MPD_FILENAME:
      g_free (sink->mpd_filename);
      sink->mpd_filename = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_PATH:
      g_free (sink->mpd_root_path);
      sink->mpd_root_path = g_value_dup_string (value);
      break;
    case PROP_MPD_BASEURL:
      g_free (sink->mpd_baseurl);
      sink->mpd_baseurl = g_value_dup_string (value);
      break;
    case PROP_TARGET_DURATION:
      sink->target_duration = g_value_get_uint (value);
      break;
    case PROP_SEND_KEYFRAME_REQUESTS:
      sink->send_keyframe_requests = g_value_get_boolean (value);
      break;
    case PROP_USE_SEGMENT_LIST:
      sink->use_segment_list = g_value_get_boolean (value);
      break;
    case PROP_MPD_DYNAMIC:
      sink->is_dynamic = g_value_get_boolean (value);
      break;
    case PROP_MUXER:
      sink->muxer = g_value_get_enum (value);
      break;
    case PROP_MPD_MINIMUM_UPDATE_PERIOD:
      sink->minimum_update_period = g_value_get_uint64 (value);
      break;
    case PROP_MPD_SUGGESTED_PRESENTATION_DELAY:
      sink->suggested_presentation_delay = g_value_get_uint64 (value);
      break;
    case PROP_MPD_MIN_BUFFER_TIME:
      sink->min_buffer_time = g_value_get_uint64 (value);
      break;
    case PROP_MPD_PERIOD_DURATION:
      sink->period_duration = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dash_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  switch (prop_id) {
    case PROP_MPD_FILENAME:
      g_value_set_string (value, sink->mpd_filename);
      break;
    case PROP_MPD_ROOT_PATH:
      g_value_set_string (value, sink->mpd_root_path);
      break;
    case PROP_MPD_BASEURL:
      g_value_set_string (value, sink->mpd_baseurl);
      break;
    case PROP_TARGET_DURATION:
      g_value_set_uint (value, sink->target_duration);
      break;
    case PROP_SEND_KEYFRAME_REQUESTS:
      g_value_set_boolean (value, sink->send_keyframe_requests);
      break;
    case PROP_USE_SEGMENT_LIST:
      g_value_set_boolean (value, sink->use_segment_list);
      break;
    case PROP_MPD_DYNAMIC:
      g_value_set_boolean (value, sink->is_dynamic);
      break;
    case PROP_MUXER:
      g_value_set_enum (value, sink->muxer);
      break;
    case PROP_MPD_MINIMUM_UPDATE_PERIOD:
      g_value_set_uint64 (value, sink->minimum_update_period);
      break;
    case PROP_MPD_SUGGESTED_PRESENTATION_DELAY:
      g_value_set_uint64 (value, sink->suggested_presentation_delay);
      break;
    case PROP_MPD_MIN_BUFFER_TIME:
      g_value_set_uint64 (value, sink->min_buffer_time);
      break;
    case PROP_MPD_PERIOD_DURATION:
      g_value_set_uint64 (value, sink->period_duration);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
