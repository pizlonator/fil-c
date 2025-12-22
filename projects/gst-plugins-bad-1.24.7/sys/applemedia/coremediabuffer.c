/*
 * Copyright (C) 2009 Ole André Vadla Ravnås <oleavr@soundrop.com>
 * Copyright (C) 2014 Collabora Ltd.
 *   Authors:    Matthieu Bouron <matthieu.bouron@collabora.com>
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

#include "corevideobuffer.h"
#include "coremediabuffer.h"
#include "corevideomemory.h"

static const GstMetaInfo *gst_core_media_meta_get_info (void);

static void
gst_core_media_meta_add (GstBuffer * buffer, CMSampleBufferRef sample_buf,
    CVImageBufferRef image_buf, CMBlockBufferRef block_buf)
{
  GstCoreMediaMeta *meta;

  meta =
      (GstCoreMediaMeta *) gst_buffer_add_meta (buffer,
      gst_core_media_meta_get_info (), NULL);
  CFRetain (sample_buf);
  if (image_buf)
    CVBufferRetain (image_buf);
  if (block_buf)
    CFRetain (block_buf);
  meta->sample_buf = sample_buf;
  meta->image_buf = image_buf;
  meta->block_buf = block_buf;
  if (image_buf != NULL && CFGetTypeID (image_buf) == CVPixelBufferGetTypeID ())
    meta->pixel_buf = (CVPixelBufferRef) image_buf;
  else
    meta->pixel_buf = NULL;
}

static gboolean
gst_core_media_meta_init (GstCoreMediaMeta * meta, gpointer params,
    GstBuffer * buf)
{
  meta->sample_buf = NULL;
  meta->image_buf = NULL;
  meta->pixel_buf = NULL;
  meta->block_buf = NULL;

  return TRUE;
}

static void
gst_core_media_meta_free (GstCoreMediaMeta * meta, GstBuffer * buf)
{
  if (meta->image_buf != NULL) {
    CVBufferRelease (meta->image_buf);
  }

  if (meta->block_buf != NULL) {
    CFRelease (meta->block_buf);
  }

  CFRelease (meta->sample_buf);
}

static gboolean
gst_core_media_meta_transform (GstBuffer * transbuf, GstCoreMediaMeta * meta,
    GstBuffer * buffer, GQuark type, GstMetaTransformCopy * data)
{
  if (!data->region) {
    /* only copy if the complete data is copied as well */
    gst_core_media_meta_add (transbuf, meta->sample_buf, meta->image_buf,
        meta->block_buf);
  } else {
    GST_WARNING_OBJECT (transbuf,
        "dropping Core Media metadata due to partial buffer");
  }

  return TRUE;                  /* retval unused */
}

GType
gst_core_media_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstCoreMediaMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static const GstMetaInfo *
gst_core_media_meta_get_info (void)
{
  static const GstMetaInfo *core_media_meta_info = NULL;

  if (g_once_init_enter (&core_media_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_CORE_MEDIA_META_API_TYPE,
        "GstCoreMediaMeta", sizeof (GstCoreMediaMeta),
        (GstMetaInitFunction) gst_core_media_meta_init,
        (GstMetaFreeFunction) gst_core_media_meta_free,
        (GstMetaTransformFunction) gst_core_media_meta_transform);
    g_once_init_leave (&core_media_meta_info, meta);
  }
  return core_media_meta_info;
}

