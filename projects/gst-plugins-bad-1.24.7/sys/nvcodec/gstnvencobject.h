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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/cuda/gstcuda.h>
#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#endif
#include "nvEncodeAPI.h"
#include "gstnvenc.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <set>
#include <string>
#include <atomic>

G_BEGIN_DECLS

#define GST_TYPE_NV_ENC_BUFFER (gst_nv_enc_buffer_get_type ())
struct GstNvEncBuffer;

GType gst_nv_enc_buffer_get_type (void);

NVENCSTATUS   gst_nv_enc_buffer_lock (GstNvEncBuffer * buffer,
                                      gpointer * data,
                                      guint32 * pitch);

void          gst_nv_enc_buffer_unlock (GstNvEncBuffer * buffer);


static inline GstNvEncBuffer *
gst_nv_enc_buffer_ref (GstNvEncBuffer * buffer)
{
  return (GstNvEncBuffer *)
      gst_mini_object_ref (GST_MINI_OBJECT_CAST (buffer));
}

static inline void
gst_nv_enc_buffer_unref (GstNvEncBuffer * buffer)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (buffer));
}

static inline void
gst_clear_nv_encoder_buffer (GstNvEncBuffer ** buffer)
{
  if (buffer && *buffer) {
    gst_nv_enc_buffer_unref (*buffer);
    *buffer = NULL;
  }
}

#define GST_TYPE_NV_ENC_RESOURCE (gst_nv_enc_resource_get_type ())
struct GstNvEncResource;

GType gst_nv_enc_resource_get_type (void);

static inline GstNvEncResource *
gst_nv_enc_resource_ref (GstNvEncResource * resource)
{
  return (GstNvEncResource *)
      gst_mini_object_ref (GST_MINI_OBJECT_CAST (resource));
}

static inline void
gst_nv_enc_resource_unref (GstNvEncResource * resource)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (resource));
}

static inline void
gst_clear_nv_encoder_resource (GstNvEncResource ** resource)
{
  if (resource && *resource) {
    gst_nv_enc_resource_unref (*resource);
    *resource = NULL;
  }
}

#define GST_TYPE_NV_ENC_TASK (gst_nv_enc_task_get_type ())
struct GstNvEncTask;

GType gst_nv_enc_task_get_type (void);

gboolean    gst_nv_enc_task_set_buffer (GstNvEncTask * task,
                                        GstNvEncBuffer * buffer);

gboolean    gst_nv_enc_task_set_resource (GstNvEncTask * task,
                                          GstBuffer * buffer,
                                          GstNvEncResource * resource);

GArray *    gst_nv_enc_task_get_sei_payload (GstNvEncTask * task);

NVENCSTATUS gst_nv_enc_task_lock_bitstream (GstNvEncTask * task,
                                            NV_ENC_LOCK_BITSTREAM * bitstream);

void gst_nv_enc_task_unlock_bitstream (GstNvEncTask * task);

static inline GstNvEncTask *
gst_nv_enc_task_ref (GstNvEncTask * task)
{
  return (GstNvEncTask *)
      gst_mini_object_ref (GST_MINI_OBJECT_CAST (task));
}

static inline void
gst_nv_enc_task_unref (GstNvEncTask * task)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (task));
}

const gchar * nvenc_status_to_string (NVENCSTATUS status);

G_END_DECLS

enum GstNvEncCodec
{
  GST_NV_ENC_CODEC_H264,
  GST_NV_ENC_CODEC_H265,
};

class GstNvEncObject : public std::enable_shared_from_this <GstNvEncObject>
{
public:
  static bool IsSuccess (NVENCSTATUS status,
                         GstNvEncObject * self,
                         const gchar * file,
                         const gchar * function,
                         gint line);

  static std::shared_ptr<GstNvEncObject>
  CreateInstance (GstElement * client,
                  GstObject * device,
                  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS * params);

  ~GstNvEncObject ();

  gpointer      GetHandle ();

  guint         GetTaskSize ();

  NVENCSTATUS   InitSession (NV_ENC_INITIALIZE_PARAMS * params,
                             GstCudaStream * stream,
                             const GstVideoInfo * info,
                             guint pool_size);

  NVENCSTATUS   Reconfigure (NV_ENC_RECONFIGURE_PARAMS * params);

  void          SetFlushing (bool flushing);

  NVENCSTATUS   Encode      (GstVideoCodecFrame * codec_frame,
                             NV_ENC_PIC_STRUCT pic_struct,
                             GstNvEncTask * task);

  NVENCSTATUS   Drain       (GstNvEncTask * task);

  GstFlowReturn GetOutput   (GstNvEncTask ** task);

  NVENCSTATUS   LockBitstream (NV_ENC_LOCK_BITSTREAM * bitstream);

  NVENCSTATUS   UnlockBitstream (NV_ENC_OUTPUT_PTR output_ptr);

  NVENCSTATUS   AcquireBuffer (GstNvEncBuffer ** buffer);

  NVENCSTATUS   AcquireResource (GstMemory * mem,
                                 GstNvEncResource ** resource);

  GstFlowReturn AcquireTask (GstNvEncTask ** task,
                             bool force);

  void PushEmptyTask (GstNvEncTask * task);

  void PushEmptyBuffer (GstNvEncBuffer * buffer);

  void ReleaseResource (GstNvEncResource * resource);

  void DeactivateResource (GstNvEncResource * resource);

  bool DeviceLock ();

  bool DeviceUnlock ();

private:
  void releaseResourceUnlocked (GstNvEncResource * resource);

  void releaseTaskUnlocked (GstNvEncTask * task);

  NVENCSTATUS acquireResourceCuda (GstMemory * mem,
                                   GstNvEncResource ** resource);

#ifdef G_OS_WIN32
  NVENCSTATUS acquireResourceD3D11 (GstMemory * mem,
                                    GstNvEncResource ** resource);
#endif

  void runResourceGC ();

private:
  std::string id_;
  std::mutex lock_;
  std::recursive_mutex resource_lock_;
  std::condition_variable cond_;
  /* holding unused GstNvEncBuffer object, holding ownership */
  std::queue <GstNvEncBuffer *> buffer_queue_;

  /* GstNvEncResource resource is always owned by GstMemory.
   * below two data struct will track the resource's life cycle  */

  /* list of all registered GstNvEncResource, without ownership */
  std::set <GstNvEncResource *> resource_queue_;

  /* list of GstNvEncResource in task_queue */
  std::set <GstNvEncResource *> active_resource_queue_;
  std::queue <GstNvEncTask *> task_queue_;
  std::queue <GstNvEncTask *> pending_task_queue_;
  std::queue <GstNvEncTask *> empty_task_queue_;
  gint64 user_token_;
  GstCudaContext *context_ = nullptr;
  GstCudaStream *stream_ = nullptr;
#ifdef G_OS_WIN32
  GstD3D11Device *device_ = nullptr;
#endif
  GstVideoInfo info_;
  gpointer session_ = nullptr;
  bool initialized_ = false;
  bool flushing_ = false;
  guint task_size_ = 0;
  guint lookahead_ = 0;

  NV_ENC_DEVICE_TYPE device_type_ = NV_ENC_DEVICE_TYPE_CUDA;
  NV_ENC_BUFFER_FORMAT buffer_format_ = NV_ENC_BUFFER_FORMAT_UNDEFINED;
  GstNvEncCodec codec_;

  std::atomic<guint> buffer_seq_;
  std::atomic<guint> resource_seq_;
  std::atomic<guint> task_seq_;
};
