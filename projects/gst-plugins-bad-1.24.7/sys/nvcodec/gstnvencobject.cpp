/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstnvencobject.h"
#include <algorithm>

GST_DEBUG_CATEGORY_EXTERN (gst_nv_encoder_debug);
#define GST_CAT_DEFAULT gst_nv_encoder_debug

/* Both CUDA and D3D11 use the same value */
#define GST_MAP_NVENC (GST_MAP_FLAG_LAST << 1)
#define GST_MAP_READ_NVENC (GstMapFlags)(GST_MAP_READ | GST_MAP_NVENC)

/* *INDENT-OFF* */
static GstNvEncBuffer * gst_nv_enc_buffer_new (const std::string & id,
    guint seq_num);
static GstNvEncResource * gst_nv_enc_resource_new (const std::string & id,
    guint seq_num);
static GstNvEncTask * gst_nv_enc_task_new (const std::string & id,
    guint seq_num);

struct GstNvEncBuffer : public GstMiniObject
{
  GstNvEncBuffer (const std::string parent_id, guint seq)
      : id (parent_id), seq_num (seq)
  {
    memset (&buffer, 0, sizeof (NV_ENC_CREATE_INPUT_BUFFER));
    memset (&buffer_lock, 0, sizeof (NV_ENC_LOCK_INPUT_BUFFER));

    buffer.version = gst_nvenc_get_create_input_buffer_version ();
    buffer_lock.version = gst_nvenc_get_lock_input_buffer_version ();
  }

  std::shared_ptr<GstNvEncObject> object;

  NV_ENC_CREATE_INPUT_BUFFER buffer;
  NV_ENC_LOCK_INPUT_BUFFER buffer_lock;

  bool locked = false;
  std::string id;
  guint seq_num;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstNvEncBuffer, gst_nv_enc_buffer);

struct GstNvEncResource : public GstMiniObject
{
  GstNvEncResource (const std::string & parent_id, guint seq)
      : id (parent_id), seq_num (seq)
  {
    memset (&resource, 0, sizeof (NV_ENC_REGISTER_RESOURCE));
    memset (&mapped_resource, 0, sizeof (NV_ENC_MAP_INPUT_RESOURCE));

    resource.version = gst_nvenc_get_register_resource_version ();
    mapped_resource.version = gst_nvenc_get_map_input_resource_version ();
  }

  std::weak_ptr<GstNvEncObject> object;

  NV_ENC_REGISTER_RESOURCE resource;
  NV_ENC_MAP_INPUT_RESOURCE mapped_resource;

  std::string id;
  guint seq_num;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstNvEncResource, gst_nv_enc_resource);

static void
gst_nv_enc_task_clear_sei (NV_ENC_SEI_PAYLOAD * payload)
{
  g_clear_pointer (&payload->payload, g_free);
}

struct GstNvEncTask : public GstMiniObject
{
  GstNvEncTask (const std::string & parent_id, guint seq)
      : id (parent_id), seq_num (seq)
  {
    memset (&event_params, 0, sizeof (NV_ENC_EVENT_PARAMS));
    memset (&bitstream, 0, sizeof (NV_ENC_LOCK_BITSTREAM));

    event_params.version = gst_nvenc_get_event_params_version ();
    bitstream.version = gst_nvenc_get_lock_bitstream_version ();

    sei_payload = g_array_new (FALSE, FALSE, sizeof (NV_ENC_SEI_PAYLOAD));
    g_array_set_clear_func (sei_payload,
        (GDestroyNotify) gst_nv_enc_task_clear_sei);
  }

  ~GstNvEncTask ()
  {
    if (sei_payload)
      g_array_unref (sei_payload);
  }

  std::shared_ptr<GstNvEncObject> object;

  GstNvEncBuffer *buffer = nullptr;
  GstNvEncResource *resource = nullptr;

  GstBuffer *gst_buffer = nullptr;;
  GstMapInfo info;
  NV_ENC_DEVICE_TYPE device_type = NV_ENC_DEVICE_TYPE_CUDA;

  NV_ENC_EVENT_PARAMS event_params;
  NV_ENC_OUTPUT_PTR output_ptr = nullptr;

  NV_ENC_LOCK_BITSTREAM bitstream;
  bool locked = false;
  std::string id;
  guint seq_num;

  GArray *sei_payload;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstNvEncTask, gst_nv_enc_task);

bool
GstNvEncObject::IsSuccess (NVENCSTATUS status, GstNvEncObject * self,
    const gchar * file, const gchar * function, gint line)
{
  if (status == NV_ENC_SUCCESS)
    return true;

#ifndef GST_DISABLE_GST_DEBUG
  const gchar *status_str = nvenc_status_to_string (status);

  if (self) {
    gst_debug_log_id (GST_CAT_DEFAULT, GST_LEVEL_ERROR, file, function,
        line, self->id_.c_str (), "NvEnc API call failed: 0x%x, %s",
        (guint) status, status_str);
  } else {
    gst_debug_log (GST_CAT_DEFAULT, GST_LEVEL_ERROR, file, function,
      line, nullptr, "NvEnc API call failed: 0x%x, %s",
      (guint) status, status_str);
  }
#endif

  return false;
}

