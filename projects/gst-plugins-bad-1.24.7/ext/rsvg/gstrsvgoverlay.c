/* GStreamer
 * Copyright (C) 2010 Olivier Aubert <olivier.aubert@liris.cnrs.fr>
 * Copyright (C) 2013 Collabora Ltda
 *  Author: Luciana Fujii Pontello <luciana.fujii@collabora.com>
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
 * SECTION:element-rsvgoverlay
 * @title: rsvgoverlay
 *
 * This elements overlays SVG graphics over the video. SVG data can
 * either be specified through properties, or fed through the
 * data-sink pad.
 *
 * Position and dimension of the SVG graphics can be achieved by
 * specifying appropriate dimensions in the SVG file itself, but
 * shortcuts are provided by the element to specify x/y position and
 * width/height dimension, both in absolute form (pixels) and in
 * relative form (percentage of video dimension).
 *
 * For any measure (x/y/width/height), the absolute value (in pixels)
 * takes precedence over the relative one if both are
 * specified. Absolute values must be set to 0 to disable them.
 *
 * If all parameters are 0, the image is displayed without rescaling
 * at (0, 0) position.
 *
 * The fit-to-frame property is a shortcut for displaying the SVG
 * overlay at (0, 0) position filling the whole screen. It modifies
 * the values of the x/y/width/height attributes, by setting
 * height-/width-relative to 1.0. and all other attributes to 0.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v videotestsrc ! videoconvert ! rsvgoverlay location=foo.svg ! videoconvert ! autovideosink
 * ]| specifies the SVG location through the filename property.
 * |[
 * gst-launch-1.0 -v videotestsrc ! videoconvert ! rsvgoverlay name=overlay ! videoconvert ! autovideosink filesrc location=foo.svg ! image/svg ! overlay.data_sink
 * ]| does the same by feeding data through the data_sink pad. You can also specify the SVG data itself as parameter:
 * |[
 * gst-launch-1.0 -v videotestsrc ! videoconvert ! rsvgoverlay data='<svg viewBox="0 0 800 600"><image x="80%" y="80%" width="10%" height="10%" xlink:href="foo.jpg" /></svg>' ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstrsvgoverlay.h"
#include <string.h>

#include <gst/video/video.h>

#include <librsvg/rsvg.h>

enum
{
  PROP_0,
  PROP_DATA,
  PROP_LOCATION,
  PROP_FIT_TO_FRAME,
  PROP_X,
  PROP_Y,
  PROP_X_RELATIVE,
  PROP_Y_RELATIVE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_WIDTH_RELATIVE,
  PROP_HEIGHT_RELATIVE
};

GST_DEBUG_CATEGORY_STATIC (rsvg_overlay_debug);
#define GST_CAT_DEFAULT rsvg_overlay_debug

#define GST_RSVG_LOCK(overlay) G_STMT_START { \
  GST_LOG_OBJECT (overlay, "Locking rsvgoverlay from thread %p", g_thread_self ()); \
  g_mutex_lock (&overlay->rsvg_lock); \
  GST_LOG_OBJECT (overlay, "Locked rsvgoverlay from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_RSVG_UNLOCK(overlay) G_STMT_START { \
  GST_LOG_OBJECT (overlay, "Unlocking rsvgoverlay from thread %p", g_thread_self ()); \
  g_mutex_unlock (&overlay->rsvg_lock); \
} G_STMT_END

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GST_RSVG_VIDEO_CAPS GST_VIDEO_CAPS_MAKE ("BGRA")
#else
#define GST_RSVG_VIDEO_CAPS GST_VIDEO_CAPS_MAKE ("ARGB")
#endif

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_RSVG_VIDEO_CAPS)
    );

static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_RSVG_VIDEO_CAPS)
    );

static GstStaticPadTemplate data_sink_template =
    GST_STATIC_PAD_TEMPLATE ("data_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/svg+xml; image/svg; text/plain"));

#define gst_rsv_overlay_parent_class parent_class

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (rsvg_overlay_debug, "rsvgoverlay", 0, "SVG Overlay");

G_DEFINE_TYPE_WITH_CODE (GstRsvgOverlay, gst_rsvg_overlay,
    GST_TYPE_VIDEO_FILTER, _do_init);
GST_ELEMENT_REGISTER_DEFINE (rsvgoverlay, "rsvgoverlay", GST_RANK_NONE,
    GST_TYPE_RSVG_OVERLAY);

static void gst_rsvg_overlay_finalize (GObject * object);

static void
gst_rsvg_overlay_set_svg_data (GstRsvgOverlay * overlay, const gchar * data,
    gboolean consider_as_filename)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM (overlay);
  gsize size;
  GError *error = NULL;

  if (overlay->handle) {
    g_object_unref (overlay->handle);
    overlay->handle = NULL;
    gst_base_transform_set_passthrough (btrans, TRUE);
  }

  /* data may be NULL */
  if (data) {
    size = strlen (data);
    if (size) {
      /* Read data either from string or from file */
      if (consider_as_filename)
        overlay->handle = rsvg_handle_new_from_file (data, &error);
      else
        overlay->handle =
            rsvg_handle_new_from_data ((guint8 *) data, size, &error);
      if (error || overlay->handle == NULL) {
        if (error) {
          GST_ERROR_OBJECT (overlay, "Cannot read SVG data: %s\n%s",
              error->message, data);
          g_error_free (error);
        } else {
          GST_ERROR_OBJECT (overlay, "Cannot read SVG data: %s", data);
        }
      } else {
        /* Get SVG dimension. */
        RsvgDimensionData svg_dimension;
        rsvg_handle_get_dimensions (overlay->handle, &svg_dimension);
        overlay->svg_width = svg_dimension.width;
        overlay->svg_height = svg_dimension.height;
        gst_base_transform_set_passthrough (btrans, FALSE);
        GST_INFO_OBJECT (overlay, "updated SVG, %d x %d", overlay->svg_width,
            overlay->svg_height);
      }
    }
  }
}

