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

#include "gstwasapi2ringbuffer.h"
#include <string.h>
#include <mfapi.h>
#include <wrl.h>
#include <memory>
#include <atomic>
#include <vector>

GST_DEBUG_CATEGORY_STATIC (gst_wasapi2_ring_buffer_debug);
#define GST_CAT_DEFAULT gst_wasapi2_ring_buffer_debug

static HRESULT gst_wasapi2_ring_buffer_io_callback (GstWasapi2RingBuffer * buf);
static HRESULT
gst_wasapi2_ring_buffer_loopback_callback (GstWasapi2RingBuffer * buf);

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct GstWasapi2RingBufferPtr
{
  GstWasapi2RingBufferPtr (GstWasapi2RingBuffer * ringbuffer)
      : obj(ringbuffer)
  {
  }

  /* Point to ringbuffer without holding ownership */
  GstWasapi2RingBuffer *obj;
};

class GstWasapiAsyncCallback : public IMFAsyncCallback
{
public:
  GstWasapiAsyncCallback(std::shared_ptr<GstWasapi2RingBufferPtr> listener,
                         DWORD queue_id,
                         gboolean loopback)
    : ref_count_(1)
    , queue_id_(queue_id)
    , listener_(listener)
    , loopback_(loopback)
  {
  }

  /* IUnknown */
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

  STDMETHODIMP
  QueryInterface (REFIID riid, void ** object)
  {
    if (!object)
      return E_POINTER;

    if (riid == IID_IUnknown) {
      GST_TRACE ("query IUnknown interface %p", this);
      *object = static_cast<IUnknown *> (static_cast<GstWasapiAsyncCallback *> (this));
    } else if (riid == __uuidof (IMFAsyncCallback)) {
      GST_TRACE ("query IUnknown interface %p", this);
      *object = static_cast<IUnknown *> (static_cast<GstWasapiAsyncCallback *> (this));
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef ();

    return S_OK;
  }

  /* IMFAsyncCallback */
  STDMETHODIMP
  GetParameters(DWORD * pdwFlags, DWORD * pdwQueue)
  {
    *pdwFlags = 0;
    *pdwQueue = queue_id_;

    return S_OK;
  }

  STDMETHODIMP
  Invoke(IMFAsyncResult * pAsyncResult)
  {
    HRESULT hr;
    auto ptr = listener_.lock ();

    if (!ptr) {
      GST_WARNING ("Listener was removed");
      return S_OK;
    }

    if (loopback_)
      hr = gst_wasapi2_ring_buffer_loopback_callback (ptr->obj);
    else
      hr = gst_wasapi2_ring_buffer_io_callback (ptr->obj);

    return hr;
  }

private:
  ULONG ref_count_;
  DWORD queue_id_;
  std::weak_ptr<GstWasapi2RingBufferPtr> listener_;
  gboolean loopback_;
};

struct GstWasapi2RingBufferPrivate
{
  std::shared_ptr<GstWasapi2RingBufferPtr> obj_ptr;
  std::atomic<bool> monitor_device_mute;
};
/* *INDENT-ON* */

struct _GstWasapi2RingBuffer
{
  GstAudioRingBuffer parent;

  GstWasapi2ClientDeviceClass device_class;
  gchar *device_id;
  gboolean low_latency;
  gboolean mute;
  gdouble volume;
  gpointer dispatcher;
  gboolean can_auto_routing;
  guint loopback_target_pid;

  GstWasapi2Client *client;
  GstWasapi2Client *loopback_client;
  IAudioCaptureClient *capture_client;
  IAudioRenderClient *render_client;
  IAudioStreamVolume *volume_object;

  GstWasapiAsyncCallback *callback_object;
  IMFAsyncResult *callback_result;
  MFWORKITEM_KEY callback_key;
  HANDLE event_handle;

  GstWasapiAsyncCallback *loopback_callback_object;
  IMFAsyncResult *loopback_callback_result;
  MFWORKITEM_KEY loopback_callback_key;
  HANDLE loopback_event_handle;

  guint64 expected_position;
  gboolean is_first;
  gboolean running;
  UINT32 buffer_size;
  UINT32 loopback_buffer_size;

  gint segoffset;
  guint64 write_frame_offset;

  GMutex volume_lock;
  gboolean mute_changed;
  gboolean volume_changed;

  GstCaps *supported_caps;

  GstWasapi2RingBufferPrivate *priv;
};

static void gst_wasapi2_ring_buffer_constructed (GObject * object);
static void gst_wasapi2_ring_buffer_dispose (GObject * object);
static void gst_wasapi2_ring_buffer_finalize (GObject * object);

static gboolean gst_wasapi2_ring_buffer_open_device (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_close_device (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec);
static gboolean gst_wasapi2_ring_buffer_release (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_start (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_resume (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_pause (GstAudioRingBuffer * buf);
static gboolean gst_wasapi2_ring_buffer_stop (GstAudioRingBuffer * buf);
static guint gst_wasapi2_ring_buffer_delay (GstAudioRingBuffer * buf);

#define gst_wasapi2_ring_buffer_parent_class parent_class
G_DEFINE_TYPE (GstWasapi2RingBuffer, gst_wasapi2_ring_buffer,
    GST_TYPE_AUDIO_RING_BUFFER);

static void
gst_wasapi2_ring_buffer_class_init (GstWasapi2RingBufferClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioRingBufferClass *ring_buffer_class =
      GST_AUDIO_RING_BUFFER_CLASS (klass);

  gobject_class->constructed = gst_wasapi2_ring_buffer_constructed;
  gobject_class->dispose = gst_wasapi2_ring_buffer_dispose;
  gobject_class->finalize = gst_wasapi2_ring_buffer_finalize;

  ring_buffer_class->open_device =
      GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_open_device);
  ring_buffer_class->close_device =
      GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_close_device);
  ring_buffer_class->acquire =
      GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_acquire);
  ring_buffer_class->release =
      GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_release);
  ring_buffer_class->start = GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_start);
  ring_buffer_class->resume =
      GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_resume);
  ring_buffer_class->pause = GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_pause);
  ring_buffer_class->stop = GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_stop);
  ring_buffer_class->delay = GST_DEBUG_FUNCPTR (gst_wasapi2_ring_buffer_delay);

  GST_DEBUG_CATEGORY_INIT (gst_wasapi2_ring_buffer_debug,
      "wasapi2ringbuffer", 0, "wasapi2ringbuffer");
}

