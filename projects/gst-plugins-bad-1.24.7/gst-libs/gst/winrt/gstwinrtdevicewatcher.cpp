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

#include "gstwinrtdevicewatcher.h"

/* workaround for GetCurrentTime collision */
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <windows.foundation.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

/* *INDENT-OFF* */
typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformation IAddedHandler;
typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformationUpdate IUpdatedHandler;
typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformationUpdate IRemovedHandler;
typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable IEnumerationCompletedHandler;
typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable IStoppedHandler;

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Devices;
using namespace ABI::Windows::Devices::Enumeration;

GST_DEBUG_CATEGORY_STATIC (gst_winrt_device_watcher_debug);
#define GST_CAT_DEFAULT gst_winrt_device_watcher_debug

static void
gst_winrt_device_watcher_device_added (GstWinRTDeviceWatcher * self,
    IDeviceInformation * info);
static void
gst_winrt_device_watcher_device_updated (GstWinRTDeviceWatcher * self,
    IDeviceInformationUpdate * info_update);
static void
gst_winrt_device_watcher_device_removed (GstWinRTDeviceWatcher * self,
    IDeviceInformationUpdate * info_update);
static void
gst_winrt_device_watcher_device_enumeration_completed (GstWinRTDeviceWatcher *
    self);
static void
gst_winrt_device_watcher_device_enumeration_stopped (GstWinRTDeviceWatcher *
    self);

class AddedHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAddedHandler>
{
public:
  AddedHandler () {}
  HRESULT RuntimeClassInitialize (GstWinRTDeviceWatcher * listenr)
  {
    if (!listenr)
      return E_INVALIDARG;

    listener_ = listenr;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IDeviceWatcher* sender, IDeviceInformation * arg)
  {
    gst_winrt_device_watcher_device_added (listener_, arg);

    return S_OK;
  }

private:
  GstWinRTDeviceWatcher * listener_;
};

class UpdatedHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IUpdatedHandler>
{
public:
  UpdatedHandler () {}
  HRESULT RuntimeClassInitialize (GstWinRTDeviceWatcher * listenr)
  {
    if (!listenr)
      return E_INVALIDARG;

    listener_ = listenr;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IDeviceWatcher* sender, IDeviceInformationUpdate * arg)
  {
    gst_winrt_device_watcher_device_updated (listener_, arg);

    return S_OK;
  }

private:
  GstWinRTDeviceWatcher * listener_;
};

class RemovedHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IRemovedHandler>
{
public:
  RemovedHandler () {}
  HRESULT RuntimeClassInitialize (GstWinRTDeviceWatcher * listenr)
  {
    if (!listenr)
      return E_INVALIDARG;

    listener_ = listenr;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IDeviceWatcher* sender, IDeviceInformationUpdate * arg)
  {
    gst_winrt_device_watcher_device_removed (listener_, arg);

    return S_OK;
  }

private:
  GstWinRTDeviceWatcher * listener_;
};

class EnumerationCompletedHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumerationCompletedHandler>
{
public:
  EnumerationCompletedHandler () {}
  HRESULT RuntimeClassInitialize (GstWinRTDeviceWatcher * listenr)
  {
    if (!listenr)
      return E_INVALIDARG;

    listener_ = listenr;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IDeviceWatcher* sender, IInspectable * arg)
  {
    gst_winrt_device_watcher_device_enumeration_completed (listener_);

    return S_OK;
  }

private:
  GstWinRTDeviceWatcher * listener_;
};

class StoppedHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IStoppedHandler>
{
public:
  StoppedHandler () {}
  HRESULT RuntimeClassInitialize (GstWinRTDeviceWatcher * listenr)
  {
    if (!listenr)
      return E_INVALIDARG;

    listener_ = listenr;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IDeviceWatcher* sender, IInspectable * arg)
  {
    gst_winrt_device_watcher_device_enumeration_stopped (listener_);

    return S_OK;
  }

private:
  GstWinRTDeviceWatcher * listener_;
};
/* *INDENT-ON* */

typedef struct
{
  ComPtr < IDeviceWatcher > watcher;

  EventRegistrationToken added_token;
  EventRegistrationToken updated_token;
  EventRegistrationToken removed_token;
  EventRegistrationToken enum_completed_token;
  EventRegistrationToken stopped_token;
} GstWinRTDeviceWatcherInner;

enum
{
  PROP_0,
  PROP_DEVICE_CLASS,
};

#define DEFAULT_DEVICE_CLASS GST_WINRT_DEVICE_CLASS_ALL

