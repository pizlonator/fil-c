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

#include "gstmmdeviceenumerator.h"

#ifndef INITGUID
#include <initguid.h>
#endif

/* *INDENT-OFF* */
G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (gst_wasapi_debug);
#define GST_CAT_DEFAULT gst_wasapi_debug

G_END_DECLS

/* IMMNotificationClient implementation */
class GstIMMNotificationClient : public IMMNotificationClient
{
public:
  static HRESULT
  CreateInstance (GstMMDeviceEnumerator * enumerator,
      const GstMMNotificationClientCallbacks * callbacks,
      gpointer user_data,
      IMMNotificationClient ** client)
  {
    GstIMMNotificationClient *self;

    self = new GstIMMNotificationClient ();

    self->callbacks_ = *callbacks;
    self->user_data_ = user_data;
    g_weak_ref_set (&self->enumerator_, enumerator);

    *client = (IMMNotificationClient *) self;

    return S_OK;
  }

  /* IUnknown */
  STDMETHODIMP
  QueryInterface (REFIID riid, void ** object)
  {
    if (!object)
      return E_POINTER;

    if (riid == IID_IUnknown) {
      *object = static_cast<IUnknown *> (this);
    } else if (riid == __uuidof(IMMNotificationClient)) {
      *object = static_cast<IMMNotificationClient *> (this);
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef ();

    return S_OK;
  }

  STDMETHODIMP_ (ULONG)
  AddRef (void)
  {
    GST_TRACE ("%p, %d", this, (guint) ref_count_);
    return InterlockedIncrement (&ref_count_);
  }

  STDMETHODIMP_ (ULONG)
  Release (void)
  {
    ULONG ref_count;

    GST_TRACE ("%p, %d", this, (guint) ref_count_);
    ref_count = InterlockedDecrement (&ref_count_);

    if (ref_count == 0) {
      GST_TRACE ("Delete instance %p", this);
      delete this;
    }

    return ref_count;
  }

  /* IMMNotificationClient */
  STDMETHODIMP
  OnDeviceStateChanged (LPCWSTR device_id, DWORD new_state)
  {
    GstMMDeviceEnumerator *listener;
    HRESULT hr;

    if (!callbacks_.device_state_changed)
      return S_OK;

    listener = (GstMMDeviceEnumerator *) g_weak_ref_get (&enumerator_);
    if (!listener)
      return S_OK;

    hr = callbacks_.device_state_changed (listener, device_id, new_state,
        user_data_);
    gst_object_unref (listener);

    return hr;
  }

  STDMETHODIMP
  OnDeviceAdded (LPCWSTR device_id)
  {
    GstMMDeviceEnumerator *listener;
    HRESULT hr;

    if (!callbacks_.device_added)
      return S_OK;

    listener = (GstMMDeviceEnumerator *) g_weak_ref_get (&enumerator_);
    if (!listener)
      return S_OK;

    hr = callbacks_.device_added (listener, device_id, user_data_);
    gst_object_unref (listener);

    return hr;
  }

  STDMETHODIMP
  OnDeviceRemoved (LPCWSTR device_id)
  {
    GstMMDeviceEnumerator *listener;
    HRESULT hr;

    if (!callbacks_.device_removed)
      return S_OK;

    listener = (GstMMDeviceEnumerator *) g_weak_ref_get (&enumerator_);
    if (!listener)
      return S_OK;

    hr = callbacks_.device_removed (listener, device_id, user_data_);
    gst_object_unref (listener);

    return hr;
  }

  STDMETHODIMP
  OnDefaultDeviceChanged (EDataFlow flow, ERole role, LPCWSTR default_device_id)
  {
    GstMMDeviceEnumerator *listener;
    HRESULT hr;

    if (!callbacks_.default_device_changed)
      return S_OK;

    listener = (GstMMDeviceEnumerator *) g_weak_ref_get (&enumerator_);
    if (!listener)
      return S_OK;

    hr = callbacks_.default_device_changed (listener,
        flow, role, default_device_id, user_data_);
    gst_object_unref (listener);

    return hr;
  }

  STDMETHODIMP
  OnPropertyValueChanged (LPCWSTR device_id, const PROPERTYKEY key)
  {
    GstMMDeviceEnumerator *listener;
    HRESULT hr;

    if (!callbacks_.property_value_changed)
      return S_OK;

    listener = (GstMMDeviceEnumerator *) g_weak_ref_get (&enumerator_);
    if (!device_id)
      return S_OK;

    hr = callbacks_.property_value_changed (listener,
        device_id, key, user_data_);
    gst_object_unref (listener);

    return hr;
  }

private:
  GstIMMNotificationClient ()
    : ref_count_ (1)
  {
    g_weak_ref_init (&enumerator_, nullptr);
  }

  virtual ~GstIMMNotificationClient ()
  {
    g_weak_ref_clear (&enumerator_);
  }

private:
  ULONG ref_count_;
  GstMMNotificationClientCallbacks callbacks_;
  gpointer user_data_;
  GWeakRef enumerator_;
};
/* *INDENT-ON* */

struct _GstMMDeviceEnumerator
{
  GstObject parent;

  IMMDeviceEnumerator *handle;
  IMMNotificationClient *client;

  GMutex lock;
  GCond cond;

  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;