static GstVideoFormat
gst_core_media_buffer_get_video_format (OSType format)
{
  switch (format) {
    case kCVPixelFormatType_420YpCbCr8Planar:
      return GST_VIDEO_FORMAT_I420;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return GST_VIDEO_FORMAT_NV12;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
      return GST_VIDEO_FORMAT_YUY2;
    case kCVPixelFormatType_422YpCbCr8:
      return GST_VIDEO_FORMAT_UYVY;
    case kCVPixelFormatType_32BGRA:
      return GST_VIDEO_FORMAT_BGRA;
    case kCVPixelFormatType_32RGBA:
      return GST_VIDEO_FORMAT_RGBA;
    default:
      GST_WARNING ("Unknown OSType format: %d", (gint) format);
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

static gboolean
gst_core_media_buffer_wrap_block_buffer (GstBuffer * buf,
    CMBlockBufferRef block_buf)
{
  OSStatus status;
  gchar *data = NULL;
  size_t offset = 0, length_at_offset, total_length;

  /* CMBlockBuffer can contain multiple non-continuous memory blocks */
  do {
    status =
        CMBlockBufferGetDataPointer (block_buf, offset, &length_at_offset,
        &total_length, &data);
    if (status != kCMBlockBufferNoErr) {
      return FALSE;
    }

    /* retaining the CMBlockBuffer so it won't go away for the lifetime of the GstMemory */
    gst_buffer_append_memory (buf,
        gst_memory_new_wrapped (0, data, length_at_offset, 0, length_at_offset,
            (gpointer) CFRetain (block_buf), (GDestroyNotify) CFRelease));

    offset += length_at_offset;
  } while (offset < total_length);

  return TRUE;
}

static GstBuffer *
gst_core_media_buffer_new_from_buffer (GstBuffer * buf, GstVideoInfo * info)
{
  gboolean ret;
  GstBuffer *copy_buf;
  GstVideoFrame dest, src;
  GstAllocator *allocator;

  allocator = gst_allocator_find (GST_ALLOCATOR_SYSMEM);
  if (!allocator) {
    GST_ERROR ("Could not find SYSMEM allocator");
    return NULL;
  }

  copy_buf = gst_buffer_new_allocate (allocator, info->size, NULL);

  gst_object_unref (allocator);

  if (!gst_video_frame_map (&dest, info, copy_buf, GST_MAP_WRITE)) {
    GST_ERROR ("Could not map destination frame");
    goto error;
  }

  if (!gst_video_frame_map (&src, info, buf, GST_MAP_READ)) {
    GST_ERROR ("Could not map source frame");
    gst_video_frame_unmap (&dest);
    goto error;
  }

  ret = gst_video_frame_copy (&dest, &src);

  gst_video_frame_unmap (&dest);
  gst_video_frame_unmap (&src);

  if (!ret) {
    GST_ERROR ("Could not copy frame");
    goto error;
  }

  return copy_buf;

error:
  if (copy_buf) {
    gst_buffer_unref (copy_buf);
  }
  return NULL;
}

static gboolean
gst_video_info_init_from_pixel_buffer (GstVideoInfo * info,
    CVPixelBufferRef pixel_buf)
{
  size_t width, height;
  OSType format_type;
  GstVideoFormat video_format;

  width = CVPixelBufferGetWidth (pixel_buf);
  height = CVPixelBufferGetHeight (pixel_buf);
  format_type = CVPixelBufferGetPixelFormatType (pixel_buf);
  video_format = gst_core_media_buffer_get_video_format (format_type);

  if (video_format == GST_VIDEO_FORMAT_UNKNOWN) {
    return FALSE;
  }

  gst_video_info_init (info);
  gst_video_info_set_format (info, video_format, width, height);

  return TRUE;
}

GstBuffer *
gst_core_media_buffer_new (CMSampleBufferRef sample_buf,
    gboolean use_video_meta, GstVideoTextureCache * cache)
{
  CVImageBufferRef image_buf;
  CMBlockBufferRef block_buf;
  GstBuffer *buf;

  image_buf = CMSampleBufferGetImageBuffer (sample_buf);
  block_buf = CMSampleBufferGetDataBuffer (sample_buf);

  buf = gst_buffer_new ();

  gst_core_media_meta_add (buf, sample_buf, image_buf, block_buf);

  if (image_buf != NULL && CFGetTypeID (image_buf) == CVPixelBufferGetTypeID ()) {
    GstVideoInfo info;
    gboolean has_padding = FALSE;
    CVPixelBufferRef pixel_buf = (CVPixelBufferRef) image_buf;

    if (!gst_video_info_init_from_pixel_buffer (&info, pixel_buf)) {
      goto error;
    }

    gst_core_video_wrap_pixel_buffer (buf, &info, pixel_buf, cache,
        &has_padding);

    /* If the video meta API is not supported, remove padding by
     * copying the core media buffer to a system memory buffer */
    if (has_padding && !use_video_meta) {
      GstBuffer *copy_buf;
      copy_buf = gst_core_media_buffer_new_from_buffer (buf, &info);
      if (!copy_buf) {
        goto error;
      }

      gst_buffer_unref (buf);
      buf = copy_buf;
    }

  } else if (block_buf != NULL) {
    if (!gst_core_media_buffer_wrap_block_buffer (buf, block_buf)) {
      goto error;
    }
  } else {
    goto error;
  }

  return buf;

error:
  if (buf) {
    gst_buffer_unref (buf);
  }
  return NULL;
}

CVPixelBufferRef
gst_core_media_buffer_get_pixel_buffer (GstBuffer * buf)
{
  GstCoreMediaMeta *meta = (GstCoreMediaMeta *) gst_buffer_get_meta (buf,
      GST_CORE_MEDIA_META_API_TYPE);
  g_return_val_if_fail (meta != NULL, NULL);

  return CVPixelBufferRetain (meta->pixel_buf);
}
