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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvktrash.h"
#include "gstvkhandle.h"

/**
 * SECTION:vktrash
 * @title: GstVulkanTrash
 * @short_description: Vulkan helper object for freeing resources after a #GstVulkanFence is signalled
 * @see_also: #GstVulkanFence, #GstVulkanQueue
 *
 * #GstVulkanTrash is a helper object for freeing resources after a
 * #GstVulkanFence is signalled.
 */

GST_DEBUG_CATEGORY (gst_debug_vulkan_trash);
#define GST_CAT_DEFAULT gst_debug_vulkan_trash

#define gst_vulkan_trash_release(c,t) \
    gst_vulkan_handle_pool_release (GST_VULKAN_HANDLE_POOL_CAST (c), t);

static void
_init_debug (void)
{
  static gsize init;

  if (g_once_init_enter (&init)) {
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_trash,
        "vulkantrash", 0, "Vulkan Trash");
    g_once_init_leave (&init, 1);
  }
}

static gboolean
gst_vulkan_trash_dispose (GstVulkanTrash * trash)
{
  GstVulkanTrashList *cache;

  /* no pool, do free */
  if ((cache = trash->cache) == NULL)
    return TRUE;

  /* keep the buffer alive */
  gst_vulkan_trash_ref (trash);
  /* return the trash object to the pool */
  gst_vulkan_trash_release (cache, trash);

  return FALSE;
}

static void
gst_vulkan_trash_deinit (GstVulkanTrash * trash)
{
  if (trash->fence) {
    g_warn_if_fail (gst_vulkan_fence_is_signaled (trash->fence));
    gst_vulkan_fence_unref (trash->fence);
    trash->fence = NULL;
  }

  trash->notify = NULL;
  trash->user_data = NULL;
}

static void
gst_vulkan_trash_free (GstMiniObject * object)
{
  GstVulkanTrash *trash = (GstVulkanTrash *) object;

  GST_TRACE ("Freeing trash object %p with fence %" GST_PTR_FORMAT, trash,
      trash->fence);

  gst_vulkan_trash_deinit (trash);

  g_free (trash);
}

static void
gst_vulkan_trash_init (GstVulkanTrash * trash, GstVulkanFence * fence,
    GstVulkanTrashNotify notify, gpointer user_data)
{
  g_return_if_fail (fence != NULL);
  g_return_if_fail (GST_IS_VULKAN_DEVICE (fence->device));
  g_return_if_fail (notify != NULL);

  gst_mini_object_init ((GstMiniObject *) trash, 0,
      gst_vulkan_trash_get_type (), NULL,
      (GstMiniObjectDisposeFunction) gst_vulkan_trash_dispose,
      (GstMiniObjectFreeFunction) gst_vulkan_trash_free);
  GST_TRACE ("Initializing trash object %p with fence %" GST_PTR_FORMAT
      " on device %" GST_PTR_FORMAT, trash, fence, fence->device);
  trash->fence = gst_vulkan_fence_ref (fence);
  trash->notify = notify;
  trash->user_data = user_data;
}

/**
 * gst_vulkan_trash_new:
 * @fence: a #GstVulkanFence
 * @notify: (scope async): a #GstVulkanTrashNotify
 * @user_data: (closure notify): user data for @notify
 *
 * Create and return a new #GstVulkanTrash object that will stores a callback
 * to call when @fence is signalled.
 *
 * Returns: (transfer full): a new #GstVulkanTrash
 *
 * Since: 1.18
 */
GstVulkanTrash *
gst_vulkan_trash_new (GstVulkanFence * fence, GstVulkanTrashNotify notify,
    gpointer user_data)
{
  GstVulkanTrash *ret = NULL;

  g_return_val_if_fail (fence != NULL, NULL);
  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (fence->device), NULL);
  g_return_val_if_fail (notify != NULL, NULL);

  _init_debug ();

  ret = g_new0 (GstVulkanTrash, 1);
  GST_TRACE ("Creating new trash object %p with fence %" GST_PTR_FORMAT
      " on device %" GST_PTR_FORMAT, ret, fence, fence->device);
  gst_vulkan_trash_init (ret, fence, notify, user_data);

  return ret;
}

#if GLIB_SIZEOF_VOID_P == 8
# define PUSH_NON_DISPATCHABLE_HANDLE_TO_GPOINTER(pointer, handle) pointer = (gpointer) handle
# define TAKE_NON_DISPATCHABLE_HANDLE_FROM_GPOINTER(handle, type, pointer) handle = (type) pointer
#else
# define PUSH_NON_DISPATCHABLE_HANDLE_TO_GPOINTER(pointer, handle) \
    G_STMT_START { \
      pointer = g_new0(guint64, 1); \
      *((GstVulkanHandleTypedef *) pointer) = (GstVulkanHandleTypedef) handle; \
    } G_STMT_END
