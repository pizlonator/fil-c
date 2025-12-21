/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/videooverlay.h>
#include <gst/video/navigation.h>
#include <gst/controller/gstproxycontrolbinding.h>

#include "gstglelements.h"
#include "gstglsinkbin.h"

GST_DEBUG_CATEGORY (gst_debug_gl_sink_bin);
#define GST_CAT_DEFAULT gst_debug_gl_sink_bin

static void gst_gl_sink_bin_finalize (GObject * object);
static void gst_gl_sink_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_gl_sink_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static GstStateChangeReturn gst_gl_sink_bin_change_state (GstElement * element,
    GstStateChange transition);

static void gst_gl_sink_bin_video_overlay_init (gpointer g_iface,
    gpointer g_iface_data);
static void gst_gl_sink_bin_navigation_interface_init (gpointer g_iface,
    gpointer g_iface_data);
static void gst_gl_sink_bin_color_balance_init (gpointer g_iface,
    gpointer g_iface_data);

#define DEFAULT_SYNC                TRUE
#define DEFAULT_MAX_LATENESS        -1
#define DEFAULT_QOS                 FALSE
#define DEFAULT_ASYNC               TRUE
#define DEFAULT_TS_OFFSET           0
#define DEFAULT_BLOCKSIZE           4096
#define DEFAULT_RENDER_DELAY        0
#define DEFAULT_ENABLE_LAST_SAMPLE  TRUE
#define DEFAULT_THROTTLE_TIME       0
#define DEFAULT_MAX_BITRATE         0

/* GstGLColorBalance properties */
#define DEFAULT_PROP_CONTRAST       1.0
#define DEFAULT_PROP_BRIGHTNESS	    0.0
#define DEFAULT_PROP_HUE            0.0
#define DEFAULT_PROP_SATURATION	    1.0

enum
{
  PROP_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_SINK,
  PROP_SYNC,
  PROP_MAX_LATENESS,
  PROP_QOS,
  PROP_ASYNC,
  PROP_TS_OFFSET,
  PROP_ENABLE_LAST_SAMPLE,
  PROP_LAST_SAMPLE,
  PROP_BLOCKSIZE,
  PROP_RENDER_DELAY,
  PROP_THROTTLE_TIME,
  PROP_MAX_BITRATE,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION,
};

enum
{
  SIGNAL_0,
  SIGNAL_CREATE_ELEMENT,
  SIGNAL_LAST,
};

static guint gst_gl_sink_bin_signals[SIGNAL_LAST] = { 0, };

#define gst_gl_sink_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLSinkBin, gst_gl_sink_bin,
    GST_TYPE_BIN, G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_gl_sink_bin_video_overlay_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_gl_sink_bin_navigation_interface_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_gl_sink_bin_color_balance_init)
    GST_DEBUG_CATEGORY_INIT (gst_debug_gl_sink_bin, "glimagesink", 0,
        "OpenGL Video Sink Bin"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glsinkbin, "glsinkbin",
    GST_RANK_NONE, GST_TYPE_GL_SINK_BIN, gl_element_init (plugin));

