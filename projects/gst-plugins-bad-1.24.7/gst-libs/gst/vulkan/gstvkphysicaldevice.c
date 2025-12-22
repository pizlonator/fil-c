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

#include "gstvkphysicaldevice.h"

#include "gstvkphysicaldevice-private.h"
#include "gstvkdebug.h"

#include <string.h>

/**
 * SECTION:vkphysicaldevice
 * @title: GstVulkanPhysicalDevice
 * @short_description: Vulkan physical device
 * @see_also: #GstVulkanInstance, #GstVulkanDevice
 *
 * A #GstVulkanPhysicalDevice encapsulates a VkPhysicalDevice
 */

#define GST_CAT_DEFAULT gst_vulkan_physical_device_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_INSTANCE,
  PROP_DEVICE_ID,
  PROP_NAME,
};

static void gst_vulkan_physical_device_finalize (GObject * object);

struct _GstVulkanPhysicalDevicePrivate
{
  guint32 n_available_layers;
  VkLayerProperties *available_layers;

  guint32 n_available_extensions;
  VkExtensionProperties *available_extensions;

#if defined (VK_API_VERSION_1_2)
  VkPhysicalDeviceFeatures2 features10;
  VkPhysicalDeviceProperties2 properties10;
  VkPhysicalDeviceVulkan11Features features11;
  VkPhysicalDeviceVulkan11Properties properties11;
  VkPhysicalDeviceVulkan12Features features12;
  VkPhysicalDeviceVulkan12Properties properties12;
#endif
#if defined (VK_API_VERSION_1_3)
  VkPhysicalDeviceVulkan13Features features13;
  VkPhysicalDeviceVulkan13Properties properties13;
#endif
};

static void
_init_debug (void)
{
  static gsize init;

  if (g_once_init_enter (&init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkandevice", 0,
        "Vulkan Device");
    g_once_init_leave (&init, 1);
  }
}

#define GET_PRIV(device) gst_vulkan_physical_device_get_instance_private (device)

#define gst_vulkan_physical_device_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanPhysicalDevice, gst_vulkan_physical_device,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstVulkanPhysicalDevice);
    _init_debug ());

static gboolean gst_vulkan_physical_device_fill_info (GstVulkanPhysicalDevice *
    device, GError ** error);

/**
 * gst_vulkan_physical_device_new:
 * @instance: the parent #GstVulkanInstance
 *
 * Returns: (transfer full): a new #GstVulkanPhysicalDevice
 *
 * Since: 1.18
 */
GstVulkanPhysicalDevice *
gst_vulkan_physical_device_new (GstVulkanInstance * instance,
    guint device_index)
{
  GstVulkanPhysicalDevice *device;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), NULL);

  device = g_object_new (GST_TYPE_VULKAN_PHYSICAL_DEVICE, "instance", instance,
      "device-index", device_index, NULL);
  gst_object_ref_sink (device);

  return device;
}