#define NVENC_IS_SUCCESS(status,obj) \
  GstNvEncObject::IsSuccess (status, obj, __FILE__, GST_FUNCTION, __LINE__)

std::shared_ptr<GstNvEncObject>
GstNvEncObject::CreateInstance (GstElement * client, GstObject * device,
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS * params)
{
  NVENCSTATUS status;
  gpointer session;

  status = NvEncOpenEncodeSessionEx (params, &session);
  if (!NVENC_IS_SUCCESS (status, nullptr)) {
    GST_ERROR_OBJECT (device, "NvEncOpenEncodeSessionEx failed");
    /* Report error to abort if GST_CUDA_CRITICAL_ERRORS is configured */
    gst_cuda_result (CUDA_ERROR_NO_DEVICE);
    return nullptr;
  }

  std::shared_ptr<GstNvEncObject> self =
      std::make_shared <GstNvEncObject> ();
  self->id_ = GST_ELEMENT_NAME (client);
  self->session_ = session;

#ifdef G_OS_WIN32
  if (params->deviceType == NV_ENC_DEVICE_TYPE_DIRECTX) {
    self->device_ = (GstD3D11Device *) gst_object_ref (device);
    self->user_token_ = gst_d3d11_create_user_token ();
  } else
#endif
  {
    self->context_ = (GstCudaContext *) gst_object_ref (device);
    self->user_token_ = gst_cuda_create_user_token ();
  }

  self->device_type_ = params->deviceType;
  self->buffer_seq_ = 0;
  self->resource_seq_ = 0;
  self->task_seq_ = 0;

  GST_INFO_ID (self->id_.c_str (), "New encoder object for type %d is created",
      self->device_type_);

  return self;
}

GstNvEncObject::~GstNvEncObject ()
{
  GST_INFO_ID (id_.c_str (), "Destroying instance");

  DeviceLock ();
  while (!buffer_queue_.empty ()) {
    GstNvEncBuffer *buf = buffer_queue_.front ();

    NvEncDestroyInputBuffer (session_, buf->buffer.inputBuffer);
    gst_nv_enc_buffer_unref (buf);
    buffer_queue_.pop ();
  }

  if (!resource_queue_.empty ()) {
    GST_INFO_ID (id_.c_str (), "Have %u outstanding input resource(s)",
        (guint) resource_queue_.size ());
    for (auto it : resource_queue_)
      releaseResourceUnlocked (it);
  }

  while (!empty_task_queue_.empty ()) {
    GstNvEncTask *task = empty_task_queue_.front ();

    releaseTaskUnlocked (task);
    empty_task_queue_.pop ();
  }

  NvEncDestroyEncoder (session_);
  DeviceUnlock ();

  gst_clear_object (&context_);
  gst_clear_cuda_stream (&stream_);
#ifdef G_OS_WIN32
  gst_clear_object (&device_);
#endif

  GST_INFO_ID (id_.c_str (), "Cleared all resources");
}

gpointer
GstNvEncObject::GetHandle ()
{
  return session_;
}

guint
GstNvEncObject::GetTaskSize ()
{
  return task_size_;
}

void
GstNvEncObject::releaseTaskUnlocked (GstNvEncTask * task)
{
  if (!task)
    return;

  if (task->output_ptr) {
    NvEncDestroyBitstreamBuffer (session_, task->output_ptr);
    task->output_ptr = nullptr;
  }

#ifdef G_OS_WIN32
  if (task->event_params.completionEvent) {
    gpointer handle = task->event_params.completionEvent;
    NvEncUnregisterAsyncEvent (session_, &task->event_params);
    CloseHandle (handle);

    memset (&task->event_params, 0, sizeof (NV_ENC_EVENT_PARAMS));
  }
#endif

  gst_nv_enc_task_unref (task);
}

