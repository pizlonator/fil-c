/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_INSTANCE_H__
#define __GST_VULKAN_INSTANCE_H__

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_INSTANCE         (gst_vulkan_instance_get_type())
#define GST_VULKAN_INSTANCE(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_INSTANCE, GstVulkanInstance))
#define GST_VULKAN_INSTANCE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VULKAN_INSTANCE, GstVulkanInstanceClass))
#define GST_IS_VULKAN_INSTANCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_INSTANCE))
#define GST_IS_VULKAN_INSTANCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_INSTANCE))
#define GST_VULKAN_INSTANCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_INSTANCE, GstVulkanInstanceClass))
GST_VULKAN_API
GType gst_vulkan_instance_get_type       (void);

/**
 * GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR:
 *
 * Since: 1.18
 */
#define GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR "gst.vulkan.instance"

/**
 * GstVulkanInstance:
 * @parent: parent #GstObject
 * @instance: the Vulkan instance handle
 * @physical_devices: list of vulkan physical device handles
 * @n_physical_device: number of entries in @physical_devices
 *
 * Since: 1.18
 */
struct _GstVulkanInstance
{
  GstObject parent;

  VkInstance instance; /* hides a pointer */
  VkPhysicalDevice *physical_devices; /* hides a pointer */
  guint32 n_physical_devices;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanInstanceClass:
 * @parent_class: parent #GstObjectClass
 *
 * Since: 1.18
 */
struct _GstVulkanInstanceClass
{
  GstObjectClass parent_class;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstVulkanInstance, gst_object_unref)

GST_VULKAN_API
GstVulkanInstance * gst_vulkan_instance_new                     (void);
GST_VULKAN_API
gboolean            gst_vulkan_instance_fill_info               (GstVulkanInstance * instance,
                                                                 GError ** error);
GST_VULKAN_API
gboolean            gst_vulkan_instance_open                    (GstVulkanInstance * instance,
                                                                 GError ** error);

GST_VULKAN_API
gpointer            gst_vulkan_instance_get_proc_address        (GstVulkanInstance * instance,
                                                                 const gchar * name);

GST_VULKAN_API
GstVulkanDevice *   gst_vulkan_instance_create_device           (GstVulkanInstance * instance,
                                                                 GError ** error);

GST_VULKAN_API
void                gst_context_set_vulkan_instance             (GstContext * context,
                                                                 GstVulkanInstance * instance);
GST_VULKAN_API
gboolean            gst_context_get_vulkan_instance             (GstContext * context,
                                                                 GstVulkanInstance ** instance);
GST_VULKAN_API
gboolean            gst_vulkan_instance_handle_context_query    (GstElement * element,
                                                                 GstQuery * query,
                                                                 GstVulkanInstance * instance);
GST_VULKAN_API
gboolean            gst_vulkan_instance_run_context_query       (GstElement * element,
                                                                 GstVulkanInstance ** instance);
GST_VULKAN_API
gboolean            gst_vulkan_instance_check_version           (GstVulkanInstance * instance,
                                                                 guint major,
                                                                 guint minor,
                                                                 guint patch);
GST_VULKAN_API
void                gst_vulkan_instance_get_version             (GstVulkanInstance * instance,
                                                                 guint * major,
                                                                 guint * minor,
                                                                 guint * patch);

GST_VULKAN_API
gboolean            gst_vulkan_instance_get_extension_info      (GstVulkanInstance * instance,
                                                                 const gchar * name,
                                                                 guint32 * spec_version);
GST_VULKAN_API
gboolean            gst_vulkan_instance_enable_extension        (GstVulkanInstance * instance,
                                                                 const gchar * name);
GST_VULKAN_API
gboolean            gst_vulkan_instance_disable_extension       (GstVulkanInstance * instance,
                                                                 const gchar * name);
GST_VULKAN_API
gboolean            gst_vulkan_instance_is_extension_enabled    (GstVulkanInstance * instance,
                                                                 const gchar * name);
GST_VULKAN_API
gboolean            gst_vulkan_instance_get_layer_info          (GstVulkanInstance * instance,
                                                                 const gchar * name,
                                                                 gchar ** description,
                                                                 guint32 * spec_version,
                                                                 guint32 * implementation_version);
GST_VULKAN_API
gboolean            gst_vulkan_instance_enable_layer            (GstVulkanInstance * instance,
                                                                 const gchar * name);
GST_VULKAN_API
gboolean            gst_vulkan_instance_is_layer_enabled        (GstVulkanInstance * instance,
                                                                 const gchar * name);

G_END_DECLS

#endif /* __GST_VULKAN_INSTANCE_H__ */
