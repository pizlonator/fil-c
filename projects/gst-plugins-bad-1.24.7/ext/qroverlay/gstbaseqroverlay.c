/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (c) 2020 Anthony Violo <anthony.violo@ubicast.eu>
 * Copyright (c) 2020 Thibault Saunier <tsaunier@igalia.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <json-glib/json-glib.h>

#include <qrencode.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gstbaseqroverlay.h"

GST_DEBUG_CATEGORY_STATIC (gst_base_qr_overlay_debug);
#define GST_CAT_DEFAULT gst_base_qr_overlay_debug

enum
{
  PROP_0,
  PROP_X_AXIS,
  PROP_Y_AXIS,
  PROP_PIXEL_SIZE,
  PROP_QRCODE_ERROR_CORRECTION,
  PROP_CASE_SENSITIVE,
};

typedef struct _GstBaseQROverlayPrivate GstBaseQROverlayPrivate;
struct _GstBaseQROverlayPrivate
{
  gfloat qrcode_size;
  guint qrcode_quality;
  guint span_frame;
  QRecLevel level;
  gfloat x_percent;
  gfloat y_percent;
  GstElement *overlaycomposition;
  GstVideoInfo info;
  gboolean valid;
  gboolean case_sensitive;

  GstPad *sinkpad, *srcpad;
  GstVideoOverlayComposition *prev_overlay;
};

#define PRIV(s) gst_base_qr_overlay_get_instance_private (GST_BASE_QR_OVERLAY (s))

#define OVERLAY_COMPOSITION_CAPS GST_VIDEO_CAPS_MAKE (GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS)

#define ALL_CAPS OVERLAY_COMPOSITION_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ANY)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALL_CAPS)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALL_CAPS)
    );

#define DEFAULT_PROP_QUALITY    1
#define DEFAULT_PROP_PIXEL_SIZE    3
#define DEFAULT_PROP_CASE_SENSITIVE FALSE

#define GST_TYPE_QRCODE_QUALITY (gst_qrcode_quality_get_type())
static GType
gst_qrcode_quality_get_type (void)
{
  static GType qrcode_quality_type = 0;

  static const GEnumValue qrcode_quality[] = {
    {0, "Level L", "Approx 7%"},
    {1, "Level M", "Approx 15%"},
    {2, "Level Q", "Approx 25%"},
    {3, "Level H", "Approx 30%"},
    {0, NULL, NULL},
  };

  if (!qrcode_quality_type) {
    qrcode_quality_type =
        g_enum_register_static ("GstQrcodeOverlayCorrection", qrcode_quality);
  }
  return qrcode_quality_type;
}

#define gst_base_qr_overlay_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstBaseQROverlay, gst_base_qr_overlay,
    GST_TYPE_BIN);

static void gst_base_qr_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_base_qr_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_base_qr_overlay_caps_changed_cb (GstBaseQROverlay * self, GstCaps * caps,
    gint window_width, gint window_height, GstElement * overlay)
{
  GstBaseQROverlayPrivate *priv = PRIV (self);

  GST_DEBUG_OBJECT (self, "%" GST_PTR_FORMAT, caps);

  if (gst_video_info_from_caps (&priv->info, caps))
    priv->valid = TRUE;
  else
    priv->valid = FALSE;

  /* needs to redraw the overlay as its position depends of the video size */
  gst_mini_object_replace (((GstMiniObject **) & priv->prev_overlay), NULL);
}

