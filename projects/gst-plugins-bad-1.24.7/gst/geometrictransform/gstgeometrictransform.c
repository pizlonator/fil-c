/* GStreamer
 * Copyright (C) <2010> Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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

#include "gstgeometrictransform.h"
#include "geometricmath.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (geometric_transform_debug);
#define GST_CAT_DEFAULT geometric_transform_debug

static GstStaticPadTemplate gst_geometric_transform_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ ARGB, BGR, BGRA, BGRx, RGB, "
            "RGBA, RGBx, AYUV, xBGR, xRGB, GRAY8, GRAY16_BE, GRAY16_LE }"))
    );

static GstStaticPadTemplate gst_geometric_transform_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ ARGB, BGR, BGRA, BGRx, RGB, "
            "RGBA, RGBx, AYUV, xBGR, xRGB, GRAY8, GRAY16_BE, GRAY16_LE }"))
    );

static GstVideoFilterClass *parent_class = NULL;

enum
{
  PROP_0,
  PROP_OFF_EDGE_PIXELS
};

#define GST_GT_OFF_EDGES_PIXELS_METHOD_TYPE ( \
    gst_geometric_transform_off_edges_pixels_method_get_type())
static GType
gst_geometric_transform_off_edges_pixels_method_get_type (void)
{
  static GType method_type = 0;

  static const GEnumValue method_types[] = {
    {GST_GT_OFF_EDGES_PIXELS_IGNORE, "Ignore", "ignore"},
    {GST_GT_OFF_EDGES_PIXELS_CLAMP, "Clamp", "clamp"},
    {GST_GT_OFF_EDGES_PIXELS_WRAP, "Wrap", "wrap"},
    {0, NULL, NULL}
  };

  if (!method_type) {
    method_type =
        g_enum_register_static ("GstGeometricTransformOffEdgesPixelsMethod",
        method_types);
  }
  return method_type;
}

#define DEFAULT_OFF_EDGE_PIXELS GST_GT_OFF_EDGES_PIXELS_IGNORE

/* must be called with the object lock */
static gboolean
gst_geometric_transform_generate_map (GstGeometricTransform * gt)
{
  gint x, y;
  gdouble in_x, in_y;
  gboolean ret = TRUE;
  GstGeometricTransformClass *klass;
  gdouble *ptr;

  GST_INFO_OBJECT (gt, "Generating new transform map");

  /* cleanup old map */
  g_free (gt->map);
  gt->map = NULL;

  klass = GST_GEOMETRIC_TRANSFORM_GET_CLASS (gt);

  /* subclass must have defined the map_func */
  g_return_val_if_fail (klass->map_func, FALSE);

  /*
   * (x,y) pairs of the inverse mapping
   */
  gt->map = g_malloc0 (sizeof (gdouble) * gt->width * gt->height * 2);
  ptr = gt->map;

  for (y = 0; y < gt->height; y++) {
    for (x = 0; x < gt->width; x++) {
      if (!klass->map_func (gt, x, y, &in_x, &in_y)) {
        /* child should have warned */
        ret = FALSE;
        goto end;
      }

      ptr[0] = in_x;
      ptr[1] = in_y;
      ptr += 2;
    }
  }

end:
  if (!ret) {
    GST_WARNING_OBJECT (gt, "Generating transform map failed");
    g_free (gt->map);
    gt->map = NULL;
  } else
    gt->needs_remap = FALSE;
  return ret;
}

