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

#include "gstvkdevice.h"
#include "gstvkdebug.h"
#include "gstvkphysicaldevice-private.h"

#include <string.h>

/**
 * SECTION:vkdevice
 * @title: GstVulkanDevice
 * @short_description: Vulkan device
 * @see_also: #GstVulkanPhysicalDevice, #GstVulkanInstance
 *
 * A #GstVulkanDevice encapsulates a VkDevice
 */

#define GST_CAT_DEFAULT gst_vulkan_device_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

#define GET_PRIV(o) (gst_vulkan_device_get_instance_private (o))

enum
{
  PROP_0,
  PROP_INSTANCE,
  PROP_PHYSICAL_DEVICE,
};

static void gst_vulkan_device_dispose (GObject * object);
static void gst_vulkan_device_finalize (GObject * object);

struct _GstVulkanDevicePrivate
{
  GPtrArray *enabled_layers;
  GPtrArray *enabled_extensions;

  gboolean opened;
  GArray *queues;
  GArray *queue_family_indices;

  GstVulkanFenceCache *fence_cache;
};

static void
_init_debug (void)
{
  static gsize init;

  if (g_once_init_enter (&init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkandevice", 0,
        "Vulkan Device");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&init, 1);
  }
}

#define gst_vulkan_device_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanDevice, gst_vulkan_device, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstVulkanDevice);
    _init_debug ());