static void
gst_wasapi2_ring_buffer_init (GstWasapi2RingBuffer * self)
{
  self->volume = 1.0f;
  self->mute = FALSE;

  self->event_handle = CreateEvent (nullptr, FALSE, FALSE, nullptr);
  self->loopback_event_handle = CreateEvent (nullptr, FALSE, FALSE, nullptr);
  g_mutex_init (&self->volume_lock);

  self->priv = new GstWasapi2RingBufferPrivate ();
  self->priv->obj_ptr = std::make_shared < GstWasapi2RingBufferPtr > (self);
  self->priv->monitor_device_mute.store (false, std::memory_order_release);
}

static void
gst_wasapi2_ring_buffer_constructed (GObject * object)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (object);
  HRESULT hr;
  DWORD task_id = 0;
  DWORD queue_id = 0;

  hr = MFLockSharedWorkQueue (L"Pro Audio", 0, &task_id, &queue_id);
  if (!gst_wasapi2_result (hr)) {
    GST_WARNING_OBJECT (self, "Failed to get work queue id");
    goto out;
  }

  self->callback_object = new GstWasapiAsyncCallback (self->priv->obj_ptr,
      queue_id, FALSE);
  hr = MFCreateAsyncResult (nullptr, self->callback_object, nullptr,
      &self->callback_result);
  if (!gst_wasapi2_result (hr)) {
    GST_WARNING_OBJECT (self, "Failed to create IAsyncResult");
    GST_WASAPI2_CLEAR_COM (self->callback_object);
  }

  /* Create another callback object for loopback silence feed */
  self->loopback_callback_object =
      new GstWasapiAsyncCallback (self->priv->obj_ptr, queue_id, TRUE);
  hr = MFCreateAsyncResult (nullptr, self->loopback_callback_object, nullptr,
      &self->loopback_callback_result);
  if (!gst_wasapi2_result (hr)) {
    GST_WARNING_OBJECT (self, "Failed to create IAsyncResult");
    GST_WASAPI2_CLEAR_COM (self->callback_object);
    GST_WASAPI2_CLEAR_COM (self->callback_result);
    GST_WASAPI2_CLEAR_COM (self->loopback_callback_object);
  }

out:
  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_wasapi2_ring_buffer_dispose (GObject * object)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (object);

  self->priv->obj_ptr = nullptr;

  GST_WASAPI2_CLEAR_COM (self->render_client);
  GST_WASAPI2_CLEAR_COM (self->capture_client);
  GST_WASAPI2_CLEAR_COM (self->volume_object);
  GST_WASAPI2_CLEAR_COM (self->callback_result);
  GST_WASAPI2_CLEAR_COM (self->callback_object);
  GST_WASAPI2_CLEAR_COM (self->loopback_callback_result);
  GST_WASAPI2_CLEAR_COM (self->loopback_callback_object);

  gst_clear_object (&self->client);
  gst_clear_object (&self->loopback_client);
  gst_clear_caps (&self->supported_caps);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_wasapi2_ring_buffer_finalize (GObject * object)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (object);

  g_free (self->device_id);
  CloseHandle (self->event_handle);
  CloseHandle (self->loopback_event_handle);
  g_mutex_clear (&self->volume_lock);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wasapi2_ring_buffer_post_open_error (GstWasapi2RingBuffer * self)
{
  GstElement *parent = (GstElement *) GST_OBJECT_PARENT (self);

  if (!parent) {
    GST_WARNING_OBJECT (self, "Cannot find parent");
    return;
  }

  if (self->device_class == GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER) {
    GST_ELEMENT_ERROR (parent, RESOURCE, OPEN_WRITE,
        (nullptr), ("Failed to open device"));
  } else {
    GST_ELEMENT_ERROR (parent, RESOURCE, OPEN_READ,
        (nullptr), ("Failed to open device"));
  }
}

static void
gst_wasapi2_ring_buffer_post_scheduling_error (GstWasapi2RingBuffer * self)
{
  GstElement *parent = (GstElement *) GST_OBJECT_PARENT (self);

  if (!parent) {
    GST_WARNING_OBJECT (self, "Cannot find parent");
    return;
  }

  GST_ELEMENT_ERROR (parent, RESOURCE, FAILED,
      (nullptr), ("Failed to schedule next I/O"));
}

static void
gst_wasapi2_ring_buffer_post_io_error (GstWasapi2RingBuffer * self, HRESULT hr)
{
  GstElement *parent = (GstElement *) GST_OBJECT_PARENT (self);
  gchar *error_msg;

  if (!parent) {
    GST_WARNING_OBJECT (self, "Cannot find parent");
    return;
  }

  error_msg = gst_wasapi2_util_get_error_message (hr);

  GST_ERROR_OBJECT (self, "Posting I/O error %s (hr: 0x%x)", error_msg, hr);
  if (self->device_class == GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER) {
    GST_ELEMENT_ERROR (parent, RESOURCE, WRITE,
        ("Failed to write to device"), ("%s, hr: 0x%x", error_msg, hr));
  } else {
    GST_ELEMENT_ERROR (parent, RESOURCE, READ,
        ("Failed to read from device"), ("%s hr: 0x%x", error_msg, hr));
  }

  g_free (error_msg);
}

