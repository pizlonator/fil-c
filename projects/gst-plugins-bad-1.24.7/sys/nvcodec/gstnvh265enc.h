/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
 * Copyright (C) 2018 Seungha Yang <pudding8757@gmail.com>
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

#ifndef __GST_NV_HEVC_ENC_H_INCLUDED__
#define __GST_NV_HEVC_ENC_H_INCLUDED__

#include "gstnvbaseenc.h"

typedef struct {
  GstNvBaseEnc base_nvenc;

  NV_ENC_SEI_PAYLOAD *sei_payload;
  guint num_sei_payload;

  /* properties */
  gboolean aud;
} GstNvH265Enc;

typedef struct {
  GstNvBaseEncClass video_encoder_class;
} GstNvH265EncClass;

void gst_nv_h265_enc_register (GstPlugin * plugin,
                               guint device_id,
                               guint rank,
                               GstCaps * sink_caps,
                               GstCaps * src_caps,
                               GstNvEncDeviceCaps * device_caps);

#endif /* __GST_NV_HEVC_ENC_H_INCLUDED__ */
