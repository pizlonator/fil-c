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
#include <gst/audio/audio.h>
#include "gstmfutils.h"
#include "gstmftransform.h"

G_BEGIN_DECLS

#define GST_TYPE_MF_AUDIO_DECODER           (gst_mf_audio_decoder_get_type())
#define GST_MF_AUDIO_DECODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MF_AUDIO_DECODER,GstMFAudioDecoder))
#define GST_MF_AUDIO_DECODER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_MF_AUDIO_DECODER,GstMFAudioDecoderClass))
#define GST_MF_AUDIO_DECODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_MF_AUDIO_DECODER,GstMFAudioDecoderClass))
#define GST_IS_MF_AUDIO_DECODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MF_AUDIO_DECODER))
#define GST_IS_MF_AUDIO_DECODER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MF_AUDIO_DECODER))

typedef struct _GstMFAudioDecoder GstMFAudioDecoder;
typedef struct _GstMFAudioDecoderClass GstMFAudioDecoderClass;

typedef struct
{
  gchar * device_name;
  guint32 enum_flags;
  guint device_index;
} GstMFAudioDecoderClassData;

struct _GstMFAudioDecoder
{
  GstAudioDecoder parent;

  GstMFTransform *transform;
};

struct _GstMFAudioDecoderClass
{
  GstAudioDecoderClass parent_class;

  GUID codec_id;
  guint32 enum_flags;
  guint device_index;

  gboolean (*set_format)      (GstMFAudioDecoder * decoder,
                               GstMFTransform * transform,
                               GstCaps * caps);
};

GType gst_mf_audio_decoder_get_type (void);

G_END_DECLS

