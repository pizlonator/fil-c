/* GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 *
 * vkdeviceprovider.c: vulkan device probing and monitoring
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvulkanelements.h"
#include "vkdeviceprovider.h"

#include <string.h>

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (vulkan_device_debug);
#define GST_CAT_DEFAULT vulkan_device_debug


static GstDevice *gst_vulkan_device_object_new (GstVulkanPhysicalDevice *
    device, GstCaps * caps, GstVulkanDeviceType type, GstStructure * properties,
    gboolean is_default);

G_DEFINE_TYPE_WITH_CODE (GstVulkanDeviceProvider, gst_vulkan_device_provider,
    GST_TYPE_DEVICE_PROVIDER, GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "vulkandevice", 0, "Vulkan Device");
    );
GST_DEVICE_PROVIDER_REGISTER_DEFINE (vulkandeviceprovider,
    "vulkandeviceprovider", GST_RANK_MARGINAL, GST_TYPE_VULKAN_DEVICE_PROVIDER);

static void gst_vulkan_device_provider_finalize (GObject * object);
static void gst_vulkan_device_provider_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_vulkan_device_provider_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GList *gst_vulkan_device_provider_probe (GstDeviceProvider * provider);

enum
{
  PROP_PROVIDER_0,
  PROP_PROVIDER_LAST,
};

static void
gst_vulkan_device_provider_class_init (GstVulkanDeviceProviderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  gobject_class->set_property = gst_vulkan_device_provider_set_property;
  gobject_class->get_property = gst_vulkan_device_provider_get_property;
  gobject_class->finalize = gst_vulkan_device_provider_finalize;

  dm_class->probe = gst_vulkan_device_provider_probe;

  gst_device_provider_class_set_static_metadata (dm_class,
      "Vulkan Device Provider", "Sink/Video",
      "List and provider Vulkan sink devices",
      "Matthew Waters <matthew@centricular.com>");
}

static void
gst_vulkan_device_provider_init (GstVulkanDeviceProvider * self)
{
}

static void
gst_vulkan_device_provider_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_vulkan_device_provider_parent_class)->finalize (object);
}

static void
gst_vulkan_device_provider_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_device_provider_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
fill_properties (GstVulkanPhysicalDevice * device, GstStructure * s)
{
  gst_structure_set (s, "vulkan.name", G_TYPE_STRING,
      device->properties.deviceName, NULL);
  gst_structure_set (s, "vulkan.type", G_TYPE_STRING,
      gst_vulkan_physical_device_type_to_string (device->properties.deviceType),
      NULL);

  {
    int maj = VK_VERSION_MAJOR (device->properties.apiVersion);
    int min = VK_VERSION_MINOR (device->properties.apiVersion);
    int patch = VK_VERSION_PATCH (device->properties.apiVersion);
    gchar *api_str = g_strdup_printf ("%i.%i.%i", maj, min, patch);
    gst_structure_set (s, "vulkan.api.version", G_TYPE_STRING, api_str,
        "vulkan.api.version.major", G_TYPE_UINT, maj,
        "vulkan.api.version.minor", G_TYPE_UINT, min,
        "vulkan.api.version.patch", G_TYPE_UINT, patch, NULL);
    g_free (api_str);
  }

  {
    int maj = VK_VERSION_MAJOR (device->properties.driverVersion);
    int min = VK_VERSION_MINOR (device->properties.driverVersion);
    int patch = VK_VERSION_PATCH (device->properties.driverVersion);
    gchar *api_str = g_strdup_printf ("%i.%i.%i", maj, min, patch);
    gst_structure_set (s, "vulkan.driver.version", G_TYPE_STRING, api_str,
        "vulkan.driver.version.major", G_TYPE_UINT, maj,
        "vulkan.driver.version.minor", G_TYPE_UINT, min,
        "vulkan.driver.version.patch", G_TYPE_UINT, patch, NULL);
    g_free (api_str);
  }

  gst_structure_set (s, "vulkan.vendor.id", G_TYPE_UINT,
      device->properties.vendorID, NULL);
  gst_structure_set (s, "vulkan.device.id", G_TYPE_UINT,
      device->properties.deviceID, NULL);

  /* memory properties */
  {
    int i;

    gst_structure_set (s, "vulkan.memory.n_heaps", G_TYPE_UINT,
        (guint) device->memory_properties.memoryHeapCount, NULL);
    for (i = 0; i < device->memory_properties.memoryHeapCount; i++) {
      gchar *prop_flags_str =
          gst_vulkan_memory_heap_flags_to_string (device->
          memory_properties.memoryHeaps[i].flags);
      gchar *prop_id;

      prop_id = g_strdup_printf ("vulkan.memory.heaps.%i.size", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT64,
          (guint64) device->memory_properties.memoryHeaps[i].size, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.memory.heaps.%i.flags", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->memory_properties.memoryHeaps[i].flags, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.memory.heaps.%i.flags.str", i);
      gst_structure_set (s, prop_id, G_TYPE_STRING, prop_flags_str, NULL);
      g_free (prop_id);

      g_free (prop_flags_str);
    }

    gst_structure_set (s, "vulkan.memory.n_types", G_TYPE_UINT,
        (guint) device->memory_properties.memoryTypeCount, NULL);
    for (i = 0; i < device->memory_properties.memoryTypeCount; i++) {
      gchar *prop_flags_str =
          gst_vulkan_memory_property_flags_to_string (device->memory_properties.
          memoryTypes[i].propertyFlags);
      gchar *prop_id;

      prop_id = g_strdup_printf ("vulkan.memory.types.%i.heap", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->memory_properties.memoryTypes[i].heapIndex, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.memory.types.%i.flags", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->memory_properties.memoryTypes[i].propertyFlags, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.memory.types.%i.flags.str", i);
      gst_structure_set (s, prop_id, G_TYPE_STRING, prop_flags_str, NULL);
      g_free (prop_id);

      g_free (prop_flags_str);
    }

    gst_structure_set (s, "vulkan.n_queue_families", G_TYPE_UINT,
        device->n_queue_families, NULL);
    for (i = 0; i < device->n_queue_families; i++) {
      gchar *queue_flags_str =
          gst_vulkan_queue_flags_to_string (device->
          queue_family_props[i].queueFlags);
      gchar *prop_id;

      prop_id = g_strdup_printf ("vulkan.queue_family.%i.n_queues", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].queueCount, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.queue_family.%i.flags", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].queueFlags, NULL);
      g_free (prop_id);

      prop_id = g_strdup_printf ("vulkan.queue_family.%i.flags.str", i);
      gst_structure_set (s, prop_id, G_TYPE_STRING, queue_flags_str, NULL);
      g_free (prop_id);

      prop_id =
          g_strdup_printf ("vulkan.queue_family.%i.timestamp_resolution", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].timestampValidBits, NULL);
      g_free (prop_id);

      prop_id =
          g_strdup_printf
          ("vulkan.queue_family.%i.min_image_transfer_granuality.width", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].
          minImageTransferGranularity.width, NULL);
      g_free (prop_id);

      prop_id =
          g_strdup_printf
          ("vulkan.queue_family.%i.min_image_transfer_granuality.height", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].
          minImageTransferGranularity.height, NULL);
      g_free (prop_id);

      prop_id =
          g_strdup_printf
          ("vulkan.queue_family.%i.min_image_transfer_granuality.depth", i);
      gst_structure_set (s, prop_id, G_TYPE_UINT,
          (guint) device->queue_family_props[i].
          minImageTransferGranularity.depth, NULL);
      g_free (prop_id);

      g_free (queue_flags_str);
    }
  }
}

