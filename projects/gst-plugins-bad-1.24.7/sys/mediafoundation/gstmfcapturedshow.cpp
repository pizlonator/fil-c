/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include <gst/base/base.h>
#include <gst/video/video.h>
#include "gstmfcapturedshow.h"
#include <string.h>
#include <wrl.h>
#include <dshow.h>
#include <objidl.h>
#include <string>
#include <locale>
#include <codecvt>
#include <vector>
#include <algorithm>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_mf_source_object_debug);
#define GST_CAT_DEFAULT gst_mf_source_object_debug

DEFINE_GUID (MF_MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00,
    0xAA, 0x00, 0x38, 0x9B, 0x71);

/* From qedit.h */
DEFINE_GUID (CLSID_SampleGrabber, 0xc1f400A0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00,
    0x60, 0x08, 0x03, 0x9e, 0x37);

DEFINE_GUID (CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00,
    0x60, 0x08, 0x03, 0x9e, 0x37);

/* *INDENT-OFF* */
struct DECLSPEC_UUID("0579154a-2b53-4994-b0d0-e773148eff85")
ISampleGrabberCB : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE SampleCB(
      double SampleTime,
      IMediaSample *pSample) = 0;

  virtual HRESULT STDMETHODCALLTYPE BufferCB(
      double SampleTime,
      BYTE *pBuffer,
      LONG BufferLen) = 0;
};

struct DECLSPEC_UUID("6b652fff-11fe-4fce-92ad-0266b5d7c78f")
ISampleGrabber : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE SetOneShot(
      BOOL OneShot) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetMediaType(
      const AM_MEDIA_TYPE *pType) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
      AM_MEDIA_TYPE *pType) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
      BOOL BufferThem) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
      LONG *pBufferSize,
      LONG *pBuffer) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
      IMediaSample **ppSample) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetCallback(
      ISampleGrabberCB *pCallback,
      LONG WhichMethodToCallback) = 0;
};

typedef void (*OnBufferCB) (double sample_time,
                            BYTE * buffer,
                            LONG len,
                            gpointer user_data);