/**
 * gst_vulkan_device_new:
 * @physical_device: the associated #GstVulkanPhysicalDevice
 *
 * Returns: (transfer full): a new #GstVulkanDevice
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_device_new (GstVulkanPhysicalDevice * physical_device)
{
  GstVulkanDevice *device;

  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (physical_device), NULL);

  device = g_object_new (GST_TYPE_VULKAN_DEVICE, "physical-device",
      physical_device, NULL);
  gst_object_ref_sink (device);

  return device;
}

/**
 * gst_vulkan_device_new_with_index:
 * @instance: the associated #GstVulkanInstance
 * @device_index: the device index to create the new #GstVulkanDevice from
 *
 * Returns: (transfer full): a new #GstVulkanDevice
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_device_new_with_index (GstVulkanInstance * instance,
    guint device_index)
{
  GstVulkanPhysicalDevice *physical;
  GstVulkanDevice *device;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), NULL);

  physical = gst_vulkan_physical_device_new (instance, device_index);
  device = gst_vulkan_device_new (physical);
  gst_object_unref (physical);
  return device;
}

static void
gst_vulkan_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanDevice *device = GST_VULKAN_DEVICE (object);

  switch (prop_id) {
    case PROP_PHYSICAL_DEVICE:
      device->physical_device = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanDevice *device = GST_VULKAN_DEVICE (object);

  switch (prop_id) {
    case PROP_INSTANCE:
      g_value_set_object (value, device->instance);
      break;
    case PROP_PHYSICAL_DEVICE:
      g_value_set_object (value, device->physical_device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_device_init (GstVulkanDevice * device)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  priv->enabled_layers = g_ptr_array_new_with_free_func (g_free);
  priv->enabled_extensions = g_ptr_array_new_with_free_func (g_free);
}

static void
gst_vulkan_device_constructed (GObject * object)
{
  GstVulkanDevice *device = GST_VULKAN_DEVICE (object);
  const char *optional_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
#if defined(VK_KHR_timeline_semaphore)
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
#endif
#if defined(VK_KHR_synchronization2)
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
#endif
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
#ifdef VK_ENABLE_BETA_EXTENSIONS
    VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,
    VK_EXT_VIDEO_ENCODE_H264_EXTENSION_NAME,
    VK_EXT_VIDEO_ENCODE_H265_EXTENSION_NAME,
#endif
#endif
  };
  int i;

  g_object_get (device->physical_device, "instance", &device->instance, NULL);

  /* Ignore the failure if the extension does not exist. */
  for (i = 0; i < G_N_ELEMENTS (optional_extensions); i++) {
    if (!gst_vulkan_device_enable_extension (device, optional_extensions[i])) {
      GST_INFO_OBJECT (device, "Could not enable extension %s",
          optional_extensions[i]);
    }
  }

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_vulkan_device_class_init (GstVulkanDeviceClass * device_class)
{
  GObjectClass *gobject_class = (GObjectClass *) device_class;

  gobject_class->set_property = gst_vulkan_device_set_property;
  gobject_class->get_property = gst_vulkan_device_get_property;
  gobject_class->finalize = gst_vulkan_device_finalize;
  gobject_class->dispose = gst_vulkan_device_dispose;
  gobject_class->constructed = gst_vulkan_device_constructed;

  g_object_class_install_property (gobject_class, PROP_INSTANCE,
      g_param_spec_object ("instance", "Instance",
          "Associated Vulkan Instance",
          GST_TYPE_VULKAN_INSTANCE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PHYSICAL_DEVICE,
      g_param_spec_object ("physical-device", "Physical Device",
          "Associated Vulkan Physical Device",
          GST_TYPE_VULKAN_PHYSICAL_DEVICE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gst_vulkan_device_dispose (GObject * object)
{
  GstVulkanDevice *device = GST_VULKAN_DEVICE (object);
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  if (priv->queue_family_indices) {
    g_array_unref (priv->queue_family_indices);
    priv->queue_family_indices = NULL;
  }

  if (priv->queues) {
    g_array_unref (priv->queues);
    priv->queues = NULL;
  }

  if (priv->fence_cache) {
    /* clear any outstanding fences */
    g_object_run_dispose (G_OBJECT (priv->fence_cache));

    /* don't double free this device */
    priv->fence_cache->parent.device = NULL;
  }
  gst_clear_object (&priv->fence_cache);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_vulkan_device_finalize (GObject * object)
{
  GstVulkanDevice *device = GST_VULKAN_DEVICE (object);
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  if (device->device) {
    vkDeviceWaitIdle (device->device);
    vkDestroyDevice (device->device, NULL);
  }
  device->device = VK_NULL_HANDLE;

  gst_clear_object (&device->physical_device);
  gst_clear_object (&device->instance);

  g_ptr_array_unref (priv->enabled_layers);
  priv->enabled_layers = NULL;

  g_ptr_array_unref (priv->enabled_extensions);
  priv->enabled_extensions = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
/* TODO: add this function for general use and consider compiler builtins */
static inline guint32
_pop_count (guint32 n)
{
  n = n - ((n >> 1) & 0x55555555);
  n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
  return (((n + (n >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

/* look for the queue with more capabilities for the requested flag and also
 * used by other flags, thus we could use the same queue for more ops. Though,
 * perhaps it's not the best strategy for parallelism. */
static inline int
_pick_queue_family (VkQueueFamilyProperties * queue_family_props,
    guint32 num_queue_families, VkQueueFlagBits flags, guint32 * family_scores)
{
  int i, index = -1;
  guint32 score, max_score = 0;

  for (i = 0; i < num_queue_families; i++) {
    const VkQueueFlagBits queue_flags = queue_family_props[i].queueFlags;
    if (queue_flags & flags) {
      score = _pop_count (queue_flags) + family_scores[i];
      if (score > max_score) {
        index = i;
        max_score = score;
      }
    }
  }

  if (index > -1)
    family_scores[index]++;

  return index;
}

static GArray *
_append_queue_create_info (GArray * array, int family_index,
    VkQueueFamilyProperties * queue_family_props)
{
  int i;
  VkDeviceQueueCreateInfo queue_info;
  gint queue_count;
  gfloat *priorities;

  if (family_index == -1)
    return array;

  for (i = 0; i < array->len; i++) {
    VkDeviceQueueCreateInfo *qi =
        &g_array_index (array, VkDeviceQueueCreateInfo, i);
    if (qi->queueFamilyIndex == family_index)
      return array;
  }

  /* shall we open all -- queue_family_props[family_index].queueCount ? */
  queue_count = 1;

  priorities = g_new (gfloat, queue_count);
  for (i = 0; i < queue_count; i++)
    priorities[i] = 1.0 / queue_count;

  /* *INDENT-OFF* */
  queue_info = (VkDeviceQueueCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = family_index,
      .queueCount = queue_count,
      .pQueuePriorities = priorities,
  };
  /* *INDENT-ON* */

  return g_array_append_val (array, queue_info);
}


/* Returns an array of VkDeviceQueueCreateInfo with the list of queues to
 * create. The list will contain one or more queues which will support all the
 * required families */
static GArray *
gst_vulkan_device_choose_queues (GstVulkanDevice * device)
{
  VkQueueFamilyProperties *queue_family_props;
  GArray *array;
  guint32 *family_scores, n_queue_families;
  int graph_index, comp_index, tx_index;
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  int dec_index = -1;
#ifdef VK_ENABLE_BETA_EXTENSIONS
  int enc_index = -1;
#endif
#endif

  n_queue_families = device->physical_device->n_queue_families;
  queue_family_props = device->physical_device->queue_family_props;

  array = g_array_sized_new (FALSE, FALSE, sizeof (VkDeviceQueueCreateInfo),
      n_queue_families);

  family_scores = g_new0 (guint32, n_queue_families);

  graph_index = _pick_queue_family (queue_family_props, n_queue_families,
      VK_QUEUE_GRAPHICS_BIT, family_scores);
  array = _append_queue_create_info (array, graph_index, queue_family_props);
  comp_index = _pick_queue_family (queue_family_props, n_queue_families,
      VK_QUEUE_COMPUTE_BIT, family_scores);
  array = _append_queue_create_info (array, comp_index, queue_family_props);
  tx_index = _pick_queue_family (queue_family_props, n_queue_families,
      VK_QUEUE_TRANSFER_BIT, family_scores);
  array = _append_queue_create_info (array, tx_index, queue_family_props);
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  dec_index = _pick_queue_family (queue_family_props, n_queue_families,
      VK_QUEUE_VIDEO_DECODE_BIT_KHR, family_scores);
  array = _append_queue_create_info (array, dec_index, queue_family_props);
#ifdef VK_ENABLE_BETA_EXTENSIONS
  enc_index = _pick_queue_family (queue_family_props, n_queue_families,
      VK_QUEUE_VIDEO_ENCODE_BIT_KHR, family_scores);
  array = _append_queue_create_info (array, enc_index, queue_family_props);
#endif
#endif

  g_free (family_scores);

  return array;
}

/**
 * gst_vulkan_device_open:
 * @device: a #GstVulkanDevice
 * @error: a #GError
 *
 * Attempts to create the internal `VkDevice` object.
 *
 * Returns: whether a vulkan device could be created
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_open (GstVulkanDevice * device, GError ** error)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);
  VkResult err;
  guint i;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);

  GST_OBJECT_LOCK (device);

  if (priv->opened) {
    GST_OBJECT_UNLOCK (device);
    return TRUE;
  }

  priv->queues = gst_vulkan_device_choose_queues (device);
  if (priv->queues->len == 0) {
    g_array_unref (priv->queues);
    priv->queues = NULL;
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Failed to find a compatible queue family");
    goto error;
  }

  GST_INFO_OBJECT (device, "Creating a device from physical %" GST_PTR_FORMAT
      " with %u layers and %u extensions", device->physical_device,
      priv->enabled_layers->len, priv->enabled_extensions->len);

  for (i = 0; i < priv->enabled_layers->len; i++)
    GST_DEBUG_OBJECT (device, "layer %u: %s", i,
        (gchar *) g_ptr_array_index (priv->enabled_layers, i));
  for (i = 0; i < priv->enabled_extensions->len; i++)
    GST_DEBUG_OBJECT (device, "extension %u: %s", i,
        (gchar *) g_ptr_array_index (priv->enabled_extensions, i));

  {
    VkPhysicalDevice gpu;
    VkDeviceCreateInfo device_info = { 0, };

    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext =
        gst_vulkan_physical_device_get_features (device->physical_device);
    device_info.queueCreateInfoCount = priv->queues->len;
    device_info.pQueueCreateInfos = (VkDeviceQueueCreateInfo *)
        priv->queues->data;
    device_info.enabledLayerCount = priv->enabled_layers->len;
    device_info.ppEnabledLayerNames =
        (const char *const *) priv->enabled_layers->pdata;
    device_info.enabledExtensionCount = priv->enabled_extensions->len;
    device_info.ppEnabledExtensionNames =
        (const char *const *) priv->enabled_extensions->pdata;
    device_info.pEnabledFeatures = NULL;

    gpu = gst_vulkan_device_get_physical_device (device);
    err = vkCreateDevice (gpu, &device_info, NULL, &device->device);
    if (gst_vulkan_error_to_g_error (err, error, "vkCreateDevice") < 0) {
      goto error;
    }
  }

  priv->fence_cache = gst_vulkan_fence_cache_new (device);
  /* avoid reference loops between us and the fence cache */
  gst_object_unref (device);

  for (i = 0; i < priv->queues->len; i++) {
    VkDeviceQueueCreateInfo *qi =
        &g_array_index (priv->queues, VkDeviceQueueCreateInfo, i);
    g_free ((gpointer) qi->pQueuePriorities);
  }

  priv->opened = TRUE;
  GST_OBJECT_UNLOCK (device);
  return TRUE;

error:
  {
    GST_OBJECT_UNLOCK (device);
    return FALSE;
  }
}

/**
 * gst_vulkan_device_get_queue:
 * @device: a #GstVulkanDevice
 * @queue_family: a queue family to retrieve
 * @queue_i: index of the family to retrieve
 *
 * Returns: (transfer full): a new #GstVulkanQueue
 *
 * Since: 1.18
 */
GstVulkanQueue *
gst_vulkan_device_get_queue (GstVulkanDevice * device, guint32 queue_family,
    guint32 queue_i)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);
  GstVulkanQueue *ret;
  int i;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);
  g_return_val_if_fail (device->device != NULL, NULL);
  g_return_val_if_fail (priv->opened, NULL);

  for (i = 0; i < priv->queues->len; i++) {
    VkDeviceQueueCreateInfo *qi =
        &g_array_index (priv->queues, VkDeviceQueueCreateInfo, i);
    if (qi->queueFamilyIndex == queue_family && qi->queueCount >= queue_i)
      break;
  }

  g_return_val_if_fail (i < priv->queues->len, NULL);

  ret = g_object_new (GST_TYPE_VULKAN_QUEUE, NULL);
  gst_object_ref_sink (ret);
  ret->device = gst_object_ref (device);
  ret->family = queue_family;
  ret->index = queue_i;

  vkGetDeviceQueue (device->device, queue_family, queue_i, &ret->queue);

  return ret;
}

/**
 * gst_vulkan_device_foreach_queue:
 * @device: a #GstVulkanDevice
 * @func: (scope call): a #GstVulkanDeviceForEachQueueFunc to run for each #GstVulkanQueue
 * @user_data: (closure func): user data to pass to each call of @func
 *
 * Iterate over each queue family available on #GstVulkanDevice
 *
 * Since: 1.18
 */
void
gst_vulkan_device_foreach_queue (GstVulkanDevice * device,
    GstVulkanDeviceForEachQueueFunc func, gpointer user_data)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);
  gboolean done = FALSE;
  guint i, j;

  g_return_if_fail (GST_IS_VULKAN_DEVICE (device));
  g_return_if_fail (priv->opened);

  for (i = 0; i < priv->queues->len; i++) {
    VkDeviceQueueCreateInfo *qi =
        &g_array_index (priv->queues, VkDeviceQueueCreateInfo, i);

    for (j = 0; j < qi->queueCount; j++) {
      GstVulkanQueue *queue =
          gst_vulkan_device_get_queue (device, qi->queueFamilyIndex, j);

      if (!func (device, queue, user_data))
        done = TRUE;

      gst_object_unref (queue);

      if (done)
        return;
    }
  }
}

/**
 * gst_vulkan_device_queue_family_indices:
 * @device: a #GstVulkanDevice
 *
 * Returns: (element-type uint32_t) (transfer full): An array with the family
 *     indexes of the created queues in @device
 *
 * Since: 1.24
 */
GArray *
gst_vulkan_device_queue_family_indices (GstVulkanDevice * device)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);
  guint i, j;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);
  g_return_val_if_fail (priv->opened, NULL);

  GST_OBJECT_LOCK (device);

  if (priv->queue_family_indices)
    goto beach;

  priv->queue_family_indices =
      g_array_sized_new (FALSE, FALSE, sizeof (uint32_t), priv->queues->len);

  for (i = 0; i < priv->queues->len; i++) {
    VkDeviceQueueCreateInfo *qi =
        &g_array_index (priv->queues, VkDeviceQueueCreateInfo, i);

    for (j = 0; j < priv->queue_family_indices->len; j++) {
      uint32_t qfi = g_array_index (priv->queue_family_indices, uint32_t, j);
      if (qfi == qi->queueFamilyIndex)
        break;
    }

    if (j == priv->queue_family_indices->len)
      g_array_append_val (priv->queue_family_indices, qi->queueFamilyIndex);
  }

beach:
  GST_OBJECT_UNLOCK (device);
  return g_array_ref (priv->queue_family_indices);
}

/**
 * gst_vulkan_device_get_proc_address:
 * @device: a #GstVulkanDevice
 * @name: name of the function to retrieve
 *
 * Performs `vkGetDeviceProcAddr()` with @device and @name
 *
 * Returns: (nullable): the function pointer for @name or %NULL
 *
 * Since: 1.18
 */
gpointer
gst_vulkan_device_get_proc_address (GstVulkanDevice * device,
    const gchar * name)
{
  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);
  g_return_val_if_fail (device->device != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  GST_TRACE_OBJECT (device, "%s", name);

  return vkGetDeviceProcAddr (device->device, name);
}

/**
 * gst_vulkan_device_get_instance:
 * @device: a #GstVulkanDevice
 *
 * Returns: (transfer full) (nullable): the #GstVulkanInstance used to create this @device
 *
 * Since: 1.18
 */
GstVulkanInstance *
gst_vulkan_device_get_instance (GstVulkanDevice * device)
{
  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);

  return gst_object_ref (device->instance);
}

/**
 * gst_vulkan_device_get_physical_device: (skip)
 * @device: a #GstVulkanDevice
 *
 * Returns: The VkPhysicalDevice used to create @device
 *
 * Since: 1.18
 */
VkPhysicalDevice
gst_vulkan_device_get_physical_device (GstVulkanDevice * device)
{
  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);

  return gst_vulkan_physical_device_get_handle (device->physical_device);
}

