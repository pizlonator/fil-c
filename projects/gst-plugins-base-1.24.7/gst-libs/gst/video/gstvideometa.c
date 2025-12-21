/* GStreamer
 * Copyright (C) <2011> Wim Taymans <wim.taymans@gmail.com>
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

#include "gstvideometa.h"

#include <string.h>
#include <gst/base/base.h>

/**
 * SECTION:gstvideometa
 * @title: GstMeta for video
 * @short_description: Video related GstMeta
 *
 */

static gboolean
default_map (GstVideoMeta * meta, guint plane, GstMapInfo * info,
    gpointer * data, gint * stride, GstMapFlags flags);
static gboolean
default_unmap (GstVideoMeta * meta, guint plane, GstMapInfo * info);

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("videometa", 0, "videometa");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static gboolean
gst_video_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVideoMeta *emeta = (GstVideoMeta *) meta;

  emeta->buffer = NULL;
  emeta->flags = GST_VIDEO_FRAME_FLAG_NONE;
  emeta->format = GST_VIDEO_FORMAT_UNKNOWN;
  emeta->id = 0;
  emeta->width = emeta->height = emeta->n_planes = 0;
  memset (emeta->offset, 0, sizeof (emeta->offset));
  memset (emeta->stride, 0, sizeof (emeta->stride));
  gst_video_alignment_reset (&emeta->alignment);
  emeta->map = NULL;
  emeta->unmap = NULL;

  return TRUE;
}

static gboolean
gst_video_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoMeta *dmeta, *smeta;
  guint i;

  smeta = (GstVideoMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta =
          (GstVideoMeta *) gst_buffer_add_meta (dest, GST_VIDEO_META_INFO,
          NULL);

      if (!dmeta)
        return FALSE;

      dmeta->buffer = dest;

      GST_DEBUG ("copy video metadata");
      dmeta->flags = smeta->flags;
      dmeta->format = smeta->format;
      dmeta->id = smeta->id;
      dmeta->width = smeta->width;
      dmeta->height = smeta->height;

      dmeta->n_planes = smeta->n_planes;
      for (i = 0; i < dmeta->n_planes; i++) {
        dmeta->offset[i] = smeta->offset[i];
        dmeta->stride[i] = smeta->stride[i];
        dmeta->alignment = smeta->alignment;
      }
      dmeta->map = smeta->map;
      dmeta->unmap = smeta->unmap;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

GType
gst_video_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR,
    GST_META_TAG_VIDEO_COLORSPACE_STR,
    GST_META_TAG_VIDEO_SIZE_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVideoMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
video_meta_serialize (const GstMeta * meta, GstByteArrayInterface * data,
    guint8 * version)
{
  GstVideoMeta *vmeta = (GstVideoMeta *) meta;

  if (vmeta->map != default_map || vmeta->unmap != default_unmap) {
    GST_WARNING ("Cannot serialize video meta with custom map/unmap functions");
    return FALSE;
  }

  gsize size = 36 + vmeta->n_planes * 16;
  guint8 *ptr = gst_byte_array_interface_append (data, size);
  if (ptr == NULL)
    return FALSE;

  GstByteWriter bw;
  gboolean success = TRUE;
  gst_byte_writer_init_with_data (&bw, ptr, size, FALSE);
  success &= gst_byte_writer_put_int32_le (&bw, vmeta->flags);
  success &= gst_byte_writer_put_int32_le (&bw, vmeta->format);
  success &= gst_byte_writer_put_uint32_le (&bw, vmeta->width);
  success &= gst_byte_writer_put_uint32_le (&bw, vmeta->height);
  success &= gst_byte_writer_put_uint32_le (&bw, vmeta->n_planes);
  for (int n = 0; n < vmeta->n_planes; n++)
    success &= gst_byte_writer_put_uint64_le (&bw, vmeta->offset[n]);
  for (int n = 0; n < vmeta->n_planes; n++)
    success &= gst_byte_writer_put_int32_le (&bw, vmeta->stride[n]);
  success &= gst_byte_writer_put_uint32_le (&bw, vmeta->alignment.padding_top);
  success &=
      gst_byte_writer_put_uint32_le (&bw, vmeta->alignment.padding_bottom);
  success &= gst_byte_writer_put_uint32_le (&bw, vmeta->alignment.padding_left);
  success &=
      gst_byte_writer_put_uint32_le (&bw, vmeta->alignment.padding_right);
  for (int n = 0; n < vmeta->n_planes; n++)
    success &=
        gst_byte_writer_put_uint32_le (&bw, vmeta->alignment.stride_align[n]);
  g_assert (success);

  return TRUE;
}

static GstMeta *
video_meta_deserialize (const GstMetaInfo * info, GstBuffer * buffer,
    const guint8 * data, gsize size, guint8 version)
{
  GstVideoMeta *vmeta = NULL;
  gint32 flags;
  gint32 format;
  guint width;
  guint height;
  guint n_planes;
  GstVideoAlignment align;
  guint64 offset64[GST_VIDEO_MAX_PLANES];
  gint32 stride[GST_VIDEO_MAX_PLANES];

  if (version != 0)
    return NULL;

  GstByteReader br;
  gboolean success = TRUE;
  gst_byte_reader_init (&br, data, size);
  success &= gst_byte_reader_get_int32_le (&br, &flags);
  success &= gst_byte_reader_get_int32_le (&br, &format);
  success &= gst_byte_reader_get_uint32_le (&br, &width);
  success &= gst_byte_reader_get_uint32_le (&br, &height);
  success &= gst_byte_reader_get_uint32_le (&br, &n_planes);

  if (!success || n_planes > GST_VIDEO_MAX_PLANES)
    return NULL;

  for (int n = 0; n < n_planes; n++)
    success &= gst_byte_reader_get_uint64_le (&br, &offset64[n]);
  for (int n = 0; n < n_planes; n++)
    success &= gst_byte_reader_get_int32_le (&br, &stride[n]);
  success &= gst_byte_reader_get_uint32_le (&br, &align.padding_top);
  success &= gst_byte_reader_get_uint32_le (&br, &align.padding_bottom);
  success &= gst_byte_reader_get_uint32_le (&br, &align.padding_left);
  success &= gst_byte_reader_get_uint32_le (&br, &align.padding_right);
  for (int n = 0; n < n_planes; n++)
    success &= gst_byte_reader_get_uint32_le (&br, &align.stride_align[n]);

  if (!success)
    return NULL;

#if GLIB_SIZEOF_SIZE_T != 8
  gsize offset[GST_VIDEO_MAX_PLANES];
  for (int i = 0; i < n_planes; i++) {
    if (offset64[i] > G_MAXSIZE)
      return NULL;
    offset[i] = offset64[i];
  }
#else
  gsize *offset = (gsize *) offset64;
#endif

  vmeta =
      gst_buffer_add_video_meta_full (buffer, flags, format, width, height,
      n_planes, offset, stride);
  gst_video_meta_set_alignment (vmeta, align);

  return (GstMeta *) vmeta;
}

/* video metadata */
const GstMetaInfo *
gst_video_meta_get_info (void)
{
  static const GstMetaInfo *video_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & video_meta_info)) {
    GstMetaInfo *info = gst_meta_info_new (GST_VIDEO_META_API_TYPE,
        "GstVideoMeta",
        sizeof (GstVideoMeta));
    info->init_func = gst_video_meta_init;
    info->transform_func = gst_video_meta_transform;
    info->serialize_func = video_meta_serialize;
    info->deserialize_func = video_meta_deserialize;
    const GstMetaInfo *meta = gst_meta_info_register (info);
    g_once_init_leave ((GstMetaInfo **) & video_meta_info,
        (GstMetaInfo *) meta);
  }
  return video_meta_info;
}

