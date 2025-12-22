/*
 * midiparse - midi parser plugin for gstreamer
 *
 * Copyright 2013 Wim Taymans <wim.taymans@gmail.com>
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

/**
 * SECTION:element-midiparse
 * @title: midiparse
 * @see_also: fluiddec
 *
 * This element parses midi-files into midi events. You would need a midi
 * renderer such as fluidsynth to convert the events into raw samples.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 filesrc location=song.mid ! midiparse ! fluiddec ! pulsesink
 * ]| This example pipeline will parse the midi and render to raw audio which is
 * played via pulseaudio.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include <glib.h>

#include "midiparse.h"

GST_DEBUG_CATEGORY_STATIC (gst_midi_parse_debug);
#define GST_CAT_DEFAULT gst_midi_parse_debug

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  /* FILL ME */
};

#define DEFAULT_TEMPO   500000  /* 120 BPM is the default */

typedef struct
{
  guint8 *data;
  guint size;
  guint offset;

  guint8 running_status;
  guint64 pulse;
  gboolean eot;

} GstMidiTrack;

typedef GstFlowReturn (*GstMidiPushFunc) (GstMidiParse * parse,
    GstMidiTrack * track, guint8 event, guint8 * data, guint length,
    gpointer user_data);

static void gst_midi_parse_finalize (GObject * object);

static gboolean gst_midi_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_midi_parse_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStateChangeReturn gst_midi_parse_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_midi_parse_activate (GstPad * pad, GstObject * parent);
static gboolean gst_midi_parse_activatemode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active);

static void gst_midi_parse_loop (GstPad * sinkpad);
static GstFlowReturn gst_midi_parse_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * buffer);

static gboolean gst_midi_parse_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static void gst_midi_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_midi_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void reset_track (GstMidiTrack * track, GstMidiParse * midiparse);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/midi; audio/riff-midi")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-midi-event"));

