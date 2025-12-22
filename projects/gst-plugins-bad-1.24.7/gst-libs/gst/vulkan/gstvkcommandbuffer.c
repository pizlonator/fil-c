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
 * SECTION:vulkancommandbuffer
 * @title: GstVulkanCommandBuffer
 * @short_description: Vulkan command buffer
 * @see_also: #GstVulkanCommandPool
 *
 * vulkancommandbuffer holds information about a command buffer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkcommandbuffer.h"
#include "gstvkcommandpool.h"
#include "gstvkcommandpool-private.h"

#define GST_CAT_DEFAULT gst_debug_vulkan_command_buffer
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

static void
init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkancommandbuffer", 0,
        "Vulkan command buffer");
    g_once_init_leave (&_init, 1);
  }
}

static gboolean
gst_vulkan_command_buffer_dispose (GstVulkanCommandBuffer * cmd)
{
  GstVulkanCommandPool *pool;

  /* no pool, do free */
  if ((pool = cmd->pool) == NULL)
    return TRUE;

  /* keep the buffer alive */
  gst_vulkan_command_buffer_ref (cmd);
  /* return the buffer to the pool */
  gst_vulkan_command_pool_release_buffer (pool, cmd);

  return FALSE;
}

static void
gst_vulkan_command_buffer_free (GstVulkanCommandBuffer * cmd)
{
  g_assert (cmd->pool == NULL);

  GST_TRACE ("Freeing %p", cmd);

  g_free (cmd);
}

static void
gst_vulkan_command_buffer_init (GstVulkanCommandBuffer * cmd,
    VkCommandBuffer cmd_buf, VkCommandBufferLevel level)
{
  cmd->cmd = cmd_buf;
  cmd->level = level;

  init_debug ();

  GST_TRACE ("new %p", cmd);

  gst_mini_object_init (&cmd->parent, 0, GST_TYPE_VULKAN_COMMAND_BUFFER,
      NULL, (GstMiniObjectDisposeFunction) gst_vulkan_command_buffer_dispose,
      (GstMiniObjectFreeFunction) gst_vulkan_command_buffer_free);
}

/**
 * gst_vulkan_command_buffer_new_wrapped:
 * @cmd: a VkCommandBuffer
 * @level: the VkCommandBufferLevel for @cmd
 *
 * Returns: (transfer full): a new #GstVulkanCommandBuffer
 *
 * Since: 1.18
 */
GstVulkanCommandBuffer *
gst_vulkan_command_buffer_new_wrapped (VkCommandBuffer cmd,
    VkCommandBufferLevel level)
{
  GstVulkanCommandBuffer *ret;

  ret = g_new0 (GstVulkanCommandBuffer, 1);
  gst_vulkan_command_buffer_init (ret, cmd, level);

  return ret;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstVulkanCommandBuffer, gst_vulkan_command_buffer);