/**
 * gst_buffer_get_video_meta:
 * @buffer: a #GstBuffer
 *
 * Find the #GstVideoMeta on @buffer with the lowest @id.
 *
 * Buffers can contain multiple #GstVideoMeta metadata items when dealing with
 * multiview buffers.
 *
 * Returns: (transfer none) (nullable): the #GstVideoMeta with lowest id (usually 0) or %NULL when there
 * is no such metadata on @buffer.
 */
GstVideoMeta *
gst_buffer_get_video_meta (GstBuffer * buffer)
{
  gpointer state = NULL;
  GstVideoMeta *out = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_VIDEO_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      GstVideoMeta *vmeta = (GstVideoMeta *) meta;
      if (vmeta->id == 0)
        return vmeta;           /* Early out for id 0 */
      if (out == NULL || vmeta->id < out->id)
        out = vmeta;
    }
  }
  return out;
}

/**
 * gst_buffer_get_video_meta_id:
 * @buffer: a #GstBuffer
 * @id: a metadata id
 *
 * Find the #GstVideoMeta on @buffer with the given @id.
 *
 * Buffers can contain multiple #GstVideoMeta metadata items when dealing with
 * multiview buffers.
 *
 * Returns: (transfer none) (nullable): the #GstVideoMeta with @id or %NULL when there is no such metadata
 * on @buffer.
 */
GstVideoMeta *
gst_buffer_get_video_meta_id (GstBuffer * buffer, gint id)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_VIDEO_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      GstVideoMeta *vmeta = (GstVideoMeta *) meta;
      if (vmeta->id == id)
        return vmeta;
    }
  }
  return NULL;
}