NVENCSTATUS
GstNvEncObject::InitSession (NV_ENC_INITIALIZE_PARAMS * params,
    GstCudaStream * stream, const GstVideoInfo * info, guint task_size)
{
  NVENCSTATUS status = NV_ENC_SUCCESS;

  if (initialized_) {
    GST_ERROR_ID (id_.c_str(), "Was initialized");
    return NV_ENC_ERR_INVALID_CALL;
  }

  if (memcmp (&params->encodeGUID, &NV_ENC_CODEC_H264_GUID, sizeof (GUID)) == 0) {
    codec_ = GST_NV_ENC_CODEC_H264;
  } else {
    codec_ = GST_NV_ENC_CODEC_H265;
  }

  info_ = *info;
  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_NV12:
      buffer_format_ = NV_ENC_BUFFER_FORMAT_NV12;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_GBR:
      buffer_format_ = NV_ENC_BUFFER_FORMAT_YUV444;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      buffer_format_ = NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
      break;
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GBR_16LE:
      buffer_format_ = NV_ENC_BUFFER_FORMAT_YUV444_10BIT;
      break;
    default:
      GST_ERROR_ID (id_.c_str (), "Unexpected format %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));
      return NV_ENC_ERR_INVALID_PARAM;
  }

  GST_DEBUG_ID (id_.c_str (), "Initializing encoder, buffer type %d",
      buffer_format_);

  status = NvEncInitializeEncoder (session_, params);
  if (!NVENC_IS_SUCCESS (status, this))
    return status;

  if (device_type_ == NV_ENC_DEVICE_TYPE_CUDA && stream) {
    CUstream stream_handle;

    stream_ = gst_cuda_stream_ref (stream);
    stream_handle = gst_cuda_stream_get_handle (stream);
    status = NvEncSetIOCudaStreams (session_,
        (NV_ENC_CUSTREAM_PTR) & stream_handle,
        (NV_ENC_CUSTREAM_PTR) & stream_handle);
    if (!NVENC_IS_SUCCESS (status, this))
      return status;
  }

  for (guint i = 0; i < task_size; i++) {
    GstNvEncTask *task = gst_nv_enc_task_new (id_, task_seq_.fetch_add (1));
    NV_ENC_CREATE_BITSTREAM_BUFFER buffer_params = { 0, };

    task->device_type = device_type_;

    buffer_params.version = gst_nvenc_get_create_bitstream_buffer_version ();
    status = NvEncCreateBitstreamBuffer (session_, &buffer_params);
    if (!NVENC_IS_SUCCESS (status, this)) {
      gst_nv_enc_task_unref (task);
      goto out;
    }

    task->output_ptr = buffer_params.bitstreamBuffer;

#ifdef G_OS_WIN32
    if (params->enableEncodeAsync) {
      task->event_params.version =  gst_nvenc_get_event_params_version ();
      task->event_params.completionEvent = CreateEvent (nullptr,
          FALSE, FALSE, nullptr);
      status = NvEncRegisterAsyncEvent (session_, &task->event_params);
      if (!NVENC_IS_SUCCESS (status, this)) {
        CloseHandle (task->event_params.completionEvent);
        releaseTaskUnlocked (task);
        goto out;
      }
    }
#endif

    empty_task_queue_.push (task);
  }

  task_size_ = task_size;
  lookahead_ = params->encodeConfig->rcParams.lookaheadDepth;
  initialized_ = true;

out:
  if (status != NV_ENC_SUCCESS) {
    while (!empty_task_queue_.empty ()) {
      GstNvEncTask *task = empty_task_queue_.front ();

      releaseTaskUnlocked (task);
      empty_task_queue_.pop ();
    }
  }

  return status;
}

NVENCSTATUS
GstNvEncObject::Reconfigure (NV_ENC_RECONFIGURE_PARAMS * params)
{
  return NvEncReconfigureEncoder (session_, params);
}

void
GstNvEncObject::SetFlushing (bool flushing)
{
  std::lock_guard <std::mutex> lk (lock_);
  flushing_ = flushing;
  cond_.notify_all ();
}

NVENCSTATUS
GstNvEncObject::Encode (GstVideoCodecFrame * codec_frame,
    NV_ENC_PIC_STRUCT pic_struct, GstNvEncTask * task)
{
  NVENCSTATUS status;
  guint retry_count = 0;
  const guint retry_threshold = 100;
  NV_ENC_PIC_PARAMS params = { 0, };

  std::unique_lock <std::mutex> lk (lock_);

  params.version = gst_nvenc_get_pic_params_version ();
  params.completionEvent = task->event_params.completionEvent;

  g_assert (task->buffer || task->resource);

  GST_LOG_ID (id_.c_str (), "Encoding frame %u",
      codec_frame->system_frame_number);

  if (task->buffer) {
    params.inputWidth = task->buffer->buffer.width;
    params.inputHeight = task->buffer->buffer.height;
    params.inputPitch = task->buffer->buffer_lock.pitch;
    params.inputBuffer = task->buffer->buffer.inputBuffer;
    params.bufferFmt = task->buffer->buffer.bufferFmt;
  } else {
    params.inputWidth = task->resource->resource.width;
    params.inputHeight = task->resource->resource.height;
    params.inputPitch = task->resource->resource.pitch;
    params.inputBuffer = task->resource->mapped_resource.mappedResource;
    params.bufferFmt = task->resource->mapped_resource.mappedBufferFmt;
  }

  params.frameIdx = codec_frame->system_frame_number;
  params.inputTimeStamp = codec_frame->pts;
  params.inputDuration = codec_frame->duration;
  params.outputBitstream = task->output_ptr;
  params.pictureStruct = pic_struct;
  if (task->sei_payload->len > 0) {
    if (codec_ == GST_NV_ENC_CODEC_H264) {
      params.codecPicParams.h264PicParams.seiPayloadArray =
          &g_array_index (task->sei_payload, NV_ENC_SEI_PAYLOAD, 0);
      params.codecPicParams.h264PicParams.seiPayloadArrayCnt =
          task->sei_payload->len;
    } else {
      params.codecPicParams.hevcPicParams.seiPayloadArray =
          &g_array_index (task->sei_payload, NV_ENC_SEI_PAYLOAD, 0);
      params.codecPicParams.hevcPicParams.seiPayloadArrayCnt =
          task->sei_payload->len;
    }
  }

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (codec_frame))
    params.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;

  do {
    DeviceLock ();
    status = NvEncEncodePicture (session_, &params);
    DeviceUnlock ();

    if (status == NV_ENC_ERR_ENCODER_BUSY) {
      if (retry_count < 100) {
        GST_DEBUG_ID (id_.c_str (), "GPU is busy, retry count (%d/%d)",
            retry_count, retry_threshold);
        retry_count++;

        /* Magic number 1ms */
        g_usleep (1000);
        continue;
      } else {
        GST_ERROR_ID (id_.c_str (), "GPU is keep busy, give up");
        break;
      }
    }

    break;
  } while (true);

  if (status != NV_ENC_SUCCESS && status != NV_ENC_ERR_NEED_MORE_INPUT) {
    NVENC_IS_SUCCESS (status, this);
    lk.unlock ();
    gst_nv_enc_task_unref (task);
    return status;
  }

  gst_video_codec_frame_set_user_data (codec_frame, task, nullptr);

  {
    std::lock_guard <std::recursive_mutex> rlk (resource_lock_);

    if (task->resource)
      active_resource_queue_.insert (task->resource);
  }

  /* On Windows and if async encoding is enabled, output thread will wait
   * for completion event. But on Linux, async encoding is not supported.
   * So, we should wait for NV_ENC_SUCCESS in case of sync mode
   * (it would introduce latency though).
   * Otherwise nvEncLockBitstream() will return error */
  if (params.completionEvent) {
    /* Windows only path */
    task_queue_.push (task);
    cond_.notify_all ();
  } else {
    pending_task_queue_.push (task);
    if (status == NV_ENC_SUCCESS) {
      bool notify = false;

      /* XXX: nvEncLockBitstream() will return NV_ENC_ERR_INVALID_PARAM
       * if lookahead is enabled. See also
       * https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/494
       */
      while (pending_task_queue_.size() > lookahead_) {
        notify = true;
        task_queue_.push (pending_task_queue_.front ());
        pending_task_queue_.pop ();
      }

      if (notify)
        cond_.notify_all ();
    }
  }

  return NV_ENC_SUCCESS;
}

