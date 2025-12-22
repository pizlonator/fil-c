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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkdescriptorcache.h"

/**
 * SECTION:vkdescriptorcache
 * @title: GstVulkanDescriptorCache
 * @short_description: Vulkan descriptor cache
 * @see_also: #GstVulkanDescriptorSet, #GstVulkanDescriptorPool, #GstVulkanDevice
 */

#define GET_PRIV(cache) gst_vulkan_descriptor_cache_get_instance_private (cache)

#define GST_CAT_DEFAULT gst_vulkan_descriptor_cache_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

struct _GstVulkanDescriptorCachePrivate
{
  guint n_layouts;
  GstVulkanHandle **layouts;
};

#define parent_class gst_vulkan_descriptor_cache_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanDescriptorCache, gst_vulkan_descriptor_cache,
    GST_TYPE_VULKAN_HANDLE_POOL, G_ADD_PRIVATE (GstVulkanDescriptorCache);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkancommandcache", 0, "Vulkan Command Cache"));

static void
gst_vulkan_descriptor_cache_finalize (GObject * object)
{
  GstVulkanDescriptorCache *cache = GST_VULKAN_DESCRIPTOR_CACHE (object);
  GstVulkanDescriptorCachePrivate *priv = GET_PRIV (cache);
  guint i;

  for (i = 0; i < priv->n_layouts; i++)
    gst_vulkan_handle_unref (priv->layouts[i]);
  g_free (priv->layouts);

  gst_clear_object (&cache->pool);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gst_vulkan_descriptor_cache_new:
 * @pool: a #GstVulkanDescriptorPool
 * @n_layouts: number of @layouts
 * @layouts: (array length=n_layouts): list of #GstVulkanHandle containing
 *                                     descriptor set layouts
 *
 * Returns: (transfer full): a new #GstVulkanDescriptorCache
 *
 * Since: 1.18
 */
GstVulkanDescriptorCache *
gst_vulkan_descriptor_cache_new (GstVulkanDescriptorPool * pool,
    guint n_layouts, GstVulkanHandle ** layouts)
{
  GstVulkanHandlePool *handle_pool;
  GstVulkanDescriptorCache *ret;
  GstVulkanDescriptorCachePrivate *priv;
  guint i;

  g_return_val_if_fail (GST_IS_VULKAN_DESCRIPTOR_POOL (pool), NULL);

  ret = g_object_new (GST_TYPE_VULKAN_DESCRIPTOR_CACHE, NULL);
  ret->pool = gst_object_ref (pool);

  priv = GET_PRIV (ret);
  priv->n_layouts = n_layouts;
  priv->layouts = g_new0 (GstVulkanHandle *, n_layouts);
  for (i = 0; i < n_layouts; i++)
    priv->layouts[i] = gst_vulkan_handle_ref (layouts[i]);

  handle_pool = GST_VULKAN_HANDLE_POOL (ret);
  handle_pool->device = gst_object_ref (pool->device);

  gst_object_ref_sink (ret);

  return ret;
}

static gpointer
gst_vulkan_descriptor_cache_acquire_impl (GstVulkanHandlePool * pool,
    GError ** error)
{
  GstVulkanDescriptorSet *set;

  if ((set =
          GST_VULKAN_HANDLE_POOL_CLASS (parent_class)->acquire (pool, error)))
    set->cache = gst_object_ref (pool);

  return set;
}

static gpointer
gst_vulkan_descriptor_cache_alloc_impl (GstVulkanHandlePool * pool,
    GError ** error)
{
  GstVulkanDescriptorCache *desc = GST_VULKAN_DESCRIPTOR_CACHE (pool);
  GstVulkanDescriptorCachePrivate *priv = GET_PRIV (desc);

  return gst_vulkan_descriptor_pool_create (desc->pool, priv->n_layouts,
      priv->layouts, error);
}

static void
gst_vulkan_descriptor_cache_release_impl (GstVulkanHandlePool * pool,
    gpointer handle)
{
  GstVulkanDescriptorSet *set = handle;

  GST_VULKAN_HANDLE_POOL_CLASS (parent_class)->release (pool, handle);

  /* decrease the refcount that the set had to us */
  gst_clear_object (&set->cache);
}

static void
gst_vulkan_descriptor_cache_free_impl (GstVulkanHandlePool * pool,
    gpointer handle)
{
  GstVulkanDescriptorSet *set = handle;

  GST_VULKAN_HANDLE_POOL_CLASS (parent_class)->free (pool, handle);

  gst_vulkan_descriptor_set_unref (set);
}

/**
 * gst_vulkan_descriptor_cache_acquire:
 * @cache: a #GstVulkanDescriptorCache
 * @error: a #GError
 *
 * Returns: a new #GstVulkanDescriptorSet
 *
 * Since: 1.18
 */
GstVulkanDescriptorSet *
gst_vulkan_descriptor_cache_acquire (GstVulkanDescriptorCache * cache,
    GError ** error)
{
  return gst_vulkan_handle_pool_acquire (GST_VULKAN_HANDLE_POOL_CAST (cache),
      error);
}

static void
gst_vulkan_descriptor_cache_init (GstVulkanDescriptorCache * cache)
{
}

static void
gst_vulkan_descriptor_cache_class_init (GstVulkanDescriptorCacheClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVulkanHandlePoolClass *handle_class = (GstVulkanHandlePoolClass *) klass;

  gobject_class->finalize = gst_vulkan_descriptor_cache_finalize;

  handle_class->acquire = gst_vulkan_descriptor_cache_acquire_impl;
  handle_class->alloc = gst_vulkan_descriptor_cache_alloc_impl;
  handle_class->release = gst_vulkan_descriptor_cache_release_impl;
  handle_class->free = gst_vulkan_descriptor_cache_free_impl;
}
