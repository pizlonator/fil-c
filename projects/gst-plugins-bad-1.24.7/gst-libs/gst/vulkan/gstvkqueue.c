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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkqueue.h"

/**
 * SECTION:vkqueue
 * @title: GstVulkanQueue
 * @short_description: Vulkan command queue
 * @see_also: #GstVulkanDevice
 *
 * GstVulkanQueue encapsulates the vulkan command queue.
 */

#define GST_CAT_DEFAULT gst_vulkan_queue_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

static void
_init_debug (void)
{
  static gsize init;

  if (g_once_init_enter (&init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanqueue", 0, "Vulkan Queue");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&init, 1);
  }
}

struct _GstVulkanQueuePrivate
{
  GMutex submit_lock;
};

#define parent_class gst_vulkan_queue_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanQueue, gst_vulkan_queue, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstVulkanQueue); _init_debug ());

#define GET_PRIV(queue) gst_vulkan_queue_get_instance_private (queue)

static void gst_vulkan_queue_dispose (GObject * object);

static void
gst_vulkan_queue_init (GstVulkanQueue * queue)
{
  GstVulkanQueuePrivate *priv = GET_PRIV (queue);

  g_mutex_init (&priv->submit_lock);
}

static void
gst_vulkan_queue_class_init (GstVulkanQueueClass * queue_class)
{
  GObjectClass *gobject_class = (GObjectClass *) queue_class;

  gobject_class->dispose = gst_vulkan_queue_dispose;
}

static void
gst_vulkan_queue_dispose (GObject * object)
{
  GstVulkanQueue *queue = GST_VULKAN_QUEUE (object);
  GstVulkanQueuePrivate *priv = GET_PRIV (queue);

  if (queue->device)
    gst_object_unref (queue->device);
  queue->device = NULL;

  g_mutex_clear (&priv->submit_lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/**
 * gst_vulkan_queue_get_device
 * @queue: a #GstVulkanQueue
 *
 * Returns: (transfer full) (nullable): the #GstVulkanDevice for @queue
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_queue_get_device (GstVulkanQueue * queue)
{
  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  return gst_object_ref (queue->device);
}

/**
 * gst_vulkan_queue_create_command_pool:
 * @queue: a #GstVulkanQueue
 * @error: (out) (optional): a #GError
 *
 * Returns: (transfer full): a new #GstVulkanCommandPool or %NULL
 *
 * Since: 1.18
 */
GstVulkanCommandPool *
gst_vulkan_queue_create_command_pool (GstVulkanQueue * queue, GError ** error)
{
  GstVulkanCommandPool *pool;
  VkCommandPoolCreateInfo cmd_pool_info = { 0, };
  VkCommandPool vk_pool;
  VkResult err;

  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.pNext = NULL;
  cmd_pool_info.queueFamilyIndex = queue->family;
  cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  GST_OBJECT_LOCK (queue->device);
  err =
      vkCreateCommandPool (queue->device->device, &cmd_pool_info, NULL,
      &vk_pool);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateCommandPool") < 0) {
    GST_OBJECT_LOCK (queue->device);
    goto error;
  }
  GST_OBJECT_UNLOCK (queue->device);

  pool = g_object_new (GST_TYPE_VULKAN_COMMAND_POOL, NULL);
  gst_object_ref_sink (pool);
  pool->queue = gst_object_ref (queue);
  pool->pool = vk_pool;

  return pool;

error:
  return NULL;
}

/**
 * gst_vulkan_queue_create_decoder:
 * @queue: a #GstVulkanQueue
 * @codec: (type guint): the VkVideoCodecOperationFlagBitsKHR to decode
 *
 * Creates a #GstVulkanDecoder object if @codec decoding is supported by @queue
 *
 * Returns: (transfer full) (nullable): the #GstVulkanDecoder object
 *
 * Since: 1.24
 */
GstVulkanDecoder *
gst_vulkan_queue_create_decoder (GstVulkanQueue * queue, guint codec)
{
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  GstVulkanPhysicalDevice *device;
  GstVulkanDecoder *decoder;
  guint flags, expected_flag, supported_video_ops;
  const char *extension;

  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  device = queue->device->physical_device;
  expected_flag = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
  flags = device->queue_family_props[queue->family].queueFlags;
  supported_video_ops = device->queue_family_ops[queue->family].video;

  if (device->properties.apiVersion < VK_MAKE_VERSION (1, 3, 238)) {
    GST_WARNING_OBJECT (queue,
        "Driver API version [%d.%d.%d] doesn't support Video extensions",
        VK_VERSION_MAJOR (device->properties.apiVersion),
        VK_VERSION_MINOR (device->properties.apiVersion),
        VK_VERSION_PATCH (device->properties.apiVersion));
    return NULL;
  }

  switch (codec) {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      extension = VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
      break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      extension = VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
      break;
    default:
      GST_WARNING_OBJECT (queue, "Unsupported codec %u", codec);
      return NULL;
  }
  if ((flags & expected_flag) != expected_flag) {
    GST_WARNING_OBJECT (queue, "Queue doesn't support decoding");
    return NULL;
  }
  if ((supported_video_ops & codec) != codec) {
    GST_WARNING_OBJECT (queue, "Queue doesn't support codec %u decoding",
        codec);
    return NULL;
  }

  if (!(gst_vulkan_device_is_extension_enabled (queue->device,
              VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)
          && gst_vulkan_device_is_extension_enabled (queue->device,
              VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME)
          && gst_vulkan_device_is_extension_enabled (queue->device, extension)))
    return NULL;

  decoder = g_object_new (GST_TYPE_VULKAN_DECODER, NULL);
  gst_object_ref_sink (decoder);
  decoder->queue = gst_object_ref (queue);
  decoder->codec = codec;

  return decoder;
#else
  return NULL;
#endif
}

/**
 * gst_context_set_vulkan_queue:
 * @context: a #GstContext
 * @queue: a #GstVulkanQueue
 *
 * Sets @queue on @context
 *
 * Since: 1.18
 */
void
gst_context_set_vulkan_queue (GstContext * context, GstVulkanQueue * queue)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);
  g_return_if_fail (gst_context_is_writable (context));

  if (queue)
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVulkanQueue(%" GST_PTR_FORMAT ") on context(%"
        GST_PTR_FORMAT ")", queue, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_QUEUE, queue, NULL);
}

