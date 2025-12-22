/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha.yang@navercorp.com>
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

#include <gst/gst.h>
#include "gstmftransform.h"
#include "gstmfutils.h"
#include "gstmfplatloader.h"
#include <string.h>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

GST_DEBUG_CATEGORY_EXTERN (gst_mf_transform_debug);
#define GST_CAT_DEFAULT gst_mf_transform_debug

typedef HRESULT (*GstMFTransformAsyncCallbackOnEvent) (MediaEventType event,
    GstObject * client);

class GstMFTransformAsyncCallback : public IMFAsyncCallback
{
public:
  static HRESULT
  CreateInstance (IMFTransform * mft,
      GstMFTransformAsyncCallbackOnEvent event_cb, GstObject * client,
      GstMFTransformAsyncCallback ** callback)
  {
    HRESULT hr;
    GstMFTransformAsyncCallback *self;

    if (!mft || !callback)
      return E_INVALIDARG;

    self = new GstMFTransformAsyncCallback ();

    if (!self)
      return E_OUTOFMEMORY;

    hr = self->Initialize (mft, event_cb, client);

    if (!gst_mf_result (hr)) {
      self->Release ();
      return hr;
    }

    *callback = self;

    return S_OK;
  }

  HRESULT
  BeginGetEvent (void)
  {
    if (!gen_)
      return E_FAIL;

    /* we are running already */
    if (running_)
      return S_OK;

    running_ = true;

    return gen_->BeginGetEvent (this, nullptr);
  }

  HRESULT
  Stop (void)
  {
    running_ = false;

    return S_OK;
  }

  /* IUnknown */
  STDMETHODIMP
  QueryInterface (REFIID riid, void ** object)
  {
    return E_NOTIMPL;
  }

  STDMETHODIMP_ (ULONG)
  AddRef (void)
  {
    GST_TRACE ("%p, %d", this, ref_count_);
    return InterlockedIncrement (&ref_count_);
  }

  STDMETHODIMP_ (ULONG)
  Release (void)
  {
    ULONG ref_count;

    GST_TRACE ("%p, %d", this, ref_count_);
    ref_count = InterlockedDecrement (&ref_count_);

    if (ref_count == 0) {
      GST_TRACE ("Delete instance %p", this);
      delete this;
    }

    return ref_count;
  }

  /* IMFAsyncCallback */
  STDMETHODIMP
  GetParameters (DWORD * flags, DWORD * queue)
  {
    /* this callback could be blocked */
    *flags = MFASYNC_BLOCKING_CALLBACK;
    *queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;
    return S_OK;
  }

  STDMETHODIMP
  Invoke (IMFAsyncResult * async_result)
  {
    ComPtr<IMFMediaEvent> event;
    HRESULT hr;
    bool do_next = true;

    hr = gen_->EndGetEvent (async_result, &event);

    if (!gst_mf_result (hr))
      return hr;

    if (event) {
      MediaEventType type;
      GstObject *client = nullptr;
      hr = event->GetType(&type);
      if (!gst_mf_result (hr))
        return hr;

      if (!event_cb_)
        return S_OK;

      client = (GstObject *) g_weak_ref_get (&client_);
      if (!client)
        return S_OK;

      hr = event_cb_ (type, client);
      gst_object_unref (client);
      if (!gst_mf_result (hr))
        return hr;

      /* On Drain event, this callback object will stop calling BeginGetEvent()
       * since there might be no more following events. Client should call
       * our BeginGetEvent() method to run again */
      if (type == METransformDrainComplete)
        do_next = false;
    }

    if (do_next)
      gen_->BeginGetEvent(this, nullptr);

    return S_OK;
  }

private:
  GstMFTransformAsyncCallback ()
    : ref_count_ (1)
    , running_ (false)
  {
    g_weak_ref_init (&client_, nullptr);
  }

  ~GstMFTransformAsyncCallback ()
  {
    g_weak_ref_clear (&client_);
  }