NVENCSTATUS
GstNvEncObject::Drain (GstNvEncTask * task)
{
  NVENCSTATUS status;
  guint retry_count = 0;
  const guint retry_threshold = 100;
  NV_ENC_PIC_PARAMS params = { 0, };

  std::unique_lock <std::mutex> lk (lock_);

  params.version = gst_nvenc_get_pic_params_version ();
  params.completionEvent = task->event_params.completionEvent;
  params.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

  do {
    status = NvEncEncodePicture (session_, &params);

    if (status == NV_ENC_ERR_ENCODER_BUSY) {
      if (retry_count < 100) {
        GST_DEBUG_ID (id_.c_str (), "GPU is busy, retry count (%d/%d)",
            retry_count, retry_threshold);
        retry_count++;

        /* Magic number 1ms */
        g_usleep (1000);
        continue;
      } else {
        GST_ERROR_ID (id_.c_str (), "GPU is keep busy, give up");
        break;
      }
    }

    break;
  } while (true);

  while (!pending_task_queue_.empty ()) {
    task_queue_.push (pending_task_queue_.front ());
    pending_task_queue_.pop ();
  }

  task_queue_.push (task);
  cond_.notify_all ();

  return status;
}

GstFlowReturn
GstNvEncObject::GetOutput (GstNvEncTask ** task)
{
  GstNvEncTask *ret = nullptr;
  std::unique_lock <std::mutex> lk (lock_);

  while (task_queue_.empty ())
    cond_.wait (lk);

  ret = task_queue_.front ();
  task_queue_.pop ();
  lk.unlock ();

  if (!ret->buffer && !ret->resource) {
    gst_nv_enc_task_unref (ret);
    return GST_FLOW_EOS;
  }

#ifdef G_OS_WIN32
  if (ret->event_params.completionEvent &&
      WaitForSingleObject (ret->event_params.completionEvent, 20000) ==
      WAIT_FAILED) {
    GST_ERROR_ID (id_.c_str (), "Failed to wait for completion event");
    gst_nv_enc_task_unref (ret);
    return GST_FLOW_ERROR;
  }
#endif

  *task = ret;

  return GST_FLOW_OK;
}

NVENCSTATUS
GstNvEncObject::LockBitstream (NV_ENC_LOCK_BITSTREAM * bitstream)
{
  return NvEncLockBitstream (session_, bitstream);
}

NVENCSTATUS
GstNvEncObject::UnlockBitstream (NV_ENC_OUTPUT_PTR output_ptr)
{
  return NvEncUnlockBitstream (session_, output_ptr);
}

NVENCSTATUS
GstNvEncObject::AcquireBuffer (GstNvEncBuffer ** buffer)
{
  GstNvEncBuffer *new_buf = nullptr;
  std::unique_lock <std::mutex> lk (lock_);

  if (buffer_queue_.empty ()) {
    NVENCSTATUS status;
    NV_ENC_CREATE_INPUT_BUFFER in_buf = { 0, };

    GST_LOG_ID (id_.c_str (), "No available input buffer, creating new one");

    in_buf.version = gst_nvenc_get_create_input_buffer_version ();
    in_buf.width = info_.width;
    in_buf.height = info_.height;
    in_buf.bufferFmt = buffer_format_;

    status = NvEncCreateInputBuffer (session_, &in_buf);
    if (!NVENC_IS_SUCCESS (status, this))
      return status;

    new_buf = gst_nv_enc_buffer_new (id_, buffer_seq_.fetch_add (1));
    new_buf->buffer = in_buf;
    new_buf->buffer_lock.inputBuffer = in_buf.inputBuffer;
  } else {
    new_buf = buffer_queue_.front ();
    buffer_queue_.pop ();
  }

  g_assert (!new_buf->object);

  new_buf->object = shared_from_this ();

  *buffer = new_buf;

  GST_TRACE_ID (id_.c_str (), "Acquired buffer %u", new_buf->seq_num);

  return NV_ENC_SUCCESS;
}