/**
 * gst_context_get_vulkan_queue:
 * @context: a #GstContext
 * @queue: resulting #GstVulkanQueue
 *
 * Returns: Whether @queue was in @context
 *
 * Since: 1.18
 */
gboolean
gst_context_get_vulkan_queue (GstContext * context, GstVulkanQueue ** queue)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (queue != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_QUEUE, queue, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVulkanQueue(%" GST_PTR_FORMAT
      ") from context(%" GST_PTR_FORMAT ")", *queue, context);

  return ret;
}

/**
 * gst_vulkan_queue_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type #GST_QUERY_CONTEXT
 * @queue: (nullable): the #GstVulkanQueue
 *
 * If a #GstVulkanQueue is requested in @query, sets @queue as the reply.
 *
 * Intended for use with element query handlers to respond to #GST_QUERY_CONTEXT
 * for a #GstVulkanQueue.
 *
 * Returns: whether @query was responded to with @queue
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_queue_handle_context_query (GstElement * element, GstQuery * query,
    GstVulkanQueue * queue)
{
  gboolean res = FALSE;
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (query != NULL, FALSE);
  g_return_val_if_fail (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT, FALSE);

  if (!queue)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);

  if (g_strcmp0 (context_type, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR) == 0) {
    gst_query_parse_context (query, &old_context);

    if (old_context)
      context = gst_context_copy (old_context);
    else
      context = gst_context_new (GST_VULKAN_QUEUE_CONTEXT_TYPE_STR, TRUE);

    gst_context_set_vulkan_queue (context, queue);
    gst_query_set_context (query, context);
    gst_context_unref (context);

    res = queue != NULL;
  }

  return res;
}

/**
 * gst_vulkan_queue_run_context_query:
 * @element: a #GstElement
 * @queue: (inout): a #GstVulkanQueue
 *
 * Attempt to retrieve a #GstVulkanQueue using #GST_QUERY_CONTEXT from the
 * surrounding elements of @element.
 *
 * Returns: whether @queue contains a valid #GstVulkanQueue
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_queue_run_context_query (GstElement * element,
    GstVulkanQueue ** queue)
{
  GstQuery *query;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (queue != NULL, FALSE);

  _init_debug ();

  if (*queue && GST_IS_VULKAN_QUEUE (*queue))
    return TRUE;

  if ((query =
          gst_vulkan_local_context_query (element,
              GST_VULKAN_QUEUE_CONTEXT_TYPE_STR))) {
    GstContext *context;

    gst_query_parse_context (query, &context);
    if (context)
      gst_context_get_vulkan_queue (context, queue);

    gst_query_unref (query);
  }

  GST_DEBUG_OBJECT (element, "found queue %p", *queue);

  if (*queue)
    return TRUE;

  return FALSE;
}

/**
 * gst_vulkan_queue_submit_lock:
 * @queue: a #GstVulkanQueue
 *
 * Locks the queue for command submission using `vkQueueSubmit()` to meet the
 * Vulkan requirements for externally synchronised resources.
 *
 * Since: 1.18
 */
void
gst_vulkan_queue_submit_lock (GstVulkanQueue * queue)
{
  GstVulkanQueuePrivate *priv = GET_PRIV (queue);

  g_mutex_lock (&priv->submit_lock);
}

/**
 * gst_vulkan_queue_submit_unlock:
 * @queue: a #GstVulkanQueue
 *
 * Unlocks the queue for command submission using `vkQueueSubmit()`.
 *
 * See gst_vulkan_queue_submit_lock() for details on when this call is needed.
 *
 * Since: 1.18
 */
void
gst_vulkan_queue_submit_unlock (GstVulkanQueue * queue)
{
  GstVulkanQueuePrivate *priv = GET_PRIV (queue);

  g_mutex_unlock (&priv->submit_lock);
}
