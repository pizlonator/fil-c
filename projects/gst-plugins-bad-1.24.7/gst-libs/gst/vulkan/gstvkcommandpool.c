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

#include "gstvkcommandpool.h"
#include "gstvkcommandpool-private.h"

/**
 * SECTION:vkcommandpool
 * @title: GstVulkanCommandPool
 * @short_description: Vulkan command pool
 * @see_also: #GstVulkanCommandBuffer, #GstVulkanDevice
 */

#define GST_VULKAN_COMMAND_POOL_LARGE_OUTSTANDING 1024

#define GET_PRIV(pool) gst_vulkan_command_pool_get_instance_private (pool)

#define GST_CAT_DEFAULT gst_vulkan_command_pool_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

struct _GstVulkanCommandPoolPrivate
{
  GQueue *available;

  GRecMutex rec_mutex;

  gsize outstanding;
};

#define parent_class gst_vulkan_command_pool_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanCommandPool, gst_vulkan_command_pool,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstVulkanCommandPool);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkancommandpool", 0, "Vulkan Command Pool"));

static void gst_vulkan_command_pool_finalize (GObject * object);

static void
gst_vulkan_command_pool_init (GstVulkanCommandPool * pool)
{
  GstVulkanCommandPoolPrivate *priv = GET_PRIV (pool);

  g_rec_mutex_init (&priv->rec_mutex);
  priv->available = g_queue_new ();
}

static void
gst_vulkan_command_pool_class_init (GstVulkanCommandPoolClass * device_class)
{
  GObjectClass *gobject_class = (GObjectClass *) device_class;

  gobject_class->finalize = gst_vulkan_command_pool_finalize;
}

static void
do_free_buffer (GstVulkanCommandBuffer * cmd_buf)
{
  gst_vulkan_command_buffer_unref (cmd_buf);
}

static void
gst_vulkan_command_pool_finalize (GObject * object)
{
  GstVulkanCommandPool *pool = GST_VULKAN_COMMAND_POOL (object);
  GstVulkanCommandPoolPrivate *priv = GET_PRIV (pool);

  gst_vulkan_command_pool_lock (pool);
  g_queue_free_full (priv->available, (GDestroyNotify) do_free_buffer);
  priv->available = NULL;
  gst_vulkan_command_pool_unlock (pool);

  if (priv->outstanding > 0)
    g_critical
        ("Destroying a Vulkan command pool that has outstanding buffers!");

  if (pool->pool)
    vkDestroyCommandPool (pool->queue->device->device, pool->pool, NULL);
  pool->pool = VK_NULL_HANDLE;

  gst_clear_object (&pool->queue);

  g_rec_mutex_clear (&priv->rec_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gst_vulkan_command_pool_get_queue
 * @pool: a #GstVulkanCommandPool
 *
 * Returns: (transfer full): the parent #GstVulkanQueue for this command pool
 *
 * Since: 1.18
 */
GstVulkanQueue *
gst_vulkan_command_pool_get_queue (GstVulkanCommandPool * pool)
{
  g_return_val_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool), NULL);

  return gst_object_ref (pool->queue);
}

static GstVulkanCommandBuffer *
command_alloc (GstVulkanCommandPool * pool, GError ** error)
{
  VkResult err;
  VkCommandBufferAllocateInfo cmd_info = { 0, };
  GstVulkanCommandBuffer *buf;
  VkCommandBuffer cmd;

  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_info.pNext = NULL;
  cmd_info.commandPool = pool->pool;
  cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_info.commandBufferCount = 1;

  gst_vulkan_command_pool_lock (pool);
  err = vkAllocateCommandBuffers (pool->queue->device->device, &cmd_info, &cmd);
  gst_vulkan_command_pool_unlock (pool);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateCommandBuffer") < 0)
    return NULL;

  buf =
      gst_vulkan_command_buffer_new_wrapped (cmd,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  GST_LOG_OBJECT (pool, "created cmd buffer %p", buf);

  return buf;
}

static gboolean
gst_vulkan_command_pool_can_reset (GstVulkanCommandPool * pool)
{
  g_return_val_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool), FALSE);

  /* whether VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT was in flags on
   * pool creation */
  return TRUE;
}