void
GstNvEncObject::runResourceGC ()
{
  std::lock_guard <std::recursive_mutex> lk (resource_lock_);

  /* hard coded max size 64 */
  if (resource_queue_.size () < 64)
    return;

  GST_LOG_ID (id_.c_str (), "Running resource GC");

  DeviceLock ();
  for (auto it : resource_queue_) {
    if (active_resource_queue_.find (it) == active_resource_queue_.end ()) {
      releaseResourceUnlocked (it);
      resource_queue_.erase (it);
    }
  }
  DeviceUnlock ();

  GST_LOG_ID (id_.c_str (), "resource queue size after GC %u",
      (guint) resource_queue_.size ());
}

bool
GstNvEncObject::DeviceLock ()
{
  if (context_)
    return gst_cuda_context_push (context_);

  return true;
}

bool
GstNvEncObject::DeviceUnlock ()
{
  if (context_)
    return gst_cuda_context_pop (nullptr);

  return true;
}

NVENCSTATUS
GstNvEncObject::acquireResourceCuda (GstMemory * mem,
    GstNvEncResource ** resource)
{
  GstNvEncResource *res;
  GstCudaMemory *cmem;
  NV_ENC_REGISTER_RESOURCE new_resource;
  NV_ENC_MAP_INPUT_RESOURCE mapped_resource;
  NVENCSTATUS status;
  GstMapInfo info;

  if (!gst_is_cuda_memory (mem)) {
    GST_ERROR_ID (id_.c_str (), "Not a CUDA memory");
    return NV_ENC_ERR_INVALID_CALL;
  }

  cmem = GST_CUDA_MEMORY_CAST (mem);

  res = (GstNvEncResource *) gst_cuda_memory_get_token_data (cmem,
      user_token_);
  if (res) {
    auto iter = resource_queue_.find (res);
    /* This resource can be released already */
    if (iter != resource_queue_.end ()) {
      GST_LOG_ID (id_.c_str (), "Memory is holding registered resource");
      *resource = gst_nv_enc_resource_ref (res);
      return NV_ENC_SUCCESS;
    }
  }

  if (!gst_memory_map (mem, &info, GST_MAP_READ_NVENC)) {
    GST_ERROR_ID (id_.c_str (), "Couldn't map CUDA memory");
    return NV_ENC_ERR_MAP_FAILED;
  }

  memset (&new_resource, 0, sizeof (NV_ENC_REGISTER_RESOURCE));
  memset (&mapped_resource, 0, sizeof (NV_ENC_MAP_INPUT_RESOURCE));

  new_resource.version = gst_nvenc_get_register_resource_version ();
  new_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
  new_resource.width = cmem->info.width;
  new_resource.height = cmem->info.height;
  new_resource.pitch = cmem->info.stride[0];
  new_resource.resourceToRegister = info.data;
  new_resource.bufferFormat = buffer_format_;

  GST_LOG_ID (id_.c_str (), "Registering CUDA resource %p, %dx%d, pitch %u",
      info.data, new_resource.width, new_resource.height, new_resource.pitch);

  status = NvEncRegisterResource (session_, &new_resource);
  gst_memory_unmap (mem, &info);

  if (!NVENC_IS_SUCCESS (status, this))
    return status;

  mapped_resource.version = gst_nvenc_get_map_input_resource_version ();
  mapped_resource.registeredResource = new_resource.registeredResource;
  status = NvEncMapInputResource (session_, &mapped_resource);
  if (!NVENC_IS_SUCCESS (status, this)) {
    NvEncUnregisterResource (session_, new_resource.registeredResource);
    return status;
  }

  res = gst_nv_enc_resource_new (id_, resource_seq_.fetch_add (1));
  /* weak ref */
  res->object = shared_from_this ();

  res->resource = new_resource;
  res->mapped_resource = mapped_resource;

  gst_cuda_memory_set_token_data (cmem, user_token_,
      gst_nv_enc_resource_ref (res),
      (GDestroyNotify) gst_nv_enc_resource_unref);
  resource_queue_.insert (res);

  *resource = res;

  return NV_ENC_SUCCESS;
}

