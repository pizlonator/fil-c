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

G_BEGIN_DECLS

#define GST_TYPE_CUDA_IPC_CLIENT             (gst_cuda_ipc_client_get_type())
#define GST_CUDA_IPC_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_IPC_CLIENT,GstCudaIpcClient))
#define GST_CUDA_IPC_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_IPC_CLIENT,GstCudaIpcClientClass))
#define GST_CUDA_IPC_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_IPC_CLIENT,GstCudaIpcClientClass))
#define GST_IS_CUDA_IPC_CLIENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_IPC_CLIENT))
#define GST_IS_CUDA_IPC_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_IPC_CLIENT))

struct GstCudaIpcClientConn;
struct GstCudaIpcClientPrivate;

enum GstCudaIpcIOMode
{
  GST_CUDA_IPC_IO_COPY,
  GST_CUDA_IPC_IO_IMPORT,
};

#define GST_TYPE_CUDA_IPC_IO_MODE (gst_cuda_ipc_io_mode_get_type ())
GType gst_cuda_ipc_io_mode_get_type (void);

struct GstCudaIpcClient
{
  GstObject parent;

  GstCudaContext *context;
  GstCudaStream *stream;
  GstCudaIpcIOMode io_mode;
  guint buffer_size;

  GstCudaIpcClientPrivate *priv;
};

struct GstCudaIpcClientClass
{
  GstObjectClass parent_class;

  bool  (*send_msg)      (GstCudaIpcClient * client,
                          GstCudaIpcClientConn * conn);

  bool  (*wait_msg)      (GstCudaIpcClient * client,
                          GstCudaIpcClientConn * conn);

  void  (*terminate)     (GstCudaIpcClient * client);

  void  (*invoke)        (GstCudaIpcClient * client);

  void  (*set_flushing)  (GstCudaIpcClient * client,
                          bool flushing);

  void  (*loop)          (GstCudaIpcClient * client);

  bool  (*config)        (GstCudaIpcClient * client,
                          GstCudaPid pid,
                          gboolean use_mmap);
};

GType             gst_cuda_ipc_client_get_type (void);

GstFlowReturn     gst_cuda_ipc_client_get_sample (GstCudaIpcClient * client,
                                                  GstSample ** sample);

void              gst_cuda_ipc_client_set_flushing (GstCudaIpcClient * client,
                                                    bool flushing);

GstCaps *         gst_cuda_ipc_client_get_caps     (GstCudaIpcClient * client);

GstFlowReturn     gst_cuda_ipc_client_run          (GstCudaIpcClient * client);

void              gst_cuda_ipc_client_stop         (GstCudaIpcClient * client);

/* subclass methods */
void              gst_cuda_ipc_client_new_connection (GstCudaIpcClient * client,
                                                      std::shared_ptr<GstCudaIpcClientConn> conn);

void              gst_cuda_ipc_client_send_msg_finish (GstCudaIpcClient * client,
                                                       bool result);

void              gst_cuda_ipc_client_wait_msg_finish (GstCudaIpcClient * client,
                                                       bool result);

void              gst_cuda_ipc_client_have_mmap_data  (GstCudaIpcClient * client,
                                                       GstClockTime pts,
                                                       const GstCudaIpcMemLayout & layout,
                                                       GstCaps * caps,
                                                       GstCudaSharableHandle server_handle,
                                                       GstCudaSharableHandle client_handle,
                                                       std::vector <guint8> & meta);

void              gst_cuda_ipc_client_on_idle         (GstCudaIpcClient * client);

void              gst_cuda_ipc_client_abort           (GstCudaIpcClient * client);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstCudaIpcClient, gst_object_unref)

G_END_DECLS

struct GstCudaIpcClientConn : public OVERLAPPED
{
  GstCudaIpcClientConn ()
  {
    client_msg.resize (GST_CUDA_IPC_PKT_HEADER_SIZE);
    server_msg.resize (GST_CUDA_IPC_PKT_HEADER_SIZE);
  }

  virtual ~GstCudaIpcClientConn()
  {
    gst_clear_object (&context);
  }

  GstCudaIpcClient *client;

  GstCudaContext *context = nullptr;

  GstCudaIpcPktType type;
  std::vector<guint8> client_msg;
  std::vector<guint8> server_msg;
};