class DECLSPEC_UUID("bfae6598-5df6-11ed-9b6a-0242ac120002")
IGstMFSampleGrabberCB : public ISampleGrabberCB
{
public:
  static HRESULT
  CreateInstance (OnBufferCB callback, gpointer user_data,
      IGstMFSampleGrabberCB ** cb)
  {
    IGstMFSampleGrabberCB *self =
        new IGstMFSampleGrabberCB (callback, user_data);

    if (!self)
      return E_OUTOFMEMORY;

    *cb = self;

    return S_OK;
  }

  STDMETHODIMP_ (ULONG)
  AddRef (void)
  {
    return InterlockedIncrement (&ref_count_);
  }

  STDMETHODIMP_ (ULONG)
  Release (void)
  {
    ULONG ref_count;

    ref_count = InterlockedDecrement (&ref_count_);

    if (ref_count == 0)
      delete this;

    return ref_count;
  }

  STDMETHODIMP
  QueryInterface (REFIID riid, void ** object)
  {
    if (riid == __uuidof (IUnknown)) {
      *object = static_cast<IUnknown *>
          (static_cast<IGstMFSampleGrabberCB *> (this));
    } else if (riid == __uuidof (ISampleGrabberCB)) {
      *object = static_cast<ISampleGrabberCB *>
          (static_cast<IGstMFSampleGrabberCB *> (this));
    } else if (riid == __uuidof (IGstMFSampleGrabberCB)) {
      *object = this;
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef ();

    return S_OK;
  }

  STDMETHODIMP
  SampleCB (double SampleTime, IMediaSample *pSample)
  {
    return E_NOTIMPL;
  }

  STDMETHODIMP
  BufferCB (double SampleTime, BYTE *pBuffer, LONG BufferLen)
  {
    if (callback_)
      callback_ (SampleTime, pBuffer, BufferLen, user_data_);

    return S_OK;
  }

private:
  IGstMFSampleGrabberCB (OnBufferCB callback, gpointer user_data)
    : callback_ (callback), user_data_ (user_data), ref_count_ (1)
  {
  }

  virtual ~IGstMFSampleGrabberCB (void)
  {
  }

private:
  OnBufferCB callback_;
  gpointer user_data_;
  ULONG ref_count_;
};

struct GStMFDShowMoniker
{
  GStMFDShowMoniker ()
  {
  }

  GStMFDShowMoniker (ComPtr<IMoniker> m, const std::string &d, const std::string & n,
      const std::string p, guint i)
  {
    moniker = m;
    desc = d;
    name = n;
    path = p;
    index = i;
  }

  GStMFDShowMoniker (const GStMFDShowMoniker & other)
  {
    moniker = other.moniker;
    desc = other.desc;
    name = other.name;
    path = other.path;
    index = other.index;
  }

  ComPtr<IMoniker> moniker;
  std::string desc;
  std::string name;
  std::string path;
  guint index = 0;
};

static void
ClearMediaType (AM_MEDIA_TYPE * type)
{
  if (type->cbFormat && type->pbFormat)
    CoTaskMemFree (type->pbFormat);

  if (type->pUnk)
    type->pUnk->Release ();
}

static void
FreeMediaType (AM_MEDIA_TYPE * type)
{
  if (!type)
    return;

  ClearMediaType (type);
  CoTaskMemFree (type);
}

static inline std::string
convert_to_string (const wchar_t * wstr)
{
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;

  return conv.to_bytes (wstr);
}

static HRESULT
FindFirstPin (IBaseFilter * filter, PIN_DIRECTION dir, IPin ** pin)
{
  ComPtr < IEnumPins > enum_pins;
  HRESULT hr;
  PIN_DIRECTION direction;

  hr = filter->EnumPins (&enum_pins);
  if (!gst_mf_result (hr))
    return hr;

  do {
    ComPtr < IPin > tmp;

    hr = enum_pins->Next (1, &tmp, nullptr);
    if (hr != S_OK)
      return hr;

    hr = tmp->QueryDirection (&direction);
    if (!gst_mf_result (hr) || direction != dir)
      continue;

    *pin = tmp.Detach ();
    return S_OK;
  } while (hr == S_OK);

  return E_FAIL;
}

struct GstMFDShowPinInfo
{
  GstMFDShowPinInfo ()
  {
  }

  GstMFDShowPinInfo (const std::wstring & id, GstCaps *c, gint i,
      gboolean top_down)
  {
    pin_id = id;
    caps = c;
    index = i;
    top_down_image = top_down;
  }

  GstMFDShowPinInfo (const GstMFDShowPinInfo & other)
  {
    pin_id = other.pin_id;
    if (other.caps)
      caps = gst_caps_ref (other.caps);
    index = other.index;
    top_down_image = other.top_down_image;
  }

  ~GstMFDShowPinInfo ()
  {
    if (caps)
      gst_caps_unref (caps);
  }

  bool operator< (const GstMFDShowPinInfo & other)
  {
    return gst_mf_source_object_caps_compare (caps, other.caps) < 0;
  }

  GstMFDShowPinInfo & operator= (const GstMFDShowPinInfo & other)
  {
    gst_clear_caps (&caps);

    pin_id = other.pin_id;
    if (other.caps)
      caps = gst_caps_ref (other.caps);
    index = other.index;
    top_down_image = other.top_down_image;

    return *this;
  }

  std::wstring pin_id;
  GstCaps *caps = nullptr;
  gint index = 0;
  gboolean top_down_image = TRUE;
};

struct GstMFCaptureDShowInner
{
  ~GstMFCaptureDShowInner()
  {
    if (grabber)
      grabber->SetCallback (nullptr, 0);
  }

  std::vector<GstMFDShowPinInfo> pin_infos;
  ComPtr <IFilterGraph> graph;
  ComPtr <IMediaControl> control;
  ComPtr <IBaseFilter> capture;
  ComPtr <ISampleGrabber> grabber;
  ComPtr <IBaseFilter> fakesink;
  GstMFDShowPinInfo selected_pin_info;
};
/* *INDENT-ON* */

enum CAPTURE_STATE
{
  CAPTURE_STATE_STOPPED,
  CAPTURE_STATE_RUNNING,
  CAPTURE_STATE_ERROR,
};

struct _GstMFCaptureDShow
{
  GstMFSourceObject parent;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  GstMFCaptureDShowInner *inner;
  GstBufferPool *pool;

  /* protected by lock */
  GQueue sample_queue;
  CAPTURE_STATE state;

  GstCaps *supported_caps;
  GstCaps *selected_caps;
  GstVideoInfo info;

  gboolean top_down_image;
  gboolean flushing;
};

static void gst_mf_capture_dshow_constructed (GObject * object);
static void gst_mf_capture_dshow_finalize (GObject * object);

static gboolean gst_mf_capture_dshow_start (GstMFSourceObject * object);
static gboolean gst_mf_capture_dshow_stop (GstMFSourceObject * object);
static GstFlowReturn
gst_mf_capture_dshow_get_sample (GstMFSourceObject * object,
    GstSample ** sample);
static gboolean gst_mf_capture_dshow_unlock (GstMFSourceObject * object);
static gboolean gst_mf_capture_dshow_unlock_stop (GstMFSourceObject * object);
static GstCaps *gst_mf_capture_dshow_get_caps (GstMFSourceObject * object);
static gboolean gst_mf_capture_dshow_set_caps (GstMFSourceObject * object,
    GstCaps * caps);

static gpointer gst_mf_capture_dshow_thread_func (GstMFCaptureDShow * self);
static void gst_mf_capture_dshow_on_buffer (double sample_time,
    BYTE * buffer, LONG buffer_len, gpointer user_data);

#define gst_mf_capture_dshow_parent_class parent_class
G_DEFINE_TYPE (GstMFCaptureDShow, gst_mf_capture_dshow,
    GST_TYPE_MF_SOURCE_OBJECT);

static void
gst_mf_capture_dshow_class_init (GstMFCaptureDShowClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstMFSourceObjectClass *source_class = GST_MF_SOURCE_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_mf_capture_dshow_constructed;
  gobject_class->finalize = gst_mf_capture_dshow_finalize;

  source_class->start = GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_start);
  source_class->stop = GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_stop);
  source_class->get_sample =
      GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_get_sample);
  source_class->unlock = GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_unlock);
  source_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_unlock_stop);
  source_class->get_caps = GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_get_caps);
  source_class->set_caps = GST_DEBUG_FUNCPTR (gst_mf_capture_dshow_set_caps);
}

