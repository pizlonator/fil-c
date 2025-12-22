/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_ENCODER            (gst_d3d12_encoder_get_type())
#define GST_D3D12_ENCODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D12_ENCODER,GstD3D12Encoder))
#define GST_D3D12_ENCODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D12_ENCODER,GstD3D12EncoderClass))
#define GST_D3D12_ENCODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_D3D12_ENCODER,GstD3D12EncoderClass))
#define GST_IS_D3D12_ENCODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D12_ENCODER))
#define GST_IS_D3D12_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D12_ENCODER))

struct GstD3D12EncoderPrivate;

struct GstD3D12EncoderConfig
{
  D3D12_VIDEO_ENCODER_PROFILE_DESC profile_desc;
  D3D12_VIDEO_ENCODER_LEVEL_SETTING level;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION codec_config;
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA layout;
  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE gop_struct;
  D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC resolution;
  D3D12_VIDEO_ENCODER_SUPPORT_FLAGS support_flags;
  D3D12_VIDEO_ENCODER_RATE_CONTROL_CQP cqp;;
  D3D12_VIDEO_ENCODER_RATE_CONTROL_CBR cbr;
  D3D12_VIDEO_ENCODER_RATE_CONTROL_VBR vbr;
  D3D12_VIDEO_ENCODER_RATE_CONTROL_QVBR qvbr;
  D3D12_VIDEO_ENCODER_RATE_CONTROL rate_control;
  guint max_subregions;
};

enum GstD3D12EncoderSeiInsertMode
{
  GST_D3D12_ENCODER_SEI_INSERT,
  GST_D3D12_ENCODER_SEI_INSERT_AND_DROP,
  GST_D3D12_ENCODER_SEI_DISABLED,
};

#define GST_TYPE_D3D12_ENCODER_RATE_CONTROL (gst_d3d12_encoder_rate_control_get_type ())
GType gst_d3d12_encoder_rate_control_get_type (void);

#define GST_TYPE_D3D12_ENCODER_RATE_CONTROL_SUPPORT (gst_d3d12_encoder_rate_control_support_get_type ())
GType gst_d3d12_encoder_rate_control_support_get_type (void);

#define GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT (gst_d3d12_encoder_subregion_layout_get_type())
GType gst_d3d12_encoder_subregion_layout_get_type (void);

#define GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT_SUPPORT (gst_d3d12_encoder_subregion_layout_support_get_type ())
GType gst_d3d12_encoder_subregion_layout_support_get_type (void);

#define GST_TYPE_D3D12_ENCODER_SEI_INSERT_MODE (gst_d3d12_encoder_sei_insert_mode_get_type ())
GType gst_d3d12_encoder_sei_insert_mode_get_type (void);

struct GstD3D12Encoder
{
  GstVideoEncoder parent;

  GstD3D12Device *device;

  GstD3D12EncoderPrivate *priv;
};

struct GstD3D12EncoderClass
{
  GstVideoEncoderClass parent_class;

  D3D12_VIDEO_ENCODER_CODEC codec;
  gint64 adapter_luid;
  guint device_id;
  guint vendor_id;

  gboolean  (*new_sequence)   (GstD3D12Encoder * encoder,
                               ID3D12VideoDevice * video_device,
                               GstVideoCodecState * state,
                               GstD3D12EncoderConfig * config);

  gboolean  (*start_frame)    (GstD3D12Encoder * encoder,
                               ID3D12VideoDevice * video_device,
                               GstVideoCodecFrame * frame,
                               D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_DESC * seq_ctrl,
                               D3D12_VIDEO_ENCODER_PICTURE_CONTROL_DESC * picture_ctrl,
                               D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * recon_pic,
                               GstD3D12EncoderConfig * config,
                               gboolean * need_new_session);

  gboolean  (*end_frame)      (GstD3D12Encoder * encoder);
};

GType gst_d3d12_encoder_get_type (void);

gboolean gst_d3d12_encoder_check_needs_new_session (D3D12_VIDEO_ENCODER_SUPPORT_FLAGS support_flags,
                                                    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS seq_flags);

#define CHECK_SUPPORT_FLAG(flags, f) \
    ((flags & D3D12_VIDEO_ENCODER_SUPPORT_FLAG_ ##f) != 0)

G_END_DECLS
