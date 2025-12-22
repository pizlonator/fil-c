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
#include <string>
#include <vector>
#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

/*
 * Communication Sequence
 *
 *              +--------+                      +--------+
 *              | client |                      | server |
 *              +--------+                      +--------+
 *                  |                               |
 *                  |                               |
 *                  +<---------- CONFIG ------------|
 *                  |                               |
 *                  +--------- NEED-DATA ---------->|
 *                  |                               +-------+
 *                  |                               |     Export
 *                  |                               |   CUDA memory
 *                  |                               +<------+
 *                  +<-------- HAVE-DATA -----------|
 *         +--------+                               |
 *       Import     |                               |
 *     CUDA memory  |                               |
 *         +------->+                               |
 *                  |--------- READ-DONE ---------->|
 *         +--------+                               |
 *      Release     |                               |
 *    CUDA memory   |                               |
 *         +--------|                               |
 *                  |-------- RELEASE-DATA -------->|
 *                  |                               |
 *                  +--------- NEED-DATA ---------->|
 *                  |                               |
 *                  |<----------- EOS --------------|
 *         +--------+                               |
 *    Cleanup all   |                               |
 * shared resources |                               |
 *         +--------|                               |
 *                  |------------ FIN ------------->|
 */

enum class GstCudaIpcPktType : guint8
{
  UNKNOWN,
  CONFIG,
  NEED_DATA,
  HAVE_DATA,
  READ_DONE,
  RELEASE_DATA,
  HAVE_MMAP_DATA,
  RELEASE_MMAP_DATA,
  EOS,
  FIN,
};

#ifdef G_OS_WIN32
typedef HANDLE GstCudaSharableHandle;
typedef DWORD GstCudaPid;
#define GST_CUDA_OS_HANDLE_FORMAT "p"
#else
typedef int GstCudaSharableHandle;
typedef pid_t GstCudaPid;
#define GST_CUDA_OS_HANDLE_FORMAT "d"
struct OVERLAPPED
{
  gpointer dummy;
};
#endif

#pragma pack(push, 1)
struct GstCudaIpcPacketHeader
{
  GstCudaIpcPktType type;
  guint32 payload_size;
  guint32 magic;
};

struct GstCudaIpcMemLayout
{
  guint32 size;
  guint32 max_size;
  guint32 pitch;
  guint32 offset[4];
};
#pragma pack(pop)

constexpr guint GST_CUDA_IPC_PKT_HEADER_SIZE = sizeof (GstCudaIpcPacketHeader);

bool gst_cuda_ipc_pkt_identify (std::vector<guint8> & buf,
                                 GstCudaIpcPacketHeader & header);

bool gst_cuda_ipc_pkt_build_config (std::vector<guint8> & buf,
                                     GstCudaPid pid,
                                     gboolean use_mmap,
                                     GstCaps * caps);

bool gst_cuda_ipc_pkt_parse_config (std::vector<guint8> & buf,
                                    GstCudaPid * pid,
                                    gboolean * use_mmap,
                                    GstCaps ** caps);

void gst_cuda_ipc_pkt_build_need_data (std::vector<guint8> & buf);

bool gst_cuda_ipc_pkt_build_have_data (std::vector<guint8> & buf,
                                       GstClockTime pts,
                                       const GstVideoInfo & info,
                                       const CUipcMemHandle & handle,
                                       GstCaps * caps,
                                       const std::vector<guint8> & meta);

bool gst_cuda_ipc_pkt_parse_have_data (const std::vector<guint8> & buf,
                                       GstClockTime & pts,
                                       GstCudaIpcMemLayout & layout,
                                       CUipcMemHandle & handle,
                                       GstCaps ** caps,
                                       std::vector<guint8> & meta);

bool gst_cuda_ipc_pkt_build_have_mmap_data (std::vector<guint8> & buf,
                                            GstClockTime pts,
                                            const GstVideoInfo & info,
                                            guint32 max_size,
                                            GstCudaSharableHandle handle,
                                            GstCaps * caps,
                                            const std::vector<guint8> & meta);

bool gst_cuda_ipc_pkt_parse_have_mmap_data (const std::vector<guint8> & buf,
                                            GstClockTime & pts,
                                            GstCudaIpcMemLayout & layout,
                                            GstCudaSharableHandle * handle,
                                            GstCaps ** caps,
                                            std::vector<guint8> & meta);

void gst_cuda_ipc_pkt_build_read_done (std::vector<guint8> & buf);

void gst_cuda_ipc_pkt_build_release_data (std::vector<guint8> & buf,
                                          const CUipcMemHandle & handle);

bool gst_cuda_ipc_pkt_parse_release_data (std::vector<guint8> & buf,
                                          CUipcMemHandle & handle);

void gst_cuda_ipc_pkt_build_release_mmap_data (std::vector<guint8> & buf,
                                               GstCudaSharableHandle handle);

bool gst_cuda_ipc_pkt_parse_release_mmap_data (std::vector<guint8> & buf,
                                               GstCudaSharableHandle * handle);


void gst_cuda_ipc_pkt_build_eos (std::vector<guint8> & buf);

void gst_cuda_ipc_pkt_build_fin (std::vector<guint8> & buf);

std::string gst_cuda_ipc_mem_handle_to_string (const CUipcMemHandle & handle);

bool gst_cuda_ipc_clock_is_system (GstClock * clock);

std::string gst_cuda_ipc_win32_error_to_string (guint err);

bool gst_cuda_ipc_handle_is_equal (const CUipcMemHandle & handle,
                                   const CUipcMemHandle & other);
