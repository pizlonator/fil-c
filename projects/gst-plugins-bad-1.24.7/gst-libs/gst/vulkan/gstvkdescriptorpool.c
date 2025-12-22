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

#include "gstvkdescriptorpool.h"

/**
 * SECTION:vkdescriptorpool
 * @title: GstVulkanDescriptorPool
 * @short_description: Vulkan descriptor pool
 * @see_also: #GstVulkanDescriptorSet, #GstVulkanDescriptorCache, #GstVulkanDevice
 */

#define GET_PRIV(pool) gst_vulkan_descriptor_pool_get_instance_private (pool)

#define GST_CAT_DEFAULT gst_vulkan_descriptor_pool_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

struct _GstVulkanDescriptorPoolPrivate
{
  gsize max_sets;
  gsize outstanding;
};

#define parent_class gst_vulkan_descriptor_pool_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanDescriptorPool, gst_vulkan_descriptor_pool,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstVulkanDescriptorPool);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkancommandpool", 0, "Vulkan Command Pool"));

static void gst_vulkan_descriptor_pool_finalize (GObject * object);

static void
gst_vulkan_descriptor_pool_init (GstVulkanDescriptorPool * pool)
{
}

static void
gst_vulkan_descriptor_pool_class_init (GstVulkanDescriptorPoolClass *
    device_class)
{
  GObjectClass *gobject_class = (GObjectClass *) device_class;

  gobject_class->finalize = gst_vulkan_descriptor_pool_finalize;
}

static void
gst_vulkan_descriptor_pool_finalize (GObject * object)
{
  GstVulkanDescriptorPool *pool = GST_VULKAN_DESCRIPTOR_POOL (object);
#if 0
  GstVulkanDescriptorPoolPrivate *priv = GET_PRIV (pool);

  /* FIXME: track these correctly */
  if (priv->outstanding > 0)
    g_critical
        ("Destroying a Vulkan descriptor pool that has outstanding descriptors!");
#endif

  if (pool->pool)
    vkDestroyDescriptorPool (pool->device->device, pool->pool, NULL);
  pool->pool = VK_NULL_HANDLE;

  gst_clear_object (&pool->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gst_vulkan_descriptor_pool_new_wrapped:
 * @device: a #GstVulkanDevice
 * @pool: (transfer full): a `VkDescriptorPool`
 * @max_sets: maximum descriptor sets allocatable wit @pool
 *
 * Returns: (transfer full): a new #GstVulkanDescriptorPool
 *
 * Since: 1.18
 */
GstVulkanDescriptorPool *
gst_vulkan_descriptor_pool_new_wrapped (GstVulkanDevice * device,
    VkDescriptorPool pool, gsize max_sets)
{
  GstVulkanDescriptorPool *ret;
  GstVulkanDescriptorPoolPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);
  g_return_val_if_fail (pool != VK_NULL_HANDLE, NULL);
  g_return_val_if_fail (max_sets > 0, NULL);

  ret = g_object_new (GST_TYPE_VULKAN_DESCRIPTOR_POOL, NULL);
  ret->device = gst_object_ref (device);
  ret->pool = pool;

  priv = GET_PRIV (ret);
  priv->max_sets = max_sets;

  gst_object_ref_sink (ret);

  return ret;
}

/**
 * gst_vulkan_descriptor_pool_get_device
 * @pool: a #GstVulkanDescriptorPool
 *
 * Returns: (transfer full): the parent #GstVulkanDevice for this descriptor pool
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_descriptor_pool_get_device (GstVulkanDescriptorPool * pool)
{
  g_return_val_if_fail (GST_IS_VULKAN_DESCRIPTOR_POOL (pool), NULL);

  return gst_object_ref (pool->device);
}

/**
 * gst_vulkan_descriptor_pool_get_max_sets:
 * @pool: a #GstVulkanDescriptorPool
 *
 * Returns: the maximum number of sets allocatable from @pool
 *
 * Since: 1.18
 */
gsize
gst_vulkan_descriptor_pool_get_max_sets (GstVulkanDescriptorPool * pool)
{
  GstVulkanDescriptorPoolPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DESCRIPTOR_POOL (pool), 0);

  priv = GET_PRIV (pool);

  return priv->max_sets;
}

static GstVulkanDescriptorSet *
descriptor_set_alloc (GstVulkanDescriptorPool * pool, guint n_layouts,
    GstVulkanHandle ** layouts, GError ** error)
{
  VkDescriptorSetLayout *vk_layouts;
  VkDescriptorSetAllocateInfo alloc_info;
  GstVulkanDescriptorSet *set;
  VkDescriptorSet descriptor;
  VkResult err;
  guint i;

  vk_layouts = g_alloca (n_layouts * sizeof (VkDescriptorSetLayout));
  for (i = 0; i < n_layouts; i++)
    vk_layouts[i] = (VkDescriptorSetLayout) layouts[i]->handle;

  /* *INDENT-OFF* */
  alloc_info = (VkDescriptorSetAllocateInfo) {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = NULL,
      .descriptorPool = pool->pool,
      .descriptorSetCount = n_layouts,
      .pSetLayouts = vk_layouts
  };
  /* *INDENT-ON* */

  err =
      vkAllocateDescriptorSets (pool->device->device, &alloc_info, &descriptor);
  if (gst_vulkan_error_to_g_error (err, error, "vkAllocateDescriptorSets") < 0)
    return NULL;

  set =
      gst_vulkan_descriptor_set_new_wrapped (pool, descriptor, n_layouts,
      layouts);
  GST_LOG_OBJECT (pool, "created descriptor set %p", set);

  return set;
}

/**
 * gst_vulkan_descriptor_pool_create:
 * @pool: a #GstVulkanDescriptorPool
 * @n_layouts: number of @layouts
 * @layouts: (array length=n_layouts): list of #GstVulkanHandle containing
 *                                     descriptor set layouts
 * @error: a #GError
 *
 * Returns: a new #GstVulkanDescriptorSet
 *
 * Since: 1.18
 */
GstVulkanDescriptorSet *
gst_vulkan_descriptor_pool_create (GstVulkanDescriptorPool * pool,
    guint n_layouts, GstVulkanHandle ** layouts, GError ** error)
{
  GstVulkanDescriptorSet *cmd = NULL;
  GstVulkanDescriptorPoolPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DESCRIPTOR_POOL (pool), NULL);
  g_return_val_if_fail (n_layouts > 0, NULL);
  g_return_val_if_fail (layouts != NULL, NULL);

  priv = GET_PRIV (pool);

  GST_OBJECT_LOCK (pool);
  priv->outstanding++;
  if (priv->outstanding >= priv->max_sets) {
    g_warning ("%s: Attempt was made to allocate more descriptor sets than are "
        "available", GST_OBJECT_NAME (pool));
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_TOO_MANY_OBJECTS,
        "Attempt was made to allocate more descriptor sets than are available");
    priv->outstanding--;
    GST_OBJECT_UNLOCK (pool);
    return NULL;
  }
  GST_OBJECT_UNLOCK (pool);

  cmd = descriptor_set_alloc (pool, n_layouts, layouts, error);
  if (!cmd)
    return NULL;

  return cmd;
}
