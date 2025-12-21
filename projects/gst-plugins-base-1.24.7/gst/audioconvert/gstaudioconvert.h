/* GStreamer
 * Copyright (C) 2005 Wim Taymans <wim at fluendo dot com>
 *
 * gstaudioconvert.h: Convert audio to different audio formats automatically
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

#ifndef __GST_AUDIO_CONVERT_H__
#define __GST_AUDIO_CONVERT_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>

#define GST_TYPE_AUDIO_CONVERT (gst_audio_convert_get_type())
G_DECLARE_FINAL_TYPE (GstAudioConvert, gst_audio_convert,
    GST, AUDIO_CONVERT, GstBaseTransform);

/**
 * GstAudioConvert:
 *
 * The audioconvert object structure.
 */
struct _GstAudioConvert
{
  GstBaseTransform element;

  /* properties */
  GstAudioDitherMethod dither;
  guint dither_threshold;
  GstAudioNoiseShapingMethod ns;
  GValue mix_matrix;
  gboolean mix_matrix_is_set;

  GstAudioInfo in_info;
  GstAudioInfo out_info;
  GstAudioConverter *convert;
};

GST_ELEMENT_REGISTER_DECLARE (audioconvert);

#endif /* __GST_AUDIO_CONVERT_H__ */