struct _GstWinRTDeviceWatcherPrivate
{
  GMutex lock;
  GCond cond;

  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;

  gboolean running;

  GstWinRTDeviceWatcherCallbacks callbacks;
  gpointer user_data;

  GstWinRTDeviceWatcherInner *inner;

  GstWinRTDeviceClass device_class;
};

GType
gst_winrt_device_class_get_type (void)
{
  static gsize device_class_type = 0;

  if (g_once_init_enter (&device_class_type)) {
    static const GEnumValue classes[] = {
      {GST_WINRT_DEVICE_CLASS_ALL, "All", "all"},
      {GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE, "AudioCapture", "audio-capture"},
      {GST_WINRT_DEVICE_CLASS_AUDIO_RENDER, "AudioRender", "audio-render"},
      {GST_WINRT_DEVICE_CLASS_PORTABLE_STORAGE_DEVICE,
          "PortableStorageDevice", "portable-storage-device"},
      {GST_WINRT_DEVICE_CLASS_VIDEO_CAPTURE,
          "VideoCapture", "video-capture"},
      {0, nullptr, nullptr},
    };
    GType tmp = g_enum_register_static ("GstWinRTDeviceClass", classes);
    g_once_init_leave (&device_class_type, tmp);
  }

  return (GType) device_class_type;
}

static void gst_winrt_device_watcher_constructed (GObject * object);
static void gst_winrt_device_watcher_finalize (GObject * object);
static void gst_winrt_device_watcher_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_winrt_device_watcher_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gpointer
gst_winrt_device_watcher_thread_func (GstWinRTDeviceWatcher * self);

#define gst_winrt_device_watcher_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstWinRTDeviceWatcher, gst_winrt_device_watcher,
    GST_TYPE_OBJECT);

static void
gst_winrt_device_watcher_class_init (GstWinRTDeviceWatcherClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_winrt_device_watcher_constructed;
  gobject_class->finalize = gst_winrt_device_watcher_finalize;
  gobject_class->set_property = gst_winrt_device_watcher_set_property;
  gobject_class->get_property = gst_winrt_device_watcher_get_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE_CLASS,
      g_param_spec_enum ("device-class", "Device Class",
          "Device class to watch", GST_TYPE_WINRT_DEVICE_CLASS,
          DEFAULT_DEVICE_CLASS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));

  GST_DEBUG_CATEGORY_INIT (gst_winrt_device_watcher_debug,
      "winrtdevicewatcher", 0, "winrtdevicewatcher");
}

static void
gst_winrt_device_watcher_init (GstWinRTDeviceWatcher * self)
{
  GstWinRTDeviceWatcherPrivate *priv;

  self->priv = priv = (GstWinRTDeviceWatcherPrivate *)
      gst_winrt_device_watcher_get_instance_private (self);

  g_mutex_init (&priv->lock);
  g_cond_init (&priv->cond);
  priv->context = g_main_context_new ();
  priv->loop = g_main_loop_new (priv->context, FALSE);
}

static void
gst_winrt_device_watcher_constructed (GObject * object)
{
  GstWinRTDeviceWatcher *self = GST_WINRT_DEVICE_WATCHER (object);
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  g_mutex_lock (&priv->lock);
  priv->thread = g_thread_new ("GstWinRTDeviceWatcher",
      (GThreadFunc) gst_winrt_device_watcher_thread_func, self);
  while (!g_main_loop_is_running (priv->loop))
    g_cond_wait (&priv->cond, &priv->lock);
  g_mutex_unlock (&priv->lock);
}

static void
gst_winrt_device_watcher_finalize (GObject * object)
{
  GstWinRTDeviceWatcher *self = GST_WINRT_DEVICE_WATCHER (object);
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  g_main_loop_quit (priv->loop);
  if (g_thread_self () != priv->thread) {
    g_thread_join (priv->thread);
    g_main_loop_unref (priv->loop);
    g_main_context_unref (priv->context);
  } else {
    g_warning ("Trying join from self-thread");
  }

  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_winrt_device_watcher_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWinRTDeviceWatcher *self = GST_WINRT_DEVICE_WATCHER (object);
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_DEVICE_CLASS:
      priv->device_class = (GstWinRTDeviceClass) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_winrt_device_watcher_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWinRTDeviceWatcher *self = GST_WINRT_DEVICE_WATCHER (object);
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_DEVICE_CLASS:
      g_value_set_enum (value, priv->device_class);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
loop_running_cb (GstWinRTDeviceWatcher * self)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  g_mutex_lock (&priv->lock);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return G_SOURCE_REMOVE;
}