static GList *
gst_vulkan_device_provider_probe (GstDeviceProvider * provider)
{
  GstVulkanInstance *instance;
  GError *error = NULL;
  GList *ret = NULL;
  guint i;

  instance = gst_vulkan_instance_new ();
  if (!gst_vulkan_instance_open (instance, &error))
    goto failed;

  for (i = 0; i < instance->n_physical_devices; i++) {
    GstVulkanPhysicalDevice *device;
    gboolean is_default = i == 0;
    GstStructure *props;
    GstCaps *caps;

    device = gst_vulkan_physical_device_new (instance, i);

    props = gst_structure_new_empty ("properties");
    fill_properties (device, props);
    caps = gst_caps_from_string ("video/x-raw(memory:VulkanImage)");
    ret = g_list_prepend (ret, gst_vulkan_device_object_new (device, caps,
            GST_VULKAN_DEVICE_TYPE_SINK, props, is_default));
    gst_caps_unref (caps);
    gst_structure_free (props);
  }

  /* TODO: device groups? */

  gst_object_unref (instance);
  return ret;

failed:
  if (error) {
    GST_WARNING_OBJECT (provider, "%s", error->message);
    g_clear_error (&error);
  }
  if (instance)
    gst_object_unref (instance);

  return NULL;
}

enum
{
  PROP_0,
  PROP_PHYSICAL_DEVICE,
};

G_DEFINE_TYPE (GstVulkanDeviceObject, gst_vulkan_device_object,
    GST_TYPE_DEVICE);

static void gst_vulkan_device_object_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_vulkan_device_object_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_vulkan_device_object_finalize (GObject * object);
static GstElement *gst_vulkan_device_object_create_element (GstDevice * device,
    const gchar * name);
static gboolean gst_vulkan_device_object_reconfigure_element (GstDevice *
    device, GstElement * element);

