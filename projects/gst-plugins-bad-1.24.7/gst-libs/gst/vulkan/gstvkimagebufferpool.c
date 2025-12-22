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

#include "gstvkimagebufferpool.h"

/**
 * SECTION:vkimagebufferpool
 * @title: GstVulkanImageBufferPool
 * @short_description: buffer pool for #GstVulkanImageMemory objects
 * @see_also: #GstBufferPool, #GstVulkanImageMemory
 *
 * a #GstVulkanImageBufferPool is an object that allocates buffers with #GstVulkanImageMemory
 *
 * A #GstVulkanImageBufferPool is created with gst_vulkan_image_buffer_pool_new()
 */

/* bufferpool */
struct _GstVulkanImageBufferPoolPrivate
{
  GstCaps *caps;
  gboolean raw_caps;
  GstVideoInfo v_info;
  VkImageUsageFlags usage;
  VkMemoryPropertyFlags mem_props;
  VkImageLayout initial_layout;
  guint64 initial_access;
  VkFormat vk_fmts[GST_VIDEO_MAX_PLANES];
  int n_imgs;
  guint32 n_layers;
  gboolean has_profile;
  GstVulkanVideoProfile profile;
  GstVulkanOperation *exec;
};

static void gst_vulkan_image_buffer_pool_finalize (GObject * object);

GST_DEBUG_CATEGORY_STATIC (GST_CAT_VULKAN_IMAGE_BUFFER_POOL);
#define GST_CAT_DEFAULT GST_CAT_VULKAN_IMAGE_BUFFER_POOL

#define GET_PRIV(pool) gst_vulkan_image_buffer_pool_get_instance_private (pool)

#define gst_vulkan_image_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanImageBufferPool, gst_vulkan_image_buffer_pool,
    GST_TYPE_BUFFER_POOL, G_ADD_PRIVATE (GstVulkanImageBufferPool)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_IMAGE_BUFFER_POOL,
        "vulkanimagebufferpool", 0, "Vulkan Image Buffer Pool"));

/**
 * gst_vulkan_image_buffer_pool_config_set_allocation_params:
 * @config: the #GstStructure with the pool's configuration.
 * @usage: The Vulkan image usage flags.
 * @mem_properties: Vulkan memory property flags.
 * @initial_layout: Initial Vulkan image layout.
 * @initial_access: Access flags for the layout transition if @initial_layout is
 * not VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED.
 *
 * Sets the @usage and @mem_properties of the images to setup.
 *
 * Since: 1.24
 */
void
gst_vulkan_image_buffer_pool_config_set_allocation_params (GstStructure *
    config, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_properties,
    VkImageLayout initial_layout, guint64 initial_access)
{
  /* assumption: G_TYPE_UINT is compatible with uint32_t (VkFlags) */
  gst_structure_set (config, "usage", G_TYPE_UINT, usage, "memory-properties",
      G_TYPE_UINT, mem_properties, "initial-layout", G_TYPE_UINT,
      initial_layout, "initial-access", G_TYPE_UINT64, initial_access, NULL);
}

/**
 * gst_vulkan_image_buffer_pool_config_set_decode_caps:
 * @config: the #GstStructure with the pool's configuration.
 * @caps: Upstream decode caps.
 *
 * Decode @caps are used when the buffers are going to be used either as decoded
 * dest or DPB images.
 *
 * Since: 1.24
 */
void
gst_vulkan_image_buffer_pool_config_set_decode_caps (GstStructure * config,
    GstCaps * caps)
{
  g_return_if_fail (GST_IS_CAPS (caps));

  gst_structure_set (config, "decode-caps", GST_TYPE_CAPS, caps, NULL);
}

static inline gboolean
gst_vulkan_image_buffer_pool_config_get_allocation_params (GstStructure *
    config, VkImageUsageFlags * usage, VkMemoryPropertyFlags * mem_props,
    VkImageLayout * initial_layout, guint64 * initial_access,
    guint32 * n_layers, GstCaps ** decode_caps)
{
  if (!gst_structure_get_uint (config, "usage", usage)) {
    *usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }

  if (!gst_structure_get_uint (config, "memory-properties", mem_props))
    *mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (!gst_structure_get_uint (config, "initial-layout", initial_layout))
    *initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (!gst_structure_get_uint64 (config, "initial-access", initial_access))
    *initial_access = 0;

  if (!gst_structure_get_uint (config, "num-layers", n_layers))
    *n_layers = 1;

  if (decode_caps)
    gst_structure_get (config, "decode-caps", GST_TYPE_CAPS, decode_caps, NULL);

  return TRUE;
}

