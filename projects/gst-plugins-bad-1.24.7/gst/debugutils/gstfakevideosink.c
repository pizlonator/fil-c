/*
 * GStreamer
 * Copyright (C) 2017 Collabora Inc.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-fakevideosink
 * @title: fakevideosink
 *
 * This element is the same as fakesink but will pretend to support various
 * allocation meta API like GstVideoMeta in order to prevent memory copies.
 * This is useful for throughput testing and testing zero-copy path while
 * creating a new pipeline.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 videotestsrc ! fakevideosink
 * gst-launch-1.0 videotestsrc ! fpsdisplaysink text-overlay=false video-sink=fakevideosink
 * ]|
 *
 * Since 1.14
 */

#include "gstdebugutilsbadelements.h"
#include "gstfakevideosink.h"
#include "gstfakesinkutils.h"

#include <gst/video/video.h>

#define C_FLAGS(v) ((guint) v)

GType
gst_fake_video_sink_allocation_meta_flags_get_type (void)
{
  static const GFlagsValue values[] = {
    {C_FLAGS (GST_ALLOCATION_FLAG_CROP_META),
        "Expose the crop meta as supported", "crop"},
    {C_FLAGS (GST_ALLOCATION_FLAG_OVERLAY_COMPOSITION_META),
          "Expose the overlay composition meta as supported",
        "overlay-composition"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id =
        g_flags_register_static ("GstFakeVideoSinkAllocationMetaFlags", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

typedef enum
{
  FAKE_SINK_STATE_ERROR_NONE = 0,
  FAKE_SINK_STATE_ERROR_NULL_READY,
  FAKE_SINK_STATE_ERROR_READY_PAUSED,
  FAKE_SINK_STATE_ERROR_PAUSED_PLAYING,
  FAKE_SINK_STATE_ERROR_PLAYING_PAUSED,
  FAKE_SINK_STATE_ERROR_PAUSED_READY,
  FAKE_SINK_STATE_ERROR_READY_NULL
} GstFakeSinkStateError;

#define DEFAULT_DROP_OUT_OF_SEGMENT TRUE
#define DEFAULT_STATE_ERROR FAKE_SINK_STATE_ERROR_NONE
#define DEFAULT_SILENT TRUE
#define DEFAULT_DUMP FALSE
#define DEFAULT_SIGNAL_HANDOFFS FALSE
#define DEFAULT_LAST_MESSAGE NULL
#define DEFAULT_CAN_ACTIVATE_PUSH TRUE
#define DEFAULT_CAN_ACTIVATE_PULL FALSE
#define DEFAULT_NUM_BUFFERS -1

/**
 * GstFakeVideoSinkStateError:
 *
 * Proxy for GstFakeSinkError.
 *
 * Since: 1.22
 */

#define GST_TYPE_FAKE_VIDEO_SINK_STATE_ERROR (gst_fake_video_sink_state_error_get_type())
static GType
gst_fake_video_sink_state_error_get_type (void)
{
  static GType fakevideosink_state_error_type = 0;
  static const GEnumValue fakevideosink_state_error[] = {
    {FAKE_SINK_STATE_ERROR_NONE, "No state change errors", "none"},
    {FAKE_SINK_STATE_ERROR_NULL_READY,
        "Fail state change from NULL to READY", "null-to-ready"},
    {FAKE_SINK_STATE_ERROR_READY_PAUSED,
        "Fail state change from READY to PAUSED", "ready-to-paused"},
    {FAKE_SINK_STATE_ERROR_PAUSED_PLAYING,
        "Fail state change from PAUSED to PLAYING", "paused-to-playing"},
    {FAKE_SINK_STATE_ERROR_PLAYING_PAUSED,
        "Fail state change from PLAYING to PAUSED", "playing-to-paused"},
    {FAKE_SINK_STATE_ERROR_PAUSED_READY,
        "Fail state change from PAUSED to READY", "paused-to-ready"},
    {FAKE_SINK_STATE_ERROR_READY_NULL,
        "Fail state change from READY to NULL", "ready-to-null"},
    {0, NULL, NULL},
  };

  if (!fakevideosink_state_error_type) {
    fakevideosink_state_error_type =
        g_enum_register_static ("GstFakeVideoSinkStateError",
        fakevideosink_state_error);
  }
  return fakevideosink_state_error_type;
}

enum
{
  PROP_0,
  PROP_ALLOCATION_META_FLAGS,
  PROP_STATE_ERROR,
  PROP_SILENT,
  PROP_DUMP,
  PROP_SIGNAL_HANDOFFS,
  PROP_DROP_OUT_OF_SEGMENT,
  PROP_LAST_MESSAGE,
  PROP_CAN_ACTIVATE_PUSH,
  PROP_CAN_ACTIVATE_PULL,
  PROP_NUM_BUFFERS,
  PROP_LAST
};

enum
{
  SIGNAL_HANDOFF,
  SIGNAL_PREROLL_HANDOFF,
  LAST_SIGNAL
};

static guint gst_fake_video_sink_signals[LAST_SIGNAL] = { 0 };

static GParamSpec *pspec_last_message = NULL;

#define ALLOCATION_META_DEFAULT_FLAGS GST_ALLOCATION_FLAG_CROP_META | GST_ALLOCATION_FLAG_OVERLAY_COMPOSITION_META

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
            GST_VIDEO_FORMATS_ANY)));

G_DEFINE_TYPE (GstFakeVideoSink, gst_fake_video_sink, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (fakevideosink, "fakevideosink",
    GST_RANK_NONE, gst_fake_video_sink_get_type ());

static gboolean
gst_fake_video_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstFakeVideoSink *self = GST_FAKE_VIDEO_SINK (parent);
  GstCaps *caps;
  GstVideoInfo info;
  guint min_buffers = 1;

  if (GST_QUERY_TYPE (query) != GST_QUERY_ALLOCATION)
    return gst_pad_query_default (pad, parent, query);

  gst_query_parse_allocation (query, &caps, NULL);
  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  /* Request an extra buffer if we are keeping a ref on the last rendered buffer */
  if (gst_base_sink_is_last_sample_enabled (GST_BASE_SINK (self->child)))
    min_buffers++;

  gst_query_add_allocation_pool (query, NULL, info.size, min_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  GST_OBJECT_LOCK (self);
  if (self->allocation_meta_flags & GST_ALLOCATION_FLAG_CROP_META)
    gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  if (self->allocation_meta_flags &
      GST_ALLOCATION_FLAG_OVERLAY_COMPOSITION_META)
    gst_query_add_allocation_meta (query,
        GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);

  GST_OBJECT_UNLOCK (self);

  /* add here any meta API that would help support zero-copy */

  return TRUE;
}

static void
gst_fake_video_sink_proxy_handoff (GstElement * element, GstBuffer * buffer,
    GstPad * pad, GstFakeVideoSink * self)
{
  g_signal_emit (self, gst_fake_video_sink_signals[SIGNAL_HANDOFF], 0,
      buffer, self->sinkpad);
}

static void
gst_fake_video_sink_proxy_preroll_handoff (GstElement * element,
    GstBuffer * buffer, GstPad * pad, GstFakeVideoSink * self)
{
  g_signal_emit (self, gst_fake_video_sink_signals[SIGNAL_PREROLL_HANDOFF], 0,
      buffer, self->sinkpad);
}

static void
gst_fake_video_sink_init (GstFakeVideoSink * self)
{
  GstElement *child;
  GstPadTemplate *template = gst_static_pad_template_get (&sink_factory);

  child = gst_element_factory_make ("fakesink", "sink");

  self->allocation_meta_flags = ALLOCATION_META_DEFAULT_FLAGS;

  if (child) {
    GstPad *sink_pad = gst_element_get_static_pad (child, "sink");
    GstPad *ghost_pad;

    /* mimic GstVideoSink base class */
    g_object_set (child, "max-lateness", 5 * GST_MSECOND,
        "processing-deadline", 15 * GST_MSECOND, "qos", TRUE, "sync", TRUE,
        NULL);

    gst_bin_add (GST_BIN (self), child);

    self->sinkpad = ghost_pad =
        gst_ghost_pad_new_from_template ("sink", sink_pad, template);
    gst_object_unref (template);
    gst_element_add_pad (GST_ELEMENT (self), ghost_pad);
    gst_object_unref (sink_pad);

    gst_pad_set_query_function (ghost_pad, gst_fake_video_sink_query);

    self->child = child;

    g_signal_connect (child, "handoff",
        G_CALLBACK (gst_fake_video_sink_proxy_handoff), self);
    g_signal_connect (child, "preroll-handoff",
        G_CALLBACK (gst_fake_video_sink_proxy_preroll_handoff), self);
  } else {
    g_warning ("Check your GStreamer installation, "
        "core element 'fakesink' is missing.");
  }
}

static void
gst_fake_video_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFakeVideoSink *self = GST_FAKE_VIDEO_SINK (object);

  switch (property_id) {
    case PROP_ALLOCATION_META_FLAGS:
      GST_OBJECT_LOCK (self);
      g_value_set_flags (value, self->allocation_meta_flags);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      g_object_get_property (G_OBJECT (self->child), pspec->name, value);
      break;
  }
}

static void
gst_fake_video_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFakeVideoSink *self = GST_FAKE_VIDEO_SINK (object);

  switch (property_id) {
    case PROP_ALLOCATION_META_FLAGS:
      GST_OBJECT_LOCK (self);
      self->allocation_meta_flags = g_value_get_flags (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      g_object_set_property (G_OBJECT (self->child), pspec->name, value);
      break;
  }
}

static void
gst_fake_video_sink_class_init (GstFakeVideoSinkClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GObjectClass *base_sink_class;

  object_class->get_property = gst_fake_video_sink_get_property;
  object_class->set_property = gst_fake_video_sink_set_property;

  gst_element_class_add_static_pad_template (element_class, &sink_factory);
  gst_element_class_set_static_metadata (element_class, "Fake Video Sink",
      "Video/Sink", "Fake video display that allows zero-copy",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  /**
   * GstFakeVideoSink:allocation-meta-flags
   *
   * Control the behaviour of the sink allocation query handler.
   *
   * Since: 1.18
   */
  g_object_class_install_property (object_class, PROP_ALLOCATION_META_FLAGS,
      g_param_spec_flags ("allocation-meta-flags", "Flags",
          "Flags to control behaviour",
          GST_TYPE_FAKE_VIDEO_SINK_ALLOCATION_META_FLAGS,
          ALLOCATION_META_DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstFakeVideoSink::handoff:
   * @fakevideosink: the fakevideosink instance
   * @buffer: the buffer that just has been received
   * @pad: the pad that received it
   *
   * This signal gets emitted before unreffing the buffer.
   *
   * Since: 1.22
   */
  gst_fake_video_sink_signals[SIGNAL_HANDOFF] =
      g_signal_new ("handoff", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstFakeVideoSinkClass, handoff), NULL, NULL,
      NULL, G_TYPE_NONE, 2, GST_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE,
      GST_TYPE_PAD);

  /**
   * GstFakeVideoSink::preroll-handoff:
   * @fakevideosink: the fakevideosink instance
   * @buffer: the buffer that just has been received
   * @pad: the pad that received it
   *
   * This signal gets emitted before unreffing the buffer.
   *
   * Since: 1.22
   */
  gst_fake_video_sink_signals[SIGNAL_PREROLL_HANDOFF] =
      g_signal_new ("preroll-handoff", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstFakeVideoSinkClass,
          preroll_handoff), NULL, NULL, NULL, G_TYPE_NONE, 2,
      GST_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE, GST_TYPE_PAD);

  g_object_class_install_property (object_class, PROP_STATE_ERROR,
      g_param_spec_enum ("state-error", "State Error",
          "Generate a state change error", GST_TYPE_FAKE_VIDEO_SINK_STATE_ERROR,
          DEFAULT_STATE_ERROR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  pspec_last_message = g_param_spec_string ("last-message", "Last Message",
      "The message describing current status", DEFAULT_LAST_MESSAGE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LAST_MESSAGE,
      pspec_last_message);
  g_object_class_install_property (object_class, PROP_SIGNAL_HANDOFFS,
      g_param_spec_boolean ("signal-handoffs", "Signal handoffs",
          "Send a signal before unreffing the buffer", DEFAULT_SIGNAL_HANDOFFS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DROP_OUT_OF_SEGMENT,
      g_param_spec_boolean ("drop-out-of-segment",
          "Drop out-of-segment buffers",
          "Drop and don't render / hand off out-of-segment buffers",
          DEFAULT_DROP_OUT_OF_SEGMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent",
          "Don't produce last_message events", DEFAULT_SILENT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DUMP,
      g_param_spec_boolean ("dump", "Dump", "Dump buffer contents to stdout",
          DEFAULT_DUMP,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_ACTIVATE_PUSH,
      g_param_spec_boolean ("can-activate-push", "Can activate push",
          "Can activate in push mode", DEFAULT_CAN_ACTIVATE_PUSH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_ACTIVATE_PULL,
      g_param_spec_boolean ("can-activate-pull", "Can activate pull",
          "Can activate in pull mode", DEFAULT_CAN_ACTIVATE_PULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NUM_BUFFERS,
      g_param_spec_int ("num-buffers", "num-buffers",
          "Number of buffers to accept going EOS", -1, G_MAXINT,
          DEFAULT_NUM_BUFFERS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  base_sink_class = g_type_class_ref (GST_TYPE_BASE_SINK);
  gst_util_proxy_class_properties (object_class, base_sink_class, PROP_LAST);
  g_type_class_unref (base_sink_class);

  gst_type_mark_as_plugin_api (GST_TYPE_FAKE_VIDEO_SINK_ALLOCATION_META_FLAGS,
      0);
  gst_type_mark_as_plugin_api (GST_TYPE_FAKE_VIDEO_SINK_STATE_ERROR, 0);
}