/**
 * gst_context_set_vulkan_device:
 * @context: a #GstContext
 * @device: a #GstVulkanDevice
 *
 * Sets @device on @context
 *
 * Since: 1.18
 */
void
gst_context_set_vulkan_device (GstContext * context, GstVulkanDevice * device)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);
  g_return_if_fail (gst_context_is_writable (context));

  if (device)
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVulkanDevice(%" GST_PTR_FORMAT ") on context(%"
        GST_PTR_FORMAT ")", device, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_VULKAN_DEVICE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_DEVICE, device, NULL);
}

/**
 * gst_context_get_vulkan_device:
 * @context: a #GstContext
 * @device: resulting #GstVulkanDevice
 *
 * Returns: Whether @device was in @context
 *
 * Since: 1.18
 */
gboolean
gst_context_get_vulkan_device (GstContext * context, GstVulkanDevice ** device)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (device != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_VULKAN_DEVICE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_DEVICE, device, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVulkanDevice(%" GST_PTR_FORMAT
      ") from context(%" GST_PTR_FORMAT ")", *device, context);

  return ret;
}

/**
 * gst_vulkan_device_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type #GST_QUERY_CONTEXT
 * @device: the #GstVulkanDevice
 *
 * If a #GstVulkanDevice is requested in @query, sets @device as the reply.
 *
 * Intended for use with element query handlers to respond to #GST_QUERY_CONTEXT
 * for a #GstVulkanDevice.
 *
 * Returns: whether @query was responded to with @device
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_handle_context_query (GstElement * element, GstQuery * query,
    GstVulkanDevice * device)
{
  gboolean res = FALSE;
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (query != NULL, FALSE);
  g_return_val_if_fail (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT, FALSE);

  if (!device)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);

  if (g_strcmp0 (context_type, GST_VULKAN_DEVICE_CONTEXT_TYPE_STR) == 0) {
    gst_query_parse_context (query, &old_context);

    if (old_context)
      context = gst_context_copy (old_context);
    else
      context = gst_context_new (GST_VULKAN_DEVICE_CONTEXT_TYPE_STR, TRUE);

    gst_context_set_vulkan_device (context, device);
    gst_query_set_context (query, context);
    gst_context_unref (context);

    res = device != NULL;
  }

  return res;
}

/**
 * gst_vulkan_device_run_context_query:
 * @element: a #GstElement
 * @device: (inout): a #GstVulkanDevice
 *
 * Attempt to retrieve a #GstVulkanDevice using #GST_QUERY_CONTEXT from the
 * surrounding elements of @element.
 *
 * Returns: whether @device contains a valid #GstVulkanDevice
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_run_context_query (GstElement * element,
    GstVulkanDevice ** device)
{
  GstQuery *query;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  _init_debug ();

  if (*device && GST_IS_VULKAN_DEVICE (*device))
    return TRUE;

  if ((query =
          gst_vulkan_local_context_query (element,
              GST_VULKAN_DEVICE_CONTEXT_TYPE_STR))) {
    GstContext *context;

    gst_query_parse_context (query, &context);
    if (context)
      gst_context_get_vulkan_device (context, device);

    gst_query_unref (query);
  }

  GST_DEBUG_OBJECT (element, "found device %p", *device);

  if (*device)
    return TRUE;

  return FALSE;
}

/**
 * gst_vulkan_device_create_fence:
 * @device: a #GstVulkanDevice
 * @error: a #GError to fill on failure
 *
 * Returns: (transfer full) (nullable): a new #GstVulkanFence or %NULL
 *
 * Since: 1.18
 */
