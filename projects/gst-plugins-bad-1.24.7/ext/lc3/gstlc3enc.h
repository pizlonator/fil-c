/* GStreamer LC3 Bluetooth LE audio encoder
 * Copyright (C) 2023 Asymptotic Inc. <taruntej@asymptotic.io>
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

#ifndef _GST_LC3ENC_H_
#define _GST_LC3ENC_H_

#include <gst/audio/gstaudioencoder.h>

#include <lc3.h>

G_BEGIN_DECLS
#define GST_TYPE_LC3_ENC   (gst_lc3_enc_get_type())
G_DECLARE_FINAL_TYPE (GstLc3Enc, gst_lc3_enc, GST, LC3_ENC, GstAudioEncoder)

struct _GstLc3Enc
{
  GstAudioEncoder base;
  lc3_encoder_t *enc_ch;
  enum lc3_pcm_format format;
  int rate;
  int channels;
  int frame_duration_us;
  /* byte count per channel, same for all channels */
  int frame_bytes;
  /* bytes per sample */
  int bpf;
  /* pcm samples per encoded frame */
  int frame_samples;
  gboolean first_frame;
  int pending_bytes;
};

struct _GstLc3EncClass
{
  GstAudioEncoderClass base_class;
};

GST_ELEMENT_REGISTER_DECLARE (lc3enc);

G_END_DECLS
#endif
