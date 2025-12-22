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

#ifndef __GST_VULKAN_DESCRIPTOR_SET_H__
#define __GST_VULKAN_DESCRIPTOR_SET_H__

#include <gst/gst.h>

#include <gst/vulkan/vulkan_fwd.h>
#include <gst/vulkan/gstvkapi.h>

G_BEGIN_DECLS

/**
 * gst_vulkan_descriptor_set_get_type:
 *
 * Since: 1.18
 */
GST_VULKAN_API
GType gst_vulkan_descriptor_set_get_type (void);
/**
 * GST_TYPE_VULKAN_DESCRIPTOR_SET:
 *
 * Since: 1.18
 */
#define GST_TYPE_VULKAN_DESCRIPTOR_SET (gst_vulkan_descriptor_set_get_type ())

typedef struct _GstVulkanDescriptorSet GstVulkanDescriptorSet;

/**
 * GstVulkanDescriptorSet:
 * @parent: the parent #GstMiniObject
 * @set: the vulkan descriptor set handle
 * @pool: the parent #GstVulkanDescriptorPool for pooling
 * @cache: the parent #GstVulkanDescriptorCache for reuse
 * @n_layouts: number of layouts applied to this descriptor set
 * @layouts: layouts applied to this descriptor set
 *
 * Since: 1.18
 */
struct _GstVulkanDescriptorSet
{
  GstMiniObject             parent;

  VkDescriptorSet           set;

  /* <protected> */
  GstVulkanDescriptorPool  *pool;
  GstVulkanDescriptorCache *cache;

  guint                     n_layouts;
  GstVulkanHandle         **layouts;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * gst_vulkan_descriptor_set_ref: (skip)
 * @set: a #GstVulkanDescriptorSet.
 *
 * Increases the refcount of the given buffer by one.
 *
 * Returns: (transfer full): @set
 *
 * Since: 1.18
 */
static inline GstVulkanDescriptorSet* gst_vulkan_descriptor_set_ref(GstVulkanDescriptorSet* set);
static inline GstVulkanDescriptorSet *
gst_vulkan_descriptor_set_ref (GstVulkanDescriptorSet * set)
{
  return (GstVulkanDescriptorSet *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (set));
}

/**
 * gst_vulkan_descriptor_set_unref: (skip)
 * @set: (transfer full): a #GstVulkanDescriptorSet.
 *
 * Decreases the refcount of the buffer. If the refcount reaches 0, the buffer
 * will be freed.
 *
 * Since: 1.18
 */
static inline void gst_vulkan_descriptor_set_unref(GstVulkanDescriptorSet* set);
static inline void
gst_vulkan_descriptor_set_unref (GstVulkanDescriptorSet * set)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (set));
}

/**
 * gst_clear_vulkan_descriptor_set: (skip)
 * @set_ptr: a pointer to a #GstVulkanDescriptorSet reference
 *
 * Clears a reference to a #GstVulkanDescriptorSet.
 *
 * @set_ptr must not be %NULL.
 *
 * If the reference is %NULL then this function does nothing. Otherwise, the
 * reference count of the descriptor set is decreased and the pointer is set
 * to %NULL.
 *
 * Since: 1.18
 */
static inline void
gst_clear_vulkan_descriptor_set (GstVulkanDescriptorSet ** set_ptr)
{
  gst_clear_mini_object ((GstMiniObject **) set_ptr);
}

GST_VULKAN_API
GstVulkanDescriptorSet *    gst_vulkan_descriptor_set_new_wrapped       (GstVulkanDescriptorPool * pool,
                                                                         VkDescriptorSet set,
                                                                         guint n_layouts,
                                                                         GstVulkanHandle ** layouts);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanDescriptorSet, gst_vulkan_descriptor_set_unref);

G_END_DECLS

#endif /* _GST_VULKAN_DESCRIPTOR_SET_H_ */