#ifdef G_OS_WIN32
NVENCSTATUS
GstNvEncObject::acquireResourceD3D11 (GstMemory * mem,
    GstNvEncResource ** resource)
{
  GstNvEncResource *res;
  GstD3D11Memory *dmem;
  NV_ENC_REGISTER_RESOURCE new_resource;
  NV_ENC_MAP_INPUT_RESOURCE mapped_resource;
  NVENCSTATUS status;
  D3D11_TEXTURE2D_DESC desc;
  GstMapInfo info;

  if (!gst_is_d3d11_memory (mem)) {
    GST_ERROR_ID (id_.c_str (), "Not a D3D11 memory");
    return NV_ENC_ERR_INVALID_CALL;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);

  res = (GstNvEncResource *) gst_d3d11_memory_get_token_data (dmem,
      user_token_);
  if (res) {
    auto iter = resource_queue_.find (res);
    /* This resource can be released already */
    if (iter != resource_queue_.end ()) {
      GST_LOG_ID (id_.c_str (), "Memory is holding registered resource");
      *resource = gst_nv_enc_resource_ref (res);
      return NV_ENC_SUCCESS;
    }
  }

  if (!gst_memory_map (mem, &info, GST_MAP_READ_NVENC)) {
    GST_ERROR_ID (id_.c_str (), "Couldn't map D3D11 memory");
    return NV_ENC_ERR_MAP_FAILED;
  }

  gst_d3d11_memory_get_texture_desc (dmem, &desc);

  memset (&new_resource, 0, sizeof (NV_ENC_REGISTER_RESOURCE));
  memset (&mapped_resource, 0, sizeof (NV_ENC_MAP_INPUT_RESOURCE));

  new_resource.version = gst_nvenc_get_register_resource_version ();
  new_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
  new_resource.width = desc.Width;
  new_resource.height = desc.Height;
  new_resource.pitch = 0;
  new_resource.resourceToRegister = info.data;
  new_resource.subResourceIndex = gst_d3d11_memory_get_subresource_index (dmem);
  new_resource.bufferFormat = buffer_format_;

  status = NvEncRegisterResource (session_, &new_resource);
  gst_memory_unmap (mem, &info);
  if (!NVENC_IS_SUCCESS (status, this))
    return status;

  mapped_resource.version = gst_nvenc_get_map_input_resource_version ();
  mapped_resource.registeredResource = new_resource.registeredResource;
  status = NvEncMapInputResource (session_, &mapped_resource);
  if (!NVENC_IS_SUCCESS (status, this)) {
    NvEncUnregisterResource (session_, new_resource.registeredResource);
    return status;
  }

  res = gst_nv_enc_resource_new (id_, resource_seq_.fetch_add (1));
  /* weak ref */
  res->object = shared_from_this ();

  res->resource = new_resource;
  res->mapped_resource = mapped_resource;

  gst_d3d11_memory_set_token_data (dmem, user_token_,
      gst_nv_enc_resource_ref (res),
      (GDestroyNotify) gst_nv_enc_resource_unref);
  resource_queue_.insert (res);

  *resource = res;

  return NV_ENC_SUCCESS;
}
#endif

NVENCSTATUS
GstNvEncObject::AcquireResource (GstMemory * mem, GstNvEncResource ** resource)
{
  NVENCSTATUS status;
  std::lock_guard <std::recursive_mutex> lk (resource_lock_);

#ifdef G_OS_WIN32
  if (device_type_ == NV_ENC_DEVICE_TYPE_DIRECTX) {
    status = acquireResourceD3D11 (mem, resource);
  } else
#endif
  {
    status = acquireResourceCuda (mem, resource);
  }

  if (status == NV_ENC_SUCCESS) {
    GST_TRACE_ID (id_.c_str (), "Returning resource %u, "
        "resource queue size %u (active %u)",
        (*resource)->seq_num, (guint) resource_queue_.size (),
        (guint) active_resource_queue_.size ());
  }

  return status;
}

GstFlowReturn
GstNvEncObject::AcquireTask (GstNvEncTask ** task, bool force)
{
  GstNvEncTask *new_task = nullptr;

  std::unique_lock <std::mutex> lk (lock_);

  do {
    if (!force && flushing_) {
      GST_DEBUG_ID (id_.c_str (), "We are flushing");
      return GST_FLOW_FLUSHING;
    }

    if (!empty_task_queue_.empty ()) {
      new_task = empty_task_queue_.front ();
      empty_task_queue_.pop ();
      break;
    }

    GST_LOG_ID (id_.c_str (), "No available task, waiting for release");
    cond_.wait (lk);
  } while (true);

  g_assert (!new_task->object);

  new_task->object = shared_from_this ();
  g_array_set_size (new_task->sei_payload, 0);

  *task = new_task;

  GST_TRACE_ID (id_.c_str (), "Acquired task %u", new_task->seq_num);

  runResourceGC ();

  return GST_FLOW_OK;
}

void
GstNvEncObject::PushEmptyTask (GstNvEncTask * task)
{
  std::lock_guard <std::mutex> lk (lock_);

  empty_task_queue_.push (task);
  cond_.notify_all ();
}

void
GstNvEncObject::PushEmptyBuffer (GstNvEncBuffer * buffer)
{
  std::lock_guard <std::mutex> lk (lock_);

  buffer_queue_.push (buffer);
  cond_.notify_all ();
}

void
GstNvEncObject::releaseResourceUnlocked (GstNvEncResource * resource)
{
  NvEncUnmapInputResource (session_, resource->mapped_resource.mappedResource);
  NvEncUnregisterResource (session_, resource->resource.registeredResource);

  resource->mapped_resource.mappedResource = nullptr;
  resource->resource.registeredResource = nullptr;
}

