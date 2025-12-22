/*
 * gstmidiparse - midiparse plugin for gstreamer
 *
 * Copyright 2007 Wouter Paesen <wouter@blue-gate.be>
 * Copyright 2013 Wim Taymans <wim.taymans@gmail.be>
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

#ifndef __GST_MIDIPARSE_H__
#define __GST_MIDIPARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <midiparse.h>

G_BEGIN_DECLS

#define GST_TYPE_MIDI_PARSE \
  (gst_midi_parse_get_type())
#define GST_MIDI_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MIDI_PARSE,GstMidiParse))
#define GST_MIDI_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MIDI_PARSE,GstMidiParseClass))
#define GST_IS_MIDI_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MIDI_PARSE))
#define GST_IS_MIDI_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MIDI_PARSE))

typedef struct _GstMidiParse GstMidiParse;
typedef struct _GstMidiParseClass GstMidiParseClass;

typedef enum {
  GST_MIDI_PARSE_STATE_LOAD,
  GST_MIDI_PARSE_STATE_PARSE,
  GST_MIDI_PARSE_STATE_PLAY
} GstMidiParseState;

struct _GstMidiParse
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean have_group_id;
  guint group_id;

  /* input stream properties */
  GstMidiParseState state;

  guint tempo;
  guint16 ntracks;
  guint16 division;

  GList *tracks;
  guint  track_count;

  guint64 offset;
  GstAdapter *adapter;
  guint8 *data;

  /* output data */
  gboolean discont;
  GstSegment segment;
  gboolean segment_pending;
  guint32 seqnum;

  guint64 pulse;
};

struct _GstMidiParseClass
{
    GstElementClass parent_class;
};

GType gst_midi_parse_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (midiparse);

G_END_DECLS

#endif /* __GST_MIDI_PARSE_H__ */