static gboolean
default_map (GstVideoMeta * meta, guint plane, GstMapInfo * info,
    gpointer * data, gint * stride, GstMapFlags flags)
{
  guint idx, length;
  gsize offset, skip;
  GstBuffer *buffer = meta->buffer;

  offset = meta->offset[plane];

  /* find the memory block for this plane, this is the memory block containing
   * the plane offset. FIXME use plane size */
  if (!gst_buffer_find_memory (buffer, offset, 1, &idx, &length, &skip))
    goto no_memory;

  if (!gst_buffer_map_range (buffer, idx, length, info, flags))
    goto cannot_map;

  *stride = meta->stride[plane];
  *data = (guint8 *) info->data + skip;

  return TRUE;

  /* ERRORS */
no_memory:
  {
    GST_ERROR ("plane %u, no memory at offset %" G_GSIZE_FORMAT, plane, offset);
    return FALSE;
  }
cannot_map:
  {
    GST_ERROR ("cannot map memory range %u-%u", idx, length);
    return FALSE;
  }
}

static gboolean
default_unmap (GstVideoMeta * meta, guint plane, GstMapInfo * info)
{
  GstBuffer *buffer = meta->buffer;

  gst_buffer_unmap (buffer, info);

  return TRUE;
}

/**
 * gst_buffer_add_video_meta:
 * @buffer: a #GstBuffer
 * @flags: #GstVideoFrameFlags
 * @format: a #GstVideoFormat
 * @width: the width
 * @height: the height
 *
 * Attaches GstVideoMeta metadata to @buffer with the given parameters and the
 * default offsets and strides for @format and @width x @height.
 *
 * This function calculates the default offsets and strides and then calls
 * gst_buffer_add_video_meta_full() with them.
 *
 * Returns: (transfer none): the #GstVideoMeta on @buffer.
 */
GstVideoMeta *
gst_buffer_add_video_meta (GstBuffer * buffer,
    GstVideoFrameFlags flags, GstVideoFormat format, guint width, guint height)
{
  GstVideoMeta *meta;
  GstVideoInfo info;

  if (!gst_video_info_set_format (&info, format, width, height))
    return NULL;

  meta =
      gst_buffer_add_video_meta_full (buffer, flags, format, width,
      height, info.finfo->n_planes, info.offset, info.stride);

  return meta;
}

/**
 * gst_buffer_add_video_meta_full:
 * @buffer: a #GstBuffer
 * @flags: #GstVideoFrameFlags
 * @format: a #GstVideoFormat
 * @width: the width
 * @height: the height
 * @n_planes: number of planes
 * @offset: (array fixed-size=4): offset of each plane
 * @stride: (array fixed-size=4): stride of each plane
 *
 * Attaches GstVideoMeta metadata to @buffer with the given parameters.
 *
 * Returns: (transfer none): the #GstVideoMeta on @buffer.
 */
GstVideoMeta *
gst_buffer_add_video_meta_full (GstBuffer * buffer,
    GstVideoFrameFlags flags, GstVideoFormat format, guint width,
    guint height, guint n_planes, const gsize offset[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES])
{
  GstVideoMeta *meta;
  guint i;

  meta =
      (GstVideoMeta *) gst_buffer_add_meta (buffer, GST_VIDEO_META_INFO, NULL);

  if (!meta)
    return NULL;

  meta->flags = flags;
  meta->format = format;
  meta->id = 0;
  meta->width = width;
  meta->height = height;
  meta->buffer = buffer;

  meta->n_planes = n_planes;
  for (i = 0; i < n_planes; i++) {
    meta->offset[i] = offset[i];
    meta->stride[i] = stride[i];
    GST_LOG ("plane %d, offset %" G_GSIZE_FORMAT ", stride %d", i, offset[i],
        stride[i]);
  }
  meta->map = default_map;
  meta->unmap = default_unmap;

  return meta;
}

/**
 * gst_video_meta_map:
 * @meta: a #GstVideoMeta
 * @plane: a plane
 * @info: a #GstMapInfo
 * @data: (out): the data of @plane
 * @stride: (out): the stride of @plane
 * @flags: @GstMapFlags
 *
 * Map the video plane with index @plane in @meta and return a pointer to the
 * first byte of the plane and the stride of the plane.
 *
 * Returns: TRUE if the map operation was successful.
 */