static gboolean
gst_wasapi2_ring_buffer_open_device (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Open");

  if (self->client) {
    GST_DEBUG_OBJECT (self, "Already opened");
    return TRUE;
  }

  self->client = gst_wasapi2_client_new (self->device_class,
      -1, self->device_id, self->loopback_target_pid, self->dispatcher);
  if (!self->client) {
    gst_wasapi2_ring_buffer_post_open_error (self);
    return FALSE;
  }

  g_object_get (self->client, "auto-routing", &self->can_auto_routing, nullptr);

  /* Open another render client to feed silence */
  if (gst_wasapi2_device_class_is_loopback (self->device_class)) {
    self->loopback_client =
        gst_wasapi2_client_new (GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER,
        -1, self->device_id, 0, self->dispatcher);

    if (!self->loopback_client) {
      gst_wasapi2_ring_buffer_post_open_error (self);
      gst_clear_object (&self->client);

      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_wasapi2_ring_buffer_close_device_internal (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Close device");

  if (self->running)
    gst_wasapi2_ring_buffer_stop (buf);

  GST_WASAPI2_CLEAR_COM (self->capture_client);
  GST_WASAPI2_CLEAR_COM (self->render_client);

  g_mutex_lock (&self->volume_lock);
  GST_WASAPI2_CLEAR_COM (self->volume_object);
  g_mutex_unlock (&self->volume_lock);

  gst_clear_object (&self->client);
  gst_clear_object (&self->loopback_client);

  return TRUE;
}

static gboolean
gst_wasapi2_ring_buffer_close_device (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Close");

  gst_wasapi2_ring_buffer_close_device_internal (buf);

  gst_clear_caps (&self->supported_caps);

  return TRUE;
}

static HRESULT
gst_wasapi2_ring_buffer_read (GstWasapi2RingBuffer * self)
{
  GstAudioRingBuffer *ringbuffer = GST_AUDIO_RING_BUFFER_CAST (self);
  BYTE *data = nullptr;
  UINT32 to_read = 0;
  guint32 to_read_bytes;
  DWORD flags = 0;
  HRESULT hr;
  guint64 position;
  GstAudioInfo *info = &ringbuffer->spec.info;
  IAudioCaptureClient *capture_client = self->capture_client;
  guint gap_size = 0;
  guint offset = 0;
  gint segment;
  guint8 *readptr;
  gint len;
  bool is_device_muted;

  if (!capture_client) {
    GST_ERROR_OBJECT (self, "IAudioCaptureClient is not available");
    return E_FAIL;
  }

  hr = capture_client->GetBuffer (&data, &to_read, &flags, &position, nullptr);
  if (hr == AUDCLNT_S_BUFFER_EMPTY || to_read == 0) {
    GST_LOG_OBJECT (self, "Empty buffer");
    to_read = 0;
    goto out;
  }

  is_device_muted =
      self->priv->monitor_device_mute.load (std::memory_order_acquire) &&
      gst_wasapi2_client_is_endpoint_muted (self->client);

  to_read_bytes = to_read * GST_AUDIO_INFO_BPF (info);

  GST_LOG_OBJECT (self, "Reading %d frames offset at %" G_GUINT64_FORMAT
      ", expected position %" G_GUINT64_FORMAT, to_read, position,
      self->expected_position);

  /* XXX: position might not be increased in case of process loopback  */
  if (!gst_wasapi2_device_class_is_process_loopback (self->device_class)) {
    if (self->is_first) {
      self->expected_position = position + to_read;
      self->is_first = FALSE;
    } else {
      if (position > self->expected_position) {
        guint gap_frames;

        gap_frames = (guint) (position - self->expected_position);
        GST_WARNING_OBJECT (self, "Found %u frames gap", gap_frames);
        gap_size = gap_frames * GST_AUDIO_INFO_BPF (info);
      }

      self->expected_position = position + to_read;
    }
  } else if (self->mute) {
    /* volume clinet might not be available in case of process loopback */
    flags |= AUDCLNT_BUFFERFLAGS_SILENT;
  }

  /* Fill gap data if any */
  while (gap_size > 0) {
    if (!gst_audio_ring_buffer_prepare_read (ringbuffer,
            &segment, &readptr, &len)) {
      GST_INFO_OBJECT (self, "No segment available");
      goto out;
    }

    g_assert (self->segoffset >= 0);

    len -= self->segoffset;
    if (len > gap_size)
      len = gap_size;

    gst_audio_format_info_fill_silence (ringbuffer->spec.info.finfo,
        readptr + self->segoffset, len);

    self->segoffset += len;
    gap_size -= len;

    if (self->segoffset == ringbuffer->spec.segsize) {
      gst_audio_ring_buffer_advance (ringbuffer, 1);
      self->segoffset = 0;
    }
  }

  while (to_read_bytes) {
    if (!gst_audio_ring_buffer_prepare_read (ringbuffer,
            &segment, &readptr, &len)) {
      GST_INFO_OBJECT (self, "No segment available");
      goto out;
    }

    len -= self->segoffset;
    if (len > to_read_bytes)
      len = to_read_bytes;

    if (((flags & AUDCLNT_BUFFERFLAGS_SILENT) == AUDCLNT_BUFFERFLAGS_SILENT) ||
        is_device_muted) {
      gst_audio_format_info_fill_silence (ringbuffer->spec.info.finfo,
          readptr + self->segoffset, len);
    } else {
      memcpy (readptr + self->segoffset, data + offset, len);
    }

    self->segoffset += len;
    offset += len;
    to_read_bytes -= len;

    if (self->segoffset == ringbuffer->spec.segsize) {
      gst_audio_ring_buffer_advance (ringbuffer, 1);
      self->segoffset = 0;
    }
  }

out:
  hr = capture_client->ReleaseBuffer (to_read);
  /* For debugging */
  gst_wasapi2_result (hr);

  return hr;
}

static HRESULT
gst_wasapi2_ring_buffer_write (GstWasapi2RingBuffer * self, gboolean preroll)
{
  GstAudioRingBuffer *ringbuffer = GST_AUDIO_RING_BUFFER_CAST (self);
  HRESULT hr;
  IAudioClient *client_handle;
  IAudioRenderClient *render_client;
  guint32 padding_frames = 0;
  guint32 can_write;
  guint32 can_write_bytes;
  gint segment;
  guint8 *readptr;
  gint len;
  BYTE *data = nullptr;

  client_handle = gst_wasapi2_client_get_handle (self->client);
  if (!client_handle) {
    GST_ERROR_OBJECT (self, "IAudioClient is not available");
    return E_FAIL;
  }

  render_client = self->render_client;
  if (!render_client) {
    GST_ERROR_OBJECT (self, "IAudioRenderClient is not available");
    return E_FAIL;
  }

  hr = client_handle->GetCurrentPadding (&padding_frames);
  if (!gst_wasapi2_result (hr))
    return hr;

  if (padding_frames >= self->buffer_size) {
    GST_INFO_OBJECT (self,
        "Padding size %d is larger than or equal to buffer size %d",
        padding_frames, self->buffer_size);
    return S_OK;
  }

  can_write = self->buffer_size - padding_frames;
  can_write_bytes = can_write * GST_AUDIO_INFO_BPF (&ringbuffer->spec.info);
  if (preroll) {
    GST_INFO_OBJECT (self, "Pre-fill %d frames with silence", can_write);

    hr = render_client->GetBuffer (can_write, &data);
    if (!gst_wasapi2_result (hr))
      return hr;

    hr = render_client->ReleaseBuffer (can_write, AUDCLNT_BUFFERFLAGS_SILENT);
    return gst_wasapi2_result (hr);
  }

  GST_LOG_OBJECT (self, "Writing %d frames offset at %" G_GUINT64_FORMAT,
      can_write, self->write_frame_offset);
  self->write_frame_offset += can_write;

  while (can_write_bytes > 0) {
    if (!gst_audio_ring_buffer_prepare_read (ringbuffer,
            &segment, &readptr, &len)) {
      GST_INFO_OBJECT (self, "No segment available, fill silence");

      /* This would be case where in the middle of PAUSED state change.
       * Just fill silent buffer to avoid immediate I/O callback after
       * we return here */
      hr = render_client->GetBuffer (can_write, &data);
      if (!gst_wasapi2_result (hr))
        return hr;

      hr = render_client->ReleaseBuffer (can_write, AUDCLNT_BUFFERFLAGS_SILENT);
      /* for debugging */
      gst_wasapi2_result (hr);
      return hr;
    }

    len -= self->segoffset;

    if (len > can_write_bytes)
      len = can_write_bytes;

    can_write = len / GST_AUDIO_INFO_BPF (&ringbuffer->spec.info);
    if (can_write == 0)
      break;

    hr = render_client->GetBuffer (can_write, &data);
    if (!gst_wasapi2_result (hr))
      return hr;

    memcpy (data, readptr + self->segoffset, len);
    hr = render_client->ReleaseBuffer (can_write, 0);

    self->segoffset += len;
    can_write_bytes -= len;

    if (self->segoffset == ringbuffer->spec.segsize) {
      gst_audio_ring_buffer_clear (ringbuffer, segment);
      gst_audio_ring_buffer_advance (ringbuffer, 1);
      self->segoffset = 0;
    }

    if (!gst_wasapi2_result (hr)) {
      GST_WARNING_OBJECT (self, "Failed to release buffer");
      break;
    }
  }

  return S_OK;
}

static HRESULT
gst_wasapi2_ring_buffer_io_callback (GstWasapi2RingBuffer * self)
{
  HRESULT hr = E_FAIL;

  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (self), E_FAIL);

  if (!self->running) {
    GST_INFO_OBJECT (self, "We are not running now");
    return S_OK;
  }

  switch (self->device_class) {
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_CAPTURE:
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE:
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_INCLUDE_PROCESS_LOOPBACK_CAPTURE:
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_EXCLUDE_PROCESS_LOOPBACK_CAPTURE:
      hr = gst_wasapi2_ring_buffer_read (self);
      break;
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER:
      hr = gst_wasapi2_ring_buffer_write (self, FALSE);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  /* We can ignore errors for device unplugged event if client can support
   * automatic stream routing, but except for loopback capture.
   * loopback capture client doesn't seem to be able to recover status from this
   * situation */
  if (self->can_auto_routing &&
      !gst_wasapi2_device_class_is_loopback (self->device_class) &&
      !gst_wasapi2_device_class_is_process_loopback (self->device_class) &&
      (hr == AUDCLNT_E_ENDPOINT_CREATE_FAILED
          || hr == AUDCLNT_E_DEVICE_INVALIDATED)) {
    GST_WARNING_OBJECT (self,
        "Device was unplugged but client can support automatic routing");
    hr = S_OK;
  }

  if (self->running) {
    if (gst_wasapi2_result (hr) &&
        /* In case of normal loopback capture, this method is called from
         * silence feeding thread. Don't schedule again in that case */
        self->device_class != GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE) {
      hr = MFPutWaitingWorkItem (self->event_handle, 0, self->callback_result,
          &self->callback_key);

      if (!gst_wasapi2_result (hr)) {
        GST_ERROR_OBJECT (self, "Failed to put item");
        gst_wasapi2_ring_buffer_post_scheduling_error (self);

        return hr;
      }
    }
  } else {
    GST_INFO_OBJECT (self, "We are not running now");
    return S_OK;
  }

  if (FAILED (hr))
    gst_wasapi2_ring_buffer_post_io_error (self, hr);

  return hr;
}

static HRESULT
gst_wasapi2_ring_buffer_fill_loopback_silence (GstWasapi2RingBuffer * self)
{
  HRESULT hr;
  IAudioClient *client_handle;
  IAudioRenderClient *render_client;
  guint32 padding_frames = 0;
  guint32 can_write;
  BYTE *data = nullptr;

  client_handle = gst_wasapi2_client_get_handle (self->loopback_client);
  if (!client_handle) {
    GST_ERROR_OBJECT (self, "IAudioClient is not available");
    return E_FAIL;
  }

  render_client = self->render_client;
  if (!render_client) {
    GST_ERROR_OBJECT (self, "IAudioRenderClient is not available");
    return E_FAIL;
  }

  hr = client_handle->GetCurrentPadding (&padding_frames);
  if (!gst_wasapi2_result (hr))
    return hr;

  if (padding_frames >= self->loopback_buffer_size) {
    GST_INFO_OBJECT (self,
        "Padding size %d is larger than or equal to buffer size %d",
        padding_frames, self->loopback_buffer_size);
    return S_OK;
  }

  can_write = self->loopback_buffer_size - padding_frames;

  GST_TRACE_OBJECT (self, "Writing %d silent frames", can_write);

  hr = render_client->GetBuffer (can_write, &data);
  if (!gst_wasapi2_result (hr))
    return hr;

  hr = render_client->ReleaseBuffer (can_write, AUDCLNT_BUFFERFLAGS_SILENT);
  return gst_wasapi2_result (hr);
}

static HRESULT
gst_wasapi2_ring_buffer_loopback_callback (GstWasapi2RingBuffer * self)
{
  HRESULT hr = E_FAIL;

  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (self), E_FAIL);
  g_return_val_if_fail (gst_wasapi2_device_class_is_loopback
      (self->device_class), E_FAIL);

  if (!self->running) {
    GST_INFO_OBJECT (self, "We are not running now");
    return S_OK;
  }

  hr = gst_wasapi2_ring_buffer_fill_loopback_silence (self);

  /* On Windows versions prior to Windows 10, a pull-mode capture client will
   * not receive any events when a stream is initialized with event-driven
   * buffering */
  if (gst_wasapi2_result (hr))
    hr = gst_wasapi2_ring_buffer_io_callback (self);

  if (self->running) {
    if (gst_wasapi2_result (hr)) {
      hr = MFPutWaitingWorkItem (self->loopback_event_handle, 0,
          self->loopback_callback_result, &self->loopback_callback_key);

      if (!gst_wasapi2_result (hr)) {
        GST_ERROR_OBJECT (self, "Failed to put item");
        gst_wasapi2_ring_buffer_post_scheduling_error (self);

        return hr;
      }
    }
  } else {
    GST_INFO_OBJECT (self, "We are not running now");
    return S_OK;
  }

  if (FAILED (hr))
    gst_wasapi2_ring_buffer_post_io_error (self, hr);

  return hr;
}

static HRESULT
gst_wasapi2_ring_buffer_initialize_audio_client3 (GstWasapi2RingBuffer * self,
    IAudioClient * client_handle, WAVEFORMATEX * mix_format, guint * period)
{
  HRESULT hr = S_OK;
  UINT32 default_period, fundamental_period, min_period, max_period;
  /* AUDCLNT_STREAMFLAGS_NOPERSIST is not allowed for
   * InitializeSharedAudioStream */
  DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  ComPtr < IAudioClient3 > audio_client;

  hr = client_handle->QueryInterface (IID_PPV_ARGS (&audio_client));
  if (!gst_wasapi2_result (hr)) {
    GST_INFO_OBJECT (self, "IAudioClient3 interface is unavailable");
    return hr;
  }

  hr = audio_client->GetSharedModeEnginePeriod (mix_format,
      &default_period, &fundamental_period, &min_period, &max_period);
  if (!gst_wasapi2_result (hr)) {
    GST_INFO_OBJECT (self, "Couldn't get period");
    return hr;
  }

  GST_INFO_OBJECT (self, "Using IAudioClient3, default period %d frames, "
      "fundamental period %d frames, minimum period %d frames, maximum period "
      "%d frames", default_period, fundamental_period, min_period, max_period);

  *period = min_period;

  hr = audio_client->InitializeSharedAudioStream (stream_flags, min_period,
      mix_format, nullptr);

  if (!gst_wasapi2_result (hr))
    GST_WARNING_OBJECT (self, "Failed to initialize IAudioClient3");

  return hr;
}

static HRESULT
gst_wasapi2_ring_buffer_initialize_audio_client (GstWasapi2RingBuffer * self,
    IAudioClient * client_handle, WAVEFORMATEX * mix_format, guint * period,
    DWORD extra_flags, GstWasapi2ClientDeviceClass device_class,
    GstAudioRingBufferSpec * spec, gboolean low_latency)
{
  GstAudioRingBuffer *ringbuffer = GST_AUDIO_RING_BUFFER_CAST (self);
  REFERENCE_TIME default_period, min_period;
  DWORD stream_flags =
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST;
  HRESULT hr;
  REFERENCE_TIME buf_dur = 0;

  stream_flags |= extra_flags;

  if (!gst_wasapi2_device_class_is_process_loopback (device_class)) {
    hr = client_handle->GetDevicePeriod (&default_period, &min_period);
    if (!gst_wasapi2_result (hr)) {
      GST_WARNING_OBJECT (self, "Couldn't get device period info");
      return hr;
    }

    GST_INFO_OBJECT (self, "wasapi2 default period: %" G_GINT64_FORMAT
        ", min period: %" G_GINT64_FORMAT, default_period, min_period);

    /* https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
     * For a shared-mode stream that uses event-driven buffering,
     * the caller must set both hnsPeriodicity and hnsBufferDuration to 0
     *
     * The above MS documentation does not seem to correct. By setting
     * zero hnsBufferDuration, we can use audio engine determined buffer size
     * but it seems to cause glitch depending on device. Calculate buffer size
     * like wasapi plugin does. Note that MS example code uses non-zero
     * buffer duration for event-driven shared-mode case as well.
     */
    if (spec && !low_latency) {
      /* Ensure that the period (latency_time) used is an integral multiple of
       * either the default period or the minimum period */
      guint64 factor = (spec->latency_time * 10) / default_period;
      REFERENCE_TIME period = default_period * MAX (factor, 1);

      buf_dur = spec->buffer_time * 10;
      if (buf_dur < 2 * period)
        buf_dur = 2 * period;
    }

    hr = client_handle->Initialize (AUDCLNT_SHAREMODE_SHARED, stream_flags,
        buf_dur,
        /* This must always be 0 in shared mode */
        0, mix_format, nullptr);
  } else {
    /* XXX: virtual device will not report device period.
     * Use hardcoded period 20ms, same as Microsoft sample code
     * https://github.com/microsoft/windows-classic-samples/tree/main/Samples/ApplicationLoopback
     */
    default_period = (20 * GST_MSECOND) / 100;
    hr = client_handle->Initialize (AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        default_period,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, mix_format, nullptr);
  }

  if (!gst_wasapi2_result (hr)) {
    GST_WARNING_OBJECT (self, "Couldn't initialize audioclient");
    return hr;
  }

  *period = gst_util_uint64_scale_round (default_period * 100,
      GST_AUDIO_INFO_RATE (&ringbuffer->spec.info), GST_SECOND);

  return S_OK;
}

static gboolean
gst_wasapi2_ring_buffer_prepare_loopback_client (GstWasapi2RingBuffer * self)
{
  IAudioClient *client_handle;
  HRESULT hr;
  WAVEFORMATEX *mix_format = nullptr;
  guint period = 0;
  ComPtr < IAudioRenderClient > render_client;

  if (!self->loopback_client) {
    GST_ERROR_OBJECT (self, "No configured client object");
    return FALSE;
  }

  if (!gst_wasapi2_client_ensure_activation (self->loopback_client)) {
    GST_ERROR_OBJECT (self, "Failed to activate audio client");
    return FALSE;
  }

  client_handle = gst_wasapi2_client_get_handle (self->loopback_client);
  if (!client_handle) {
    GST_ERROR_OBJECT (self, "IAudioClient handle is not available");
    return FALSE;
  }

  hr = client_handle->GetMixFormat (&mix_format);
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to get mix format");
    return FALSE;
  }

  hr = gst_wasapi2_ring_buffer_initialize_audio_client (self, client_handle,
      mix_format, &period, 0, GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER,
      nullptr, FALSE);
  CoTaskMemFree (mix_format);

  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to initialize audio client");
    return FALSE;
  }

  hr = client_handle->SetEventHandle (self->loopback_event_handle);
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to set event handle");
    return FALSE;
  }

  hr = client_handle->GetBufferSize (&self->loopback_buffer_size);
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to query buffer size");
    return FALSE;
  }

  hr = client_handle->GetService (IID_PPV_ARGS (&render_client));
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "IAudioRenderClient is unavailable");
    return FALSE;
  }

  self->render_client = render_client.Detach ();

  return TRUE;
}