GstVulkanFence *
gst_vulkan_device_create_fence (GstVulkanDevice * device, GError ** error)
{
  GstVulkanDevicePrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), NULL);
  priv = GET_PRIV (device);

  return gst_vulkan_fence_cache_acquire (priv->fence_cache, error);
}

static gboolean
gst_vulkan_device_is_extension_enabled_unlocked (GstVulkanDevice * device,
    const gchar * name, guint * index)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  return g_ptr_array_find_with_equal_func (priv->enabled_extensions, name,
      g_str_equal, index);
}

/**
 * gst_vulkan_device_is_extension_enabled:
 * @device: a # GstVulkanDevice
 * @name: extension name
 *
 * Returns: whether extension @name is enabled
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_is_extension_enabled (GstVulkanDevice * device,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret = gst_vulkan_device_is_extension_enabled_unlocked (device, name, NULL);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

static gboolean
gst_vulkan_device_enable_extension_unlocked (GstVulkanDevice * device,
    const gchar * name)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  if (gst_vulkan_device_is_extension_enabled_unlocked (device, name, NULL))
    /* extension is already enabled */
    return TRUE;

  if (!gst_vulkan_physical_device_get_extension_info (device->physical_device,
          name, NULL))
    return FALSE;

  g_ptr_array_add (priv->enabled_extensions, g_strdup (name));

  return TRUE;
}