static void
gst_gl_sink_bin_class_init (GstGLSinkBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstCaps *upload_caps;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  element_class->change_state = gst_gl_sink_bin_change_state;

  gobject_class->set_property = gst_gl_sink_bin_set_property;
  gobject_class->get_property = gst_gl_sink_bin_get_property;
  gobject_class->finalize = gst_gl_sink_bin_finalize;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINK,
      g_param_spec_object ("sink",
          "GL sink element",
          "The GL sink chain to use",
          GST_TYPE_ELEMENT,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  /* base sink */
  g_object_class_install_property (gobject_class, PROP_SYNC,
      g_param_spec_boolean ("sync", "Sync", "Sync on the clock", DEFAULT_SYNC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_LATENESS,
      g_param_spec_int64 ("max-lateness", "Max Lateness",
          "Maximum number of nanoseconds that a buffer can be late before it "
          "is dropped (-1 unlimited)", -1, G_MAXINT64, DEFAULT_MAX_LATENESS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QOS,
      g_param_spec_boolean ("qos", "Qos",
          "Generate Quality-of-Service events upstream", DEFAULT_QOS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ASYNC,
      g_param_spec_boolean ("async", "Async",
          "Go asynchronously to PAUSED", DEFAULT_ASYNC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TS_OFFSET,
      g_param_spec_int64 ("ts-offset", "TS Offset",
          "Timestamp offset in nanoseconds", G_MININT64, G_MAXINT64,
          DEFAULT_TS_OFFSET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ENABLE_LAST_SAMPLE,
      g_param_spec_boolean ("enable-last-sample", "Enable Last Buffer",
          "Enable the last-sample property", DEFAULT_ENABLE_LAST_SAMPLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LAST_SAMPLE,
      g_param_spec_boxed ("last-sample", "Last Sample",
          "The last sample received in the sink", GST_TYPE_SAMPLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BLOCKSIZE,
      g_param_spec_uint ("blocksize", "Block size",
          "Size in bytes to pull per buffer (0 = default)", 0, G_MAXUINT,
          DEFAULT_BLOCKSIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RENDER_DELAY,
      g_param_spec_uint64 ("render-delay", "Render Delay",
          "Additional render delay of the sink in nanoseconds", 0, G_MAXUINT64,
          DEFAULT_RENDER_DELAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_THROTTLE_TIME,
      g_param_spec_uint64 ("throttle-time", "Throttle time",
          "The time to keep between rendered buffers (0 = disabled)", 0,
          G_MAXUINT64, DEFAULT_THROTTLE_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint64 ("max-bitrate", "Max Bitrate",
          "The maximum bits per second to render (0 = disabled)", 0,
          G_MAXUINT64, DEFAULT_MAX_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* colorbalance */
  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_double ("contrast", "Contrast", "contrast",
          0.0, 2.0, DEFAULT_PROP_CONTRAST,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_double ("brightness", "Brightness", "brightness", -1.0, 1.0,
          DEFAULT_PROP_BRIGHTNESS,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_double ("hue", "Hue", "hue", -1.0, 1.0, DEFAULT_PROP_HUE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_double ("saturation", "Saturation", "saturation", 0.0, 2.0,
          DEFAULT_PROP_SATURATION,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLSinkBin::create-element:
   * @object: the #glsinkbin
   *
   * Will be emitted when we need the processing element/s that this bin will use
   *
   * Returns: a new #GstElement
   */
  gst_gl_sink_bin_signals[SIGNAL_CREATE_ELEMENT] =
      g_signal_new ("create-element", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_ELEMENT, 0);

  gst_element_class_set_metadata (element_class,
      "GL Sink Bin", "Sink/Video",
      "Infrastructure to process GL textures",
      "Matthew Waters <matthew@centricular.com>");

  upload_caps = gst_gl_upload_get_input_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, upload_caps));
  gst_caps_unref (upload_caps);
}

static void
gst_gl_sink_bin_init (GstGLSinkBin * self)
{
  gboolean res = TRUE;
  GstPad *pad;

  self->upload = gst_element_factory_make ("glupload", NULL);
  self->convert = gst_element_factory_make ("glcolorconvert", NULL);
  self->balance = gst_element_factory_make ("glcolorbalance", NULL);

  res &= gst_bin_add (GST_BIN (self), self->upload);
  res &= gst_bin_add (GST_BIN (self), self->convert);
  res &= gst_bin_add (GST_BIN (self), self->balance);

  res &= gst_element_link_pads (self->upload, "src", self->convert, "sink");
  res &= gst_element_link_pads (self->convert, "src", self->balance, "sink");

  pad = gst_element_get_static_pad (self->upload, "sink");
  if (!pad) {
    res = FALSE;
  } else {
    GST_DEBUG_OBJECT (self, "setting target sink pad %" GST_PTR_FORMAT, pad);
    self->sinkpad = gst_ghost_pad_new ("sink", pad);
    gst_element_add_pad (GST_ELEMENT_CAST (self), self->sinkpad);
    gst_object_unref (pad);
  }

#define ADD_BINDING(obj,ref,prop) \
    gst_object_add_control_binding (GST_OBJECT (obj), \
        gst_proxy_control_binding_new (GST_OBJECT (obj), prop, \
            GST_OBJECT (ref), prop));
  ADD_BINDING (self->balance, self, "contrast");
  ADD_BINDING (self->balance, self, "brightness");
  ADD_BINDING (self->balance, self, "hue");
  ADD_BINDING (self->balance, self, "saturation");
#undef ADD_BINDING

  if (!res) {
    GST_WARNING_OBJECT (self, "Failed to add/connect the necessary machinery");
  }

  gst_type_mark_as_plugin_api (GST_TYPE_GL_SINK_BIN, 0);
}

static void
gst_gl_sink_bin_finalize (GObject * object)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (object);

  if (self->sink)
    gst_object_unref (self->sink);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
_connect_sink_element (GstGLSinkBin * self)
{
  gst_object_set_name (GST_OBJECT (self->sink), "sink");

  if (gst_bin_add (GST_BIN (self), self->sink) &&
      gst_element_link_pads (self->balance, "src", self->sink, "sink"))
    return TRUE;

  GST_ERROR_OBJECT (self, "Failed to link sink element into the pipeline");
  return FALSE;
}

/*
 * @sink: (transfer full):
 */
static gboolean
gst_gl_sink_bin_set_sink (GstGLSinkBin * self, GstElement * sink)
{
  g_return_val_if_fail (GST_IS_ELEMENT (sink), FALSE);

  if (self->sink) {
    gst_element_set_locked_state (self->sink, TRUE);
    gst_bin_remove (GST_BIN (self), self->sink);
    gst_element_set_state (self->sink, GST_STATE_NULL);
    gst_object_unref (self->sink);
    self->sink = NULL;
  }
  self->sink = sink;

  gst_object_ref_sink (sink);

  if (sink && !_connect_sink_element (self)) {
    gst_object_unref (self->sink);
    self->sink = NULL;
    return FALSE;
  }

  return TRUE;
}

void
gst_gl_sink_bin_finish_init_with_element (GstGLSinkBin * self,
    GstElement * element)
{
  gst_gl_sink_bin_set_sink (self, element);
}

void
gst_gl_sink_bin_finish_init (GstGLSinkBin * self)
{
  GstGLSinkBinClass *klass = GST_GL_SINK_BIN_GET_CLASS (self);
  GstElement *element = NULL;

  if (klass->create_element)
    element = klass->create_element ();

  if (element)
    gst_gl_sink_bin_finish_init_with_element (self, element);
}

static void
gst_gl_sink_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (object);
  GParamSpec *sink_pspec;

  switch (prop_id) {
    case PROP_SINK:
      gst_gl_sink_bin_set_sink (self, g_value_get_object (value));
      break;
    case PROP_CONTRAST:
    case PROP_BRIGHTNESS:
    case PROP_HUE:
    case PROP_SATURATION:
      if (self->balance)
        g_object_set_property (G_OBJECT (self->balance), pspec->name, value);
      break;
    default:
      if (self->sink) {
        sink_pspec =
            g_object_class_find_property (G_OBJECT_GET_CLASS (self->sink),
            pspec->name);
        if (sink_pspec
            && G_PARAM_SPEC_TYPE (sink_pspec) == G_PARAM_SPEC_TYPE (pspec)) {
          g_object_set_property (G_OBJECT (self->sink), pspec->name, value);
        } else {
          GST_INFO ("Failed to set unmatched property %s", pspec->name);
        }
      }
      break;
  }
}

static void
gst_gl_sink_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (object);
  GParamSpec *sink_pspec;

  switch (prop_id) {
    case PROP_SINK:
      g_value_set_object (value, self->sink);
      break;
    case PROP_CONTRAST:
    case PROP_BRIGHTNESS:
    case PROP_HUE:
    case PROP_SATURATION:
      if (self->balance)
        g_object_get_property (G_OBJECT (self->balance), pspec->name, value);
      break;
    default:
      if (self->sink) {
        sink_pspec =
            g_object_class_find_property (G_OBJECT_GET_CLASS (self->sink),
            pspec->name);
        if (sink_pspec
            && G_PARAM_SPEC_TYPE (sink_pspec) == G_PARAM_SPEC_TYPE (pspec)) {
          g_object_get_property (G_OBJECT (self->sink), pspec->name, value);
        } else {
          GST_INFO ("Failed to get unmatched property %s", pspec->name);
        }
      }
      break;
  }
}

static GstStateChangeReturn
gst_gl_sink_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (element);
  GstGLSinkBinClass *klass = GST_GL_SINK_BIN_GET_CLASS (self);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!self->sink) {
        if (klass->create_element)
          self->sink = klass->create_element ();

        if (!self->sink) {
          g_signal_emit (element,
              gst_gl_sink_bin_signals[SIGNAL_CREATE_ELEMENT], 0, &self->sink);
          if (self->sink && g_object_is_floating (self->sink))
            gst_object_ref_sink (self->sink);
        }

        if (!self->sink) {
          GST_ERROR_OBJECT (element, "Failed to retrieve element");
          return GST_STATE_CHANGE_FAILURE;
        }
        if (!_connect_sink_element (self))
          return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    default:
      break;
  }

  return ret;
}

static void
gst_gl_sink_bin_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (navigation);
  GstElement *nav =
      gst_bin_get_by_interface (GST_BIN (self), GST_TYPE_NAVIGATION);

  if (nav) {
    gst_navigation_send_event_simple (GST_NAVIGATION (nav), event);
    gst_object_unref (nav);
  } else {
    gst_element_send_event (GST_ELEMENT (self), event);
  }
}

static void
gst_gl_sink_bin_navigation_interface_init (gpointer g_iface,
    gpointer g_iface_data)
{
  GstNavigationInterface *iface = (GstNavigationInterface *) g_iface;
  iface->send_event_simple = gst_gl_sink_bin_navigation_send_event;
}

static void
gst_gl_sink_bin_overlay_expose (GstVideoOverlay * overlay)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (overlay);
  GstVideoOverlay *overlay_element = NULL;

  overlay_element =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_VIDEO_OVERLAY));

  if (overlay_element) {
    gst_video_overlay_expose (overlay_element);
    gst_object_unref (overlay_element);
  }
}

static void
gst_gl_sink_bin_overlay_handle_events (GstVideoOverlay * overlay,
    gboolean handle_events)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (overlay);
  GstVideoOverlay *overlay_element = NULL;

  overlay_element =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_VIDEO_OVERLAY));

  if (overlay_element) {
    gst_video_overlay_handle_events (overlay_element, handle_events);
    gst_object_unref (overlay_element);
  }
}

static void
gst_gl_sink_bin_overlay_set_render_rectangle (GstVideoOverlay * overlay, gint x,
    gint y, gint width, gint height)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (overlay);
  GstVideoOverlay *overlay_element = NULL;

  overlay_element =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_VIDEO_OVERLAY));

  if (overlay_element) {
    gst_video_overlay_set_render_rectangle (overlay_element, x, y, width,
        height);
    gst_object_unref (overlay_element);
  }
}

static void
gst_gl_sink_bin_overlay_set_window_handle (GstVideoOverlay * overlay,
    guintptr handle)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (overlay);
  GstVideoOverlay *overlay_element = NULL;

  overlay_element =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_VIDEO_OVERLAY));

  if (overlay_element) {
    gst_video_overlay_set_window_handle (overlay_element, handle);
    gst_object_unref (overlay_element);
  }
}

