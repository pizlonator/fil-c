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

#include "gstvkinstance.h"

#include <string.h>

/**
 * SECTION:vkinstance
 * @title: GstVulkanInstance
 * @short_description: GStreamer Vulkan instance
 * @see_also: #GstVulkanPhysicalDevice, #GstVulkanDevice, #GstVulkanDisplay
 *
 * #GstVulkanInstance encapsulates the necessary information for the toplevel
 * Vulkan instance object.
 *
 * If GStreamer is built with debugging support, the default Vulkan API chosen
 * can be selected with the environment variable
 * `GST_VULKAN_INSTANCE_API_VERSION=1.0`.  Any subsequent setting of the
 * requested Vulkan API version through the available properties will override
 * the environment variable.
 */

#define APP_SHORT_NAME "GStreamer"

#define GST_CAT_DEFAULT gst_vulkan_instance_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);
GST_DEBUG_CATEGORY (GST_VULKAN_DEBUG_CAT);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

enum
{
  SIGNAL_0,
  SIGNAL_CREATE_DEVICE,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_REQUESTED_API_MAJOR_VERSION,
  PROP_REQUESTED_API_MINOR_VERSION,
};

#define DEFAULT_REQUESTED_API_VERSION_MAJOR 0
#define DEFAULT_REQUESTED_API_VERSION_MINOR 0

static guint gst_vulkan_instance_signals[LAST_SIGNAL] = { 0 };

static void gst_vulkan_instance_finalize (GObject * object);

struct _GstVulkanInstancePrivate
{
  gboolean info_collected;
  gboolean opened;
  guint requested_api_major;
  guint requested_api_minor;
  uint32_t supported_instance_api;

  guint n_available_layers;
  VkLayerProperties *available_layers;
  guint n_available_extensions;
  VkExtensionProperties *available_extensions;
  GPtrArray *enabled_layers;
  GPtrArray *enabled_extensions;

#if !defined (GST_DISABLE_DEBUG)
  VkDebugReportCallbackEXT msg_callback;
  PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;
  PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback;
  PFN_vkDebugReportMessageEXT dbgReportMessage;
#endif
};

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkaninstance", 0,
        "Vulkan Instance");
    GST_DEBUG_CATEGORY_INIT (GST_VULKAN_DEBUG_CAT, "vulkandebug", 0,
        "Vulkan Debug");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&_init, 1);
  }
}

#define GET_PRIV(instance) gst_vulkan_instance_get_instance_private (instance)

#define gst_vulkan_instance_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanInstance, gst_vulkan_instance,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstVulkanInstance)
    _init_debug ());

/**
 * gst_vulkan_instance_new:
 *
 * Returns: (transfer full): a new uninitialized #GstVulkanInstance
 *
 * Since: 1.18
 */
GstVulkanInstance *
gst_vulkan_instance_new (void)
{
  GstVulkanInstance *instance;

  instance = g_object_new (GST_TYPE_VULKAN_INSTANCE, NULL);
  gst_object_ref_sink (instance);

  return instance;
}

static void
gst_vulkan_instance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanInstance *instance = GST_VULKAN_INSTANCE (object);
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  GST_OBJECT_LOCK (instance);
  switch (prop_id) {
    case PROP_REQUESTED_API_MAJOR_VERSION:
      if (priv->opened)
        g_warning ("Attempt to set the requested API version after the "
            "instance has been opened");
      priv->requested_api_major = g_value_get_uint (value);
      break;
    case PROP_REQUESTED_API_MINOR_VERSION:
      if (priv->opened)
        g_warning ("Attempt to set the requested API version after the "
            "instance has been opened");
      priv->requested_api_minor = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (instance);
}

static void
gst_vulkan_instance_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstVulkanInstance *instance = GST_VULKAN_INSTANCE (object);
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  GST_OBJECT_LOCK (instance);
  switch (prop_id) {
    case PROP_REQUESTED_API_MAJOR_VERSION:
      g_value_set_uint (value, priv->requested_api_major);
      break;
    case PROP_REQUESTED_API_MINOR_VERSION:
      g_value_set_uint (value, priv->requested_api_minor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (instance);
}

static void
gst_vulkan_instance_init (GstVulkanInstance * instance)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  priv->requested_api_major = DEFAULT_REQUESTED_API_VERSION_MAJOR;
  priv->requested_api_minor = DEFAULT_REQUESTED_API_VERSION_MINOR;

  priv->enabled_layers = g_ptr_array_new_with_free_func (g_free);
  priv->enabled_extensions = g_ptr_array_new_with_free_func (g_free);

#if !defined (GST_DISABLE_DEBUG)
  {
    const gchar *api_override = g_getenv ("GST_VULKAN_INSTANCE_API_VERSION");
    if (api_override) {
      gchar *end;
      gint64 major, minor;

      major = g_ascii_strtoll (api_override, &end, 10);
      if (end && end[0] == '.') {
        minor = g_ascii_strtoll (&end[1], NULL, 10);
        if (major > 0 && major < G_MAXINT64 && minor >= 0 && minor < G_MAXINT64) {
          priv->requested_api_major = major;
          priv->requested_api_minor = minor;
        }
      }
    }
  }
#endif
}

