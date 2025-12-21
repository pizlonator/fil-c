/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 * Copyright (C) 2013      Sebastian Dröge <slomo@circular-chaos.org>
 *
 * gstaudiomixer.h: Header for GstAudioMixer element
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

#ifndef __GST_AUDIO_MIXER_H__
#define __GST_AUDIO_MIXER_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudioaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_MIXER (gst_audiomixer_get_type())
G_DECLARE_FINAL_TYPE (GstAudioMixer, gst_audiomixer, GST, AUDIO_MIXER,
    GstAudioAggregator)

/**
 * GstAudioMixer:
 *
 * The audiomixer object structure.
 */
struct _GstAudioMixer {
  GstAudioAggregator element;
};

#define GST_TYPE_AUDIO_MIXER_PAD (gst_audiomixer_pad_get_type())
G_DECLARE_FINAL_TYPE (GstAudioMixerPad, gst_audiomixer_pad,
    GST, AUDIO_MIXER_PAD, GstAudioAggregatorConvertPad)

struct _GstAudioMixerPad {
  GstAudioAggregatorConvertPad parent;

  gdouble volume;
  gint volume_i32;
  gint volume_i16;
  gint volume_i8;
  gboolean mute;
};

G_END_DECLS

#endif /* __GST_AUDIO_MIXER_H__ */