static HRESULT
gst_wasapi2_ring_buffer_set_channel_volumes (IAudioStreamVolume * iface,
    float volume)
{
  float target;
  HRESULT hr = S_OK;

  if (!iface)
    return hr;

  target = CLAMP (volume, 0.0f, 1.0f);
  UINT32 channel_count = 0;
  hr = iface->GetChannelCount (&channel_count);
  if (!gst_wasapi2_result (hr) || channel_count == 0)
    return hr;

  std::vector < float >volumes;
  for (guint i = 0; i < channel_count; i++)
    volumes.push_back (target);

  return iface->SetAllVolumes (channel_count, &volumes[0]);
}

static gboolean
gst_wasapi2_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);
  IAudioClient *client_handle;
  HRESULT hr;
  WAVEFORMATEX *mix_format = nullptr;
  ComPtr < IAudioStreamVolume > audio_volume;
  GstAudioChannelPosition *position = nullptr;
  guint period = 0;
  gint segtotal = 2;

  GST_DEBUG_OBJECT (buf, "Acquire");

  if (!self->client && !gst_wasapi2_ring_buffer_open_device (buf))
    return FALSE;

  if (gst_wasapi2_device_class_is_loopback (self->device_class)) {
    if (!gst_wasapi2_ring_buffer_prepare_loopback_client (self)) {
      GST_ERROR_OBJECT (self, "Failed to prepare loopback client");
      goto error;
    }
  }

  if (!gst_wasapi2_client_ensure_activation (self->client)) {
    GST_ERROR_OBJECT (self, "Failed to activate audio client");
    goto error;
  }

  client_handle = gst_wasapi2_client_get_handle (self->client);
  if (!client_handle) {
    GST_ERROR_OBJECT (self, "IAudioClient handle is not available");
    goto error;
  }

  /* TODO: convert given caps to mix format */
  hr = client_handle->GetMixFormat (&mix_format);
  if (!gst_wasapi2_result (hr)) {
    if (gst_wasapi2_device_class_is_process_loopback (self->device_class)) {
      mix_format = gst_wasapi2_get_default_mix_format ();
    } else {
      GST_ERROR_OBJECT (self, "Failed to get mix format");
      goto error;
    }
  }

  /* Only use audioclient3 when low-latency is requested because otherwise
   * very slow machines and VMs with 1 CPU allocated will get glitches:
   * https://bugzilla.gnome.org/show_bug.cgi?id=794497 */
  hr = E_FAIL;
  if (self->low_latency &&
      /* AUDCLNT_STREAMFLAGS_LOOPBACK is not allowed for
       * InitializeSharedAudioStream */
      !gst_wasapi2_device_class_is_loopback (self->device_class) &&
      !gst_wasapi2_device_class_is_process_loopback (self->device_class)) {
    hr = gst_wasapi2_ring_buffer_initialize_audio_client3 (self, client_handle,
        mix_format, &period);
  }

  /* Try again if IAudioClinet3 API is unavailable.
   * NOTE: IAudioClinet3:: methods might not be available for default device
   * NOTE: The default device is a special device which is needed for supporting
   * automatic stream routing
   * https://docs.microsoft.com/en-us/windows/win32/coreaudio/automatic-stream-routing
   */
  if (FAILED (hr)) {
    DWORD extra_flags = 0;
    if (gst_wasapi2_device_class_is_loopback (self->device_class))
      extra_flags = AUDCLNT_STREAMFLAGS_LOOPBACK;

    hr = gst_wasapi2_ring_buffer_initialize_audio_client (self, client_handle,
        mix_format, &period, extra_flags, self->device_class, spec,
        self->low_latency);
  }

  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to initialize audio client");
    goto error;
  }

  hr = client_handle->SetEventHandle (self->event_handle);
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to set event handle");
    goto error;
  }

  gst_wasapi2_util_waveformatex_to_channel_mask (mix_format, &position);
  if (position)
    gst_audio_ring_buffer_set_channel_positions (buf, position);
  g_free (position);

  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to init audio client");
    goto error;
  }

  hr = client_handle->GetBufferSize (&self->buffer_size);
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to query buffer size");
    goto error;
  }

  g_assert (period > 0);

  spec->segsize = period * GST_AUDIO_INFO_BPF (&buf->spec.info);
  segtotal = (self->buffer_size / period);
  spec->segtotal = MAX (segtotal, 2);

  GST_INFO_OBJECT (self,
      "Buffer size: %d frames, period: %d frames, segsize: %d bytes, "
      "segtotal: %d", self->buffer_size, period, spec->segsize, spec->segtotal);

  if (self->device_class == GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER) {
    ComPtr < IAudioRenderClient > render_client;

    hr = client_handle->GetService (IID_PPV_ARGS (&render_client));
    if (!gst_wasapi2_result (hr)) {
      GST_ERROR_OBJECT (self, "IAudioRenderClient is unavailable");
      goto error;
    }

    self->render_client = render_client.Detach ();
  } else {
    ComPtr < IAudioCaptureClient > capture_client;

    hr = client_handle->GetService (IID_PPV_ARGS (&capture_client));
    if (!gst_wasapi2_result (hr)) {
      GST_ERROR_OBJECT (self, "IAudioCaptureClient is unavailable");
      goto error;
    }

    self->capture_client = capture_client.Detach ();
  }

  hr = client_handle->GetService (IID_PPV_ARGS (&audio_volume));
  if (!gst_wasapi2_result (hr)) {
    GST_WARNING_OBJECT (self, "ISimpleAudioVolume is unavailable");
  } else {
    g_mutex_lock (&self->volume_lock);
    self->volume_object = audio_volume.Detach ();
    float volume = (float) self->volume;
    if (self->mute)
      volume = 0.0f;

    gst_wasapi2_ring_buffer_set_channel_volumes (self->volume_object, volume);

    self->mute_changed = FALSE;
    self->volume_changed = FALSE;
    g_mutex_unlock (&self->volume_lock);
  }

  buf->size = spec->segtotal * spec->segsize;
  buf->memory = (guint8 *) g_malloc (buf->size);
  gst_audio_format_info_fill_silence (buf->spec.info.finfo,
      buf->memory, buf->size);

  CoTaskMemFree (mix_format);

  return TRUE;