static void
gst_vulkan_instance_class_init (GstVulkanInstanceClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gst_vulkan_memory_init_once ();
  gst_vulkan_image_memory_init_once ();
  gst_vulkan_buffer_memory_init_once ();

  gobject_class->get_property = gst_vulkan_instance_get_property;
  gobject_class->set_property = gst_vulkan_instance_set_property;
  gobject_class->finalize = gst_vulkan_instance_finalize;

  /**
   * GstVulkanInstance:requested-api-major:
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class,
      PROP_REQUESTED_API_MAJOR_VERSION,
      g_param_spec_uint ("requested-api-major", "Requested API Major",
          "Major version of the requested Vulkan API (0 = maximum supported)",
          0, G_MAXUINT, DEFAULT_REQUESTED_API_VERSION_MAJOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVulkanInstance:requested-api-minor:
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class,
      PROP_REQUESTED_API_MINOR_VERSION,
      g_param_spec_uint ("requested-api-minor", "Requested API Minor",
          "Minor version of the requested Vulkan API",
          0, G_MAXUINT, DEFAULT_REQUESTED_API_VERSION_MINOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVulkanInstance::create-device:
   * @object: the #GstVulkanDisplay
   *
   * Overrides the #GstVulkanDevice creation mechanism.
   * It can be called from any thread.
   *
   * Returns: (transfer full): the newly created #GstVulkanDevice.
   *
   * Since: 1.18
   */
  gst_vulkan_instance_signals[SIGNAL_CREATE_DEVICE] =
      g_signal_new ("create-device", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_VULKAN_DEVICE, 0);
}

