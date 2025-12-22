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

#include <gst/base/base.h>
#include <gst/video/video.h>
#include "gstmfcapturewinrt.h"
#include "gstmfutils.h"
#include "mediacapturewrapper.h"
#include <memorybuffer.h>
#include <memory>
#include <algorithm>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Graphics::Imaging;
using namespace ABI::Windows::Foundation;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_mf_source_object_debug);
#define GST_CAT_DEFAULT gst_mf_source_object_debug


enum
{
  PROP_0,
  PROP_DISPATCHER,
};

struct _GstMFCaptureWinRT
{
  GstMFSourceObject parent;

  MediaCaptureWrapper *capture;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  /* protected by lock */
  GstQueueArray *queue;

  GstCaps *supported_caps;
  GstVideoInfo info;
  gboolean flushing;
  gboolean got_error;

  gpointer dispatcher;
};

typedef struct _GstMFCaptureWinRTFrame
{
  IMediaFrameReference *frame;
  GstClockTime clock_time;
} GstMFCaptureWinRTFrame;

static void gst_mf_capture_winrt_constructed (GObject * object);
static void gst_mf_capture_winrt_finalize (GObject * object);
static void gst_mf_capture_winrt_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_mf_capture_winrt_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_mf_capture_winrt_start (GstMFSourceObject * object);
static gboolean gst_mf_capture_winrt_stop (GstMFSourceObject * object);
static GstFlowReturn gst_mf_capture_winrt_fill (GstMFSourceObject * object,
    GstBuffer * buffer);
static gboolean gst_mf_capture_winrt_unlock (GstMFSourceObject * object);
static gboolean gst_mf_capture_winrt_unlock_stop (GstMFSourceObject * object);
static GstCaps *gst_mf_capture_winrt_get_caps (GstMFSourceObject * object);
static gboolean gst_mf_capture_winrt_set_caps (GstMFSourceObject * object,
    GstCaps * caps);
static HRESULT gst_mf_capture_winrt_on_frame (IMediaFrameReference * frame,
    void *user_data);
static HRESULT gst_mf_capture_winrt_on_failed (const std::string & error,
    UINT32 error_code, void *user_data);

static gpointer gst_mf_capture_winrt_thread_func (GstMFCaptureWinRT * self);
static void
gst_mf_capture_winrt_frame_clear (GstMFCaptureWinRTFrame * winrt_frame);

#define gst_mf_capture_winrt_parent_class parent_class
G_DEFINE_TYPE (GstMFCaptureWinRT, gst_mf_capture_winrt,
    GST_TYPE_MF_SOURCE_OBJECT);

static void
gst_mf_capture_winrt_class_init (GstMFCaptureWinRTClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstMFSourceObjectClass *source_class = GST_MF_SOURCE_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_mf_capture_winrt_constructed;
  gobject_class->finalize = gst_mf_capture_winrt_finalize;
  gobject_class->get_property = gst_mf_capture_winrt_get_property;
  gobject_class->set_property = gst_mf_capture_winrt_set_property;

  g_object_class_install_property (gobject_class, PROP_DISPATCHER,
      g_param_spec_pointer ("dispatcher", "Dispatcher",
          "ICoreDispatcher COM object to use",
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));

  source_class->start = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_start);
  source_class->stop = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_stop);
  source_class->fill = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_fill);
  source_class->unlock = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_unlock);
  source_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_unlock_stop);
  source_class->get_caps = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_get_caps);
  source_class->set_caps = GST_DEBUG_FUNCPTR (gst_mf_capture_winrt_set_caps);
}

static void
gst_mf_capture_winrt_init (GstMFCaptureWinRT * self)
{
  self->queue =
      gst_queue_array_new_for_struct (sizeof (GstMFCaptureWinRTFrame), 2);
  gst_queue_array_set_clear_func (self->queue,
      (GDestroyNotify) gst_mf_capture_winrt_frame_clear);
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);
}

