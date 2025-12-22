/* GStreamer LC3 Bluetooth LE audio decoder
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

#ifndef _GST_LC3DEC_H_
#define _GST_LC3DEC_H_

#include <gst/audio/gstaudioencoder.h>
#include <lc3.h>

G_BEGIN_DECLS
#define GST_TYPE_LC3_DEC   (gst_lc3_dec_get_type())
G_DECLARE_FINAL_TYPE (GstLc3Dec, gst_lc3_dec, GST, LC3_DEC, GstAudioDecoder)

struct _GstLc3Dec
{
  GstAudioDecoder base;
  lc3_decoder_t *dec_ch;
  gint channels;
  gint rate;
  gint frame_duration_us;
  /* byte count per channel, same for all channels */
  gint frame_bytes;
  gint frame_samples;
  enum lc3_pcm_format format;
  /* bytes per sample */
  gint bpf;
};

struct _GstLc3DecClass
{
  GstAudioDecoderClass base_class;
};

GST_ELEMENT_REGISTER_DECLARE (lc3dec);

G_END_DECLS
#endif