static void
gst_rsvg_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (object);

  GST_RSVG_LOCK (overlay);

  switch (prop_id) {
    case PROP_DATA:
    {
      gst_rsvg_overlay_set_svg_data (overlay, g_value_get_string (value),
          FALSE);
      break;
    }
    case PROP_LOCATION:
    {
      gst_rsvg_overlay_set_svg_data (overlay, g_value_get_string (value), TRUE);
      break;
    }
    case PROP_FIT_TO_FRAME:
    {
      if (g_value_get_boolean (value)) {
        overlay->x_offset = 0;
        overlay->y_offset = 0;
        overlay->x_relative = 0.0;
        overlay->y_relative = 0.0;
        overlay->width = 0;
        overlay->height = 0;
        overlay->width_relative = 1.0;
        overlay->height_relative = 1.0;
      } else {
        overlay->width_relative = 0;
        overlay->height_relative = 0;
      }
      break;
    }
    case PROP_X:
    {
      overlay->x_offset = g_value_get_int (value);
      break;
    }
    case PROP_Y:
    {
      overlay->y_offset = g_value_get_int (value);
      break;
    }
    case PROP_X_RELATIVE:
    {
      overlay->x_relative = g_value_get_float (value);
      break;
    }
    case PROP_Y_RELATIVE:
    {
      overlay->y_relative = g_value_get_float (value);
      break;
    }

    case PROP_WIDTH:
    {
      overlay->width = g_value_get_int (value);
      break;
    }
    case PROP_HEIGHT:
    {
      overlay->height = g_value_get_int (value);
      break;
    }
    case PROP_WIDTH_RELATIVE:
    {
      overlay->width_relative = g_value_get_float (value);
      break;
    }
    case PROP_HEIGHT_RELATIVE:
    {
      overlay->height_relative = g_value_get_float (value);
      break;
    }

    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }

  GST_RSVG_UNLOCK (overlay);
}

