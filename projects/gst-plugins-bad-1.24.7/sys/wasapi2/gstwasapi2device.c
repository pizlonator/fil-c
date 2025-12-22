/* GStreamer
 * Copyright (C) 2018 Nirbheek Chauhan <nirbheek@centricular.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include "gstwasapi2device.h"
#include "gstwasapi2client.h"
#include "gstwasapi2util.h"
#include <gst/winrt/gstwinrt.h>

GST_DEBUG_CATEGORY_EXTERN (gst_wasapi2_debug);
#define GST_CAT_DEFAULT gst_wasapi2_debug

enum
{
  PROP_0,
  PROP_DEVICE,
};

struct _GstWasapi2Device
{
  GstDevice parent;

  gchar *device_id;
  const gchar *factory_name;
  GstWasapi2ClientDeviceClass device_class;
};

G_DEFINE_TYPE (GstWasapi2Device, gst_wasapi2_device, GST_TYPE_DEVICE);

static void gst_wasapi2_device_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_wasapi2_device_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_wasapi2_device_finalize (GObject * object);
static GstElement *gst_wasapi2_device_create_element (GstDevice * device,
    const gchar * name);

static void
gst_wasapi2_device_class_init (GstWasapi2DeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);

  dev_class->create_element = gst_wasapi2_device_create_element;

  gobject_class->get_property = gst_wasapi2_device_get_property;
  gobject_class->set_property = gst_wasapi2_device_set_property;
  gobject_class->finalize = gst_wasapi2_device_finalize;

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Audio device ID as provided by "
          "Windows.Devices.Enumeration.DeviceInformation.Id", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gst_wasapi2_device_init (GstWasapi2Device * self)
{
}

static void
gst_wasapi2_device_finalize (GObject * object)
{
  GstWasapi2Device *self = GST_WASAPI2_DEVICE (object);

  g_free (self->device_id);

  G_OBJECT_CLASS (gst_wasapi2_device_parent_class)->finalize (object);
}

static GstElement *
gst_wasapi2_device_create_element (GstDevice * device, const gchar * name)
{
  GstWasapi2Device *self = GST_WASAPI2_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (self->factory_name, name);

  g_object_set (elem, "device", self->device_id, NULL);

  if (self->device_class == GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE)
    g_object_set (elem, "loopback", TRUE, NULL);

  return elem;
}

static void
gst_wasapi2_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapi2Device *self = GST_WASAPI2_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, self->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wasapi2_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapi2Device *self = GST_WASAPI2_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE:
      self->device_id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

typedef struct _GstWasapi2DeviceProvider
{
  GstDeviceProvider parent;

  GstWinRTDeviceWatcher *watcher;

  GMutex lock;
  GCond cond;

  gboolean enum_completed;
} GstWasapi2DeviceProvider;

typedef struct _GstWasapi2DeviceProviderClass
{
  GstDeviceProviderClass parent_class;

  GstWinRTDeviceClass winrt_device_class;
} GstWasapi2DeviceProviderClass;

static GstDeviceProviderClass *parent_class = NULL;

#define GST_WASAPI2_DEVICE_PROVIDER(object) \
    ((GstWasapi2DeviceProvider *) (object))
#define GST_WASAPI2_DEVICE_PROVIDER_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstWasapi2DeviceProviderClass))

static void gst_wasapi2_device_provider_dispose (GObject * object);
static void gst_wasapi2_device_provider_finalize (GObject * object);

static GList *gst_wasapi2_device_provider_probe (GstDeviceProvider * provider);
static gboolean
gst_wasapi2_device_provider_start (GstDeviceProvider * provider);
static void gst_wasapi2_device_provider_stop (GstDeviceProvider * provider);

static void
gst_wasapi2_device_provider_device_added (GstWinRTDeviceWatcher * watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformation * info,
    gpointer user_data);
static void
gst_wasapi2_device_provider_device_updated (GstWinRTDeviceWatcher * watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate *
    info_update, gpointer user_data);
static void gst_wasapi2_device_provider_device_removed (GstWinRTDeviceWatcher *
    watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate *
    info_update, gpointer user_data);
static void
gst_wasapi2_device_provider_device_enum_completed (GstWinRTDeviceWatcher *
    watcher, gpointer user_data);

static void
gst_wasapi2_device_provider_class_init (GstWasapi2DeviceProviderClass * klass,
    gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceProviderClass *provider_class = GST_DEVICE_PROVIDER_CLASS (klass);

  gobject_class->dispose = gst_wasapi2_device_provider_dispose;
  gobject_class->finalize = gst_wasapi2_device_provider_finalize;

  provider_class->probe = GST_DEBUG_FUNCPTR (gst_wasapi2_device_provider_probe);
  provider_class->start = GST_DEBUG_FUNCPTR (gst_wasapi2_device_provider_start);
  provider_class->stop = GST_DEBUG_FUNCPTR (gst_wasapi2_device_provider_stop);

  parent_class = (GstDeviceProviderClass *) g_type_class_peek_parent (klass);

  klass->winrt_device_class = (GstWinRTDeviceClass) GPOINTER_TO_INT (data);
  if (klass->winrt_device_class == GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE) {
    gst_device_provider_class_set_static_metadata (provider_class,
        "WASAPI (Windows Audio Session API) Capture Device Provider",
        "Source/Audio", "List WASAPI source devices",
        "Nirbheek Chauhan <nirbheek@centricular.com>, "
        "Seungha Yang <seungha@centricular.com>");
  } else {
    gst_device_provider_class_set_static_metadata (provider_class,
        "WASAPI (Windows Audio Session API) Render and Loopback Capture Device Provider",
        "Source/Sink/Audio", "List WASAPI loop back source and sink devices",
        "Nirbheek Chauhan <nirbheek@centricular.com>, "
        "Seungha Yang <seungha@centricular.com>");
  }
}

static void
gst_wasapi2_device_provider_init (GstWasapi2DeviceProvider * self)
{
  GstWasapi2DeviceProviderClass *klass =
      GST_WASAPI2_DEVICE_PROVIDER_GET_CLASS (self);
  GstWinRTDeviceWatcherCallbacks callbacks;

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  callbacks.added = gst_wasapi2_device_provider_device_added;
  callbacks.updated = gst_wasapi2_device_provider_device_updated;
  callbacks.removed = gst_wasapi2_device_provider_device_removed;
  callbacks.enumeration_completed =
      gst_wasapi2_device_provider_device_enum_completed;

  self->watcher =
      gst_winrt_device_watcher_new (klass->winrt_device_class,
      &callbacks, self);
}

static void
gst_wasapi2_device_provider_dispose (GObject * object)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (object);

  gst_clear_object (&self->watcher);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_wasapi2_device_provider_finalize (GObject * object)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (object);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wasapi2_device_provider_probe_internal (GstWasapi2DeviceProvider * self,
    GstWasapi2ClientDeviceClass client_class, GList ** devices)
{
  gint i;
  const gchar *device_class, *factory_name;

  if (client_class == GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER) {
    device_class = "Audio/Sink";
    factory_name = "wasapi2sink";
  } else {
    device_class = "Audio/Source";
    factory_name = "wasapi2src";
  }

  for (i = 0;; i++) {
    GstWasapi2Client *client = NULL;
    GstDevice *device;
    GstStructure *props = NULL;
    GstCaps *caps = NULL;
    gchar *device_id = NULL;
    gchar *device_name = NULL;
    GstWasapi2Result result;

    result = gst_wasapi2_client_enumerate (client_class, i, &client);
    if (result == GST_WASAPI2_DEVICE_NOT_FOUND)
      return;
    else if (result == GST_WASAPI2_ACTIVATION_FAILED)
      continue;

    g_assert (client);

    caps = gst_wasapi2_client_get_caps (client);
    if (!caps) {
      GST_WARNING_OBJECT (self, "Couldn't get caps from client %d", i);
      /* this might be a case where device activation is not finished yet */
      caps = gst_caps_from_string (GST_WASAPI2_STATIC_CAPS);
    }

    g_object_get (client,
        "device", &device_id, "device-name", &device_name, NULL);
    if (!device_id) {
      GST_WARNING_OBJECT (self, "Couldn't get device name from client %d", i);
      goto next;
    }

    if (!device_name) {
      GST_WARNING_OBJECT (self, "Couldn't get device name from client %d", i);
      goto next;
    }

    props = gst_structure_new ("wasapi2-proplist",
        "device.api", G_TYPE_STRING, "wasapi2",
        "device.id", G_TYPE_STRING, device_id,
        "device.default", G_TYPE_BOOLEAN, i == 0,
        "wasapi2.device.description", G_TYPE_STRING, device_name, NULL);
    switch (client_class) {
      case GST_WASAPI2_CLIENT_DEVICE_CLASS_CAPTURE:
        gst_structure_set (props,
            "wasapi2.device.loopback", G_TYPE_BOOLEAN, FALSE, NULL);
        break;
      case GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE:
        gst_structure_set (props,
            "wasapi2.device.loopback", G_TYPE_BOOLEAN, TRUE, NULL);
        break;
      default:
        break;
    }

    device = g_object_new (GST_TYPE_WASAPI2_DEVICE, "device", device_id,
        "display-name", device_name, "caps", caps,
        "device-class", device_class, "properties", props, NULL);
    GST_WASAPI2_DEVICE (device)->factory_name = factory_name;
    GST_WASAPI2_DEVICE (device)->device_class = client_class;

    *devices = g_list_append (*devices, device);

  next:
    gst_clear_object (&client);
    gst_clear_caps (&caps);
    g_free (device_id);
    g_free (device_name);
    if (props)
      gst_structure_free (props);
  }

  return;
}