static void
gst_mf_capture_dshow_init (GstMFCaptureDShow * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);
  g_queue_init (&self->sample_queue);

  self->state = CAPTURE_STATE_STOPPED;
}

static void
gst_mf_capture_dshow_constructed (GObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);

  /* Create a new thread to ensure that COM thread can be MTA thread */
  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstMFCaptureDShow",
      (GThreadFunc) gst_mf_capture_dshow_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_mf_capture_dshow_finalize (GObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);
  g_main_loop_unref (self->loop);
  g_main_context_unref (self->context);

  gst_clear_caps (&self->supported_caps);
  gst_clear_caps (&self->selected_caps);
  g_queue_clear_full (&self->sample_queue, (GDestroyNotify) gst_sample_unref);
  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_mf_capture_dshow_start (GstMFSourceObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);
  GstMFCaptureDShowInner *inner = self->inner;
  HRESULT hr;
  ComPtr < IAMStreamConfig > config;
  ComPtr < IBaseFilter > grabber;
  ComPtr < IPin > output;
  ComPtr < IPin > input;
  AM_MEDIA_TYPE *type = nullptr;
  VIDEO_STREAM_CONFIG_CAPS config_caps;
  GstMFDShowPinInfo & selected = inner->selected_pin_info;
  GstStructure *pool_config;

  if (!selected.caps) {
    GST_ERROR_OBJECT (self, "No selected pin");
    return FALSE;
  }

  gst_video_info_from_caps (&self->info, selected.caps);
  gst_clear_caps (&self->selected_caps);
  self->selected_caps = gst_caps_ref (selected.caps);
  self->top_down_image = selected.top_down_image;

  /* Get pin and mediainfo of capture filter */
  hr = inner->capture->FindPin (selected.pin_id.c_str (), &output);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Could not find output pin of capture filter");
    return FALSE;
  }

  hr = output.As (&config);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Could not get IAMStreamConfig interface");
    return FALSE;
  }

  hr = config->GetStreamCaps (selected.index, &type, (BYTE *) & config_caps);
  if (!gst_mf_result (hr) || !type) {
    GST_ERROR_OBJECT (self, "Could not get type from pin");
    return FALSE;
  }

  hr = inner->grabber.As (&grabber);
  if (!gst_mf_result (hr)) {
    FreeMediaType (type);
    return FALSE;
  }

  /* Find input pint of grabber */
  hr = FindFirstPin (grabber.Get (), PINDIR_INPUT, &input);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Couldn't get input pin from grabber");
    FreeMediaType (type);
    return FALSE;
  }

  hr = inner->graph->ConnectDirect (output.Get (), input.Get (), type);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not connect capture and grabber");
    FreeMediaType (type);
    return FALSE;
  }

  /* Link grabber and fakesink here */
  input = nullptr;
  output = nullptr;
  hr = FindFirstPin (grabber.Get (), PINDIR_OUTPUT, &output);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Couldn't get output pin from grabber");
    FreeMediaType (type);
    return FALSE;
  }

  hr = FindFirstPin (inner->fakesink.Get (), PINDIR_INPUT, &input);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Couldn't get input pin from fakesink");
    FreeMediaType (type);
    return FALSE;
  }

  hr = inner->graph->ConnectDirect (output.Get (), input.Get (), type);
  FreeMediaType (type);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not connect grabber and fakesink");
    return FALSE;
  }

  self->pool = gst_video_buffer_pool_new ();
  pool_config = gst_buffer_pool_get_config (self->pool);
  gst_buffer_pool_config_add_option (pool_config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (pool_config, selected.caps,
      GST_VIDEO_INFO_SIZE (&self->info), 0, 0);
  if (!gst_buffer_pool_set_config (self->pool, pool_config)) {
    GST_ERROR_OBJECT (self, "Couldn not set buffer pool config");
    gst_clear_object (&self->pool);
    return FALSE;
  }

  if (!gst_buffer_pool_set_active (self->pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't activate pool");
    gst_clear_object (&self->pool);
    return FALSE;
  }

  self->state = CAPTURE_STATE_RUNNING;

  hr = inner->control->Run ();
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't start graph");
    self->state = CAPTURE_STATE_ERROR;
    g_cond_broadcast (&self->cond);
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mf_capture_dshow_stop (GstMFSourceObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);
  GstMFCaptureDShowInner *inner = self->inner;

  GST_DEBUG_OBJECT (self, "Stop");

  g_mutex_lock (&self->lock);
  self->state = CAPTURE_STATE_STOPPED;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  if (inner->control)
    inner->control->Stop ();

  if (self->pool)
    gst_buffer_pool_set_active (self->pool, FALSE);

  return TRUE;
}

static GstFlowReturn
gst_mf_capture_dshow_get_sample (GstMFSourceObject * object,
    GstSample ** sample)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);

  g_mutex_lock (&self->lock);
  while (g_queue_is_empty (&self->sample_queue) && !self->flushing &&
      self->state == CAPTURE_STATE_RUNNING) {
    g_cond_wait (&self->cond, &self->lock);
  }

  if (self->flushing) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_FLUSHING;
  }

  if (self->state == CAPTURE_STATE_ERROR) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_ERROR;
  }

  *sample = (GstSample *) g_queue_pop_head (&self->sample_queue);
  g_mutex_unlock (&self->lock);

  return GST_FLOW_OK;
}