static GstVideoOverlayComposition *
draw_overlay (GstBaseQROverlay * self, QRcode * qrcode)
{
  guint8 *qr_data, *pixels;
  gint stride, pstride, y, x, yy, square_size;
  gsize offset, line_offset;
  GstVideoInfo info;
  GstVideoOverlayRectangle *rect;
  GstVideoOverlayComposition *comp;
  GstBuffer *buf;
  GstBaseQROverlayPrivate *priv = PRIV (self);

  gst_video_info_init (&info);

  square_size = (qrcode->width + 4 * 2) * priv->qrcode_size;
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_ARGB, square_size,
      square_size);

  pixels = g_malloc ((size_t) info.size);
  stride = info.stride[0];
  pstride = info.finfo->pixel_stride[0];

  /* White background */
  for (y = 0; y < info.height; y++)
    memset (&pixels[y * stride], 0xff, stride);

  /* Draw the black QR code blocks with 4px white space around it
   * on top */
  line_offset = 4 * priv->qrcode_size * stride;
  qr_data = qrcode->data;
  for (y = 0; y < qrcode->width; y++) {
    for (x = 0; x < (qrcode->width); x++) {
      for (yy = 0; yy < priv->qrcode_size * pstride; yy += pstride) {
        if (!(*qr_data & 1))
          continue;

        offset =
            (((line_offset + (stride * (yy / pstride))) +
                x * priv->qrcode_size * pstride)) +
            (priv->qrcode_size * pstride) + (4 * priv->qrcode_size * pstride);

        for (gint i = 0; i < priv->qrcode_size * pstride; i += pstride) {
          pixels[offset + i] = 0x00;
          pixels[offset + i + 1] = 0x00;
          pixels[offset + i + 2] = 0x00;
        }
      }
      qr_data++;
    }
    line_offset += (stride * priv->qrcode_size);
  }

  buf = gst_buffer_new_wrapped (pixels, info.size);
  gst_buffer_add_video_meta (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, info.width, info.height);

  x = (int) (priv->info.width - square_size) * (priv->x_percent / 100);
  x = GST_ROUND_DOWN_2 (x);
  y = (int) (priv->info.height - square_size) * (priv->y_percent / 100);
  y = GST_ROUND_DOWN_4 (y);

  GST_DEBUG_OBJECT (self, "draw overlay at (%d,%d) size: %dx%d", x, y,
      info.width, info.height);

  rect = gst_video_overlay_rectangle_new_raw (buf, x, y,
      info.width, info.height, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  comp = gst_video_overlay_composition_new (rect);
  gst_video_overlay_rectangle_unref (rect);

  return comp;
}

static GstVideoOverlayComposition *
gst_base_qr_overlay_draw_cb (GstBaseQROverlay * self, GstSample * sample,
    GstElement * _)
{
  GstBaseQROverlayPrivate *priv = PRIV (self);
  QRcode *qrcode;
  gchar *content;
  gboolean reuse_previous = FALSE;
  GstVideoOverlayComposition *overlay = NULL;
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstSegment *segment = gst_sample_get_segment (sample);
  GstClockTime rtime = gst_segment_to_running_time (segment, GST_FORMAT_TIME,
      GST_BUFFER_PTS (buffer));

  if (!priv->valid) {
    GST_ERROR_OBJECT (self, "Trying to draw before negotiation?");

    return NULL;
  }

  if (GST_CLOCK_TIME_IS_VALID (rtime))
    gst_object_sync_values (GST_OBJECT (self), rtime);

  content =
      GST_BASE_QR_OVERLAY_GET_CLASS (self)->get_content (GST_BASE_QR_OVERLAY
      (self), buffer, &priv->info, &reuse_previous);
  if (reuse_previous && priv->prev_overlay) {
    overlay = gst_video_overlay_composition_ref (priv->prev_overlay);
  } else if (content) {
    GST_INFO_OBJECT (self, "String will be encoded : %s", content);
    qrcode =
        QRcode_encodeString (content, 0, priv->qrcode_quality, QR_MODE_8,
        priv->case_sensitive);

    if (qrcode) {
      GST_DEBUG_OBJECT (self, "String encoded");
      overlay = draw_overlay (GST_BASE_QR_OVERLAY (self), qrcode);
      gst_mini_object_replace (((GstMiniObject **) & priv->prev_overlay),
          (GstMiniObject *) overlay);
    } else {
      GST_WARNING_OBJECT (self, "Could not encode content: %s", content);
    }
  }
  g_free (content);

  return overlay;
}

/* GObject vmethod implementations */

static void
gst_base_qr_overlay_dispose (GObject * object)
{
  GstBaseQROverlayPrivate *priv = PRIV (object);

  gst_mini_object_replace (((GstMiniObject **) & priv->prev_overlay), NULL);
}

