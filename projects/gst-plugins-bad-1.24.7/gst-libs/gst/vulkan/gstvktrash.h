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

#ifndef __GST_VULKAN_TRASH_H__
#define __GST_VULKAN_TRASH_H__

#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

/**
 * GstVulkanTrashNotify:
 * @device: the #GstVulkanDevice
 * @user_data: user data
 *
 * Since: 1.18
 */
typedef void (*GstVulkanTrashNotify) (GstVulkanDevice * device, gpointer user_data);

/**
 * GstVulkanTrash:
 *
 * Since: 1.18
 */
struct _GstVulkanTrash
{
  GstMiniObject         parent;

  GstVulkanTrashList   *cache;

  GstVulkanFence       *fence;

  GstVulkanTrashNotify  notify;
  gpointer              user_data;

  /* <private> */
  gpointer              _padding[GST_PADDING];
};

/**
 * GST_TYPE_VULKAN_TRASH:
 *
 * Since: 1.18
 */
#define GST_TYPE_VULKAN_TRASH gst_vulkan_trash_get_type()
GST_VULKAN_API
GType               gst_vulkan_trash_get_type (void);

/**
 * gst_vulkan_trash_ref: (skip)
 * @trash: a #GstVulkanTrash.
 *
 * Increases the refcount of the given trash object by one.
 *
 * Returns: (transfer full): @trash
 *
 * Since: 1.18
 */
static inline GstVulkanTrash* gst_vulkan_trash_ref(GstVulkanTrash* trash);
static inline GstVulkanTrash *
gst_vulkan_trash_ref (GstVulkanTrash * trash)
{
  return (GstVulkanTrash *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (trash));
}

/**
 * gst_vulkan_trash_unref: (skip)
 * @trash: (transfer full): a #GstVulkanTrash.
 *
 * Decreases the refcount of the trash object. If the refcount reaches 0, the
 * trash will be freed.
 *
 * Since: 1.18
 */
static inline void gst_vulkan_trash_unref(GstVulkanTrash* trash);
static inline void
gst_vulkan_trash_unref (GstVulkanTrash * trash)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (trash));
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanTrash, gst_vulkan_trash_unref)

GST_VULKAN_API
GstVulkanTrash *    gst_vulkan_trash_new                            (GstVulkanFence * fence,
                                                                     GstVulkanTrashNotify notify,
                                                                     gpointer user_data);
GST_VULKAN_API
void                gst_vulkan_trash_mini_object_unref              (GstVulkanDevice * device,
                                                                     gpointer user_data);
GST_VULKAN_API
void                gst_vulkan_trash_object_unref                   (GstVulkanDevice * device,
                                                                     gpointer user_data);
GST_VULKAN_API
GstVulkanTrash *    gst_vulkan_trash_new_free_semaphore             (GstVulkanFence * fence,
                                                                     VkSemaphore semaphore);

/**
 * gst_vulkan_trash_new_object_unref:
 * @fence: the #GstVulkanFence
 * @object: a #GstObject to unref
 *
 * Returns: (transfer full): a new #GstVulkanTrash object that will the unref
 *     @object when @fence is signalled
 *
 * Since: 1.18
 */
static inline GstVulkanTrash *
gst_vulkan_trash_new_object_unref (GstVulkanFence * fence, GstObject * object)
{
  g_return_val_if_fail (GST_IS_OBJECT (object), NULL);
  return gst_vulkan_trash_new (fence,
      (GstVulkanTrashNotify) gst_vulkan_trash_object_unref, (gpointer) object);
}

/**
 * gst_vulkan_trash_new_mini_object_unref:
 * @fence: the #GstVulkanFence
 * @object: a #GstMiniObject to unref
 *
 * Returns: (transfer full): a new #GstVulkanTrash object that will the unref
 *     @object when @fence is signalled
 *
 * Since: 1.18
 */
static inline GstVulkanTrash *
gst_vulkan_trash_new_mini_object_unref (GstVulkanFence * fence, GstMiniObject * object)
{
  return gst_vulkan_trash_new (fence,
      (GstVulkanTrashNotify) gst_vulkan_trash_mini_object_unref, (gpointer) object);
}