static void
gst_vulkan_device_object_class_init (GstVulkanDeviceObjectClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_vulkan_device_object_create_element;
  dev_class->reconfigure_element = gst_vulkan_device_object_reconfigure_element;

  object_class->get_property = gst_vulkan_device_object_get_property;
  object_class->set_property = gst_vulkan_device_object_set_property;
  object_class->finalize = gst_vulkan_device_object_finalize;

  g_object_class_install_property (object_class, PROP_PHYSICAL_DEVICE,
      g_param_spec_object ("physical-device", "Physical Device",
          "Associated Vulkan Physical Device", GST_TYPE_VULKAN_PHYSICAL_DEVICE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_vulkan_device_object_init (GstVulkanDeviceObject * device)
{
}

static void
gst_vulkan_device_object_finalize (GObject * object)
{
  GstVulkanDeviceObject *device = GST_VULKAN_DEVICE_OBJECT (object);

  gst_clear_object (&device->physical_device);

  G_OBJECT_CLASS (gst_vulkan_device_object_parent_class)->finalize (object);
}

static gpointer
_ref_if_set (gpointer data, gpointer user_data)
{
  if (data) {
    return g_weak_ref_get (data);
  }
  return NULL;
}

static void
_ref_free (GWeakRef * ref)
{
  g_weak_ref_clear (ref);
  g_free (ref);
}

static GstPadProbeReturn
device_context_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstVulkanPhysicalDevice *physical = GST_VULKAN_PHYSICAL_DEVICE (user_data);
  GstElement *element = GST_ELEMENT (gst_object_get_parent (GST_OBJECT (pad)));
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_QUERY_BOTH) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

    if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT) {
      const char *context_type = NULL;

      gst_query_parse_context_type (query, &context_type);

      if (gst_vulkan_instance_handle_context_query (element, query,
              physical->instance)) {
        ret = GST_PAD_PROBE_HANDLED;
        goto out;
      }

      if (g_strcmp0 (context_type, GST_VULKAN_DEVICE_CONTEXT_TYPE_STR) == 0) {
        GstVulkanDevice *device = NULL;

        GST_OBJECT_LOCK (physical);
        device = g_object_dup_data (G_OBJECT (physical),
            "vkdeviceprovider.physical.device", (GDuplicateFunc) _ref_if_set,
            NULL);
        GST_OBJECT_UNLOCK (physical);
        if (!device || !GST_IS_VULKAN_DEVICE (device)) {
          GWeakRef *ref = g_new0 (GWeakRef, 1);
          if (device)
            gst_object_unref (device);
          device = gst_vulkan_device_new (physical);
          g_weak_ref_init (ref, device);

          GST_OBJECT_LOCK (physical);
          g_object_set_data_full (G_OBJECT (physical),
              "vkdeviceprovider.physical.device", ref,
              (GDestroyNotify) _ref_free);
          GST_OBJECT_UNLOCK (physical);
        }

        if (gst_vulkan_device_handle_context_query (element, query, device)) {
          ret = GST_PAD_PROBE_HANDLED;
          gst_object_unref (device);
          goto out;
        }
        gst_object_unref (device);
      }
    }
  }

out:
  gst_object_unref (element);
  return ret;
}

static GstElement *
gst_vulkan_device_object_create_element (GstDevice * device, const gchar * name)
{
  GstVulkanDeviceObject *vulkan_device = GST_VULKAN_DEVICE_OBJECT (device);
  GstElement *elem;
  GstPad *pad;

  elem = gst_element_factory_make (vulkan_device->element, name);
  pad = gst_element_get_static_pad (elem, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_QUERY_BOTH,
      (GstPadProbeCallback) device_context_probe,
      gst_object_ref (vulkan_device->physical_device),
      (GDestroyNotify) gst_object_unref);
  gst_object_unref (pad);

  return elem;
}

static gboolean
gst_vulkan_device_object_reconfigure_element (GstDevice * device,
    GstElement * element)
{
  return FALSE;
}

/* Takes ownership of @caps and @props */
static GstDevice *
gst_vulkan_device_object_new (GstVulkanPhysicalDevice * device, GstCaps * caps,
    GstVulkanDeviceType type, GstStructure * props, gboolean is_default)
{
  GstVulkanDeviceObject *gstdev;
  const gchar *element = NULL;
  const gchar *klass = NULL;
  gchar *device_name = NULL;

  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), NULL);
  g_return_val_if_fail (caps, NULL);
  g_return_val_if_fail (props, NULL);

  switch (type) {
    case GST_VULKAN_DEVICE_TYPE_SINK:
      element = "vulkansink";
      klass = "Video/Sink";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  g_object_get (device, "name", &device_name, NULL);

  gst_structure_set (props, "is-default", G_TYPE_BOOLEAN, is_default, NULL);
  gstdev =
      g_object_new (GST_TYPE_VULKAN_DEVICE_OBJECT, "display-name", device_name,
      "caps", caps, "device-class", klass, "properties", props, NULL);

  gstdev->physical_device = device;
  gstdev->type = type;
  g_object_get (device, "device-index", &gstdev->device_index, NULL);
  gstdev->element = element;
  gstdev->is_default = is_default;

  g_free (device_name);

  return GST_DEVICE (gstdev);
}

static void
gst_vulkan_device_object_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanDeviceObject *vulkan_device = GST_VULKAN_DEVICE_OBJECT (object);

  switch (prop_id) {
    case PROP_PHYSICAL_DEVICE:
      g_value_set_object (value, vulkan_device->physical_device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_device_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