static gboolean
gst_vulkan_image_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config)
{
  GstVulkanImageBufferPool *vk_pool = GST_VULKAN_IMAGE_BUFFER_POOL_CAST (pool);
  GstVulkanImageBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  VkImageTiling tiling;
  VkImageUsageFlags requested_usage, supported_usage;
  VkImageCreateInfo image_info;
  guint min_buffers, max_buffers;
  GstCaps *caps = NULL, *decode_caps = NULL;
  GstCapsFeatures *features;
  gboolean found, no_multiplane, ret = TRUE;
  guint i;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, &min_buffers,
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

  features = gst_caps_get_features (caps, 0);
  priv->raw_caps = features == NULL || gst_caps_features_is_equal (features,
      GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY);

  gst_vulkan_image_buffer_pool_config_get_allocation_params (config,
      &priv->usage, &priv->mem_props, &priv->initial_layout,
      &priv->initial_access, &priv->n_layers, &decode_caps);

  priv->has_profile = FALSE;
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  if (decode_caps && ((priv->usage
              & (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                  | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR)) != 0)) {
    priv->has_profile =
        gst_vulkan_video_profile_from_caps (&priv->profile, decode_caps,
        GST_VULKAN_VIDEO_OPERATION_DECODE);
  }
#endif
  gst_clear_caps (&decode_caps);

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  if (((priv->usage & (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                  | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR)) != 0)
      && !priv->has_profile)
    goto missing_profile;

  if (priv->has_profile) {
    no_multiplane = FALSE;

    /* HACK(victor): NVIDIA & RADV drivers don't report decoding features for
     * color format. Setting usage to zero to short circuit validation. */
    requested_usage = 0;
  } else
#endif
  {
    no_multiplane = TRUE;
    requested_usage = priv->usage;
  }

  tiling = priv->raw_caps ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
  found = gst_vulkan_format_from_video_info_2 (vk_pool->device->physical_device,
      &priv->v_info, tiling, no_multiplane, requested_usage, priv->vk_fmts,
      &priv->n_imgs, &supported_usage);
  if (!found)
    goto no_vk_format;

  if (priv->usage == 0) {
    priv->usage = supported_usage & (VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT);
  }

  /* get the size of the buffer to allocate */
  /* *INDENT-OFF* */
  image_info = (VkImageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    /* .format = fill per image,  */
    /* .extent = fill per plane, */
    .mipLevels = 1,
    .arrayLayers = priv->n_layers,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = tiling,
    .usage = priv->usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = NULL,
    .initialLayout = priv->initial_layout == VK_IMAGE_LAYOUT_PREINITIALIZED
                     ? VK_IMAGE_LAYOUT_PREINITIALIZED
                     : VK_IMAGE_LAYOUT_UNDEFINED,
  };
  /* *INDENT-ON* */
  priv->v_info.size = 0;
  for (i = 0; i < priv->n_imgs; i++) {
    GstVulkanImageMemory *img_mem;
    guint width, height;
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    VkVideoProfileListInfoKHR profile_list = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,
      .profileCount = 1,
      .pProfiles = &priv->profile.profile,
    };
#endif

    if (GST_VIDEO_INFO_N_PLANES (&priv->v_info) != priv->n_imgs) {
      width = GST_VIDEO_INFO_WIDTH (&priv->v_info);
      height = GST_VIDEO_INFO_HEIGHT (&priv->v_info);
    } else {
      width = GST_VIDEO_INFO_COMP_WIDTH (&priv->v_info, i);
      height = GST_VIDEO_INFO_COMP_HEIGHT (&priv->v_info, i);
    }

    image_info.format = priv->vk_fmts[i];
    /* *INDENT-OFF* */
    image_info.extent = (VkExtent3D) { width, height, 1 };
    /* *INDENT-ON* */
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    if (priv->has_profile)
      image_info.pNext = &profile_list;
#endif

    img_mem = (GstVulkanImageMemory *)
        gst_vulkan_image_memory_alloc_with_image_info (vk_pool->device,
        &image_info, priv->mem_props);
    if (!img_mem)
      goto mem_create_failed;

    if (!img_mem)
      goto image_failed;

    priv->v_info.offset[i] = priv->v_info.size;
    priv->v_info.size += img_mem->requirements.size;

    gst_memory_unref (GST_MEMORY_CAST (img_mem));
  }

  gst_buffer_pool_config_set_params (config, caps,
      priv->v_info.size, min_buffers, max_buffers);

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
no_vk_format:
  {
    GST_WARNING_OBJECT (pool, "no Vulkan format available for %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&priv->v_info)));
    return FALSE;
  }
mem_create_failed:
  {
    GST_WARNING_OBJECT (pool, "Could not create Vulkan Memory");
    return FALSE;
  }
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
missing_profile:
  {
    GST_WARNING_OBJECT (pool, "missing or invalid decode-caps");
    return FALSE;
  }
#endif
image_failed:
  {
    GST_WARNING_OBJECT (pool, "Failed to allocate image");
    return FALSE;
  }
}