static void
gst_vulkan_instance_finalize (GObject * object)
{
  GstVulkanInstance *instance = GST_VULKAN_INSTANCE (object);
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  if (priv->opened) {
    if (priv->dbgDestroyDebugReportCallback)
      priv->dbgDestroyDebugReportCallback (instance->instance,
          priv->msg_callback, NULL);

    g_free (instance->physical_devices);
  }
  priv->opened = FALSE;

  if (instance->instance)
    vkDestroyInstance (instance->instance, NULL);
  instance->instance = NULL;

  g_free (priv->available_layers);
  priv->available_layers = NULL;

  g_free (priv->available_extensions);
  priv->available_extensions = NULL;

  g_ptr_array_unref (priv->enabled_layers);
  priv->enabled_layers = NULL;

  g_ptr_array_unref (priv->enabled_extensions);
  priv->enabled_extensions = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

VKAPI_ATTR static VkBool32
_gst_vk_debug_callback (VkDebugReportFlagsEXT msgFlags,
    VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location,
    int32_t msgCode, const char *pLayerPrefix, const char *pMsg,
    void *pUserData)
{
  if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    GST_CAT_ERROR (GST_VULKAN_DEBUG_CAT, "[%s] Code %d : %s", pLayerPrefix,
        msgCode, pMsg);
    g_critical ("[%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    GST_CAT_WARNING (GST_VULKAN_DEBUG_CAT, "[%s] Code %d : %s", pLayerPrefix,
        msgCode, pMsg);
    g_warning ("[%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    GST_CAT_LOG (GST_VULKAN_DEBUG_CAT, "[%s] Code %d : %s", pLayerPrefix,
        msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    GST_CAT_FIXME (GST_VULKAN_DEBUG_CAT, "[%s] Code %d : %s", pLayerPrefix,
        msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    GST_CAT_TRACE (GST_VULKAN_DEBUG_CAT, "[%s] Code %d : %s", pLayerPrefix,
        msgCode, pMsg);
  } else {
    return FALSE;
  }

  /*
   * false indicates that layer should not bail-out of an
   * API call that had validation failures. This may mean that the
   * app dies inside the driver due to invalid parameter(s).
   * That's what would happen without validation layers, so we'll
   * keep that behavior here.
   */
  return FALSE;
}

static gboolean
gst_vulkan_instance_get_layer_info_unlocked (GstVulkanInstance * instance,
    const gchar * name, gchar ** description, guint32 * spec_version,
    guint32 * implementation_version)
{
  GstVulkanInstancePrivate *priv;
  int i;

  priv = GET_PRIV (instance);

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
 * gst_vulkan_instance_get_layer_info:
 * @instance: a #GstVulkanInstance
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
gst_vulkan_instance_get_layer_info (GstVulkanInstance * instance,
    const gchar * name, gchar ** description, guint32 * spec_version,
    guint32 * implementation_version)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret =
      gst_vulkan_instance_get_layer_info_unlocked (instance, name, description,
      spec_version, implementation_version);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

G_GNUC_INTERNAL gboolean
gst_vulkan_instance_get_extension_info_unlocked (GstVulkanInstance * instance,
    const gchar * name, guint32 * spec_version);

G_GNUC_INTERNAL gboolean
gst_vulkan_instance_get_extension_info_unlocked (GstVulkanInstance * instance,
    const gchar * name, guint32 * spec_version)
{
  GstVulkanInstancePrivate *priv;
  int i;

  priv = GET_PRIV (instance);

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
 * gst_vulkan_instance_get_extension_info:
 * @instance: a #GstVulkanInstance
 * @name: the layer name to look for
 * @spec_version: (out) (nullable): return value for the layer specification version
 *
 * Retrieves information about an extension.
 *
 * Will not find any extensions before gst_vulkan_instance_fill_info() has been
 * called.
 *
 * Returns: whether extension @name is available
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_get_extension_info (GstVulkanInstance * instance,
    const gchar * name, guint32 * spec_version)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret =
      gst_vulkan_instance_get_extension_info_unlocked (instance, name,
      spec_version);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static gboolean
gst_vulkan_instance_is_extension_enabled_unlocked (GstVulkanInstance * instance,
    const gchar * name, guint * index)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  return g_ptr_array_find_with_equal_func (priv->enabled_extensions, name,
      g_str_equal, index);
}

/**
 * gst_vulkan_instance_is_extension_enabled:
 * @instance: a # GstVulkanInstance
 * @name: extension name
 *
 * Returns: whether extension @name is enabled
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_is_extension_enabled (GstVulkanInstance * instance,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret =
      gst_vulkan_instance_is_extension_enabled_unlocked (instance, name, NULL);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static gboolean
gst_vulkan_instance_enable_extension_unlocked (GstVulkanInstance * instance,
    const gchar * name)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);
  gboolean extension_is_available = FALSE;
  guint i;

  if (gst_vulkan_instance_is_extension_enabled_unlocked (instance, name, NULL))
    /* extension is already enabled */
    return TRUE;

  for (i = 0; i < priv->n_available_extensions; i++) {
    if (g_strcmp0 (name, priv->available_extensions[i].extensionName) == 0) {
      extension_is_available = TRUE;
      break;
    }
  }

  if (!extension_is_available)
    return FALSE;

  g_ptr_array_add (priv->enabled_extensions, g_strdup (name));

  return TRUE;
}

/**
 * gst_vulkan_instance_enable_extension:
 * @instance: a #GstVulkanInstance
 * @name: extension name to enable
 *
 * Enable an Vulkan extension by @name.  Extensions cannot be enabled until
 * gst_vulkan_instance_fill_info() has been called.  Enabling an extension will
 * only have an effect before the call to gst_vulkan_instance_open().
 *
 * Returns: whether the Vulkan extension could be enabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_enable_extension (GstVulkanInstance * instance,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret = gst_vulkan_instance_enable_extension_unlocked (instance, name);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static gboolean
gst_vulkan_instance_disable_extension_unlocked (GstVulkanInstance * instance,
    const gchar * name)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);
  gboolean extension_is_available = FALSE;
  guint i;

  for (i = 0; i < priv->n_available_extensions; i++) {
    if (g_strcmp0 (name, priv->available_extensions[i].extensionName) == 0) {
      extension_is_available = TRUE;
      break;
    }
  }

  if (!extension_is_available)
    return FALSE;

  if (!gst_vulkan_instance_is_extension_enabled_unlocked (instance, name, &i))
    /* extension is already enabled */
    return TRUE;

  g_ptr_array_remove_index_fast (priv->enabled_extensions, i);

  return TRUE;
}

/**
 * gst_vulkan_instance_disable_extension:
 * @instance: a #GstVulkanInstance
 * @name: extension name to enable
 *
 * Disable an Vulkan extension by @name.  Disabling an extension will only have
 * an effect before the call to gst_vulkan_instance_open().
 *
 * Returns: whether the Vulkan extension could be disabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_disable_extension (GstVulkanInstance * instance,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret = gst_vulkan_instance_disable_extension_unlocked (instance, name);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static gboolean
gst_vulkan_instance_is_layer_enabled_unlocked (GstVulkanInstance * instance,
    const gchar * name)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);

  return g_ptr_array_find_with_equal_func (priv->enabled_layers, name,
      g_str_equal, NULL);
}

/**
 * gst_vulkan_instance_is_layer_enabled:
 * @instance: a # GstVulkanInstance
 * @name: layer name
 *
 * Returns: whether layer @name is enabled
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_is_layer_enabled (GstVulkanInstance * instance,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret = gst_vulkan_instance_is_layer_enabled_unlocked (instance, name);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static gboolean
gst_vulkan_instance_enable_layer_unlocked (GstVulkanInstance * instance,
    const gchar * name)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);
  gboolean layer_is_available = FALSE;
  guint i;

  if (gst_vulkan_instance_is_layer_enabled_unlocked (instance, name))
    /* layer is already enabled */
    return TRUE;

  for (i = 0; i < priv->n_available_layers; i++) {
    if (g_strcmp0 (name, priv->available_layers[i].layerName) == 0) {
      layer_is_available = TRUE;
      break;
    }
  }

  if (!layer_is_available)
    return FALSE;

  g_ptr_array_add (priv->enabled_layers, g_strdup (name));

  return TRUE;
}