static void
gst_rsvg_overlay_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (object);

  switch (prop_id) {
    case PROP_X:
      g_value_set_int (value, overlay->x_offset);
      break;
    case PROP_Y:
      g_value_set_int (value, overlay->y_offset);
      break;
    case PROP_X_RELATIVE:
      g_value_set_float (value, overlay->x_relative);
      break;
    case PROP_Y_RELATIVE:
      g_value_set_float (value, overlay->y_relative);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, overlay->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, overlay->height);
      break;
    case PROP_WIDTH_RELATIVE:
      g_value_set_float (value, overlay->width_relative);
      break;
    case PROP_HEIGHT_RELATIVE:
      g_value_set_float (value, overlay->height_relative);
      break;

    case PROP_FIT_TO_FRAME:
      g_value_set_boolean (value, (overlay->width_relative == 1.0
              && overlay->height_relative == 1.0));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_rsvg_overlay_data_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (GST_PAD_PARENT (pad));

  gst_adapter_push (overlay->adapter, buffer);
  return GST_FLOW_OK;
}

static gboolean
gst_rsvg_overlay_data_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (GST_PAD_PARENT (pad));

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_EOS:{
      guint data_size;

      GST_RSVG_LOCK (overlay);
      /* FIXME: rsvgdec looks for </svg> in data to determine the end
         of SVG code. Should we really do the same here? IOW, could
         data be sent and _pushed() before the EOS gets processed? */
      data_size = gst_adapter_available (overlay->adapter);
      if (data_size) {
        gst_rsvg_overlay_set_svg_data (overlay,
            (const gchar *) gst_adapter_take (overlay->adapter, data_size),
            FALSE);
        gst_adapter_clear (overlay->adapter);
      }
      GST_RSVG_UNLOCK (overlay);
    }
      break;

    case GST_EVENT_FLUSH_STOP:
      gst_adapter_clear (overlay->adapter);
      break;

    default:
      break;
  }

  /* Dropping all events here */
  gst_event_unref (event);
  return TRUE;
}

static GstFlowReturn
gst_rsvg_overlay_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (vfilter);
  cairo_surface_t *surface;
  cairo_t *cr;
  double applied_x_offset = (double) overlay->x_offset;
  double applied_y_offset = (double) overlay->y_offset;
  int applied_width = overlay->width;
  int applied_height = overlay->height;

  GST_RSVG_LOCK (overlay);
  if (!overlay->handle) {
    GST_RSVG_UNLOCK (overlay);
    return GST_FLOW_OK;
  }

  surface =
      cairo_image_surface_create_for_data (GST_VIDEO_FRAME_PLANE_DATA (frame,
          0), CAIRO_FORMAT_ARGB32, GST_VIDEO_FRAME_WIDTH (frame),
      GST_VIDEO_FRAME_HEIGHT (frame), GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0));
  if (G_UNLIKELY (!surface))
    return GST_FLOW_ERROR;

  cr = cairo_create (surface);
  if (G_UNLIKELY (!cr)) {
    cairo_surface_destroy (surface);
    return GST_FLOW_ERROR;
  }

  /* Compute relative dimensions if absolute dimensions are not set */
  if (!applied_x_offset && overlay->x_relative) {
    applied_x_offset = overlay->x_relative * GST_VIDEO_FRAME_WIDTH (frame);
  }
  if (!applied_y_offset && overlay->y_relative) {
    applied_y_offset = overlay->y_relative * GST_VIDEO_FRAME_HEIGHT (frame);
  }
  if (!applied_width && overlay->width_relative) {
    applied_width =
        (int) (overlay->width_relative * GST_VIDEO_FRAME_WIDTH (frame));
  }
  if (!applied_height && overlay->height_relative) {
    applied_height =
        (int) (overlay->height_relative * GST_VIDEO_FRAME_HEIGHT (frame));
  }

  if (applied_x_offset || applied_y_offset) {
    cairo_translate (cr, applied_x_offset, applied_y_offset);
  }

  /* Scale when necessary, i.e. an absolute or relative dimension has been specified. */
  if ((applied_width || applied_height) && overlay->svg_width
      && overlay->svg_height) {
    /* If may happen that only one of the dimension is specified. Use
       the original SVG size for the other dimension. */
    if (!applied_width)
      applied_width = overlay->svg_width;
    if (!applied_height)
      applied_height = overlay->svg_height;

    cairo_scale (cr, (double) applied_width / overlay->svg_width,
        (double) applied_height / overlay->svg_height);
  }
  rsvg_handle_render_cairo (overlay->handle, cr);
  GST_RSVG_UNLOCK (overlay);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return GST_FLOW_OK;
}

