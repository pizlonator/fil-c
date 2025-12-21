/* GStreamer
 * Copyright (C) <2014> Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright (C) 2015, Matthew Waters <matthew@centricular.com>
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

#include "gstvideoaffinetransformationmeta.h"

#include <string.h>

/**
 * SECTION:gstvideoaffinetransformationmeta
 * @title: GstVideoAffineTransformationMeta
 * @short_description: GstMeta for video affine transformation
 *
 */

GType
gst_video_affine_transformation_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type =
        gst_meta_api_type_register ("GstVideoAffineTransformationAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_affine_transformation_meta_transform (GstBuffer * dest,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoAffineTransformationMeta *dmeta, *smeta;

  smeta = (GstVideoAffineTransformationMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta =
        (GstVideoAffineTransformationMeta *) gst_buffer_add_meta (dest,
        GST_VIDEO_AFFINE_TRANSFORMATION_META_INFO, NULL);

    if (!dmeta)
      return FALSE;

    memcpy (dmeta->matrix, smeta->matrix, sizeof (dmeta->matrix[0]) * 16);
  }
  return TRUE;
}

static gboolean
gst_video_affine_transformation_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoAffineTransformationMeta *af_meta =
      (GstVideoAffineTransformationMeta *) meta;
  gfloat matrix[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  memcpy (af_meta->matrix, matrix, sizeof (matrix[0]) * 16);

  return TRUE;
}

const GstMetaInfo *
gst_video_affine_transformation_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE,
        "GstVideoAffineTransformationMeta",
        sizeof (GstVideoAffineTransformationMeta),
        gst_video_affine_transformation_meta_init,
        NULL,
        gst_video_affine_transformation_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & info, (GstMetaInfo *) meta);
  }
  return info;
}

/**
 * gst_buffer_add_video_affine_transformation_meta:
 * @buffer: a #GstBuffer
 *
 * Attaches GstVideoAffineTransformationMeta metadata to @buffer with
 * the given parameters.
 *
 * Returns: (transfer none): the #GstVideoAffineTransformationMeta on @buffer.
 *
 * Since: 1.8
 */
GstVideoAffineTransformationMeta *
gst_buffer_add_video_affine_transformation_meta (GstBuffer * buffer)
{
  GstVideoAffineTransformationMeta *meta;

  g_return_val_if_fail (buffer != NULL, NULL);

  meta =
      (GstVideoAffineTransformationMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_AFFINE_TRANSFORMATION_META_INFO, NULL);

  if (!meta)
    return NULL;

  return meta;
}

/**
 * gst_video_affine_transformation_meta_apply_matrix:
 * @meta: a #GstVideoAffineTransformationMeta
 * @matrix: (array fixed-size=16): a 4x4 transformation matrix to be applied
 *
 * Apply a transformation using the given 4x4 transformation matrix.
 * Performs the multiplication, meta->matrix X matrix.
 *
 * Since: 1.8
 */
void gst_video_affine_transformation_meta_apply_matrix
    (GstVideoAffineTransformationMeta * meta, const gfloat matrix[16])
{
  gfloat res[16] = { 0.0f };
  int i, j, k;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      for (k = 0; k < 4; k++) {
        res[i + (j * 4)] += meta->matrix[i + (k * 4)] * matrix[k + (j * 4)];
      }
    }
  }

  memcpy (meta->matrix, res, sizeof (meta->matrix[0]) * 16);
}
