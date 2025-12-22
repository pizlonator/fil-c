/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

/**
 * SECTION:vulkanhandlepool
 * @title: GstVulkanHandlePool
 * @short_description: Vulkan handle pool
 * @see_also: #GstVulkanHandle, #GstVulkanDevice
 *
 * #GstVulkanHandlePool holds a number of handles that are pooled together.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkhandlepool.h"
#include "gstvkdevice.h"

#define GST_VULKAN_HANDLE_POOL_LARGE_OUTSTANDING 1024

#define GST_CAT_DEFAULT gst_debug_vulkan_handle_pool
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

#define parent_class gst_vulkan_handle_pool_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstVulkanHandlePool, gst_vulkan_handle_pool,
    GST_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkanhandlepool", 0, "Vulkan handle pool"));

static gpointer
gst_vulkan_handle_pool_default_alloc (GstVulkanHandlePool * pool,
    GError ** error)
{
  return NULL;
}

static gpointer
gst_vulkan_handle_pool_default_acquire (GstVulkanHandlePool * pool,
    GError ** error)
{
  gpointer ret;

  GST_OBJECT_LOCK (pool);
  if (pool->available->len > 0) {
    ret = g_ptr_array_remove_index_fast (pool->available, 0);
  } else {
    ret = gst_vulkan_handle_pool_alloc (pool, error);
  }

  if (ret) {
    g_ptr_array_add (pool->outstanding, ret);

#if defined(GST_ENABLE_EXTRA_CHECKS)
    if (pool->outstanding->len > GST_VULKAN_HANDLE_POOL_LARGE_OUTSTANDING)
      g_critical ("%s: There are a large number of handles outstanding! "
          "This usually means there is a reference counting issue somewhere.",
          GST_OBJECT_NAME (pool));
#endif
  }
  GST_OBJECT_UNLOCK (pool);

  return ret;
}

static void
gst_vulkan_handle_pool_default_release (GstVulkanHandlePool * pool,
    gpointer handle)
{
  GST_OBJECT_LOCK (pool);
  if (!g_ptr_array_remove_fast (pool->outstanding, handle)) {
    g_warning ("%s: Attempt was made to release a handle (%p) that does not "
        "belong to us", GST_OBJECT_NAME (pool), handle);
    GST_OBJECT_UNLOCK (pool);
    return;
  }

  g_ptr_array_add (pool->available, handle);
  GST_OBJECT_UNLOCK (pool);
}

static void
gst_vulkan_handle_pool_default_free (GstVulkanHandlePool * pool,
    gpointer handle)
{
}

static void
do_free_handle (gpointer handle, GstVulkanHandlePool * pool)
{
  GstVulkanHandlePoolClass *klass = GST_VULKAN_HANDLE_POOL_GET_CLASS (pool);
  klass->free (pool, handle);
}

static void
gst_vulkan_handle_pool_dispose (GObject * object)
{
  GstVulkanHandlePool *pool = GST_VULKAN_HANDLE_POOL (object);

  if (pool->outstanding) {
    g_warn_if_fail (pool->outstanding->len <= 0);
    g_ptr_array_unref (pool->outstanding);
  }
  pool->outstanding = NULL;

  if (pool->available) {
    g_ptr_array_foreach (pool->available, (GFunc) do_free_handle, pool);
    g_ptr_array_unref (pool->available);
  }
  pool->available = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_vulkan_handle_pool_finalize (GObject * object)
{
  GstVulkanHandlePool *pool = GST_VULKAN_HANDLE_POOL (object);

  gst_clear_object (&pool->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vulkan_handle_pool_init (GstVulkanHandlePool * handle)
{
  handle->outstanding = g_ptr_array_new ();
  handle->available = g_ptr_array_new ();
}

static void
gst_vulkan_handle_pool_class_init (GstVulkanHandlePoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_vulkan_handle_pool_dispose;
  gobject_class->finalize = gst_vulkan_handle_pool_finalize;

  klass->alloc = gst_vulkan_handle_pool_default_alloc;
  klass->acquire = gst_vulkan_handle_pool_default_acquire;
  klass->release = gst_vulkan_handle_pool_default_release;
  klass->free = gst_vulkan_handle_pool_default_free;
}

gpointer
gst_vulkan_handle_pool_alloc (GstVulkanHandlePool * pool, GError ** error)
{
  GstVulkanHandlePoolClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_HANDLE_POOL (pool), NULL);
  klass = GST_VULKAN_HANDLE_POOL_GET_CLASS (pool);
  g_return_val_if_fail (klass->alloc != NULL, NULL);

  return klass->alloc (pool, error);
}

gpointer
gst_vulkan_handle_pool_acquire (GstVulkanHandlePool * pool, GError ** error)
{
  GstVulkanHandlePoolClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_HANDLE_POOL (pool), NULL);
  klass = GST_VULKAN_HANDLE_POOL_GET_CLASS (pool);
  g_return_val_if_fail (klass->acquire != NULL, NULL);

  return klass->acquire (pool, error);
}

void
gst_vulkan_handle_pool_release (GstVulkanHandlePool * pool, gpointer handle)
{
  GstVulkanHandlePoolClass *klass;

  g_return_if_fail (GST_IS_VULKAN_HANDLE_POOL (pool));
  klass = GST_VULKAN_HANDLE_POOL_GET_CLASS (pool);
  g_return_if_fail (klass->release != NULL);

  klass->release (pool, handle);
}