  HRESULT
  Initialize (IMFTransform * mft, GstMFTransformAsyncCallbackOnEvent event_cb,
      GstObject * client)
  {
    HRESULT hr = mft->QueryInterface(IID_PPV_ARGS(&gen_));

    if (!gst_mf_result (hr))
      return hr;

    event_cb_ = event_cb;
    g_weak_ref_set (&client_, client);

    return S_OK;
  }

private:
  ULONG ref_count_;
  ComPtr<IMFMediaEventGenerator> gen_;
  GstMFTransformAsyncCallbackOnEvent event_cb_;
  GWeakRef client_;

  bool running_;
};
/* *INDENT-ON* */

enum
{
  PROP_0,
  PROP_DEVICE_NAME,
  PROP_HARDWARE,
  PROP_ENUM_PARAMS,
  PROP_D3D11_AWARE,
};

struct _GstMFTransform
{
  GstObject object;
  gboolean initialized;

  GstMFTransformEnumParams enum_params;

  gchar *device_name;
  gboolean hardware;
  gboolean d3d11_aware;

  IMFActivate *activate;
  IMFTransform *transform;
  ICodecAPI *codec_api;
  GstMFTransformAsyncCallback *callback_object;

  GQueue *output_queue;

  DWORD input_id;
  DWORD output_id;

  gboolean running;

  gint pending_need_input;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMutex event_lock;
  GCond event_cond;
  GMainContext *context;
  GMainLoop *loop;
  gboolean draining;
  gboolean flushing;

  GstMFTransformNewSampleCallback callback;
  gpointer user_data;
};

#define gst_mf_transform_parent_class parent_class
G_DEFINE_TYPE (GstMFTransform, gst_mf_transform, GST_TYPE_OBJECT);

static void gst_mf_transform_constructed (GObject * object);
static void gst_mf_transform_finalize (GObject * object);
static void gst_mf_transform_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_mf_transform_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static gpointer gst_mf_transform_thread_func (GstMFTransform * self);
static gboolean gst_mf_transform_close (GstMFTransform * self);
static HRESULT gst_mf_transform_on_event (MediaEventType event,
    GstMFTransform * self);