void
GstNvEncObject::ReleaseResource (GstNvEncResource * resource)
{
  std::lock_guard <std::recursive_mutex> lk (resource_lock_);

  active_resource_queue_.erase (resource);

  auto it = resource_queue_.find (resource);
  if (it != resource_queue_.end ()) {
    DeviceLock ();
    releaseResourceUnlocked (resource);
    DeviceUnlock ();
    resource_queue_.erase (it);
  }
}

void
GstNvEncObject::DeactivateResource (GstNvEncResource * resource)
{
  std::lock_guard <std::recursive_mutex> lk (resource_lock_);

  GST_TRACE_ID (resource->id.c_str (), "Deactivating resource %u",
      resource->seq_num);

  active_resource_queue_.erase (resource);
}

/* *INDENT-ON* */

NVENCSTATUS
gst_nv_enc_buffer_lock (GstNvEncBuffer * buffer,
    gpointer * data, guint32 * pitch)
{
  std::shared_ptr < GstNvEncObject > object = buffer->object;
  NVENCSTATUS status;

  g_assert (object);

  GST_TRACE_ID (buffer->id.c_str (), "Locking buffer %u", buffer->seq_num);

  if (!buffer->locked) {
    buffer->buffer_lock.inputBuffer = buffer->buffer.inputBuffer;
    status = NvEncLockInputBuffer (object->GetHandle (), &buffer->buffer_lock);
    if (!NVENC_IS_SUCCESS (status, object.get ()))
      return status;

    buffer->locked = true;
  }

  *data = buffer->buffer_lock.bufferDataPtr;
  *pitch = buffer->buffer_lock.pitch;

  return NV_ENC_SUCCESS;
}

void
gst_nv_enc_buffer_unlock (GstNvEncBuffer * buffer)
{
  std::shared_ptr < GstNvEncObject > object = buffer->object;

  if (!buffer->locked) {
    GST_DEBUG_ID (buffer->id.c_str (),
        "Buffer %u was not locked", buffer->seq_num);
    return;
  }

  g_assert (object);

  NvEncUnlockInputBuffer (object->GetHandle (), buffer->buffer.inputBuffer);
  buffer->locked = false;
}

static gboolean
gst_nv_enc_buffer_dispose (GstNvEncBuffer * buffer)
{
  std::shared_ptr < GstNvEncObject > object = buffer->object;

  GST_TRACE_ID (buffer->id.c_str (), "Disposing buffer %u", buffer->seq_num);

  if (!object)
    return TRUE;

  gst_nv_enc_buffer_unlock (buffer);
  buffer->object = nullptr;

  GST_TRACE_ID (buffer->id.c_str (),
      "Back to buffer queue %u", buffer->seq_num);

  /* Back to task queue */
  gst_nv_enc_buffer_ref (buffer);
  object->PushEmptyBuffer (buffer);

  return FALSE;
}

static void
gst_nv_enc_buffer_free (GstNvEncBuffer * buffer)
{
  GST_TRACE_ID (buffer->id.c_str (), "Freeing buffer %u", buffer->seq_num);

  delete buffer;
}

static GstNvEncBuffer *
gst_nv_enc_buffer_new (const std::string & id, guint seq_num)
{
  GstNvEncBuffer *buffer = new GstNvEncBuffer (id, seq_num);

  gst_mini_object_init (buffer, 0, GST_TYPE_NV_ENC_BUFFER, nullptr,
      (GstMiniObjectDisposeFunction) gst_nv_enc_buffer_dispose,
      (GstMiniObjectFreeFunction) gst_nv_enc_buffer_free);

  return buffer;
}

static gboolean
gst_nv_enc_resource_dispose (GstNvEncResource * resource)
{
  std::shared_ptr < GstNvEncObject > object;

  GST_TRACE_ID (resource->id.c_str (),
      "Disposing resource %u", resource->seq_num);

  object = resource->object.lock ();

  if (!object)
    return TRUE;

  object->ReleaseResource (resource);

  return TRUE;
}

static void
gst_nv_enc_resource_free (GstNvEncResource * resource)
{
  GST_TRACE_ID (resource->id.c_str (),
      "Freeing resource %u", resource->seq_num);

  delete resource;
}

static GstNvEncResource *
gst_nv_enc_resource_new (const std::string & id, guint seq_num)
{
  GstNvEncResource *resource = new GstNvEncResource (id, seq_num);

  gst_mini_object_init (resource, 0, GST_TYPE_NV_ENC_RESOURCE, nullptr,
      (GstMiniObjectDisposeFunction) gst_nv_enc_resource_dispose,
      (GstMiniObjectFreeFunction) gst_nv_enc_resource_free);

  return resource;
}

gboolean
gst_nv_enc_task_set_buffer (GstNvEncTask * task, GstNvEncBuffer * buffer)
{
  g_assert (!task->buffer);
  g_assert (!task->resource);

  task->buffer = buffer;

  return TRUE;
}

gboolean
gst_nv_enc_task_set_resource (GstNvEncTask * task,
    GstBuffer * buffer, GstNvEncResource * resource)
{
  if (!gst_buffer_map (buffer, &task->info, GST_MAP_READ_NVENC)) {
    GST_ERROR_ID (task->id.c_str (), "Couldn't map resource buffer");
    gst_buffer_unref (buffer);
    gst_nv_enc_resource_unref (resource);
    return FALSE;
  }

  task->gst_buffer = buffer;
  task->resource = resource;

  return TRUE;
}