static void
gst_mf_capture_winrt_constructed (GObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);

  /* Create a new thread to ensure that COM thread can be MTA thread */
  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstMFCaptureWinRT",
      (GThreadFunc) gst_mf_capture_winrt_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_mf_capture_winrt_finalize (GObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);
  g_main_loop_unref (self->loop);
  g_main_context_unref (self->context);

  gst_queue_array_free (self->queue);
  gst_clear_caps (&self->supported_caps);
  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mf_capture_winrt_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  switch (prop_id) {
    case PROP_DISPATCHER:
      g_value_set_pointer (value, self->dispatcher);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_capture_winrt_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  switch (prop_id) {
    case PROP_DISPATCHER:
      self->dispatcher = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_mf_capture_winrt_main_loop_running_cb (GstMFCaptureWinRT * self)
{
  GST_DEBUG_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
gst_mf_capture_winrt_thread_func (GstMFCaptureWinRT * self)
{
  GstMFSourceObject *source = GST_MF_SOURCE_OBJECT (self);
  HRESULT hr;
  guint index;
  GSource *idle_source;
  std::shared_ptr < GstWinRTMediaFrameSourceGroup > target_group;
  std::vector < GstWinRTMediaFrameSourceGroup > group_list;
  MediaCaptureWrapperCallbacks callbacks;

  RoInitializeWrapper init_wrapper (RO_INIT_MULTITHREADED);

  self->capture = new MediaCaptureWrapper (self->dispatcher);
  callbacks.frame_arrived = gst_mf_capture_winrt_on_frame;
  callbacks.failed = gst_mf_capture_winrt_on_failed;
  self->capture->RegisterCb (callbacks, self);

  g_main_context_push_thread_default (self->context);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) gst_mf_capture_winrt_main_loop_running_cb, self, nullptr);
  g_source_attach (idle_source, self->context);
  g_source_unref (idle_source);

  hr = self->capture->EnumrateFrameSourceGroup (group_list);

  /* *INDENT-OFF* */
#ifndef GST_DISABLE_GST_DEBUG
  index = 0;
  for (const auto& iter: group_list) {
    GST_DEBUG_OBJECT (self, "device %d, name: \"%s\", path: \"%s\"",
        index, iter.display_name_.c_str(), iter.id_.c_str());
    index++;
  }
#endif

  GST_DEBUG_OBJECT (self,
      "Requested device index: %d, name: \"%s\", path \"%s\"",
      source->device_index, GST_STR_NULL (source->device_name),
      GST_STR_NULL (source->device_path));

  index = 0;
  for (const auto& iter: group_list) {
    gboolean match;

    if (source->device_path) {
      match = g_ascii_strcasecmp (iter.id_.c_str(), source->device_path) == 0;
    } else if (source->device_name) {
      match = g_ascii_strcasecmp (iter.display_name_.c_str(),
          source->device_name) == 0;
    } else if (source->device_index >= 0) {
      match = index == source->device_index;
    } else {
      /* pick the first entry */
      match = TRUE;
    }

    if (match) {
      target_group = std::make_shared<GstWinRTMediaFrameSourceGroup>(iter);
      break;
    }

    index++;
  }
  /* *INDENT-ON* */

  if (!target_group) {
    GST_WARNING_OBJECT (self, "No matching device");
    source->source_state = GST_MF_DEVICE_NOT_FOUND;
    goto run_loop;
  }

  if (target_group->source_list_.empty ()) {
    GST_WARNING_OBJECT (self, "No available source list");
    goto run_loop;
  }

  self->capture->SetSourceGroup (*target_group);

  std::sort (target_group->source_list_.begin (),
      target_group->source_list_.end (), WinRTCapsCompareFunc);

  self->supported_caps = gst_caps_new_empty ();

  /* *INDENT-OFF* */
  for (auto iter: target_group->source_list_)
    gst_caps_append (self->supported_caps, gst_caps_copy (iter.caps_));
  /* *INDENT-ON* */

  GST_DEBUG_OBJECT (self, "Available output caps %" GST_PTR_FORMAT,
      self->supported_caps);

  source->source_state = GST_MF_OK;

  g_free (source->device_path);
  source->device_path = g_strdup (target_group->id_.c_str ());

  g_free (source->device_name);
  source->device_name = g_strdup (target_group->display_name_.c_str ());

  source->device_index = index;

run_loop:
  GST_DEBUG_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_DEBUG_OBJECT (self, "Stopped main loop");

  g_main_context_pop_thread_default (self->context);

  gst_mf_capture_winrt_stop (source);

  delete self->capture;
  self->capture = nullptr;

  return nullptr;
}

static gboolean
gst_mf_capture_winrt_start (GstMFSourceObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);
  HRESULT hr;

  if (!self->capture) {
    GST_ERROR_OBJECT (self, "No capture object was configured");
    return FALSE;
  }

  hr = self->capture->StartCapture ();
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Capture object doesn't want to start capture");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mf_capture_winrt_stop (GstMFSourceObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);
  HRESULT hr;

  if (!self->capture) {
    GST_ERROR_OBJECT (self, "No capture object was configured");
    return FALSE;
  }

  hr = self->capture->StopCapture ();

  gst_queue_array_clear (self->queue);

  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Capture object doesn't want to stop capture");
    return FALSE;
  }

  return TRUE;
}