static gboolean
gst_geometric_transform_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstGeometricTransform *gt;
  gboolean ret = TRUE;
  gint old_width;
  gint old_height;
  GstGeometricTransformClass *klass;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (vfilter);
  klass = GST_GEOMETRIC_TRANSFORM_GET_CLASS (gt);

  old_width = gt->width;
  old_height = gt->height;

  gt->width = in_info->width;
  gt->height = in_info->height;
  gt->row_stride = in_info->stride[0];
  gt->pixel_stride = GST_VIDEO_INFO_COMP_PSTRIDE (in_info, 0);

  /* regenerate the map */
  GST_OBJECT_LOCK (gt);
  if (gt->map == NULL || old_width == 0 || old_height == 0
      || gt->width != old_width || gt->height != old_height) {
    if (klass->prepare_func)
      if (!klass->prepare_func (gt)) {
        GST_OBJECT_UNLOCK (gt);
        return FALSE;
      }
    if (gt->precalc_map)
      gst_geometric_transform_generate_map (gt);
  }
  GST_OBJECT_UNLOCK (gt);
  return ret;
}

static void
gst_geometric_transform_do_map (GstGeometricTransform * gt, guint8 * in_data,
    guint8 * out_data, gint x, gint y, gdouble in_x, gdouble in_y)
{
  gint in_offset;
  gint out_offset;

  out_offset = y * gt->row_stride + x * gt->pixel_stride;

  /* operate on out of edge pixels */
  switch (gt->off_edge_pixels) {
    case GST_GT_OFF_EDGES_PIXELS_CLAMP:
      in_x = CLAMP (in_x, 0, gt->width - 1);
      in_y = CLAMP (in_y, 0, gt->height - 1);
      break;

    case GST_GT_OFF_EDGES_PIXELS_WRAP:
      in_x = gst_gm_mod_float (in_x, gt->width);
      in_y = gst_gm_mod_float (in_y, gt->height);
      if (in_x < 0)
        in_x += gt->width;
      if (in_y < 0)
        in_y += gt->height;
      break;

    default:
      break;
  }

  {
    gint trunc_x = (gint) in_x;
    gint trunc_y = (gint) in_y;
    /* only set the values if the values are valid */
    if (trunc_x >= 0 && trunc_x < gt->width && trunc_y >= 0 &&
        trunc_y < gt->height) {
      in_offset = trunc_y * gt->row_stride + trunc_x * gt->pixel_stride;

      memcpy (out_data + out_offset, in_data + in_offset, gt->pixel_stride);
    }
  }
}

static void
gst_geometric_transform_before_transform (GstBaseTransform * trans,
    GstBuffer * outbuf)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM_CAST (trans);
  GstClockTime timestamp, stream_time;

  timestamp = GST_BUFFER_TIMESTAMP (outbuf);
  stream_time =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (gt, "sync to %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (gt), stream_time);
}

static GstFlowReturn
gst_geometric_transform_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstGeometricTransform *gt;
  GstGeometricTransformClass *klass;
  gint x, y, i;
  GstFlowReturn ret = GST_FLOW_OK;
  gdouble *ptr;
  guint8 *in_data;
  guint8 *out_data;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (vfilter);
  klass = GST_GEOMETRIC_TRANSFORM_GET_CLASS (gt);

  in_data = GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0);
  out_data = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);

  if (GST_VIDEO_FRAME_FORMAT (out_frame) == GST_VIDEO_FORMAT_AYUV) {
    /* in AYUV black is not just all zeros:
     * 0x10 is black for Y,
     * 0x80 is black for Cr and Cb */
    for (i = 0; i < out_frame->map[0].size; i += 4)
      GST_WRITE_UINT32_BE (out_data + i, 0xff108080);
  } else {
    memset (out_data, 0, out_frame->map[0].size);
  }

  GST_OBJECT_LOCK (gt);
  if (gt->precalc_map) {
    if (gt->needs_remap) {
      if (klass->prepare_func)
        if (!klass->prepare_func (gt)) {
          ret = FALSE;
          goto end;
        }
      gst_geometric_transform_generate_map (gt);
    }
    g_return_val_if_fail (gt->map, GST_FLOW_ERROR);
    ptr = gt->map;
    for (y = 0; y < gt->height; y++) {
      for (x = 0; x < gt->width; x++) {
        /* do the mapping */
        gst_geometric_transform_do_map (gt, in_data, out_data, x, y, ptr[0],
            ptr[1]);
        ptr += 2;
      }
    }
  } else {
    for (y = 0; y < gt->height; y++) {
      for (x = 0; x < gt->width; x++) {
        gdouble in_x, in_y;

        if (klass->map_func (gt, x, y, &in_x, &in_y)) {
          gst_geometric_transform_do_map (gt, in_data, out_data, x, y, in_x,
              in_y);
        } else {
          GST_WARNING_OBJECT (gt, "Failed to do mapping for %d %d", x, y);
          ret = GST_FLOW_ERROR;
          goto end;
        }
      }
    }
  }
