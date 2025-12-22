/* GStreamer
 * Copyright (C) 2015 Руслан Ижбулатов <lrn1986@gmail.com>
 *
 * ksdeviceprovider.c: Kernel Streaming device probing and monitoring
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

#include "gstksvideosrc.h"
#include "ksdeviceprovider.h"

#include <string.h>

#include <dbt.h>                /* for DBT_* consts and [_]DEV_* structs */
#include <devguid.h>            /* for GUID_DEVCLASS_WCEUSBS */
#include <setupapi.h>           /* for DIGCF_ALLCLASSES */

#include <gst/gst.h>

#include "kshelpers.h"
#include "ksvideohelpers.h"


GST_DEBUG_CATEGORY_EXTERN (gst_ks_debug);
#define GST_CAT_DEFAULT gst_ks_debug


static GstDevice *gst_ks_device_new (guint id,
    const gchar * device_name, GstCaps * caps, const gchar * device_path,
    GstKsDeviceType type);

G_DEFINE_TYPE (GstKsDeviceProvider, gst_ks_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static GList *gst_ks_device_provider_probe (GstDeviceProvider * provider);
static gboolean gst_ks_device_provider_start (GstDeviceProvider * provider);
static void gst_ks_device_provider_stop (GstDeviceProvider * provider);

static void
gst_ks_device_provider_class_init (GstKsDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  dm_class->probe = gst_ks_device_provider_probe;
  dm_class->start = gst_ks_device_provider_start;
  dm_class->stop = gst_ks_device_provider_stop;

  gst_device_provider_class_set_static_metadata (dm_class,
      "KernelStreaming Device Provider", "Sink/Source/Audio/Video",
      "List and provide KernelStreaming source and sink devices",
      "Руслан Ижбулатов <lrn1986@gmail.com>");
}

static void
gst_ks_device_provider_init (GstKsDeviceProvider * self)
{
}

static GstDevice *
new_video_source (const KsDeviceEntry * info)
{
  GstCaps *caps;
  HANDLE filter_handle;
  GList *media_types;
  GList *cur;

  g_assert (info->path != NULL);

  caps = gst_caps_new_empty ();

  filter_handle = CreateFile (info->path,
      GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
  if (!ks_is_valid_handle (filter_handle))
    goto error;

  media_types = ks_video_probe_filter_for_caps (filter_handle);

  for (cur = media_types; cur != NULL; cur = cur->next) {
    KsVideoMediaType *media_type = cur->data;

    gst_caps_append (caps, gst_caps_copy (media_type->translated_caps));

    ks_video_media_type_free (media_type);
  }

  CloseHandle (filter_handle);
  g_list_free (media_types);

  return gst_ks_device_new (info->index, info->name,
      caps, info->path, GST_KS_DEVICE_TYPE_VIDEO_SOURCE);
error:
  gst_caps_unref (caps);
  return NULL;
}

static GList *
gst_ks_device_provider_probe (GstDeviceProvider * provider)
{
  /*GstKsDeviceProvider *self = GST_KS_DEVICE_PROVIDER (provider); */
  GList *devices, *cur;
  GList *result;

  result = NULL;

  devices = ks_enumerate_devices (&KSCATEGORY_VIDEO, &KSCATEGORY_CAPTURE);
  if (devices == NULL)
    return result;

  devices = ks_video_device_list_sort_cameras_first (devices);

  for (cur = devices; cur != NULL; cur = cur->next) {
    GstDevice *source;
    KsDeviceEntry *entry = cur->data;

    source = new_video_source (entry);
    if (source)
      result = g_list_prepend (result, gst_object_ref_sink (source));

    ks_device_entry_free (entry);
  }

  result = g_list_reverse (result);

  g_list_free (devices);

  return result;
}

static const gchar *
get_dev_type (DEV_BROADCAST_HDR * dev_msg_header)
{
  switch (dev_msg_header->dbch_devicetype) {
    case DBT_DEVTYP_DEVICEINTERFACE:
      return "Device interface class";
    case DBT_DEVTYP_HANDLE:
      return "Filesystem handle";
    case DBT_DEVTYP_OEM:
      return "OEM or IHV device type";
    case DBT_DEVTYP_PORT:
      return "Port device";
    case DBT_DEVTYP_VOLUME:
      return "Logical volume";
    default:
      return "Unknown device type";
  }
}

#define KS_MSG_WINDOW_CLASS "gst_winks_device_msg_window"
#define WM_QUITTHREAD (WM_USER + 0)

static void unreg_msg_window_class (ATOM class_id, const char *class_name,
    HINSTANCE inst);

static HDEVNOTIFY
register_device_interface (GstKsDeviceProvider * self,
    GUID interface_class_guid, HWND window_handle)
{
  DEV_BROADCAST_DEVICEINTERFACE notification_filter;
  HDEVNOTIFY notification_handle;
  DWORD error;

  memset (&notification_filter, 0, sizeof (notification_filter));
  notification_filter.dbcc_size = sizeof (notification_filter);
  notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  notification_filter.dbcc_classguid = interface_class_guid;

  notification_handle = RegisterDeviceNotificationW (window_handle,
      &notification_filter,
      DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
  error = GetLastError ();

  if (notification_handle == NULL)
    GST_ERROR_OBJECT (self,
        "Could not register for a device notification: %lu", error);

  return notification_handle;
}

static INT_PTR WINAPI
msg_window_message_proc (HWND window_handle, UINT message,
    WPARAM wparam, LPARAM lparam)
{
  LRESULT result;
  LONG_PTR user_data;
  GstKsDeviceProvider *self;
  PDEV_BROADCAST_DEVICEINTERFACE bcdi;
  DEV_BROADCAST_HDR *dev_msg_header;
  struct _DEV_BROADCAST_USERDEFINED *user_dev_msg_header;
  CREATESTRUCT *create_data;
  DWORD error;
  HINSTANCE inst;
  GstKsDevice *dev;
  GstDevice *source;
  GList *item;
  GstDeviceProvider *provider;
  GList *devices;
  gchar *guid_str;

  result = TRUE;

  switch (message) {
    case WM_CREATE:
      create_data = (CREATESTRUCT *) lparam;

      if (create_data->lpCreateParams == NULL) {
        /* DO SOMETHING!! */
      }

      self = GST_KS_DEVICE_PROVIDER (create_data->lpCreateParams);

      SetLastError (0);
      SetWindowLongPtr (window_handle, GWLP_USERDATA, (LONG_PTR) self);
      error = GetLastError ();
      if (error != NO_ERROR) {
        GST_ERROR_OBJECT (self,
            "Could not attach user data to the message window: %lu", error);
        DestroyWindow (window_handle);
        inst = (HINSTANCE) GetModuleHandle (NULL);
        GST_OBJECT_LOCK (self);
        unreg_msg_window_class (self->message_window_class, KS_MSG_WINDOW_CLASS,
            inst);
        self->message_window_class = 0;
        GST_OBJECT_UNLOCK (self);
      }
      result = FALSE;
      break;
    case WM_DEVICECHANGE:
      GST_DEBUG ("WM_DEVICECHANGE for %x %x", (unsigned int) wparam,
          (unsigned int) lparam);

      user_data = GetWindowLongPtr (window_handle, GWLP_USERDATA);
      if (user_data == 0)
        break;

      self = GST_KS_DEVICE_PROVIDER (user_data);
      provider = GST_DEVICE_PROVIDER (self);

      dev_msg_header = (DEV_BROADCAST_HDR *) lparam;

      switch (wparam) {
        case DBT_CONFIGCHANGECANCELED:
          GST_DEBUG_OBJECT (self, "DBT_CONFIGCHANGECANCELED for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_CONFIGCHANGED:
          GST_DEBUG_OBJECT (self, "DBT_CONFIGCHANGED for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_CUSTOMEVENT:
          GST_DEBUG_OBJECT (self, "DBT_CUSTOMEVENT for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_DEVICEARRIVAL:
          GST_DEBUG_OBJECT (self, "DBT_DEVICEARRIVAL for %s",
              get_dev_type (dev_msg_header));

          if (dev_msg_header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
            break;

          bcdi = (PDEV_BROADCAST_DEVICEINTERFACE) lparam;
          guid_str = ks_guid_to_string (&bcdi->dbcc_classguid);
          GST_INFO_OBJECT (self, "New device, class interface GUID %s, path %s",
              guid_str, bcdi->dbcc_name);
          g_free (guid_str);
          break;
        case DBT_DEVICEQUERYREMOVE:
          GST_DEBUG_OBJECT (self, "DBT_DEVICEQUERYREMOVE for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_DEVICEQUERYREMOVEFAILED:
          GST_DEBUG_OBJECT (self, "DBT_DEVICEQUERYREMOVEFAILED for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_DEVICEREMOVECOMPLETE:
          GST_DEBUG_OBJECT (self, "DBT_DEVICEREMOVECOMPLETE for %s",
              get_dev_type (dev_msg_header));

          if (dev_msg_header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
            break;

          bcdi = (PDEV_BROADCAST_DEVICEINTERFACE) lparam;

          guid_str = ks_guid_to_string (&bcdi->dbcc_classguid);
          GST_INFO_OBJECT (self,
              "Removed device, class interface GUID %s, path %s", guid_str,
              bcdi->dbcc_name);
          g_free (guid_str);
          break;
        case DBT_DEVICEREMOVEPENDING:
          GST_DEBUG_OBJECT (self, "DBT_DEVICEREMOVEPENDING for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_DEVICETYPESPECIFIC:
          GST_DEBUG_OBJECT (self, "DBT_DEVICETYPESPECIFIC for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_DEVNODES_CHANGED:
          GST_DEBUG_OBJECT (self, "DBT_DEVNODES_CHANGED for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_QUERYCHANGECONFIG:
          GST_DEBUG_OBJECT (self, "DBT_QUERYCHANGECONFIG for %s",
              get_dev_type (dev_msg_header));
          break;
        case DBT_USERDEFINED:
          user_dev_msg_header = (struct _DEV_BROADCAST_USERDEFINED *) lparam;
          dev_msg_header =
              (DEV_BROADCAST_HDR *) & user_dev_msg_header->dbud_dbh;
          GST_DEBUG_OBJECT (self, "DBT_USERDEFINED for %s: %s",
              get_dev_type (dev_msg_header), user_dev_msg_header->dbud_szName);
          break;
        default:
          break;
      }

      switch (wparam) {
        case DBT_DEVICEARRIVAL:
          if (dev_msg_header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
            break;

          bcdi = (PDEV_BROADCAST_DEVICEINTERFACE) lparam;

          /* Since both video and audio capture device declare KSCATEGORY_CAPTURE, we filter on
             KSCATEGORY_VIDEO here. To add audio support we should accept also KSCATEGORY_AUDIO. */
          if (!IsEqualGUID (&bcdi->dbcc_classguid, &KSCATEGORY_VIDEO))
            break;

          devices =
              ks_enumerate_devices (&bcdi->dbcc_classguid, &KSCATEGORY_CAPTURE);
          if (devices == NULL)
            break;

          source = NULL;
          for (item = devices; item != NULL; item = item->next) {
            KsDeviceEntry *entry = item->data;
            GST_DEBUG_OBJECT (self, "Listed device %s = %s", entry->name,
                entry->path);

            if ((source == NULL) &&
                (g_ascii_strcasecmp (entry->path, bcdi->dbcc_name) == 0))
              source = new_video_source (entry);        /* Or audio source, not implemented yet */

            ks_device_entry_free (entry);
          }

          if (source)
            gst_device_provider_device_add (GST_DEVICE_PROVIDER (self), source);

          g_list_free (devices);
          break;
        case DBT_DEVICEREMOVECOMPLETE:
          if (dev_msg_header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
            break;

          bcdi = (PDEV_BROADCAST_DEVICEINTERFACE) lparam;
          dev = NULL;

          GST_OBJECT_LOCK (self);
          for (item = provider->devices; item; item = item->next) {
            dev = item->data;

            if (g_ascii_strcasecmp (dev->path, bcdi->dbcc_name) == 0) {
              guid_str = gst_device_get_display_name (GST_DEVICE (dev));
              GST_INFO_OBJECT (self, "Device matches to %s", guid_str);
              g_free (guid_str);
              gst_object_ref (dev);
              break;
            }
            dev = NULL;
          }
          GST_OBJECT_UNLOCK (self);

          if (dev) {
            gst_device_provider_device_remove (GST_DEVICE_PROVIDER (self),
                GST_DEVICE (dev));
            gst_object_unref (dev);
          }
          break;
        default:
          break;
      }
      result = FALSE;
      break;
    case WM_DESTROY:
      PostQuitMessage (0);
      result = FALSE;
      break;
    case WM_QUITTHREAD:
      DestroyWindow (window_handle);
      result = FALSE;
      break;
    default:
      result = DefWindowProc (window_handle, message, wparam, lparam);
      break;
  }

  return result;
}

static ATOM
reg_msg_window_class (const char *class_name, HINSTANCE inst)
{
  WNDCLASSEXA classex;

  memset (&classex, 0, sizeof (classex));
  classex.cbSize = sizeof (classex);
  classex.hInstance = inst;
  classex.lpfnWndProc = (WNDPROC) msg_window_message_proc;
  classex.lpszClassName = class_name;

  return RegisterClassExA (&classex);
}

static void
unreg_msg_window_class (ATOM class_id, const char *class_name, HINSTANCE inst)
{
  if (class_id != 0)
    UnregisterClassA ((LPCSTR) MAKELPARAM (class_id, 0), inst);
  else
    UnregisterClassA (class_name, inst);
}

static gpointer
ks_provider_msg_window_thread (gpointer dat)
{
  GstKsDeviceProvider *self;
  MSG msg;
  ATOM wnd_class;
  BOOL message_status;
  HINSTANCE inst;
  HANDLE msg_window = NULL;
  DWORD error;
  HDEVNOTIFY devnotify = NULL;

  g_return_val_if_fail (dat != NULL, NULL);

  self = GST_KS_DEVICE_PROVIDER (dat);

  GST_DEBUG_OBJECT (self, "Entering message window thread: %p",
      g_thread_self ());

  GST_OBJECT_LOCK (self);
  wnd_class = self->message_window_class;
  GST_OBJECT_UNLOCK (self);

  inst = (HINSTANCE) GetModuleHandle (NULL);

  msg_window = CreateWindowExA (0,
      wnd_class != 0 ? (LPCSTR) MAKELPARAM (wnd_class, 0) : KS_MSG_WINDOW_CLASS,
      "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, inst, self);
  error = GetLastError ();

  if (msg_window == NULL) {
    GST_ERROR_OBJECT (self, "Could not create a message window: %lu", error);
    GST_OBJECT_LOCK (self);
    unreg_msg_window_class (wnd_class, KS_MSG_WINDOW_CLASS, inst);
    self->message_window_class = 0;
    SetEvent (self->wakeup_event);
    GST_OBJECT_UNLOCK (self);
    return NULL;
  }

  GST_OBJECT_LOCK (self);
  self->message_window = msg_window;

  devnotify =
      register_device_interface (self, GUID_DEVCLASS_WCEUSBS, msg_window);
  if (devnotify == NULL) {
    DestroyWindow (msg_window);
    unreg_msg_window_class (wnd_class, KS_MSG_WINDOW_CLASS, inst);
    self->message_window_class = 0;
    self->message_window = NULL;
    SetEvent (self->wakeup_event);
    GST_OBJECT_UNLOCK (self);
    return NULL;
  }

  self->device_notify_handle = devnotify;
  SetEvent (self->wakeup_event);
  GST_OBJECT_UNLOCK (self);

  while ((message_status = GetMessage (&msg, NULL, 0, 0)) != 0) {
    if (message_status < 0 || msg.message == WM_QUIT)
      break;
    TranslateMessage (&msg);
    DispatchMessage (&msg);
  }

  GST_DEBUG_OBJECT (self, "Exiting internal window thread: %p",
      g_thread_self ());

  return NULL;
}

static gboolean
gst_ks_device_provider_start (GstDeviceProvider * provider)
{
  ATOM wnd_class = 0;
  HINSTANCE inst;
  HANDLE wakeup_event;
  HWND message_window;
  DWORD error;
  GList *devs;
  GList *dev;
  GstKsDeviceProvider *self = GST_KS_DEVICE_PROVIDER (provider);

  GST_OBJECT_LOCK (self);
  g_assert (self->message_window == NULL);
  GST_OBJECT_UNLOCK (self);

  /* We get notifications on *change*, so before we get to that,
   * we need to obtain a complete list of devices, which we will
   * watch for changes.
   */
  devs = gst_ks_device_provider_probe (provider);
  for (dev = devs; dev; dev = dev->next) {
    if (dev->data)
      gst_device_provider_device_add (provider, (GstDevice *) dev->data);
  }
  g_list_free_full (devs, gst_object_unref);

  inst = (HINSTANCE) GetModuleHandle (NULL);

  wakeup_event = CreateEvent (NULL, TRUE, FALSE, NULL);
  error = GetLastError ();
  if (wakeup_event == NULL) {
    GST_OBJECT_LOCK (self);
    GST_ERROR_OBJECT (self, "Could not create a wakeup event: %lu", error);
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  wnd_class = reg_msg_window_class (KS_MSG_WINDOW_CLASS, inst);
  error = GetLastError ();

  if ((wnd_class == 0) && (error != ERROR_CLASS_ALREADY_EXISTS)) {
    GST_ERROR_OBJECT (self,
        "Could not register message window class: %lu", error);
    CloseHandle (wakeup_event);
    return FALSE;
  }

  GST_OBJECT_LOCK (self);
  self->message_window_class = wnd_class;
  self->wakeup_event = wakeup_event;

  self->message_thread =
      g_thread_new ("ks-device-provider-message-window-thread",
      (GThreadFunc) ks_provider_msg_window_thread, self);
  if (self->message_thread == NULL) {
    GST_ERROR_OBJECT (self, "Could not create message window thread");
    unreg_msg_window_class (wnd_class, KS_MSG_WINDOW_CLASS, inst);
    self->message_window_class = 0;
    CloseHandle (self->wakeup_event);
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (self);

  if (WaitForSingleObject (wakeup_event, INFINITE) != WAIT_OBJECT_0) {
    GST_ERROR_OBJECT (self,
        "Failed to wait for the message thread to initialize");
  }

  GST_OBJECT_LOCK (self);
  CloseHandle (self->wakeup_event);
  self->wakeup_event = NULL;
  message_window = self->message_window;
  GST_OBJECT_UNLOCK (self);

  if (message_window == NULL)
    return FALSE;

  return TRUE;
}

static void
gst_ks_device_provider_stop (GstDeviceProvider * provider)
{
  HINSTANCE inst;
  GThread *message_thread;
  GstKsDeviceProvider *self = GST_KS_DEVICE_PROVIDER (provider);

  GST_OBJECT_LOCK (self);

  g_assert (self->message_window != NULL);

  UnregisterDeviceNotification (self->device_notify_handle);
  self->device_notify_handle = NULL;
  PostMessage (self->message_window, WM_QUITTHREAD, 0, 0);
  message_thread = self->message_thread;
  GST_OBJECT_UNLOCK (self);

  g_thread_join (message_thread);

  GST_OBJECT_LOCK (self);
  self->message_window = NULL;
  self->message_thread = NULL;

  inst = (HINSTANCE) GetModuleHandle (NULL);

  unreg_msg_window_class (self->message_window_class, KS_MSG_WINDOW_CLASS,
      inst);

  self->message_window_class = 0;
  GST_OBJECT_UNLOCK (self);
}

enum
{
  PROP_PATH = 1
};

G_DEFINE_TYPE (GstKsDevice, gst_ks_device, GST_TYPE_DEVICE);

static void gst_ks_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_ks_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ks_device_finalize (GObject * object);
static GstElement *gst_ks_device_create_element (GstDevice * device,
    const gchar * name);
static gboolean gst_ks_device_reconfigure_element (GstDevice * device,
    GstElement * element);

static void
gst_ks_device_class_init (GstKsDeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_ks_device_create_element;
  dev_class->reconfigure_element = gst_ks_device_reconfigure_element;

  object_class->get_property = gst_ks_device_get_property;
  object_class->set_property = gst_ks_device_set_property;
  object_class->finalize = gst_ks_device_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
      g_param_spec_string ("path", "System device path",
          "The system path to the device", "",
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_ks_device_init (GstKsDevice * device)
{
}

static void
gst_ks_device_finalize (GObject * object)
{
  GstKsDevice *device = GST_KS_DEVICE (object);

  g_free (device->path);

  G_OBJECT_CLASS (gst_ks_device_parent_class)->finalize (object);
}

static GstElement *
gst_ks_device_create_element (GstDevice * device, const gchar * name)
{
  GstKsDevice *ks_dev = GST_KS_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (ks_dev->element, name);
  g_object_set (elem, "device-path", ks_dev->path, NULL);

  return elem;
}

static gboolean
gst_ks_device_reconfigure_element (GstDevice * device, GstElement * element)
{
  GstKsDevice *ks_dev = GST_KS_DEVICE (device);

  if (!strcmp (ks_dev->element, "ksvideosrc")) {
    if (!GST_IS_KS_VIDEO_SRC (element))
      return FALSE;
/*
  } else if (!strcmp (ks_dev->element, "ksaudiosrc")) {
    if (!GST_IS_KS_AUDIO_SRC (element))
      return FALSE;
  } else if (!strcmp (ks_dev->element, "ksaudiosink")) {
    if (!GST_IS_KS_AUDIO_SINK (element))
      return FALSE;
*/
  } else {
    g_assert_not_reached ();
  }

  g_object_set (element, "path", ks_dev->path, NULL);

  return TRUE;
}

static GstDevice *
gst_ks_device_new (guint device_index, const gchar * device_name,
    GstCaps * caps, const gchar * device_path, GstKsDeviceType type)
{
  GstKsDevice *gstdev;
  const gchar *element = NULL;
  const gchar *klass = NULL;

  g_return_val_if_fail (device_name, NULL);
  g_return_val_if_fail (device_path, NULL);
  g_return_val_if_fail (caps, NULL);


  switch (type) {
    case GST_KS_DEVICE_TYPE_VIDEO_SOURCE:
      element = "ksvideosrc";
      klass = "Video/Source";
      break;
    case GST_KS_DEVICE_TYPE_AUDIO_SOURCE:
      element = "ksaudiosrc";
      klass = "Audio/Source";
      break;
    case GST_KS_DEVICE_TYPE_AUDIO_SINK:
      element = "ksaudiosink";
      klass = "Audio/Sink";
      break;
    default:
      g_assert_not_reached ();
      break;
  }


  gstdev = g_object_new (GST_TYPE_KS_DEVICE,
      "display-name", device_name, "caps", caps, "device-class", klass,
      "path", device_path, NULL);

  gstdev->type = type;
  gstdev->device_index = device_index;
  gstdev->path = g_strdup (device_path);
  gstdev->element = element;

  return GST_DEVICE (gstdev);
}


static void
gst_ks_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKsDevice *device;

  device = GST_KS_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_PATH:
      g_value_set_string (value, device->path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_ks_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKsDevice *device;

  device = GST_KS_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_PATH:
      device->path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
