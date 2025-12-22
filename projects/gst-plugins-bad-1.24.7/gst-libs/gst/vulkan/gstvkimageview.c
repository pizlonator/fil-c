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

#include <gst/vulkan/vulkan.h>

/**
 * SECTION:vkimageview
 * @title: GstVulkanImageView
 * @short_description: wrapper for `VkImageView`'s
 * @see_also: #GstVulkanImageMemory, #GstVulkanDevice
 *
 * #GstVulkanImageView is a wrapper around a `VkImageView` mostly for
 * usage across element boundaries with #GstVulkanImageMemory
 */

#define GST_CAT_DEFUALT GST_CAT_VULKAN_IMAGE_VIEW
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFUALT);

static void
init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_VULKAN_IMAGE_VIEW, "vulkanimageview",
        0, "Vulkan Image View");
    g_once_init_leave (&_init, 1);
  }
}

extern void
gst_vulkan_image_memory_release_view (GstVulkanImageMemory * image,
    GstVulkanImageView * view);

static gboolean
gst_vulkan_image_view_dispose (GstVulkanImageView * view)
{
  GstVulkanImageMemory *image;

  if ((image = view->image) == NULL)
    return TRUE;

  gst_vulkan_image_view_ref (view);
  gst_vulkan_image_memory_release_view (image, view);

  return FALSE;
}

static void
gst_vulkan_image_view_free (GstVulkanImageView * view)
{
  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_VIEW, "freeing image view:%p ", view);

  if (view->view)
    vkDestroyImageView (view->device->device, view->view, NULL);

  if (view->image) {
    gst_memory_unref (GST_MEMORY_CAST (view->image));
  }
  view->image = NULL;
  gst_clear_object (&view->device);

  g_free (view);
}

/**
 * gst_vulkan_image_view_new:
 * @image: a #GstVulkanImageMemory to create the new view from
 * @create_info: the creation information to create the view from
 *
 * Returns: (transfer full): A new #GstVulkanImageView from @image and
 *          @create_info
 *
 * Since: 1.18
 */
GstVulkanImageView *
gst_vulkan_image_view_new (GstVulkanImageMemory * image,
    const VkImageViewCreateInfo * create_info)
{
  GstVulkanImageView *view;
  GError *error = NULL;
  VkResult err;

  g_return_val_if_fail (create_info != NULL, NULL);
  g_return_val_if_fail (gst_is_vulkan_image_memory (GST_MEMORY_CAST (image)),
      NULL);

  init_debug ();

  view = g_new0 (GstVulkanImageView, 1);

  gst_mini_object_init ((GstMiniObject *) view, 0,
      gst_vulkan_image_view_get_type (), NULL,
      (GstMiniObjectDisposeFunction) gst_vulkan_image_view_dispose,
      (GstMiniObjectFreeFunction) gst_vulkan_image_view_free);

  err =
      vkCreateImageView (image->device->device, create_info, NULL, &view->view);
  if (gst_vulkan_error_to_g_error (err, &error, "vkImageCreateView") < 0)
    goto vk_error;

  view->image =
      (GstVulkanImageMemory *) gst_memory_ref (GST_MEMORY_CAST (image));
  view->device = gst_object_ref (image->device);
  view->create_info = *create_info;
  /* we cannot keep this as it may point to stack allocated memory */
  view->create_info.pNext = NULL;

  GST_CAT_TRACE (GST_CAT_VULKAN_IMAGE_VIEW, "new image view for image: %p",
      image);

  return view;

vk_error:
  {
    GST_CAT_ERROR (GST_CAT_VULKAN_IMAGE_VIEW,
        "Failed to allocate image memory %s", error->message);
    g_clear_error (&error);
    goto error;
  }

error:
  {
    g_free (view);
    return NULL;
  }
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanImageView, gst_vulkan_image_view);
