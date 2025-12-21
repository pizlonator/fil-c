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

#include "gst/video/gstvideometa.h"
#include "gst/video/gstvideopool.h"


GST_DEBUG_CATEGORY_STATIC (gst_video_pool_debug);
#define GST_CAT_DEFAULT gst_video_pool_debug

/**
 * SECTION:gstvideopool
 * @title: GstVideoBufferPool
 * @short_description: GstBufferPool for raw video buffers
 * @see_also: #GstBufferPool
 *
 * Special GstBufferPool subclass for raw video buffers.
 *
 * Allows configuration of video-specific requirements such as
 * stride alignments or pixel padding, and can also be configured
 * to automatically add #GstVideoMeta to the buffers.
 */

/**
 * gst_buffer_pool_config_set_video_alignment:
 * @config: a #GstStructure
 * @align: a #GstVideoAlignment
 *
 * Set the video alignment in @align to the bufferpool configuration
 * @config
 */
void
gst_buffer_pool_config_set_video_alignment (GstStructure * config,
    const GstVideoAlignment * align)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (align != NULL);

  gst_structure_set (config,
      "padding-top", G_TYPE_UINT, align->padding_top,
      "padding-bottom", G_TYPE_UINT, align->padding_bottom,
      "padding-left", G_TYPE_UINT, align->padding_left,
      "padding-right", G_TYPE_UINT, align->padding_right,
      "stride-align0", G_TYPE_UINT, align->stride_align[0],
      "stride-align1", G_TYPE_UINT, align->stride_align[1],
      "stride-align2", G_TYPE_UINT, align->stride_align[2],
      "stride-align3", G_TYPE_UINT, align->stride_align[3], NULL);
}

/**
 * gst_buffer_pool_config_get_video_alignment:
 * @config: a #GstStructure
 * @align: a #GstVideoAlignment
 *
 * Get the video alignment from the bufferpool configuration @config in
 * in @align
 *
 * Returns: %TRUE if @config could be parsed correctly.
 */
gboolean
gst_buffer_pool_config_get_video_alignment (GstStructure * config,
    GstVideoAlignment * align)
{
  g_return_val_if_fail (config != NULL, FALSE);
  g_return_val_if_fail (align != NULL, FALSE);

  return gst_structure_get (config,
      "padding-top", G_TYPE_UINT, &align->padding_top,
      "padding-bottom", G_TYPE_UINT, &align->padding_bottom,
      "padding-left", G_TYPE_UINT, &align->padding_left,
      "padding-right", G_TYPE_UINT, &align->padding_right,
      "stride-align0", G_TYPE_UINT, &align->stride_align[0],
      "stride-align1", G_TYPE_UINT, &align->stride_align[1],
      "stride-align2", G_TYPE_UINT, &align->stride_align[2],
      "stride-align3", G_TYPE_UINT, &align->stride_align[3], NULL);
}

/* bufferpool */
struct _GstVideoBufferPoolPrivate
{
  GstVideoInfo info;
  GstVideoAlignment video_align;
  gboolean add_videometa;
  gboolean need_alignment;
  GstAllocator *allocator;
  GstAllocationParams params;
};

static void gst_video_buffer_pool_finalize (GObject * object);

#define gst_video_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstVideoBufferPool, gst_video_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar **
video_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT, NULL
  };
  return options;
}

