/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#include "gstvkbufferpool.h"

/**
 * SECTION:vkbufferpool
 * @title: GstVulkanBufferPool
 * @short_description: buffer pool for #GstVulkanBufferMemory objects
 * @see_also: #GstBufferPool, #GstVulkanBufferMemory
 *
 * a #GstVulkanBufferPool is an object that allocates buffers with #GstVulkanBufferMemory
 *
 * A #GstVulkanBufferPool is created with gst_vulkan_buffer_pool_new()
 *
 * #GstVulkanBufferPool implements the VideoMeta buffer pool option
 * #GST_BUFFER_POOL_OPTION_VIDEO_META
 */

/* bufferpool */
struct _GstVulkanBufferPoolPrivate
{
  GstCaps *caps;
  GstVideoInfo v_info;
  gboolean add_videometa;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags mem_props;
  gsize alloc_sizes[GST_VIDEO_MAX_PLANES];
};

static void gst_vulkan_buffer_pool_finalize (GObject * object);

GST_DEBUG_CATEGORY_STATIC (GST_CAT_VULKAN_BUFFER_POOL);
#define GST_CAT_DEFAULT GST_CAT_VULKAN_BUFFER_POOL

#define GET_PRIV(pool) gst_vulkan_buffer_pool_get_instance_private (pool)

#define gst_vulkan_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanBufferPool, gst_vulkan_buffer_pool,
    GST_TYPE_BUFFER_POOL, G_ADD_PRIVATE (GstVulkanBufferPool)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_BUFFER_POOL,
        "vulkanbufferpool", 0, "Vulkan Buffer Pool"));

static const gchar **
gst_vulkan_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    NULL
  };

  return options;
}

static inline gboolean
gst_vulkan_buffer_pool_config_get_allocation_params (GstStructure *
    config, VkBufferUsageFlags * usage, VkMemoryPropertyFlags * mem_props)
{
  if (!gst_structure_get_uint (config, "usage", usage)) {
    *usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }

  if (!gst_structure_get_uint (config, "memory-properties", mem_props))
    *mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  return TRUE;
}

/**
 * gst_vulkan_buffer_pool_config_set_allocation_params:
 * @config: the #GstStructure with the pool's configuration.
 * @usage: The Vulkan buffer usage flags.
 *
 * Sets the @usage of the buffers to setup.
 *
 * Since: 1.24
 */
void
gst_vulkan_buffer_pool_config_set_allocation_params (GstStructure *
    config, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_properties)
{
  /* assumption: G_TYPE_UINT is compatible with uint32_t (VkFlags) */
  gst_structure_set (config, "usage", G_TYPE_UINT, usage, "memory-properties",
      G_TYPE_UINT, mem_properties, NULL);
}

static gboolean
gst_vulkan_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstVulkanBufferPool *vk_pool = GST_VULKAN_BUFFER_POOL_CAST (pool);
  GstVulkanBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  guint min_buffers, max_buffers, size;
  GstCaps *caps = NULL;
  gboolean ret = TRUE;
  guint i;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&priv->v_info, caps))
    goto wrong_caps;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, priv->v_info.width,
      priv->v_info.height, caps);

  gst_caps_replace (&priv->caps, caps);

  gst_vulkan_buffer_pool_config_get_allocation_params (config,
      &priv->usage, &priv->mem_props);

  /* get the size of the buffer to allocate */
  if (GST_VIDEO_INFO_FORMAT (&priv->v_info) != GST_VIDEO_FORMAT_ENCODED) {
    priv->v_info.size = 0;
    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&priv->v_info); i++) {
      priv->alloc_sizes[i] = GST_VIDEO_INFO_COMP_HEIGHT (&priv->v_info, i) *
          GST_VIDEO_INFO_PLANE_STRIDE (&priv->v_info, i);
      priv->v_info.offset[i] = priv->v_info.size;
      priv->v_info.size += priv->alloc_sizes[i];
    }

    priv->add_videometa = gst_buffer_pool_config_has_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    gst_buffer_pool_config_set_params (config, caps,
        priv->v_info.size, min_buffers, max_buffers);
  } else {
    priv->alloc_sizes[0] = size;
  }

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config) && ret;

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
}

