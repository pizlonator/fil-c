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

#pragma once

#include <gst/gst.h>
#include <gst/vulkan/gstvkapi.h>
#include <gst/vulkan/gstvkvideoutils.h>

G_BEGIN_DECLS

#define VK_CODEC_VER_MAJ(ver) (ver >> 22)
#define VK_CODEC_VER_MIN(ver) ((ver >> 12) & ((1 << 10) - 1))
#define VK_CODEC_VER_REV(ver) (ver & ((1 << 12) - 1))
#define VK_CODEC_VERSION(ver) VK_CODEC_VER_MAJ (ver), VK_CODEC_VER_MIN (ver), VK_CODEC_VER_REV (ver)

typedef struct _GstVulkanVideoFunctions GstVulkanVideoFunctions;
typedef struct _GstVulkanVideoSession GstVulkanVideoSession;

struct _GstVulkanVideoSession
{
  GstVulkanHandle *session;
  GstBuffer *buffer;
};

typedef enum {
  GST_VK_VIDEO_EXTENSION_DECODE_H264,
  GST_VK_VIDEO_EXTENSION_DECODE_H265,
} GST_VK_VIDEO_EXTENSIONS;

#define GST_VULKAN_VIDEO_FN_LIST(V)                                            \
  V(GetPhysicalDeviceVideoFormatProperties)                                    \
  V(GetPhysicalDeviceVideoCapabilities)                                        \
  V(CreateVideoSession)                                                        \
  V(DestroyVideoSession)                                                       \
  V(GetVideoSessionMemoryRequirements)                                         \
  V(DestroyVideoSessionParameters)                                             \
  V(UpdateVideoSessionParameters)                                              \
  V(CreateVideoSessionParameters)                                              \
  V(BindVideoSessionMemory)                                                    \
  V(CmdPipelineBarrier2)                                                       \
  V(CmdBeginVideoCoding)                                                       \
  V(CmdControlVideoCoding)                                                     \
  V(CmdEndVideoCoding)                                                         \
  V(CmdDecodeVideo)

struct _GstVulkanVideoFunctions
{
#define DEFINE_FUNCTION(name) G_PASTE(G_PASTE(PFN_vk, name), KHR) name;
    GST_VULKAN_VIDEO_FN_LIST (DEFINE_FUNCTION)
#undef DEFINE_FUNCTION
};

extern const VkExtensionProperties _vk_codec_extensions[2];
extern const VkComponentMapping _vk_identity_component_map;

gboolean                gst_vulkan_video_get_vk_functions       (GstVulkanInstance * instance,
                                                                 GstVulkanVideoFunctions * vk_funcs);

gboolean                gst_vulkan_video_session_create         (GstVulkanVideoSession * session,
                                                                 GstVulkanDevice * device,
                                                                 GstVulkanVideoFunctions * vk,
                                                                 VkVideoSessionCreateInfoKHR * session_create,
                                                                 GError ** error);

void                    gst_vulkan_video_session_destroy        (GstVulkanVideoSession * session);

GstBuffer *             gst_vulkan_video_codec_buffer_new       (GstVulkanDevice * device,
                                                                 const GstVulkanVideoProfile *profile,
                                                                 VkBufferUsageFlags usage,
                                                                 gsize size);

G_END_DECLS
