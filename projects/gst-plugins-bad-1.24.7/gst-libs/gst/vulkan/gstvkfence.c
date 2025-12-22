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

#include "gstvkfence.h"
#include "gstvkdevice.h"

/**
 * SECTION:vkfence
 * @title: GstVulkanFence
 * @short_description: Vulkan fences
 * @see_also: #GstVulkanDevice
 *
 * A #GstVulkanFence encapsulates a VkFence
 */

GST_DEBUG_CATEGORY (gst_debug_vulkan_fence);
#define GST_CAT_DEFAULT gst_debug_vulkan_fence

#define gst_vulkan_fence_cache_release(c,f) gst_vulkan_handle_pool_release(GST_VULKAN_HANDLE_POOL (c), f)

static void
_init_debug (void)
{
  static gsize init;

  if (g_once_init_enter (&init)) {
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_fence,
        "vulkanfence", 0, "Vulkan Fence");
    g_once_init_leave (&init, 1);
  }
}

static gboolean
gst_vulkan_fence_dispose (GstVulkanFence * fence)
{
  GstVulkanFenceCache *cache;

  /* no pool, do free */
  if ((cache = fence->cache) == NULL)
    return TRUE;

  /* keep the buffer alive */
  gst_vulkan_fence_ref (fence);
  /* return the buffer to the cache */
  gst_vulkan_fence_cache_release (cache, fence);

  return FALSE;
}

static void
gst_vulkan_fence_free (GstVulkanFence * fence)
{
  if (!fence)
    return;

  GST_TRACE ("Freeing fence %p", fence);

  if (fence->fence)
    vkDestroyFence (fence->device->device, fence->fence, NULL);

  gst_clear_object (&fence->device);

  g_free (fence);
}

/**
 * gst_vulkan_fence_new:
 * @device: the parent #GstVulkanDevice
 * @error: (out) (optional): a #GError for the failure condition
 *
 * Returns: a new #GstVulkanFence or %NULL on error
 *
 * Since: 1.18
 */
GstVulkanFence *
gst_vulkan_fence_new (GstVulkanDevice * device, GError ** error)
{
  VkFenceCreateInfo fence_info = { 0, };
  GstVulkanFence *fence;
  VkResult err;

  _init_debug ();

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);

  fence = g_new0 (GstVulkanFence, 1);
  GST_TRACE ("Creating fence %p with device %" GST_PTR_FORMAT, fence, device);
  fence->device = gst_object_ref (device);

  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = NULL;
  fence_info.flags = 0;

  err = vkCreateFence (device->device, &fence_info, NULL, &fence->fence);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateFence") < 0) {
    gst_clear_object (&fence->device);
    g_free (fence);
    return NULL;
  }

  gst_mini_object_init (GST_MINI_OBJECT_CAST (fence), 0, GST_TYPE_VULKAN_FENCE,
      NULL, (GstMiniObjectDisposeFunction) gst_vulkan_fence_dispose,
      (GstMiniObjectFreeFunction) gst_vulkan_fence_free);

  return fence;
}

/**
 * gst_vulkan_fence_new_always_signalled:
 *
 * Returns: a new #GstVulkanFence that is always in the signalled state
 *
 * Since: 1.18
 */
GstVulkanFence *
gst_vulkan_fence_new_always_signalled (GstVulkanDevice * device)
{
  GstVulkanFence *fence;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);

  _init_debug ();

  fence = g_new0 (GstVulkanFence, 1);
  GST_TRACE ("Creating always-signalled fence %p with device %" GST_PTR_FORMAT,
      fence, device);
  fence->device = gst_object_ref (device);
  fence->fence = VK_NULL_HANDLE;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (fence), 0, GST_TYPE_VULKAN_FENCE,
      NULL, NULL, (GstMiniObjectFreeFunction) gst_vulkan_fence_free);

  return fence;
}

/**
 * gst_vulkan_fence_is_signaled:
 * @fence: a #GstVulkanFence
 *
 * Returns: whether @fence has been signalled
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_fence_is_signaled (GstVulkanFence * fence)
{
  g_return_val_if_fail (fence != NULL, FALSE);

  if (!fence->fence)
    return TRUE;

  return vkGetFenceStatus (fence->device->device, fence->fence) == VK_SUCCESS;
}

void
gst_vulkan_fence_reset (GstVulkanFence * fence)
{
  g_return_if_fail (fence != NULL);

  if (!fence->fence)
    return;

  GST_TRACE ("resetting fence %p", fence);
  vkResetFences (fence->device->device, 1, &fence->fence);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanFence, gst_vulkan_fence);

#define parent_class gst_vulkan_fence_cache_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanFenceCache, gst_vulkan_fence_cache,
    GST_TYPE_VULKAN_HANDLE_POOL, _init_debug ());

GstVulkanFenceCache *
gst_vulkan_fence_cache_new (GstVulkanDevice * device)
{
  GstVulkanFenceCache *ret;
  GstVulkanHandlePool *pool;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);

  ret = g_object_new (GST_TYPE_VULKAN_FENCE_CACHE, NULL);

  pool = GST_VULKAN_HANDLE_POOL (ret);
  pool->device = gst_object_ref (device);

  gst_object_ref_sink (ret);

  return ret;
}

static gpointer
gst_vulkan_fence_cache_alloc (GstVulkanHandlePool * pool, GError ** error)
{
  return gst_vulkan_fence_new (pool->device, error);
}

static gpointer
gst_vulkan_fence_cache_acquire_impl (GstVulkanHandlePool * pool,
    GError ** error)
{
  GstVulkanFence *fence;

  if ((fence =
          GST_VULKAN_HANDLE_POOL_CLASS (parent_class)->acquire (pool, error))) {
    fence->cache = gst_object_ref (pool);
    if (!fence->device)
      fence->device = gst_object_ref (pool->device);
  }

  return fence;
}

static void
gst_vulkan_fence_cache_release_impl (GstVulkanHandlePool * pool,
    gpointer handle)
{
  GstVulkanFence *fence = handle;

  gst_vulkan_fence_reset (fence);

  GST_VULKAN_HANDLE_POOL_CLASS (parent_class)->release (pool, handle);

  if (fence) {
    gst_clear_object (&fence->cache);
    gst_clear_object (&fence->device);
  }
}

static void
gst_vulkan_fence_cache_free (GstVulkanHandlePool * pool, gpointer handle)
{
  GstVulkanFence *fence = handle;

  if (!fence->device)
    fence->device = gst_object_ref (pool->device);

  gst_vulkan_fence_unref (handle);
}

static void
gst_vulkan_fence_cache_init (GstVulkanFenceCache * cache)
{
}

static void
gst_vulkan_fence_cache_class_init (GstVulkanFenceCacheClass * klass)
{
  GstVulkanHandlePoolClass *handle_class = (GstVulkanHandlePoolClass *) klass;

  handle_class->acquire = gst_vulkan_fence_cache_acquire_impl;
  handle_class->alloc = gst_vulkan_fence_cache_alloc;
  handle_class->release = gst_vulkan_fence_cache_release_impl;
  handle_class->free = gst_vulkan_fence_cache_free;
}