static void
gst_vulkan_physical_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanPhysicalDevice *device = GST_VULKAN_PHYSICAL_DEVICE (object);

  switch (prop_id) {
    case PROP_INSTANCE:
      device->instance = g_value_dup_object (value);
      break;
    case PROP_DEVICE_ID:{
      guint device_id = g_value_get_uint (value);
      if (device->instance == VK_NULL_HANDLE
          || device_id >= device->instance->n_physical_devices) {
        g_critical ("%s: Cannot set device-index larger than the "
            "number of physical devices", GST_OBJECT_NAME (device));
      } else {
        device->device_index = device_id;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_physical_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanPhysicalDevice *device = GST_VULKAN_PHYSICAL_DEVICE (object);

  switch (prop_id) {
    case PROP_INSTANCE:
      g_value_set_object (value, device->instance);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, device->device_index);
      break;
    case PROP_NAME:
      g_value_set_string (value, device->properties.deviceName);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_physical_device_init (GstVulkanPhysicalDevice * device)
{
  GstVulkanPhysicalDevicePrivate *priv = GET_PRIV (device);

  priv->n_available_layers = 0;
  priv->n_available_extensions = 0;

#if defined (VK_API_VERSION_1_2)
  priv->properties10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  priv->properties11.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
  priv->properties12.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
  priv->properties10.pNext = &priv->properties11;
  priv->properties11.pNext = &priv->properties12;

  priv->features10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  priv->features11.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  priv->features12.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  priv->features10.pNext = &priv->features11;
  priv->features11.pNext = &priv->features12;
#endif
#if defined (VK_API_VERSION_1_3)
  priv->properties13.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
  priv->properties12.pNext = &priv->properties13;

  priv->features13.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  priv->features12.pNext = &priv->features13;
#endif
}

static void
gst_vulkan_physical_device_constructed (GObject * object)
{
  GstVulkanPhysicalDevice *device = GST_VULKAN_PHYSICAL_DEVICE (object);
  GError *error = NULL;

  if (device->instance == VK_NULL_HANDLE) {
    GST_ERROR_OBJECT (object, "Constructed without any instance set");
    return;
  }

  device->device = device->instance->physical_devices[device->device_index];

  if (!gst_vulkan_physical_device_fill_info (device, &error)) {
    GST_ERROR_OBJECT (object, "%s", error->message);
    g_clear_error (&error);
  }
}

static void
gst_vulkan_physical_device_class_init (GstVulkanPhysicalDeviceClass *
    device_class)
{
  GObjectClass *gobject_class = (GObjectClass *) device_class;

  gobject_class->set_property = gst_vulkan_physical_device_set_property;
  gobject_class->get_property = gst_vulkan_physical_device_get_property;
  gobject_class->finalize = gst_vulkan_physical_device_finalize;
  gobject_class->constructed = gst_vulkan_physical_device_constructed;

  g_object_class_install_property (gobject_class, PROP_INSTANCE,
      g_param_spec_object ("instance", "Instance",
          "Associated Vulkan Instance",
          GST_TYPE_VULKAN_INSTANCE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-index", "Device Index", "Device Index", 0,
          G_MAXUINT32, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Device Name", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_vulkan_physical_device_finalize (GObject * object)
{
  GstVulkanPhysicalDevice *device = GST_VULKAN_PHYSICAL_DEVICE (object);
  GstVulkanPhysicalDevicePrivate *priv = GET_PRIV (device);

  g_free (priv->available_layers);
  priv->available_layers = NULL;

  g_free (priv->available_extensions);
  priv->available_extensions = NULL;

  g_free (device->queue_family_ops);
  g_free (device->queue_family_props);
  device->queue_family_props = NULL;

  if (device->instance)
    gst_object_unref (device->instance);
  device->instance = VK_NULL_HANDLE;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define DEBUG_BOOL(prefix, name, value)                                     \
  GST_DEBUG_OBJECT (device, prefix " " G_STRINGIFY(name) ": %s",            \
    value ? "YES" : "NO")
#define DEBUG_1(prefix, s, name, format, type)                              \
  GST_DEBUG_OBJECT (device, prefix " " G_STRINGIFY(name) ": %" format,      \
    (type) (s)->name)
#define DEBUG_2(prefix, s, name, format, type)                              \
  GST_DEBUG_OBJECT (device, prefix " " G_STRINGIFY(name)                    \
    ": %" format ", %" format,                                              \
    (type) (s)->name[0],                                                    \
    (type) (s)->name[1])
#define DEBUG_3(prefix, s, name, format, type)                              \
  GST_DEBUG_OBJECT (device, prefix " " G_STRINGIFY(limit)                   \
    ": %" format ", %" format ", %" format,                                 \
    (type) (s)->name[0],                                                    \
    (type) (s)->name[1],                                                    \
    (type) (s)->name[2])

#define DEBUG_UINT32(prefix, s, var) DEBUG_1(prefix, s, var, G_GUINT32_FORMAT, guint32)
#define DEBUG_UINT32_2(prefix, s, var) DEBUG_2(prefix, s, var, G_GUINT32_FORMAT, guint32)
#define DEBUG_UINT32_3(prefix, s, var) DEBUG_3(prefix, s, var, G_GUINT32_FORMAT, guint32)

#define DEBUG_UINT64(prefix, s, var) DEBUG_1(prefix, s, var, G_GUINT64_FORMAT, guint64)

#define DEBUG_INT32(prefix, s, var) DEBUG_1(prefix, s, var, G_GINT32_FORMAT, gint32)

#define DEBUG_FLOAT(prefix, s, var) DEBUG_1(prefix, s, var, "f", gfloat)
#define DEBUG_FLOAT_2(prefix, s, var) DEBUG_2(prefix, s, var, "f", gfloat)

#define DEBUG_SIZE(prefix, s, var) DEBUG_1(prefix, s, var, G_GSIZE_FORMAT, gsize)
#define DEBUG_FLAGS(prefix, s, limit, under_name_type)                      \
  G_STMT_START {                                                            \
    gchar *str = G_PASTE(G_PASTE(gst_vulkan_,under_name_type),_flags_to_string) ((s)->limit); \
    GST_DEBUG_OBJECT (device, prefix " " G_STRINGIFY(limit) ": (0x%x) %s",  \
        (s)->limit, str);                                                   \
    g_free (str);                                                           \
  } G_STMT_END

#define DEBUG_BOOL_STRUCT(prefix, s, name) DEBUG_BOOL(prefix, name, (s)->name)

#define DEBUG_STRING(prefix, s, str) DEBUG_1(prefix, s, str, "s", gchar *);

static void
dump_features10 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceFeatures * features)
{
  /* *INDENT-OFF* */
  DEBUG_BOOL_STRUCT ("support for", features, robustBufferAccess);
  DEBUG_BOOL_STRUCT ("support for", features, fullDrawIndexUint32);
  DEBUG_BOOL_STRUCT ("support for", features, imageCubeArray);
  DEBUG_BOOL_STRUCT ("support for", features, independentBlend);
  DEBUG_BOOL_STRUCT ("support for", features, geometryShader);
  DEBUG_BOOL_STRUCT ("support for", features, tessellationShader);
  DEBUG_BOOL_STRUCT ("support for", features, sampleRateShading);
  DEBUG_BOOL_STRUCT ("support for", features, sampleRateShading);
  DEBUG_BOOL_STRUCT ("support for", features, dualSrcBlend);
  DEBUG_BOOL_STRUCT ("support for", features, logicOp);
  DEBUG_BOOL_STRUCT ("support for", features, multiDrawIndirect);
  DEBUG_BOOL_STRUCT ("support for", features, drawIndirectFirstInstance);
  DEBUG_BOOL_STRUCT ("support for", features, depthClamp);
  DEBUG_BOOL_STRUCT ("support for", features, depthBiasClamp);
  DEBUG_BOOL_STRUCT ("support for", features, fillModeNonSolid);
  DEBUG_BOOL_STRUCT ("support for", features, depthBounds);
  DEBUG_BOOL_STRUCT ("support for", features, wideLines);
  DEBUG_BOOL_STRUCT ("support for", features, largePoints);
  DEBUG_BOOL_STRUCT ("support for", features, alphaToOne);
  DEBUG_BOOL_STRUCT ("support for", features, multiViewport);
  DEBUG_BOOL_STRUCT ("support for", features, samplerAnisotropy);
  DEBUG_BOOL_STRUCT ("support for", features, textureCompressionETC2);
  DEBUG_BOOL_STRUCT ("support for", features, textureCompressionASTC_LDR);
  DEBUG_BOOL_STRUCT ("support for", features, textureCompressionBC);
  DEBUG_BOOL_STRUCT ("support for", features, occlusionQueryPrecise);
  DEBUG_BOOL_STRUCT ("support for", features, pipelineStatisticsQuery);
  DEBUG_BOOL_STRUCT ("support for", features, vertexPipelineStoresAndAtomics);
  DEBUG_BOOL_STRUCT ("support for", features, fragmentStoresAndAtomics);
  DEBUG_BOOL_STRUCT ("support for", features, shaderTessellationAndGeometryPointSize);
  DEBUG_BOOL_STRUCT ("support for", features, shaderImageGatherExtended);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageImageExtendedFormats);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageImageMultisample);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageImageReadWithoutFormat);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageImageWriteWithoutFormat);
  DEBUG_BOOL_STRUCT ("support for", features, shaderUniformBufferArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for", features, shaderSampledImageArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageBufferArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for", features, shaderStorageImageArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for", features, shaderClipDistance);
  DEBUG_BOOL_STRUCT ("support for", features, shaderCullDistance);
  DEBUG_BOOL_STRUCT ("support for", features, shaderFloat64);
  DEBUG_BOOL_STRUCT ("support for", features, shaderInt64);
  DEBUG_BOOL_STRUCT ("support for", features, shaderInt16);
  DEBUG_BOOL_STRUCT ("support for", features, shaderResourceResidency);
  DEBUG_BOOL_STRUCT ("support for", features, shaderResourceMinLod);
  DEBUG_BOOL_STRUCT ("support for", features, sparseBinding);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidencyBuffer);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidencyImage2D);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidencyImage3D);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidency2Samples);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidency4Samples);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidency8Samples);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidency16Samples);
  DEBUG_BOOL_STRUCT ("support for", features, sparseResidencyAliased);
  DEBUG_BOOL_STRUCT ("support for", features, variableMultisampleRate);
  DEBUG_BOOL_STRUCT ("support for", features, inheritedQueries);
  /* *INDENT-ON* */
}

