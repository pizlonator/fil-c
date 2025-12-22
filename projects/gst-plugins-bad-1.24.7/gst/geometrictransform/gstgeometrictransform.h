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

#ifndef __GST_GEOMETRIC_TRANSFORM_H__
#define __GST_GEOMETRIC_TRANSFORM_H__

#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GEOMETRIC_TRANSFORM \
  (gst_geometric_transform_get_type())
#define GST_GEOMETRIC_TRANSFORM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GEOMETRIC_TRANSFORM,GstGeometricTransform))
#define GST_GEOMETRIC_TRANSFORM_CAST(obj) ((GstGeometricTransform *)(obj))
#define GST_GEOMETRIC_TRANSFORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GEOMETRIC_TRANSFORM,GstGeometricTransformClass))
#define GST_IS_GEOMETRIC_TRANSFORM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GEOMETRIC_TRANSFORM))
#define GST_IS_GEOMETRIC_TRANSFORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GEOMETRIC_TRANSFORM))
#define GST_GEOMETRIC_TRANSFORM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GEOMETRIC_TRANSFORM,GstGeometricTransformClass))

enum
{
  GST_GT_OFF_EDGES_PIXELS_IGNORE = 0,
  GST_GT_OFF_EDGES_PIXELS_CLAMP,
  GST_GT_OFF_EDGES_PIXELS_WRAP
};

typedef struct _GstGeometricTransform GstGeometricTransform;
typedef struct _GstGeometricTransformClass GstGeometricTransformClass;

/**
 * GstGeometricTransformMapFunc:
 *
 * Given the output pixel position, this function calculates the input pixel
 * position. The element using this function will then copy the input pixel
 * data to the output pixel.
 *
 * @gt: The #GstGeometricTransform
 * @x: The output pixel x coordinate
 * @y: The output pixel y coordinate
 * @_input_x: The input pixel x coordinate
 * @_input_y: The input pixel y coordinate
 * Returns: True on success, false otherwise
 */
typedef gboolean (*GstGeometricTransformMapFunc) (GstGeometricTransform * gt,
    gint x, gint y, gdouble * _input_x, gdouble *_input_y);

/**
 * GstGeometricTransformPrepareFunc:
 *
 * Called right before starting to calculate the mapping so that
 * instances might precalculate some values.
 *
 * Called with the object lock
 */
typedef gboolean (*GstGeometricTransformPrepareFunc) (
    GstGeometricTransform * gt);

/**
 * GstGeometricTransform:
 *
 * Opaque datastructure.
 */
struct _GstGeometricTransform {
  GstVideoFilter videofilter;

  gint width, height;
  GstVideoFormat format;
  gint pixel_stride;
  gint row_stride;

  /* Must be set on NULL state.
   * Useful for subclasses that use don't want to use a fixed precalculated
   * pixel mapping table. Like 'diffuse' that uses random values for each pic.
   */
  gboolean precalc_map;
  gboolean needs_remap;

  /* properties */
  gint off_edge_pixels;

  gdouble *map;
};

struct _GstGeometricTransformClass {
  GstVideoFilterClass parent_class;

  GstGeometricTransformMapFunc map_func;
  GstGeometricTransformPrepareFunc prepare_func;
};

GType gst_geometric_transform_get_type (void);

void gst_geometric_transform_set_need_remap (GstGeometricTransform * gt);

G_END_DECLS

#endif /* __GST_GEOMETRIC_TRANSFORM_H__ */