gboolean
gst_video_meta_map (GstVideoMeta * meta, guint plane, GstMapInfo * info,
    gpointer * data, gint * stride, GstMapFlags flags)
{
  g_return_val_if_fail (meta != NULL, FALSE);
  g_return_val_if_fail (meta->map != NULL, FALSE);
  g_return_val_if_fail (plane < meta->n_planes, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (stride != NULL, FALSE);
  g_return_val_if_fail (meta->buffer != NULL, FALSE);
  g_return_val_if_fail (!(flags & GST_MAP_WRITE)
      || gst_buffer_is_writable (meta->buffer), FALSE);

  return meta->map (meta, plane, info, data, stride, flags);
}

/**
 * gst_video_meta_unmap:
 * @meta: a #GstVideoMeta
 * @plane: a plane
 * @info: a #GstMapInfo
 *
 * Unmap a previously mapped plane with gst_video_meta_map().
 *
 * Returns: TRUE if the memory was successfully unmapped.
 */
gboolean
gst_video_meta_unmap (GstVideoMeta * meta, guint plane, GstMapInfo * info)
{
  g_return_val_if_fail (meta != NULL, FALSE);
  g_return_val_if_fail (meta->unmap != NULL, FALSE);
  g_return_val_if_fail (plane < meta->n_planes, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  return meta->unmap (meta, plane, info);
}

static gboolean
gst_video_meta_is_alignment_valid (GstVideoAlignment * align)
{
  gint i;

  g_return_val_if_fail (align != NULL, FALSE);

  if (align->padding_top != 0 || align->padding_bottom != 0 ||
      align->padding_left != 0 || align->padding_right != 0)
    return TRUE;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    if (align->stride_align[i] != 0)
      return TRUE;
  }

  return FALSE;
}

static gboolean
gst_video_meta_validate_alignment (GstVideoMeta * meta,
    gsize plane_size[GST_VIDEO_MAX_PLANES])
{
  GstVideoInfo info;
  guint i;

  if (!gst_video_meta_is_alignment_valid (&meta->alignment)) {
    GST_LOG ("Set alignment on meta to all zero");

    /* When alignment is invalid, no further check is needed,
       unless user wants to calculate the pitch for each plane. */
    if (!plane_size)
      return TRUE;
  }

  gst_video_info_init (&info);
  gst_video_info_set_format (&info, meta->format, meta->width, meta->height);

  if (!gst_video_info_align_full (&info, &meta->alignment, plane_size)) {
    GST_WARNING ("Failed to align meta with its alignment");
    return FALSE;
  }

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&info); i++) {
    if (GST_VIDEO_INFO_PLANE_STRIDE (&info, i) != meta->stride[i]) {
      GST_WARNING
          ("Stride of plane %d defined in meta (%d) is different from the one computed from the alignment (%d)",
          i, meta->stride[i], GST_VIDEO_INFO_PLANE_STRIDE (&info, i));
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * gst_video_meta_set_alignment:
 * @meta: a #GstVideoMeta
 * @alignment: a #GstVideoAlignment
 *
 * Set the alignment of @meta to @alignment. This function checks that
 * the paddings defined in @alignment are compatible with the strides
 * defined in @meta and will fail to update if they are not.
 *
 * Returns: %TRUE if @alignment's meta has been updated, %FALSE if not
 *
 * Since: 1.18
 */
gboolean
gst_video_meta_set_alignment (GstVideoMeta * meta, GstVideoAlignment alignment)
{
  GstVideoAlignment old;

  g_return_val_if_fail (meta, FALSE);

  old = meta->alignment;
  meta->alignment = alignment;

  if (!gst_video_meta_validate_alignment (meta, NULL)) {
    /* Invalid alignment, restore the previous one */
    meta->alignment = old;
    return FALSE;
  }

  GST_LOG ("Set alignment on meta: padding %u-%ux%u-%u", alignment.padding_top,
      alignment.padding_left, alignment.padding_right,
      alignment.padding_bottom);

  return TRUE;
}

/**
 * gst_video_meta_get_plane_size:
 * @meta: a #GstVideoMeta
 * @plane_size: (out caller-allocates) (array fixed-size=4): array used to store the plane sizes
 *
 * Compute the size, in bytes, of each video plane described in @meta including
 * any padding and alignment constraint defined in @meta->alignment.
 *
 * Returns: %TRUE if @meta's alignment is valid and @plane_size has been
 * updated, %FALSE otherwise
 *
 * Since: 1.18
 */
gboolean
gst_video_meta_get_plane_size (GstVideoMeta * meta,
    gsize plane_size[GST_VIDEO_MAX_PLANES])
{
  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (plane_size, FALSE);

  return gst_video_meta_validate_alignment (meta, plane_size);
}

/**
 * gst_video_meta_get_plane_height:
 * @meta: a #GstVideoMeta
 * @plane_height: (out caller-allocates) (array fixed-size=4): array used to store the plane height
 *
 * Compute the padded height of each plane from @meta (padded size
 * divided by stride).
 *
 * It is not valid to call this function with a meta associated to a
 * TILED video format.
 *
 * Returns: %TRUE if @meta's alignment is valid and @plane_height has been
 * updated, %FALSE otherwise
 *
 * Since: 1.18
 */
gboolean
gst_video_meta_get_plane_height (GstVideoMeta * meta,
    guint plane_height[GST_VIDEO_MAX_PLANES])
{
  gsize plane_size[GST_VIDEO_MAX_PLANES];
  guint i;
  GstVideoInfo info;

  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (plane_height, FALSE);

  gst_video_info_init (&info);
  gst_video_info_set_format (&info, meta->format, meta->width, meta->height);
  g_return_val_if_fail (!GST_VIDEO_FORMAT_INFO_IS_TILED (&info), FALSE);

  if (!gst_video_meta_get_plane_size (meta, plane_size))
    return FALSE;

  for (i = 0; i < meta->n_planes; i++) {
    if (!meta->stride[i])
      plane_height[i] = 0;
    else
      plane_height[i] = plane_size[i] / meta->stride[i];
  }

  for (; i < GST_VIDEO_MAX_PLANES; i++)
    plane_height[i] = 0;

  return TRUE;
}

static gboolean
gst_video_crop_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoCropMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstVideoCropMeta *) meta;
    dmeta = gst_buffer_add_video_crop_meta (dest);
    if (!dmeta)
      return FALSE;

    GST_DEBUG ("copy crop metadata");
    dmeta->x = smeta->x;
    dmeta->y = smeta->y;
    dmeta->width = smeta->width;
    dmeta->height = smeta->height;
  } else if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstVideoMetaTransform *trans = data;
    gint ow, oh, nw, nh;

    smeta = (GstVideoCropMeta *) meta;
    dmeta = gst_buffer_add_video_crop_meta (dest);
    if (!dmeta)
      return FALSE;

    ow = GST_VIDEO_INFO_WIDTH (trans->in_info);
    nw = GST_VIDEO_INFO_WIDTH (trans->out_info);
    oh = GST_VIDEO_INFO_HEIGHT (trans->in_info);
    nh = GST_VIDEO_INFO_HEIGHT (trans->out_info);

    GST_DEBUG ("scaling crop metadata %dx%d -> %dx%d", ow, oh, nw, nh);
    dmeta->x = (smeta->x * nw) / ow;
    dmeta->y = (smeta->y * nh) / oh;
    dmeta->width = (smeta->width * nw) / ow;
    dmeta->height = (smeta->height * nh) / oh;
    GST_DEBUG ("crop offset %dx%d -> %dx%d", smeta->x, smeta->y, dmeta->x,
        dmeta->y);
    GST_DEBUG ("crop size   %dx%d -> %dx%d", smeta->width, smeta->height,
        dmeta->width, dmeta->height);
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

