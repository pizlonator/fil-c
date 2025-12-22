/* GStreamer
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

#include "gstmfconfig.h"

#include "gstmfvideosrc.h"
#include "gstmfutils.h"
#include "gstmfsourceobject.h"

#include "gstmfdevice.h"

#if GST_MF_WINAPI_DESKTOP
#include "gstwin32devicewatcher.h"
#include "gstmfcapturedshow.h"

#ifndef INITGUID
#include <initguid.h>
#endif

#include <dbt.h>
DEFINE_GUID (GST_KSCATEGORY_CAPTURE, 0x65E8773DL, 0x8F56,
    0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
#endif

#if GST_MF_WINAPI_APP
#include <gst/winrt/gstwinrt.h>
/* *INDENT-OFF* */
using namespace ABI::Windows::Devices::Enumeration;
/* *INDENT-ON* */
#endif

GST_DEBUG_CATEGORY_EXTERN (gst_mf_debug);
#define GST_CAT_DEFAULT gst_mf_debug

enum
{
  PROP_0,
  PROP_DEVICE_PATH,
};

struct _GstMFDevice
{
  GstDevice parent;

  gchar *device_path;
};

G_DEFINE_TYPE (GstMFDevice, gst_mf_device, GST_TYPE_DEVICE);

static void gst_mf_device_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_mf_device_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mf_device_finalize (GObject * object);
static GstElement *gst_mf_device_create_element (GstDevice * device,
    const gchar * name);