#if defined (VK_API_VERSION_1_2)
static void
dump_features11 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan11Features * features)
{
  /* *INDENT-OFF* */
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, storageBuffer16BitAccess);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, uniformAndStorageBuffer16BitAccess);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, storagePushConstant16);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, storageInputOutput16);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, multiview);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, multiviewGeometryShader);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, multiviewTessellationShader);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, variablePointersStorageBuffer);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, variablePointers);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, protectedMemory);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, samplerYcbcrConversion);
  DEBUG_BOOL_STRUCT ("support for (1.1)", features, shaderDrawParameters);
  /* *INDENT-ON* */
}

static void
dump_features12 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan12Features * features)
{
  /* *INDENT-OFF* */
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, samplerMirrorClampToEdge);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, drawIndirectCount);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, storageBuffer8BitAccess);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, uniformAndStorageBuffer8BitAccess);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderBufferInt64Atomics);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderSharedInt64Atomics);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderFloat16);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderInt8);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderInputAttachmentArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderUniformTexelBufferArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderStorageTexelBufferArrayDynamicIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderUniformBufferArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderSampledImageArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderStorageBufferArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderStorageImageArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderInputAttachmentArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderUniformTexelBufferArrayNonUniformIndexing);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingUniformBufferUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingSampledImageUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingStorageImageUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingStorageBufferUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingUniformTexelBufferUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingStorageTexelBufferUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingUpdateUnusedWhilePending);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingPartiallyBound);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, descriptorBindingVariableDescriptorCount);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, runtimeDescriptorArray);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, samplerFilterMinmax);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, scalarBlockLayout);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, imagelessFramebuffer);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, uniformBufferStandardLayout);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderSubgroupExtendedTypes);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, separateDepthStencilLayouts);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, hostQueryReset);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, timelineSemaphore);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, bufferDeviceAddress);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, bufferDeviceAddressCaptureReplay);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, bufferDeviceAddressMultiDevice);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, vulkanMemoryModel);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, vulkanMemoryModelDeviceScope);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, vulkanMemoryModelAvailabilityVisibilityChains);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderOutputViewportIndex);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, shaderOutputLayer);
  DEBUG_BOOL_STRUCT ("support for (1.2)", features, subgroupBroadcastDynamicId);
  /* *INDENT-ON* */
}
#endif

#if defined (VK_API_VERSION_1_3)
static void
dump_features13 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan13Features * features)
{
  /* *INDENT-OFF* */
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, robustImageAccess);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, inlineUniformBlock);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, descriptorBindingInlineUniformBlockUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, pipelineCreationCacheControl);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, privateData);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, shaderDemoteToHelperInvocation);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, shaderTerminateInvocation);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, subgroupSizeControl);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, computeFullSubgroups);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, synchronization2);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, textureCompressionASTC_HDR);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, shaderZeroInitializeWorkgroupMemory);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, dynamicRendering);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, shaderIntegerDotProduct);
  DEBUG_BOOL_STRUCT ("support for (1.3)", features, maintenance4);
  /* *INDENT-ON* */
}
#endif

