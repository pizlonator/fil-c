/* GStreamer
 * Copyright (C) 2018 Nirbheek Chauhan <nirbheek@centricular.com>
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstwasapidevice.h"

GST_DEBUG_CATEGORY_EXTERN (gst_wasapi_debug);
#define GST_CAT_DEFAULT gst_wasapi_debug

G_DEFINE_TYPE (GstWasapiDeviceProvider, gst_wasapi_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static void gst_wasapi_device_provider_finalize (GObject * object);
static GList *gst_wasapi_device_provider_probe (GstDeviceProvider * provider);
static gboolean gst_wasapi_device_provider_start (GstDeviceProvider * provider);
static void gst_wasapi_device_provider_stop (GstDeviceProvider * provider);

static HRESULT
gst_wasapi_device_provider_device_added (GstMMDeviceEnumerator * enumerator,
    LPCWSTR device_id, gpointer user_data);
static HRESULT
gst_wasapi_device_provider_device_removed (GstMMDeviceEnumerator * enumerator,
    LPCWSTR device_id, gpointer user_data);
static HRESULT
gst_wasapi_device_provider_default_device_changed (GstMMDeviceEnumerator *
    enumerator, EDataFlow flow, ERole role, LPCWSTR device_id,
    gpointer user_data);

static void
gst_wasapi_device_provider_class_init (GstWasapiDeviceProviderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  gobject_class->finalize = gst_wasapi_device_provider_finalize;

  dm_class->probe = gst_wasapi_device_provider_probe;
  dm_class->start = gst_wasapi_device_provider_start;
  dm_class->stop = gst_wasapi_device_provider_stop;

  gst_device_provider_class_set_static_metadata (dm_class,
      "WASAPI (Windows Audio Session API) Device Provider",
      "Source/Sink/Audio", "List WASAPI source and sink devices",
      "Nirbheek Chauhan <nirbheek@centricular.com>");
}

static void
gst_wasapi_device_provider_init (GstWasapiDeviceProvider * self)
{
  self->enumerator = gst_mm_device_enumerator_new ();
}

static gboolean
gst_wasapi_device_provider_start (GstDeviceProvider * provider)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (provider);
  GstMMNotificationClientCallbacks callbacks = { NULL, };
  GList *devices = NULL;
  GList *iter;

  if (!self->enumerator) {
    GST_WARNING_OBJECT (self, "Enumerator wasn't configured");
    return FALSE;
  }

  callbacks.device_added = gst_wasapi_device_provider_device_added;
  callbacks.device_removed = gst_wasapi_device_provider_device_removed;
  callbacks.default_device_changed =
      gst_wasapi_device_provider_default_device_changed;

  if (!gst_mm_device_enumerator_set_notification_callback (self->enumerator,
          &callbacks, self)) {
    GST_WARNING_OBJECT (self, "Failed to set callbacks");
    return FALSE;
  }

  /* baseclass will not call probe() once it's started, but we can get
   * notification only add/remove or change case. To this manually */
  devices = gst_wasapi_device_provider_probe (provider);
  if (devices) {
    for (iter = devices; iter; iter = g_list_next (iter)) {
      gst_device_provider_device_add (provider, GST_DEVICE (iter->data));
    }

    g_list_free (devices);
  }

  return TRUE;
}

static void
gst_wasapi_device_provider_stop (GstDeviceProvider * provider)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (provider);

  if (self->enumerator) {
    gst_mm_device_enumerator_set_notification_callback (self->enumerator,
        NULL, NULL);
  }
}

static void
gst_wasapi_device_provider_finalize (GObject * object)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (object);

  gst_clear_object (&self->enumerator);

  G_OBJECT_CLASS (gst_wasapi_device_provider_parent_class)->finalize (object);
}

static GList *
gst_wasapi_device_provider_probe (GstDeviceProvider * provider)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (provider);
  GList *devices = NULL;

  if (!gst_wasapi_util_get_devices (self->enumerator, TRUE, &devices))
    GST_ERROR_OBJECT (self, "Failed to enumerate devices");

  return devices;
}

