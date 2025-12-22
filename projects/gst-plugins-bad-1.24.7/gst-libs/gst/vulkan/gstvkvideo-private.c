/*
 * GStreamer
 * Copyright (C) 2023 Igalia, S.L.
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

#include "gstvkvideo-private.h"
#include "gstvkinstance.h"

#include <vk_video/vulkan_video_codecs_common.h>

/* *INDENT-OFF* */
const VkExtensionProperties _vk_codec_extensions[] = {
  [GST_VK_VIDEO_EXTENSION_DECODE_H264] = {
    .extensionName = VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME,
    .specVersion = VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION,
  },
  [GST_VK_VIDEO_EXTENSION_DECODE_H265] = {
    .extensionName = VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME,
    .specVersion = VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION,
  },
};

const VkComponentMapping _vk_identity_component_map = {
    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
};
/* *INDENT-ON* */

gboolean
gst_vulkan_video_get_vk_functions (GstVulkanInstance * instance,
    GstVulkanVideoFunctions * vk_funcs)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (vk_funcs, FALSE);

#define GET_PROC_ADDRESS_REQUIRED(name)                                 \
  G_STMT_START {                                                        \
    const char *fname = "vk" G_STRINGIFY (name) "KHR";                  \
    vk_funcs->G_PASTE (, name) = gst_vulkan_instance_get_proc_address (instance, fname); \
    if (!vk_funcs->G_PASTE(, name)) {                                   \
      GST_ERROR_OBJECT (instance, "Failed to find required function %s", fname); \
      goto bail;                                                        \
    }                                                                   \
  } G_STMT_END;
  GST_VULKAN_VIDEO_FN_LIST (GET_PROC_ADDRESS_REQUIRED)
#undef GET_PROC_ADDRESS_REQUIRED
      ret = TRUE;

bail:
  return ret;
}

static void
gst_vulkan_handle_free_video_session (GstVulkanHandle * handle, gpointer data)
{
  PFN_vkDestroyVideoSessionKHR vkDestroyVideoSession;

  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type == GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION);
  g_return_if_fail (handle->user_data);

  vkDestroyVideoSession = handle->user_data;
  vkDestroyVideoSession (handle->device->device,
      (VkVideoSessionKHR) handle->handle, NULL);
}

gboolean
gst_vulkan_video_session_create (GstVulkanVideoSession * session,
    GstVulkanDevice * device, GstVulkanVideoFunctions * vk,
    VkVideoSessionCreateInfoKHR * session_create, GError ** error)
{
  VkVideoSessionKHR vk_session;
  VkMemoryRequirements2 *mem_req = NULL;
  VkVideoSessionMemoryRequirementsKHR *mem = NULL;
  VkBindVideoSessionMemoryInfoKHR *bind_mem = NULL;
  VkResult res;
  guint32 i, n_mems, index;
  gboolean ret = FALSE;

  g_return_val_if_fail (session && !session->session, FALSE);
  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (vk, FALSE);
  g_return_val_if_fail (session_create, FALSE);

  res = vk->CreateVideoSession (device->device, session_create, NULL,
      &vk_session);
  if (gst_vulkan_error_to_g_error (res, error, "vkCreateVideoSessionKHR")
      != VK_SUCCESS)
    return FALSE;

  session->session = gst_vulkan_handle_new_wrapped (device,
      GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION, (GstVulkanHandleTypedef) vk_session,
      gst_vulkan_handle_free_video_session, vk->DestroyVideoSession);

  res = vk->GetVideoSessionMemoryRequirements (device->device, vk_session,
      &n_mems, NULL);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkGetVideoSessionMemoryRequirementsKHR") != VK_SUCCESS)
    goto beach;

  session->buffer = gst_buffer_new ();
  mem_req = g_new (VkMemoryRequirements2, n_mems);
  mem = g_new (VkVideoSessionMemoryRequirementsKHR, n_mems);
  bind_mem = g_new (VkBindVideoSessionMemoryInfoKHR, n_mems);

  for (i = 0; i < n_mems; i++) {
    /* *INDENT-OFF* */
    mem_req[i] = (VkMemoryRequirements2) {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    mem[i] = (VkVideoSessionMemoryRequirementsKHR) {
      .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR,
      .memoryRequirements = mem_req[i].memoryRequirements,
    };
    /* *INDENT-ON* */
  }
  res = vk->GetVideoSessionMemoryRequirements (device->device, vk_session,
      &n_mems, mem);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkGetVideoSessionMemoryRequirementsKHR") != VK_SUCCESS)
    goto beach;

  for (i = 0; i < n_mems; i++) {
    GstMemory *vk_mem;
    VkPhysicalDeviceMemoryProperties *props;
    VkMemoryPropertyFlags prop_flags;

    props = &device->physical_device->memory_properties;
    prop_flags = props->memoryTypes[i].propertyFlags;
    if (!gst_vulkan_memory_find_memory_type_index_with_requirements (device,
            &mem[i].memoryRequirements, G_MAXUINT32, &index)) {
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Cannot find memory type for video session");
      goto beach;
    }

    vk_mem = gst_vulkan_memory_alloc (device, index, NULL,
        mem[i].memoryRequirements.size, prop_flags);
    if (!vk_mem) {
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Cannot allocate memory for video sesson");
      goto beach;
    }
    gst_buffer_append_memory (session->buffer, vk_mem);

    /* *INDENT-OFF* */
    bind_mem[i] = (VkBindVideoSessionMemoryInfoKHR) {
      .sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR,
      .memory = ((GstVulkanMemory *) vk_mem)->mem_ptr,
      .memoryBindIndex = mem[i].memoryBindIndex,
      .memorySize = vk_mem->size,
    };
    /* *INDENT-ON* */
  }

  res =
      vk->BindVideoSessionMemory (device->device, vk_session, n_mems, bind_mem);
  if (gst_vulkan_error_to_g_error (res, error, "vkBindVideoSessionMemoryKHR")
      != VK_SUCCESS)
    goto beach;

  ret = TRUE;

beach:
  g_free (mem_req);
  g_free (mem);
  g_free (bind_mem);

  return ret;
}

void
gst_vulkan_video_session_destroy (GstVulkanVideoSession * session)
{
  g_return_if_fail (session);

  gst_clear_vulkan_handle (&session->session);
  gst_clear_buffer (&session->buffer);
}

GstBuffer *
gst_vulkan_video_codec_buffer_new (GstVulkanDevice * device,
    const GstVulkanVideoProfile * profile, VkBufferUsageFlags usage, gsize size)
{
  GstMemory *mem;
  GstBuffer *buf;
  VkVideoProfileListInfoKHR profile_list = {
    .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,
    .profileCount = 1,
    .pProfiles = &profile->profile,
  };
  VkBufferCreateInfo buf_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = &profile_list,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .size = size,
  };

  buf_info.size = MAX (buf_info.size, 1024 * 1024);

  mem = gst_vulkan_buffer_memory_alloc_with_buffer_info (device, &buf_info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  if (!mem)
    return NULL;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, mem);
  return buf;
}