error:
  GST_WASAPI2_CLEAR_COM (self->render_client);
  GST_WASAPI2_CLEAR_COM (self->capture_client);
  GST_WASAPI2_CLEAR_COM (self->volume_object);
  CoTaskMemFree (mix_format);

  gst_wasapi2_ring_buffer_post_open_error (self);

  return FALSE;
}

static gboolean
gst_wasapi2_ring_buffer_release (GstAudioRingBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "Release");

  g_clear_pointer (&buf->memory, g_free);

  /* IAudioClient handle is not reusable once it's initialized */
  gst_wasapi2_ring_buffer_close_device_internal (buf);

  return TRUE;
}

static gboolean
gst_wasapi2_ring_buffer_start_internal (GstWasapi2RingBuffer * self)
{
  IAudioClient *client_handle;
  HRESULT hr;

  if (self->running) {
    GST_INFO_OBJECT (self, "We are running already");
    return TRUE;
  }

  client_handle = gst_wasapi2_client_get_handle (self->client);
  self->is_first = TRUE;
  self->running = TRUE;
  self->segoffset = 0;
  self->write_frame_offset = 0;

  switch (self->device_class) {
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER:
      /* render client might read data from buffer immediately once it's prepared.
       * Pre-fill with silence in order to start-up glitch */
      hr = gst_wasapi2_ring_buffer_write (self, TRUE);
      if (!gst_wasapi2_result (hr)) {
        GST_ERROR_OBJECT (self, "Failed to pre-fill buffer with silence");
        goto error;
      }
      break;
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE:
    {
      IAudioClient *loopback_client_handle;

      /* Start silence feed client first */
      loopback_client_handle =
          gst_wasapi2_client_get_handle (self->loopback_client);

      hr = loopback_client_handle->Start ();
      if (!gst_wasapi2_result (hr)) {
        GST_ERROR_OBJECT (self, "Failed to start loopback client");
        self->running = FALSE;
        goto error;
      }

      hr = MFPutWaitingWorkItem (self->loopback_event_handle,
          0, self->loopback_callback_result, &self->loopback_callback_key);
      if (!gst_wasapi2_result (hr)) {
        GST_ERROR_OBJECT (self, "Failed to put waiting item");
        loopback_client_handle->Stop ();
        self->running = FALSE;
        goto error;
      }
      break;
    }
    default:
      break;
  }

  hr = client_handle->Start ();
  if (!gst_wasapi2_result (hr)) {
    GST_ERROR_OBJECT (self, "Failed to start client");
    self->running = FALSE;
    goto error;
  }

  if (self->device_class != GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE) {
    hr = MFPutWaitingWorkItem (self->event_handle, 0, self->callback_result,
        &self->callback_key);
    if (!gst_wasapi2_result (hr)) {
      GST_ERROR_OBJECT (self, "Failed to put waiting item");
      client_handle->Stop ();
      self->running = FALSE;
      goto error;
    }
  }

  return TRUE;

error:
  gst_wasapi2_ring_buffer_post_open_error (self);
  return FALSE;
}

