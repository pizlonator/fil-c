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
#include <gst/dxva/dxva-prelude.h>

G_BEGIN_DECLS

typedef struct _GstDxvaDecodingArgs GstDxvaDecodingArgs;
typedef struct _GstDxvaResolution GstDxvaResolution;

/**
 * GstDxvaDecodingArgs:
 *
 * Since: 1.24
 */
struct _GstDxvaDecodingArgs
{
  gpointer picture_params;
  gsize picture_params_size;

  gpointer slice_control;
  gsize slice_control_size;

  gpointer bitstream;
  gsize bitstream_size;

  gpointer inverse_quantization_matrix;
  gsize inverse_quantization_matrix_size;
};

/**
 * GstDxvaCodec:
 *
 * Since: 1.24
 */
typedef enum
{
  GST_DXVA_CODEC_NONE,
  GST_DXVA_CODEC_MPEG2,
  GST_DXVA_CODEC_H264,
  GST_DXVA_CODEC_H265,
  GST_DXVA_CODEC_VP8,
  GST_DXVA_CODEC_VP9,
  GST_DXVA_CODEC_AV1,

  /* the last of supported codec */
  GST_DXVA_CODEC_LAST
} GstDxvaCodec;

/**
 * GstDxvaResolution:
 *
 * Since: 1.24
 */
struct _GstDxvaResolution
{
  guint width;
  guint height;
};

static const GstDxvaResolution gst_dxva_resolutions[] = {
  {1920, 1088}, {2560, 1440}, {3840, 2160}, {4096, 2160},
  {7680, 4320}, {8192, 4320}, {15360, 8640}, {16384, 8640}
};

G_END_DECLS