/**
 * gst_vulkan_device_enable_extension:
 * @device: a #GstVulkanDevice
 * @name: extension name to enable
 *
 * Enable an Vulkan extension by @name.  Enabling an extension will
 * only have an effect before the call to gst_vulkan_device_open().
 *
 * Returns: whether the Vulkan extension could be enabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_enable_extension (GstVulkanDevice * device,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret = gst_vulkan_device_enable_extension_unlocked (device, name);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

static gboolean
gst_vulkan_device_disable_extension_unlocked (GstVulkanDevice * device,
    const gchar * name)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);
  guint i;

  if (!gst_vulkan_physical_device_get_extension_info (device->physical_device,
          name, NULL))
    return FALSE;

  if (!gst_vulkan_device_is_extension_enabled_unlocked (device, name, &i))
    /* extension is already disabled */
    return TRUE;

  g_ptr_array_remove_index_fast (priv->enabled_extensions, i);

  return TRUE;
}

/**
 * gst_vulkan_device_disable_extension:
 * @device: a #GstVulkanDevice
 * @name: extension name to enable
 *
 * Disable an Vulkan extension by @name.  Disabling an extension will only have
 * an effect before the call to gst_vulkan_device_open().
 *
 * Returns: whether the Vulkan extension could be disabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_disable_extension (GstVulkanDevice * device,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret = gst_vulkan_device_disable_extension_unlocked (device, name);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

static gboolean
gst_vulkan_device_is_layer_enabled_unlocked (GstVulkanDevice * device,
    const gchar * name)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  return g_ptr_array_find_with_equal_func (priv->enabled_layers, name,
      g_str_equal, NULL);
}

/**
 * gst_vulkan_device_is_layer_enabled:
 * @device: a # GstVulkanDevice
 * @name: layer name
 *
 * Returns: whether layer @name is enabled
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_is_layer_enabled (GstVulkanDevice * device,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret = gst_vulkan_device_is_layer_enabled_unlocked (device, name);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

static gboolean
gst_vulkan_device_enable_layer_unlocked (GstVulkanDevice * device,
    const gchar * name)
{
  GstVulkanDevicePrivate *priv = GET_PRIV (device);

  if (gst_vulkan_device_is_layer_enabled_unlocked (device, name))
    /* layer is already enabled */
    return TRUE;

  if (!gst_vulkan_physical_device_get_layer_info (device->physical_device,
          name, NULL, NULL, NULL))
    return FALSE;

  g_ptr_array_add (priv->enabled_layers, g_strdup (name));

  return TRUE;
}