/**
 * gst_vulkan_instance_enable_layer:
 * @instance: a #GstVulkanInstance
 * @name: layer name to enable
 *
 * Enable an Vulkan layer by @name.  Layer cannot be enabled until
 * gst_vulkan_instance_fill_info() has been called.  Enabling a layer will
 * only have an effect before the call to gst_vulkan_instance_open().
 *
 * Returns: whether the Vulkan layer could be enabled.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_enable_layer (GstVulkanInstance * instance,
    const gchar * name)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  GST_OBJECT_LOCK (instance);
  ret = gst_vulkan_instance_enable_layer_unlocked (instance, name);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

static void
gst_vulkan_get_supported_api_version_unlocked (GstVulkanInstance * instance)
{
  GstVulkanInstancePrivate *priv = GET_PRIV (instance);
  PFN_vkEnumerateInstanceVersion gst_vkEnumerateInstanceVersion;

  if (priv->supported_instance_api)
    return;

  gst_vkEnumerateInstanceVersion =
      (PFN_vkEnumerateInstanceVersion) vkGetInstanceProcAddr (NULL,
      "vkEnumerateInstanceVersion");

  if (!gst_vkEnumerateInstanceVersion
      || VK_SUCCESS !=
      gst_vkEnumerateInstanceVersion (&priv->supported_instance_api)) {
    priv->supported_instance_api = VK_MAKE_VERSION (1, 0, 0);
  }
}

G_GNUC_INTERNAL GstVulkanDisplayType
gst_vulkan_display_choose_type_unlocked (GstVulkanInstance * instance);

static gboolean
gst_vulkan_instance_fill_info_unlocked (GstVulkanInstance * instance,
    GError ** error)
{
  GstVulkanInstancePrivate *priv;
  VkResult err;
  guint i;

  priv = GET_PRIV (instance);

  if (priv->info_collected)
    return TRUE;
  priv->info_collected = TRUE;

  gst_vulkan_get_supported_api_version_unlocked (instance);

  /* Look for validation layers */
  err = vkEnumerateInstanceLayerProperties (&priv->n_available_layers, NULL);
  if (gst_vulkan_error_to_g_error (err, error,
          "vKEnumerateInstanceLayerProperties") < 0) {
    return FALSE;
  }

  priv->available_layers = g_new0 (VkLayerProperties, priv->n_available_layers);
  err =
      vkEnumerateInstanceLayerProperties (&priv->n_available_layers,
      priv->available_layers);
  if (gst_vulkan_error_to_g_error (err, error,
          "vKEnumerateInstanceLayerProperties") < 0) {
    return FALSE;
  }

  err =
      vkEnumerateInstanceExtensionProperties (NULL,
      &priv->n_available_extensions, NULL);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateInstanceExtensionProperties") < 0) {
    return FALSE;
  }

  priv->available_extensions =
      g_new0 (VkExtensionProperties, priv->n_available_extensions);
  err =
      vkEnumerateInstanceExtensionProperties (NULL,
      &priv->n_available_extensions, priv->available_extensions);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumerateInstanceExtensionProperties") < 0) {
    return FALSE;
  }

  GST_INFO_OBJECT (instance, "found %u layers and %u extensions",
      priv->n_available_layers, priv->n_available_extensions);

  for (i = 0; i < priv->n_available_layers; i++)
    GST_DEBUG_OBJECT (instance, "available layer %u: %s", i,
        priv->available_layers[i].layerName);
  for (i = 0; i < priv->n_available_extensions; i++)
    GST_DEBUG_OBJECT (instance, "available extension %u: %s", i,
        priv->available_extensions[i].extensionName);

  /* configure default extensions */
  {
    GstVulkanDisplayType display_type;
    const gchar *winsys_ext_name;
    GstDebugLevel vulkan_debug_level;

    display_type = gst_vulkan_display_choose_type_unlocked (instance);

    winsys_ext_name =
        gst_vulkan_display_type_to_extension_string (display_type);
    if (!winsys_ext_name) {
      GST_WARNING_OBJECT (instance, "No window system extension enabled");
    } else if (gst_vulkan_instance_get_extension_info_unlocked (instance,
            VK_KHR_SURFACE_EXTENSION_NAME, NULL)
        && gst_vulkan_instance_get_extension_info_unlocked (instance,
            winsys_ext_name, NULL)) {
      gst_vulkan_instance_enable_extension_unlocked (instance,
          VK_KHR_SURFACE_EXTENSION_NAME);
      gst_vulkan_instance_enable_extension_unlocked (instance, winsys_ext_name);
    }
#if !defined (GST_DISABLE_DEBUG)
    vulkan_debug_level =
        gst_debug_category_get_threshold (GST_VULKAN_DEBUG_CAT);

    if (vulkan_debug_level >= GST_LEVEL_ERROR) {
      if (gst_vulkan_instance_get_extension_info_unlocked (instance,
              VK_EXT_DEBUG_REPORT_EXTENSION_NAME, NULL)) {
        gst_vulkan_instance_enable_extension_unlocked (instance,
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
      }
    }
#endif
  }

  return TRUE;
}