# define TAKE_NON_DISPATCHABLE_HANDLE_FROM_GPOINTER(handle, type, pointer) \
    G_STMT_START { \
      handle = *((type *) pointer); \
      g_free (pointer); \
    } G_STMT_END
#endif

#define FREE_DESTROY_FUNC(func, type, type_name) \
static void \
G_PASTE(_free_,type_name) (GstVulkanDevice * device, gpointer resource_handle) \
{ \
  type resource; \
  TAKE_NON_DISPATCHABLE_HANDLE_FROM_GPOINTER(resource, type, resource_handle); \
  GST_TRACE_OBJECT (device, "Freeing vulkan " G_STRINGIFY (type) \
      " %" GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT, resource); \
  func (device->device, resource, NULL); \
} \
GstVulkanTrash * \
G_PASTE(gst_vulkan_trash_new_free_,type_name) (GstVulkanFence * fence, \
    type type_name) \
{ \
  GstVulkanTrash *trash; \
  gpointer handle_data; \
  PUSH_NON_DISPATCHABLE_HANDLE_TO_GPOINTER(handle_data, type_name); \
  g_return_val_if_fail (type_name != VK_NULL_HANDLE, VK_NULL_HANDLE); \
  trash = gst_vulkan_trash_new (fence, \
      (GstVulkanTrashNotify) G_PASTE(_free_,type_name), handle_data); \
  return trash; \
}

/**
 * gst_vulkan_trash_new_free_semaphore:
 * @fence: the #GstVulkanFence
 * @semaphore: a `VkSemaphore` to free
 *
 * Returns: (transfer full): a new #GstVulkanTrash object that will the free
 *     @semaphore when @fence is signalled
 *
 * Since: 1.18
 */
FREE_DESTROY_FUNC (vkDestroySemaphore, VkSemaphore, semaphore);
#define FREE_WITH_VK_PARENT(func, type, type_name, parent_type) \
struct G_PASTE(free_parent_info_,type_name) \
{ \
  parent_type parent; \
  type resource; \
}; \
static void \
G_PASTE(_free_,type_name) (GstVulkanDevice * device, struct G_PASTE(free_parent_info_,type_name) *info) \
{ \
  GST_TRACE_OBJECT (device, "Freeing vulkan " G_STRINGIFY (type) \
      " %" GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT, info->resource); \
  func (device->device, info->parent, 1, &info->resource); \
  g_free (info); \
} \
GstVulkanTrash * \
G_PASTE(gst_vulkan_trash_new_free_,type_name) (GstVulkanFence * fence, \
    parent_type parent, type type_name) \
{ \
  struct G_PASTE(free_parent_info_,type_name) *info; \
  GstVulkanTrash *trash; \
  g_return_val_if_fail (type_name != VK_NULL_HANDLE, VK_NULL_HANDLE); \
  info = g_new0 (struct G_PASTE(free_parent_info_,type_name), 1); \
  /* FIXME: keep parent alive ? */\
  info->parent = parent; \
  info->resource = type_name; \
  trash = gst_vulkan_trash_new (fence, \
      (GstVulkanTrashNotify) G_PASTE(_free_,type_name), info); \
  return trash; \
}

/**
 * gst_vulkan_trash_object_unref:
 * @device: the #GstVulkanDevice
 * @user_data: the #GstMiniObject
 *
 * A #GstVulkanTrashNotify implementation for unreffing a #GstObject when the
 * associated #GstVulkanFence is signalled
 *
 * Since: 1.18
 */
void
gst_vulkan_trash_object_unref (GstVulkanDevice * device, gpointer user_data)
{
  gst_object_unref ((GstObject *) user_data);
}

/**
 * gst_vulkan_trash_mini_object_unref:
 * @device: the #GstVulkanDevice
 * @user_data: the #GstMiniObject
 *
 * A #GstVulkanTrashNotify implementation for unreffing a #GstMiniObject when the
 * associated #GstVulkanFence is signalled
 *
 * Since: 1.18
 */
void
gst_vulkan_trash_mini_object_unref (GstVulkanDevice * device,
    gpointer user_data)
{
  gst_mini_object_unref ((GstMiniObject *) user_data);
}

G_DEFINE_TYPE_WITH_CODE (GstVulkanTrashList, gst_vulkan_trash_list,
    GST_TYPE_VULKAN_HANDLE_POOL, _init_debug ());

/**
 * gst_vulkan_trash_list_gc:
 * @trash_list: the #GstVulkanTrashList
 *
 * Remove any stored #GstVulkanTrash objects that have had their associated
 * #GstVulkanFence signalled.
 *
 * Since: 1.18
 */
