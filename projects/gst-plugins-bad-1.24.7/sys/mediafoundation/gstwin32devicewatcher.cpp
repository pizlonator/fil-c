/* GStreamer
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwin32devicewatcher.h"
#include <dbt.h>

GST_DEBUG_CATEGORY_STATIC (gst_win32_device_watcher_debug);
#define GST_CAT_DEFAULT gst_win32_device_watcher_debug

G_LOCK_DEFINE_STATIC (create_lock);

#define GST_WIN32_HWND_PROP_NAME "gst-win32-device-watcher"

struct _GstWin32DeviceWatcher
{
  GstObject parent;

  GMutex lock;
  GCond cond;

  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;

  GstWin32DeviceWatcherCallbacks callbacks;
  gpointer user_data;

  HDEVNOTIFY device_notify;
  HWND hwnd;
  DWORD device_type;
  GUID class_guid;
};

#define gst_win32_device_watcher_parent_class parent_class
G_DEFINE_TYPE (GstWin32DeviceWatcher, gst_win32_device_watcher,
    GST_TYPE_OBJECT);

static void gst_win32_device_watcher_constructed (GObject * object);
static void gst_win32_device_watcher_finalize (GObject * object);

static gpointer
gst_win32_device_watcher_thread_func (GstWin32DeviceWatcher * self);

static void
gst_win32_device_watcher_class_init (GstWin32DeviceWatcherClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_win32_device_watcher_constructed;
  gobject_class->finalize = gst_win32_device_watcher_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_win32_device_watcher_debug,
      "win32devicewatcher", 0, "win32devicewatcher");
}

static void
gst_win32_device_watcher_init (GstWin32DeviceWatcher * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);
  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
}

static void
gst_win32_device_watcher_constructed (GObject * object)
{
  GstWin32DeviceWatcher *self = GST_WIN32_DEVICE_WATCHER (object);

  /* Create a new thread for WIN32 message pumping */
  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstWin32DeviceWatcher",
      (GThreadFunc) gst_win32_device_watcher_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_win32_device_watcher_finalize (GObject * object)
{
  GstWin32DeviceWatcher *self = GST_WIN32_DEVICE_WATCHER (object);

  g_main_loop_quit (self->loop);
  if (g_thread_self () != self->thread) {
    g_thread_join (self->thread);
    g_main_loop_unref (self->loop);
    g_main_context_unref (self->context);
  } else {
    g_warning ("Trying join from self-thread");
  }

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static LRESULT CALLBACK
window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  GstWin32DeviceWatcher *self;

  switch (msg) {
    case WM_CREATE:
      self = (GstWin32DeviceWatcher *)
          ((LPCREATESTRUCT) lparam)->lpCreateParams;

      GST_DEBUG_OBJECT (self, "WM_CREATE");

      SetPropA (hwnd, GST_WIN32_HWND_PROP_NAME, self);
      break;
    case WM_DEVICECHANGE:
    {
      self = (GstWin32DeviceWatcher *) GetPropA
          (hwnd, GST_WIN32_HWND_PROP_NAME);
      if (!self) {
        GST_WARNING ("Failed to get watcher object");
        break;
      }

      self->callbacks.device_changed (self, wparam, lparam, self->user_data);
      break;
    }
    default:
      break;
  }

  return DefWindowProc (hwnd, msg, wparam, lparam);
}

static HWND
create_hwnd (GstWin32DeviceWatcher * self)
{
  WNDCLASSEXA wc;
  ATOM atom = 0;
  HINSTANCE hinstance = GetModuleHandle (nullptr);
  static const gchar *klass_name = "GstWin32DeviceWatcher";
  HWND hwnd;

  G_LOCK (create_lock);
  atom = GetClassInfoExA (hinstance, klass_name, &wc);
  if (atom == 0) {
    GST_LOG_OBJECT (self, "Register window class");
    ZeroMemory (&wc, sizeof (WNDCLASSEX));

    wc.cbSize = sizeof (WNDCLASSEXA);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hinstance;
    wc.lpszClassName = klass_name;
    atom = RegisterClassExA (&wc);

    if (atom == 0) {
      G_UNLOCK (create_lock);
      GST_ERROR_OBJECT (self, "Failed to register window class, lastError 0x%x",
          (guint) GetLastError ());
      return nullptr;
    }
  } else {
    GST_LOG_OBJECT (self, "window class was already registered");
  }
  G_UNLOCK (create_lock);

  hwnd = CreateWindowExA (0, klass_name, "", 0, 0, 0, 0, 0,
      HWND_MESSAGE, nullptr, hinstance, self);
  if (!hwnd) {
    GST_ERROR_OBJECT (self, "Failed to create window handle, lastError 0x%x",
        (guint) GetLastError ());
    return nullptr;
  }

  return hwnd;
}