static HRESULT
gst_mf_capture_winrt_on_frame (IMediaFrameReference * frame, void *user_data)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (user_data);
  GstMFCaptureWinRTFrame winrt_frame;

  g_mutex_lock (&self->lock);
  if (self->flushing) {
    g_mutex_unlock (&self->lock);
    return S_OK;
  }

  winrt_frame.frame = frame;
  winrt_frame.clock_time =
      gst_mf_source_object_get_running_time (GST_MF_SOURCE_OBJECT (self));
  gst_queue_array_push_tail_struct (self->queue, &winrt_frame);
  frame->AddRef ();

  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return S_OK;
}

static HRESULT
gst_mf_capture_winrt_on_failed (const std::string & error,
    UINT32 error_code, void *user_data)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (user_data);

  GST_DEBUG_OBJECT (self, "Have error %s (%d)", error.c_str (), error_code);

  g_mutex_lock (&self->lock);
  self->got_error = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return S_OK;
}

static GstFlowReturn
gst_mf_capture_winrt_get_video_media_frame (GstMFCaptureWinRT * self,
    IVideoMediaFrame ** media_frame, GstClockTime * timestamp,
    GstClockTime * duration)
{
  GstMFCaptureWinRTFrame *winrt_frame = nullptr;
  IMediaFrameReference *frame_ref;
  HRESULT hr;
  ComPtr < IReference < TimeSpan >> winrt_timestamp;
  TimeSpan winrt_duration;

  *media_frame = nullptr;
  *timestamp = GST_CLOCK_TIME_NONE;
  *duration = GST_CLOCK_TIME_NONE;

  g_mutex_lock (&self->lock);
  if (self->got_error) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_ERROR;
  }

  if (self->flushing) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_FLUSHING;
  }

  while (!self->flushing && !self->got_error &&
      gst_queue_array_is_empty (self->queue))
    g_cond_wait (&self->cond, &self->lock);

  if (self->got_error) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_ERROR;
  }

  if (self->flushing) {
    g_mutex_unlock (&self->lock);
    return GST_FLOW_FLUSHING;
  }

  winrt_frame =
      (GstMFCaptureWinRTFrame *) gst_queue_array_pop_head_struct (self->queue);

  frame_ref = winrt_frame->frame;
  g_assert (frame_ref);

  hr = frame_ref->get_VideoMediaFrame (media_frame);
  if (!gst_mf_result (hr)) {
    GST_WARNING_OBJECT (self, "Couldn't get IVideoMediaFrame");
    *media_frame = nullptr;
    goto done;
  }

  hr = frame_ref->get_Duration (&winrt_duration);
  if (gst_mf_result (hr))
    *duration = winrt_duration.Duration * 100;

  *timestamp = winrt_frame->clock_time;