void
gst_vulkan_trash_list_gc (GstVulkanTrashList * trash_list)
{
  GstVulkanTrashListClass *trash_class;
  g_return_if_fail (GST_IS_VULKAN_TRASH_LIST (trash_list));
  trash_class = GST_VULKAN_TRASH_LIST_GET_CLASS (trash_list);
  g_return_if_fail (trash_class->gc_func != NULL);

  trash_class->gc_func (trash_list);
}

/**
 * gst_vulkan_trash_list_add:
 * @trash_list: the #GstVulkanTrashList
 * @trash: #GstVulkanTrash object to add to the list
 *
 * Returns: whether @trash could be added to @trash_list
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_trash_list_add (GstVulkanTrashList * trash_list,
    GstVulkanTrash * trash)
{
  GstVulkanTrashListClass *trash_class;
  g_return_val_if_fail (GST_IS_VULKAN_TRASH_LIST (trash_list), FALSE);
  trash_class = GST_VULKAN_TRASH_LIST_GET_CLASS (trash_list);
  g_return_val_if_fail (trash_class->add_func != NULL, FALSE);

  return trash_class->add_func (trash_list, trash);
}

/**
 * gst_vulkan_trash_list_wait:
 * @trash_list: the #GstVulkanTrashList
 * @timeout: timeout in ns to wait, -1 for indefinite
 *
 * Returns: whether the wait succeeded in waiting for all objects to be freed.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_trash_list_wait (GstVulkanTrashList * trash_list, guint64 timeout)
{
  GstVulkanTrashListClass *trash_class;
  g_return_val_if_fail (GST_IS_VULKAN_TRASH_LIST (trash_list), FALSE);
  trash_class = GST_VULKAN_TRASH_LIST_GET_CLASS (trash_list);
  g_return_val_if_fail (trash_class->wait_func != NULL, FALSE);

  return trash_class->wait_func (trash_list, timeout);
}

static gpointer
gst_vulkan_trash_list_alloc_impl (GstVulkanHandlePool * pool, GError ** error)
{
  return g_new0 (GstVulkanTrash, 1);
}

static void
gst_vulkan_trash_list_release_impl (GstVulkanHandlePool * pool, gpointer handle)
{
  GstVulkanTrash *trash = handle;

  GST_TRACE_OBJECT (pool, "reset trash object %p", trash);

  gst_vulkan_trash_deinit (trash);
  gst_clear_object (&trash->cache);

  GST_VULKAN_HANDLE_POOL_CLASS (gst_vulkan_trash_list_parent_class)->release
      (pool, handle);
}

static void
gst_vulkan_trash_list_free_impl (GstVulkanHandlePool * pool, gpointer handle)
{
  GstVulkanTrash *trash = handle;

  gst_vulkan_trash_unref (trash);
}

static void
gst_vulkan_trash_list_class_init (GstVulkanTrashListClass * klass)
{
  GstVulkanHandlePoolClass *pool_class = (GstVulkanHandlePoolClass *) klass;

  pool_class->alloc = gst_vulkan_trash_list_alloc_impl;
  pool_class->release = gst_vulkan_trash_list_release_impl;
  pool_class->free = gst_vulkan_trash_list_free_impl;
}

static void
gst_vulkan_trash_list_init (GstVulkanTrashList * trash_list)
{
}

/**
 * gst_vulkan_trash_list_acquire:
 * @trash_list: a #GstVulkanTrashList
 * @fence: a #GstVulkanFence to wait for signalling
 * @notify: (scope async): notify function for when @fence is signalled
 * @user_data: user data for @notify
 *
 * Returns: (transfer full): a new or reused #GstVulkanTrash for the provided
 *          parameters.
 *
 * Since: 1.18
 */
GstVulkanTrash *
gst_vulkan_trash_list_acquire (GstVulkanTrashList * trash_list,
    GstVulkanFence * fence, GstVulkanTrashNotify notify, gpointer user_data)
{
  GstVulkanHandlePool *pool = GST_VULKAN_HANDLE_POOL (trash_list);
  GstVulkanHandlePoolClass *pool_class;
  GstVulkanTrash *trash;

  g_return_val_if_fail (GST_IS_VULKAN_TRASH_LIST (trash_list), NULL);

  pool_class = GST_VULKAN_HANDLE_POOL_GET_CLASS (trash_list);

  trash = pool_class->acquire (pool, NULL);
  gst_vulkan_trash_init (trash, fence, notify, user_data);
  trash->cache = gst_object_ref (trash_list);

  GST_TRACE_OBJECT (trash_list, "acquired trash object %p", trash);

  return trash;
}