static gboolean
gst_mf_capture_dshow_unlock (GstMFSourceObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);

  GST_DEBUG_OBJECT (self, "Unlock");

  g_mutex_lock (&self->lock);
  self->flushing = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_mf_capture_dshow_unlock_stop (GstMFSourceObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);

  GST_DEBUG_OBJECT (self, "Unlock Stop");

  g_mutex_lock (&self->lock);
  self->flushing = FALSE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static GstCaps *
gst_mf_capture_dshow_get_caps (GstMFSourceObject * object)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);
  GstCaps *caps = nullptr;

  g_mutex_lock (&self->lock);
  if (self->selected_caps) {
    caps = gst_caps_ref (self->selected_caps);
  } else if (self->supported_caps) {
    caps = gst_caps_ref (self->supported_caps);
  }
  g_mutex_unlock (&self->lock);

  return caps;
}

static gboolean
gst_mf_capture_dshow_set_caps (GstMFSourceObject * object, GstCaps * caps)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (object);
  GstMFCaptureDShowInner *inner = self->inner;
  GstMFDShowPinInfo pin_info;

  /* *INDENT-OFF* */
  for (const auto & iter: inner->pin_infos) {
    if (gst_caps_can_intersect (iter.caps, caps)) {
      pin_info = iter;
      break;
    }
  }
  /* *INDENT-ON* */

  if (!pin_info.caps) {
    GST_ERROR_OBJECT (self, "Could not determine target pin with given caps %"
        GST_PTR_FORMAT, caps);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Selecting caps %" GST_PTR_FORMAT " for caps %"
      GST_PTR_FORMAT, pin_info.caps, caps);

  inner->selected_pin_info = pin_info;

  return TRUE;
}

