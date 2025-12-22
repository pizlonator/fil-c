/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <windows.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/*
 * Communication Sequence
 *
 *            +--------+                      +--------+
 *            | client |                      | server |
 *            +--------+                      +--------+
 *                |                               |
 *                +--------- NEED-DATA ---------->|
 *                |                               +-------+
 *                |                               |  prepare named
 *                |                               |  shared-memory
 *                |                               +<------+
 *                +<-- HAVE-DATA (w/ shm name) ---|
 *       +--------+                               |
 *   Open named   |                               |
 *  shared-memory |                               |
 *       +------->+                               |
 *                |--------- READ-DONE ---------->|
 *                |                               |
 *       +--------+                               |
 *    release     |                               |
 *  shared-memory |                               |
 *       +--------|                               |
 *                |------- RELEASE-Data---------->|
 */

typedef enum
{
  WIN32_IPC_PKT_UNKNOWN,
  WIN32_IPC_PKT_NEED_DATA,
  WIN32_IPC_PKT_HAVE_DATA,
  WIN32_IPC_PKT_READ_DONE,
  WIN32_IPC_PKT_RELEASE_DATA,
} Win32IpcPktType;

/* Same as GstVideoFormat */
typedef enum
{
  WIN32_IPC_VIDEO_FORMAT_UNKNOWN,
  WIN32_IPC_VIDEO_FORMAT_ENCODED,
  WIN32_IPC_VIDEO_FORMAT_I420,
  WIN32_IPC_VIDEO_FORMAT_YV12,
  WIN32_IPC_VIDEO_FORMAT_YUY2,
  WIN32_IPC_VIDEO_FORMAT_UYVY,
  WIN32_IPC_VIDEO_FORMAT_AYUV,
  WIN32_IPC_VIDEO_FORMAT_RGBx,
  WIN32_IPC_VIDEO_FORMAT_BGRx,
  WIN32_IPC_VIDEO_FORMAT_xRGB,
  WIN32_IPC_VIDEO_FORMAT_xBGR,
  WIN32_IPC_VIDEO_FORMAT_RGBA,
  WIN32_IPC_VIDEO_FORMAT_BGRA,
  WIN32_IPC_VIDEO_FORMAT_ARGB,
  WIN32_IPC_VIDEO_FORMAT_ABGR,
  WIN32_IPC_VIDEO_FORMAT_RGB,
  WIN32_IPC_VIDEO_FORMAT_BGR,
  WIN32_IPC_VIDEO_FORMAT_Y41B,
  WIN32_IPC_VIDEO_FORMAT_Y42B,
  WIN32_IPC_VIDEO_FORMAT_YVYU,
  WIN32_IPC_VIDEO_FORMAT_Y444,
  WIN32_IPC_VIDEO_FORMAT_v210,
  WIN32_IPC_VIDEO_FORMAT_v216,
  WIN32_IPC_VIDEO_FORMAT_NV12,
  WIN32_IPC_VIDEO_FORMAT_NV21,
  WIN32_IPC_VIDEO_FORMAT_GRAY8,
  WIN32_IPC_VIDEO_FORMAT_GRAY16_BE,
  WIN32_IPC_VIDEO_FORMAT_GRAY16_LE,
  WIN32_IPC_VIDEO_FORMAT_v308,
  WIN32_IPC_VIDEO_FORMAT_RGB16,
  WIN32_IPC_VIDEO_FORMAT_BGR16,
  WIN32_IPC_VIDEO_FORMAT_RGB15,
  WIN32_IPC_VIDEO_FORMAT_BGR15,
  WIN32_IPC_VIDEO_FORMAT_UYVP,
  WIN32_IPC_VIDEO_FORMAT_A420,
  WIN32_IPC_VIDEO_FORMAT_RGB8P,
  WIN32_IPC_VIDEO_FORMAT_YUV9,
  WIN32_IPC_VIDEO_FORMAT_YVU9,
  WIN32_IPC_VIDEO_FORMAT_IYU1,
  WIN32_IPC_VIDEO_FORMAT_ARGB64,
  WIN32_IPC_VIDEO_FORMAT_AYUV64,
  WIN32_IPC_VIDEO_FORMAT_r210,
  WIN32_IPC_VIDEO_FORMAT_I420_10BE,
  WIN32_IPC_VIDEO_FORMAT_I420_10LE,
  WIN32_IPC_VIDEO_FORMAT_I422_10BE,
  WIN32_IPC_VIDEO_FORMAT_I422_10LE,
  WIN32_IPC_VIDEO_FORMAT_Y444_10BE,
  WIN32_IPC_VIDEO_FORMAT_Y444_10LE,
  WIN32_IPC_VIDEO_FORMAT_GBR,
  WIN32_IPC_VIDEO_FORMAT_GBR_10BE,
  WIN32_IPC_VIDEO_FORMAT_GBR_10LE,
  WIN32_IPC_VIDEO_FORMAT_NV16,
  WIN32_IPC_VIDEO_FORMAT_NV24,
  WIN32_IPC_VIDEO_FORMAT_NV12_64Z32,
  WIN32_IPC_VIDEO_FORMAT_A420_10BE,
  WIN32_IPC_VIDEO_FORMAT_A420_10LE,
  WIN32_IPC_VIDEO_FORMAT_A422_10BE,
  WIN32_IPC_VIDEO_FORMAT_A422_10LE,
  WIN32_IPC_VIDEO_FORMAT_A444_10BE,
  WIN32_IPC_VIDEO_FORMAT_A444_10LE,
  WIN32_IPC_VIDEO_FORMAT_NV61,
  WIN32_IPC_VIDEO_FORMAT_P010_10BE,
  WIN32_IPC_VIDEO_FORMAT_P010_10LE,
  WIN32_IPC_VIDEO_FORMAT_IYU2,
  WIN32_IPC_VIDEO_FORMAT_VYUY,
  WIN32_IPC_VIDEO_FORMAT_GBRA,
  WIN32_IPC_VIDEO_FORMAT_GBRA_10BE,
  WIN32_IPC_VIDEO_FORMAT_GBRA_10LE,
  WIN32_IPC_VIDEO_FORMAT_GBR_12BE,
  WIN32_IPC_VIDEO_FORMAT_GBR_12LE,
  WIN32_IPC_VIDEO_FORMAT_GBRA_12BE,
  WIN32_IPC_VIDEO_FORMAT_GBRA_12LE,
  WIN32_IPC_VIDEO_FORMAT_I420_12BE,
  WIN32_IPC_VIDEO_FORMAT_I420_12LE,
  WIN32_IPC_VIDEO_FORMAT_I422_12BE,
  WIN32_IPC_VIDEO_FORMAT_I422_12LE,
  WIN32_IPC_VIDEO_FORMAT_Y444_12BE,
  WIN32_IPC_VIDEO_FORMAT_Y444_12LE,
  WIN32_IPC_VIDEO_FORMAT_GRAY10_LE32,
  WIN32_IPC_VIDEO_FORMAT_NV12_10LE32,
  WIN32_IPC_VIDEO_FORMAT_NV16_10LE32,
  WIN32_IPC_VIDEO_FORMAT_NV12_10LE40,
  WIN32_IPC_VIDEO_FORMAT_Y210,
  WIN32_IPC_VIDEO_FORMAT_Y410,
  WIN32_IPC_VIDEO_FORMAT_VUYA,
  WIN32_IPC_VIDEO_FORMAT_BGR10A2_LE,
  WIN32_IPC_VIDEO_FORMAT_RGB10A2_LE,
  WIN32_IPC_VIDEO_FORMAT_Y444_16BE,
  WIN32_IPC_VIDEO_FORMAT_Y444_16LE,
  WIN32_IPC_VIDEO_FORMAT_P016_BE,
  WIN32_IPC_VIDEO_FORMAT_P016_LE,
  WIN32_IPC_VIDEO_FORMAT_P012_BE,
  WIN32_IPC_VIDEO_FORMAT_P012_LE,
  WIN32_IPC_VIDEO_FORMAT_Y212_BE,
  WIN32_IPC_VIDEO_FORMAT_Y212_LE,
  WIN32_IPC_VIDEO_FORMAT_Y412_BE,
  WIN32_IPC_VIDEO_FORMAT_Y412_LE,
  WIN32_IPC_VIDEO_FORMAT_NV12_4L4,
  WIN32_IPC_VIDEO_FORMAT_NV12_32L32,
  WIN32_IPC_VIDEO_FORMAT_RGBP,
  WIN32_IPC_VIDEO_FORMAT_BGRP,
  WIN32_IPC_VIDEO_FORMAT_AV12,
  WIN32_IPC_VIDEO_FORMAT_ARGB64_LE,
  WIN32_IPC_VIDEO_FORMAT_ARGB64_BE,
  WIN32_IPC_VIDEO_FORMAT_RGBA64_LE,
  WIN32_IPC_VIDEO_FORMAT_RGBA64_BE,
  WIN32_IPC_VIDEO_FORMAT_BGRA64_LE,
  WIN32_IPC_VIDEO_FORMAT_BGRA64_BE,
  WIN32_IPC_VIDEO_FORMAT_ABGR64_LE,
  WIN32_IPC_VIDEO_FORMAT_ABGR64_BE,
  WIN32_IPC_VIDEO_FORMAT_NV12_16L32S,
  WIN32_IPC_VIDEO_FORMAT_NV12_8L128,
  WIN32_IPC_VIDEO_FORMAT_NV12_10BE_8L128,
} Win32IpcVideoFormat;

