/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 * Copyright (C) 2013      Sebastian Dröge <slomo@circular-chaos.org>
 *
 * gstaudiointerleave.h: Header for audiointerleave element
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

#ifndef __GST_AUDIO_INTERLEAVE_H__
#define __GST_AUDIO_INTERLEAVE_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include <gst/audio/gstaudioaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_INTERLEAVE (gst_audio_interleave_get_type())
G_DECLARE_FINAL_TYPE (GstAudioInterleave, gst_audio_interleave,
    GST, AUDIO_INTERLEAVE, GstAudioAggregator)

typedef void (*GstInterleaveFunc) (gpointer out, gpointer in, guint stride,
    guint nframes);

/**
 * GstAudioInterleave:
 *
 * The GstAudioInterleave object structure.
 */
struct _GstAudioInterleave {
  GstAudioAggregator parent;

  gint padcounter;
  guint channels; /* object lock */

  GstCaps *sinkcaps;

  GValueArray *channel_positions;
  GValueArray *input_channel_positions;
  gboolean channel_positions_from_input;

  gint default_channels_ordering_map[64];

  GstInterleaveFunc func;
};


#define GST_TYPE_AUDIO_INTERLEAVE_PAD (gst_audio_interleave_pad_get_type())
G_DECLARE_FINAL_TYPE (GstAudioInterleavePad, gst_audio_interleave_pad,
    GST, AUDIO_INTERLEAVE_PAD, GstAudioAggregatorConvertPad)

struct _GstAudioInterleavePad {
  GstAudioAggregatorPad parent;

  guint channel;
};

G_END_DECLS

#endif /* __GST_AUDIO_INTERLEAVE_H__ */
