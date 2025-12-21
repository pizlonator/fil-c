/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstadder.h: Header for GstAdder element
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

#ifndef __GST_ADDER_H__
#define __GST_ADDER_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_ADDER (gst_adder_get_type())
G_DECLARE_FINAL_TYPE (GstAdder, gst_adder, GST, ADDER, GstElement)

/**
 * GstAdder:
 *
 * The adder object structure.
 */
struct _GstAdder {
  GstElement      element;

  GstPad         *srcpad;
  GstCollectPads *collect;
  /* pad counter, used for creating unique request pads */
  gint            padcount;

  /* the next are valid for both int and float */
  GstAudioInfo    info;

  /* counters to keep track of timestamps */
  gint64          offset;

  /* sink event handling */
  GstSegment      segment;
  gboolean new_segment_pending;
  gboolean flush_stop_pending;

  /* current caps */
  GstCaps *current_caps;

  /* target caps (set via property) */
  GstCaps *filter_caps;

  /* Pending inline events */
  GList *pending_events;
  
  gboolean send_stream_start;
  gboolean send_caps;
};

GST_ELEMENT_REGISTER_DECLARE (adder);

#define GST_TYPE_ADDER_PAD (gst_adder_pad_get_type())
G_DECLARE_FINAL_TYPE (GstAdderPad, gst_adder_pad, GST, ADDER_PAD, GstPad)

struct _GstAdderPad {
  GstPad parent;

  gdouble volume;
  gint volume_i32;
  gint volume_i16;
  gint volume_i8;
  gboolean mute;
};

G_END_DECLS

#endif /* __GST_ADDER_H__ */