/* This function handles GstBuffer creation */
static GstFlowReturn
gst_vulkan_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstVulkanBufferPool *vk_pool = GST_VULKAN_BUFFER_POOL_CAST (pool);
  GstVulkanBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  GstMemory *mem;
  GstBuffer *buf;
  guint i, num_planes;

  if (!(buf = gst_buffer_new ())) {
    goto no_buffer;
  }

  if (GST_VIDEO_INFO_FORMAT (&priv->v_info) != GST_VIDEO_FORMAT_ENCODED)
    num_planes = GST_VIDEO_INFO_N_PLANES (&priv->v_info);
  else
    num_planes = 1;

  for (i = 0; i < num_planes; i++) {
    mem =
        gst_vulkan_buffer_memory_alloc (vk_pool->device, priv->alloc_sizes[i],
        priv->usage, priv->mem_props);
    if (!mem) {
      gst_buffer_unref (buf);
      goto mem_create_failed;
    }
    gst_buffer_append_memory (buf, mem);
  }

  if (GST_VIDEO_INFO_FORMAT (&priv->v_info) != GST_VIDEO_FORMAT_ENCODED) {
    gst_buffer_add_video_meta_full (buf, 0,
        GST_VIDEO_INFO_FORMAT (&priv->v_info),
        GST_VIDEO_INFO_WIDTH (&priv->v_info),
        GST_VIDEO_INFO_HEIGHT (&priv->v_info),
        GST_VIDEO_INFO_N_PLANES (&priv->v_info), priv->v_info.offset,
        priv->v_info.stride);
  }

  *buffer = buf;

  return GST_FLOW_OK;

  /* ERROR */
no_buffer:
  {
    GST_WARNING_OBJECT (pool, "can't create image");
    return GST_FLOW_ERROR;
  }
mem_create_failed:
  {
    GST_WARNING_OBJECT (pool, "Could not create Vulkan Memory");
    return GST_FLOW_ERROR;
  }
}

/**
 * gst_vulkan_buffer_pool_new:
 * @device: the #GstVulkanDevice to use
 *
 * Returns: (transfer full): a #GstBufferPool that allocates buffers with #GstGLMemory
 *
 * Since: 1.18
 */
GstBufferPool *
gst_vulkan_buffer_pool_new (GstVulkanDevice * device)
{
  GstVulkanBufferPool *pool;

  pool = g_object_new (GST_TYPE_VULKAN_BUFFER_POOL, NULL);
  g_object_ref_sink (pool);
  pool->device = gst_object_ref (device);

  GST_LOG_OBJECT (pool, "new Vulkan buffer pool for device %" GST_PTR_FORMAT,
      device);

  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_vulkan_buffer_pool_class_init (GstVulkanBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_vulkan_buffer_pool_finalize;

  gstbufferpool_class->get_options = gst_vulkan_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_vulkan_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_vulkan_buffer_pool_alloc;
}

static void
gst_vulkan_buffer_pool_init (GstVulkanBufferPool * pool)
{
}

static void
gst_vulkan_buffer_pool_finalize (GObject * object)
{
  GstVulkanBufferPool *pool = GST_VULKAN_BUFFER_POOL_CAST (object);
  GstVulkanBufferPoolPrivate *priv = GET_PRIV (pool);

  GST_LOG_OBJECT (pool, "finalize Vulkan buffer pool %p", pool);

  if (priv->caps)
    gst_caps_unref (priv->caps);

  G_OBJECT_CLASS (gst_vulkan_buffer_pool_parent_class)->finalize (object);

  /* only release the context once all our memory have been deleted */
  if (pool->device) {
    gst_object_unref (pool->device);
    pool->device = NULL;
  }
}