GArray *
gst_nv_enc_task_get_sei_payload (GstNvEncTask * task)
{
  return task->sei_payload;
}

NVENCSTATUS
gst_nv_enc_task_lock_bitstream (GstNvEncTask * task,
    NV_ENC_LOCK_BITSTREAM * bitstream)
{
  NVENCSTATUS status;

  if (task->locked) {
    GST_ERROR_ID (task->id.c_str (), "Bitstream was locked already");
    return NV_ENC_ERR_INVALID_CALL;
  }

  task->bitstream.outputBitstream = task->output_ptr;
  status = task->object->LockBitstream (&task->bitstream);

  if (!NVENC_IS_SUCCESS (status, task->object.get ()))
    return status;

  task->locked = true;
  *bitstream = task->bitstream;

  return NV_ENC_SUCCESS;
}

void
gst_nv_enc_task_unlock_bitstream (GstNvEncTask * task)
{
  NVENCSTATUS status;

  if (!task->locked)
    return;

  status = task->object->UnlockBitstream (task->output_ptr);

  NVENC_IS_SUCCESS (status, task->object.get ());
  task->locked = false;
}

static gboolean
gst_nv_enc_task_dispose (GstNvEncTask * task)
{
  std::shared_ptr < GstNvEncObject > object;

  GST_TRACE_ID (task->id.c_str (), "Disposing task %u", task->seq_num);

  object = task->object;

  g_array_set_size (task->sei_payload, 0);

  if (task->resource) {
    object->DeactivateResource (task->resource);
    gst_clear_nv_encoder_resource (&task->resource);
  }

  gst_clear_nv_encoder_buffer (&task->buffer);

  if (task->gst_buffer) {
    if (task->device_type == NV_ENC_DEVICE_TYPE_CUDA) {
      GstMemory *mem = gst_buffer_peek_memory (task->gst_buffer, 0);
      if (gst_is_cuda_memory (mem))
        GST_MEMORY_FLAG_UNSET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);
    }
    gst_buffer_unmap (task->gst_buffer, &task->info);
    gst_clear_buffer (&task->gst_buffer);
  }

  if (!object)
    return TRUE;

  task->object = nullptr;

  GST_TRACE_ID (task->id.c_str (), "Back to task queue %u", task->seq_num);

  /* Back to task queue */
  gst_nv_enc_task_ref (task);
  object->PushEmptyTask (task);

  return FALSE;
}

static void
gst_nv_enc_task_free (GstNvEncTask * task)
{
  GST_TRACE_ID (task->id.c_str (), "Freeing task %u", task->seq_num);

  delete task;
}

static GstNvEncTask *
gst_nv_enc_task_new (const std::string & id, guint seq_num)
{
  GstNvEncTask *task = new GstNvEncTask (id, seq_num);

  gst_mini_object_init (task, 0, GST_TYPE_NV_ENC_TASK, nullptr,
      (GstMiniObjectDisposeFunction) gst_nv_enc_task_dispose,
      (GstMiniObjectFreeFunction) gst_nv_enc_task_free);

  return task;
}

const gchar *
nvenc_status_to_string (NVENCSTATUS status)
{
#define CASE(err) \
    case err: \
    return G_STRINGIFY (err);

  switch (status) {
      CASE (NV_ENC_SUCCESS);
      CASE (NV_ENC_ERR_NO_ENCODE_DEVICE);
      CASE (NV_ENC_ERR_UNSUPPORTED_DEVICE);
      CASE (NV_ENC_ERR_INVALID_ENCODERDEVICE);
      CASE (NV_ENC_ERR_INVALID_DEVICE);
      CASE (NV_ENC_ERR_DEVICE_NOT_EXIST);
      CASE (NV_ENC_ERR_INVALID_PTR);
      CASE (NV_ENC_ERR_INVALID_EVENT);
      CASE (NV_ENC_ERR_INVALID_PARAM);
      CASE (NV_ENC_ERR_INVALID_CALL);
      CASE (NV_ENC_ERR_OUT_OF_MEMORY);
      CASE (NV_ENC_ERR_ENCODER_NOT_INITIALIZED);
      CASE (NV_ENC_ERR_UNSUPPORTED_PARAM);
      CASE (NV_ENC_ERR_LOCK_BUSY);
      CASE (NV_ENC_ERR_NOT_ENOUGH_BUFFER);
      CASE (NV_ENC_ERR_INVALID_VERSION);
      CASE (NV_ENC_ERR_MAP_FAILED);
      CASE (NV_ENC_ERR_NEED_MORE_INPUT);
      CASE (NV_ENC_ERR_ENCODER_BUSY);
      CASE (NV_ENC_ERR_EVENT_NOT_REGISTERD);
      CASE (NV_ENC_ERR_GENERIC);
      CASE (NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY);
      CASE (NV_ENC_ERR_UNIMPLEMENTED);
      CASE (NV_ENC_ERR_RESOURCE_REGISTER_FAILED);
      CASE (NV_ENC_ERR_RESOURCE_NOT_REGISTERED);
      CASE (NV_ENC_ERR_RESOURCE_NOT_MAPPED);
    default:
      break;
  }
#undef CASE

  return "Unknown";
}