static gboolean
gst_mf_capture_dshow_main_loop_running_cb (GstMFCaptureDShow * self)
{
  GST_INFO_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static GstVideoFormat
subtype_to_format (REFGUID subtype)
{
  if (subtype == MEDIASUBTYPE_MJPG)
    return GST_VIDEO_FORMAT_ENCODED;
  else if (subtype == MEDIASUBTYPE_RGB555)
    return GST_VIDEO_FORMAT_RGB15;
  else if (subtype == MEDIASUBTYPE_RGB565)
    return GST_VIDEO_FORMAT_RGB16;
  else if (subtype == MEDIASUBTYPE_RGB24)
    return GST_VIDEO_FORMAT_BGR;
  else if (subtype == MEDIASUBTYPE_RGB32)
    return GST_VIDEO_FORMAT_BGRx;
  else if (subtype == MEDIASUBTYPE_ARGB32)
    return GST_VIDEO_FORMAT_BGRA;
  else if (subtype == MEDIASUBTYPE_AYUV)
    return GST_VIDEO_FORMAT_VUYA;
  else if (subtype == MEDIASUBTYPE_YUY2)
    return GST_VIDEO_FORMAT_YUY2;
  else if (subtype == MEDIASUBTYPE_UYVY)
    return GST_VIDEO_FORMAT_UYVY;
  else if (subtype == MEDIASUBTYPE_YV12)
    return GST_VIDEO_FORMAT_YV12;
  else if (subtype == MEDIASUBTYPE_NV12)
    return GST_VIDEO_FORMAT_NV12;
  else if (subtype == MF_MEDIASUBTYPE_I420)
    return GST_VIDEO_FORMAT_I420;
  else if (subtype == MEDIASUBTYPE_IYUV)
    return GST_VIDEO_FORMAT_I420;

  return GST_VIDEO_FORMAT_UNKNOWN;
}

static GstCaps *
media_type_to_caps (AM_MEDIA_TYPE * type, gboolean * top_down_image)
{
  gint fps_n = 0;
  gint fps_d = 1;
  GstVideoFormat format;
  VIDEOINFOHEADER *header;
  GstCaps *caps;

  if (!type)
    return nullptr;

  if (type->majortype != MEDIATYPE_Video ||
      type->formattype != FORMAT_VideoInfo) {
    return nullptr;
  }

  format = subtype_to_format (type->subtype);
  if (format == GST_VIDEO_FORMAT_UNKNOWN ||
      /* TODO: support jpeg */
      format == GST_VIDEO_FORMAT_ENCODED) {
    return nullptr;
  }

  if (!type->pbFormat || type->cbFormat < sizeof (VIDEOINFOHEADER))
    return nullptr;

  header = (VIDEOINFOHEADER *) type->pbFormat;
  if (header->bmiHeader.biWidth <= 0 || header->bmiHeader.biHeight <= 0) {
    return nullptr;
  }

  if (header->AvgTimePerFrame > 0) {
    /* 100ns unit */
    gst_video_guess_framerate ((GstClockTime) header->AvgTimePerFrame * 100,
        &fps_n, &fps_d);
  }

  if (top_down_image) {
    const GstVideoFormatInfo *finfo = gst_video_format_get_info (format);
    if (GST_VIDEO_FORMAT_INFO_IS_RGB (finfo) && header->bmiHeader.biHeight < 0) {
      *top_down_image = FALSE;
    } else {
      *top_down_image = TRUE;
    }
  }

  caps = gst_caps_new_empty_simple ("video/x-raw");
  gst_caps_set_simple (caps, "format", G_TYPE_STRING,
      gst_video_format_to_string (format),
      "width", G_TYPE_INT, (gint) header->bmiHeader.biWidth,
      "height", G_TYPE_INT, (gint) header->bmiHeader.biHeight,
      "framerate", GST_TYPE_FRACTION, fps_n, fps_d, nullptr);

  return caps;
}

static gboolean
gst_mf_capture_dshow_open (GstMFCaptureDShow * self, IMoniker * moniker)
{
  ComPtr < IBaseFilter > capture;
  ComPtr < IEnumPins > pin_list;
  ComPtr < IFilterGraph > graph;
  ComPtr < IMediaFilter > filter;
  ComPtr < IMediaControl > control;
  GstMFCaptureDShowInner *inner = self->inner;
  HRESULT hr;
  PIN_DIRECTION direction;

  hr = CoCreateInstance (CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS (&graph));
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not get IGraphBuilder interface");
    return FALSE;
  }

  hr = graph.As (&filter);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not get IMediaFilter interface");
    return FALSE;
  }

  /* Make graph work as if sync=false */
  filter->SetSyncSource (nullptr);

  hr = graph.As (&control);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not get IMediaControl interface");
    return FALSE;
  }

  hr = moniker->BindToObject (nullptr, nullptr, IID_PPV_ARGS (&capture));
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not bind capture object");
    return FALSE;
  }

  hr = graph->AddFilter (capture.Get (), L"CaptureFilter");
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not add capture filter to graph");
    return FALSE;
  }

  ComPtr < IBaseFilter > grabber;
  hr = inner->grabber.As (&grabber);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Could not get IBaseFilter interface from grabber");
    return FALSE;
  }

  hr = graph->AddFilter (grabber.Get (), L"SampleGrabber");
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Could not add grabber filter to graph");
    return FALSE;
  }

  hr = graph->AddFilter (inner->fakesink.Get (), L"FakeSink");
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Could not add fakesink filter to graph");
    return FALSE;
  }

  hr = capture->EnumPins (&pin_list);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Could not get pin enumerator");
    return FALSE;
  }

  /* Enumerates pins and media types */
  do {
    ComPtr < IPin > pin;
    std::wstring id;
    std::string id_str;
    WCHAR *pin_id = nullptr;
    GUID category = GUID_NULL;
    DWORD returned = 0;

    hr = pin_list->Next (1, &pin, nullptr);
    if (hr != S_OK)
      break;

    hr = pin->QueryDirection (&direction);
    if (!gst_mf_result (hr) || direction != PINDIR_OUTPUT)
      continue;

    hr = pin->QueryId (&pin_id);
    if (!gst_mf_result (hr) || !pin_id)
      continue;

    id_str = convert_to_string (pin_id);
    id = pin_id;
    CoTaskMemFree (pin_id);

    ComPtr < IKsPropertySet > prop;
    hr = pin.As (&prop);
    if (!gst_mf_result (hr))
      continue;

    prop->Get (AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, nullptr, 0,
        &category, sizeof (GUID), &returned);

    if (category == GUID_NULL) {
      GST_INFO_OBJECT (self, "Unknown category, keep checking");
    } else if (category == PIN_CATEGORY_CAPTURE) {
      GST_INFO_OBJECT (self, "Found capture pin");
    } else if (category == PIN_CATEGORY_PREVIEW) {
      GST_INFO_OBJECT (self, "Found preview pin");
    } else {
      continue;
    }

    ComPtr < IAMStreamConfig > config;
    hr = pin.As (&config);
    if (!gst_mf_result (hr))
      continue;

    int count = 0;
    int size = 0;
    hr = config->GetNumberOfCapabilities (&count, &size);
    if (!gst_mf_result (hr) || count == 0 ||
        size != sizeof (VIDEO_STREAM_CONFIG_CAPS)) {
      continue;
    }

    for (int i = 0; i < count; i++) {
      AM_MEDIA_TYPE *type = nullptr;
      VIDEO_STREAM_CONFIG_CAPS config_caps;
      GstCaps *caps;
      gboolean top_down = TRUE;

      hr = config->GetStreamCaps (i, &type, (BYTE *) & config_caps);
      if (!gst_mf_result (hr) || !type) {
        GST_WARNING_OBJECT (self, "Couldn't get caps for index %d", i);
        continue;
      }

      caps = media_type_to_caps (type, &top_down);
      if (!caps) {
        GST_WARNING_OBJECT (self,
            "Couldn't convert type to caps for index %d", i);
        FreeMediaType (type);
        continue;
      }

      GST_LOG_OBJECT (self, "Adding caps for pin id \"%s\", index %d, caps %"
          GST_PTR_FORMAT, id_str.c_str (), i, caps);

      inner->pin_infos.emplace_back (id, caps, i, top_down);
      FreeMediaType (type);
    }
  } while (hr == S_OK);

  if (inner->pin_infos.empty ()) {
    GST_WARNING_OBJECT (self, "Couldn't get pin information");
    return FALSE;
  }

  std::sort (inner->pin_infos.begin (), inner->pin_infos.end ());

  self->supported_caps = gst_caps_new_empty ();
  /* *INDENT-OFF* */
  for (const auto & iter : inner->pin_infos)
    gst_caps_append (self->supported_caps, gst_caps_ref (iter.caps));
  /* *INDENT-ON* */

  GST_DEBUG_OBJECT (self, "Available output caps %" GST_PTR_FORMAT,
      self->supported_caps);

  inner->graph = graph;
  inner->control = control;
  inner->capture = capture;

  return TRUE;
}

