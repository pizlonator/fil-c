/* Copyright (C) <2018, 2019> Philippe Normand <philn@igalia.com>
 * Copyright (C) <2018, 2019> Žan Doberšek <zdobersek@igalia.com>
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
 * SECTION:element-wpesrc
 * @title: wpesrc
 *
 * The wpesrc element is used to produce a video texture representing a web page
 * rendered off-screen by WPE.
 *
 * Starting from WPEBackend-FDO 1.6.x, software rendering support is available.
 * This features allows wpesrc to be used on machines without GPU, and/or for
 * testing purpose. To enable it, set the `LIBGL_ALWAYS_SOFTWARE=true`
 * environment variable and make sure `video/x-raw, format=BGRA` caps are
 * negotiated by the wpesrc element.
 *
 * ## Example launch lines
 *
 * ### Show the GStreamer website homepage
 *
 * ```
 * gst-launch-1.0 -v wpesrc location="https://gstreamer.freedesktop.org" ! queue ! glimagesink
 * ```
 *
 * ### Save the first 50 video frames generated for the GStreamer website as PNG files in /tmp
 *
 * ```
 * LIBGL_ALWAYS_SOFTWARE=true gst-launch-1.0 -v wpesrc num-buffers=50 location="https://gstreamer.freedesktop.org" ! videoconvert ! pngenc ! multifilesink location=/tmp/snapshot-%05d.png
 * ```
 *
 *
 * ### Show the GStreamer website homepage as played with GstPlayer in a GTK+ window
 *
 * ```
 * export GST_PLUGIN_FEATURE_RANK="wpesrc:MAX"
 * gst-play-1.0 --videosink gtkglsink web+https://gstreamer.freedesktop.org
 * ```
 *
 * Until 1.20 the `web://` URI protocol was supported, along with `wpe://`.
 *
 * The supported URI protocols are `web+http://`, `web+https://` and `web+file://`.
 * Since: 1.22
 *
 * ### Composite WPE with a video stream in a single OpenGL scene
 *
 * ```
 * gst-launch-1.0  glvideomixer name=m sink_1::zorder=0 ! glimagesink wpesrc location="file:///home/phil/Downloads/plunk/index.html" draw-background=0 ! m. videotestsrc ! queue ! glupload ! glcolorconvert ! m.
 * ```
 *
 *
 * ### Composite WPE with a video stream, sink_0 pad properties have to match the video dimensions
 *
 * ```
 * gst-launch-1.0 glvideomixer name=m sink_1::zorder=0 sink_0::height=818 sink_0::width=1920 ! gtkglsink wpesrc location="file:///home/phil/Downloads/plunk/index.html" draw-background=0 ! m. uridecodebin uri="http://192.168.1.44/Sintel.2010.1080p.mkv" name=d d. ! queue ! glupload ! glcolorconvert ! m.
 * ```
 *
 * Additionally, any audio stream created by WPE is exposed as "sometimes" audio
 * source pads.
 *
 * This source also relays GStreamer bus messages from the GStreamer pipelines
 * running inside the web pages  as [element custom](gst_message_new_custom)
 * messages which structure is called `WpeForwarded` and has the following
 * fields:
 *
 * * `message`: The original #GstMessage
 * * `wpesrc-original-src-name`: Name of the original element posting the
 *   message
 * * `wpesrc-original-src-type`: Name of the GType of the original element
 *   posting the message
 * * `wpesrc-original-src-path`: [Path](gst_object_get_path_string) of the
 *   original element positing the message
 *
 * Note: This feature will be disabled if you disable the tracer subsystem.
 */

#include "gstwpesrcbin.h"
#include "gstwpevideosrc.h"
#include "gstwpe.h"
#include "gstwpethreadedview.h"

#include <gst/allocators/allocators.h>
#include <gst/base/gstflowcombiner.h>
#include <wpe/extensions/audio.h>

#include <sys/mman.h>
#include <unistd.h>

G_DEFINE_TYPE (GstWpeAudioPad, gst_wpe_audio_pad, GST_TYPE_GHOST_PAD);

static void
gst_wpe_audio_pad_class_init (GstWpeAudioPadClass * klass)
{
}