static gboolean
prepare_buffer (GstVulkanImageBufferPool * vk_pool, GstBuffer * buffer)
{
#if defined(VK_KHR_synchronization2)
  GstVulkanImageBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  GArray *barriers = NULL;
  GError *error = NULL;

  if (priv->initial_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
      priv->initial_layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    return TRUE;

  if (!priv->exec) {
    GstVulkanCommandPool *cmd_pool;
    GstVulkanQueue *queue = NULL;

    queue =
        gst_vulkan_device_select_queue (vk_pool->device,
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    if (!queue)
      return FALSE;
    cmd_pool = gst_vulkan_queue_create_command_pool (queue, &error);
    gst_object_unref (queue);
    if (error)
      goto error;
    priv->exec = gst_vulkan_operation_new (cmd_pool);
    gst_object_unref (cmd_pool);
  }

  if (!gst_vulkan_operation_add_dependency_frame (priv->exec, buffer,
          VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT))
    return FALSE;

  if (!gst_vulkan_operation_begin (priv->exec, &error))
    goto error;

  if (!gst_vulkan_operation_add_frame_barrier (priv->exec, buffer,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, priv->initial_access,
          priv->initial_layout, NULL))
    goto error;

  barriers = gst_vulkan_operation_retrieve_image_barriers (priv->exec);
  if (barriers->len > 0) {
    if (gst_vulkan_operation_use_sync2 (priv->exec)) {
      VkDependencyInfoKHR dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .pImageMemoryBarriers = (gpointer) barriers->data,
        .imageMemoryBarrierCount = barriers->len,
      };

      gst_vulkan_operation_pipeline_barrier2 (priv->exec, &dependency_info);
    } else {
      gst_vulkan_command_buffer_lock (priv->exec->cmd_buf);
      vkCmdPipelineBarrier (priv->exec->cmd_buf->cmd,
          VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
          NULL, 0, NULL, barriers->len, (gpointer) barriers->data);
      gst_vulkan_command_buffer_unlock (priv->exec->cmd_buf);
    }
  }
  g_array_unref (barriers);

  if (!gst_vulkan_operation_end (priv->exec, &error))
    goto error;

  return TRUE;

error:
  {
    if (error) {
      GST_WARNING_OBJECT (vk_pool, "Error: %s", error->message);
      g_clear_error (&error);
    }
    return FALSE;
  }
#endif
  return TRUE;
}

/* This function handles GstBuffer creation */
static GstFlowReturn
gst_vulkan_image_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstVulkanImageBufferPool *vk_pool = GST_VULKAN_IMAGE_BUFFER_POOL_CAST (pool);
  GstVulkanImageBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  VkImageTiling tiling =
      priv->raw_caps ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
  VkImageCreateInfo image_info;
  GstBuffer *buf;
  guint i;

  /* *INDENT-OFF* */
  image_info = (VkImageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    /* .format = fill per image,  */
    /* .extent = fill per plane, */
    .mipLevels = 1,
    .arrayLayers = priv->n_layers,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = tiling,
    .usage = priv->usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = NULL,
    .initialLayout = priv->initial_layout == VK_IMAGE_LAYOUT_PREINITIALIZED
                     ? VK_IMAGE_LAYOUT_PREINITIALIZED
                     : VK_IMAGE_LAYOUT_UNDEFINED,
  };
  /* *INDENT-ON* */

  if (!(buf = gst_buffer_new ())) {
    goto no_buffer;
  }

  for (i = 0; i < priv->n_imgs; i++) {
    GstMemory *mem;
    guint width, height;
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    VkVideoProfileListInfoKHR profile_list = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,
      .profileCount = 1,
      .pProfiles = &priv->profile.profile,
    };
#endif

    if (GST_VIDEO_INFO_N_PLANES (&priv->v_info) != priv->n_imgs) {
      width = GST_VIDEO_INFO_WIDTH (&priv->v_info);
      height = GST_VIDEO_INFO_HEIGHT (&priv->v_info);
    } else {
      width = GST_VIDEO_INFO_COMP_WIDTH (&priv->v_info, i);
      height = GST_VIDEO_INFO_COMP_HEIGHT (&priv->v_info, i);
    }

    image_info.format = priv->vk_fmts[i];
    /* *INDENT-OFF* */
    image_info.extent = (VkExtent3D) { width, height, 1 };
    /* *INDENT-ON* */
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    if (priv->has_profile)
      image_info.pNext = &profile_list;
#endif

    mem = gst_vulkan_image_memory_alloc_with_image_info (vk_pool->device,
        &image_info, priv->mem_props);
    if (!mem) {
      gst_buffer_unref (buf);
      goto mem_create_failed;
    }

    gst_buffer_append_memory (buf, mem);
  }

  prepare_buffer (vk_pool, buf);

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