GType
gst_video_crop_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_VIDEO_SIZE_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVideoCropMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_crop_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVideoCropMeta *emeta = (GstVideoCropMeta *) meta;
  emeta->x = emeta->y = emeta->width = emeta->height = 0;

  return TRUE;
}

const GstMetaInfo *
gst_video_crop_meta_get_info (void)
{
  static const GstMetaInfo *video_crop_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & video_crop_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VIDEO_CROP_META_API_TYPE, "GstVideoCropMeta",
        sizeof (GstVideoCropMeta),
        (GstMetaInitFunction) gst_video_crop_meta_init,
        (GstMetaFreeFunction) NULL, gst_video_crop_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & video_crop_meta_info,
        (GstMetaInfo *) meta);
  }
  return video_crop_meta_info;
}

/**
 * gst_video_meta_transform_scale_get_quark:
 *
 * Get the #GQuark for the "gst-video-scale" metadata transform operation.
 *
 * Returns: a #GQuark
 */
GQuark
gst_video_meta_transform_scale_get_quark (void)
{
  static GQuark _value = 0;

  if (_value == 0) {
    _value = g_quark_from_static_string ("gst-video-scale");
  }
  return _value;
}


GType
gst_video_gl_texture_upload_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type =
        gst_meta_api_type_register ("GstVideoGLTextureUploadMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_gl_texture_upload_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoGLTextureUploadMeta *vmeta = (GstVideoGLTextureUploadMeta *) meta;

  vmeta->texture_orientation =
      GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_NORMAL;
  vmeta->n_textures = 0;
  memset (vmeta->texture_type, 0, sizeof (vmeta->texture_type));
  vmeta->buffer = NULL;
  vmeta->upload = NULL;
  vmeta->user_data = NULL;
  vmeta->user_data_copy = NULL;
  vmeta->user_data_free = NULL;

  return TRUE;
}

static void
gst_video_gl_texture_upload_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVideoGLTextureUploadMeta *vmeta = (GstVideoGLTextureUploadMeta *) meta;

  if (vmeta->user_data_free)
    vmeta->user_data_free (vmeta->user_data);
}

static gboolean
gst_video_gl_texture_upload_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoGLTextureUploadMeta *dmeta, *smeta;

  smeta = (GstVideoGLTextureUploadMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta =
          (GstVideoGLTextureUploadMeta *) gst_buffer_add_meta (dest,
          GST_VIDEO_GL_TEXTURE_UPLOAD_META_INFO, NULL);

      if (!dmeta)
        return FALSE;

      dmeta->texture_orientation = smeta->texture_orientation;
      dmeta->n_textures = smeta->n_textures;
      memcpy (dmeta->texture_type, smeta->texture_type,
          sizeof (smeta->texture_type[0]) * 4);
      dmeta->buffer = dest;
      dmeta->upload = smeta->upload;
      dmeta->user_data = smeta->user_data;
      dmeta->user_data_copy = smeta->user_data_copy;
      dmeta->user_data_free = smeta->user_data_free;
      if (dmeta->user_data_copy)
        dmeta->user_data = dmeta->user_data_copy (dmeta->user_data);
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

const GstMetaInfo *
gst_video_gl_texture_upload_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE,
        "GstVideoGLTextureUploadMeta",
        sizeof (GstVideoGLTextureUploadMeta),
        gst_video_gl_texture_upload_meta_init,
        gst_video_gl_texture_upload_meta_free,
        gst_video_gl_texture_upload_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & info, (GstMetaInfo *) meta);
  }
  return info;
}