static void
gst_mf_transform_class_init (GstMFTransformClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_mf_transform_constructed;
  gobject_class->finalize = gst_mf_transform_finalize;
  gobject_class->get_property = gst_mf_transform_get_property;
  gobject_class->set_property = gst_mf_transform_set_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "device-name",
          "Device name", nullptr,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_HARDWARE,
      g_param_spec_boolean ("hardware", "Hardware",
          "Whether hardware device or not", FALSE,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_ENUM_PARAMS,
      g_param_spec_pointer ("enum-params", "Enum Params",
          "GstMFTransformEnumParams for MFTEnumEx",
          (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_D3D11_AWARE,
      g_param_spec_boolean ("d3d11-aware", "D3D11 Aware",
          "Whether Direct3D11 supports Direct3D11", FALSE,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_mf_transform_init (GstMFTransform * self)
{
  self->output_queue = g_queue_new ();

  g_mutex_init (&self->lock);
  g_mutex_init (&self->event_lock);
  g_cond_init (&self->cond);
  g_cond_init (&self->event_cond);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
}

static void
gst_mf_transform_constructed (GObject * object)
{
  GstMFTransform *self = GST_MF_TRANSFORM (object);

  /* Create thread so that ensure COM thread can be MTA thread */
  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstMFTransform",
      (GThreadFunc) gst_mf_transform_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_mf_transform_clear_enum_params (GstMFTransformEnumParams * params)
{
  g_free (params->input_typeinfo);
  params->input_typeinfo = nullptr;

  g_free (params->output_typeinfo);
  params->output_typeinfo = nullptr;
}

static void
release_mf_sample (IMFSample * sample)
{
  if (sample)
    sample->Release ();
}

static void
gst_mf_transform_finalize (GObject * object)
{
  GstMFTransform *self = GST_MF_TRANSFORM (object);

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);
  g_main_loop_unref (self->loop);
  g_main_context_unref (self->context);

  g_queue_free_full (self->output_queue, (GDestroyNotify) release_mf_sample);
  gst_mf_transform_clear_enum_params (&self->enum_params);
  g_free (self->device_name);
  g_mutex_clear (&self->lock);
  g_mutex_clear (&self->event_lock);
  g_cond_clear (&self->cond);
  g_cond_clear (&self->event_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mf_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFTransform *self = GST_MF_TRANSFORM (object);

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      g_value_set_string (value, self->device_name);
      break;
    case PROP_HARDWARE:
      g_value_set_boolean (value, self->hardware);
      break;
    case PROP_D3D11_AWARE:
      g_value_set_boolean (value, self->d3d11_aware);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFTransform *self = GST_MF_TRANSFORM (object);

  switch (prop_id) {
    case PROP_ENUM_PARAMS:
    {
      GstMFTransformEnumParams *params;
      params = (GstMFTransformEnumParams *) g_value_get_pointer (value);

      gst_mf_transform_clear_enum_params (&self->enum_params);
      self->enum_params.category = params->category;
      self->enum_params.enum_flags = params->enum_flags;
      self->enum_params.device_index = params->device_index;
      self->enum_params.adapter_luid = params->adapter_luid;
      if (params->input_typeinfo) {
        self->enum_params.input_typeinfo = g_new0 (MFT_REGISTER_TYPE_INFO, 1);
        memcpy (self->enum_params.input_typeinfo, params->input_typeinfo,
            sizeof (MFT_REGISTER_TYPE_INFO));
      }

      if (params->output_typeinfo) {
        self->enum_params.output_typeinfo = g_new0 (MFT_REGISTER_TYPE_INFO, 1);
        memcpy (self->enum_params.output_typeinfo, params->output_typeinfo,
            sizeof (MFT_REGISTER_TYPE_INFO));
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_mf_transform_main_loop_running_cb (GstMFTransform * self)
{
  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
gst_mf_transform_thread_func (GstMFTransform * self)
{
  HRESULT hr = S_OK;
  IMFActivate **devices = nullptr;
  UINT32 num_devices, i;
  LPWSTR name = nullptr;
  GSource *source;

  CoInitializeEx (nullptr, COINIT_MULTITHREADED);

  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gst_mf_transform_main_loop_running_cb, self, nullptr);
  g_source_attach (source, self->context);
  g_source_unref (source);

  /* NOTE: MFTEnum2 is desktop only and requires Windows 10 */
#if GST_MF_HAVE_D3D11
  if (gst_mf_plat_load_library () && self->enum_params.adapter_luid &&
      (self->enum_params.enum_flags & MFT_ENUM_FLAG_HARDWARE) != 0) {
    ComPtr < IMFAttributes > attr;
    LUID luid;

    hr = MFCreateAttributes (&attr, 1);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't create IMFAttributes");
      goto run_loop;
    }

    GST_INFO_OBJECT (self,
        "Enumerating MFT for adapter-luid %" G_GINT64_FORMAT,
        self->enum_params.adapter_luid);

    luid.LowPart = (DWORD) (self->enum_params.adapter_luid & 0xffffffff);
    luid.HighPart = (LONG) (self->enum_params.adapter_luid >> 32);

    hr = attr->SetBlob (GST_GUID_MFT_ENUM_ADAPTER_LUID, (BYTE *) & luid,
        sizeof (LUID));
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't set MFT_ENUM_ADAPTER_LUID");
      goto run_loop;
    }

    hr = GstMFTEnum2 (self->enum_params.category,
        self->enum_params.enum_flags, self->enum_params.input_typeinfo,
        self->enum_params.output_typeinfo, attr.Get (), &devices, &num_devices);
  } else
#endif
  {
    hr = MFTEnumEx (self->enum_params.category, self->enum_params.enum_flags,
        self->enum_params.input_typeinfo, self->enum_params.output_typeinfo,
        &devices, &num_devices);
  }

  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "MFTEnumEx failure");
    goto run_loop;
  }

  if (num_devices == 0 || self->enum_params.device_index >= num_devices) {
    GST_WARNING_OBJECT (self, "No available device at index %d",
        self->enum_params.device_index);
    for (i = 0; i < num_devices; i++)
      devices[i]->Release ();

    CoTaskMemFree (devices);
    goto run_loop;
  }

  self->activate = devices[self->enum_params.device_index];
  self->activate->AddRef ();

  for (i = 0; i < num_devices; i++)
    devices[i]->Release ();

  hr = self->activate->GetAllocatedString (MFT_FRIENDLY_NAME_Attribute,
      &name, nullptr);

  if (gst_mf_result (hr)) {
    self->device_name = g_utf16_to_utf8 ((const gunichar2 *) name,
        -1, nullptr, nullptr, nullptr);

    GST_INFO_OBJECT (self, "Open device %s", self->device_name);
    CoTaskMemFree (name);
  }

  CoTaskMemFree (devices);

  if ((self->enum_params.enum_flags & MFT_ENUM_FLAG_HARDWARE) != 0)
    self->hardware = TRUE;
  else
    self->hardware = FALSE;

  self->initialized = TRUE;

run_loop:
  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  g_main_context_pop_thread_default (self->context);

  /* cleanup internal COM object here */
  gst_mf_transform_close (self);

  if (self->activate) {
    self->activate->Release ();
    self->activate = nullptr;
  }

  CoUninitialize ();

  return nullptr;
}

static GstFlowReturn
gst_mf_transform_process_output (GstMFTransform * self)
{
  DWORD status;
  HRESULT hr;
  IMFTransform *transform = self->transform;
  DWORD stream_id = self->output_id;
  MFT_OUTPUT_STREAM_INFO out_stream_info = { 0 };
  MFT_OUTPUT_DATA_BUFFER out_data = { 0 };
  GstFlowReturn ret = GST_FLOW_OK;

  GST_TRACE_OBJECT (self, "Process output");

  hr = transform->GetOutputStreamInfo (stream_id, &out_stream_info);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't get output stream info");
    return GST_FLOW_ERROR;
  }

  if ((out_stream_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
              MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0) {
    ComPtr < IMFMediaBuffer > buffer;
    ComPtr < IMFSample > new_sample;

    hr = MFCreateMemoryBuffer (out_stream_info.cbSize, &buffer);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't create memory buffer");
      return GST_FLOW_ERROR;
    }

    hr = MFCreateSample (&new_sample);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't create sample");
      return GST_FLOW_ERROR;
    }

    hr = new_sample->AddBuffer (buffer.Get ());
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't add buffer to sample");
      return GST_FLOW_ERROR;
    }

    out_data.pSample = new_sample.Detach ();
  }

  out_data.dwStreamID = stream_id;

  hr = transform->ProcessOutput (0, 1, &out_data, &status);

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    GST_LOG_OBJECT (self, "Need more input data");
    ret = GST_MF_TRANSFORM_FLOW_NEED_DATA;
  } else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    ComPtr < IMFMediaType > output_type;

    GST_DEBUG_OBJECT (self, "Stream change, set output type again");

    hr = transform->GetOutputAvailableType (stream_id, 0, &output_type);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't get available output type");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    hr = transform->SetOutputType (stream_id, output_type.Get (), 0);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Couldn't set output type");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    ret = GST_MF_TRANSFORM_FLOW_NEED_DATA;
  } else if (!gst_mf_result (hr)) {
    if (self->flushing) {
      GST_DEBUG_OBJECT (self, "Ignore error on flushing");
      ret = GST_FLOW_FLUSHING;
    } else {
      GST_ERROR_OBJECT (self, "ProcessOutput error, hr 0x%x", hr);
      ret = GST_FLOW_ERROR;
    }
  }

done:
  if (ret != GST_FLOW_OK) {
    if (out_data.pSample)
      out_data.pSample->Release ();

    return ret;
  }

  if (!out_data.pSample) {
    GST_WARNING_OBJECT (self, "No output sample");
    return GST_FLOW_OK;
  }

  if (self->callback) {
    self->callback (self, out_data.pSample, self->user_data);
    out_data.pSample->Release ();
    return GST_FLOW_OK;
  }

  g_queue_push_tail (self->output_queue, out_data.pSample);

  return GST_FLOW_OK;
}

/* Must be called with event_lock */
static gboolean
gst_mf_transform_process_input_sync (GstMFTransform * self, IMFSample * sample)
{
  HRESULT hr;

  hr = self->transform->ProcessInput (self->output_id, sample, 0);

  if (self->hardware)
    self->pending_need_input--;

  return gst_mf_result (hr);
}

gboolean
gst_mf_transform_process_input (GstMFTransform * object, IMFSample * sample)
{
  HRESULT hr;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (sample != nullptr, FALSE);

  GST_TRACE_OBJECT (object, "Process input");

  if (!object->transform)
    return FALSE;

  g_mutex_lock (&object->event_lock);
  if (!object->running) {
    object->pending_need_input = 0;

    hr = object->transform->ProcessMessage (MFT_MESSAGE_NOTIFY_START_OF_STREAM,
        0);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (object, "Cannot post start-of-stream message");
      goto done;
    }

    hr = object->transform->ProcessMessage (MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
        0);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (object, "Cannot post begin-stream message");
      goto done;
    }

    if (object->callback_object) {
      hr = object->callback_object->BeginGetEvent ();
      if (!gst_mf_result (hr)) {
        GST_ERROR_OBJECT (object, "BeginGetEvent failed");
        goto done;
      }
    }

    GST_DEBUG_OBJECT (object, "MFT is running now");

    object->running = TRUE;
    object->flushing = FALSE;
  }

  /* Wait METransformNeedInput event. While waiting METransformNeedInput
   * event, we can still output data if MFT notifyes METransformHaveOutput
   * event. */
  if (object->hardware) {
    while (object->pending_need_input == 0 && !object->flushing)
      g_cond_wait (&object->event_cond, &object->event_lock);
  }

  if (object->flushing) {
    GST_DEBUG_OBJECT (object, "We are flushing");
    ret = TRUE;
    goto done;
  }

  ret = gst_mf_transform_process_input_sync (object, sample);

