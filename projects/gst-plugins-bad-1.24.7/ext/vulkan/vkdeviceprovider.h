/* GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 *
 * vkdeviceprovider.h: Device probing and monitoring
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_VULKAN_DEVICE_PROVIDER_H__
#define __GST_VULKAN_DEVICE_PROVIDER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/vulkan/vulkan.h>

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstVulkanDeviceProvider GstVulkanDeviceProvider;
typedef struct _GstVulkanDeviceProviderClass GstVulkanDeviceProviderClass;

#define GST_TYPE_VULKAN_DEVICE_PROVIDER                 (gst_vulkan_device_provider_get_type())
#define GST_IS_VULKAN_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_DEVICE_PROVIDER))
#define GST_IS_VULKAN_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VULKAN_DEVICE_PROVIDER))
#define GST_VULKAN_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VULKAN_DEVICE_PROVIDER, GstVulkanDeviceProviderClass))
#define GST_VULKAN_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_DEVICE_PROVIDER, GstVulkanDeviceProvider))
#define GST_VULKAN_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstVulkanDeviceProviderClass))
#define GST_VULKAN_DEVICE_PROVIDER_CAST(obj)            ((GstVulkanDeviceProvider *)(obj))

struct _GstVulkanDeviceProvider {
  GstDeviceProvider         parent;
};

typedef enum {
  GST_VULKAN_DEVICE_TYPE_SINK
} GstVulkanDeviceType;

struct _GstVulkanDeviceProviderClass {
  GstDeviceProviderClass    parent_class;
};

GType gst_vulkan_device_provider_get_type (void);

typedef struct _GstVulkanDeviceObject GstVulkanDeviceObject;
typedef struct _GstVulkanDeviceObjectClass GstVulkanDeviceObjectClass;

#define GST_TYPE_VULKAN_DEVICE_OBJECT                 (gst_vulkan_device_object_get_type())
#define GST_IS_VULKAN_DEVICE_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VULKAN_DEVICE_OBJECT))
#define GST_IS_VULKAN_DEVICE_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VULKAN_DEVICE_OBJECT))
#define GST_VULKAN_DEVICE_OBJECT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VULKAN_DEVICE_OBJECT, GstVulkanDeviceObjectClass))
#define GST_VULKAN_DEVICE_OBJECT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VULKAN_DEVICE_OBJECT, GstVulkanDeviceObject))
#define GST_VULKAN_DEVICE_OBJECT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VULKAN_DEVICE_OBJECT, GstVulkanDeviceObjectClass))
#define GST_VULKAN_DEVICE_OBJECT_CAST(obj)            ((GstVulkanDevice *)(obj))

struct _GstVulkanDeviceObject {
  GstDevice                 parent;

  GstVulkanDeviceType       type;
  guint                     device_index;
  gboolean                  is_default;
  const gchar              *element;
  GstVulkanPhysicalDevice  *physical_device;
};

struct _GstVulkanDeviceObjectClass {
  GstDeviceClass            parent_class;
};

GType        gst_vulkan_device_object_get_type (void);

GST_DEVICE_PROVIDER_REGISTER_DECLARE (vulkandeviceprovider);

G_END_DECLS

#endif /* __GST_VULKAN_DEVICE_PROVIDER_H__ */