done:
  gst_mf_capture_winrt_frame_clear (winrt_frame);
  g_mutex_unlock (&self->lock);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mf_capture_winrt_fill (GstMFSourceObject * object, GstBuffer * buffer)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);
  GstFlowReturn ret = GST_FLOW_OK;
  HRESULT hr;
  GstVideoFrame frame;
  BYTE *data;
  UINT32 size;
  gint i, j;
  ComPtr < IVideoMediaFrame > video_frame;
  ComPtr < ISoftwareBitmap > bitmap;
  ComPtr < IBitmapBuffer > bitmap_buffer;
  ComPtr < IMemoryBuffer > mem_buf;
  ComPtr < IMemoryBufferReference > mem_ref;
  ComPtr < Windows::Foundation::IMemoryBufferByteAccess > byte_access;
  INT32 plane_count;
  BitmapPlaneDescription desc[GST_VIDEO_MAX_PLANES];
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime duration = GST_CLOCK_TIME_NONE;

  do {
    ret = gst_mf_capture_winrt_get_video_media_frame (self,
        video_frame.ReleaseAndGetAddressOf (), &timestamp, &duration);
  } while (ret == GST_FLOW_OK && !video_frame);

  if (ret != GST_FLOW_OK)
    return ret;

  hr = video_frame->get_SoftwareBitmap (&bitmap);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't get ISoftwareBitmap");
    return GST_FLOW_ERROR;
  }

  hr = bitmap->LockBuffer (BitmapBufferAccessMode::BitmapBufferAccessMode_Read,
      &bitmap_buffer);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot lock ISoftwareBitmap");
    return GST_FLOW_ERROR;
  }

  hr = bitmap_buffer->GetPlaneCount (&plane_count);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot get plane count");
    return GST_FLOW_ERROR;
  }

  if (plane_count > GST_VIDEO_MAX_PLANES) {
    GST_ERROR_OBJECT (self, "Invalid plane count %d", plane_count);
    return GST_FLOW_ERROR;
  }

  if (plane_count != GST_VIDEO_INFO_N_PLANES (&self->info)) {
    GST_ERROR_OBJECT (self, "Ambiguous plane count %d", plane_count);
    return GST_FLOW_ERROR;
  }

  for (i = 0; i < plane_count; i++) {
    hr = bitmap_buffer->GetPlaneDescription (i, &desc[i]);
    if (!gst_mf_result (hr)) {
      GST_ERROR_OBJECT (self, "Cannot get description for plane %d", i);
      return GST_FLOW_ERROR;
    }
  }

  hr = bitmap_buffer.As (&mem_buf);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot get IMemoryBuffer");
    return GST_FLOW_ERROR;
  }

  hr = mem_buf->CreateReference (&mem_ref);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot get IMemoryBufferReference");
    return GST_FLOW_ERROR;
  }

  hr = mem_ref.As (&byte_access);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot get IMemoryBufferByteAccess");
    return GST_FLOW_ERROR;
  }

  hr = byte_access->GetBuffer (&data, &size);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Cannot get raw buffer data");
    return GST_FLOW_ERROR;
  }

  if (size < GST_VIDEO_INFO_SIZE (&self->info)) {
    GST_ERROR_OBJECT (self, "Too small buffer size %d", size);
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&frame, &self->info, buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&self->info); i++) {
    guint8 *src, *dst;
    gint src_stride, dst_stride;
    gint width;

    src = data + desc[i].StartIndex;
    dst = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, i);

    src_stride = desc[i].Stride;
    dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, i);
    width = GST_VIDEO_INFO_COMP_WIDTH (&self->info, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (&self->info, i);

    for (j = 0; j < GST_VIDEO_INFO_COMP_HEIGHT (&self->info, i); j++) {
      memcpy (dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }

  gst_video_frame_unmap (&frame);

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = duration;

  return ret;
}

static gboolean
gst_mf_capture_winrt_unlock (GstMFSourceObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  g_mutex_lock (&self->lock);
  if (self->flushing) {
    g_mutex_unlock (&self->lock);
    return TRUE;
  }

  self->flushing = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_mf_capture_winrt_unlock_stop (GstMFSourceObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  g_mutex_lock (&self->lock);
  if (!self->flushing) {
    g_mutex_unlock (&self->lock);
    return TRUE;
  }

  self->flushing = FALSE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static GstCaps *
gst_mf_capture_winrt_get_caps (GstMFSourceObject * object)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);

  if (self->supported_caps)
    return gst_caps_ref (self->supported_caps);

  return nullptr;
}

/* *INDENT-OFF* */
static gboolean
gst_mf_capture_winrt_set_caps (GstMFSourceObject * object, GstCaps * caps)
{
  GstMFCaptureWinRT *self = GST_MF_CAPTURE_WINRT (object);
  std::vector<GstWinRTMediaDescription> desc_list;
  HRESULT hr;
  GstCaps *target_caps = nullptr;

  hr = self->capture->GetAvailableDescriptions(desc_list);
  if (!gst_mf_result (hr) || desc_list.empty()) {
    GST_ERROR_OBJECT (self, "No available media description");
    return FALSE;
  }

  for (const auto& iter: desc_list) {
    if (gst_caps_can_intersect (iter.caps_, caps)) {
      target_caps = gst_caps_ref (iter.caps_);
      self->capture->SetMediaDescription(iter);
      break;
    }
  }

  if (!target_caps) {
    GST_ERROR_OBJECT (self,
        "Could not determine target media type with given caps %"
        GST_PTR_FORMAT, caps);

    return FALSE;
  }

  gst_video_info_from_caps (&self->info, target_caps);
  gst_caps_unref (target_caps);

  return TRUE;
}
/* *INDENT-ON* */

static void
gst_mf_capture_winrt_frame_clear (GstMFCaptureWinRTFrame * winrt_frame)
{
  if (!winrt_frame)
    return;

  if (winrt_frame->frame)
    winrt_frame->frame->Release ();

  winrt_frame->frame = nullptr;
  winrt_frame->clock_time = GST_CLOCK_TIME_NONE;
}

GstMFSourceObject *
gst_mf_capture_winrt_new (GstMFSourceType type, gint device_index,
    const gchar * device_name, const gchar * device_path, gpointer dispatcher)
{
  GstMFSourceObject *self;
  ComPtr < ICoreDispatcher > core_dispatcher;
  /* Multiple COM init is allowed */
  RoInitializeWrapper init_wrapper (RO_INIT_MULTITHREADED);

  /* TODO: Add audio capture support */
  g_return_val_if_fail (type == GST_MF_SOURCE_TYPE_VIDEO, nullptr);

  /* If application didn't pass ICoreDispatcher object,
   * try to get dispatcher object for the current thread */
  if (!dispatcher) {
    HRESULT hr;

    hr = FindCoreDispatcherForCurrentThread (&core_dispatcher);
    if (gst_mf_result (hr)) {
      GST_DEBUG ("UI dispatcher is available");
      dispatcher = core_dispatcher.Get ();
    } else {
      GST_DEBUG ("UI dispatcher is unavailable");
    }
  } else {
    GST_DEBUG ("Use user passed UI dispatcher");
  }

  self = (GstMFSourceObject *) g_object_new (GST_TYPE_MF_CAPTURE_WINRT,
      "source-type", type, "device-index", device_index, "device-name",
      device_name, "device-path", device_path, "dispatcher", dispatcher,
      nullptr);
  gst_object_ref_sink (self);

  /* Reset explicitly to ensure that it happens before
   * RoInitializeWrapper dtor is called */
  core_dispatcher.Reset ();

  if (self->source_state != GST_MF_OK) {
    GST_WARNING_OBJECT (self, "Couldn't open device");
    gst_object_unref (self);
    return nullptr;
  }

  return self;
}

GstMFSourceResult
gst_mf_capture_winrt_enumerate (gint device_index, GstMFSourceObject ** object)
{
  ComPtr < ICoreDispatcher > core_dispatcher;
  /* Multiple COM init is allowed */
  RoInitializeWrapper init_wrapper (RO_INIT_MULTITHREADED);
  FindCoreDispatcherForCurrentThread (&core_dispatcher);

  auto self = (GstMFSourceObject *) g_object_new (GST_TYPE_MF_CAPTURE_WINRT,
      "source-type", GST_MF_SOURCE_TYPE_VIDEO, "device-index", device_index,
      "dispatcher", core_dispatcher.Get (), nullptr);
  gst_object_ref_sink (self);

  auto ret = self->source_state;
  if (ret != GST_MF_OK) {
    gst_object_unref (self);
    return ret;
  }

  *object = self;
  return GST_MF_OK;
}