static gboolean
gst_wasapi2_ring_buffer_start (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Start");

  return gst_wasapi2_ring_buffer_start_internal (self);
}

static gboolean
gst_wasapi2_ring_buffer_resume (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Resume");

  return gst_wasapi2_ring_buffer_start_internal (self);
}

static gboolean
gst_wasapi2_ring_buffer_stop_internal (GstWasapi2RingBuffer * self)
{
  IAudioClient *client_handle;
  HRESULT hr;

  if (!self->client) {
    GST_DEBUG_OBJECT (self, "No configured client");
    return TRUE;
  }

  if (!self->running) {
    GST_DEBUG_OBJECT (self, "We are not running");
    return TRUE;
  }

  client_handle = gst_wasapi2_client_get_handle (self->client);

  self->running = FALSE;
  MFCancelWorkItem (self->callback_key);

  hr = client_handle->Stop ();
  gst_wasapi2_result (hr);

  /* Call reset for later reuse case */
  hr = client_handle->Reset ();
  self->expected_position = 0;
  self->write_frame_offset = 0;

  if (self->loopback_client) {
    client_handle = gst_wasapi2_client_get_handle (self->loopback_client);

    MFCancelWorkItem (self->loopback_callback_key);

    hr = client_handle->Stop ();
    gst_wasapi2_result (hr);

    client_handle->Reset ();
  }

  return TRUE;
}