  gboolean running;
};

static void gst_mm_device_enumerator_constructed (GObject * object);
static void gst_mm_device_enumerator_finalize (GObject * object);

static gpointer
gst_mm_device_enumerator_thread_func (GstMMDeviceEnumerator * self);

#define gst_mm_device_enumerator_parent_class parent_class
G_DEFINE_TYPE (GstMMDeviceEnumerator,
    gst_mm_device_enumerator, GST_TYPE_OBJECT);

static void
gst_mm_device_enumerator_class_init (GstMMDeviceEnumeratorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_mm_device_enumerator_constructed;
  gobject_class->finalize = gst_mm_device_enumerator_finalize;
}

static void
gst_mm_device_enumerator_init (GstMMDeviceEnumerator * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);
  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
}

static void
gst_mm_device_enumerator_constructed (GObject * object)
{
  GstMMDeviceEnumerator *self = GST_MM_DEVICE_ENUMERATOR (object);

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstMMDeviceEnumerator",
      (GThreadFunc) gst_mm_device_enumerator_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);
}

static void
gst_mm_device_enumerator_finalize (GObject * object)
{
  GstMMDeviceEnumerator *self = GST_MM_DEVICE_ENUMERATOR (object);

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);
  g_main_loop_unref (self->loop);
  g_main_context_unref (self->context);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
loop_running_cb (GstMMDeviceEnumerator * self)
{
  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
gst_mm_device_enumerator_thread_func (GstMMDeviceEnumerator * self)
{
  GSource *idle_source;
  IMMDeviceEnumerator *enumerator = nullptr;
  HRESULT hr;

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  g_main_context_push_thread_default (self->context);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) loop_running_cb, self, nullptr);
  g_source_attach (idle_source, self->context);
  g_source_unref (idle_source);

  hr = CoCreateInstance (__uuidof (MMDeviceEnumerator),
      nullptr, CLSCTX_ALL, IID_PPV_ARGS (&enumerator));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create IMMDeviceEnumerator instance");
    goto run_loop;
  }

  self->handle = enumerator;

run_loop:
  GST_INFO_OBJECT (self, "Starting loop");
  g_main_loop_run (self->loop);
  GST_INFO_OBJECT (self, "Stopped loop");

  if (self->client && self->handle) {
    self->handle->UnregisterEndpointNotificationCallback (self->client);

    self->client->Release ();
  }

  if (self->handle)
    self->handle->Release ();

  g_main_context_pop_thread_default (self->context);
  CoUninitialize ();

  return nullptr;
}

GstMMDeviceEnumerator *
gst_mm_device_enumerator_new (void)
{
  GstMMDeviceEnumerator *self;

  self = (GstMMDeviceEnumerator *) g_object_new (GST_TYPE_MM_DEVICE_ENUMERATOR,
      nullptr);

  if (!self->handle) {
    gst_object_unref (self);
    return nullptr;
  }

  gst_object_ref_sink (self);

  return self;
}

IMMDeviceEnumerator *
gst_mm_device_enumerator_get_handle (GstMMDeviceEnumerator * enumerator)
{
  g_return_val_if_fail (GST_IS_MM_DEVICE_ENUMERATOR (enumerator), nullptr);

  return enumerator->handle;
}

typedef struct
{
  GstMMDeviceEnumerator *self;
  GstMMNotificationClientCallbacks *callbacks;
  gpointer user_data;

  gboolean handled;
  GMutex lock;
  GCond cond;

  gboolean ret;
} SetNotificationCallbackData;

static gboolean
set_notification_callback (SetNotificationCallbackData * data)
{
  GstMMDeviceEnumerator *self = data->self;
  HRESULT hr;

  g_mutex_lock (&data->lock);
  g_mutex_lock (&self->lock);

  data->ret = TRUE;

  if (self->client) {
    self->handle->UnregisterEndpointNotificationCallback (self->client);
    self->client->Release ();
    self->client = nullptr;
  }

  if (data->callbacks) {
    IMMNotificationClient *client;

    hr = GstIMMNotificationClient::CreateInstance (self, data->callbacks,
        data->user_data, &client);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self,
          "Failed to create IMMNotificationClient instance");
      data->ret = FALSE;
      goto out;
    }

    hr = self->handle->RegisterEndpointNotificationCallback (client);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to register callback");
      client->Release ();
      data->ret = FALSE;
      goto out;
    }

    self->client = client;
  }

out:
  data->handled = TRUE;
  g_cond_signal (&data->cond);
  g_mutex_unlock (&self->lock);
  g_mutex_unlock (&data->lock);

  return G_SOURCE_REMOVE;
}

gboolean
gst_mm_device_enumerator_set_notification_callback (GstMMDeviceEnumerator *
    enumerator, GstMMNotificationClientCallbacks * callbacks,
    gpointer user_data)
{
  SetNotificationCallbackData data;
  gboolean ret;

  g_return_val_if_fail (GST_IS_MM_DEVICE_ENUMERATOR (enumerator), FALSE);

  data.self = enumerator;
  data.callbacks = callbacks;
  data.user_data = user_data;
  data.handled = FALSE;

  g_mutex_init (&data.lock);
  g_cond_init (&data.cond);

  g_main_context_invoke (enumerator->context,
      (GSourceFunc) set_notification_callback, &data);
  g_mutex_lock (&data.lock);
  while (!data.handled)
    g_cond_wait (&data.cond, &data.lock);
  g_mutex_unlock (&data.lock);

  ret = data.ret;

  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);

  return ret;
}
