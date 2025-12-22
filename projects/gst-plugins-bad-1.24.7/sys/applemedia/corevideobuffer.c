/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@soundrop.com>
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
#  include "config.h"
#endif
#include "corevideobuffer.h"
#include "corevideomemory.h"
#if !HAVE_IOS
#include "iosurfaceglmemory.h"
#endif
#include "videotexturecache-gl.h"
#if defined(APPLEMEDIA_MOLTENVK)
#include "videotexturecache-vulkan.h"
#if !HAVE_IOS
#include "iosurfacevulkanmemory.h"
#endif
#endif

static const GstMetaInfo *gst_core_video_meta_get_info (void);

static void
gst_core_video_meta_add (GstBuffer * buffer, CVBufferRef cvbuf)
{
  GstCoreVideoMeta *meta;

  meta = (GstCoreVideoMeta *) gst_buffer_add_meta (buffer,
      gst_core_video_meta_get_info (), NULL);
  meta->cvbuf = CVBufferRetain (cvbuf);
  meta->pixbuf = (CVPixelBufferRef) cvbuf;
}

static gboolean
gst_core_video_meta_init (GstCoreVideoMeta * meta, gpointer params,
    GstBuffer * buf)
{
  meta->cvbuf = NULL;
  meta->pixbuf = NULL;

  return TRUE;
}

static void
gst_core_video_meta_free (GstCoreVideoMeta * meta, GstBuffer * buf)
{
  CVBufferRelease (meta->cvbuf);
}

static gboolean
gst_core_video_meta_transform (GstBuffer * transbuf, GstCoreVideoMeta * meta,
    GstBuffer * buffer, GQuark type, GstMetaTransformCopy * data)
{
  if (!data->region) {
    /* only copy if the complete data is copied as well */
    gst_core_video_meta_add (transbuf, meta->cvbuf);
  } else {
    GST_WARNING_OBJECT (transbuf,
        "dropping Core Video metadata due to partial buffer");
  }

  return TRUE;                  /* retval unused */
}

GType
gst_core_video_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstCoreVideoMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static const GstMetaInfo *
gst_core_video_meta_get_info (void)
{
  static const GstMetaInfo *core_video_meta_info = NULL;

  if (g_once_init_enter (&core_video_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_CORE_VIDEO_META_API_TYPE,
        "GstCoreVideoMeta", sizeof (GstCoreVideoMeta),
        (GstMetaInitFunction) gst_core_video_meta_init,
        (GstMetaFreeFunction) gst_core_video_meta_free,
        (GstMetaTransformFunction) gst_core_video_meta_transform);
    g_once_init_leave (&core_video_meta_info, meta);
  }
  return core_video_meta_info;
}

static GstMemory *
_create_glmem (GstAppleCoreVideoPixelBuffer * gpixbuf,
    GstVideoInfo * info, guint plane, gsize size, GstVideoTextureCache * cache)
{
#if HAVE_IOS
  return gst_video_texture_cache_create_memory (cache, gpixbuf, plane, size);
#else
  GstIOSurfaceGLMemory *mem;
  CVPixelBufferRef pixel_buf = gpixbuf->buf;
  IOSurfaceRef surface = CVPixelBufferGetIOSurface (pixel_buf);
  GstGLFormat tex_format;
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (cache);

  tex_format = gst_gl_format_from_video_info (cache_gl->ctx, info, plane);

  CFRetain (pixel_buf);
  mem = gst_io_surface_gl_memory_wrapped (cache_gl->ctx,
      surface, GST_GL_TEXTURE_TARGET_RECTANGLE, tex_format,
      info, plane, NULL, pixel_buf, (GDestroyNotify) CFRelease);
  return GST_MEMORY_CAST (mem);
#endif
}

#if defined(APPLEMEDIA_MOLTENVK)
/* in videotexturecache-vulkan.m to avoid objc-ism from Metal being included
  * in a non-objc file */
extern GstMemory *_create_vulkan_memory (GstAppleCoreVideoPixelBuffer * gpixbuf,
    GstVideoInfo * info, guint plane, gsize size, GstVideoTextureCache * cache);
#endif

void
gst_core_video_wrap_pixel_buffer (GstBuffer * buf,
    GstVideoInfo * info,
    CVPixelBufferRef pixel_buf,
    GstVideoTextureCache * cache, gboolean * has_padding)
{
  guint n_planes;
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0 };
  gint stride[GST_VIDEO_MAX_PLANES] = { 0 };
  UInt32 size;
  GstAppleCoreVideoPixelBuffer *gpixbuf;
  GstMemory *mem = NULL;
  gboolean do_gl = GST_IS_VIDEO_TEXTURE_CACHE_GL (cache);
