/* GStreamer openaptx audio encoder
 *
 * Copyright (C) 2020 Igor V. Kovalenko <igor.v.kovalenko@gmail.com>
 * Copyright (C) 2020 Thomas Weißschuh <thomas@t-8ch.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef __GST_OPENAPTXENC_H__
#define __GST_OPENAPTXENC_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#ifdef USE_FREEAPTX
#include <freeaptx.h>
#else
#include <openaptx.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_OPENAPTX_ENC (gst_openaptx_enc_get_type())
G_DECLARE_FINAL_TYPE (GstOpenaptxEnc, gst_openaptx_enc, GST, OPENAPTX_ENC, GstAudioEncoder)

struct _GstOpenaptxEnc {
  GstAudioEncoder audio_encoder;

  gboolean hd;

  struct aptx_context *aptx_c;
};

GST_ELEMENT_REGISTER_DECLARE(openaptxenc);

G_END_DECLS

#endif /* __GST_OPENAPTXENC_H__ */
