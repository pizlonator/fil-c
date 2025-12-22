/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (c) 2020 Anthony Violo <anthony.violo@ubicast.eu>
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

/**
 * SECTION:element-debugqroverlay
 *
 * This element will build a Json string that contains a description of the
 * buffer and will convert the string to a QRcode. The QRcode contains a
 * timestamp, a buffer number, a framerate and some custom extra-data. Each
 * frame will have a Qrcode overlaid in the video stream. Some properties are
 * available to set the position and to define its size. You can add custom data
 * with the properties #debugqroverlay:extra-data-name and
 * #debugqroverlay:extra-data-array. You can also define the quality of the Qrcode
 * with #GstBaseQROverlay:qrcode-error-correction. You can also define interval and
 * span of #debugqroverlay:extra-data-name #debugqroverlay:extra-data-array
 *
 * ## Example launch line
 *
 * ``` bash
 * gst-launch -v -m videotestsrc ! debugqroverlay ! fakesink silent=TRUE
 * ```
 *
 * Since: 1.20
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

#include "gstqroverlayelements.h"
#include "gstdebugqroverlay.h"


GST_DEBUG_CATEGORY_STATIC (gst_debug_qr_overlay_debug);
#define GST_CAT_DEFAULT gst_debug_qr_overlay_debug

static gchar *get_qrcode_content (GstBaseQROverlay * base, GstBuffer * buf,
    GstVideoInfo * info, gboolean * reuse_prev);

enum
{
  PROP_0,
  PROP_DATA_INTERVAL_BUFFERS,
  PROP_DATA_SPAN_BUFFERS,
  PROP_EXTRA_DATA_NAME,
  PROP_EXTRA_DATA_ARRAY,
};

struct _GstDebugQROverlay
{
  GstBaseQROverlay parent;

  guint32 frame_number;
  guint array_counter;
  guint array_size;
  guint span_frame;
  guint64 extra_data_interval_buffers;
  guint64 extra_data_span_buffers;
  gchar *extra_data_name;
  gchar *extra_data_str;
  gchar **extra_data_array;
  gfloat x_percent;
  gfloat y_percent;
  gboolean silent;
  gboolean extra_data_enabled;
};

#define DEFAULT_PROP_QUALITY    1
#define DEFAULT_PROP_PIXEL_SIZE    3

#define gst_debug_qr_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDebugQROverlay, gst_debug_qr_overlay,
    GST_TYPE_BASE_QR_OVERLAY);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (debugqroverlay, "debugqroverlay",
    GST_RANK_NONE, GST_TYPE_DEBUG_QR_OVERLAY, qroverlay_element_init (plugin));


static void gst_debug_qr_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_debug_qr_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* initialize the qroverlay's class */
static void
gst_debug_qr_overlay_class_init (GstDebugQROverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_debug_qr_overlay_debug, "debugqroverlay", 0,
      "Qrcode overlay element");

  gobject_class->set_property = gst_debug_qr_overlay_set_property;
  gobject_class->get_property = gst_debug_qr_overlay_get_property;

  g_object_class_install_property (gobject_class,
      PROP_DATA_INTERVAL_BUFFERS,
      g_param_spec_int64 ("extra-data-interval-buffers",
          "extra-data-interval-buffers",
          "Extra data append into the Qrcode at the first buffer of each "
          " interval", 0, G_MAXINT64, 60, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_DATA_SPAN_BUFFERS, g_param_spec_int64 ("extra-data-span-buffers",
          "extra-data-span-buffers",
          "Numbers of consecutive buffers that the extra data will be inserted "
          " (counting the first buffer)", 0, G_MAXINT64, 1, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_EXTRA_DATA_NAME, g_param_spec_string ("extra-data-name",
          "Extra data name",
          "Json key name for extra append data", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_EXTRA_DATA_ARRAY, g_param_spec_string ("extra-data-array",
          "Extra data array",
          "List of comma separated values that the extra data value will be "
          " cycled from at each interval, example array structure :"
          " \"240,480,720,960,1200,1440,1680,1920\"", NULL, G_PARAM_READWRITE));


  gst_element_class_set_details_simple (gstelement_class,
      "qroverlay",
      "Qrcode overlay containing buffer information",
      "Overlay Qrcodes over each buffer with buffer information and custom data",
      "Anthony Violo <anthony.violo@ubicast.eu>");

  gst_type_mark_as_plugin_api (GST_TYPE_BASE_QR_OVERLAY, 0);

  GST_BASE_QR_OVERLAY_CLASS (klass)->get_content =
      GST_DEBUG_FUNCPTR (get_qrcode_content);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_debug_qr_overlay_init (GstDebugQROverlay * filter)
{
  filter->frame_number = 1;
  filter->x_percent = 50.0;
  filter->y_percent = 50.0;
  filter->array_counter = 0;
  filter->array_size = 0;
  filter->extra_data_interval_buffers = 60;
  filter->extra_data_span_buffers = 1;
  filter->span_frame = 0;
}

static void
gst_debug_qr_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDebugQROverlay *filter = GST_DEBUG_QR_OVERLAY (object);

  switch (prop_id) {
    case PROP_DATA_INTERVAL_BUFFERS:
      filter->extra_data_interval_buffers = g_value_get_int64 (value);
      break;
    case PROP_DATA_SPAN_BUFFERS:
      filter->extra_data_span_buffers = g_value_get_int64 (value);
      break;
    case PROP_EXTRA_DATA_NAME:
      filter->extra_data_name = g_value_dup_string (value);
      break;
    case PROP_EXTRA_DATA_ARRAY:
    {
      g_clear_pointer (&filter->extra_data_str, g_free);
      g_clear_pointer (&filter->extra_data_array, g_strfreev);
      filter->extra_data_str = g_value_dup_string (value);
      if (filter->extra_data_str) {
        filter->extra_data_array = g_strsplit (filter->extra_data_str, ",", -1);
        filter->array_size = g_strv_length (filter->extra_data_array);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_debug_qr_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDebugQROverlay *filter = GST_DEBUG_QR_OVERLAY (object);

  switch (prop_id) {
    case PROP_DATA_INTERVAL_BUFFERS:
      g_value_set_int64 (value, filter->extra_data_interval_buffers);
      break;
    case PROP_DATA_SPAN_BUFFERS:
      g_value_set_int64 (value, filter->extra_data_span_buffers);
      break;
    case PROP_EXTRA_DATA_NAME:
      g_value_set_string (value, filter->extra_data_name);
      break;
    case PROP_EXTRA_DATA_ARRAY:
      g_value_set_string (value, filter->extra_data_str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gchar *
get_qrcode_content (GstBaseQROverlay * base, GstBuffer * buf,
    GstVideoInfo * info, gboolean * reuse_prev)
{
  GstDebugQROverlay *filter = GST_DEBUG_QR_OVERLAY (base);
  GString *res;
  JsonGenerator *jgen;
  gchar *framerate_string = g_strdup_printf ("%d/%d", info->fps_n, info->fps_d);

  JsonObject *jobj = json_object_new ();
  JsonNode *root = json_node_new (JSON_NODE_OBJECT);

  *reuse_prev = FALSE;
  json_object_set_int_member (jobj, "TIMESTAMP",
      (gint64) GST_BUFFER_TIMESTAMP (buf));
  json_object_set_int_member (jobj, "BUFFERCOUNT",
      (gint64) filter->frame_number);
  json_object_set_string_member (jobj, "FRAMERATE", framerate_string);
  json_object_set_string_member (jobj, "NAME", GST_ELEMENT_NAME (filter));
  g_free (framerate_string);

  if (filter->extra_data_array && filter->extra_data_name &&
      (filter->frame_number == 1
          || filter->frame_number % filter->extra_data_interval_buffers == 1
          || (filter->span_frame > 0
              && filter->span_frame < filter->extra_data_span_buffers))) {
    json_object_set_string_member (jobj, filter->extra_data_name,
        filter->extra_data_array[filter->array_counter]);

    filter->span_frame++;
    if (filter->span_frame == filter->extra_data_span_buffers) {
      filter->array_counter++;
      filter->span_frame = 0;
      if (filter->array_counter >= filter->array_size)
        filter->array_counter = 0;
    }
  }

  jgen = json_generator_new ();
  json_node_set_object (root, jobj);
  json_generator_set_root (jgen, root);
  res = g_string_new (NULL);
  res = json_generator_to_gstring (jgen, res);
  g_object_unref (jgen);
  filter->frame_number++;

  return g_string_free (res, FALSE);
}
