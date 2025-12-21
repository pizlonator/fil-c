/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#ifndef __GST_AUDIO_RATE_H__
#define __GST_AUDIO_RATE_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_RATE (gst_audio_rate_get_type())
G_DECLARE_FINAL_TYPE (GstAudioRate, gst_audio_rate, GST, AUDIO_RATE, GstElement)

/**
 * GstAudioRate:
 *
 * Opaque data structure.
 */
struct _GstAudioRate
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  /* audio format */
  GstAudioInfo info;

  /* stats */
  guint64 in, out, add, drop;
  gboolean silent;
  guint64 tolerance;
  gboolean skip_to_first;

  /* audio state */
  guint64 next_offset;
  guint64 next_ts;

  gboolean discont;

  gboolean new_segment;
  /* we accept all formats on the sink */
  GstSegment sink_segment;
  /* we output TIME format on the src */
  GstSegment src_segment;
};
GST_ELEMENT_REGISTER_DECLARE (audiorate);

G_END_DECLS

#endif /* __GST_AUDIO_RATE_H__ */