done:
  g_mutex_unlock (&object->event_lock);

  return ret;
}

GstFlowReturn
gst_mf_transform_get_output (GstMFTransform * object, IMFSample ** sample)
{
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), GST_FLOW_ERROR);
  g_return_val_if_fail (sample != nullptr, GST_FLOW_ERROR);
  /* Hardware MFT must not call this method, instead client must install
   * new sample callback so that outputting data from Media Foundation's
   * worker thread */
  g_return_val_if_fail (!object->hardware, GST_FLOW_ERROR);

  if (!object->transform)
    return GST_FLOW_ERROR;

  ret = gst_mf_transform_process_output (object);

  if (ret != GST_MF_TRANSFORM_FLOW_NEED_DATA && ret != GST_FLOW_OK)
    return ret;

  if (g_queue_is_empty (object->output_queue))
    return GST_MF_TRANSFORM_FLOW_NEED_DATA;

  *sample = (IMFSample *) g_queue_pop_head (object->output_queue);

  return GST_FLOW_OK;
}

gboolean
gst_mf_transform_flush (GstMFTransform * object)
{
  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  g_mutex_lock (&object->event_lock);
  object->flushing = TRUE;
  g_cond_broadcast (&object->event_cond);
  g_mutex_unlock (&object->event_lock);

  if (object->transform) {
    /* In case of async MFT, there would be no more event after FLUSH,
     * then callback object shouldn't wait another event.
     * Call Stop() so that our callback object can stop calling BeginGetEvent()
     * from it's Invoke() method */
    if (object->callback_object)
      object->callback_object->Stop ();

    if (object->running) {
      object->transform->ProcessMessage (MFT_MESSAGE_COMMAND_FLUSH, 0);
    }

    object->pending_need_input = 0;
  }

  object->running = FALSE;

  while (!g_queue_is_empty (object->output_queue)) {
    IMFSample *sample = (IMFSample *) g_queue_pop_head (object->output_queue);
    sample->Release ();
  }

  return TRUE;
}