static void
gst_mf_device_class_init (GstMFDeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);

  dev_class->create_element = gst_mf_device_create_element;

  gobject_class->get_property = gst_mf_device_get_property;
  gobject_class->set_property = gst_mf_device_set_property;
  gobject_class->finalize = gst_mf_device_finalize;

  g_object_class_install_property (gobject_class, PROP_DEVICE_PATH,
      g_param_spec_string ("device-path", "Device Path",
          "The device path", nullptr,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
}

static void
gst_mf_device_init (GstMFDevice * self)
{
}

static void
gst_mf_device_finalize (GObject * object)
{
  GstMFDevice *self = GST_MF_DEVICE (object);

  g_free (self->device_path);

  G_OBJECT_CLASS (gst_mf_device_parent_class)->finalize (object);
}

static GstElement *
gst_mf_device_create_element (GstDevice * device, const gchar * name)
{
  GstMFDevice *self = GST_MF_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make ("mfvideosrc", name);

  g_object_set (elem, "device-path", self->device_path, nullptr);

  return elem;
}

static void
gst_mf_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFDevice *self = GST_MF_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      g_value_set_string (value, self->device_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFDevice *self = GST_MF_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      self->device_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

struct _GstMFDeviceProvider
{
  GstDeviceProvider parent;

  GstObject *watcher;

  GMutex lock;
  GCond cond;

  gboolean enum_completed;
};

G_DEFINE_TYPE (GstMFDeviceProvider, gst_mf_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static void gst_mf_device_provider_dispose (GObject * object);
static void gst_mf_device_provider_finalize (GObject * object);

static GList *gst_mf_device_provider_probe (GstDeviceProvider * provider);
static gboolean gst_mf_device_provider_start (GstDeviceProvider * provider);
static void gst_mf_device_provider_stop (GstDeviceProvider * provider);

#if GST_MF_WINAPI_DESKTOP
static gboolean gst_mf_device_provider_start_win32 (GstDeviceProvider * self);
static void gst_mf_device_provider_device_changed (GstWin32DeviceWatcher *
    watcher, WPARAM wparam, LPARAM lparam, gpointer user_data);
#endif

#if GST_MF_WINAPI_APP
static gboolean gst_mf_device_provider_start_winrt (GstDeviceProvider * self);
static void
gst_mf_device_provider_device_added (GstWinRTDeviceWatcher * watcher,
    IDeviceInformation * info, gpointer user_data);
static void
gst_mf_device_provider_device_updated (GstWinRTDeviceWatcher * watcher,
    IDeviceInformationUpdate * info_update, gpointer user_data);
static void gst_mf_device_provider_device_removed (GstWinRTDeviceWatcher *
    watcher, IDeviceInformationUpdate * info_update, gpointer user_data);
static void
gst_mf_device_provider_device_enum_completed (GstWinRTDeviceWatcher *
    watcher, gpointer user_data);
#endif

static void
gst_mf_device_provider_on_device_updated (GstMFDeviceProvider * self);

static void
gst_mf_device_provider_class_init (GstMFDeviceProviderClass * klass)
{
  GstDeviceProviderClass *provider_class = GST_DEVICE_PROVIDER_CLASS (klass);

  provider_class->probe = GST_DEBUG_FUNCPTR (gst_mf_device_provider_probe);
  provider_class->start = GST_DEBUG_FUNCPTR (gst_mf_device_provider_start);
  provider_class->stop = GST_DEBUG_FUNCPTR (gst_mf_device_provider_stop);

  gst_device_provider_class_set_static_metadata (provider_class,
      "Media Foundation Device Provider",
      "Source/Video", "List Media Foundation source devices",
      "Seungha Yang <seungha@centricular.com>");
}

static void
gst_mf_device_provider_init (GstMFDeviceProvider * self)
{
#if GST_MF_WINAPI_DESKTOP
  GstWin32DeviceWatcherCallbacks win32_callbacks;

  win32_callbacks.device_changed = gst_mf_device_provider_device_changed;
  self->watcher = (GstObject *)
      gst_win32_device_watcher_new (DBT_DEVTYP_DEVICEINTERFACE,
      &GST_KSCATEGORY_CAPTURE, &win32_callbacks, self);
#endif
#if GST_MF_WINAPI_APP
  if (!self->watcher) {
    GstWinRTDeviceWatcherCallbacks winrt_callbacks;
    winrt_callbacks.added = gst_mf_device_provider_device_added;
    winrt_callbacks.updated = gst_mf_device_provider_device_updated;
    winrt_callbacks.removed = gst_mf_device_provider_device_removed;
    winrt_callbacks.enumeration_completed =
        gst_mf_device_provider_device_enum_completed;

    self->watcher = (GstObject *)
        gst_winrt_device_watcher_new (GST_WINRT_DEVICE_CLASS_VIDEO_CAPTURE,
        &winrt_callbacks, self);
  }
#endif

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);
}

static void
gst_mf_device_provider_dispose (GObject * object)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (object);

  gst_clear_object (&self->watcher);

  G_OBJECT_CLASS (gst_mf_device_provider_parent_class)->dispose (object);
}

static void
gst_mf_device_provider_finalize (GObject * object)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (object);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (gst_mf_device_provider_parent_class)->finalize (object);
}

static void
gst_mf_device_provider_probe_internal (GstDeviceProvider * provider,
    gboolean try_dshow, GList ** list)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (provider);
  gint i;

  for (i = 0;; i++) {
    GstMFSourceObject *obj = nullptr;
    GstDevice *device;
    GstStructure *props = nullptr;
    GstCaps *caps = nullptr;
    gchar *device_name = nullptr;
    gchar *device_path = nullptr;
    GstMFSourceResult ret = GST_MF_DEVICE_NOT_FOUND;

#if GST_MF_WINAPI_DESKTOP
    if (try_dshow) {
      ret = gst_mf_capture_dshow_enumerate (i, &obj);
    } else {
      ret = gst_mf_source_object_enumerate (i, &obj);
    }
#else
    ret = gst_mf_source_object_enumerate (i, &obj);
#endif
    if (ret == GST_MF_DEVICE_NOT_FOUND)
      break;
    else if (ret == GST_MF_ACTIVATION_FAILED)
      continue;

    g_assert (ret == GST_MF_OK);

    caps = gst_mf_source_object_get_caps (obj);
    if (!caps) {
      GST_WARNING_OBJECT (self, "Empty caps for device index %d", i);
      goto next;
    }

    g_object_get (obj,
        "device-path", &device_path, "device-name", &device_name, nullptr);

    if (!device_path) {
      GST_WARNING_OBJECT (self, "Device path is unavailable");
      goto next;
    }

    if (!device_name) {
      GST_WARNING_OBJECT (self, "Device name is unavailable");
      goto next;
    }

    props = gst_structure_new ("mf-proplist",
        "device.api", G_TYPE_STRING, "mediafoundation",
        "device.path", G_TYPE_STRING, device_path,
        "device.name", G_TYPE_STRING, device_name, nullptr);

    device = (GstDevice *) g_object_new (GST_TYPE_MF_DEVICE,
        "device-path", device_path,
        "display-name", device_name, "caps", caps,
        "device-class", "Source/Video", "properties", props, nullptr);

    *list = g_list_append (*list, device);

  next:
    if (caps)
      gst_caps_unref (caps);
    if (props)
      gst_structure_free (props);
    g_free (device_path);
    g_free (device_name);
    gst_object_unref (obj);
  }
}

