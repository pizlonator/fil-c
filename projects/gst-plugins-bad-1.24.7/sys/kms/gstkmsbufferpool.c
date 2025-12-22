/*
 * GStreamer
 * Copyright (C) 2016 Igalia
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/gstvideometa.h>

#include "gstkmsbufferpool.h"
#include "gstkmsallocator.h"

GST_DEBUG_CATEGORY_STATIC (gst_kms_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_kms_buffer_pool_debug

struct _GstKMSBufferPoolPrivate
{
  GstVideoInfo vinfo;
  GstAllocator *allocator;
  gboolean add_videometa;
  gboolean has_prime_export;
};

#define parent_class gst_kms_buffer_pool_parent_class
G_DEFINE_TYPE_WITH_CODE (GstKMSBufferPool, gst_kms_buffer_pool,
    GST_TYPE_VIDEO_BUFFER_POOL, G_ADD_PRIVATE (GstKMSBufferPool);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "kmsbufferpool", 0,
        "KMS buffer pool"));

static const gchar **
gst_kms_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    GST_BUFFER_POOL_OPTION_KMS_BUFFER,
    GST_BUFFER_POOL_OPTION_KMS_PRIME_EXPORT,
    NULL
  };
  return options;
}

static gboolean
gst_kms_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstKMSBufferPool *vpool;
  GstKMSBufferPoolPrivate *priv;
  GstCaps *caps;
  GstVideoInfo vinfo;
  GstAllocator *allocator;
  GstAllocationParams params;

  vpool = GST_KMS_BUFFER_POOL_CAST (pool);
  priv = vpool->priv;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL))
    goto wrong_config;

  if (!caps)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&vinfo, caps))
    goto wrong_caps;

  allocator = NULL;
  gst_buffer_pool_config_get_allocator (config, &allocator, &params);

  /* not our allocator, not our buffers */
  if (allocator && GST_IS_KMS_ALLOCATOR (allocator)) {
    if (priv->allocator)
      gst_object_unref (priv->allocator);
    priv->allocator = gst_object_ref (allocator);
  }
  if (!priv->allocator)
    goto no_allocator;

  priv->vinfo = vinfo;

  /* enable metadata based on config of the pool */
  priv->add_videometa = gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);
  priv->has_prime_export = gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_KMS_PRIME_EXPORT);

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
no_allocator:
  {
    GST_WARNING_OBJECT (pool, "no valid allocator in pool");
    return FALSE;
  }
}

static GstFlowReturn
gst_kms_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstKMSBufferPool *vpool;
  GstKMSBufferPoolPrivate *priv;
  GstVideoInfo *info;
  GstMemory *mem;

  vpool = GST_KMS_BUFFER_POOL_CAST (pool);
  priv = vpool->priv;
  info = &priv->vinfo;

  mem = gst_kms_allocator_bo_alloc (priv->allocator, info);
  if (!mem)
    goto no_memory;

  if (vpool->priv->has_prime_export) {
    GstMemory *dmabufmem;

    dmabufmem = gst_kms_allocator_dmabuf_export (priv->allocator, mem);
    if (dmabufmem)
      mem = dmabufmem;
    else
      GST_WARNING_OBJECT (pool, "Failed to export DMABuf from Dumb buffer.");
  }

  *buffer = gst_buffer_new ();
  gst_buffer_append_memory (*buffer, mem);

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

static void
gst_kms_buffer_pool_finalize (GObject * object)
{
  GstKMSBufferPool *pool;
  GstKMSBufferPoolPrivate *priv;

  pool = GST_KMS_BUFFER_POOL (object);
  priv = pool->priv;

  if (priv->allocator)
    gst_object_unref (priv->allocator);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_kms_buffer_pool_init (GstKMSBufferPool * pool)
{
  pool->priv = gst_kms_buffer_pool_get_instance_private (pool);
}

static void
gst_kms_buffer_pool_class_init (GstKMSBufferPoolClass * klass)
{
  GObjectClass *gobject_class;
  GstBufferPoolClass *gstbufferpool_class;

  gobject_class = (GObjectClass *) klass;
  gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_kms_buffer_pool_finalize;

  gstbufferpool_class->get_options = gst_kms_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_kms_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_kms_buffer_pool_alloc_buffer;
}

GstBufferPool *
gst_kms_buffer_pool_new (void)
{
  GstBufferPool *pool;

  pool = g_object_new (GST_TYPE_KMS_BUFFER_POOL, NULL);
  gst_object_ref_sink (pool);

  return pool;
}