static gboolean
gst_mf_dshow_enum_device (GstMFCaptureDShow * self,
    GstMFSourceType source_type, std::vector < GStMFDShowMoniker > &dev_list)
{
  HRESULT hr;
  ComPtr < ICreateDevEnum > dev_enum;
  ComPtr < IEnumMoniker > enum_moniker;

  hr = CoCreateInstance (CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS (&dev_enum));
  if (!gst_mf_result (hr))
    return FALSE;

  switch (source_type) {
    case GST_MF_SOURCE_TYPE_VIDEO:
      /* directshow native filter only */
      hr = dev_enum->CreateClassEnumerator (CLSID_VideoInputDeviceCategory,
          &enum_moniker, CDEF_DEVMON_FILTER);
      break;
    default:
      GST_ERROR_OBJECT (self, "Unknown source type %d", source_type);
      return FALSE;
  }

  // Documentation states that the result of CreateClassEnumerator must be checked against S_OK
  if (hr != S_OK)
    return FALSE;

  for (guint i = 0;; i++) {
    ComPtr < IMoniker > moniker;
    ComPtr < IPropertyBag > prop_bag;
    WCHAR *display_name = nullptr;
    VARIANT var;
    std::string desc;
    std::string name;
    std::string path;

    hr = enum_moniker->Next (1, &moniker, nullptr);
    if (hr != S_OK)
      break;

    hr = moniker->BindToStorage (nullptr, nullptr, IID_PPV_ARGS (&prop_bag));
    if (!gst_mf_result (hr))
      continue;

    VariantInit (&var);
    hr = prop_bag->Read (L"Description", &var, nullptr);
    if (SUCCEEDED (hr)) {
      desc = convert_to_string (var.bstrVal);
      VariantClear (&var);
    }

    hr = prop_bag->Read (L"FriendlyName", &var, nullptr);
    if (SUCCEEDED (hr)) {
      name = convert_to_string (var.bstrVal);
      VariantClear (&var);
    }

    if (desc.empty () && name.empty ()) {
      desc = "Unknown capture device";
      name = "Unknown capture device";
      GST_WARNING_OBJECT (self, "Unknown device desc/name");
    }

    if (desc.empty ())
      desc = name;
    else if (name.empty ())
      name = desc;

    hr = moniker->GetDisplayName (nullptr, nullptr, &display_name);
    if (!gst_mf_result (hr) || !display_name)
      continue;

    path = convert_to_string (display_name);
    CoTaskMemFree (display_name);

    dev_list.push_back ( {
        moniker, desc, name, path, i}
    );
  }

  if (dev_list.empty ())
    return FALSE;

  return TRUE;
}

