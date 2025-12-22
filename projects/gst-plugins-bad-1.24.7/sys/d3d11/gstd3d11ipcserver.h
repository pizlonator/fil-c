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
#include <gst/d3d11/gstd3d11.h>
#include "gstd3d11ipc.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D11_IPC_SERVER (gst_d3d11_ipc_server_get_type())
G_DECLARE_FINAL_TYPE (GstD3D11IpcServer, gst_d3d11_ipc_server,
    GST, D3D11_IPC_SERVER, GstObject);

GstD3D11IpcServer * gst_d3d11_ipc_server_new (const std::string & address,
                                              gint64 adapter_luid);

GstFlowReturn       gst_d3d11_ipc_server_send_data (GstD3D11IpcServer * server,
                                                    GstSample * sample,
                                                    const GstD3D11IpcMemLayout & layout,
                                                    HANDLE handle,
                                                    GstClockTime pts);

void                gst_d3d11_ipc_server_stop      (GstD3D11IpcServer * server);

gint64              gst_d3d11_ipc_server_get_adapter_luid (GstD3D11IpcServer * server);

G_END_DECLS