static void
gst_wpe_audio_pad_init (GstWpeAudioPad * pad)
{
  gst_audio_info_init (&pad->info);
  pad->discont_pending = TRUE;
  pad->buffer_time = 0;
}

static GstWpeAudioPad *
gst_wpe_audio_pad_new (const gchar * name)
{
  GstWpeAudioPad *pad = GST_WPE_AUDIO_PAD (g_object_new (gst_wpe_audio_pad_get_type (),
    "name", name, "direction", GST_PAD_SRC, NULL));

  if (!gst_ghost_pad_construct (GST_GHOST_PAD (pad))) {
    gst_object_unref (pad);
    return NULL;
  }

  return pad;
}

struct _GstWpeSrc
{
  GstBin parent;

  GstAllocator *fd_allocator;
  GstElement *video_src;
  GHashTable *audio_src_pads;
  GstFlowCombiner *flow_combiner;
};

enum
{
 PROP_0,
 PROP_LOCATION,
 PROP_DRAW_BACKGROUND
};

enum
{
 SIGNAL_LOAD_BYTES,
 SIGNAL_RUN_JAVASCRIPT,
 LAST_SIGNAL
};

static guint gst_wpe_video_src_signals[LAST_SIGNAL] = { 0 };

static void gst_wpe_src_uri_handler_init (gpointer iface, gpointer data);

GST_DEBUG_CATEGORY_EXTERN (wpe_src_debug);
#define GST_CAT_DEFAULT wpe_src_debug

#define gst_wpe_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWpeSrc, gst_wpe_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_wpe_src_uri_handler_init));

/**
 * GstWpeSrc!video
 *
 * Since: 1.20
 */
static GstStaticPadTemplate video_src_factory =
GST_STATIC_PAD_TEMPLATE ("video", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(memory:GLMemory), "
                     "format = (string) RGBA, "
                     "width = " GST_VIDEO_SIZE_RANGE ", "
                     "height = " GST_VIDEO_SIZE_RANGE ", "
                     "framerate = " GST_VIDEO_FPS_RANGE ", "
                     "pixel-aspect-ratio = (fraction)1/1,"
                     "texture-target = (string)2D"
                     "; video/x-raw, "
                     "format = (string) BGRA, "
                     "width = " GST_VIDEO_SIZE_RANGE ", "
                     "height = " GST_VIDEO_SIZE_RANGE ", "
                     "framerate = " GST_VIDEO_FPS_RANGE ", "
                     "pixel-aspect-ratio = (fraction)1/1"
                     ));

/**
 * GstWpeSrc!audio_%u
 *
 * Each audio stream in the renderer web page will expose the and `audio_%u`
 * #GstPad.
 *
 * Since: 1.20
 */