static gboolean
gst_vulkan_image_buffer_pool_stop (GstBufferPool * pool)
{
#if defined(VK_KHR_synchronization2)
  GstVulkanImageBufferPool *vk_pool = GST_VULKAN_IMAGE_BUFFER_POOL_CAST (pool);
  GstVulkanImageBufferPoolPrivate *priv = GET_PRIV (vk_pool);
  if (priv->exec)
    gst_vulkan_operation_wait (priv->exec);
#endif

  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

/**
 * gst_vulkan_image_buffer_pool_new:
 * @device: the #GstVulkanDevice to use
 *
 * Returns: (transfer full): a #GstBufferPool that allocates buffers with #GstGLMemory
 *
 * Since: 1.18
 */
GstBufferPool *
gst_vulkan_image_buffer_pool_new (GstVulkanDevice * device)
{
  GstVulkanImageBufferPool *pool;

  pool = g_object_new (GST_TYPE_VULKAN_IMAGE_BUFFER_POOL, NULL);
  g_object_ref_sink (pool);
  pool->device = gst_object_ref (device);

  GST_LOG_OBJECT (pool, "new Vulkan buffer pool for device %" GST_PTR_FORMAT,
      device);

  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_vulkan_image_buffer_pool_class_init (GstVulkanImageBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_vulkan_image_buffer_pool_finalize;

  gstbufferpool_class->set_config = gst_vulkan_image_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_vulkan_image_buffer_pool_alloc;
  gstbufferpool_class->stop = gst_vulkan_image_buffer_pool_stop;
}

static void
gst_vulkan_image_buffer_pool_init (GstVulkanImageBufferPool * pool)
{
}

static void
gst_vulkan_image_buffer_pool_finalize (GObject * object)
{
  GstVulkanImageBufferPool *pool = GST_VULKAN_IMAGE_BUFFER_POOL_CAST (object);
  GstVulkanImageBufferPoolPrivate *priv = GET_PRIV (pool);

  GST_LOG_OBJECT (pool, "finalize Vulkan buffer pool %p", pool);

  if (priv->caps)
    gst_caps_unref (priv->caps);

  gst_clear_object (&priv->exec);

  G_OBJECT_CLASS (gst_vulkan_image_buffer_pool_parent_class)->finalize (object);

  /* only release the context once all our memory have been deleted */
  if (pool->device) {
    gst_object_unref (pool->device);
    pool->device = NULL;
  }
}