/**
 * gst_buffer_add_video_gl_texture_upload_meta:
 * @buffer: a #GstBuffer
 * @texture_orientation: the #GstVideoGLTextureOrientation
 * @n_textures: the number of textures
 * @texture_type: array of #GstVideoGLTextureType
 * @upload: (scope call): the function to upload the buffer to a specific texture ID
 * @user_data: user data for the implementor of @upload
 * @user_data_copy: (scope call): function to copy @user_data
 * @user_data_free: (scope call): function to free @user_data
 *
 * Attaches GstVideoGLTextureUploadMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoGLTextureUploadMeta on @buffer.
 */
GstVideoGLTextureUploadMeta *
gst_buffer_add_video_gl_texture_upload_meta (GstBuffer * buffer,
    GstVideoGLTextureOrientation texture_orientation, guint n_textures,
    GstVideoGLTextureType texture_type[4], GstVideoGLTextureUpload upload,
    gpointer user_data, GBoxedCopyFunc user_data_copy,
    GBoxedFreeFunc user_data_free)
{
  GstVideoGLTextureUploadMeta *meta;

  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (upload != NULL, NULL);
  g_return_val_if_fail (n_textures > 0 && n_textures < 5, NULL);

  meta =
      (GstVideoGLTextureUploadMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_GL_TEXTURE_UPLOAD_META_INFO, NULL);

  if (!meta)
    return NULL;

  meta->texture_orientation = texture_orientation;
  meta->n_textures = n_textures;
  memcpy (meta->texture_type, texture_type, sizeof (texture_type[0]) * 4);
  meta->buffer = buffer;
  meta->upload = upload;
  meta->user_data = user_data;
  meta->user_data_copy = user_data_copy;
  meta->user_data_free = user_data_free;

  return meta;
}

/**
 * gst_video_gl_texture_upload_meta_upload:
 * @meta: a #GstVideoGLTextureUploadMeta
 * @texture_id: the texture IDs to upload to
 *
 * Uploads the buffer which owns the meta to a specific texture ID.
 *
 * Returns: %TRUE if uploading succeeded, %FALSE otherwise.
 */
gboolean
gst_video_gl_texture_upload_meta_upload (GstVideoGLTextureUploadMeta * meta,
    guint texture_id[4])
{
  g_return_val_if_fail (meta != NULL, FALSE);

  return meta->upload (meta, texture_id);
}

/* Region of Interest Meta implementation *******************************************/

GType
gst_video_region_of_interest_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_SIZE_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type =
        gst_meta_api_type_register ("GstVideoRegionOfInterestMetaAPI", tags);
    GST_INFO ("registering");
    g_once_init_leave (&type, _type);
  }
  return type;
}


static gboolean
gst_video_region_of_interest_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoRegionOfInterestMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstVideoRegionOfInterestMeta *) meta;

    GST_DEBUG ("copy region of interest metadata");
    dmeta =
        gst_buffer_add_video_region_of_interest_meta_id (dest,
        smeta->roi_type, smeta->x, smeta->y, smeta->w, smeta->h);
    if (!dmeta)
      return FALSE;

    dmeta->id = smeta->id;
    dmeta->parent_id = smeta->parent_id;
    dmeta->params = g_list_copy_deep (smeta->params,
        (GCopyFunc) gst_structure_copy, NULL);
  } else if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstVideoMetaTransform *trans = data;
    gint ow, oh, nw, nh;
    ow = GST_VIDEO_INFO_WIDTH (trans->in_info);
    nw = GST_VIDEO_INFO_WIDTH (trans->out_info);
    oh = GST_VIDEO_INFO_HEIGHT (trans->in_info);
    nh = GST_VIDEO_INFO_HEIGHT (trans->out_info);
    GST_DEBUG ("scaling region of interest metadata %dx%d -> %dx%d", ow, oh, nw,
        nh);

    smeta = (GstVideoRegionOfInterestMeta *) meta;
    dmeta =
        gst_buffer_add_video_region_of_interest_meta_id (dest,
        smeta->roi_type, (smeta->x * nw) / ow, (smeta->y * nh) / oh,
        (smeta->w * nw) / ow, (smeta->h * nh) / oh);
    if (!dmeta)
      return FALSE;

    dmeta->id = smeta->id;
    dmeta->parent_id = smeta->parent_id;

    GST_DEBUG ("region of interest (id:%d, parent id:%d) offset %dx%d -> %dx%d",
        smeta->id, smeta->parent_id, smeta->x, smeta->y, dmeta->x, dmeta->y);
    GST_DEBUG ("region of interest size   %dx%d -> %dx%d", smeta->w, smeta->h,
        dmeta->w, dmeta->h);
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_video_region_of_interest_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoRegionOfInterestMeta *emeta = (GstVideoRegionOfInterestMeta *) meta;
  emeta->roi_type = 0;
  emeta->id = 0;
  emeta->parent_id = 0;
  emeta->x = emeta->y = emeta->w = emeta->h = 0;
  emeta->params = NULL;

  return TRUE;
}