static gboolean
gst_wasapi2_ring_buffer_stop (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (buf, "Stop");

  return gst_wasapi2_ring_buffer_stop_internal (self);
}

static gboolean
gst_wasapi2_ring_buffer_pause (GstAudioRingBuffer * buf)
{
  GstWasapi2RingBuffer *self = GST_WASAPI2_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (buf, "Pause");

  return gst_wasapi2_ring_buffer_stop_internal (self);
}

static guint
gst_wasapi2_ring_buffer_delay (GstAudioRingBuffer * buf)
{
  /* NOTE: WASAPI supports GetCurrentPadding() method for querying
   * currently unread buffer size, but it doesn't seem to be quite useful
   * here because:
   *
   * In case of capture client, GetCurrentPadding() will return the number of
   * unread frames which will be identical to pNumFramesToRead value of
   * IAudioCaptureClient::GetBuffer()'s return. Since we are running on
   * event-driven mode and whenever available, WASAPI will notify signal
   * so it's likely zero at this moment. And there is a chance to
   * return incorrect value here because our IO callback happens from
   * other thread.
   *
   * And render client's padding size will return the total size of buffer
   * which is likely larger than twice of our period. Which doesn't represent
   * the amount queued frame size in device correctly
   */
  return 0;
}

GstAudioRingBuffer *
gst_wasapi2_ring_buffer_new (GstWasapi2ClientDeviceClass device_class,
    gboolean low_latency, const gchar * device_id, gpointer dispatcher,
    const gchar * name, guint loopback_target_pid)
{
  GstWasapi2RingBuffer *self;

  self = (GstWasapi2RingBuffer *)
      g_object_new (GST_TYPE_WASAPI2_RING_BUFFER, "name", name, nullptr);

  if (!self->callback_object) {
    gst_object_unref (self);
    return nullptr;
  }

  self->device_class = device_class;
  self->low_latency = low_latency;
  self->device_id = g_strdup (device_id);
  self->dispatcher = dispatcher;
  self->loopback_target_pid = loopback_target_pid;

  return GST_AUDIO_RING_BUFFER_CAST (self);
}