/**
 * gst_vulkan_instance_fill_info:
 * @instance: a #GstVulkanInstance
 * @error: #GError
 *
 * Retrieve as much information about the available Vulkan instance without
 * actually creating an Vulkan instance.  Will not do anything while @instance
 * is open.
 *
 * Returns: whether the instance information could be retrieved
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_fill_info (GstVulkanInstance * instance, GError ** error)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);

  GST_OBJECT_LOCK (instance);
  ret = gst_vulkan_instance_fill_info_unlocked (instance, error);
  GST_OBJECT_UNLOCK (instance);

  return ret;
}

/**
 * gst_vulkan_instance_open:
 * @instance: a #GstVulkanInstance
 * @error: #GError
 *
 * Returns: whether the instance could be created
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_open (GstVulkanInstance * instance, GError ** error)
{
  GstVulkanInstancePrivate *priv;
  uint32_t requested_instance_api;
  GstDebugLevel vulkan_debug_level;
  VkResult err;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);

  priv = GET_PRIV (instance);

  GST_OBJECT_LOCK (instance);
  if (priv->opened) {
    GST_OBJECT_UNLOCK (instance);
    return TRUE;
  }

  if (!gst_vulkan_instance_fill_info_unlocked (instance, error))
    goto error;

  if (priv->requested_api_major) {
    requested_instance_api =
        VK_MAKE_VERSION (priv->requested_api_major, priv->requested_api_minor,
        0);
  } else {
    requested_instance_api = priv->supported_instance_api;
  }

  if (requested_instance_api > priv->supported_instance_api) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Requested API version (%u.%u) is larger than the maximum supported "
        "version (%u.%u)", VK_VERSION_MAJOR (requested_instance_api),
        VK_VERSION_MINOR (requested_instance_api),
        VK_VERSION_MAJOR (priv->supported_instance_api),
        VK_VERSION_MINOR (priv->supported_instance_api));
    goto error;
  }

  /* list of known vulkan loader environment variables taken from:
   * https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/loader/LoaderAndLayerInterface.md#table-of-debug-environment-variables */
  GST_DEBUG_OBJECT (instance, "VK_ICD_FILENAMES: %s",
      g_getenv ("VK_ICD_FILENAMES"));
  GST_DEBUG_OBJECT (instance, "VK_INSTANCE_LAYERS: %s",
      g_getenv ("VK_INSTANCE_LAYERS"));
  GST_DEBUG_OBJECT (instance, "VK_LAYER_PATH: %s", g_getenv ("VK_LAYER_PATH"));
  GST_DEBUG_OBJECT (instance, "VK_LOADER_DISABLE_INST_EXT_FILTER: %s",
      g_getenv ("VK_LOADER_DISABLE_INST_EXT_FILTER"));
  GST_DEBUG_OBJECT (instance, "VK_LOADER_DEBUG: %s",
      g_getenv ("VK_LOADER_DEBUG"));

  {
    guint i;

    GST_INFO_OBJECT (instance, "attempting to create instance for Vulkan API "
        "%u.%u, max supported %u.%u with %u layers and %u extensions",
        VK_VERSION_MAJOR (requested_instance_api),
        VK_VERSION_MINOR (requested_instance_api),
        VK_VERSION_MAJOR (priv->supported_instance_api),
        VK_VERSION_MINOR (priv->supported_instance_api),
        priv->enabled_layers->len, priv->enabled_extensions->len);

    for (i = 0; i < priv->enabled_layers->len; i++)
      GST_DEBUG_OBJECT (instance, "layer %u: %s", i,
          (gchar *) g_ptr_array_index (priv->enabled_layers, i));
    for (i = 0; i < priv->enabled_extensions->len; i++)
      GST_DEBUG_OBJECT (instance, "extension %u: %s", i,
          (gchar *) g_ptr_array_index (priv->enabled_extensions, i));
  }

  {
    VkApplicationInfo app = { 0, };
    VkInstanceCreateInfo inst_info = { 0, };
#if !defined (GST_DISABLE_DEBUG) && defined (VK_API_VERSION_1_2)
    VkValidationFeaturesEXT validation_features;
    VkValidationFeatureEnableEXT feat_list[] = {
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
#if defined (VK_API_VERSION_1_3)
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
#endif
    };
#endif

    /* *INDENT-OFF* */
    app = (VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = APP_SHORT_NAME,
        .applicationVersion = 0,
        .pEngineName = APP_SHORT_NAME,
        .engineVersion = VK_MAKE_VERSION (GST_VERSION_MAJOR, GST_VERSION_MINOR,
            GST_VERSION_MICRO),
        .apiVersion = requested_instance_api,
    };

    inst_info = (VkInstanceCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &app,
        .enabledLayerCount = priv->enabled_layers->len,
        .ppEnabledLayerNames = (const char *const *) priv->enabled_layers->pdata,
        .enabledExtensionCount = priv->enabled_extensions->len,
        .ppEnabledExtensionNames = (const char *const *) priv->enabled_extensions->pdata,
    };
    /* *INDENT-ON* */

#if !defined (GST_DISABLE_DEBUG)
    vulkan_debug_level =
        gst_debug_category_get_threshold (GST_VULKAN_DEBUG_CAT);

#if defined (VK_API_VERSION_1_2)
    if (vulkan_debug_level >= GST_LEVEL_ERROR) {
      /* *INDENT-OFF* */
      validation_features = (VkValidationFeaturesEXT) {
          .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
          .pEnabledValidationFeatures = feat_list,
          .enabledValidationFeatureCount = G_N_ELEMENTS (feat_list),
      };
      inst_info.pNext = &validation_features;
      /* *INDENT-ON* */
    }
#endif
#endif

    err = vkCreateInstance (&inst_info, NULL, &instance->instance);
    if (gst_vulkan_error_to_g_error (err, error, "vkCreateInstance") < 0) {
      goto error;
    }
  }

  err =
      vkEnumeratePhysicalDevices (instance->instance,
      &instance->n_physical_devices, NULL);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumeratePhysicalDevices") < 0)
    goto error;

  if (instance->n_physical_devices == 0) {
    GST_WARNING_OBJECT (instance, "No available physical device");
    g_set_error_literal (error,
        GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        "No available physical device");
    goto error;
  }

  instance->physical_devices =
      g_new0 (VkPhysicalDevice, instance->n_physical_devices);
  err =
      vkEnumeratePhysicalDevices (instance->instance,
      &instance->n_physical_devices, instance->physical_devices);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkEnumeratePhysicalDevices") < 0)
    goto error;