/* initialize the qroverlay's class */
static void
gst_base_qr_overlay_class_init (GstBaseQROverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_base_qr_overlay_set_property;
  gobject_class->get_property = gst_base_qr_overlay_get_property;
  gobject_class->dispose = gst_base_qr_overlay_dispose;

  GST_DEBUG_CATEGORY_INIT (gst_base_qr_overlay_debug, "qroverlay", 0,
      "Qrcode overlay base class");

  g_object_class_install_property (gobject_class,
      PROP_X_AXIS, g_param_spec_float ("x",
          "X position (in percent of the width)",
          "X position (in percent of the width)",
          0.0, 100.0, 50.0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_Y_AXIS, g_param_spec_float ("y",
          "Y position (in percent of the height)",
          "Y position (in percent of the height)",
          0.0, 100.0, 50.0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_PIXEL_SIZE, g_param_spec_float ("pixel-size",
          "pixel-size", "Pixel size of each Qrcode pixel",
          1, 100.0, DEFAULT_PROP_PIXEL_SIZE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_QRCODE_ERROR_CORRECTION,
      g_param_spec_enum ("qrcode-error-correction", "qrcode-error-correction",
          "qrcode-error-correction", GST_TYPE_QRCODE_QUALITY,
          DEFAULT_PROP_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseQROverlay::case-sensitive:
   *
   * Strings to encode are case sensitive (e.g. passwords or SSIDs).
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CASE_SENSITIVE,
      g_param_spec_boolean ("case-sensitive", "Case Sensitive",
          "Strings to encode are case sensitive (e.g. passwords or SSIDs)",
          DEFAULT_PROP_CASE_SENSITIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_type_mark_as_plugin_api (GST_TYPE_QRCODE_QUALITY, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_QRCODE_QUALITY, 0);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_base_qr_overlay_init (GstBaseQROverlay * self)
{
  GstBaseQROverlayPrivate *priv = PRIV (self);

  priv->x_percent = 50.0;
  priv->y_percent = 50.0;
  priv->qrcode_quality = DEFAULT_PROP_QUALITY;
  priv->case_sensitive = DEFAULT_PROP_CASE_SENSITIVE;
  priv->span_frame = 0;
  priv->qrcode_size = DEFAULT_PROP_PIXEL_SIZE;
  priv->overlaycomposition =
      gst_element_factory_make ("overlaycomposition", NULL);
  gst_video_info_init (&priv->info);

  if (priv->overlaycomposition) {
    GstPadTemplate *sink_tmpl = gst_static_pad_template_get (&sink_template);
    GstPadTemplate *src_tmpl = gst_static_pad_template_get (&src_template);

    gst_bin_add (GST_BIN (self), priv->overlaycomposition);

    gst_element_add_pad (GST_ELEMENT_CAST (self),
        gst_ghost_pad_new_from_template ("sink",
            priv->overlaycomposition->sinkpads->data, sink_tmpl));
    gst_element_add_pad (GST_ELEMENT_CAST (self),
        gst_ghost_pad_new_from_template ("src",
            priv->overlaycomposition->srcpads->data, src_tmpl));
    gst_object_unref (sink_tmpl);
    gst_object_unref (src_tmpl);

    g_signal_connect_swapped (priv->overlaycomposition, "draw",
        G_CALLBACK (gst_base_qr_overlay_draw_cb), self);
    g_signal_connect_swapped (priv->overlaycomposition, "caps-changed",
        G_CALLBACK (gst_base_qr_overlay_caps_changed_cb), self);
  }
}

static void
gst_base_qr_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseQROverlayPrivate *priv = PRIV (object);

  switch (prop_id) {
    case PROP_X_AXIS:
      priv->x_percent = g_value_get_float (value);
      break;
    case PROP_Y_AXIS:
      priv->y_percent = g_value_get_float (value);
      break;
    case PROP_PIXEL_SIZE:
      priv->qrcode_size = g_value_get_float (value);
      break;
    case PROP_QRCODE_ERROR_CORRECTION:
      priv->qrcode_quality = g_value_get_enum (value);
      break;
    case PROP_CASE_SENSITIVE:
      priv->case_sensitive = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_qr_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseQROverlayPrivate *priv = PRIV (object);

  switch (prop_id) {
    case PROP_X_AXIS:
      g_value_set_float (value, priv->x_percent);
      break;
    case PROP_Y_AXIS:
      g_value_set_float (value, priv->y_percent);
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_float (value, priv->qrcode_size);
      break;
    case PROP_QRCODE_ERROR_CORRECTION:
      g_value_set_enum (value, priv->qrcode_quality);
      break;
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, priv->case_sensitive);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
