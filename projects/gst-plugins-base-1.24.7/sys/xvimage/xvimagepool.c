/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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

/* Object header */
#include "xvimagepool.h"
#include "xvimageallocator.h"

/* Debugging category */
#include <gst/gstinfo.h>

/* Helper functions */
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>


GST_DEBUG_CATEGORY (gst_debug_xv_image_pool);
#define GST_CAT_DEFAULT gst_debug_xv_image_pool

/* bufferpool */
static void gst_xvimage_buffer_pool_finalize (GObject * object);

#define gst_xvimage_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstXvImageBufferPool, gst_xvimage_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar **
xvimage_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT, NULL
  };

  return options;
}

static gboolean
xvimage_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstXvImageBufferPool *xvpool = GST_XVIMAGE_BUFFER_POOL_CAST (pool);
  GstVideoInfo info;
  GstCaps *caps;
  guint size, min_buffers, max_buffers;
  GstXvContext *context;
  GError *error = NULL;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps))
    goto wrong_caps;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  context = gst_xvimage_allocator_peek_context (xvpool->allocator);

  xvpool->im_format = gst_xvcontext_get_format_from_info (context, &info);
  if (xvpool->im_format == -1)
    goto unknown_format;

  if (xvpool->caps)
    gst_caps_unref (xvpool->caps);
  xvpool->caps = gst_caps_ref (caps);

  /* enable metadata based on config of the pool */
  xvpool->add_metavideo =
      gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);

  /* parse extra alignment info */
  xvpool->need_alignment = gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);

  if (xvpool->need_alignment) {
    gst_buffer_pool_config_get_video_alignment (config, &xvpool->align);

    GST_LOG_OBJECT (pool, "padding %u-%ux%u-%u", xvpool->align.padding_top,
        xvpool->align.padding_left, xvpool->align.padding_right,
        xvpool->align.padding_bottom);

    /* do padding and alignment */
    gst_video_info_align (&info, &xvpool->align);

    gst_buffer_pool_config_set_video_alignment (config, &xvpool->align);

    /* we need the video metadata too now */
    xvpool->add_metavideo = TRUE;
  } else {
    gst_video_alignment_reset (&xvpool->align);
  }

  /* add the padding */
  xvpool->padded_width =
      GST_VIDEO_INFO_WIDTH (&info) + xvpool->align.padding_left +
      xvpool->align.padding_right;
  xvpool->padded_height =
      GST_VIDEO_INFO_HEIGHT (&info) + xvpool->align.padding_top +
      xvpool->align.padding_bottom;

  xvpool->info = info;
  xvpool->crop.x = xvpool->align.padding_left;
  xvpool->crop.y = xvpool->align.padding_top;
  xvpool->crop.w = xvpool->info.width;
  xvpool->crop.h = xvpool->info.height;

  /* update offset, stride and size with actual xvimage buffer */
  if (xvpool->pre_alloc_mem)
    gst_memory_unref (xvpool->pre_alloc_mem);

  xvpool->pre_alloc_mem = gst_xvimage_allocator_alloc (xvpool->allocator,
      xvpool->im_format, &info, xvpool->padded_width,
      xvpool->padded_height, &xvpool->crop, &error);

  if (!xvpool->pre_alloc_mem) {
    GST_ERROR_OBJECT (pool, "Couldn't allocate image: %s", error->message);
    g_error_free (error);
    return FALSE;
  } else {
    gint i;
    XvImage *img;

    img = gst_xvimage_memory_get_xvimage ((GstXvImageMemory *)
        xvpool->pre_alloc_mem);

    info.size = img->data_size;

    for (i = 0; i < img->num_planes; i++) {
      info.stride[i] = img->pitches[i];
      info.offset[i] = img->offsets[i];
    }

    if (!gst_video_info_is_equal (&xvpool->info, &info) ||
        xvpool->info.size != info.size) {
      GST_WARNING_OBJECT (pool, "different size, stride and/or offset, update");
      xvpool->info = info;
    }
  }

  gst_buffer_pool_config_set_params (config, caps, info.size, min_buffers,
      max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);

  /* ERRORS */
wrong_config:
  {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }
no_caps:
  {
    GST_WARNING_OBJECT (pool, "no caps in config");
    return FALSE;
  }
wrong_caps:
  {
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
unknown_format:
  {
    GST_WARNING_OBJECT (pool, "failed to get format from caps %"
        GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

/* This function handles GstXImageBuffer creation depending on XShm availability */
static GstFlowReturn
xvimage_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstXvImageBufferPool *xvpool = GST_XVIMAGE_BUFFER_POOL_CAST (pool);
  GstVideoInfo *info;
  GstBuffer *xvimage;
  GstMemory *mem;
  GError *err = NULL;

  info = &xvpool->info;

  xvimage = gst_buffer_new ();

  if (xvpool->pre_alloc_mem) {
    mem = xvpool->pre_alloc_mem;
    xvpool->pre_alloc_mem = NULL;
  } else {
    mem = gst_xvimage_allocator_alloc (xvpool->allocator, xvpool->im_format,
        info, xvpool->padded_width, xvpool->padded_height, &xvpool->crop, &err);
  }

  if (mem == NULL) {
    gst_buffer_unref (xvimage);
    goto no_buffer;
  }
  gst_buffer_append_memory (xvimage, mem);

  if (xvpool->add_metavideo) {
    GstVideoMeta *meta;

    GST_DEBUG_OBJECT (pool, "adding GstVideoMeta");
    meta = gst_buffer_add_video_meta_full (xvimage, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
        GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
        info->offset, info->stride);

    gst_video_meta_set_alignment (meta, xvpool->align);
  }

  *buffer = xvimage;

  return GST_FLOW_OK;

  /* ERROR */
no_buffer:
  {
    GST_WARNING_OBJECT (pool, "can't create image: %s", err->message);
    g_clear_error (&err);
    return GST_FLOW_ERROR;
  }
}

GstBufferPool *
gst_xvimage_buffer_pool_new (GstXvImageAllocator * allocator)
{
  GstXvImageBufferPool *pool;

  pool = g_object_new (GST_TYPE_XVIMAGE_BUFFER_POOL, NULL);
  gst_object_ref_sink (pool);
  pool->allocator = gst_object_ref (allocator);

  GST_LOG_OBJECT (pool, "new XvImage buffer pool %p", pool);

  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_xvimage_buffer_pool_class_init (GstXvImageBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_xvimage_buffer_pool_finalize;

  gstbufferpool_class->get_options = xvimage_buffer_pool_get_options;
  gstbufferpool_class->set_config = xvimage_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = xvimage_buffer_pool_alloc;
}

static void
gst_xvimage_buffer_pool_init (GstXvImageBufferPool * pool)
{
  /* nothing to do here */
}

static void
gst_xvimage_buffer_pool_finalize (GObject * object)
{
  GstXvImageBufferPool *pool = GST_XVIMAGE_BUFFER_POOL_CAST (object);

  GST_LOG_OBJECT (pool, "finalize XvImage buffer pool %p", pool);

  if (pool->pre_alloc_mem)
    gst_memory_unref (pool->pre_alloc_mem);
  if (pool->caps)
    gst_caps_unref (pool->caps);
  if (pool->allocator)
    gst_object_unref (pool->allocator);

  G_OBJECT_CLASS (gst_xvimage_buffer_pool_parent_class)->finalize (object);
}