/* *INDENT-OFF* */
static gpointer
gst_mf_capture_dshow_thread_func (GstMFCaptureDShow * self)
{
  GstMFSourceObject *object = GST_MF_SOURCE_OBJECT (self);
  GSource *source;

  CoInitializeEx (nullptr, COINIT_MULTITHREADED);

  g_main_context_push_thread_default (self->context);

  self->inner = new GstMFCaptureDShowInner ();

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gst_mf_capture_dshow_main_loop_running_cb, self, nullptr);
  g_source_attach (source, self->context);
  g_source_unref (source);

  {
    std::vector<GStMFDShowMoniker> device_list;
    GStMFDShowMoniker selected;
    HRESULT hr;

    if (!gst_mf_dshow_enum_device (self, object->source_type, device_list)) {
      GST_WARNING_OBJECT (self, "No available video capture device");
      goto run_loop;
    }

    for (const auto & iter : device_list) {
      GST_DEBUG_OBJECT (self, "device %d, name: \"%s\", path: \"%s\"",
          iter.index, iter.name.c_str (), iter.path.c_str ());
    }

    GST_DEBUG_OBJECT (self,
        "Requested device index: %d, name: \"%s\", path \"%s\"",
        object->device_index, GST_STR_NULL (object->device_name),
        GST_STR_NULL (object->device_path));

    for (const auto & iter : device_list) {
      bool match = false;

      if (object->device_path) {
        match = (g_ascii_strcasecmp (iter.path.c_str (),
            object->device_path) == 0);
      } else if (object->device_name) {
        match = (g_ascii_strcasecmp (iter.name.c_str (),
            object->device_name) == 0);
      } else if (object->device_index >= 0) {
        match = iter.index == (guint) object->device_index;
      } else {
        /* pick the first entry */
        match = TRUE;
      }

      if (match) {
        selected = iter;
        break;
      }
    }

    if (selected.moniker) {
      ComPtr<ISampleGrabber> grabber;
      ComPtr<IBaseFilter> fakesink;
      ComPtr<IGstMFSampleGrabberCB> cb;

      /* Make sure ISampleGrabber and NullRenderer are available,
       * MS may want to drop the the legacy implementations */
      hr = CoCreateInstance (CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
          IID_PPV_ARGS (&grabber));
      if (!gst_mf_result (hr)) {
        GST_WARNING_OBJECT (self, "ISampleGrabber interface is not available");
        goto run_loop;
      }

      grabber->SetBufferSamples (FALSE);
      grabber->SetOneShot (FALSE);

      hr = IGstMFSampleGrabberCB::CreateInstance (gst_mf_capture_dshow_on_buffer,
        self, &cb);
      if (!gst_mf_result (hr)) {
        GST_WARNING_OBJECT (self, "Could not create callback object");
        goto run_loop;
      }

      hr = grabber->SetCallback (cb.Get (), 1);
      if (!gst_mf_result (hr)) {
        GST_WARNING_OBJECT (self, "Could not set sample callback");
        goto run_loop;
      }

      hr = CoCreateInstance (CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER,
          IID_PPV_ARGS (&fakesink));
      if (!gst_mf_result (hr)) {
        GST_WARNING_OBJECT (self, "NullRenderer interface is not available");
        goto run_loop;
      }

      self->inner->grabber = grabber;
      self->inner->fakesink = fakesink;

      if (!gst_mf_capture_dshow_open (self, selected.moniker.Get ())) {
        object->source_state = GST_MF_ACTIVATION_FAILED;
      } else {
        object->source_state = GST_MF_OK;
        g_free (object->device_path);
        object->device_path = g_strdup (selected.path.c_str());

        g_free (object->device_name);
        object->device_name = g_strdup (selected.name.c_str());

        object->device_index = selected.index;
      }
    } else {
      object->source_state = GST_MF_DEVICE_NOT_FOUND;
    }
  }

run_loop:
  GST_DEBUG_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_DEBUG_OBJECT (self, "Stopped main loop");

  gst_mf_capture_dshow_stop (object);
  delete self->inner;

  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  g_main_context_pop_thread_default (self->context);

  CoUninitialize ();

  return nullptr;
}
/* *INDENT-ON* */