end:
  GST_OBJECT_UNLOCK (gt);
  return ret;
}

static void
gst_geometric_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGeometricTransform *gt;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);

  switch (prop_id) {
    case PROP_OFF_EDGE_PIXELS:
      GST_OBJECT_LOCK (gt);
      gt->off_edge_pixels = g_value_get_enum (value);
      GST_OBJECT_UNLOCK (gt);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_geometric_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGeometricTransform *gt;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);

  switch (prop_id) {
    case PROP_OFF_EDGE_PIXELS:
      g_value_set_enum (value, gt->off_edge_pixels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_geometric_transform_stop (GstBaseTransform * trans)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM_CAST (trans);

  GST_INFO_OBJECT (gt, "Deleting transform map");

  gt->width = 0;
  gt->height = 0;

  g_free (gt->map);
  gt->map = NULL;

  return TRUE;
}

static void
gst_geometric_transform_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class,
      &gst_geometric_transform_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_geometric_transform_src_template);
}

static void
gst_geometric_transform_class_init (gpointer klass, gpointer class_data)
{
  GObjectClass *obj_class;
  GstBaseTransformClass *trans_class;
  GstVideoFilterClass *vfilter_class;

  obj_class = (GObjectClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;
  vfilter_class = (GstVideoFilterClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  obj_class->set_property = gst_geometric_transform_set_property;
  obj_class->get_property = gst_geometric_transform_get_property;

  trans_class->stop = GST_DEBUG_FUNCPTR (gst_geometric_transform_stop);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_geometric_transform_before_transform);

  vfilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_geometric_transform_set_info);
  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_geometric_transform_transform_frame);

  g_object_class_install_property (obj_class, PROP_OFF_EDGE_PIXELS,
      g_param_spec_enum ("off-edge-pixels", "Off edge pixels",
          "What to do with off edge pixels",
          GST_GT_OFF_EDGES_PIXELS_METHOD_TYPE, DEFAULT_OFF_EDGE_PIXELS,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_GT_OFF_EDGES_PIXELS_METHOD_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GEOMETRIC_TRANSFORM, 0);
}

static void
gst_geometric_transform_init (GTypeInstance * instance, gpointer g_class)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM_CAST (instance);

  gt->off_edge_pixels = DEFAULT_OFF_EDGE_PIXELS;
  gt->precalc_map = TRUE;
  gt->needs_remap = TRUE;
}

GType
gst_geometric_transform_get_type (void)
{
  static GType geometric_transform_type = 0;

  if (!geometric_transform_type) {
    static const GTypeInfo geometric_transform_info = {
      sizeof (GstGeometricTransformClass),
      gst_geometric_transform_base_init,
      NULL,
      gst_geometric_transform_class_init,
      NULL,
      NULL,
      sizeof (GstGeometricTransform),
      0,
      gst_geometric_transform_init,
    };

    geometric_transform_type = g_type_register_static (GST_TYPE_VIDEO_FILTER,
        "GstGeometricTransform", &geometric_transform_info,
        G_TYPE_FLAG_ABSTRACT);

    GST_DEBUG_CATEGORY_INIT (geometric_transform_debug, "geometrictransform", 0,
        "Base class for geometric transform elements");
  }
  return geometric_transform_type;
}

/*
 * Must be called with the object lock
 */
void
gst_geometric_transform_set_need_remap (GstGeometricTransform * gt)
{
  gt->needs_remap = TRUE;
}
