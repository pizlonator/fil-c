/* GStreamer Wayland Library
 *
 * Copyright (C) 2017 Collabora Ltd.
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
#include <config.h>
#endif

#include "gstwlvideobufferpool.h"

#include <gst/allocators/allocators.h>
#include <drm_fourcc.h>

GST_DEBUG_CATEGORY (gst_wl_videobufferpool_debug);
#define GST_CAT_DEFAULT gst_wl_videobufferpool_debug

struct _GstWlVideoBufferPool
{
  GstVideoBufferPool parent;
  GstAllocator *allocator;
  GstVideoInfo vinfo;
};

#define gst_wl_video_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWlVideoBufferPool, gst_wl_video_buffer_pool,
    GST_TYPE_VIDEO_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_wl_videobufferpool_debug,
        "wl_videobufferpool", 0, "wl_dmabuf library"));

static void
gst_wl_update_video_info_from_pitch (GstVideoInfo * vinfo, gint n_planes,
    guint32 pitch)
{
  gint i;
  gsize offs = 0;
  guint32 height = GST_VIDEO_INFO_HEIGHT (vinfo);

  if (!pitch)
    return;

  for (i = 0; i < n_planes; i++) {
    guint32 plane_pitch;

    /* Overwrite the video info's stride and offset using the pitch calculcated
     * by the kms driver. */
    plane_pitch = gst_video_format_info_extrapolate_stride (vinfo->finfo, i,
        pitch);
    GST_VIDEO_INFO_PLANE_STRIDE (vinfo, i) = plane_pitch;
    GST_VIDEO_INFO_PLANE_OFFSET (vinfo, i) = offs;

    /* Note that we cannot negotiate special padding betweem each planes,
     * hence using the display height here. */
    offs += plane_pitch
        * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (vinfo->finfo, i, height);

    GST_DEBUG ("Updated plane %i with stride %i and "
        "offset %" G_GSIZE_FORMAT "(from pitch %u)", i,
        GST_VIDEO_INFO_PLANE_STRIDE (vinfo, i),
        GST_VIDEO_INFO_PLANE_OFFSET (vinfo, i), pitch);
  }

  /* Update with the size use for display, excluding any padding at the end */
  GST_VIDEO_INFO_SIZE (vinfo) = offs;
}

static gboolean
gst_wl_video_buffer_pool_start (GstBufferPool * pool)
{
  GstWlVideoBufferPool *self = GST_WL_VIDEO_BUFFER_POOL (pool);
  GstStructure *config = gst_buffer_pool_get_config (pool);
  GstCaps *caps;
  GstAllocator *allocator;
  gboolean ret = FALSE;
  GstVideoFormat format;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL))
    goto wrong_config;

  if (!gst_buffer_pool_config_get_allocator (config, &allocator, NULL))
    goto wrong_config;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&self->vinfo, caps))
    goto wrong_caps;

  format = GST_VIDEO_INFO_FORMAT (&self->vinfo);
  if (!gst_video_format_to_wl_dmabuf_format (format))
    goto unsupported_pixel_format;

  if (GST_IS_DRM_DUMB_ALLOCATOR (allocator)) {
    if (!gst_drm_dumb_allocator_has_prime_export (allocator))
      goto no_prime_export;

    self->allocator = gst_object_ref (allocator);
  }

  /* all good */
  ret = GST_BUFFER_POOL_CLASS (parent_class)->start (pool);

done:
  gst_structure_free (config);
  return ret;

wrong_config:
  {
    GST_WARNING_OBJECT (pool, "invalid config");
    goto done;
  }
wrong_caps:
  {
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    goto done;
  }
unsupported_pixel_format:
  {
    GST_WARNING_OBJECT (pool, "no support for %s pixel format",
        gst_video_format_to_string (format));
    goto done;
  }
no_prime_export:
  {
    GST_WARNING_OBJECT (self,
        "not using DRM Dumb allocator as it can't export DMABuf.");
    goto done;
  }
}