static GList *
gst_wasapi2_device_provider_probe (GstDeviceProvider * provider)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (provider);
  GstWasapi2DeviceProviderClass *klass =
      GST_WASAPI2_DEVICE_PROVIDER_GET_CLASS (self);
  GList *devices = NULL;

  if (klass->winrt_device_class == GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE) {
    gst_wasapi2_device_provider_probe_internal (self,
        GST_WASAPI2_CLIENT_DEVICE_CLASS_CAPTURE, &devices);
  } else {
    gst_wasapi2_device_provider_probe_internal (self,
        GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE, &devices);
    gst_wasapi2_device_provider_probe_internal (self,
        GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER, &devices);
  }

  return devices;
}

static gboolean
gst_wasapi2_device_provider_start (GstDeviceProvider * provider)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (provider);
  GList *devices = NULL;
  GList *iter;

  if (!self->watcher) {
    GST_ERROR_OBJECT (self, "DeviceWatcher object wasn't configured");
    return FALSE;
  }

  self->enum_completed = FALSE;

  if (!gst_winrt_device_watcher_start (self->watcher))
    return FALSE;

  /* Wait for initial enumeration to be completed */
  g_mutex_lock (&self->lock);
  while (!self->enum_completed)
    g_cond_wait (&self->cond, &self->lock);

  devices = gst_wasapi2_device_provider_probe (provider);
  if (devices) {
    for (iter = devices; iter; iter = g_list_next (iter)) {
      gst_device_provider_device_add (provider, GST_DEVICE (iter->data));
    }

    g_list_free (devices);
  }
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static void
gst_wasapi2_device_provider_stop (GstDeviceProvider * provider)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (provider);

  if (self->watcher)
    gst_winrt_device_watcher_stop (self->watcher);
}