static gboolean
dump_features (GstVulkanPhysicalDevice * device, GError ** error)
{
#if defined (VK_API_VERSION_1_2)
  GstVulkanPhysicalDevicePrivate *priv = GET_PRIV (device);
  VkBaseOutStructure *iter;

  if (gst_vulkan_instance_check_version (device->instance, 1, 2, 0)) {
    for (iter = (VkBaseOutStructure *) & priv->features10; iter;
        iter = iter->pNext) {
      if (iter->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
        dump_features10 (device,
            &((VkPhysicalDeviceFeatures2 *) iter)->features);
      else if (iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
        dump_features11 (device, (VkPhysicalDeviceVulkan11Features *) iter);
      else if (iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
        dump_features12 (device, (VkPhysicalDeviceVulkan12Features *) iter);
#if defined (VK_API_VERSION_1_3)
      else if (gst_vulkan_instance_check_version (device->instance, 1, 3, 0)
          && iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
        dump_features13 (device, (VkPhysicalDeviceVulkan13Features *) iter);
#endif
    }
  } else
#endif
  {
    dump_features10 (device, &device->features);
  }

  return TRUE;
}

static gboolean
dump_memory_properties (GstVulkanPhysicalDevice * device, GError ** error)
{
  int i;

  GST_DEBUG_OBJECT (device, "found %" G_GUINT32_FORMAT " memory heaps",
      device->memory_properties.memoryHeapCount);
  for (i = 0; i < device->memory_properties.memoryHeapCount; i++) {
    gchar *prop_flags_str =
        gst_vulkan_memory_heap_flags_to_string (device->
        memory_properties.memoryHeaps[i].flags);
    GST_LOG_OBJECT (device,
        "memory heap at index %i has size %" G_GUINT64_FORMAT
        " and flags (0x%x) \'%s\'", i,
        (guint64) device->memory_properties.memoryHeaps[i].size,
        device->memory_properties.memoryHeaps[i].flags, prop_flags_str);
    g_free (prop_flags_str);
  }
  GST_DEBUG_OBJECT (device, "found %" G_GUINT32_FORMAT " memory types",
      device->memory_properties.memoryTypeCount);
  for (i = 0; i < device->memory_properties.memoryTypeCount; i++) {
    gchar *prop_flags_str =
        gst_vulkan_memory_property_flags_to_string (device->memory_properties.
        memoryTypes[i].propertyFlags);
    GST_LOG_OBJECT (device,
        "memory type at index %i is allocatable from "
        "heap %i with flags (0x%x) \'%s\'", i,
        device->memory_properties.memoryTypes[i].heapIndex,
        device->memory_properties.memoryTypes[i].propertyFlags, prop_flags_str);
    g_free (prop_flags_str);
  }

  return TRUE;
}

static gboolean
dump_queue_properties (GstVulkanPhysicalDevice * device, GError ** error)
{
  int i;

  GST_DEBUG_OBJECT (device, "found %" G_GUINT32_FORMAT " queue families",
      device->n_queue_families);
  for (i = 0; i < device->n_queue_families; i++) {
    gchar *queue_flags_str =
        gst_vulkan_queue_flags_to_string (device->
        queue_family_props[i].queueFlags);
    GST_LOG_OBJECT (device,
        "queue family at index %i supports %i queues "
        "with flags (0x%x) \'%s\', video operations (0x%x), %" G_GUINT32_FORMAT
        " timestamp bits and a minimum image transfer granuality of %"
        GST_VULKAN_EXTENT3D_FORMAT, i, device->queue_family_props[i].queueCount,
        device->queue_family_props[i].queueFlags, queue_flags_str,
        device->queue_family_ops ? device->queue_family_ops[i].video : 0,
        device->queue_family_props[i].timestampValidBits,
        GST_VULKAN_EXTENT3D_ARGS (device->
            queue_family_props[i].minImageTransferGranularity));
    g_free (queue_flags_str);
  }

  return TRUE;
}

static gboolean
dump_limits (GstVulkanPhysicalDevice * device, GError ** error)
{
  VkPhysicalDeviceLimits *limits = &device->properties.limits;

  /* *INDENT-OFF* */
  DEBUG_UINT32 ("limit", limits, maxImageDimension1D);
  DEBUG_UINT32 ("limit", limits, maxImageDimension2D);
  DEBUG_UINT32 ("limit", limits, maxImageDimension3D);
  DEBUG_UINT32 ("limit", limits, maxImageDimensionCube);
  DEBUG_UINT32 ("limit", limits, maxImageArrayLayers);
  DEBUG_UINT32 ("limit", limits, maxTexelBufferElements);
  DEBUG_UINT32 ("limit", limits, maxUniformBufferRange);
  DEBUG_UINT32 ("limit", limits, maxStorageBufferRange);
  DEBUG_UINT32 ("limit", limits, maxPushConstantsSize);
  DEBUG_UINT32 ("limit", limits, maxMemoryAllocationCount);
  DEBUG_UINT32 ("limit", limits, maxSamplerAllocationCount);
  DEBUG_UINT64 ("limit", limits, bufferImageGranularity);
  DEBUG_UINT64 ("limit", limits, sparseAddressSpaceSize);
  DEBUG_UINT32 ("limit", limits, maxBoundDescriptorSets);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorSamplers);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorUniformBuffers);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorStorageBuffers);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorSampledImages);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorStorageImages);
  DEBUG_UINT32 ("limit", limits, maxPerStageDescriptorInputAttachments);
  DEBUG_UINT32 ("limit", limits, maxPerStageResources);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetSamplers);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetUniformBuffers);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetUniformBuffersDynamic);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetStorageBuffers);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetStorageBuffersDynamic);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetSampledImages);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetStorageImages);
  DEBUG_UINT32 ("limit", limits, maxDescriptorSetInputAttachments);
  DEBUG_UINT32 ("limit", limits, maxVertexInputAttributes);
  DEBUG_UINT32 ("limit", limits, maxVertexInputBindings);
  DEBUG_UINT32 ("limit", limits, maxVertexInputBindings);
  DEBUG_UINT32 ("limit", limits, maxVertexInputAttributeOffset);
  DEBUG_UINT32 ("limit", limits, maxVertexInputBindingStride);
  DEBUG_UINT32 ("limit", limits, maxVertexOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationGenerationLevel);
  DEBUG_UINT32 ("limit", limits, maxTessellationPatchSize);
  DEBUG_UINT32 ("limit", limits, maxTessellationControlPerVertexInputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationControlPerVertexOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationControlPerPatchOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationControlTotalOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationControlTotalOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationEvaluationInputComponents);
  DEBUG_UINT32 ("limit", limits, maxTessellationEvaluationOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxGeometryShaderInvocations);
  DEBUG_UINT32 ("limit", limits, maxGeometryInputComponents);
  DEBUG_UINT32 ("limit", limits, maxGeometryOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxGeometryOutputVertices);
  DEBUG_UINT32 ("limit", limits, maxGeometryTotalOutputComponents);
  DEBUG_UINT32 ("limit", limits, maxFragmentInputComponents);
  DEBUG_UINT32 ("limit", limits, maxFragmentOutputAttachments);
  DEBUG_UINT32 ("limit", limits, maxFragmentDualSrcAttachments);
  DEBUG_UINT32 ("limit", limits, maxFragmentCombinedOutputResources);
  DEBUG_UINT32 ("limit", limits, maxComputeSharedMemorySize);
  DEBUG_UINT32_3 ("limit", limits, maxComputeWorkGroupCount);
  DEBUG_UINT32 ("limit", limits, maxComputeWorkGroupInvocations);
  DEBUG_UINT32_3 ("limit", limits, maxComputeWorkGroupSize);
  DEBUG_UINT32 ("limit", limits, subPixelPrecisionBits);
  DEBUG_UINT32 ("limit", limits, subTexelPrecisionBits);
  DEBUG_UINT32 ("limit", limits, mipmapPrecisionBits);
  DEBUG_UINT32 ("limit", limits, maxDrawIndexedIndexValue);
  DEBUG_UINT32 ("limit", limits, maxDrawIndirectCount);
  DEBUG_FLOAT ("limit", limits, maxSamplerLodBias);
  DEBUG_FLOAT ("limit", limits, maxSamplerAnisotropy);
  DEBUG_UINT32 ("limit", limits, maxViewports);
  DEBUG_UINT32_2 ("limit", limits, maxViewportDimensions);
  DEBUG_FLOAT_2 ("limit", limits, viewportBoundsRange);
  DEBUG_UINT32 ("limit", limits, viewportSubPixelBits);
  DEBUG_SIZE ("limit", limits, minMemoryMapAlignment);
  DEBUG_UINT64 ("limit", limits, minTexelBufferOffsetAlignment);
  DEBUG_UINT64 ("limit", limits, minUniformBufferOffsetAlignment);
  DEBUG_UINT64 ("limit", limits, minStorageBufferOffsetAlignment);
  DEBUG_INT32 ("limit", limits, minTexelOffset);
  DEBUG_UINT32 ("limit", limits, maxTexelOffset);
  DEBUG_INT32 ("limit", limits, minTexelGatherOffset);
  DEBUG_UINT32 ("limit", limits, maxTexelGatherOffset);
  DEBUG_FLOAT ("limit", limits, minInterpolationOffset);
  DEBUG_FLOAT ("limit", limits, maxInterpolationOffset);
  DEBUG_UINT32 ("limit", limits, subPixelInterpolationOffsetBits);
  DEBUG_UINT32 ("limit", limits, maxFramebufferWidth);
  DEBUG_UINT32 ("limit", limits, maxFramebufferHeight);
  DEBUG_UINT32 ("limit", limits, maxFramebufferLayers);
  DEBUG_FLAGS ("limit", limits, framebufferColorSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, framebufferDepthSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, framebufferStencilSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, framebufferNoAttachmentsSampleCounts, sample_count);
  DEBUG_UINT32 ("limit", limits, maxColorAttachments);
  DEBUG_FLAGS ("limit", limits, sampledImageColorSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, sampledImageIntegerSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, sampledImageDepthSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, sampledImageStencilSampleCounts, sample_count);
  DEBUG_FLAGS ("limit", limits, storageImageSampleCounts, sample_count);
  DEBUG_BOOL_STRUCT ("limit", limits, timestampComputeAndGraphics);
  DEBUG_FLOAT ("limit", limits, timestampPeriod);
  DEBUG_UINT32 ("limit", limits, maxClipDistances);
  DEBUG_UINT32 ("limit", limits, maxCullDistances);
  DEBUG_UINT32 ("limit", limits, maxCombinedClipAndCullDistances);
  DEBUG_UINT32 ("limit", limits, discreteQueuePriorities);
  DEBUG_FLOAT_2 ("limit", limits, pointSizeRange);
  DEBUG_FLOAT_2 ("limit", limits, lineWidthRange);
  DEBUG_FLOAT ("limit", limits, pointSizeGranularity);
  DEBUG_FLOAT ("limit", limits, lineWidthGranularity);
  DEBUG_BOOL_STRUCT ("limit", limits, strictLines);
  DEBUG_BOOL_STRUCT ("limit", limits, standardSampleLocations);
  DEBUG_UINT64 ("limit", limits, optimalBufferCopyOffsetAlignment);
  DEBUG_UINT64 ("limit", limits, optimalBufferCopyRowPitchAlignment);
  DEBUG_UINT64 ("limit", limits, nonCoherentAtomSize);
  /* *INDENT-ON* */

  return TRUE;
}