#define parent_class gst_midi_parse_parent_class
G_DEFINE_TYPE (GstMidiParse, gst_midi_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (midiparse, "midiparse", GST_RANK_PRIMARY,
    GST_TYPE_MIDI_PARSE);

/* initialize the plugin's class */
static void
gst_midi_parse_class_init (GstMidiParseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_midi_parse_finalize;
  gobject_class->set_property = gst_midi_parse_set_property;
  gobject_class->get_property = gst_midi_parse_get_property;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);
  gst_element_class_set_static_metadata (gstelement_class, "MidiParse",
      "Codec/Demuxer/Audio",
      "Midi Parser Element", "Wim Taymans <wim.taymans@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (gst_midi_parse_debug, "midiparse",
      0, "MIDI parser plugin");

  gstelement_class->change_state = gst_midi_parse_change_state;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_midi_parse_init (GstMidiParse * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");

  gst_pad_set_activatemode_function (filter->sinkpad,
      gst_midi_parse_activatemode);
  gst_pad_set_activate_function (filter->sinkpad, gst_midi_parse_activate);
  gst_pad_set_event_function (filter->sinkpad, gst_midi_parse_sink_event);
  gst_pad_set_chain_function (filter->sinkpad, gst_midi_parse_chain);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");

  gst_pad_set_query_function (filter->srcpad, gst_midi_parse_src_query);
  gst_pad_set_event_function (filter->srcpad, gst_midi_parse_src_event);
  gst_pad_use_fixed_caps (filter->srcpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  gst_segment_init (&filter->segment, GST_FORMAT_TIME);

  filter->adapter = gst_adapter_new ();

  filter->have_group_id = FALSE;
  filter->group_id = G_MAXUINT;
}

static void
gst_midi_parse_finalize (GObject * object)
{
  GstMidiParse *midiparse;

  midiparse = GST_MIDI_PARSE (object);

  g_object_unref (midiparse->adapter);
  g_free (midiparse->data);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_midi_parse_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
  GstMidiParse *midiparse = GST_MIDI_PARSE (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
      gst_query_set_duration (query, GST_FORMAT_TIME,
          midiparse->segment.duration);
      break;
    case GST_QUERY_POSITION:
      gst_query_set_position (query, GST_FORMAT_TIME,
          midiparse->segment.position);
      break;
    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 1, GST_FORMAT_TIME);
      break;
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = midiparse->segment.format;

      start =
          gst_segment_to_stream_time (&midiparse->segment, format,
          midiparse->segment.start);
      if ((stop = midiparse->segment.stop) == -1)
        stop = midiparse->segment.duration;
      else
        stop = gst_segment_to_stream_time (&midiparse->segment, format, stop);

      gst_query_set_segment (query, midiparse->segment.rate, format, start,
          stop);
      res = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:
      gst_query_set_seeking (query, midiparse->segment.format,
          FALSE, 0, midiparse->segment.duration);
      break;
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}

static gboolean
gst_midi_parse_do_seek (GstMidiParse * midiparse, GstSegment * segment)
{
  /* if seeking backwards, start from 0 else we just let things run and
   * have it clip downstream */
  GST_DEBUG_OBJECT (midiparse, "seeking back to 0");
  segment->position = 0;
  g_list_foreach (midiparse->tracks, (GFunc) reset_track, midiparse);
  midiparse->pulse = 0;

  return TRUE;
}

static gboolean
gst_midi_parse_perform_seek (GstMidiParse * midiparse, GstEvent * event)
{
  gboolean res = TRUE, tres;
  gdouble rate;
  GstFormat seek_format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment;
  guint32 seqnum;
  GstEvent *tevent;

  GST_DEBUG_OBJECT (midiparse, "doing seek: %" GST_PTR_FORMAT, event);

  if (event) {
    gst_event_parse_seek (event, &rate, &seek_format, &flags,
        &start_type, &start, &stop_type, &stop);

    if (seek_format != GST_FORMAT_TIME)
      goto invalid_format;

    flush = flags & GST_SEEK_FLAG_FLUSH;
    seqnum = gst_event_get_seqnum (event);
  } else {
    flush = FALSE;
    /* get next seqnum */
    seqnum = gst_util_seqnum_next ();
  }

  /* send flush start */
  if (flush) {
    tevent = gst_event_new_flush_start ();
    gst_event_set_seqnum (tevent, seqnum);
    gst_pad_push_event (midiparse->srcpad, tevent);
  } else
    gst_pad_pause_task (midiparse->srcpad);

  /* grab streaming lock, this should eventually be possible, either
   * because the task is paused, our streaming thread stopped
   * or because our peer is flushing. */
  GST_PAD_STREAM_LOCK (midiparse->sinkpad);
  if (G_UNLIKELY (midiparse->seqnum == seqnum)) {
    /* we have seen this event before, issue a warning for now */
    GST_WARNING_OBJECT (midiparse, "duplicate event found %" G_GUINT32_FORMAT,
        seqnum);
  } else {
    midiparse->seqnum = seqnum;
    GST_DEBUG_OBJECT (midiparse, "seek with seqnum %" G_GUINT32_FORMAT, seqnum);
  }

  /* Copy the current segment info into the temp segment that we can actually
   * attempt the seek with. We only update the real segment if the seek succeeds. */
  memcpy (&seeksegment, &midiparse->segment, sizeof (GstSegment));

  /* now configure the final seek segment */
  if (event) {
    gst_segment_do_seek (&seeksegment, rate, seek_format, flags,
        start_type, start, stop_type, stop, &update);
  }

  /* Else, no seek event passed, so we're just (re)starting the
     current segment. */
  GST_DEBUG_OBJECT (midiparse, "segment configured from %" G_GINT64_FORMAT
      " to %" G_GINT64_FORMAT ", position %" G_GINT64_FORMAT,
      seeksegment.start, seeksegment.stop, seeksegment.position);

  /* do the seek, segment.position contains the new position. */
  res = gst_midi_parse_do_seek (midiparse, &seeksegment);

  /* and prepare to continue streaming */
  if (flush) {
    tevent = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (tevent, seqnum);
    /* send flush stop, peer will accept data and events again. We
     * are not yet providing data as we still have the STREAM_LOCK. */
    gst_pad_push_event (midiparse->srcpad, tevent);
  }

  /* if the seek was successful, we update our real segment and push
   * out the new segment. */
  if (res) {
    GST_OBJECT_LOCK (midiparse);
    memcpy (&midiparse->segment, &seeksegment, sizeof (GstSegment));
    GST_OBJECT_UNLOCK (midiparse);

    if (seeksegment.flags & GST_SEGMENT_FLAG_SEGMENT) {
      GstMessage *message;

      message = gst_message_new_segment_start (GST_OBJECT (midiparse),
          seeksegment.format, seeksegment.position);
      gst_message_set_seqnum (message, seqnum);

      gst_element_post_message (GST_ELEMENT (midiparse), message);
    }
    /* for deriving a stop position for the playback segment from the seek
     * segment, we must take the duration when the stop is not set */
    if ((stop = seeksegment.stop) == -1)
      stop = seeksegment.duration;

    midiparse->segment_pending = TRUE;
    midiparse->discont = TRUE;
  }

  /* and restart the task in case it got paused explicitly or by
   * the FLUSH_START event we pushed out. */
  tres =
      gst_pad_start_task (midiparse->sinkpad,
      (GstTaskFunction) gst_midi_parse_loop, midiparse->sinkpad, NULL);
  if (res && !tres)
    res = FALSE;

  /* and release the lock again so we can continue streaming */
  GST_PAD_STREAM_UNLOCK (midiparse->sinkpad);

  return res;

  /* ERROR */
invalid_format:
  {
    GST_DEBUG_OBJECT (midiparse, "Unsupported seek format %s",
        gst_format_get_name (seek_format));
    return FALSE;
  }
}

static gboolean
gst_midi_parse_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = FALSE;
  GstMidiParse *midiparse = GST_MIDI_PARSE (parent);

  GST_DEBUG_OBJECT (pad, "%s event received", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      res = gst_midi_parse_perform_seek (midiparse, event);
      break;
    default:
      break;
  }
  gst_event_unref (event);

  return res;
}

static gboolean
gst_midi_parse_activate (GstPad * sinkpad, GstObject * parent)
{
  GstQuery *query;
  gboolean pull_mode;

  query = gst_query_new_scheduling ();

  if (!gst_pad_peer_query (sinkpad, query)) {
    gst_query_unref (query);
    goto activate_push;
  }

  pull_mode = gst_query_has_scheduling_mode_with_flags (query,
      GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE);
  gst_query_unref (query);

  if (!pull_mode)
    goto activate_push;

  GST_DEBUG_OBJECT (sinkpad, "activating pull");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  {
    GST_DEBUG_OBJECT (sinkpad, "activating push");
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}

static gboolean
gst_midi_parse_activatemode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      res = TRUE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        res = gst_pad_start_task (pad, (GstTaskFunction) gst_midi_parse_loop,
            pad, NULL);
      } else {
        res = gst_pad_stop_task (pad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static gboolean
parse_MThd (GstMidiParse * midiparse, guint8 * data, guint size)
{
  guint16 format, ntracks, division;
  gboolean multitrack;

  format = GST_READ_UINT16_BE (data);
  switch (format) {
    case 0:
      multitrack = FALSE;
      break;
    case 1:
      multitrack = TRUE;
      break;
    default:
    case 2:
      goto invalid_format;
  }
  ntracks = GST_READ_UINT16_BE (data + 2);
  if (ntracks > 1 && !multitrack)
    goto invalid_tracks;

  division = GST_READ_UINT16_BE (data + 4);
  if (division & 0x8000)
    goto invalid_division;

  GST_DEBUG_OBJECT (midiparse, "format %u, tracks %u, division %u",
      format, ntracks, division);

  midiparse->ntracks = ntracks;
  midiparse->division = division;

  return TRUE;

invalid_format:
  {
    GST_ERROR_OBJECT (midiparse, "unsupported midi format %u", format);
    return FALSE;
  }
invalid_tracks:
  {
    GST_ERROR_OBJECT (midiparse, "invalid number of tracks %u for format %u",
        ntracks, format);
    return FALSE;
  }
invalid_division:
  {
    GST_ERROR_OBJECT (midiparse, "unsupported division");
    return FALSE;
  }
}

static guint
parse_varlen (GstMidiParse * midiparse, guint8 * data, guint size,
    gint32 * result)
{
  gint32 res;
  gint i;

  res = 0;
  for (i = 0; i < 4; i++) {
    if (size == 0)
      return 0;

    res = (res << 7) | ((data[i]) & 0x7f);
    if ((data[i] & 0x80) == 0) {
      *result = res;
      return i + 1;
    }
  }
  return 0;
}

static GstFlowReturn
handle_meta_event (GstMidiParse * midiparse, GstMidiTrack * track, guint8 event)
{
  guint8 type;
  guint8 *data;
  gchar *bytes;
  guint size, consumed;
  gint32 length;

  track->offset += 1;

  data = track->data + track->offset;
  size = track->size - track->offset;

  if (size < 1)
    goto short_file;

  type = data[0];

  consumed = parse_varlen (midiparse, data + 1, size - 1, &length);
  if (consumed == 0)
    goto short_file;

  data += consumed + 1;
  size -= consumed + 1;

  if (size < length)
    goto short_file;

  GST_DEBUG_OBJECT (midiparse, "handle meta event type 0x%02x, length %u",
      type, length);

  bytes = g_strndup ((const gchar *) data, length);

  switch (type) {
    case 0x01:
      GST_DEBUG_OBJECT (midiparse, "Text: %s", bytes);
      break;
    case 0x02:
      GST_DEBUG_OBJECT (midiparse, "Copyright: %s", bytes);
      break;
    case 0x03:
      GST_DEBUG_OBJECT (midiparse, "Track Name: %s", bytes);
      break;
    case 0x04:
      GST_DEBUG_OBJECT (midiparse, "Instrument: %s", bytes);
      break;
    case 0x05:
      GST_DEBUG_OBJECT (midiparse, "Lyric: %s", bytes);
      break;
    case 0x06:
      GST_DEBUG_OBJECT (midiparse, "Marker: %s", bytes);
      break;
    case 0x07:
      GST_DEBUG_OBJECT (midiparse, "Cue point: %s", bytes);
      break;
    case 0x08:
      GST_DEBUG_OBJECT (midiparse, "Patch name: %s", bytes);
      break;
    case 0x09:
      GST_DEBUG_OBJECT (midiparse, "MIDI port: %s", bytes);
      break;
    case 0x2f:
      GST_DEBUG_OBJECT (midiparse, "End of track");
      break;
    case 0x51:
    {
      guint32 uspqn = (data[0] << 16) | (data[1] << 8) | data[2];
      midiparse->tempo = (uspqn ? uspqn : DEFAULT_TEMPO);
      GST_DEBUG_OBJECT (midiparse, "tempo %u", midiparse->tempo);
      break;
    }
    case 0x54:
      GST_DEBUG_OBJECT (midiparse, "SMPTE offset");
      break;
    case 0x58:
      GST_DEBUG_OBJECT (midiparse, "Time signature");
      break;
    case 0x59:
      GST_DEBUG_OBJECT (midiparse, "Key signature");
      break;
    case 0x7f:
      GST_DEBUG_OBJECT (midiparse, "Proprietary event");
      break;
    default:
      GST_DEBUG_OBJECT (midiparse, "unknown event 0x%02x length %d", type,
          length);
      break;
  }
  g_free (bytes);

  track->offset += consumed + length + 1;

  return GST_FLOW_OK;

  /* ERRORS */
short_file:
  {
    GST_DEBUG_OBJECT (midiparse, "not enough data");
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
handle_sysex_event (GstMidiParse * midiparse, GstMidiTrack * track,
    guint8 event, GstMidiPushFunc pushfunc, gpointer user_data)
{
  GstFlowReturn ret;
  guint8 *data;
  guint size, consumed;
  gint32 length;

  track->offset += 1;

  data = track->data + track->offset;
  size = track->size - track->offset;

  consumed = parse_varlen (midiparse, data, size, &length);
  if (consumed == 0)
    goto short_file;

  data += consumed;
  size -= consumed;

  if (size < length)
    goto short_file;

  GST_DEBUG_OBJECT (midiparse, "handle sysex event 0x%02x, length %u",
      event, length);

  if (pushfunc)
    ret = pushfunc (midiparse, track, event, data, length, user_data);
  else
    ret = GST_FLOW_OK;

  track->offset += consumed + length;

  return ret;

  /* ERRORS */
short_file:
  {
    GST_DEBUG_OBJECT (midiparse, "not enough data");
    return GST_FLOW_ERROR;
  }
}


static guint8
event_from_status (GstMidiParse * midiparse, GstMidiTrack * track,
    guint8 status)
{
  if ((status & 0x80) == 0) {
    if ((track->running_status & 0x80) == 0)
      return 0;

    return track->running_status;
  } else {
    return status;
  }
}

static gboolean
update_track_position (GstMidiParse * midiparse, GstMidiTrack * track)
{
  gint32 delta_time;
  guint8 *data;
  guint size, consumed;

  if (track->offset >= track->size)
    goto eot;

  data = track->data + track->offset;
  size = track->size - track->offset;

  consumed = parse_varlen (midiparse, data, size, &delta_time);
  if (consumed == 0)
    goto eot;

  track->pulse += delta_time;
  track->offset += consumed;

  GST_LOG_OBJECT (midiparse, "updated track to pulse %" G_GUINT64_FORMAT,
      track->pulse);

  return TRUE;

  /* ERRORS */
eot:
  {
    GST_DEBUG_OBJECT (midiparse, "track ended");
    track->eot = TRUE;
    return FALSE;
  }
}

static GstFlowReturn
handle_next_event (GstMidiParse * midiparse, GstMidiTrack * track,
    GstMidiPushFunc pushfunc, gpointer user_data)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint8 status, event;
  guint length;
  guint8 *data;

  data = &track->data[track->offset];

  status = data[0];
  event = event_from_status (midiparse, track, status);

  GST_LOG_OBJECT (midiparse, "track %p, status 0x%02x, event 0x%02x", track,
      status, event);

  switch (event & 0xf0) {
    case 0xf0:
      switch (event) {
        case 0xff:
          ret = handle_meta_event (midiparse, track, event);
          break;
        case 0xf0:
        case 0xf7:
          ret =
              handle_sysex_event (midiparse, track, event, pushfunc, user_data);
          break;
        default:
          goto unhandled_event;
      }
      length = 0;
      break;
    case 0xc0:
    case 0xd0:
      length = 1;
      break;
    case 0x80:
    case 0x90:
    case 0xa0:
    case 0xb0:
    case 0xe0:
      length = 2;
      break;
    default:
      goto undefined_status;
  }
  if (length > 0) {
    if (status & 0x80) {
      if (pushfunc)
        ret = pushfunc (midiparse, track, event, data + 1, length, user_data);
      track->offset += length + 1;
    } else {
      if (pushfunc)
        ret = pushfunc (midiparse, track, event, data, length + 1, user_data);
      track->offset += length;
    }
  }

  if (ret == GST_FLOW_OK) {
    if (event < 0xF8)
      track->running_status = event;

    update_track_position (midiparse, track);
  }
  return ret;

  /* ERRORS */
undefined_status:
  {
    GST_ERROR_OBJECT (midiparse, "Undefined status and invalid running status");
    return GST_FLOW_ERROR;
  }
unhandled_event:
  {
    /* we don't know the size so we can't continue parsing */
    GST_ERROR_OBJECT (midiparse, "unhandled event 0x%08x", event);
    return GST_FLOW_ERROR;
  }
}

static void
reset_track (GstMidiTrack * track, GstMidiParse * midiparse)
{
  GST_DEBUG_OBJECT (midiparse, "reset track");
  track->offset = 0;
  track->pulse = 0;
  track->eot = FALSE;
  track->running_status = 0xff;
  update_track_position (midiparse, track);
}

static gboolean
parse_MTrk (GstMidiParse * midiparse, guint8 * data, guint size)
{
  GstMidiTrack *track;
  GstClockTime duration;

  /* ignore excess tracks */
  if (midiparse->track_count >= midiparse->ntracks)
    return TRUE;

  track = g_new (GstMidiTrack, 1);
  track->data = data;
  track->size = size;
  reset_track (track, midiparse);

  midiparse->tracks = g_list_append (midiparse->tracks, track);
  midiparse->track_count++;

  /* now loop over all events and calculate the duration */
  while (!track->eot) {
    handle_next_event (midiparse, track, NULL, NULL);
  }

  duration = gst_util_uint64_scale (track->pulse,
      1000 * midiparse->tempo, midiparse->division);

  GST_DEBUG_OBJECT (midiparse, "duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (duration));

  if (duration > midiparse->segment.duration)
    midiparse->segment.duration = duration;

  reset_track (track, midiparse);

  return TRUE;
}

static gboolean
find_midi_chunk (GstMidiParse * midiparse, guint8 * data, guint size,
    guint * offset, guint * length)
{
  guint32 type;

  *length = 0;

  if (size < 8)
    goto short_chunk;

  type = GST_STR_FOURCC (data);

  if (type == GST_MAKE_FOURCC ('R', 'I', 'F', 'F')) {
    guint32 riff_len;

    GST_DEBUG_OBJECT (midiparse, "found RIFF");

    if (size < 12)
      goto short_chunk;

    if (GST_STR_FOURCC (data + 8) != GST_MAKE_FOURCC ('R', 'M', 'I', 'D'))
      goto invalid_format;

    riff_len = GST_READ_UINT32_LE (data + 4);

    if (size < riff_len)
      goto short_chunk;

    data += 12;
    size -= 12;
    *offset = 12;

    GST_DEBUG_OBJECT (midiparse, "found RIFF RMID of size %u", riff_len);

    while (TRUE) {
      guint32 chunk_type;
      guint32 chunk_len;

      if (riff_len < 8)
        goto short_chunk;

      chunk_type = GST_STR_FOURCC (data);
      chunk_len = GST_READ_UINT32_LE (data + 4);

      riff_len -= 8;
      if (riff_len < chunk_len)
        goto short_chunk;

      data += 8;
      size -= 8;
      *offset += 8;
      riff_len -= chunk_len;

      if (chunk_type == GST_MAKE_FOURCC ('d', 'a', 't', 'a')) {
        *length = chunk_len;
        break;
      }

      data += chunk_len;
      size -= chunk_len;
    }
  } else {
    *offset = 0;
    *length = size;
  }
  return TRUE;

  /* ERRORS */
short_chunk:
  {
    GST_LOG_OBJECT (midiparse, "not enough data %u < %u", *length + 8, size);
    return FALSE;
  }
invalid_format:
  {
    GST_ERROR_OBJECT (midiparse, "invalid format");
    return FALSE;
  }
}

static guint
gst_midi_parse_chunk (GstMidiParse * midiparse, guint8 * data, guint size)
{
  guint32 type, length = 0;

  if (size < 8)
    goto short_chunk;

  length = GST_READ_UINT32_BE (data + 4);

  GST_DEBUG_OBJECT (midiparse, "have type %c%c%c%c, length %u",
      data[0], data[1], data[2], data[3], length);

  if (size < length + 8)
    goto short_chunk;

  type = GST_STR_FOURCC (data);

  switch (type) {
    case GST_MAKE_FOURCC ('M', 'T', 'h', 'd'):
      if (!parse_MThd (midiparse, data + 8, length))
        goto invalid_format;
      break;
    case GST_MAKE_FOURCC ('M', 'T', 'r', 'k'):
      if (!parse_MTrk (midiparse, data + 8, length))
        goto invalid_format;
      break;
    default:
      GST_LOG_OBJECT (midiparse, "ignore chunk");
      break;
  }

  return length + 8;

  /* ERRORS */
short_chunk:
  {
    GST_LOG_OBJECT (midiparse, "not enough data %u < %u", size, length + 8);
    return 0;
  }
invalid_format:
  {
    GST_ERROR_OBJECT (midiparse, "invalid format");
    return 0;
  }
}

static GstFlowReturn
gst_midi_parse_parse_song (GstMidiParse * midiparse)
{
  GstCaps *outcaps;
  guint8 *data;
  guint size, offset, length;
  GstEvent *event;
  gchar *stream_id;

  GST_DEBUG_OBJECT (midiparse, "Parsing song");

  gst_segment_init (&midiparse->segment, GST_FORMAT_TIME);
  midiparse->segment.duration = 0;
  midiparse->pulse = 0;

  size = gst_adapter_available (midiparse->adapter);
  data = gst_adapter_take (midiparse->adapter, size);

  midiparse->data = data;
  midiparse->tempo = DEFAULT_TEMPO;

  if (!find_midi_chunk (midiparse, data, size, &offset, &length))
    goto invalid_format;

  while (length) {
    guint consumed;

    consumed = gst_midi_parse_chunk (midiparse, &data[offset], length);
    if (consumed == 0)
      goto short_file;

    offset += consumed;
    length -= consumed;
  }

  GST_DEBUG_OBJECT (midiparse, "song duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (midiparse->segment.duration));

  stream_id =
      gst_pad_create_stream_id (midiparse->srcpad, GST_ELEMENT_CAST (midiparse),
      NULL);
  event =
      gst_pad_get_sticky_event (midiparse->sinkpad, GST_EVENT_STREAM_START, 0);
  if (event) {
    if (gst_event_parse_group_id (event, &midiparse->group_id))
      midiparse->have_group_id = TRUE;
    else
      midiparse->have_group_id = FALSE;
    gst_event_unref (event);
  } else if (!midiparse->have_group_id) {
    midiparse->have_group_id = TRUE;
    midiparse->group_id = gst_util_group_id_next ();
  }
  event = gst_event_new_stream_start (stream_id);
  if (midiparse->have_group_id)
    gst_event_set_group_id (event, midiparse->group_id);
  gst_pad_push_event (midiparse->srcpad, event);
  g_free (stream_id);

  outcaps = gst_pad_get_pad_template_caps (midiparse->srcpad);
  gst_pad_set_caps (midiparse->srcpad, outcaps);
  gst_caps_unref (outcaps);

  midiparse->segment_pending = TRUE;
  midiparse->discont = TRUE;

  GST_DEBUG_OBJECT (midiparse, "Parsing song done");

  return GST_FLOW_OK;

  /* ERRORS */
short_file:
  {
    GST_ERROR_OBJECT (midiparse, "not enough data");
    return GST_FLOW_ERROR;
  }
invalid_format:
  {
    GST_ERROR_OBJECT (midiparse, "invalid format");
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
play_push_func (GstMidiParse * midiparse, GstMidiTrack * track,
    guint8 event, guint8 * data, guint length, gpointer user_data)
{
  GstBuffer *outbuf;
  GstMapInfo info;
  GstClockTime position;

  outbuf = gst_buffer_new_allocate (NULL, length + 1, NULL);

  gst_buffer_map (outbuf, &info, GST_MAP_WRITE);
  info.data[0] = event;
  if (length)
    memcpy (&info.data[1], data, length);
  gst_buffer_unmap (outbuf, &info);

  position = midiparse->segment.position;
  GST_BUFFER_PTS (outbuf) = position;
  GST_BUFFER_DTS (outbuf) = position;

  GST_DEBUG_OBJECT (midiparse, "pushing %" GST_TIME_FORMAT,
      GST_TIME_ARGS (position));

  if (midiparse->discont) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    midiparse->discont = FALSE;
  }

  return gst_pad_push (midiparse->srcpad, outbuf);
}

static GstFlowReturn
gst_midi_parse_do_play (GstMidiParse * midiparse)
{
  GstFlowReturn res;
  GList *walk;
  guint64 pulse, next_pulse = G_MAXUINT64;
  GstClockTime position, next_position;
  guint64 tick;

  pulse = midiparse->pulse;
  position = midiparse->segment.position;

  if (midiparse->segment_pending) {
    gst_pad_push_event (midiparse->srcpad,
        gst_event_new_segment (&midiparse->segment));
    midiparse->segment_pending = FALSE;
  }

  GST_DEBUG_OBJECT (midiparse, "pulse %" G_GUINT64_FORMAT ", position %"
      GST_TIME_FORMAT, pulse, GST_TIME_ARGS (position));

  for (walk = midiparse->tracks; walk; walk = g_list_next (walk)) {
    GstMidiTrack *track = walk->data;

    while (!track->eot && track->pulse == pulse) {
      res = handle_next_event (midiparse, track, play_push_func, NULL);
      if (res != GST_FLOW_OK)
        goto error;
    }

    if (!track->eot && track->pulse < next_pulse)
      next_pulse = track->pulse;
  }

  if (next_pulse == G_MAXUINT64)
    goto eos;

  tick = position / (10 * GST_MSECOND);
  GST_DEBUG_OBJECT (midiparse, "current tick %" G_GUINT64_FORMAT, tick);

  next_position = gst_util_uint64_scale (next_pulse,
      1000 * midiparse->tempo, midiparse->division);
  GST_DEBUG_OBJECT (midiparse, "next position %" GST_TIME_FORMAT,
      GST_TIME_ARGS (next_position));

  /* send 10ms ticks to advance the downstream element */
  while (TRUE) {
    /* get position of next tick */
    position = ++tick * (10 * GST_MSECOND);
    GST_DEBUG_OBJECT (midiparse, "tick %" G_GUINT64_FORMAT
        ", position %" GST_TIME_FORMAT, tick, GST_TIME_ARGS (position));

    if (position >= next_position)
      break;

    midiparse->segment.position = position;
    res = play_push_func (midiparse, NULL, 0xf9, NULL, 0, NULL);
    if (res != GST_FLOW_OK)
      goto error;
  }

  midiparse->pulse = next_pulse;
  midiparse->segment.position = next_position;

  return GST_FLOW_OK;

  /* ERRORS */
eos:
  {
    GST_DEBUG_OBJECT (midiparse, "we are EOS");
    return GST_FLOW_EOS;
  }
error:
  {
    GST_DEBUG_OBJECT (midiparse, "have flow result %s",
        gst_flow_get_name (res));
    return res;
  }
}

static gboolean
gst_midi_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstMidiParse *midiparse = GST_MIDI_PARSE (parent);

  GST_DEBUG_OBJECT (pad, "%s event received", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      midiparse->state = GST_MIDI_PARSE_STATE_PARSE;
      /* now start the parsing task */
      res = gst_pad_start_task (midiparse->sinkpad,
          (GstTaskFunction) gst_midi_parse_loop, midiparse->sinkpad, NULL);
      /* don't forward the event */
      gst_event_unref (event);
      break;
    case GST_EVENT_CAPS:
    case GST_EVENT_STREAM_START:
    case GST_EVENT_SEGMENT:
      res = TRUE;
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
  return res;
}

static GstFlowReturn
gst_midi_parse_chain (GstPad * sinkpad, GstObject * parent, GstBuffer * buffer)
{
  GstMidiParse *midiparse;

  midiparse = GST_MIDI_PARSE (parent);

  /* push stuff in the adapter, we will start doing something in the sink event
   * handler when we get EOS */
  gst_adapter_push (midiparse->adapter, buffer);

  return GST_FLOW_OK;
}

static void
gst_midi_parse_loop (GstPad * sinkpad)
{
  GstMidiParse *midiparse = GST_MIDI_PARSE (GST_PAD_PARENT (sinkpad));
  GstFlowReturn ret;

  switch (midiparse->state) {
    case GST_MIDI_PARSE_STATE_LOAD:
    {
      GstBuffer *buffer = NULL;

      GST_DEBUG_OBJECT (midiparse, "loading song");

      ret =
          gst_pad_pull_range (midiparse->sinkpad, midiparse->offset, -1,
          &buffer);

      if (ret == GST_FLOW_EOS) {
        GST_DEBUG_OBJECT (midiparse, "Song loaded");
        midiparse->state = GST_MIDI_PARSE_STATE_PARSE;
      } else if (ret != GST_FLOW_OK) {
        GST_ELEMENT_ERROR (midiparse, STREAM, DECODE, (NULL),
            ("Unable to read song"));
        goto pause;
      } else {
        GST_DEBUG_OBJECT (midiparse, "pushing buffer");
        gst_adapter_push (midiparse->adapter, buffer);
        midiparse->offset += gst_buffer_get_size (buffer);
      }
      break;
    }
    case GST_MIDI_PARSE_STATE_PARSE:
      ret = gst_midi_parse_parse_song (midiparse);
      if (ret != GST_FLOW_OK)
        goto pause;
      midiparse->state = GST_MIDI_PARSE_STATE_PLAY;
      break;
    case GST_MIDI_PARSE_STATE_PLAY:
      ret = gst_midi_parse_do_play (midiparse);
      if (ret != GST_FLOW_OK)
        goto pause;
      break;
    default:
      break;
  }
  return;

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);
    GstEvent *event;

    GST_DEBUG_OBJECT (midiparse, "pausing task, reason %s", reason);
    gst_pad_pause_task (sinkpad);
    if (ret == GST_FLOW_EOS) {
      /* perform EOS logic */
      event = gst_event_new_eos ();
      gst_pad_push_event (midiparse->srcpad, event);
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS) {
      event = gst_event_new_eos ();
      /* for fatal errors we post an error message, post the error
       * first so the app knows about the error first. */
      GST_ELEMENT_FLOW_ERROR (midiparse, ret);
      gst_pad_push_event (midiparse->srcpad, event);
    }
  }
}

static void
free_track (GstMidiTrack * track, GstMidiParse * midiparse)
{
  g_free (track);
}

static void
gst_midi_parse_reset (GstMidiParse * midiparse)
{
  gst_adapter_clear (midiparse->adapter);
  g_free (midiparse->data);
  midiparse->data = NULL;
  g_list_foreach (midiparse->tracks, (GFunc) free_track, midiparse);
  g_list_free (midiparse->tracks);
  midiparse->tracks = NULL;
  midiparse->track_count = 0;
  midiparse->have_group_id = FALSE;
  midiparse->group_id = G_MAXUINT;
}

static GstStateChangeReturn
gst_midi_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstMidiParse *midiparse = GST_MIDI_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      midiparse->offset = 0;
      midiparse->state = GST_MIDI_PARSE_STATE_LOAD;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_midi_parse_reset (midiparse);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_midi_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_midi_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