static void
gst_video_region_of_interest_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVideoRegionOfInterestMeta *emeta = (GstVideoRegionOfInterestMeta *) meta;

  g_list_free_full (emeta->params, (GDestroyNotify) gst_structure_free);
}

const GstMetaInfo *
gst_video_region_of_interest_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi =
        gst_meta_register (GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE,
        "GstVideoRegionOfInterestMeta",
        sizeof (GstVideoRegionOfInterestMeta),
        gst_video_region_of_interest_meta_init,
        gst_video_region_of_interest_meta_free,
        gst_video_region_of_interest_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_get_video_region_of_interest_meta_id:
 * @buffer: a #GstBuffer
 * @id: a metadata id
 *
 * Find the #GstVideoRegionOfInterestMeta on @buffer with the given @id.
 *
 * Buffers can contain multiple #GstVideoRegionOfInterestMeta metadata items if
 * multiple regions of interests are marked on a frame.
 *
 * Returns: (transfer none) (nullable): the #GstVideoRegionOfInterestMeta with @id or %NULL when there is
 * no such metadata on @buffer.
 */
GstVideoRegionOfInterestMeta *
gst_buffer_get_video_region_of_interest_meta_id (GstBuffer * buffer, gint id)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_VIDEO_REGION_OF_INTEREST_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      GstVideoRegionOfInterestMeta *vmeta =
          (GstVideoRegionOfInterestMeta *) meta;
      if (vmeta->id == id)
        return vmeta;
    }
  }
  return NULL;
}

/**
 * gst_buffer_add_video_region_of_interest_meta:
 * @buffer: a #GstBuffer
 * @roi_type: Type of the region of interest (e.g. "face")
 * @x: X position
 * @y: Y position
 * @w: width
 * @h: height
 *
 * Attaches #GstVideoRegionOfInterestMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoRegionOfInterestMeta on @buffer.
 */
GstVideoRegionOfInterestMeta *
gst_buffer_add_video_region_of_interest_meta (GstBuffer * buffer,
    const gchar * roi_type, guint x, guint y, guint w, guint h)
{
  return gst_buffer_add_video_region_of_interest_meta_id (buffer,
      g_quark_from_string (roi_type), x, y, w, h);
}

/**
 * gst_buffer_add_video_region_of_interest_meta_id:
 * @buffer: a #GstBuffer
 * @roi_type: Type of the region of interest (e.g. "face")
 * @x: X position
 * @y: Y position
 * @w: width
 * @h: height
 *
 * Attaches #GstVideoRegionOfInterestMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoRegionOfInterestMeta on @buffer.
 */
GstVideoRegionOfInterestMeta *
gst_buffer_add_video_region_of_interest_meta_id (GstBuffer * buffer,
    GQuark roi_type, guint x, guint y, guint w, guint h)
{
  GstVideoRegionOfInterestMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (GstVideoRegionOfInterestMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_REGION_OF_INTEREST_META_INFO, NULL);
  meta->roi_type = roi_type;
  meta->x = x;
  meta->y = y;
  meta->w = w;
  meta->h = h;

  return meta;
}

/**
 * gst_video_region_of_interest_meta_add_param:
 * @meta: a #GstVideoRegionOfInterestMeta
 * @s: (transfer full): a #GstStructure
 *
 * Attach element-specific parameters to @meta meant to be used by downstream
 * elements which may handle this ROI.
 * The name of @s is used to identify the element these parameters are meant for.
 *
 * This is typically used to tell encoders how they should encode this specific region.
 * For example, a structure named "roi/x264enc" could be used to give the
 * QP offsets this encoder should use when encoding the region described in @meta.
 * Multiple parameters can be defined for the same meta so different encoders
 * can be supported by cross platform applications).
 *
 * Since: 1.14
 */
void
gst_video_region_of_interest_meta_add_param (GstVideoRegionOfInterestMeta *
    meta, GstStructure * s)
{
  g_return_if_fail (meta);
  g_return_if_fail (s);

  meta->params = g_list_append (meta->params, s);
}