static gboolean
gst_wasapi_device_is_in_list (GList * list, GstDevice * device)
{
  GList *iter;
  GstStructure *s;
  const gchar *device_id;
  gboolean found = FALSE;

  s = gst_device_get_properties (device);
  g_assert (s);

  device_id = gst_structure_get_string (s, "device.strid");
  g_assert (device_id);

  for (iter = list; iter; iter = g_list_next (iter)) {
    GstStructure *other_s;
    const gchar *other_id;

    other_s = gst_device_get_properties (GST_DEVICE (iter->data));
    g_assert (other_s);

    other_id = gst_structure_get_string (other_s, "device.strid");
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
gst_wasapi_device_provider_update_devices (GstWasapiDeviceProvider * self)
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

  new_devices = gst_wasapi_device_provider_probe (provider);

  /* Ownership of GstDevice for gst_device_provider_device_add()
   * and gst_device_provider_device_remove() is a bit complicated.
   * Remove floating reference here for things to be clear */
  for (iter = new_devices; iter; iter = g_list_next (iter))
    gst_object_ref_sink (iter->data);

  /* Check newly added devices */
  for (iter = new_devices; iter; iter = g_list_next (iter)) {
    if (!gst_wasapi_device_is_in_list (prev_devices, GST_DEVICE (iter->data))) {
      to_add = g_list_prepend (to_add, gst_object_ref (iter->data));
    }
  }

  /* Check removed device */
  for (iter = prev_devices; iter; iter = g_list_next (iter)) {
    if (!gst_wasapi_device_is_in_list (new_devices, GST_DEVICE (iter->data))) {
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

static HRESULT
gst_wasapi_device_provider_device_added (GstMMDeviceEnumerator * enumerator,
    LPCWSTR device_id, gpointer user_data)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (user_data);

  gst_wasapi_device_provider_update_devices (self);

  return S_OK;
}

static HRESULT
gst_wasapi_device_provider_device_removed (GstMMDeviceEnumerator * enumerator,
    LPCWSTR device_id, gpointer user_data)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (user_data);

  gst_wasapi_device_provider_update_devices (self);

  return S_OK;
}

static HRESULT
gst_wasapi_device_provider_default_device_changed (GstMMDeviceEnumerator *
    enumerator, EDataFlow flow, ERole role, LPCWSTR device_id,
    gpointer user_data)
{
  GstWasapiDeviceProvider *self = GST_WASAPI_DEVICE_PROVIDER (user_data);

  gst_wasapi_device_provider_update_devices (self);

  return S_OK;
}

/* GstWasapiDevice begins */

enum
{
  PROP_DEVICE_STRID = 1,
};

G_DEFINE_TYPE (GstWasapiDevice, gst_wasapi_device, GST_TYPE_DEVICE);

static void gst_wasapi_device_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_wasapi_device_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_wasapi_device_finalize (GObject * object);
static GstElement *gst_wasapi_device_create_element (GstDevice * device,
    const gchar * name);

static void
gst_wasapi_device_class_init (GstWasapiDeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_wasapi_device_create_element;

  object_class->get_property = gst_wasapi_device_get_property;
  object_class->set_property = gst_wasapi_device_set_property;
  object_class->finalize = gst_wasapi_device_finalize;

  g_object_class_install_property (object_class, PROP_DEVICE_STRID,
      g_param_spec_string ("device", "Device string ID",
          "Device strId", NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_wasapi_device_init (GstWasapiDevice * device)
{
}

static void
gst_wasapi_device_finalize (GObject * object)
{
  GstWasapiDevice *device = GST_WASAPI_DEVICE (object);

  g_free (device->strid);

  G_OBJECT_CLASS (gst_wasapi_device_parent_class)->finalize (object);
}

static GstElement *
gst_wasapi_device_create_element (GstDevice * device, const gchar * name)
{
  GstWasapiDevice *wasapi_dev = GST_WASAPI_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (wasapi_dev->element, name);

  g_object_set (elem, "device", wasapi_dev->strid, NULL);

  return elem;
}

static void
gst_wasapi_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWasapiDevice *device = GST_WASAPI_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE_STRID:
      g_value_set_string (value, device->strid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wasapi_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWasapiDevice *device = GST_WASAPI_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE_STRID:
      device->strid = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