typedef struct
{
  Win32IpcVideoFormat format;
  UINT32 width;
  UINT32 height;
  UINT32 fps_n;
  UINT32 fps_d;
  UINT32 par_n;
  UINT32 par_d;
  /* the size of memory */
  UINT64 size;
  /* plane offsets */
  UINT64 offset[4];
  /* stride of each plane */
  UINT32 stride[4];
  /* QPC time */
  UINT64 qpc;
} Win32IpcVideoInfo;

/* 1 byte (type) + 8 byte (seq-num) */
#define WIN32_IPC_PKT_NEED_DATA_SIZE 9

/* 1 byte (type) + 8 byte (seq-num) + N bytes (name) + 4 (format) +
 * 4 (width) + 4 (height) + 4 (fps_n) + 4 (fps_d) + 4 (par_n) + 4 (par_d) +
 * 8 (size) + 8 * 4 (offset) + 4 * 4 (stride) + 8 (timestamp) */
#define WIN32_IPC_PKT_HAVE_DATA_SIZE 101

/* 1 byte (type) + 8 byte (seq-num) */
#define WIN32_IPC_PKT_READ_DONE_SIZE 5

const char *     win32_ipc_pkt_type_to_string  (Win32IpcPktType type);

Win32IpcPktType  win32_ipc_pkt_type_from_raw   (UINT8 type);

UINT8            win32_ipc_pkt_type_to_raw     (Win32IpcPktType type);

UINT32           win32_ipc_pkt_build_need_data (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 seq_num);

BOOL             win32_ipc_pkt_parse_need_data (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 * seq_num);

UINT32           win32_ipc_pkt_build_have_data (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 seq_num,
                                                const char * mmf_name,
                                                const Win32IpcVideoInfo * info);

BOOL             win32_ipc_pkt_parse_have_data (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 * seq_num,
                                                char * mmf_name,
                                                Win32IpcVideoInfo * info);

UINT32           win32_ipc_pkt_build_read_done (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 seq_num);

BOOL             win32_ipc_pkt_parse_read_done (UINT8 * pkt,
                                                UINT32 pkt_size,
                                                UINT64 * seq_num);

UINT32           win32_ipc_pkt_build_release_data (UINT8 * pkt,
                                                   UINT32 pkt_size,
                                                   UINT64 seq_num,
                                                   const char * mmf_name);

BOOL             win32_ipc_pkt_parse_release_data (UINT8 * pkt,
                                                   UINT32 pkt_size,
                                                   UINT64 * seq_num,
                                                   char * mmf_name);

G_END_DECLS