static gboolean
gst_rsvg_overlay_stop (GstBaseTransform * btrans)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (btrans);

  if (overlay->handle) {
    g_object_unref (overlay->handle);
    overlay->handle = NULL;
  }

  gst_adapter_clear (overlay->adapter);

  return TRUE;
}

static void
gst_rsvg_overlay_class_init (GstRsvgOverlayClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *videofilter_class = GST_VIDEO_FILTER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class,
      &video_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &data_sink_template);

  gst_element_class_set_static_metadata (element_class, "RSVG overlay",
      "Filter/Editor/Video",
      "Overlays SVG graphics over a video stream",
      "Olivier Aubert <olivier.aubert@liris.cnrs.fr>");

  gobject_class->set_property = gst_rsvg_overlay_set_property;
  gobject_class->get_property = gst_rsvg_overlay_get_property;
  gobject_class->finalize = gst_rsvg_overlay_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DATA,
      g_param_spec_string ("data", "data", "SVG data.", "",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LOCATION,
      g_param_spec_string ("location", "location", "SVG file location.", "",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FIT_TO_FRAME,
      g_param_spec_boolean ("fit-to-frame", "fit to frame",
          "Fit the SVG to fill the whole frame.", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_X,
      g_param_spec_int ("x", "x offset",
          "Specify an x offset.", -G_MAXINT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_Y,
      g_param_spec_int ("y", "y offset",
          "Specify a y offset.", -G_MAXINT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_X_RELATIVE,
      g_param_spec_float ("x-relative", "x relative offset",
          "Specify an x offset relative to the display size.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_Y_RELATIVE,
      g_param_spec_float ("y-relative", "y relative offset",
          "Specify a y offset relative to the display size.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WIDTH,
      g_param_spec_int ("width", "width",
          "Specify a width in pixels.", -G_MAXINT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HEIGHT,
      g_param_spec_int ("height", "height",
          "Specify a height in pixels.", -G_MAXINT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WIDTH_RELATIVE,
      g_param_spec_float ("width-relative", "relative width",
          "Specify a width relative to the display size.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HEIGHT_RELATIVE,
      g_param_spec_float ("height-relative", "relative height",
          "Specify a height relative to the display size.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  videofilter_class->transform_frame_ip = gst_rsvg_overlay_transform_frame_ip;
  basetransform_class->stop = gst_rsvg_overlay_stop;
  basetransform_class->passthrough_on_same_caps = FALSE;
}

static void
gst_rsvg_overlay_init (GstRsvgOverlay * overlay)
{
  overlay->x_offset = 0;
  overlay->y_offset = 0;
  overlay->x_relative = 0.0;
  overlay->y_relative = 0.0;
  overlay->width = 0;
  overlay->height = 0;
  overlay->width_relative = 0.0;
  overlay->height_relative = 0.0;

  overlay->adapter = gst_adapter_new ();

  /* data sink */
  overlay->data_sinkpad =
      gst_pad_new_from_static_template (&data_sink_template, "data_sink");
  gst_pad_set_chain_function (overlay->data_sinkpad,
      GST_DEBUG_FUNCPTR (gst_rsvg_overlay_data_sink_chain));
  gst_pad_set_event_function (overlay->data_sinkpad,
      GST_DEBUG_FUNCPTR (gst_rsvg_overlay_data_sink_event));
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->data_sinkpad);
}

static void
gst_rsvg_overlay_finalize (GObject * object)
{
  GstRsvgOverlay *overlay = GST_RSVG_OVERLAY (object);

  g_object_unref (overlay->adapter);

  G_OBJECT_CLASS (gst_rsvg_overlay_parent_class)->finalize (object);
}