gboolean
gst_mf_transform_drain (GstMFTransform * object)
{
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  if (!object->transform || !object->running)
    return TRUE;

  object->running = FALSE;
  object->draining = TRUE;

  GST_DEBUG_OBJECT (object, "Start drain");

  object->transform->ProcessMessage (MFT_MESSAGE_COMMAND_DRAIN, 0);

  if (object->hardware) {
    g_mutex_lock (&object->event_lock);
    while (object->draining)
      g_cond_wait (&object->event_cond, &object->event_lock);
    g_mutex_unlock (&object->event_lock);
  } else {
    do {
      ret = gst_mf_transform_process_output (object);
    } while (ret == GST_FLOW_OK);
  }

  GST_DEBUG_OBJECT (object, "End drain");

  object->draining = FALSE;
  object->pending_need_input = 0;

  return TRUE;
}

typedef struct
{
  GstMFTransform *object;
  gboolean invoked;
  gboolean ret;
} GstMFTransformOpenData;

static gboolean
gst_mf_transform_open_internal (GstMFTransformOpenData * data)
{
  GstMFTransform *object = data->object;
  HRESULT hr;

  data->ret = FALSE;

  gst_mf_transform_close (object);
  hr = object->activate->ActivateObject (IID_PPV_ARGS (&object->transform));

  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (object, "Couldn't open MFT");
    goto done;
  }

  if (object->hardware) {
    ComPtr < IMFAttributes > attr;
    UINT32 supports_d3d11 = 0;

    hr = object->transform->GetAttributes (&attr);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (object, "Couldn't get attribute object");
      goto done;
    }

    hr = attr->SetUINT32 (MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (object, "MF_TRANSFORM_ASYNC_UNLOCK error");
      goto done;
    }

    hr = attr->GetUINT32 (GST_GUID_MF_SA_D3D11_AWARE, &supports_d3d11);
    if (gst_mf_result (hr) && supports_d3d11 != 0) {
      GST_DEBUG_OBJECT (object, "MFT supports direct3d11");
      object->d3d11_aware = TRUE;
    }

    /* Create our IMFAsyncCallback object so that listen METransformNeedInput
     * and METransformHaveOutput events. The event callback will be called from
     * Media Foundation's worker queue thread */
    hr = GstMFTransformAsyncCallback::CreateInstance (object->transform,
        (GstMFTransformAsyncCallbackOnEvent) gst_mf_transform_on_event,
        GST_OBJECT_CAST (object), &object->callback_object);

    if (!object->callback_object) {
      GST_ERROR_OBJECT (object, "IMFMediaEventGenerator unavailable");
      goto done;
    }
  }

  hr = object->transform->GetStreamIDs (1, &object->input_id, 1,
      &object->output_id);
  if (hr == E_NOTIMPL) {
    object->input_id = 0;
    object->output_id = 0;
  }

  hr = object->transform->QueryInterface (IID_PPV_ARGS (&object->codec_api));
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (object, "ICodecAPI is unavailable");
  }

  data->ret = TRUE;