static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE ("audio_%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ( \
        GST_AUDIO_CAPS_MAKE (GST_AUDIO_NE (F32)) ", layout=(string)interleaved; " \
        GST_AUDIO_CAPS_MAKE (GST_AUDIO_NE (F64)) ", layout=(string)interleaved; " \
        GST_AUDIO_CAPS_MAKE (GST_AUDIO_NE (S16)) ", layout=(string)interleaved" \
));

static GstFlowReturn
gst_wpe_src_chain_buffer (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstWpeSrc *src = GST_WPE_SRC (gst_object_get_parent (parent));
  GstFlowReturn result, chain_result;

  chain_result = gst_proxy_pad_chain_default (pad, GST_OBJECT_CAST (src), buffer);
  result = gst_flow_combiner_update_pad_flow (src->flow_combiner, pad, chain_result);
  gst_object_unref (src);

  if (result == GST_FLOW_FLUSHING)
    return chain_result;

  return result;
}

void
gst_wpe_src_new_audio_stream (GstWpeSrc *src, guint32 id, GstCaps *caps, const gchar *stream_id)
{
  GstWpeAudioPad *audio_pad;
  GstPad *pad;
  gchar *name;
  GstEvent *stream_start;
  GstSegment segment;

  name = g_strdup_printf ("audio_%u", id);
  audio_pad = gst_wpe_audio_pad_new (name);
  pad = GST_PAD_CAST (audio_pad);
  g_free (name);

  gst_pad_set_active (pad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (src), pad);
  gst_flow_combiner_add_pad (src->flow_combiner, pad);

  GST_DEBUG_OBJECT (src, "Adding pad: %" GST_PTR_FORMAT, pad);

  stream_start = gst_event_new_stream_start (stream_id);
  gst_pad_push_event (pad, stream_start);

  gst_audio_info_from_caps (&audio_pad->info, caps);
  gst_pad_push_event (pad, gst_event_new_caps (caps));

  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_push_event (pad, gst_event_new_segment (&segment));

  g_hash_table_insert (src->audio_src_pads, GUINT_TO_POINTER (id), audio_pad);
}

void
gst_wpe_src_set_audio_shm (GstWpeSrc* src, GUnixFDList *fds, guint32 id)
{
  gint fd;
  GstWpeAudioPad *audio_pad = GST_WPE_AUDIO_PAD (g_hash_table_lookup (src->audio_src_pads, GUINT_TO_POINTER (id)));

  g_return_if_fail (GST_IS_WPE_SRC (src));
  g_return_if_fail (fds);
  g_return_if_fail (g_unix_fd_list_get_length (fds) == 1);
  g_return_if_fail (audio_pad->fd <= 0);

  fd = g_unix_fd_list_get (fds, 0, NULL);
  g_return_if_fail (fd >= 0);

  if (audio_pad->fd > 0)
    close(audio_pad->fd);

  audio_pad->fd = dup(fd);
}

void
gst_wpe_src_push_audio_buffer (GstWpeSrc* src, guint32 id, guint64 size)
{
  GstWpeAudioPad *audio_pad = GST_WPE_AUDIO_PAD (g_hash_table_lookup (src->audio_src_pads, GUINT_TO_POINTER (id)));
  GstBuffer *buffer;

  g_return_if_fail (audio_pad->fd > 0);

  GST_TRACE_OBJECT (audio_pad, "Handling incoming audio packet");

  gpointer data = mmap (0, size, PROT_READ, MAP_PRIVATE, audio_pad->fd, 0);
  buffer = gst_buffer_new_memdup (data, size);
  munmap (data, size);
  gst_buffer_add_audio_meta(
      buffer, &audio_pad->info,
      size / GST_AUDIO_INFO_BPF(&audio_pad->info), NULL);

  audio_pad->buffer_time = gst_element_get_current_running_time (GST_ELEMENT (src));
  GST_BUFFER_DTS (buffer) = audio_pad->buffer_time;
  GST_BUFFER_PTS (buffer) = audio_pad->buffer_time;

  GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  if (audio_pad->discont_pending) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    audio_pad->discont_pending = FALSE;
  }

  gst_flow_combiner_update_pad_flow (src->flow_combiner, GST_PAD (audio_pad),
    gst_pad_push (GST_PAD_CAST (audio_pad), buffer));
}

static void
gst_wpe_src_remove_audio_pad (GstWpeSrc *src, GstPad *pad)
{
  GST_DEBUG_OBJECT (src, "Removing pad: %" GST_PTR_FORMAT, pad);
  gst_element_remove_pad (GST_ELEMENT_CAST (src), pad);
  gst_flow_combiner_remove_pad (src->flow_combiner, pad);
}

void
gst_wpe_src_stop_audio_stream(GstWpeSrc* src, guint32 id)
{
  GstPad *pad = GST_PAD (g_hash_table_lookup (src->audio_src_pads, GUINT_TO_POINTER (id)));
  g_return_if_fail (GST_IS_PAD (pad));

  GST_INFO_OBJECT(pad, "Stopping");
  gst_pad_push_event (pad, gst_event_new_eos ());
  gst_wpe_src_remove_audio_pad (src, pad);
  g_hash_table_remove (src->audio_src_pads, GUINT_TO_POINTER (id));
}

