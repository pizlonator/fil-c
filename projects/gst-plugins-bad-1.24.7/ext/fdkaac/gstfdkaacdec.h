/*
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_FDKAACDEC_H__
#define __GST_FDKAACDEC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

#include <fdk-aac/aacdecoder_lib.h>

G_BEGIN_DECLS

#define GST_TYPE_FDKAACDEC \
  (gst_fdkaacdec_get_type())
#define GST_FDKAACDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FDKAACDEC, GstFdkAacDec))
#define GST_FDKAACDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FDKAACDEC, GstFdkAacDecClass))
#define GST_IS_FDKAACDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FDKAACDEC))
#define GST_IS_FDKAACDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FDKAACDEC))

typedef struct _GstFdkAacDec GstFdkAacDec;
typedef struct _GstFdkAacDecClass GstFdkAacDecClass;

struct _GstFdkAacDec {
  GstAudioDecoder element;

  HANDLE_AACDECODER dec;
  gint16 *decode_buffer;
  gint decode_buffer_size;

  INT sample_rate;
  CHANNEL_MODE config;
  INT channels;
  AUDIO_CHANNEL_TYPE channel_types[64];
  UCHAR channel_indices[64];
  GstAudioChannelPosition positions[64];

  gboolean need_reorder;
  GstAudioInfo info;
};

struct _GstFdkAacDecClass {
  GstAudioDecoderClass parent_class;
};

GType gst_fdkaacdec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (fdkaacdec);

G_END_DECLS

#endif /* __GST_FDKAACDEC_H__ */