done:
  if (!data->ret)
    gst_mf_transform_close (object);

  g_mutex_lock (&object->lock);
  data->invoked = TRUE;
  g_cond_broadcast (&object->cond);
  g_mutex_unlock (&object->lock);

  return G_SOURCE_REMOVE;
}

gboolean
gst_mf_transform_open (GstMFTransform * object)
{
  GstMFTransformOpenData data;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (object->activate != nullptr, FALSE);

  data.object = object;
  data.invoked = FALSE;
  data.ret = FALSE;

  g_main_context_invoke (object->context,
      (GSourceFunc) gst_mf_transform_open_internal, &data);

  g_mutex_lock (&object->lock);
  while (!data.invoked)
    g_cond_wait (&object->cond, &object->lock);
  g_mutex_unlock (&object->lock);

  return data.ret;
}

gboolean
gst_mf_transform_set_device_manager (GstMFTransform * object,
    IMFDXGIDeviceManager * manager)
{
  HRESULT hr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  if (!object->transform) {
    GST_ERROR_OBJECT (object, "IMFTransform is not configured yet");
    return FALSE;
  }

  hr = object->transform->ProcessMessage (MFT_MESSAGE_SET_D3D_MANAGER,
      (ULONG_PTR) manager);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (object, "Couldn't set device manager");
    return FALSE;
  }

  return TRUE;
}