typedef struct _GstVulkanTrashFenceList GstVulkanTrashFenceList;

struct _GstVulkanTrashFenceList
{
  GstVulkanTrashList parent;

  GList *list;
};

G_DEFINE_TYPE (GstVulkanTrashFenceList, gst_vulkan_trash_fence_list,
    GST_TYPE_VULKAN_TRASH_LIST);

static void
gst_vulkan_trash_fence_list_gc (GstVulkanTrashList * trash_list)
{
  GstVulkanTrashFenceList *fence_list = (GstVulkanTrashFenceList *) trash_list;
  GList *l = fence_list->list;

  while (l) {
    GstVulkanTrash *trash = l->data;

    if (gst_vulkan_fence_is_signaled (trash->fence)) {
      GList *next = g_list_next (l);
      GST_TRACE_OBJECT (fence_list, "fence %" GST_PTR_FORMAT " has been "
          "signalled, notifying", trash->fence);
      trash->notify (trash->fence->device, trash->user_data);
      gst_vulkan_trash_unref (trash);
      fence_list->list = g_list_delete_link (fence_list->list, l);
      l = next;
    } else {
      l = g_list_next (l);
    }
  }
}

static gboolean
gst_vulkan_trash_fence_list_wait (GstVulkanTrashList * trash_list,
    guint64 timeout)
{
  GstVulkanTrashFenceList *fence_list = (GstVulkanTrashFenceList *) trash_list;
  VkResult err = VK_SUCCESS;
  guint i, n;

  /* remove all the previously signaled fences */
  gst_vulkan_trash_fence_list_gc (trash_list);

  n = g_list_length (fence_list->list);
  if (n > 0) {
    VkFence *fences;
    GstVulkanDevice *device = NULL;
    GList *l = NULL;

    fences = g_new0 (VkFence, n);
    for (i = 0, l = fence_list->list; i < n; i++, l = g_list_next (l)) {
      GstVulkanTrash *trash = l->data;

      if (device == NULL)
        device = trash->fence->device;

      fences[i] = trash->fence->fence;

      /* only support waiting on fences from the same device */
      g_assert (device == trash->fence->device);
    }

    GST_TRACE_OBJECT (trash_list, "Waiting on %d fences with timeout %"
        GST_TIME_FORMAT, n, GST_TIME_ARGS (timeout));
    err = vkWaitForFences (device->device, n, fences, TRUE, timeout);
    g_free (fences);

    gst_vulkan_trash_fence_list_gc (trash_list);
  }

  return err == VK_SUCCESS;
}

static gboolean
gst_vulkan_trash_fence_list_add (GstVulkanTrashList * trash_list,
    GstVulkanTrash * trash)
{
  GstVulkanTrashFenceList *fence_list = (GstVulkanTrashFenceList *) trash_list;

  g_return_val_if_fail (GST_MINI_OBJECT_TYPE (trash) == GST_TYPE_VULKAN_TRASH,
      FALSE);

  /* XXX: do something better based on the actual fence */
  fence_list->list = g_list_prepend (fence_list->list, trash);

  return TRUE;
}

static void
gst_vulkan_trash_fence_list_finalize (GObject * object)
{
  GstVulkanTrashList *trash_list = (GstVulkanTrashList *) object;
  GstVulkanTrashFenceList *fence_list = (GstVulkanTrashFenceList *) object;

  gst_vulkan_trash_fence_list_gc (trash_list);
  g_warn_if_fail (fence_list->list == NULL);

  G_OBJECT_CLASS (gst_vulkan_trash_fence_list_parent_class)->finalize (object);
}

static void
gst_vulkan_trash_fence_list_class_init (GstVulkanTrashFenceListClass * klass)
{
  GstVulkanTrashListClass *trash_class = (GstVulkanTrashListClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  trash_class->add_func = gst_vulkan_trash_fence_list_add;
  trash_class->gc_func = gst_vulkan_trash_fence_list_gc;
  trash_class->wait_func = gst_vulkan_trash_fence_list_wait;

  object_class->finalize = gst_vulkan_trash_fence_list_finalize;
}

static void
gst_vulkan_trash_fence_list_init (GstVulkanTrashFenceList * trash_list)
{
}

/**
 * gst_vulkan_trash_fence_list_new:
 *
 * Returns: (transfer full): a new #gst_vulkan_trash_fence_list_new
 *
 * Since: a.18
 */
GstVulkanTrashList *
gst_vulkan_trash_fence_list_new (void)
{
  GstVulkanTrashList *ret;

  ret = g_object_new (gst_vulkan_trash_fence_list_get_type (), NULL);
  gst_object_ref_sink (ret);

  return ret;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanTrash, gst_vulkan_trash);
