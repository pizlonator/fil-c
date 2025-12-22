/* GStreamer
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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

#ifndef _GST_AUDIO_CHANNEL_MIX_H_
#define _GST_AUDIO_CHANNEL_MIX_H_

#include <gst/audio/gstaudiofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_CHANNEL_MIX   (gst_audio_channel_mix_get_type())
#define GST_AUDIO_CHANNEL_MIX(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_CHANNEL_MIX,GstAudioChannelMix))
#define GST_AUDIO_CHANNEL_MIX_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_CHANNEL_MIX,GstAudioChannelMixClass))
#define GST_IS_AUDIO_CHANNEL_MIX(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_CHANNEL_MIX))
#define GST_IS_AUDIO_CHANNEL_MIX_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_CHANNEL_MIX))

typedef struct _GstAudioChannelMix GstAudioChannelMix;
typedef struct _GstAudioChannelMixClass GstAudioChannelMixClass;

struct _GstAudioChannelMix
{
  GstAudioFilter base_audiochannelmix;

  double left_to_left;
  double left_to_right;
  double right_to_left;
  double right_to_right;
};

struct _GstAudioChannelMixClass
{
  GstAudioFilterClass base_audiochannelmix_class;
};

GType gst_audio_channel_mix_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (audiochannelmix);

G_END_DECLS

#endif