/**
 * gst_vulkan_command_pool_create:
 * @pool: a #GstVulkanCommandPool
 * @error: a #GError
 *
 * Returns: a new or recycled primary #GstVulkanCommandBuffer
 *
 * Since: 1.18
 */
GstVulkanCommandBuffer *
gst_vulkan_command_pool_create (GstVulkanCommandPool * pool, GError ** error)
{
  GstVulkanCommandBuffer *cmd = NULL;
  GstVulkanCommandPoolPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool), NULL);

  priv = GET_PRIV (pool);

  if (gst_vulkan_command_pool_can_reset (pool)) {
    gst_vulkan_command_pool_lock (pool);
    cmd = g_queue_pop_head (priv->available);
    gst_vulkan_command_pool_unlock (pool);
  }
  if (!cmd)
    cmd = command_alloc (pool, error);
  if (!cmd)
    return NULL;

  cmd->pool = gst_object_ref (pool);

  gst_vulkan_command_pool_lock (pool);
  priv->outstanding++;
  if (priv->outstanding > GST_VULKAN_COMMAND_POOL_LARGE_OUTSTANDING)
    g_critical ("%s: There are a large number of command buffers outstanding! "
        "This usually means there is a reference counting issue somewhere.",
        GST_OBJECT_NAME (pool));
  gst_vulkan_command_pool_unlock (pool);
  return cmd;
}

void
gst_vulkan_command_pool_release_buffer (GstVulkanCommandPool * pool,
    GstVulkanCommandBuffer * buffer)
{
  GstVulkanCommandPoolPrivate *priv;
  gboolean can_reset;

  g_return_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool));
  g_return_if_fail (buffer != NULL);
  g_return_if_fail (buffer->pool == pool);

  priv = GET_PRIV (pool);
  can_reset = gst_vulkan_command_pool_can_reset (pool);

  gst_vulkan_command_pool_lock (pool);
  if (can_reset) {
    vkResetCommandBuffer (buffer->cmd, 0);
    g_queue_push_tail (priv->available, buffer);
    GST_TRACE_OBJECT (pool, "reset command buffer %p", buffer);
  }
  /* TODO: if this is a secondary command buffer, all primary command buffers
   * that reference this command buffer will be invalid */
  priv->outstanding--;
  gst_vulkan_command_pool_unlock (pool);

  /* decrease the refcount that the buffer had to us */
  gst_clear_object (&buffer->pool);

  if (!can_reset)
    gst_vulkan_command_buffer_unref (buffer);
}

/**
 * gst_vulkan_command_pool_lock:
 * @pool: a #GstVulkanCommandPool
 *
 * This should be called to ensure no other thread will attempt to access
 * the pool's internal resources.  Any modification of any of the allocated
 * #GstVulkanCommandBuffer's need to be encapsulated in a
 * gst_vulkan_command_pool_lock()/gst_vulkan_command_pool_unlock() pair to meet
 * the Vulkan API requirements that host access to the command pool is
 * externally synchronised.
 *
 * Since: 1.18
 */
void
gst_vulkan_command_pool_lock (GstVulkanCommandPool * pool)
{
  GstVulkanCommandPoolPrivate *priv;
  g_return_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool));
  priv = GET_PRIV (pool);
  g_rec_mutex_lock (&priv->rec_mutex);
}

/**
 * gst_vulkan_command_pool_unlock:
 * @pool: a #GstVulkanCommandPool
 *
 * See the documentation for gst_vulkan_command_pool_lock() for when you would
 * need to use this function.
 *
 * Since: 1.18
 */
void
gst_vulkan_command_pool_unlock (GstVulkanCommandPool * pool)
{
  GstVulkanCommandPoolPrivate *priv;
  g_return_if_fail (GST_IS_VULKAN_COMMAND_POOL (pool));
  priv = GET_PRIV (pool);
  g_rec_mutex_unlock (&priv->rec_mutex);
}
