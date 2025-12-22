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
 * SECTION:vkdescriptorset
 * @title: GstVulkanDescriptorSet
 * @short_description: Vulkan descriptor set
 * @see_also: #GstVulkanDescriptorPool, #GstVulkanDescriptorCache, #GstVulkanDevice
 *
 * vulkandescriptorset holds information about a descriptor set.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkdescriptorset.h"
#include "gstvkdescriptorpool.h"
#include "gstvkdescriptorcache.h"

#define GST_CAT_DEFAULT gst_debug_vulkan_descriptor_set
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

#define gst_vulkan_descriptor_cache_release_set(c,s) \
    gst_vulkan_handle_pool_release (GST_VULKAN_HANDLE_POOL_CAST (c), s);

static void
init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkandescriptorset", 0,
        "Vulkan descriptor set");
    g_once_init_leave (&_init, 1);
  }
}

static gboolean
gst_vulkan_descriptor_set_dispose (GstVulkanDescriptorSet * set)
{
  GstVulkanDescriptorCache *cache;

  /* no pool, do free */
  if ((cache = set->cache) == NULL)
    return TRUE;

  /* keep the buffer alive */
  gst_vulkan_descriptor_set_ref (set);
  /* return the buffer to the pool */
  gst_vulkan_descriptor_cache_release_set (cache, set);

  return FALSE;
}

static void
gst_vulkan_descriptor_set_free (GstVulkanDescriptorSet * set)
{
  guint i;

  g_assert (set->cache == NULL);

  GST_TRACE ("Freeing %p", set);

  for (i = 0; i < set->n_layouts; i++)
    gst_vulkan_handle_unref (set->layouts[i]);
  g_free (set->layouts);

  vkFreeDescriptorSets (set->pool->device->device, set->pool->pool, 1,
      &set->set);

  gst_clear_object (&set->pool);

  g_free (set);
}

static void
gst_vulkan_descriptor_set_init (GstVulkanDescriptorSet * set,
    GstVulkanDescriptorPool * pool, VkDescriptorSet desc_set, guint n_layouts,
    GstVulkanHandle ** layouts)
{
  guint i;

  set->pool = gst_object_ref (pool);
  set->set = desc_set;
  set->n_layouts = n_layouts;
  set->layouts = g_new0 (GstVulkanHandle *, n_layouts);
  for (i = 0; i < n_layouts; i++)
    set->layouts[i] = gst_vulkan_handle_ref (layouts[i]);

  init_debug ();

  GST_TRACE ("new %p", set);

  gst_mini_object_init (&set->parent, 0, GST_TYPE_VULKAN_DESCRIPTOR_SET,
      NULL, (GstMiniObjectDisposeFunction) gst_vulkan_descriptor_set_dispose,
      (GstMiniObjectFreeFunction) gst_vulkan_descriptor_set_free);
}

/**
 * gst_vulkan_descriptor_set_new_wrapped:
 * @set: a VkDescriptorSet
 * @n_layouts: number of @layouts
 * @layouts: (array length=n_layouts): list of #GstVulkanHandle containing
 *                                     descriptor set layouts
 *
 * Returns: (transfer full): a new #GstVulkanDescriptorSet
 *
 * Since: 1.18
 */
GstVulkanDescriptorSet *
gst_vulkan_descriptor_set_new_wrapped (GstVulkanDescriptorPool * pool,
    VkDescriptorSet set, guint n_layouts, GstVulkanHandle ** layouts)
{
  GstVulkanDescriptorSet *ret;

  g_return_val_if_fail (GST_IS_VULKAN_DESCRIPTOR_POOL (pool), NULL);
  g_return_val_if_fail (set != VK_NULL_HANDLE, NULL);
  g_return_val_if_fail (n_layouts > 0, NULL);
  g_return_val_if_fail (layouts != NULL, NULL);

  ret = g_new0 (GstVulkanDescriptorSet, 1);
  gst_vulkan_descriptor_set_init (ret, pool, set, n_layouts, layouts);

  return ret;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanDescriptorSet, gst_vulkan_descriptor_set);