static gboolean
dump_sparse_properties (GstVulkanPhysicalDevice * device, GError ** error)
{
  VkPhysicalDeviceSparseProperties *props =
      &device->properties.sparseProperties;

  /* *INDENT-OFF* */
  DEBUG_BOOL_STRUCT ("sparse property", props, residencyStandard2DBlockShape);
  DEBUG_BOOL_STRUCT ("sparse property", props, residencyStandard2DMultisampleBlockShape);
  DEBUG_BOOL_STRUCT ("sparse property", props, residencyStandard3DBlockShape);
  DEBUG_BOOL_STRUCT ("sparse property", props, residencyAlignedMipSize);
  DEBUG_BOOL_STRUCT ("sparse property", props, residencyNonResidentStrict);
  /* *INDENT-ON* */

  return TRUE;
}

#if defined (VK_API_VERSION_1_2)
static void
dump_properties11 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan11Properties * properties)
{
  /* *INDENT-OFF* */
/*    uint8_t                    deviceUUID[VK_UUID_SIZE];
    uint8_t                    driverUUID[VK_UUID_SIZE];
    uint8_t                    deviceLUID[VK_LUID_SIZE];*/
  DEBUG_UINT32 ("properties (1.1)", properties, deviceNodeMask);
/*    VkBool32                   deviceLUIDValid;*/
  DEBUG_UINT32 ("properties (1.1)", properties, subgroupSize);
/*    VkShaderStageFlags         subgroupSupportedStages;
    VkSubgroupFeatureFlags     subgroupSupportedOperations;*/
  DEBUG_BOOL_STRUCT ("properties (1.1)", properties, subgroupQuadOperationsInAllStages);
/*    VkPointClippingBehavior    pointClippingBehavior;*/
  DEBUG_UINT32 ("properties (1.1)", properties, maxMultiviewViewCount);
  DEBUG_UINT32 ("properties (1.1)", properties, maxMultiviewInstanceIndex);
  DEBUG_BOOL_STRUCT ("properties (1.1)", properties, protectedNoFault);
  DEBUG_UINT32 ("properties (1.1)", properties, maxPerSetDescriptors);
  DEBUG_SIZE ("properties (1.1)", properties, maxMemoryAllocationSize);
  /* *INDENT-ON* */
}