static gboolean
gst_wasapi2_device_is_in_list (GList * list, GstDevice * device)
{
  GList *iter;
  GstStructure *s;
  const gchar *device_id;
  gboolean found = FALSE;

  s = gst_device_get_properties (device);
  g_assert (s);

  device_id = gst_structure_get_string (s, "device.id");
  g_assert (device_id);

  for (iter = list; iter; iter = g_list_next (iter)) {
    GstStructure *other_s;
    const gchar *other_id;

    other_s = gst_device_get_properties (GST_DEVICE (iter->data));
    g_assert (other_s);

    other_id = gst_structure_get_string (other_s, "device.id");
    g_assert (other_id);

    if (g_ascii_strcasecmp (device_id, other_id) == 0) {
      found = TRUE;
    }

    gst_structure_free (other_s);
    if (found)
      break;
  }

  gst_structure_free (s);

  return found;
}

static void
gst_wasapi2_device_provider_update_devices (GstWasapi2DeviceProvider * self)
{
  GstDeviceProvider *provider = GST_DEVICE_PROVIDER_CAST (self);
  GList *prev_devices = NULL;
  GList *new_devices = NULL;
  GList *to_add = NULL;
  GList *to_remove = NULL;
  GList *iter;

  GST_OBJECT_LOCK (self);
  prev_devices = g_list_copy_deep (provider->devices,
      (GCopyFunc) gst_object_ref, NULL);
  GST_OBJECT_UNLOCK (self);

  new_devices = gst_wasapi2_device_provider_probe (provider);

  /* Ownership of GstDevice for gst_device_provider_device_add()
   * and gst_device_provider_device_remove() is a bit complicated.
   * Remove floating reference here for things to be clear */
  for (iter = new_devices; iter; iter = g_list_next (iter))
    gst_object_ref_sink (iter->data);

  /* Check newly added devices */
  for (iter = new_devices; iter; iter = g_list_next (iter)) {
    if (!gst_wasapi2_device_is_in_list (prev_devices, GST_DEVICE (iter->data))) {
      to_add = g_list_prepend (to_add, gst_object_ref (iter->data));
    }
  }

  /* Check removed device */
  for (iter = prev_devices; iter; iter = g_list_next (iter)) {
    if (!gst_wasapi2_device_is_in_list (new_devices, GST_DEVICE (iter->data))) {
      to_remove = g_list_prepend (to_remove, gst_object_ref (iter->data));
    }
  }

  for (iter = to_remove; iter; iter = g_list_next (iter))
    gst_device_provider_device_remove (provider, GST_DEVICE (iter->data));

  for (iter = to_add; iter; iter = g_list_next (iter))
    gst_device_provider_device_add (provider, GST_DEVICE (iter->data));

  if (prev_devices)
    g_list_free_full (prev_devices, (GDestroyNotify) gst_object_unref);

  if (to_add)
    g_list_free_full (to_add, (GDestroyNotify) gst_object_unref);

  if (to_remove)
    g_list_free_full (to_remove, (GDestroyNotify) gst_object_unref);
}