static void
gst_winrt_device_watcher_thread_func_inner (GstWinRTDeviceWatcher * self)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;
  GSource *idle_source;
  HRESULT hr;
  GstWinRTDeviceWatcherInner *inner = nullptr;
  ComPtr < IDeviceInformationStatics > factory;
  ComPtr < IDeviceWatcher > watcher;
  ComPtr < IAddedHandler > added_handler;
  ComPtr < IUpdatedHandler > updated_handler;
  ComPtr < IRemovedHandler > removed_handler;
  ComPtr < IEnumerationCompletedHandler > enum_completed_handler;
  ComPtr < IStoppedHandler > stopped_handler;

  g_main_context_push_thread_default (priv->context);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) loop_running_cb, self, nullptr);
  g_source_attach (idle_source, priv->context);
  g_source_unref (idle_source);

  hr = GetActivationFactory (HStringReference
      (RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get (),
      &factory);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self,
        "Failed to get IDeviceInformationStatics, hr: 0x%x", (guint) hr);
    goto run_loop;
  }

  hr = factory->CreateWatcherDeviceClass ((DeviceClass) priv->device_class,
      &watcher);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self,
        "Failed to create IDeviceWatcher, hr: 0x%x", (guint) hr);
    goto run_loop;
  }

  hr = MakeAndInitialize < AddedHandler > (&added_handler, self);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create added handler, hr: 0x%x", hr);
    goto run_loop;
  }

  hr = MakeAndInitialize < UpdatedHandler > (&updated_handler, self);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create updated handler, hr: 0x%x", hr);
    goto run_loop;
  }

  hr = MakeAndInitialize < RemovedHandler > (&removed_handler, self);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create removed handler, hr: 0x%x", hr);
    goto run_loop;
  }

  hr = MakeAndInitialize < EnumerationCompletedHandler >
      (&enum_completed_handler, self);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self,
        "Failed to create enumeration completed handler, hr: 0x%x", hr);
    goto run_loop;
  }

  hr = MakeAndInitialize < StoppedHandler > (&stopped_handler, self);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create stopped handler, hr: 0x%x", hr);
    goto run_loop;
  }

  inner = new GstWinRTDeviceWatcherInner ();
  hr = watcher->add_Added (added_handler.Get (), &inner->added_token);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to register added handler, hr: 0x%x", hr);
    delete inner;
    inner = nullptr;

    goto run_loop;
  }

  hr = watcher->add_Updated (updated_handler.Get (), &inner->updated_token);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to register updated handler, hr: 0x%x", hr);
    delete inner;
    inner = nullptr;

    goto run_loop;
  }

  hr = watcher->add_Removed (removed_handler.Get (), &inner->removed_token);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to register removed handler, hr: 0x%x", hr);
    delete inner;
    inner = nullptr;

    goto run_loop;
  }

  hr = watcher->add_EnumerationCompleted (enum_completed_handler.Get (),
      &inner->enum_completed_token);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self,
        "Failed to register enumeration completed handler, hr: 0x%x", hr);
    delete inner;
    inner = nullptr;

    goto run_loop;
  }

  hr = watcher->add_Stopped (stopped_handler.Get (), &inner->stopped_token);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to register stopped handler, hr: 0x%x", hr);
    delete inner;
    inner = nullptr;

    goto run_loop;
  }

  inner->watcher = watcher;
  priv->inner = inner;

run_loop:
  GST_INFO_OBJECT (self, "Starting loop");
  g_main_loop_run (priv->loop);
  GST_INFO_OBJECT (self, "Stopped loop");

  if (inner) {
    if (priv->running)
      watcher->Stop ();

    watcher->remove_Added (inner->added_token);
    watcher->remove_Updated (inner->updated_token);
    watcher->remove_Removed (inner->removed_token);
    watcher->remove_EnumerationCompleted (inner->enum_completed_token);
    watcher->remove_Stopped (inner->stopped_token);

    delete inner;
  }

  g_main_context_pop_thread_default (priv->context);
}

static gpointer
gst_winrt_device_watcher_thread_func (GstWinRTDeviceWatcher * self)
{
  RoInitializeWrapper initialize (RO_INIT_MULTITHREADED);

  /* wrap with another function so that everything can happen
   * before RoInitializeWrapper is destructed */
  gst_winrt_device_watcher_thread_func_inner (self);

  return nullptr;
}

static void
gst_winrt_device_watcher_device_added (GstWinRTDeviceWatcher * self,
    IDeviceInformation * info)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Device added");

  if (priv->callbacks.added)
    priv->callbacks.added (self, info, priv->user_data);
}