static void
dump_properties12 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan12Properties * properties)
{
  /* *INDENT-OFF* */
/*    VkDriverId                           driverID;*/
  DEBUG_STRING ("properties (1.2)", properties, driverName);
  DEBUG_STRING ("properties (1.2)", properties, driverInfo);
/*    VkConformanceVersion                 conformanceVersion;
    VkShaderFloatControlsIndependence    denormBehaviorIndependence;
    VkShaderFloatControlsIndependence    roundingModeIndependence;*/
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderSignedZeroInfNanPreserveFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderSignedZeroInfNanPreserveFloat32);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderSignedZeroInfNanPreserveFloat64);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormPreserveFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormPreserveFloat32);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormPreserveFloat64);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormFlushToZeroFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormFlushToZeroFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderDenormFlushToZeroFloat32);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTEFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTEFloat32);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTEFloat64);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTZFloat16);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTZFloat32);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderRoundingModeRTZFloat64);
  DEBUG_UINT32 ("properties (1.2)", properties, maxUpdateAfterBindDescriptorsInAllPools);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderUniformBufferArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderSampledImageArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderSampledImageArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderStorageBufferArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderStorageImageArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, shaderInputAttachmentArrayNonUniformIndexingNative);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, robustBufferAccessUpdateAfterBind);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, quadDivergentImplicitLod);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindSamplers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindUniformBuffers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindStorageBuffers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindSampledImages);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindStorageImages);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageDescriptorUpdateAfterBindInputAttachments);
  DEBUG_UINT32 ("properties (1.2)", properties, maxPerStageUpdateAfterBindResources);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindSamplers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindUniformBuffers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindStorageBuffers);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindSampledImages);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindStorageImages);
  DEBUG_UINT32 ("properties (1.2)", properties, maxDescriptorSetUpdateAfterBindInputAttachments);
/*    VkResolveModeFlags                   supportedDepthResolveModes;
    VkResolveModeFlags                   supportedStencilResolveModes;*/
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, independentResolveNone);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, independentResolve);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, filterMinmaxSingleComponentFormats);
  DEBUG_BOOL_STRUCT ("properties (1.2)", properties, filterMinmaxImageComponentMapping);
  DEBUG_UINT64 ("properties (1.2)", properties, maxTimelineSemaphoreValueDifference);
  DEBUG_FLAGS ("properties (1.2)", properties, framebufferIntegerColorSampleCounts, sample_count);
  /* *INDENT-ON* */
}
#endif

#if defined(VK_API_VERSION_1_3)
static void
dump_properties13 (GstVulkanPhysicalDevice * device,
    VkPhysicalDeviceVulkan13Properties * properties)
{
  /* *INDENT-OFF* */
  DEBUG_UINT32 ("properties (1.3)", properties, minSubgroupSize);
  DEBUG_UINT32 ("properties (1.3)", properties, maxSubgroupSize);
  DEBUG_UINT32 ("properties (1.3)", properties, maxComputeWorkgroupSubgroups);
  /* VkShaderStageFlags    requiredSubgroupSizeStages; */
  DEBUG_UINT32 ("properties (1.3)", properties, maxInlineUniformBlockSize);
  DEBUG_UINT32 ("properties (1.3)", properties, maxPerStageDescriptorInlineUniformBlocks);
  DEBUG_UINT32 ("properties (1.3)", properties, maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks);
  DEBUG_UINT32 ("properties (1.3)", properties, maxDescriptorSetInlineUniformBlocks);
  DEBUG_UINT32 ("properties (1.3)", properties, maxDescriptorSetUpdateAfterBindInlineUniformBlocks);
  DEBUG_UINT32 ("properties (1.3)", properties, maxInlineUniformTotalSize);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct8BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct8BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct8BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct4x8BitPackedUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct4x8BitPackedSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct4x8BitPackedMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct16BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct16BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct16BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct32BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct32BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct32BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct64BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct64BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProduct64BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating8BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating8BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating16BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating16BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating32BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating32BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating64BitUnsignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating64BitSignedAccelerated);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated);
  DEBUG_SIZE ("properties (1.3)", properties, storageTexelBufferOffsetAlignmentBytes);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, storageTexelBufferOffsetSingleTexelAlignment);
  DEBUG_SIZE ("properties (1.3)", properties, uniformTexelBufferOffsetAlignmentBytes);
  DEBUG_BOOL_STRUCT ("properties (1.3)", properties, uniformTexelBufferOffsetSingleTexelAlignment);
  DEBUG_SIZE ("properties (1.3)", properties, maxBufferSize);
  /* *INDENT-ON* */
}
#endif