static void
gst_wasapi2_device_provider_device_added (GstWinRTDeviceWatcher * watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformation * info,
    gpointer user_data)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (user_data);

  if (self->enum_completed)
    gst_wasapi2_device_provider_update_devices (self);
}

static void
gst_wasapi2_device_provider_device_removed (GstWinRTDeviceWatcher * watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate *
    info_update, gpointer user_data)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (user_data);

  if (self->enum_completed)
    gst_wasapi2_device_provider_update_devices (self);
}

static void
gst_wasapi2_device_provider_device_updated (GstWinRTDeviceWatcher * watcher,
    __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate * info,
    gpointer user_data)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (user_data);

  gst_wasapi2_device_provider_update_devices (self);
}

static void
gst_wasapi2_device_provider_device_enum_completed (GstWinRTDeviceWatcher *
    watcher, gpointer user_data)
{
  GstWasapi2DeviceProvider *self = GST_WASAPI2_DEVICE_PROVIDER (user_data);

  g_mutex_lock (&self->lock);
  GST_DEBUG_OBJECT (self, "Enumeration completed");
  self->enum_completed = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);
}

static void
gst_wasapi2_device_provider_register_internal (GstPlugin * plugin,
    guint rank, GstWinRTDeviceClass winrt_device_class)
{
  GType type;
  const gchar *type_name = NULL;
  const gchar *feature_name = NULL;
  GTypeInfo type_info = {
    sizeof (GstWasapi2DeviceProviderClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_wasapi2_device_provider_class_init,
    NULL,
    NULL,
    sizeof (GstWasapi2DeviceProvider),
    0,
    (GInstanceInitFunc) gst_wasapi2_device_provider_init,
  };

  type_info.class_data = GINT_TO_POINTER (winrt_device_class);

  if (winrt_device_class == GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE) {
    type_name = "GstWasapi2CaputreDeviceProvider";
    feature_name = "wasapi2capturedeviceprovider";
  } else if (winrt_device_class == GST_WINRT_DEVICE_CLASS_AUDIO_RENDER) {
    type_name = "GstWasapi2RenderDeviceProvider";
    feature_name = "wasapi2renderdeviceprovider";
  } else {
    g_assert_not_reached ();
    return;
  }

  type = g_type_register_static (GST_TYPE_DEVICE_PROVIDER,
      type_name, &type_info, (GTypeFlags) 0);

  if (!gst_device_provider_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register provider '%s'", type_name);
}

void
gst_wasapi2_device_provider_register (GstPlugin * plugin, guint rank)
{
  gst_wasapi2_device_provider_register_internal (plugin, rank,
      GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE);
  gst_wasapi2_device_provider_register_internal (plugin, rank,
      GST_WINRT_DEVICE_CLASS_AUDIO_RENDER);
}
