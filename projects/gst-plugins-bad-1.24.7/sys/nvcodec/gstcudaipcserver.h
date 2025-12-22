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
#include <gst/cuda/gstcuda.h>
#include "gstcudaipc.h"
#include <memory>
#include <vector>
#include <string.h>

G_BEGIN_DECLS

#define GST_TYPE_CUDA_IPC_SERVER             (gst_cuda_ipc_server_get_type())
#define GST_CUDA_IPC_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_IPC_SERVER,GstCudaIpcServer))
#define GST_CUDA_IPC_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_IPC_SERVER,GstCudaIpcServerClass))
#define GST_CUDA_IPC_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_IPC_SERVER,GstCudaIpcServerClass))
#define GST_IS_CUDA_IPC_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_IPC_SERVER))
#define GST_IS_CUDA_IPC_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_IPC_SERVER))

struct GstCudaIpcServerData;
struct GstCudaIpcServerConn;
struct GstCudaIpcServerPrivate;

enum GstCudaIpcMode
{
  GST_CUDA_IPC_LEGACY,
  GST_CUDA_IPC_MMAP,
};

#define GST_TYPE_CUDA_IPC_MODE (gst_cuda_ipc_mode_get_type())
GType gst_cuda_ipc_mode_get_type (void);

struct GstCudaIpcServer
{
  GstObject parent;

  GstCudaContext *context;
  GstCudaIpcMode ipc_mode;
  GstCudaPid pid;

  GstCudaIpcServerPrivate *priv;
};

struct GstCudaIpcServerClass
{
  GstObjectClass parent_class;

  void  (*loop)       (GstCudaIpcServer * server);

  void  (*terminate)  (GstCudaIpcServer * server);

  void  (*invoke)     (GstCudaIpcServer * server);

  bool  (*wait_msg)   (GstCudaIpcServer * server,
                       GstCudaIpcServerConn * conn);

  bool  (*send_msg)   (GstCudaIpcServer * server,
                       GstCudaIpcServerConn * conn);

  bool  (*send_mmap_msg) (GstCudaIpcServer * server,
                          GstCudaIpcServerConn * conn,
                          GstCudaSharableHandle handle);
};

GType         gst_cuda_ipc_server_get_type (void);

GstFlowReturn gst_cuda_ipc_server_send_data (GstCudaIpcServer * server,
                                             GstSample * sample,
                                             const GstVideoInfo & info,
                                             const CUipcMemHandle & handle,
                                             GstClockTime pts,
                                             GByteArray * meta);

GstFlowReturn gst_cuda_ipc_server_send_mmap_data (GstCudaIpcServer * server,
                                                  GstSample * sample,
                                                  const GstVideoInfo & info,
                                                  GstCudaSharableHandle handle,
                                                  GstClockTime pts,
                                                  GByteArray * meta);


void          gst_cuda_ipc_server_stop      (GstCudaIpcServer * server);


/* subclass methods */
void gst_cuda_ipc_server_run (GstCudaIpcServer * server);

void gst_cuda_ipc_server_wait_msg_finish (GstCudaIpcServer * server,
                                          GstCudaIpcServerConn * conn,
                                          bool result);

void gst_cuda_ipc_server_send_msg_finish (GstCudaIpcServer * server,
                                          GstCudaIpcServerConn * conn,
                                          bool result);

void gst_cuda_ipc_server_on_incoming_connection (GstCudaIpcServer * server,
                                                 std::shared_ptr<GstCudaIpcServerConn> conn);

void gst_cuda_ipc_server_on_idle (GstCudaIpcServer * server);

void gst_cuda_ipc_server_abort (GstCudaIpcServer * server);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstCudaIpcServer, gst_object_unref)

G_END_DECLS

struct GstCudaIpcServerData
{
  ~GstCudaIpcServerData ()
  {
    if (sample)
      gst_sample_unref (sample);
  }

  GstSample *sample;
  GstVideoInfo info;
  CUipcMemHandle handle;
  GstCudaSharableHandle os_handle;
  GstClockTime pts;
  std::vector<guint8> meta;
  guint64 seq_num;
};

struct GstCudaIpcServerConn : public OVERLAPPED
{
  GstCudaIpcServerConn ()
  {
    client_msg.resize (GST_CUDA_IPC_PKT_HEADER_SIZE);
    server_msg.resize (GST_CUDA_IPC_PKT_HEADER_SIZE);
  }

  virtual ~GstCudaIpcServerConn()
  {
    gst_clear_object (&context);
    gst_clear_caps (&caps);
  }

  virtual void CloseConn () = 0;

  GstCudaIpcServer *server;

  GstCudaContext *context = nullptr;

  GstCudaIpcPktType type;
  std::vector<guint8> client_msg;
  std::vector<guint8> server_msg;
  std::shared_ptr<GstCudaIpcServerData> data;
  std::vector<std::shared_ptr<GstCudaIpcServerData>> peer_handles;
  GstCaps *caps = nullptr;
  guint64 seq_num = 0;
  guint id;
  bool eos = false;
  bool pending_have_data = false;
  bool configured = false;
};