/**
 * gst_vulkan_device_enable_layer:
 * @device: a #GstVulkanDevice
 * @name: layer name to enable
 *
 * Enable an Vulkan layer by @name.  Enabling a layer will
 * only have an effect before the call to gst_vulkan_device_open().
 *
 * Returns: whether the Vulkan layer could be enabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_device_enable_layer (GstVulkanDevice * device, const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret = gst_vulkan_device_enable_layer_unlocked (device, name);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

struct choose_queue
{
  guint expected_flags;
  GstVulkanQueue *queue;
};

static gboolean
_choose_queue (GstVulkanDevice * device, GstVulkanQueue * queue,
    struct choose_queue *data)
{
  guint flags =
      device->physical_device->queue_family_props[queue->family].queueFlags;

  if ((flags & data->expected_flags) != 0) {
    if (data->queue)
      gst_object_unref (data->queue);
    data->queue = gst_object_ref (queue);
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_vulkan_device_select_queue
 * @device: a #GstVulkanDevice
 * @expected_flags:  a VkQueueFlagBits
 *
 * Select a compatible queue from the @device supporting the @expected_flags.
 *
 * Returns: (transfer full): a #GstVulkanQueue for @queue matching the
 *                           @expected_flags
 *
 * Since: 1.24
 */
GstVulkanQueue *
gst_vulkan_device_select_queue (GstVulkanDevice * device,
    VkQueueFlagBits expected_flags)
{
  struct choose_queue data;

  data.expected_flags = expected_flags;
  data.queue = NULL;

  if (!gst_vulkan_device_open (device, NULL)) {
    gst_object_unref (device);
    goto beach;
  }
  gst_vulkan_device_foreach_queue (device,
      (GstVulkanDeviceForEachQueueFunc) _choose_queue, &data);

beach:
  return data.queue;
}