#if !defined (GST_DISABLE_DEBUG)
  if (vulkan_debug_level >= GST_LEVEL_ERROR
      && gst_vulkan_instance_is_extension_enabled_unlocked (instance,
          VK_EXT_DEBUG_REPORT_EXTENSION_NAME, NULL)) {
    VkDebugReportCallbackCreateInfoEXT info = { 0, };

    priv->dbgCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)
        gst_vulkan_instance_get_proc_address (instance,
        "vkCreateDebugReportCallbackEXT");
    if (!priv->dbgCreateDebugReportCallback) {
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Failed to retrieve vkCreateDebugReportCallback");
      goto error;
    }
    priv->dbgDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)
        gst_vulkan_instance_get_proc_address (instance,
        "vkDestroyDebugReportCallbackEXT");
    if (!priv->dbgDestroyDebugReportCallback) {
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Failed to retrieve vkDestroyDebugReportCallback");
      goto error;
    }
    priv->dbgReportMessage = (PFN_vkDebugReportMessageEXT)
        gst_vulkan_instance_get_proc_address (instance,
        "vkDebugReportMessageEXT");
    if (!priv->dbgReportMessage) {
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Failed to retrieve vkDebugReportMessage");
      goto error;
    }

    info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    info.pNext = NULL;
    info.flags = 0;
    info.pfnCallback = (PFN_vkDebugReportCallbackEXT) _gst_vk_debug_callback;
    info.pUserData = NULL;
    /* matches the conditions in _gst_vk_debug_callback() */
    if (vulkan_debug_level >= GST_LEVEL_ERROR)
      info.flags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
    if (vulkan_debug_level >= GST_LEVEL_WARNING)
      info.flags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
    if (vulkan_debug_level >= GST_LEVEL_FIXME)
      info.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    if (vulkan_debug_level >= GST_LEVEL_LOG)
      info.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
    if (vulkan_debug_level >= GST_LEVEL_TRACE)
      info.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;

    err =
        priv->dbgCreateDebugReportCallback (instance->instance, &info, NULL,
        &priv->msg_callback);
    if (gst_vulkan_error_to_g_error (err, error,
            "vkCreateDebugReportCallback") < 0)
      goto error;
  }