void
gst_wpe_src_pause_audio_stream(GstWpeSrc* src, guint32 id)
{
  GstWpeAudioPad *audio_pad = GST_WPE_AUDIO_PAD (g_hash_table_lookup (src->audio_src_pads, GUINT_TO_POINTER (id)));
  GstPad *pad = GST_PAD_CAST (audio_pad);
  g_return_if_fail (GST_IS_PAD (pad));

  GST_INFO_OBJECT(pad, "Pausing");
  gst_pad_push_event (pad, gst_event_new_gap (audio_pad->buffer_time, GST_CLOCK_TIME_NONE));

  audio_pad->discont_pending = TRUE;
}

static void
gst_wpe_src_load_bytes (GstWpeVideoSrc * src, GBytes * bytes)
{
  GstWpeSrc *self = GST_WPE_SRC (src);

  if (self->video_src)
    g_signal_emit_by_name (self->video_src, "load-bytes", bytes, NULL);
}

static void
gst_wpe_src_run_javascript (GstWpeVideoSrc * src, const gchar * script)
{
  GstWpeSrc *self = GST_WPE_SRC (src);

  if (self->video_src)
    g_signal_emit_by_name (self->video_src, "run-javascript", script, NULL);
}

static void
gst_wpe_src_set_location (GstWpeSrc * src, const gchar * location)
{
  g_object_set (src->video_src, "location", location, NULL);
}

static void
gst_wpe_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWpeSrc *self = GST_WPE_SRC (object);

  if (self->video_src)
    g_object_get_property (G_OBJECT (self->video_src), pspec->name, value);
}

static void
gst_wpe_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWpeSrc *self = GST_WPE_SRC (object);

  if (self->video_src) {
    if (prop_id == PROP_LOCATION)
      gst_wpe_src_set_location (self, g_value_get_string (value));
    else
      g_object_set_property (G_OBJECT (self->video_src), pspec->name, value);
  }
}

static GstURIType
gst_wpe_src_uri_get_type (GType)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_wpe_src_get_protocols (GType)
{
  static const gchar *protocols[] = {"web+http", "web+https", "web+file", NULL};
  return protocols;
}

static gchar *
gst_wpe_src_get_uri (GstURIHandler * handler)
{
  GstWpeSrc *src = GST_WPE_SRC (handler);
  gchar *ret = NULL;

  g_object_get(src->video_src, "location", &ret, NULL);

  return ret;
}

static gboolean
gst_wpe_src_set_uri (GstURIHandler * handler, const gchar * uristr,
    GError ** error)
{
  gboolean res = TRUE;
  gchar *location;
  GstWpeSrc *src = GST_WPE_SRC (handler);
  const gchar *protocol;
  GstUri *uri;

  protocol = gst_uri_get_protocol (uristr);
  g_return_val_if_fail (g_str_has_prefix (protocol, "web+"), FALSE);

  uri = gst_uri_from_string (uristr);
  gst_uri_set_scheme (uri, protocol + 4);

  location = gst_uri_to_string (uri);
  gst_wpe_src_set_location (src, location);

  gst_uri_unref (uri);
  g_free (location);

  return res;
}

static void
gst_wpe_src_uri_handler_init (gpointer iface_ptr, gpointer data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) iface_ptr;

  iface->get_type = gst_wpe_src_uri_get_type;
  iface->get_protocols = gst_wpe_src_get_protocols;
  iface->get_uri = gst_wpe_src_get_uri;
  iface->set_uri = gst_wpe_src_set_uri;
}

static void
gst_wpe_src_init (GstWpeSrc * src)
{
  GstPad *pad;
  GstPad *ghost_pad;
  GstProxyPad *proxy_pad;

  gst_bin_set_suppressed_flags (GST_BIN_CAST (src),
      static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));
  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_FLAG_SOURCE);

  src->fd_allocator = gst_fd_allocator_new ();
  src->audio_src_pads = g_hash_table_new (g_direct_hash, g_direct_equal);
  src->flow_combiner = gst_flow_combiner_new ();
  src->video_src = gst_element_factory_make ("wpevideosrc", NULL);

  gst_bin_add (GST_BIN_CAST (src), src->video_src);

  pad = gst_element_get_static_pad (GST_ELEMENT_CAST (src->video_src), "src");
  ghost_pad = gst_ghost_pad_new_from_template ("video", pad,
    gst_static_pad_template_get (&video_src_factory));
  proxy_pad = gst_proxy_pad_get_internal (GST_PROXY_PAD (ghost_pad));
  gst_pad_set_active (GST_PAD_CAST (proxy_pad), TRUE);

  gst_element_add_pad (GST_ELEMENT_CAST (src), GST_PAD_CAST (ghost_pad));
  gst_flow_combiner_add_pad (src->flow_combiner, GST_PAD_CAST (ghost_pad));
  gst_pad_set_chain_function (GST_PAD_CAST (proxy_pad), gst_wpe_src_chain_buffer);

  gst_object_unref (proxy_pad);
  gst_object_unref (pad);
}