static void
gst_gl_sink_bin_video_overlay_init (gpointer g_iface, gpointer g_iface_data)
{
  GstVideoOverlayInterface *iface = (GstVideoOverlayInterface *) g_iface;
  iface->expose = gst_gl_sink_bin_overlay_expose;
  iface->handle_events = gst_gl_sink_bin_overlay_handle_events;
  iface->set_render_rectangle = gst_gl_sink_bin_overlay_set_render_rectangle;
  iface->set_window_handle = gst_gl_sink_bin_overlay_set_window_handle;
}

static const GList *
gst_gl_sink_bin_color_balance_list_channels (GstColorBalance * balance)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (balance);
  GstColorBalance *balance_element = NULL;
  const GList *list = NULL;

  balance_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_COLOR_BALANCE));

  if (balance_element) {
    list = gst_color_balance_list_channels (balance_element);
    gst_object_unref (balance_element);
  }

  return list;
}

static void
gst_gl_sink_bin_color_balance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (balance);
  GstColorBalance *balance_element = NULL;

  balance_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_COLOR_BALANCE));

  if (balance_element) {
    gst_color_balance_set_value (balance_element, channel, value);
    gst_object_unref (balance_element);
  }
}

static gint
gst_gl_sink_bin_color_balance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (balance);
  GstColorBalance *balance_element = NULL;
  gint val = 0;

  balance_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_COLOR_BALANCE));

  if (balance_element) {
    val = gst_color_balance_get_value (balance_element, channel);
    gst_object_unref (balance_element);
  }

  return val;
}

static GstColorBalanceType
gst_gl_sink_bin_color_balance_get_balance_type (GstColorBalance * balance)
{
  GstGLSinkBin *self = GST_GL_SINK_BIN (balance);
  GstColorBalance *balance_element = NULL;
  GstColorBalanceType type = 0;

  balance_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (self),
          GST_TYPE_COLOR_BALANCE));

  if (balance_element) {
    type = gst_color_balance_get_balance_type (balance_element);
    gst_object_unref (balance_element);
  }

  return type;
}

static void
gst_gl_sink_bin_color_balance_init (gpointer g_iface, gpointer g_iface_data)
{
  GstColorBalanceInterface *iface = (GstColorBalanceInterface *) g_iface;

  iface->list_channels = gst_gl_sink_bin_color_balance_list_channels;
  iface->set_value = gst_gl_sink_bin_color_balance_set_value;
  iface->get_value = gst_gl_sink_bin_color_balance_get_value;
  iface->get_balance_type = gst_gl_sink_bin_color_balance_get_balance_type;
}