/**
 * gst_video_region_of_interest_meta_get_param:
 * @meta: a #GstVideoRegionOfInterestMeta
 * @name: a name.
 *
 * Retrieve the parameter for @meta having @name as structure name,
 * or %NULL if there is none.
 *
 * Returns: (transfer none) (nullable): a #GstStructure
 *
 * Since: 1.14
 * See also: gst_video_region_of_interest_meta_add_param()
 */
GstStructure *
gst_video_region_of_interest_meta_get_param (GstVideoRegionOfInterestMeta *
    meta, const gchar * name)
{
  GList *l;

  g_return_val_if_fail (meta, NULL);
  g_return_val_if_fail (name, NULL);

  for (l = meta->params; l; l = g_list_next (l)) {
    GstStructure *s = l->data;

    if (gst_structure_has_name (s, name))
      return s;
  }

  return NULL;
}

/* Time Code Meta implementation *******************************************/

GType
gst_video_time_code_meta_api_get_type (void)
{
  static GType type;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = { NULL };
    GType _type = gst_meta_api_type_register ("GstVideoTimeCodeMetaAPI", tags);
    GST_INFO ("registering");
    g_once_init_leave (&type, _type);
  }
  return type;
}


static gboolean
gst_video_time_code_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoTimeCodeMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstVideoTimeCodeMeta *) meta;

    GST_DEBUG ("copy time code metadata");
    dmeta =
        gst_buffer_add_video_time_code_meta_full (dest, smeta->tc.config.fps_n,
        smeta->tc.config.fps_d, smeta->tc.config.latest_daily_jam,
        smeta->tc.config.flags, smeta->tc.hours, smeta->tc.minutes,
        smeta->tc.seconds, smeta->tc.frames, smeta->tc.field_count);
    if (!dmeta)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_video_time_code_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoTimeCodeMeta *emeta = (GstVideoTimeCodeMeta *) meta;
  memset (&emeta->tc, 0, sizeof (emeta->tc));
  gst_video_time_code_clear (&emeta->tc);

  return TRUE;
}

static void
gst_video_time_code_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVideoTimeCodeMeta *emeta = (GstVideoTimeCodeMeta *) meta;

  gst_video_time_code_clear (&emeta->tc);
}

const GstMetaInfo *
gst_video_time_code_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi =
        gst_meta_register (GST_VIDEO_TIME_CODE_META_API_TYPE,
        "GstVideoTimeCodeMeta",
        sizeof (GstVideoTimeCodeMeta),
        gst_video_time_code_meta_init,
        gst_video_time_code_meta_free,
        gst_video_time_code_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_video_time_code_meta:
 * @buffer: a #GstBuffer
 * @tc: a #GstVideoTimeCode
 *
 * Attaches #GstVideoTimeCodeMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none) (nullable): the #GstVideoTimeCodeMeta on @buffer, or
 * (since 1.16) %NULL if the timecode was invalid.
 *
 * Since: 1.10
 */
GstVideoTimeCodeMeta *
gst_buffer_add_video_time_code_meta (GstBuffer * buffer,
    const GstVideoTimeCode * tc)
{
  if (!gst_video_time_code_is_valid (tc))
    return NULL;

  return gst_buffer_add_video_time_code_meta_full (buffer, tc->config.fps_n,
      tc->config.fps_d, tc->config.latest_daily_jam, tc->config.flags,
      tc->hours, tc->minutes, tc->seconds, tc->frames, tc->field_count);
}

/**
 * gst_buffer_add_video_time_code_meta_full:
 * @buffer: a #GstBuffer
 * @fps_n: framerate numerator
 * @fps_d: framerate denominator
 * @latest_daily_jam: a #GDateTime for the latest daily jam
 * @flags: a #GstVideoTimeCodeFlags
 * @hours: hours since the daily jam
 * @minutes: minutes since the daily jam
 * @seconds: seconds since the daily jam
 * @frames: frames since the daily jam
 * @field_count: fields since the daily jam
 *
 * Attaches #GstVideoTimeCodeMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none) (nullable): the #GstVideoTimeCodeMeta on @buffer, or
 * (since 1.16) %NULL if the timecode was invalid.
 *
 * Since: 1.10
 */
GstVideoTimeCodeMeta *
gst_buffer_add_video_time_code_meta_full (GstBuffer * buffer, guint fps_n,
    guint fps_d, GDateTime * latest_daily_jam, GstVideoTimeCodeFlags flags,
    guint hours, guint minutes, guint seconds, guint frames, guint field_count)
{
  GstVideoTimeCodeMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (GstVideoTimeCodeMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_TIME_CODE_META_INFO, NULL);
  g_return_val_if_fail (meta != NULL, NULL);

  gst_video_time_code_init (&meta->tc, fps_n, fps_d, latest_daily_jam, flags,
      hours, minutes, seconds, frames, field_count);

  if (!gst_video_time_code_is_valid (&meta->tc)) {
    gst_buffer_remove_meta (buffer, (GstMeta *) meta);
    return NULL;
  }

  return meta;
}
