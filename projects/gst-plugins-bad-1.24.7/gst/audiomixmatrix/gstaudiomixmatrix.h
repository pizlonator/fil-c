/*
 * GStreamer
 * Copyright (C) 2017 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * gstaudiomixmatrix.h
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

#ifndef __GST_AUDIO_MIX_MATRIX_H__
#define __GST_AUDIO_MIX_MATRIX_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#define GST_TYPE_AUDIO_MIX_MATRIX            (gst_audio_mix_matrix_get_type())
#define GST_AUDIO_MIX_MATRIX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_MIX_MATRIX,GstAudioMixMatrix))
#define GST_AUDIO_MIX_MATRIX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AUDIO_MIX_MATRIX,GstAudioMixMatrixClass))
#define GST_AUDIO_MIX_MATRIX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_AUDIO_MIX_MATRIX,GstAudioMixMatrixClass))
#define GST_IS_AUDIO_MIX_MATRIX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_MIX_MATRIX))
#define GST_IS_AUDIO_MIX_MATRIX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AUDIO_MIX_MATRIX))
#define GST_TYPE_AUDIO_MIX_MATRIX_MODE (gst_audio_mix_matrix_mode_get_type())

typedef struct _GstAudioMixMatrix GstAudioMixMatrix;
typedef struct _GstAudioMixMatrixClass GstAudioMixMatrixClass;

typedef enum _GstAudioMixMatrixMode
{
  GST_AUDIO_MIX_MATRIX_MODE_MANUAL = 0,
  GST_AUDIO_MIX_MATRIX_MODE_FIRST_CHANNELS = 1
} GstAudioMixMatrixMode;

/**
 * GstAudioMixMatrix:
 *
 * Opaque data structure.
 */
struct _GstAudioMixMatrix
{
  GstBaseTransform audiofilter;

  /* < private > */
  guint in_channels;
  guint out_channels;
  gdouble *matrix;
  guint64 channel_mask;
  GstAudioMixMatrixMode mode;
  gint32 *s16_conv_matrix;
  gint64 *s32_conv_matrix;
  gint shift_bytes;

  GstAudioFormat format;
};

struct _GstAudioMixMatrixClass
{
  GstBaseTransformClass parent_class;
};

GType gst_audio_mix_matrix_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (audiomixmatrix);

GType gst_audio_mix_matrix_mode_get_type (void);

G_END_DECLS
#endif /* __GST_AUDIO_MIX_MATRIX_H__ */