static void
gst_winrt_device_watcher_device_updated (GstWinRTDeviceWatcher * self,
    IDeviceInformationUpdate * info_update)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Device updated");

  if (priv->callbacks.updated)
    priv->callbacks.updated (self, info_update, priv->user_data);
}

static void
gst_winrt_device_watcher_device_removed (GstWinRTDeviceWatcher * self,
    IDeviceInformationUpdate * info_update)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Device removed");

  if (priv->callbacks.removed)
    priv->callbacks.removed (self, info_update, priv->user_data);
}

static void
gst_winrt_device_watcher_device_enumeration_completed (GstWinRTDeviceWatcher *
    self)
{
  GstWinRTDeviceWatcherPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Enumeration completed");

  if (priv->callbacks.enumeration_completed)
    priv->callbacks.enumeration_completed (self, priv->user_data);
}

static void
gst_winrt_device_watcher_device_enumeration_stopped (GstWinRTDeviceWatcher *
    self)
{
  GST_DEBUG_OBJECT (self, "Stopped");
}

/**
 * gst_winrt_device_watcher_new:
 * @device_class: a #GstWinRTDeviceClass to watch
 * @callbacks: a pointer to #GstWinRTDeviceWatcherCallbacks
 * @user_data: a user_data argument for the callbacks
 *
 * Constructs a new #GstWinRTDeviceWatcher object for watching device update
 * of @device_class
 *
 * Returns: (transfer full) (nullable): a new #GstWinRTDeviceWatcher
 * or %NULL when failed to create/setup #GstWinRTDeviceWatcher object
 *
 * Since: 1.20
 */
GstWinRTDeviceWatcher *
gst_winrt_device_watcher_new (GstWinRTDeviceClass device_class,
    const GstWinRTDeviceWatcherCallbacks * callbacks, gpointer user_data)
{
  GstWinRTDeviceWatcher *self;
  GstWinRTDeviceWatcherPrivate *priv;

  g_return_val_if_fail (callbacks != nullptr, nullptr);

  self = (GstWinRTDeviceWatcher *)
      g_object_new (GST_TYPE_WINRT_DEVICE_WATCHER, "device-class", device_class,
      nullptr);

  priv = self->priv;
  if (!priv->inner) {
    gst_object_unref (self);
    return nullptr;
  }

  priv->callbacks = *callbacks;
  priv->user_data = user_data;

  gst_object_ref_sink (self);

  return self;
}

/**
 * gst_winrt_device_watcher_start:
 * @device_class: a #GstWinRTDeviceClass to watch
 *
 * Starts watching device update.
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.20
 */
gboolean
gst_winrt_device_watcher_start (GstWinRTDeviceWatcher * watcher)
{
  GstWinRTDeviceWatcherPrivate *priv;
  GstWinRTDeviceWatcherInner *inner;
  HRESULT hr;

  g_return_val_if_fail (GST_IS_WINRT_DEVICE_WATCHER (watcher), FALSE);

  priv = watcher->priv;
  inner = priv->inner;

  GST_DEBUG_OBJECT (watcher, "Start");

  g_mutex_lock (&priv->lock);
  if (priv->running) {
    GST_DEBUG_OBJECT (watcher, "Already running");
    g_mutex_unlock (&priv->lock);

    return TRUE;
  }

  hr = inner->watcher->Start ();
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (watcher, "Failed to start watcher, hr: 0x%x", (guint) hr);
    g_mutex_unlock (&priv->lock);

    return FALSE;
  }

  priv->running = TRUE;
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/**
 * gst_winrt_device_watcher_stop:
 * @device_class: a #GstWinRTDeviceClass to watch
 *
 * Stops watching device update.
 *
 * Since: 1.20
 */
void
gst_winrt_device_watcher_stop (GstWinRTDeviceWatcher * watcher)
{
  GstWinRTDeviceWatcherPrivate *priv;
  GstWinRTDeviceWatcherInner *inner;
  HRESULT hr;

  g_return_if_fail (GST_IS_WINRT_DEVICE_WATCHER (watcher));

  GST_DEBUG_OBJECT (watcher, "Stop");

  priv = watcher->priv;
  inner = priv->inner;

  g_mutex_lock (&priv->lock);
  if (!priv->running) {
    g_mutex_unlock (&priv->lock);

    return;
  }

  priv->running = FALSE;
  hr = inner->watcher->Stop ();
  if (FAILED (hr)) {
    GST_WARNING_OBJECT (watcher,
        "Failed to stop watcher, hr: 0x%x", (guint) hr);
  }
  g_mutex_unlock (&priv->lock);
}