static gboolean
video_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstVideoBufferPool *vpool = GST_VIDEO_BUFFER_POOL_CAST (pool);
  GstVideoBufferPoolPrivate *priv = vpool->priv;
  GstVideoInfo info;
  GstCaps *caps;
  guint size, min_buffers, max_buffers;
  gint width, height;
  GstAllocator *allocator;
  GstAllocationParams params;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps))
    goto wrong_caps;

  if (size < info.size)
    goto wrong_size;

  if (!gst_buffer_pool_config_get_allocator (config, &allocator, &params))
    goto wrong_config;

  width = info.width;
  height = info.height;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, width, height, caps);

  priv->params = params;
  if (priv->allocator)
    gst_object_unref (priv->allocator);
  if ((priv->allocator = allocator))
    gst_object_ref (allocator);

  /* enable metadata based on config of the pool */
  priv->add_videometa =
      gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);

  /* parse extra alignment info */
  priv->need_alignment = gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);

  if (priv->need_alignment && priv->add_videometa) {
    guint max_align, n;

    gst_buffer_pool_config_get_video_alignment (config, &priv->video_align);

    /* ensure GstAllocationParams alignment is compatible with video alignment */
    max_align = priv->params.align;
    for (n = 0; n < GST_VIDEO_MAX_PLANES; ++n)
      max_align |= priv->video_align.stride_align[n];

    for (n = 0; n < GST_VIDEO_MAX_PLANES; ++n)
      priv->video_align.stride_align[n] = max_align;

    /* apply the alignment to the info */
    if (!gst_video_info_align (&info, &priv->video_align))
      goto failed_to_align;

    gst_buffer_pool_config_set_video_alignment (config, &priv->video_align);

    if (priv->params.align < max_align) {
      GST_WARNING_OBJECT (pool, "allocation params alignment %u is smaller "
          "than the max specified video stride alignment %u, fixing",
          (guint) priv->params.align, max_align);
      priv->params.align = max_align;
      gst_buffer_pool_config_set_allocator (config, allocator, &priv->params);
    }
  }
  info.size = MAX (size, info.size);
  priv->info = info;

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
wrong_size:
  {
    GST_WARNING_OBJECT (pool,
        "Provided size is to small for the caps: %u < %" G_GSIZE_FORMAT, size,
        info.size);
    return FALSE;
  }
failed_to_align:
  {
    GST_WARNING_OBJECT (pool, "Failed to align");
    return FALSE;
  }
}

static GstFlowReturn
video_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstVideoBufferPool *vpool = GST_VIDEO_BUFFER_POOL_CAST (pool);
  GstVideoBufferPoolPrivate *priv = vpool->priv;
  GstVideoInfo *info;

  info = &priv->info;

  GST_DEBUG_OBJECT (pool, "alloc %" G_GSIZE_FORMAT, info->size);

  *buffer =
      gst_buffer_new_allocate (priv->allocator, info->size, &priv->params);
  if (*buffer == NULL)
    goto no_memory;

  if (priv->add_videometa) {
    GST_DEBUG_OBJECT (pool, "adding GstVideoMeta");

    gst_buffer_add_video_meta_full (*buffer, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (info),
        GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info),
        GST_VIDEO_INFO_N_PLANES (info), info->offset, info->stride);
  }

  return GST_FLOW_OK;

  /* ERROR */
no_memory:
  {
    GST_WARNING_OBJECT (pool, "can't create memory");
    return GST_FLOW_ERROR;
  }
}

/**
 * gst_video_buffer_pool_new:
 *
 * Create a new bufferpool that can allocate video frames. This bufferpool
 * supports all the video bufferpool options.
 *
 * Returns: (transfer full): a new #GstBufferPool to allocate video frames
 */
GstBufferPool *
gst_video_buffer_pool_new ()
{
  GstVideoBufferPool *pool;

  pool = g_object_new (GST_TYPE_VIDEO_BUFFER_POOL, NULL);
  gst_object_ref_sink (pool);

  GST_LOG_OBJECT (pool, "new video buffer pool %p", pool);

  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_video_buffer_pool_class_init (GstVideoBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_video_buffer_pool_finalize;

  gstbufferpool_class->get_options = video_buffer_pool_get_options;
  gstbufferpool_class->set_config = video_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = video_buffer_pool_alloc;

  GST_DEBUG_CATEGORY_INIT (gst_video_pool_debug, "videopool", 0,
      "videopool object");
}

static void
gst_video_buffer_pool_init (GstVideoBufferPool * pool)
{
  pool->priv = gst_video_buffer_pool_get_instance_private (pool);
}

static void
gst_video_buffer_pool_finalize (GObject * object)
{
  GstVideoBufferPool *pool = GST_VIDEO_BUFFER_POOL_CAST (object);
  GstVideoBufferPoolPrivate *priv = pool->priv;

  GST_LOG_OBJECT (pool, "finalize video buffer pool %p", pool);

  if (priv->allocator)
    gst_object_unref (priv->allocator);

  G_OBJECT_CLASS (gst_video_buffer_pool_parent_class)->finalize (object);
}