static gboolean
physical_device_info (GstVulkanPhysicalDevice * device, GError ** error)
{
#if defined (VK_API_VERSION_1_2)
  GstVulkanPhysicalDevicePrivate *priv = GET_PRIV (device);
  VkBaseOutStructure *iter;
#endif

  GST_INFO_OBJECT (device, "physical device %i name \'%s\' type \'%s\' "
      "api version %u.%u.%u, driver version %u.%u.%u vendor ID 0x%x, "
      "device ID 0x%x", device->device_index, device->properties.deviceName,
      gst_vulkan_physical_device_type_to_string (device->properties.deviceType),
      VK_VERSION_MAJOR (device->properties.apiVersion),
      VK_VERSION_MINOR (device->properties.apiVersion),
      VK_VERSION_PATCH (device->properties.apiVersion),
      VK_VERSION_MAJOR (device->properties.driverVersion),
      VK_VERSION_MINOR (device->properties.driverVersion),
      VK_VERSION_PATCH (device->properties.driverVersion),
      device->properties.vendorID, device->properties.deviceID);

  if (!dump_queue_properties (device, error))
    return FALSE;
  if (!dump_memory_properties (device, error))
    return FALSE;
  if (!dump_features (device, error))
    return FALSE;
  if (!dump_limits (device, error))
    return FALSE;
  if (!dump_sparse_properties (device, error))
    return FALSE;

#if defined (VK_API_VERSION_1_2)
  if (gst_vulkan_instance_check_version (device->instance, 1, 2, 0)) {
    for (iter = (VkBaseOutStructure *) & priv->properties10; iter;
        iter = iter->pNext) {
      if (iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES)
        dump_properties11 (device, (VkPhysicalDeviceVulkan11Properties *) iter);
      else if (iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES)
        dump_properties12 (device, (VkPhysicalDeviceVulkan12Properties *) iter);
#if defined (VK_API_VERSION_1_3)
      else if (gst_vulkan_instance_check_version (device->instance, 1, 3, 0)
          && iter->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES)
        dump_properties13 (device, (VkPhysicalDeviceVulkan13Properties *) iter);
#endif
    }
  }
#endif

  return TRUE;
}

static gboolean
gst_vulkan_physical_device_fill_info (GstVulkanPhysicalDevice * device,
    GError ** error)
{
  GstVulkanPhysicalDevicePrivate *priv = GET_PRIV (device);
  VkResult err;
  guint i;

  device->device = gst_vulkan_physical_device_get_handle (device);
  if (!device->device) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Failed to retrieve physical device");
    goto error;
  }

  err =
      vkEnumerateDeviceLayerProperties (device->device,
      &priv->n_available_layers, NULL);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateDeviceLayerProperties") < 0)
    goto error;

  priv->available_layers = g_new0 (VkLayerProperties, priv->n_available_layers);
  err =
      vkEnumerateDeviceLayerProperties (device->device,
      &priv->n_available_layers, priv->available_layers);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateDeviceLayerProperties") < 0) {
    goto error;
  }

  err =
      vkEnumerateDeviceExtensionProperties (device->device, NULL,
      &priv->n_available_extensions, NULL);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateDeviceExtensionProperties") < 0) {
    goto error;
  }

  priv->available_extensions =
      g_new0 (VkExtensionProperties, priv->n_available_extensions);
  err =
      vkEnumerateDeviceExtensionProperties (device->device, NULL,
      &priv->n_available_extensions, priv->available_extensions);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateDeviceExtensionProperties") < 0) {
    goto error;
  }

  GST_INFO_OBJECT (device, "found %u layers and %u extensions",
      priv->n_available_layers, priv->n_available_extensions);

  for (i = 0; i < priv->n_available_layers; i++)
    GST_DEBUG_OBJECT (device, "available layer %u: %s", i,
        priv->available_layers[i].layerName);
  for (i = 0; i < priv->n_available_extensions; i++)
    GST_DEBUG_OBJECT (device, "available extension %u: %s", i,
        priv->available_extensions[i].extensionName);

  vkGetPhysicalDeviceProperties (device->device, &device->properties);
#if defined (VK_API_VERSION_1_2)
  if (gst_vulkan_instance_check_version (device->instance, 1, 2, 0)) {
    PFN_vkGetPhysicalDeviceProperties2 get_props2;
    PFN_vkGetPhysicalDeviceMemoryProperties2 get_mem_props2;
    PFN_vkGetPhysicalDeviceFeatures2 get_features2;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2 get_queue_props2;
    VkPhysicalDeviceMemoryProperties2 mem_properties10;

    /* *INDENT-OFF* */
    mem_properties10 = (VkPhysicalDeviceMemoryProperties2) {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
      .pNext = NULL,
    };
    /* *INDENT-ON* */

    get_props2 = (PFN_vkGetPhysicalDeviceProperties2)
        gst_vulkan_instance_get_proc_address (device->instance,
        "vkGetPhysicalDeviceProperties2");
    get_props2 (device->device, &priv->properties10);

    get_mem_props2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)
        gst_vulkan_instance_get_proc_address (device->instance,
        "vkGetPhysicalDeviceMemoryProperties2");
    get_mem_props2 (device->device, &mem_properties10);
    memcpy (&device->memory_properties, &mem_properties10.memoryProperties,
        sizeof (device->memory_properties));

    get_features2 = (PFN_vkGetPhysicalDeviceFeatures2)
        gst_vulkan_instance_get_proc_address (device->instance,
        "vkGetPhysicalDeviceFeatures2");
    get_features2 (device->device, &priv->features10);
    memcpy (&device->features, &priv->features10.features,
        sizeof (device->features));

    get_queue_props2 = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)
        gst_vulkan_instance_get_proc_address (device->instance,
        "vkGetPhysicalDeviceQueueFamilyProperties2");
    get_queue_props2 (device->device, &device->n_queue_families, NULL);
    if (device->n_queue_families > 0) {
      VkQueueFamilyProperties2 *props;
      int i;
      void *next = NULL;
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
      VkQueueFamilyVideoPropertiesKHR *queue_family_video_props;
      VkQueueFamilyQueryResultStatusPropertiesKHR *queue_family_query_props;

      queue_family_video_props =
          g_new0 (VkQueueFamilyVideoPropertiesKHR, device->n_queue_families);
      queue_family_query_props =
          g_new0 (VkQueueFamilyQueryResultStatusPropertiesKHR,
          device->n_queue_families);
#endif
      props = g_new0 (VkQueueFamilyProperties2, device->n_queue_families);
      for (i = 0; i < device->n_queue_families; i++) {
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
        queue_family_query_props[i].sType =
            VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR;

        queue_family_video_props[i].sType =
            VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
        queue_family_video_props[i].pNext = &queue_family_query_props[i];

        next = &queue_family_video_props[i];
#endif
        props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        props[i].pNext = next;
      }

      get_queue_props2 (device->device, &device->n_queue_families, props);

      device->queue_family_props =
          g_new0 (VkQueueFamilyProperties, device->n_queue_families);
      device->queue_family_ops =
          g_new0 (GstVulkanQueueFamilyOps, device->n_queue_families);
      for (i = 0; i < device->n_queue_families; i++) {
        memcpy (&device->queue_family_props[i], &props[i].queueFamilyProperties,
            sizeof (device->queue_family_props[i]));

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
        device->queue_family_ops[i].video =
            queue_family_video_props[i].videoCodecOperations;
        device->queue_family_ops[i].query =
            queue_family_query_props[i].queryResultStatusSupport;
#endif
      }
      g_free (props);
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
      g_free (queue_family_video_props);
      g_free (queue_family_query_props);
#endif
    }
  } else