static gboolean
loop_running_cb (GstWin32DeviceWatcher * self)
{
  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gboolean
win32_msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

static gpointer
gst_win32_device_watcher_thread_func (GstWin32DeviceWatcher * self)
{
  GSource *idle_source;
  HWND hwnd = nullptr;
  GIOChannel *msg_io_channel = nullptr;
  GSource *msg_source = nullptr;

  g_main_context_push_thread_default (self->context);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) loop_running_cb, self, nullptr);
  g_source_attach (idle_source, self->context);
  g_source_unref (idle_source);

  hwnd = create_hwnd (self);
  if (!hwnd)
    goto run_loop;

  msg_io_channel = g_io_channel_win32_new_messages ((gsize) hwnd);
  msg_source = g_io_create_watch (msg_io_channel, G_IO_IN);
  g_source_set_callback (msg_source,
      (GSourceFunc) win32_msg_cb, nullptr, nullptr);
  g_source_attach (msg_source, self->context);

  self->hwnd = hwnd;

run_loop:
  GST_INFO_OBJECT (self, "Starting loop");
  g_main_loop_run (self->loop);
  GST_INFO_OBJECT (self, "Stopped loop");

  if (self->device_notify) {
    UnregisterDeviceNotification (self->device_notify);
    self->device_notify = nullptr;
  }

  if (msg_source) {
    g_source_destroy (msg_source);
    g_source_unref (msg_source);
  }

  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);

  if (hwnd)
    DestroyWindow (hwnd);

  g_main_context_pop_thread_default (self->context);

  return nullptr;
}

GstWin32DeviceWatcher *
gst_win32_device_watcher_new (DWORD device_type, const GUID * class_guid,
    const GstWin32DeviceWatcherCallbacks * callbacks, gpointer user_data)
{
  GstWin32DeviceWatcher *self;

  g_return_val_if_fail (class_guid != nullptr, nullptr);
  g_return_val_if_fail (callbacks != nullptr, nullptr);

  self = (GstWin32DeviceWatcher *)
      g_object_new (GST_TYPE_WIN32_DEVICE_WATCHER, nullptr);

  if (!self->hwnd) {
    gst_object_unref (self);
    return nullptr;
  }

  self->callbacks = *callbacks;
  self->user_data = user_data;
  self->device_type = device_type;
  self->class_guid = *class_guid;

  gst_object_ref_sink (self);

  return self;
}

typedef struct
{
  GstWin32DeviceWatcher *self;

  gboolean handled;
  gboolean ret;
} DeviceNotificationData;

static gboolean
register_device_notification (DeviceNotificationData * data)
{
  GstWin32DeviceWatcher *self = data->self;
  DEV_BROADCAST_DEVICEINTERFACE di = { 0, };

  if (self->device_notify)
    goto out;

  di.dbcc_size = sizeof (di);
  di.dbcc_devicetype = self->device_type;
  di.dbcc_classguid = self->class_guid;

  self->device_notify = RegisterDeviceNotificationW (self->hwnd,
      &di, DEVICE_NOTIFY_WINDOW_HANDLE);

out:
  if (self->device_notify)
    data->ret = TRUE;

  g_mutex_lock (&self->lock);
  data->handled = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

gboolean
gst_win32_device_watcher_start (GstWin32DeviceWatcher * watcher)
{
  DeviceNotificationData data;

  g_return_val_if_fail (GST_IS_WIN32_DEVICE_WATCHER (watcher), FALSE);

  data.self = watcher;
  data.handled = FALSE;
  data.ret = FALSE;

  g_main_context_invoke (watcher->context,
      (GSourceFunc) register_device_notification, &data);
  g_mutex_lock (&watcher->lock);
  while (!data.handled)
    g_cond_wait (&watcher->cond, &watcher->lock);
  g_mutex_unlock (&watcher->lock);

  return data.ret;
}

static gboolean
unregister_device_notification (DeviceNotificationData * data)
{
  GstWin32DeviceWatcher *self = data->self;

  if (!self->device_notify)
    goto out;

  UnregisterDeviceNotification (self->device_notify);
  self->device_notify = nullptr;

out:
  g_mutex_lock (&self->lock);
  data->handled = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

void
gst_win32_device_watcher_stop (GstWin32DeviceWatcher * watcher)
{
  DeviceNotificationData data;

  g_return_if_fail (GST_IS_WIN32_DEVICE_WATCHER (watcher));

  data.self = watcher;
  data.handled = FALSE;

  g_main_context_invoke (watcher->context,
      (GSourceFunc) register_device_notification, &data);
  g_mutex_lock (&watcher->lock);
  while (!data.handled)
    g_cond_wait (&watcher->cond, &watcher->lock);
  g_mutex_unlock (&watcher->lock);
}