GST_VULKAN_API
GType gst_vulkan_trash_list_get_type (void);
/**
 * GST_TYPE_VULKAN_TRASH_LIST:
 *
 * Since: 1.18
 */
#define GST_TYPE_VULKAN_TRASH_LIST gst_vulkan_trash_list_get_type()
#define GST_VULKAN_TRASH_LIST(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VULKAN_TRASH_LIST,GstVulkanTrashList))
#define GST_VULKAN_TRASH_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VULKAN_TRASH_LIST,GstVulkanTrashListClass))
#define GST_VULKAN_TRASH_LIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VULKAN_TRASH_LIST,GstVulkanTrashListClass))
#define GST_IS_VULKAN_TRASH_LIST(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VULKAN_TRASH_LIST))
#define GST_IS_VULKAN_TRASH_LIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VULKAN_TRASH_LIST))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVulkanTrashList, gst_object_unref)

/**
 * GstVulkanTrashList:
 * @parent: the parent #GstVulkanHandle
 *
 * Since: 1.18
 */
struct _GstVulkanTrashList
{
  GstVulkanHandlePool parent;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanTrashListGC:
 * @trash_list: the #GstVulkanTrashList instance
 *
 * Remove any memory allocated by any signalled objects.
 *
 * Since: 1.18
 */
typedef void        (*GstVulkanTrashListGC)     (GstVulkanTrashList * trash_list);

/**
 * GstVulkanTrashListAdd:
 * @trash_list: the #GstVulkanTrashList instance
 * @trash: the #GstVulkanTrash to add to @trash_list
 *
 * Add @trash to @trash_list for tracking
 *
 * Returns: whether @trash could be added to @trash_list
 *
 * Since: 1.18
 */
typedef gboolean    (*GstVulkanTrashListAdd)    (GstVulkanTrashList * trash_list, GstVulkanTrash * trash);

/**
 * GstVulkanTrashListWait:
 * @trash_list: the #GstVulkanTrashList instance
 * @timeout: the timeout in ns to wait
 *
 * Wait for a most @timeout to pass for all #GstVulkanTrash objects to be
 * signalled and freed.
 *
 * Returns: whether all objects were signalled and freed within the @timeout
 *
 * Since: 1.18
 */
typedef gboolean    (*GstVulkanTrashListWait)   (GstVulkanTrashList * trash_list, guint64 timeout);

/**
 * GstVulkanTrashListClass:
 * @parent_class: the #GstVulkanHandlePoolClass
 * @add_func: the #GstVulkanTrashListAdd functions
 * @gc_func: the #GstVulkanTrashListGC function
 * @wait_func: the #GstVulkanTrashListWait function
 *
 * Since: 1.18
 */
struct _GstVulkanTrashListClass
{
  GstVulkanHandlePoolClass  parent_class;

  GstVulkanTrashListAdd     add_func;
  GstVulkanTrashListGC      gc_func;
  GstVulkanTrashListWait    wait_func;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

GST_VULKAN_API
void                gst_vulkan_trash_list_gc                        (GstVulkanTrashList * trash_list);
GST_VULKAN_API
gboolean            gst_vulkan_trash_list_wait                      (GstVulkanTrashList * trash_list,
                                                                     guint64 timeout);
GST_VULKAN_API
gboolean            gst_vulkan_trash_list_add                       (GstVulkanTrashList * trash_list,
                                                                     GstVulkanTrash * trash);
GST_VULKAN_API
GstVulkanTrash *    gst_vulkan_trash_list_acquire                   (GstVulkanTrashList * trash_list,
                                                                     GstVulkanFence * fence,
                                                                     GstVulkanTrashNotify notify,
                                                                     gpointer user_data);
/**
 * GstVulkanTrashFenceList:
 *
 * Since: 1.18
 */
/**
 * GstVulkanTrashFenceListClass:
 *
 * Since: 1.18
 */
GST_VULKAN_API
G_DECLARE_FINAL_TYPE (GstVulkanTrashFenceList, gst_vulkan_trash_fence_list, GST, VULKAN_TRASH_FENCE_LIST, GstVulkanTrashList);
GST_VULKAN_API
GstVulkanTrashList * gst_vulkan_trash_fence_list_new                (void);

G_END_DECLS

#endif /* __GST_VULKAN_TRASH_H__ */
