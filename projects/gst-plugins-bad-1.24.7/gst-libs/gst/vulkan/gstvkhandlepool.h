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

#ifndef __GST_VULKAN_HANDLE_POOL_H__
#define __GST_VULKAN_HANDLE_POOL_H__

#include <gst/gst.h>

#include <gst/vulkan/vulkan_fwd.h>
#include <gst/vulkan/gstvkapi.h>

G_BEGIN_DECLS

GST_VULKAN_API
GType gst_vulkan_handle_pool_get_type (void);
#define GST_TYPE_VULKAN_HANDLE_POOL            (gst_vulkan_handle_pool_get_type())
#define GST_VULKAN_HANDLE_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_HANDLE_POOL,GstVulkanHandlePool))
#define GST_VULKAN_HANDLE_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_HANDLE_POOL,GstVulkanHandlePoolClass))
#define GST_IS_VULKAN_HANDLE_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_HANDLE_POOL))
#define GST_IS_VULKAN_HANDLE_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_HANDLE_POOL))
#define GST_VULKAN_HANDLE_POOL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_HANDLE_POOL, GstVulkanHandlePoolClass))
/**
 * GST_VULKAN_HANDLE_POOL_CAST:
 *
 * Since: 1.18
 */
#define GST_VULKAN_HANDLE_POOL_CAST(o)         ((GstVulkanHandlePool *) o)

/**
 * GstVulkanHandlePool:
 * @parent: the parent #GstObject
 * @device: the #GstVulkanDevice handles are allocated from
 * @outstanding: the collection of outstanding handles
 * @available: the collection of allocated and available handles
 *
 * Since: 1.18
 */
struct _GstVulkanHandlePool
{
  GstObject                 parent;

  GstVulkanDevice          *device;

  /* <protected> */
  GPtrArray                *outstanding;
  GPtrArray                *available;

  /* <private> */
  gpointer                  _padding[GST_PADDING];
};

/**
 * GstVulkanHandlePoolClass:
 * @parent: the parent #GstObjectClass
 * @alloc: allocate a new handle
 * @acquire: acquire a handle for usage
 * @release: release a handle for possible reuse at the next call to @acquire
 * @free: free a handle
 *
 * Since: 1.18
 */
struct _GstVulkanHandlePoolClass
{
  GstObjectClass        parent;

  gpointer              (*alloc)            (GstVulkanHandlePool * pool, GError ** error);
  gpointer              (*acquire)          (GstVulkanHandlePool * pool, GError ** error);
  void                  (*release)          (GstVulkanHandlePool * pool, gpointer handle);
  void                  (*free)             (GstVulkanHandlePool * pool, gpointer handle);

  /* <private> */
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstVulkanHandlePool, gst_object_unref)

GST_VULKAN_API
gpointer        gst_vulkan_handle_pool_alloc        (GstVulkanHandlePool * pool, GError ** error);
GST_VULKAN_API
gpointer        gst_vulkan_handle_pool_acquire      (GstVulkanHandlePool * pool, GError ** error);
GST_VULKAN_API
void            gst_vulkan_handle_pool_release      (GstVulkanHandlePool * pool, gpointer handle);

G_END_DECLS

#endif /* _GST_VULKAN_HANDLE_H_ */