static GList *
gst_mf_device_provider_probe (GstDeviceProvider * provider)
{
  GList *list = nullptr;

  gst_mf_device_provider_probe_internal (provider, FALSE, &list);
#if GST_MF_WINAPI_DESKTOP
  gst_mf_device_provider_probe_internal (provider, TRUE, &list);
#endif

  return list;
}

#if GST_MF_WINAPI_DESKTOP
static gboolean
gst_mf_device_provider_start_win32 (GstDeviceProvider * provider)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (provider);
  GstWin32DeviceWatcher *watcher;
  GList *devices = nullptr;
  GList *iter;

  if (!GST_IS_WIN32_DEVICE_WATCHER (self->watcher))
    return FALSE;

  GST_DEBUG_OBJECT (self, "Starting Win32 watcher");

  watcher = GST_WIN32_DEVICE_WATCHER (self->watcher);

  devices = gst_mf_device_provider_probe (provider);
  if (devices) {
    for (iter = devices; iter; iter = g_list_next (iter)) {
      gst_device_provider_device_add (provider, GST_DEVICE (iter->data));
    }

    g_list_free (devices);
  }

  return gst_win32_device_watcher_start (watcher);
}
#endif

#if GST_MF_WINAPI_APP
static gboolean
gst_mf_device_provider_start_winrt (GstDeviceProvider * provider)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (provider);
  GstWinRTDeviceWatcher *watcher;
  GList *devices = nullptr;
  GList *iter;

  if (!GST_IS_WINRT_DEVICE_WATCHER (self->watcher))
    return FALSE;

  GST_DEBUG_OBJECT (self, "Starting WinRT watcher");
  watcher = GST_WINRT_DEVICE_WATCHER (self->watcher);

  self->enum_completed = FALSE;

  if (!gst_winrt_device_watcher_start (watcher))
    return FALSE;

  /* Wait for initial enumeration to be completed */
  g_mutex_lock (&self->lock);
  while (!self->enum_completed)
    g_cond_wait (&self->cond, &self->lock);

  devices = gst_mf_device_provider_probe (provider);
  if (devices) {
    for (iter = devices; iter; iter = g_list_next (iter)) {
      gst_device_provider_device_add (provider, GST_DEVICE (iter->data));
    }

    g_list_free (devices);
  }
  g_mutex_unlock (&self->lock);

  return TRUE;
}
#endif

static gboolean
gst_mf_device_provider_start (GstDeviceProvider * provider)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (provider);
  gboolean ret = FALSE;

  if (!self->watcher) {
    GST_ERROR_OBJECT (self, "DeviceWatcher object wasn't configured");
    return FALSE;
  }
#if GST_MF_WINAPI_DESKTOP
  ret = gst_mf_device_provider_start_win32 (provider);
#endif

#if GST_MF_WINAPI_APP
  if (!ret)
    ret = gst_mf_device_provider_start_winrt (provider);
#endif

  return ret;
}