#endif

  priv->opened = TRUE;
  GST_OBJECT_UNLOCK (instance);

  return TRUE;

error:
  {
    GST_OBJECT_UNLOCK (instance);
    return FALSE;
  }
}

/**
 * gst_vulkan_instance_get_proc_address:
 * @instance: a #GstVulkanInstance
 * @name: name of the function to retrieve
 *
 * Performs `vkGetInstanceProcAddr()` with @instance and @name
 *
 * Returns: (nullable): the function pointer for @name or %NULL
 *
 * Since: 1.18
 */
gpointer
gst_vulkan_instance_get_proc_address (GstVulkanInstance * instance,
    const gchar * name)
{
  gpointer ret;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), NULL);
  g_return_val_if_fail (instance->instance != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  ret = vkGetInstanceProcAddr (instance->instance, name);

  GST_TRACE_OBJECT (instance, "%s = %p", name, ret);

  return ret;
}

/**
 * gst_vulkan_instance_create_device:
 * @instance: a #GstVulkanInstance
 * @error: (out) (optional): a #GError
 *
 * Returns: (transfer full): a new #GstVulkanDevice
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_instance_create_device (GstVulkanInstance * instance,
    GError ** error)
{
  GstVulkanDevice *device;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), NULL);

  g_signal_emit (instance, gst_vulkan_instance_signals[SIGNAL_CREATE_DEVICE], 0,
      &device);

  if (!device) {
    device = gst_vulkan_device_new_with_index (instance, 0);
  }

  if (!gst_vulkan_device_open (device, error)) {
    gst_object_unref (device);
    device = NULL;
  }

  return device;
}

/**
 * gst_context_set_vulkan_instance:
 * @context: a #GstContext
 * @instance: a #GstVulkanInstance
 *
 * Sets @instance on @context
 *
 * Since: 1.18
 */
void
gst_context_set_vulkan_instance (GstContext * context,
    GstVulkanInstance * instance)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);
  g_return_if_fail (gst_context_is_writable (context));

  if (instance)
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVulkanInstance(%" GST_PTR_FORMAT ") on context(%"
        GST_PTR_FORMAT ")", instance, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_INSTANCE, instance, NULL);
}

/**
 * gst_context_get_vulkan_instance:
 * @context: a #GstContext
 * @instance: resulting #GstVulkanInstance
 *
 * Returns: Whether @instance was in @context
 *
 * Since: 1.18
 */