GstCaps *
gst_wasapi2_ring_buffer_get_caps (GstWasapi2RingBuffer * buf)
{
  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf), nullptr);

  if (buf->supported_caps)
    return gst_caps_ref (buf->supported_caps);

  if (!buf->client)
    return nullptr;

  if (!gst_wasapi2_client_ensure_activation (buf->client)) {
    GST_ERROR_OBJECT (buf, "Failed to activate audio client");
    return nullptr;
  }

  buf->supported_caps = gst_wasapi2_client_get_caps (buf->client);
  if (buf->supported_caps)
    return gst_caps_ref (buf->supported_caps);

  return nullptr;
}

HRESULT
gst_wasapi2_ring_buffer_set_mute (GstWasapi2RingBuffer * buf, gboolean mute)
{
  HRESULT hr = S_OK;
  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf), E_INVALIDARG);

  g_mutex_lock (&buf->volume_lock);
  buf->mute = mute;
  if (buf->volume_object) {
    float volume = buf->volume;
    if (mute)
      volume = 0.0f;
    hr = gst_wasapi2_ring_buffer_set_channel_volumes (buf->volume_object,
        volume);
  } else {
    buf->mute_changed = TRUE;
  }
  g_mutex_unlock (&buf->volume_lock);

  return hr;
}

HRESULT
gst_wasapi2_ring_buffer_get_mute (GstWasapi2RingBuffer * buf, gboolean * mute)
{
  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf), E_INVALIDARG);
  g_return_val_if_fail (mute != nullptr, E_INVALIDARG);

  g_mutex_lock (&buf->volume_lock);
  *mute = buf->mute;
  g_mutex_unlock (&buf->volume_lock);

  return S_OK;
}

HRESULT
gst_wasapi2_ring_buffer_set_volume (GstWasapi2RingBuffer * buf, gfloat volume)
{
  HRESULT hr;

  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf), E_INVALIDARG);
  g_return_val_if_fail (volume >= 0 && volume <= 1.0, E_INVALIDARG);

  g_mutex_lock (&buf->volume_lock);
  buf->volume = volume;
  if (buf->volume_object) {
    hr = gst_wasapi2_ring_buffer_set_channel_volumes (buf->volume_object,
        volume);
  } else {
    buf->volume_changed = TRUE;
  }
  g_mutex_unlock (&buf->volume_lock);

  return hr;
}

HRESULT
gst_wasapi2_ring_buffer_get_volume (GstWasapi2RingBuffer * buf, gfloat * volume)
{
  g_return_val_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf), E_INVALIDARG);
  g_return_val_if_fail (volume != nullptr, E_INVALIDARG);

  g_mutex_lock (&buf->volume_lock);
  *volume = buf->volume;
  g_mutex_unlock (&buf->volume_lock);

  return S_OK;
}

void
gst_wasapi2_ring_buffer_set_device_mute_monitoring (GstWasapi2RingBuffer * buf,
    gboolean value)
{
  g_return_if_fail (GST_IS_WASAPI2_RING_BUFFER (buf));

  buf->priv->monitor_device_mute.store (value, std::memory_order_release);
}