static gboolean
gst_wl_video_buffer_pool_stop (GstBufferPool * pool)
{
  GstWlVideoBufferPool *self = GST_WL_VIDEO_BUFFER_POOL (pool);
  gst_clear_object (&self->allocator);
  gst_video_info_init (&self->vinfo);
  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

static GstFlowReturn
gst_wl_video_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstWlVideoBufferPool *self = GST_WL_VIDEO_BUFFER_POOL (pool);
  guint32 pitch;
  GstBuffer *buf;
  GstMemory *drm_mem, *dma_mem;
  gsize dumbbuf_size;
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (&self->vinfo);
  guint32 drm_fourcc = gst_video_format_to_wl_dmabuf_format (format);

  if (!self->allocator)
    return GST_BUFFER_POOL_CLASS (parent_class)->alloc_buffer (pool, buffer,
        params);

  drm_mem = gst_drm_dumb_allocator_alloc (self->allocator,
      drm_fourcc, GST_VIDEO_INFO_WIDTH (&self->vinfo),
      GST_VIDEO_INFO_HEIGHT (&self->vinfo), &pitch);
  if (!drm_mem)
    goto alloc_failed;

  dma_mem = gst_drm_dumb_memory_export_dmabuf (drm_mem);

  gst_memory_unref (drm_mem);
  drm_mem = NULL;

  gst_wl_update_video_info_from_pitch (&self->vinfo,
      GST_VIDEO_INFO_N_PLANES (&self->vinfo), pitch);

  gst_memory_get_sizes (dma_mem, NULL, &dumbbuf_size);
  if (dumbbuf_size < GST_VIDEO_INFO_SIZE (&self->vinfo)) {
    GST_ERROR_OBJECT (self,
        "DUMB buffer has a size of %" G_GSIZE_FORMAT
        " but we require at least %" G_GSIZE_FORMAT " to hold a frame",
        dumbbuf_size, GST_VIDEO_INFO_SIZE (&self->vinfo));
    goto buffer_too_small;
  }

  gst_memory_resize (dma_mem, 0, GST_VIDEO_INFO_SIZE (&self->vinfo));

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, dma_mem);
  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&self->vinfo), GST_VIDEO_INFO_WIDTH (&self->vinfo),
      GST_VIDEO_INFO_HEIGHT (&self->vinfo),
      GST_VIDEO_INFO_N_PLANES (&self->vinfo),
      self->vinfo.offset, self->vinfo.stride);

  *buffer = buf;

  return GST_FLOW_OK;

  /* ERRORS */
alloc_failed:
  {
    GST_ERROR_OBJECT (self, "failed to allocate DRM Dumb buffer.");
    return GST_FLOW_ERROR;
  }
buffer_too_small:
  {
    GST_ERROR_OBJECT (self, "dumb buffer too small to store an image.");
    return GST_FLOW_ERROR;
  }
}

static const gchar **
gst_wl_video_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META, NULL };
  return options;
}

static void
gst_wl_video_buffer_pool_class_init (GstWlVideoBufferPoolClass * klass)
{
  GstBufferPoolClass *pool_class = GST_BUFFER_POOL_CLASS (klass);
  pool_class->get_options = gst_wl_video_buffer_pool_get_options;
  pool_class->start = gst_wl_video_buffer_pool_start;
  pool_class->stop = gst_wl_video_buffer_pool_stop;
  pool_class->alloc_buffer = gst_wl_video_buffer_pool_alloc_buffer;
}

static void
gst_wl_video_buffer_pool_init (GstWlVideoBufferPool * self)
{
  gst_video_info_init (&self->vinfo);
}

GstBufferPool *
gst_wl_video_buffer_pool_new (void)
{
  return (GstBufferPool *) g_object_new (GST_TYPE_WL_VIDEO_BUFFER_POOL, NULL);
}
