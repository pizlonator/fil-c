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
 * SECTION:vkhandle
 * @title: GstVulkanHandle
 * @short_description: Vulkan handles
 * @see_also: #GstVulkanHandlePool, #GstVulkanDevice
 *
 * #GstVulkanHandle holds information about a vulkan handle.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkhandle.h"
#include "gstvkdevice.h"

#define GST_CAT_DEFAULT gst_debug_vulkan_handle
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

static void
init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanhandle", 0,
        "Vulkan handle");
    g_once_init_leave (&_init, 1);
  }
}

static void
gst_vulkan_handle_free (GstVulkanHandle * handle)
{
  GST_TRACE ("Freeing %p", handle);

  if (handle->notify)
    handle->notify (handle, handle->user_data);

  gst_clear_object (&handle->device);

  g_free (handle);
}

static void
gst_vulkan_handle_init (GstVulkanHandle * handle, GstVulkanDevice * device,
    GstVulkanHandleType type, GstVulkanHandleTypedef handle_val,
    GstVulkanHandleDestroyNotify notify, gpointer user_data)
{
  handle->device = gst_object_ref (device);
  handle->type = type;
  handle->handle = handle_val;
  handle->notify = notify;
  handle->user_data = user_data;

  init_debug ();

  GST_TRACE ("new %p", handle);

  gst_mini_object_init (&handle->parent, 0, GST_TYPE_VULKAN_HANDLE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_vulkan_handle_free);
}

/**
 * gst_vulkan_handle_new_wrapped:
 * @handle: a Vulkan handle
 * @notify: (scope call): a #GDestroyNotify
 * @user_data: data to pass to @notify
 *
 * Returns: (transfer full): a new #GstVulkanHandle wrapping @handle
 *
 * Since: 1.18
 */
GstVulkanHandle *
gst_vulkan_handle_new_wrapped (GstVulkanDevice * device,
    GstVulkanHandleType type, GstVulkanHandleTypedef handle,
    GstVulkanHandleDestroyNotify notify, gpointer user_data)
{
  GstVulkanHandle *ret;

  ret = g_new0 (GstVulkanHandle, 1);
  gst_vulkan_handle_init (ret, device, type, handle, notify, user_data);

  return ret;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanHandle, gst_vulkan_handle);

/**
 * gst_vulkan_handle_free_descriptor_set_layout:
 * @handle: a #GstVulkanHandle containing a vulkan `VkDescriptorSetLayout`
 * @user_data: callback user data
 *
 * Frees the descriptor set layout in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_descriptor_set_layout (GstVulkanHandle * handle,
    gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type ==
      GST_VULKAN_HANDLE_TYPE_DESCRIPTOR_SET_LAYOUT);

  vkDestroyDescriptorSetLayout (handle->device->device,
      (VkDescriptorSetLayout) handle->handle, NULL);
}

/**
 * gst_vulkan_handle_free_pipeline:
 * @handle: a #GstVulkanHandle containing a vulkan `VkPipeline`
 * @user_data: callback user data
 *
 * Frees the pipeline in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_pipeline (GstVulkanHandle * handle, gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_PIPELINE);

  vkDestroyPipeline (handle->device->device, (VkPipeline) handle->handle, NULL);
}

/**
 * gst_vulkan_handle_free_pipeline_layout:
 * @handle: a #GstVulkanHandle containing a vulkan `VkPipelineLayout`
 * @user_data: callback user data
 *
 * Frees the pipeline layout in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_pipeline_layout (GstVulkanHandle * handle,
    gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_PIPELINE_LAYOUT);

  vkDestroyPipelineLayout (handle->device->device,
      (VkPipelineLayout) handle->handle, NULL);
}

/**
 * gst_vulkan_handle_free_render_pass:
 * @handle: a #GstVulkanHandle containing a vulkan `VkRenderPass`
 * @user_data: callback user data
 *
 * Frees the render pass in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_render_pass (GstVulkanHandle * handle,
    gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_RENDER_PASS);

  vkDestroyRenderPass (handle->device->device,
      (VkRenderPass) handle->handle, NULL);
}

/**
 * gst_vulkan_handle_free_sampler:
 * @handle: a #GstVulkanHandle containing a vulkan `VkSampler`
 * @user_data: callback user data
 *
 * Frees the sampler in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_sampler (GstVulkanHandle * handle, gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_SAMPLER);

  vkDestroySampler (handle->device->device, (VkSampler) handle->handle, NULL);
}

/**
 * gst_vulkan_handle_free_framebuffer:
 * @handle: a #GstVulkanHandle containing a vulkan `VkFramebuffer`
 * @user_data: callback user data
 *
 * Frees the framebuffer in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_framebuffer (GstVulkanHandle * handle,
    gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_FRAMEBUFFER);

  vkDestroyFramebuffer (handle->device->device, (VkFramebuffer) handle->handle,
      NULL);
}

/**
 * gst_vulkan_handle_free_shader:
 * @handle: a #GstVulkanHandle containing a vulkan `VkFramebuffer`
 * @user_data: callback user data
 *
 * Frees the shader in @handle
 *
 * Since: 1.18
 */
void
gst_vulkan_handle_free_shader (GstVulkanHandle * handle, gpointer user_data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_SHADER);

  vkDestroyShaderModule (handle->device->device,
      (VkShaderModule) handle->handle, NULL);
}