#endif
  {
    vkGetPhysicalDeviceMemoryProperties (device->device,
        &device->memory_properties);
    vkGetPhysicalDeviceFeatures (device->device, &device->features);
    vkGetPhysicalDeviceQueueFamilyProperties (device->device,
        &device->n_queue_families, NULL);
    if (device->n_queue_families > 0) {
      device->queue_family_props =
          g_new0 (VkQueueFamilyProperties, device->n_queue_families);
      vkGetPhysicalDeviceQueueFamilyProperties (device->device,
          &device->n_queue_families, device->queue_family_props);
    }
  }

  if (!physical_device_info (device, error))
    goto error;

  return TRUE;

error:
  return FALSE;
}

/**
 * gst_vulkan_physical_device_get_handle: (skip)
 * @device: a #GstVulkanPhysicalDevice
 *
 * Returns: The associated `VkPhysicalDevice` handle
 *
 * Since: 1.18
 */
VkPhysicalDevice
gst_vulkan_physical_device_get_handle (GstVulkanPhysicalDevice * device)
{
  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), NULL);

  return device->device;
}

/**
 * gst_vulkan_physical_device_get_instance:
 * @device: a #GstVulkanPhysicalDevice
 *
 * Returns: (transfer full): The #GstVulkanInstance associated with this physical device
 *
 * Since: 1.18
 */
GstVulkanInstance *
gst_vulkan_physical_device_get_instance (GstVulkanPhysicalDevice * device)
{
  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), NULL);

  return gst_object_ref (device->instance);
}

static gboolean
gst_vulkan_physical_device_get_layer_info_unlocked (GstVulkanPhysicalDevice *
    device, const gchar * name, gchar ** description, guint32 * spec_version,
    guint32 * implementation_version)
{
  GstVulkanPhysicalDevicePrivate *priv;
  int i;

  priv = GET_PRIV (device);

  for (i = 0; i < priv->n_available_layers; i++) {
    if (g_strcmp0 (name, priv->available_layers[i].layerName) == 0) {
      if (description)
        *description = g_strdup (priv->available_layers[i].description);
      if (spec_version)
        *spec_version = priv->available_layers[i].specVersion;
      if (implementation_version)
        *spec_version = priv->available_layers[i].implementationVersion;
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * gst_vulkan_physical_device_get_layer_info:
 * @device: a #GstVulkanPhysicalDevice
 * @name: the layer name to look for
 * @description: (out) (nullable): return value for the layer description or %NULL
 * @spec_version: (out) (nullable): return value for the layer specification version
 * @implementation_version: (out) (nullable): return value for the layer implementation version
 *
 * Retrieves information about a layer.
 *
 * Will not find any layers before gst_vulkan_instance_fill_info() has been
 * called.
 *
 * Returns: whether layer @name is available
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_physical_device_get_layer_info (GstVulkanPhysicalDevice * device,
    const gchar * name, gchar ** description, guint32 * spec_version,
    guint32 * implementation_version)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret =
      gst_vulkan_physical_device_get_layer_info_unlocked (device, name,
      description, spec_version, implementation_version);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

static gboolean
gst_vulkan_physical_device_get_extension_info_unlocked (GstVulkanPhysicalDevice
    * device, const gchar * name, guint32 * spec_version)
{
  GstVulkanPhysicalDevicePrivate *priv;
  int i;

  priv = GET_PRIV (device);

  for (i = 0; i < priv->n_available_extensions; i++) {
    if (g_strcmp0 (name, priv->available_extensions[i].extensionName) == 0) {
      if (spec_version)
        *spec_version = priv->available_extensions[i].specVersion;
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * gst_vulkan_physical_device_get_extension_info:
 * @device: a #GstVulkanPhysicalDevice
 * @name: the extension name to look for
 * @spec_version: (out) (nullable): return value for the exteion specification version
 *
 * Retrieves information about a device extension.
 *
 * Will not find any extensions before gst_vulkan_instance_fill_info() has been
 * called.
 *
 * Returns: whether extension @name is available
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_physical_device_get_extension_info (GstVulkanPhysicalDevice * device,
    const gchar * name, guint32 * spec_version)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (device);
  ret =
      gst_vulkan_physical_device_get_extension_info_unlocked (device, name,
      spec_version);
  GST_OBJECT_UNLOCK (device);

  return ret;
}

const VkPhysicalDeviceFeatures2 *
gst_vulkan_physical_device_get_features (GstVulkanPhysicalDevice * device)
{
#if defined (VK_API_VERSION_1_2)
  GstVulkanPhysicalDevicePrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_PHYSICAL_DEVICE (device), FALSE);

  priv = GET_PRIV (device);
  if (gst_vulkan_instance_check_version (device->instance, 1, 2, 0))
    return &priv->features10;
#endif
  return NULL;
}