gboolean
gst_context_get_vulkan_instance (GstContext * context,
    GstVulkanInstance ** instance)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (instance != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_INSTANCE, instance, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVulkanInstance(%" GST_PTR_FORMAT
      ") from context(%" GST_PTR_FORMAT ")", *instance, context);

  return ret;
}

/**
 * gst_vulkan_instance_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type #GST_QUERY_CONTEXT
 * @instance: (nullable): the #GstVulkanInstance
 *
 * If a #GstVulkanInstance is requested in @query, sets @instance as the reply.
 *
 * Intended for use with element query handlers to respond to #GST_QUERY_CONTEXT
 * for a #GstVulkanInstance.
 *
 * Returns: whether @query was responded to with @instance
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_handle_context_query (GstElement * element,
    GstQuery * query, GstVulkanInstance * instance)
{
  gboolean res = FALSE;
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (query != NULL, FALSE);
  g_return_val_if_fail (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT, FALSE);

  if (!instance)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);

  if (g_strcmp0 (context_type, GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR) == 0) {
    gst_query_parse_context (query, &old_context);

    if (old_context)
      context = gst_context_copy (old_context);
    else
      context = gst_context_new (GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR, TRUE);

    gst_context_set_vulkan_instance (context, instance);
    gst_query_set_context (query, context);
    gst_context_unref (context);

    res = instance != NULL;
  }

  return res;
}

/**
 * gst_vulkan_instance_run_context_query:
 * @element: a #GstElement
 * @instance: (inout): a #GstVulkanInstance
 *
 * Attempt to retrieve a #GstVulkanInstance using #GST_QUERY_CONTEXT from the
 * surrounding elements of @element.
 *
 * Returns: whether @instance contains a valid #GstVulkanInstance
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_run_context_query (GstElement * element,
    GstVulkanInstance ** instance)
{
  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (instance != NULL, FALSE);

  _init_debug ();

  if (*instance && GST_IS_VULKAN_INSTANCE (*instance))
    return TRUE;

  gst_vulkan_global_context_query (element,
      GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR);

  GST_DEBUG_OBJECT (element, "found instance %p", *instance);

  if (*instance)
    return TRUE;

  return FALSE;
}

/**
 * gst_vulkan_instance_check_version:
 * @instance: a #GstVulkanInstance
 * @major: major version
 * @minor: minor version
 * @patch: patch version
 *
 * Check if the configured vulkan instance supports the specified version.
 * Will not work prior to opening the instance with gst_vulkan_instance_open().
 * If a specific version is requested, the @patch level is ignored.
 *
 * Returns: whether @instance is at least the requested version.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_instance_check_version (GstVulkanInstance * instance,
    guint major, guint minor, guint patch)
{
  GstVulkanInstancePrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_INSTANCE (instance), FALSE);

  priv = GET_PRIV (instance);

  return (priv->requested_api_major == 0
      && VK_MAKE_VERSION (major, minor, patch) <= priv->supported_instance_api)
      || (priv->requested_api_major >= 0 && (major < priv->requested_api_major
          || (major == priv->requested_api_major
              && minor <= priv->requested_api_minor)));
}

/**
 * gst_vulkan_instance_get_version:
 * @instance: a #GstVulkanInstance
 * @major: (out): major version
 * @minor: (out): minor version
 * @patch: (out): patch version
 *
 * Retrieve the vulkan instance configured version.  Only returns the supported
 * API version by the instance without taking into account the requested API
 * version.  This means gst_vulkan_instance_check_version() will return
 * different values if a specific version has been requested (which is the
 * default) than a version check that is performed manually by retrieving the
 * version with this function.
 *
 * Since: 1.18
 */
void
gst_vulkan_instance_get_version (GstVulkanInstance * instance,
    guint * major, guint * minor, guint * patch)
{
  GstVulkanInstancePrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_INSTANCE (instance));

  priv = GET_PRIV (instance);

  GST_OBJECT_LOCK (instance);
  if (!priv->supported_instance_api)
    gst_vulkan_get_supported_api_version_unlocked (instance);

  if (major)
    *major = VK_VERSION_MAJOR (priv->supported_instance_api);
  if (minor)
    *minor = VK_VERSION_MINOR (priv->supported_instance_api);
  if (patch)
    *patch = VK_VERSION_PATCH (priv->supported_instance_api);
  GST_OBJECT_UNLOCK (instance);
}