static gboolean
gst_wpe_audio_remove_audio_pad  (gint32  *id, GstPad *pad, GstWpeSrc  *self)
{
  gst_wpe_src_remove_audio_pad (self, pad);

  return TRUE;
}

static GstStateChangeReturn
gst_wpe_src_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn result;
  GstWpeSrc *src = GST_WPE_SRC (element);

  GST_DEBUG_OBJECT (src, "%s", gst_state_change_get_name (transition));
  result = GST_CALL_PARENT_WITH_DEFAULT (GST_ELEMENT_CLASS , change_state, (element, transition), GST_STATE_CHANGE_FAILURE);

  switch (transition) {
  case GST_STATE_CHANGE_PAUSED_TO_READY:{
    g_hash_table_foreach_remove (src->audio_src_pads, (GHRFunc) gst_wpe_audio_remove_audio_pad, src);
    gst_flow_combiner_reset (src->flow_combiner);
    break;
  }
  default:
    break;
  }

  return result;
}

static void
gst_wpe_src_finalize (GObject *object)
{
    GstWpeSrc *src = GST_WPE_SRC (object);

    g_hash_table_unref (src->audio_src_pads);
    gst_flow_combiner_free (src->flow_combiner);
    gst_object_unref (src->fd_allocator);

    GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_wpe_src_class_init (GstWpeSrcClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_wpe_src_set_property;
  gobject_class->get_property = gst_wpe_src_get_property;
  gobject_class->finalize = gst_wpe_src_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "location", "The URL to display", DEFAULT_LOCATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_DRAW_BACKGROUND,
      g_param_spec_boolean ("draw-background", "Draws the background",
          "Whether to draw the WebView background", TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class, "WPE source",
      "Source/Video/Audio", "Creates Audio/Video streams from a web"
      " page using WPE web engine",
      "Philippe Normand <philn@igalia.com>, Žan Doberšek "
      "<zdobersek@igalia.com>");

  /**
   * GstWpeSrc::load-bytes:
   * @src: the object which received the signal
   * @bytes: the GBytes data to load
   *
   * Load the specified bytes into the internal webView.
   */
  gst_wpe_video_src_signals[SIGNAL_LOAD_BYTES] =
      g_signal_new_class_handler ("load-bytes", G_TYPE_FROM_CLASS (klass),
      static_cast < GSignalFlags > (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
      G_CALLBACK (gst_wpe_src_load_bytes), NULL, NULL, NULL, G_TYPE_NONE, 1,
      G_TYPE_BYTES);

  /**
   * GstWpeSrc::run-javascript:
   * @src: the object which received the signal
   * @script: the script to run
   *
   * Asynchronously run script in the context of the current page on the
   * internal webView.
   *
   * Since: 1.22
   */
  gst_wpe_video_src_signals[SIGNAL_RUN_JAVASCRIPT] =
      g_signal_new_class_handler ("run-javascript", G_TYPE_FROM_CLASS (klass),
      static_cast < GSignalFlags > (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
      G_CALLBACK (gst_wpe_src_run_javascript), NULL, NULL, NULL, G_TYPE_NONE, 1,
      G_TYPE_STRING);
  element_class->change_state = GST_DEBUG_FUNCPTR (gst_wpe_src_change_state);

  gst_element_class_add_static_pad_template (element_class, &video_src_factory);
  gst_element_class_add_static_pad_template (element_class, &audio_src_factory);
}