static void
gst_mf_device_provider_stop (GstDeviceProvider * provider)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (provider);

  if (self->watcher) {
#if GST_MF_WINAPI_DESKTOP
    if (GST_IS_WIN32_DEVICE_WATCHER (self->watcher)) {
      gst_win32_device_watcher_stop (GST_WIN32_DEVICE_WATCHER (self->watcher));
    }
#endif
#if GST_MF_WINAPI_APP
    if (GST_IS_WINRT_DEVICE_WATCHER (self->watcher)) {
      gst_winrt_device_watcher_stop (GST_WINRT_DEVICE_WATCHER (self->watcher));
    }
#endif
  }
}

static gboolean
gst_mf_device_is_in_list (GList * list, GstDevice * device)
{
  GList *iter;
  GstStructure *s;
  const gchar *device_id;
  gboolean found = FALSE;

  s = gst_device_get_properties (device);
  g_assert (s);

  device_id = gst_structure_get_string (s, "device.path");
  g_assert (device_id);

  for (iter = list; iter; iter = g_list_next (iter)) {
    GstStructure *other_s;
    const gchar *other_id;

    other_s = gst_device_get_properties (GST_DEVICE (iter->data));
    g_assert (other_s);

    other_id = gst_structure_get_string (other_s, "device.path");
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
gst_mf_device_provider_update_devices (GstMFDeviceProvider * self)
{
  GstDeviceProvider *provider = GST_DEVICE_PROVIDER_CAST (self);
  GList *prev_devices = nullptr;
  GList *new_devices = nullptr;
  GList *to_add = nullptr;
  GList *to_remove = nullptr;
  GList *iter;

  GST_OBJECT_LOCK (self);
  prev_devices = g_list_copy_deep (provider->devices,
      (GCopyFunc) gst_object_ref, nullptr);
  GST_OBJECT_UNLOCK (self);

  new_devices = gst_mf_device_provider_probe (provider);

  /* Ownership of GstDevice for gst_device_provider_device_add()
   * and gst_device_provider_device_remove() is a bit complicated.
   * Remove floating reference here for things to be clear */
  for (iter = new_devices; iter; iter = g_list_next (iter))
    gst_object_ref_sink (iter->data);

  /* Check newly added devices */
  for (iter = new_devices; iter; iter = g_list_next (iter)) {
    if (!gst_mf_device_is_in_list (prev_devices, GST_DEVICE (iter->data))) {
      to_add = g_list_prepend (to_add, gst_object_ref (iter->data));
    }
  }

  /* Check removed device */
  for (iter = prev_devices; iter; iter = g_list_next (iter)) {
    if (!gst_mf_device_is_in_list (new_devices, GST_DEVICE (iter->data))) {
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

#if GST_MF_WINAPI_DESKTOP
static void
gst_mf_device_provider_device_changed (GstWin32DeviceWatcher * watcher,
    WPARAM wparam, LPARAM lparam, gpointer user_data)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (user_data);

  if (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE) {
    gst_mf_device_provider_update_devices (self);
  }
}
#endif

#if GST_MF_WINAPI_APP
static void
gst_mf_device_provider_device_added (GstWinRTDeviceWatcher * watcher,
    IDeviceInformation * info, gpointer user_data)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (user_data);

  if (self->enum_completed)
    gst_mf_device_provider_update_devices (self);
}

static void
gst_mf_device_provider_device_removed (GstWinRTDeviceWatcher * watcher,
    IDeviceInformationUpdate * info_update, gpointer user_data)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (user_data);

  if (self->enum_completed)
    gst_mf_device_provider_update_devices (self);
}


static void
gst_mf_device_provider_device_updated (GstWinRTDeviceWatcher * watcher,
    IDeviceInformationUpdate * info_update, gpointer user_data)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (user_data);

  gst_mf_device_provider_update_devices (self);
}

static void
gst_mf_device_provider_device_enum_completed (GstWinRTDeviceWatcher *
    watcher, gpointer user_data)
{
  GstMFDeviceProvider *self = GST_MF_DEVICE_PROVIDER (user_data);

  g_mutex_lock (&self->lock);
  GST_DEBUG_OBJECT (self, "Enumeration completed");
  self->enum_completed = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);
}
#endif