void
gst_mf_transform_set_new_sample_callback (GstMFTransform * object,
    GstMFTransformNewSampleCallback callback, gpointer user_data)
{
  g_return_if_fail (GST_IS_MF_TRANSFORM (object));

  object->callback = callback;
  object->user_data = user_data;
}

static gboolean
gst_mf_transform_close (GstMFTransform * object)
{
  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  gst_mf_transform_flush (object);

  /* Otherwise IMFTransform will be alive even after we release the IMFTransform
   * below */
  if (object->activate)
    object->activate->ShutdownObject ();

  if (object->callback_object) {
    object->callback_object->Release ();
    object->callback_object = nullptr;
  }

  if (object->codec_api) {
    object->codec_api->Release ();
    object->codec_api = nullptr;
  }

  if (object->transform) {
    object->transform->Release ();
    object->transform = nullptr;
  }

  return TRUE;
}

static const gchar *
gst_mf_transform_event_type_to_string (MediaEventType event)
{
  switch (event) {
    case METransformNeedInput:
      return "METransformNeedInput";
    case METransformHaveOutput:
      return "METransformHaveOutput";
    case METransformDrainComplete:
      return "METransformDrainComplete";
    case METransformMarker:
      return "METransformMarker";
    case METransformInputStreamStateChanged:
      return "METransformInputStreamStateChanged";
    default:
      break;
  }

  return "Unknown";
}

static HRESULT
gst_mf_transform_on_event (MediaEventType event, GstMFTransform * self)
{
  GST_TRACE_OBJECT (self, "Have event %s (%d)",
      gst_mf_transform_event_type_to_string (event), (gint) event);

  switch (event) {
    case METransformNeedInput:
      g_mutex_lock (&self->event_lock);
      self->pending_need_input++;
      g_cond_broadcast (&self->event_cond);
      g_mutex_unlock (&self->event_lock);
      break;
    case METransformHaveOutput:
      gst_mf_transform_process_output (self);
      break;
    case METransformDrainComplete:
      g_mutex_lock (&self->event_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->event_cond);
      g_mutex_unlock (&self->event_lock);
      break;
    default:
      break;
  }

  return S_OK;
}

IMFActivate *
gst_mf_transform_get_activate_handle (GstMFTransform * object)
{
  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), nullptr);

  return object->activate;
}

IMFTransform *
gst_mf_transform_get_transform_handle (GstMFTransform * object)
{
  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), nullptr);

  if (!object->transform) {
    GST_WARNING_OBJECT (object,
        "IMFTransform is not configured, open MFT first");
    return nullptr;
  }

  return object->transform;
}

ICodecAPI *
gst_mf_transform_get_codec_api_handle (GstMFTransform * object)
{
  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), nullptr);

  if (!object->codec_api) {
    GST_WARNING_OBJECT (object, "ICodecAPI is not configured, open MFT first");
    return nullptr;
  }

  return object->codec_api;
}