#if defined(APPLEMEDIA_MOLTENVK)
  gboolean do_vulkan = GST_IS_VIDEO_TEXTURE_CACHE_VULKAN (cache);
#endif

  gpixbuf = gst_apple_core_video_pixel_buffer_new (pixel_buf);

  if (has_padding)
    *has_padding = FALSE;

  if (CVPixelBufferIsPlanar (pixel_buf)) {
    gint i, size = 0, plane_offset = 0;

    n_planes = CVPixelBufferGetPlaneCount (pixel_buf);
    for (i = 0; i < n_planes; i++) {
      stride[i] = CVPixelBufferGetBytesPerRowOfPlane (pixel_buf, i);

      if (stride[i] != GST_VIDEO_INFO_PLANE_STRIDE (info, i) && has_padding)
        *has_padding = TRUE;

      size = stride[i] * CVPixelBufferGetHeightOfPlane (pixel_buf, i);
      offset[i] = plane_offset;
      plane_offset += size;

      if (do_gl)
        mem = _create_glmem (gpixbuf, info, i, size, cache);
#if defined(APPLEMEDIA_MOLTENVK)
      else if (do_vulkan)
        mem = _create_vulkan_memory (gpixbuf, info, i, size, cache);
#endif
      else
        mem =
            GST_MEMORY_CAST (gst_apple_core_video_memory_new_wrapped (gpixbuf,
                i, size));
      gst_buffer_append_memory (buf, mem);
    }
  } else {
    n_planes = 1;
    stride[0] = CVPixelBufferGetBytesPerRow (pixel_buf);
    offset[0] = 0;
    size = stride[0] * CVPixelBufferGetHeight (pixel_buf);

    if (do_gl)
      mem = _create_glmem (gpixbuf, info, 0, size, cache);
#if defined(APPLEMEDIA_MOLTENVK)
    else if (do_vulkan)
      mem = _create_vulkan_memory (gpixbuf, info, 0, size, cache);
#endif
    else
      mem =
          GST_MEMORY_CAST (gst_apple_core_video_memory_new_wrapped (gpixbuf, 0,
              size));
    gst_buffer_append_memory (buf, mem);
  }

  gst_apple_core_video_pixel_buffer_unref (gpixbuf);

  if (info) {
    gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (info), info->width, info->height, n_planes,
        offset, stride);
  }
}

static GstVideoFormat
gst_core_video_get_video_format (OSType format)
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


gboolean
gst_core_video_info_init_from_pixel_buffer (GstVideoInfo * info,
    CVPixelBufferRef pixel_buf)
{
  size_t width, height;
  OSType format_type;
  GstVideoFormat video_format;

  width = CVPixelBufferGetWidth (pixel_buf);
  height = CVPixelBufferGetHeight (pixel_buf);
  format_type = CVPixelBufferGetPixelFormatType (pixel_buf);
  video_format = gst_core_video_get_video_format (format_type);

  if (video_format == GST_VIDEO_FORMAT_UNKNOWN) {
    return FALSE;
  }

  gst_video_info_init (info);
  gst_video_info_set_format (info, video_format, width, height);

  return TRUE;
}


GstBuffer *
gst_core_video_buffer_new (CVBufferRef cvbuf, GstVideoInfo * vinfo,
    GstVideoTextureCache * cache)
{
  CVPixelBufferRef pixbuf = NULL;
  GstBuffer *buf;
  GstCoreVideoMeta *meta;

  if (CFGetTypeID (cvbuf) != CVPixelBufferGetTypeID ())
    /* TODO: Do we need to handle other buffer types? */
    return NULL;

  pixbuf = (CVPixelBufferRef) cvbuf;

  buf = gst_buffer_new ();

  /* add the corevideo meta to pass the underlying corevideo buffer */
  meta = (GstCoreVideoMeta *) gst_buffer_add_meta (buf,
      gst_core_video_meta_get_info (), NULL);
  meta->cvbuf = CVBufferRetain (cvbuf);
  meta->pixbuf = pixbuf;

  gst_core_video_wrap_pixel_buffer (buf, vinfo, pixbuf, cache, NULL);

  return buf;
}
