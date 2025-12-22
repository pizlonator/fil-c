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

#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_CUDA_FORMATS \
    "{ I420, YV12, NV12, NV21, P010_10LE, P012_LE, P016_LE, I420_10LE, I420_12LE, Y444, " \
    "Y444_10LE, Y444_12LE, Y444_16LE, BGRA, RGBA, RGBx, BGRx, ARGB, ABGR, RGB, " \
    "BGR, BGR10A2_LE, RGB10A2_LE, Y42B, I422_10LE, I422_12LE, YUY2, UYVY, RGBP, " \
    "BGRP, GBR, GBR_10LE, GBR_12LE, GBR_16LE, GBRA }"

#define GST_CUDA_GL_FORMATS \
    "{ I420, YV12, NV12, NV21, P010_10LE, P012_LE, P016_LE, Y444, " \
    "BGRA, RGBA, RGBx, BGRx, ARGB, ABGR, RGB, BGR, BGR10A2_LE, RGB10A2_LE, " \
    "YUY2, UYVY, RGBP, BGRP, GBR, GBRA }"

#define GST_CUDA_D3D11_FORMATS \
    "{ I420, YV12, I420_10LE, I420_12LE, Y444, Y444_10LE, Y444_12LE, Y444_16LE, " \
    "BGRA, RGBA, BGRx, RGBx, Y42B, I422_10LE, I422_12LE, GBR, GBR, GBR_10LE, " \
    "GBR_12LE, GBR_16LE }"

#define GST_CUDA_NVMM_FORMATS \
    "{ I420, YV12, NV12, NV21, P010_10LE, Y444, " \
    "BGRA, RGBA, RGBx, BGRx, ARGB, ABGR, RGB, BGR }"

G_END_DECLS