gboolean
gst_mf_transform_get_input_available_types (GstMFTransform * object,
    GList ** input_types)
{
  IMFTransform *transform;
  HRESULT hr;
  DWORD index = 0;
  GList *list = nullptr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (input_types != nullptr, FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  do {
    IMFMediaType *type = nullptr;

    hr = transform->GetInputAvailableType (object->input_id, index, &type);
    if (SUCCEEDED (hr))
      list = g_list_append (list, type);

    index++;
  } while (SUCCEEDED (hr));

  if (!list)
    return FALSE;

  *input_types = list;

  return TRUE;
}

gboolean
gst_mf_transform_get_output_available_types (GstMFTransform * object,
    GList ** output_types)
{
  IMFTransform *transform;
  HRESULT hr;
  DWORD index = 0;
  GList *list = nullptr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (output_types != nullptr, FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  do {
    IMFMediaType *type;

    hr = transform->GetOutputAvailableType (object->input_id, index, &type);
    if (SUCCEEDED (hr))
      list = g_list_append (list, type);

    index++;
  } while (SUCCEEDED (hr));

  if (!list)
    return FALSE;

  *output_types = list;

  return TRUE;
}

gboolean
gst_mf_transform_set_input_type (GstMFTransform * object,
    IMFMediaType * input_type)
{
  IMFTransform *transform;
  HRESULT hr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  hr = transform->SetInputType (object->input_id, input_type, 0);
  if (!gst_mf_result (hr))
    return FALSE;

  return TRUE;
}

gboolean
gst_mf_transform_set_output_type (GstMFTransform * object,
    IMFMediaType * output_type)
{
  IMFTransform *transform;
  HRESULT hr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  hr = transform->SetOutputType (object->output_id, output_type, 0);
  if (!gst_mf_result (hr)) {
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_mf_transform_get_input_current_type (GstMFTransform * object,
    IMFMediaType ** input_type)
{
  IMFTransform *transform;
  HRESULT hr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (input_type != nullptr, FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  hr = transform->GetInputCurrentType (object->input_id, input_type);
  if (!gst_mf_result (hr)) {
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_mf_transform_get_output_current_type (GstMFTransform * object,
    IMFMediaType ** output_type)
{
  IMFTransform *transform;
  HRESULT hr;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (output_type != nullptr, FALSE);

  transform = object->transform;

  if (!transform) {
    GST_ERROR_OBJECT (object, "Should open first");
    return FALSE;
  }

  hr = transform->GetOutputCurrentType (object->output_id, output_type);
  if (!gst_mf_result (hr)) {
    return FALSE;
  }

  return TRUE;
}

GstMFTransform *
gst_mf_transform_new (GstMFTransformEnumParams * params)
{
  GstMFTransform *self;

  g_return_val_if_fail (params != nullptr, nullptr);

  self = (GstMFTransform *) g_object_new (GST_TYPE_MF_TRANSFORM_OBJECT,
      "enum-params", params, nullptr);

  if (!self->initialized) {
    gst_object_unref (self);
    return nullptr;
  }

  gst_object_ref_sink (self);

  return self;
}

gboolean
gst_mf_transform_set_codec_api_uint32 (GstMFTransform * object,
    const GUID * api, guint32 value)
{
  HRESULT hr;
  VARIANT var;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (api != nullptr, FALSE);

  if (!object->codec_api) {
    GST_WARNING_OBJECT (object, "codec api unavailable");
    return FALSE;
  }

  VariantInit (&var);
  var.vt = VT_UI4;
  var.ulVal = value;

  hr = object->codec_api->SetValue (api, &var);
  VariantClear (&var);

  return gst_mf_result (hr);
}

gboolean
gst_mf_transform_set_codec_api_uint64 (GstMFTransform * object,
    const GUID * api, guint64 value)
{
  HRESULT hr;
  VARIANT var;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (api != nullptr, FALSE);

  if (!object->codec_api) {
    GST_WARNING_OBJECT (object, "codec api unavailable");
    return FALSE;
  }

  VariantInit (&var);
  var.vt = VT_UI8;
  var.ullVal = value;

  hr = object->codec_api->SetValue (api, &var);
  VariantClear (&var);

  return gst_mf_result (hr);
}

gboolean
gst_mf_transform_set_codec_api_boolean (GstMFTransform * object,
    const GUID * api, gboolean value)
{
  HRESULT hr;
  VARIANT var;

  g_return_val_if_fail (GST_IS_MF_TRANSFORM (object), FALSE);
  g_return_val_if_fail (api != nullptr, FALSE);

  if (!object->codec_api) {
    GST_WARNING_OBJECT (object, "codec api unavailable");
    return FALSE;
  }

  VariantInit (&var);
  var.vt = VT_BOOL;
  var.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;

  hr = object->codec_api->SetValue (api, &var);
  VariantClear (&var);

  return gst_mf_result (hr);
}