static void
gst_mf_capture_dshow_on_buffer (double sample_time, BYTE * data, LONG len,
    gpointer user_data)
{
  GstMFCaptureDShow *self = GST_MF_CAPTURE_DSHOW (user_data);
  GstFlowReturn ret;
  GstClockTime time;
  GstVideoFrame frame;
  AM_MEDIA_TYPE type;
  HRESULT hr;
  GstBuffer *buf = nullptr;
  GstCaps *caps = nullptr;
  GstSample *sample;

  if (!data) {
    GST_WARNING_OBJECT (self, "Null data");
    return;
  }

  memset (&type, 0, sizeof (AM_MEDIA_TYPE));
  g_mutex_lock (&self->lock);
  if (self->flushing || self->state != CAPTURE_STATE_RUNNING) {
    GST_DEBUG_OBJECT (self, "Not running state");
    g_mutex_unlock (&self->lock);
    return;
  }

  hr = self->inner->grabber->GetConnectedMediaType (&type);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't get connected media type");
    goto error;
  }

  caps = media_type_to_caps (&type, &self->top_down_image);
  ClearMediaType (&type);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Couldn't get caps from connected type");
    goto error;
  }

  if (!gst_caps_is_equal (caps, self->selected_caps)) {
    GstBufferPool *pool;
    GstStructure *pool_config;
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps)) {
      GST_ERROR_OBJECT (self, "Couldn't get video info from caps");
      gst_caps_unref (caps);
      goto error;
    }

    GST_WARNING_OBJECT (self, "Caps change %" GST_PTR_FORMAT " -> %"
        GST_PTR_FORMAT, self->selected_caps, caps);

    gst_clear_caps (&self->selected_caps);
    self->selected_caps = gst_caps_ref (caps);
    self->info = info;

    pool = gst_video_buffer_pool_new ();
    pool_config = gst_buffer_pool_get_config (self->pool);
    gst_buffer_pool_config_add_option (pool_config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_set_params (pool_config, caps,
        GST_VIDEO_INFO_SIZE (&self->info), 0, 0);
    if (!gst_buffer_pool_set_config (pool, pool_config)) {
      GST_ERROR_OBJECT (self, "Couldn not set buffer pool config");
      gst_object_unref (pool);
      goto error;
    }

    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Couldn't activate pool");
      gst_object_unref (pool);
      goto error;
    }

    if (self->pool) {
      gst_buffer_pool_set_active (self->pool, FALSE);
      gst_object_unref (self->pool);
    }

    self->pool = pool;
  } else {
    gst_clear_caps (&caps);
  }

  if (len < GST_VIDEO_INFO_SIZE (&self->info)) {
    GST_ERROR_OBJECT (self, "Too small size %d < %d",
        (gint) len, GST_VIDEO_INFO_SIZE (&self->info));
    goto error;
  }

  time = gst_mf_source_object_get_running_time (GST_MF_SOURCE_OBJECT (self));
  ret = gst_buffer_pool_acquire_buffer (self->pool, &buf, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Could not acquire buffer");
    goto error;
  }

  if (!gst_video_frame_map (&frame, &self->info, buf, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Could not map buffer");
    goto error;
  }

  if (!self->top_down_image) {
    guint8 *src, *dst;
    gint src_stride, dst_stride;
    gint width, height;

    /* must be single plane RGB */
    width = GST_VIDEO_INFO_COMP_WIDTH (&self->info, 0)
        * GST_VIDEO_INFO_COMP_PSTRIDE (&self->info, 0);
    height = GST_VIDEO_INFO_HEIGHT (&self->info);

    src_stride = GST_VIDEO_INFO_PLANE_STRIDE (&self->info, 0);
    dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);

    /* This is bottom up image, should copy lines in reverse order */
    src = data + src_stride * (height - 1);
    dst = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);

    for (guint i = 0; i < height; i++) {
      memcpy (dst, src, width);
      src -= src_stride;
      dst += dst_stride;
    }
  } else {
    for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&self->info); i++) {
      guint8 *src, *dst;
      gint src_stride, dst_stride;
      gint width;

      src = data + GST_VIDEO_INFO_PLANE_OFFSET (&self->info, i);
      dst = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, i);

      src_stride = GST_VIDEO_INFO_PLANE_STRIDE (&self->info, i);
      dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, i);
      width = GST_VIDEO_INFO_COMP_WIDTH (&self->info, i)
          * GST_VIDEO_INFO_COMP_PSTRIDE (&self->info, i);

      for (guint j = 0; j < GST_VIDEO_INFO_COMP_HEIGHT (&self->info, i); j++) {
        memcpy (dst, src, width);
        src += src_stride;
        dst += dst_stride;
      }
    }
  }

  gst_video_frame_unmap (&frame);

  GST_BUFFER_PTS (buf) = time;
  GST_BUFFER_DTS (buf) = GST_CLOCK_TIME_NONE;

  sample = gst_sample_new (buf, caps, nullptr, nullptr);
  gst_clear_caps (&caps);
  gst_buffer_unref (buf);
  g_queue_push_tail (&self->sample_queue, sample);
  /* Drop old buffers */
  while (g_queue_get_length (&self->sample_queue) > 30) {
    sample = (GstSample *) g_queue_pop_head (&self->sample_queue);
    GST_INFO_OBJECT (self, "Dropping old sample %p", sample);
    gst_sample_unref (sample);
  }
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return;

error:
  gst_clear_buffer (&buf);
  gst_clear_caps (&caps);
  self->state = CAPTURE_STATE_ERROR;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);
}

GstMFSourceObject *
gst_mf_capture_dshow_new (GstMFSourceType type, gint device_index,
    const gchar * device_name, const gchar * device_path)
{
  GstMFSourceObject *self;

  g_return_val_if_fail (type == GST_MF_SOURCE_TYPE_VIDEO, nullptr);

  self = (GstMFSourceObject *) g_object_new (GST_TYPE_MF_CAPTURE_DSHOW,
      "source-type", type, "device-index", device_index, "device-name",
      device_name, "device-path", device_path, nullptr);

  gst_object_ref_sink (self);

  if (self->source_state != GST_MF_OK) {
    GST_DEBUG_OBJECT (self, "Couldn't open device");
    gst_object_unref (self);
    return nullptr;
  }

  return self;
}

GstMFSourceResult
gst_mf_capture_dshow_enumerate (gint device_index, GstMFSourceObject ** object)
{
  auto self = (GstMFSourceObject *) g_object_new (GST_TYPE_MF_CAPTURE_DSHOW,
      "source-type", GST_MF_SOURCE_TYPE_VIDEO, "device-index", device_index,
      nullptr);
  gst_object_ref_sink (self);

  auto ret = self->source_state;
  if (ret != GST_MF_OK) {
    gst_object_unref (self);
    return ret;
  }

  *object = self;
  return GST_MF_OK;
}
