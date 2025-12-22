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

#ifndef __GST_VULKAN_PHYSICAL_DEVICE_H__
#define __GST_VULKAN_PHYSICAL_DEVICE_H__

#include <gst/vulkan/gstvkinstance.h>
#include <gst/vulkan/gstvkqueue.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_PHYSICAL_DEVICE         (gst_vulkan_physical_device_get_type())
#define GST_VULKAN_PHYSICAL_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_PHYSICAL_DEVICE, GstVulkanPhysicalDevice))
#define GST_VULKAN_PHYSICAL_DEVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VULKAN_PHYSICAL_DEVICE, GstVulkanPhysicalDeviceClass))
#define GST_IS_VULKAN_PHYSICAL_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_PHYSICAL_DEVICE))
#define GST_IS_VULKAN_PHYSICAL_DEVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_PHYSICAL_DEVICE))
#define GST_VULKAN_PHYSICAL_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_PHYSICAL_DEVICE, GstVulkanPhysicalDeviceClass))
GST_VULKAN_API
GType gst_vulkan_physical_device_get_type       (void);

/**
 * GstVulkanQueueFamilyOps:
 * @video: video operation supported by queue family
 * @query: if queue family supports queries
 *
 * Since: 1.24
 */
struct _GstVulkanQueueFamilyOps
{
  guint32 video;
  gboolean query;
};

/**
 * GstVulkanPhysicalDevice:
 * @parent: the parent #GstObject
 * @instance: the parent #GstVulkanInstance for this physical device
 * @device_index: the index into the physical device list in @instance
 * @device: the vulkan physical device handle
 * @properties: retrieved physical device properties
 * @features: retrieved physical device features
 * @memory_properties: retrieved physical device memory properties
 * @queue_family_props: vulkan family properties
 * @n_queue_families: number of elements in @queue_family_props
 *
 * Since: 1.18
 */
struct _GstVulkanPhysicalDevice
{
  GstObject parent;

  GstVulkanInstance *instance;

  guint device_index;
  VkPhysicalDevice device; /* hides a pointer */

  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceFeatures features;
  VkPhysicalDeviceMemoryProperties memory_properties;

  VkQueueFamilyProperties *queue_family_props;
  guint32 n_queue_families;

  /**
   * GstVulkanPhysicalDevice.queue_family_ops:
   *
   * vulkan operations allowed per queue family
   *
   * Since: 1.24
   */
  GstVulkanQueueFamilyOps *queue_family_ops;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanPhysicalDeviceClass:
 * @parent_class: the parent #GstObjectClass
 *
 * Since: 1.18
 */
struct _GstVulkanPhysicalDeviceClass
{
  GstObjectClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstVulkanPhysicalDevice, gst_object_unref)

GST_VULKAN_API
GstVulkanPhysicalDevice *   gst_vulkan_physical_device_new                  (GstVulkanInstance * instance,
                                                                             guint device_index);
GST_VULKAN_API
GstVulkanInstance *         gst_vulkan_physical_device_get_instance         (GstVulkanPhysicalDevice * device);

GST_VULKAN_API
VkPhysicalDevice            gst_vulkan_physical_device_get_handle           (GstVulkanPhysicalDevice * device);

GST_VULKAN_API
gboolean                    gst_vulkan_physical_device_get_extension_info   (GstVulkanPhysicalDevice * device,
                                                                             const gchar * name,
                                                                             guint32 * spec_version);
GST_VULKAN_API
gboolean                    gst_vulkan_physical_device_get_layer_info       (GstVulkanPhysicalDevice * device,
                                                                             const gchar * name,
                                                                             gchar ** description,
                                                                             guint32 * spec_version,
                                                                             guint32 * implementation_version);


G_END_DECLS

#endif /* __GST_VULKAN_PHYSICAL_DEVICE_H__ */
