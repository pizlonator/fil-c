/* GStreamer Musepack decoder plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmusepackdec.h"
#include "gstmusepackreader.h"

GST_DEBUG_CATEGORY (musepackdec_debug);
#define GST_CAT_DEFAULT musepackdec_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-musepack, streamversion = (int) { 7, 8 }")
    );

#ifdef MPC_FIXED_POINT
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GST_MPC_FORMAT "S32LE"
# else
#  define GST_MPC_FORMAT "S32BE"
# endif
#else
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GST_MPC_FORMAT "F32LE"
# else
#  define GST_MPC_FORMAT "F32BE"
# endif
#endif

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_MPC_FORMAT ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], " "channels = (int) [ 1, 2 ]")
    );

static void gst_musepackdec_dispose (GObject * obj);

static gboolean gst_musepackdec_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_musepackdec_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_musepackdec_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_musepackdec_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);

static void gst_musepackdec_loop (GstPad * sinkpad);
static GstStateChangeReturn
gst_musepackdec_change_state (GstElement * element, GstStateChange transition);

#define parent_class gst_musepackdec_parent_class
G_DEFINE_TYPE_WITH_CODE (GstMusepackDec, gst_musepackdec, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (musepackdec_debug, "musepackdec", 0,
        "mpc decoder");
    );
GST_ELEMENT_REGISTER_DEFINE (musepackdec, "musepackdec",
    GST_RANK_MARGINAL, GST_TYPE_MUSEPACK_DEC);
static void
gst_musepackdec_class_init (GstMusepackDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class, "Musepack decoder",
      "Codec/Decoder/Audio",
      "Musepack decoder", "Ronald Bultje <rbultje@ronald.bitfreak.net>");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_musepackdec_dispose);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_musepackdec_change_state);
}

static void
gst_musepackdec_init (GstMusepackDec * musepackdec)
{
  musepackdec->offset = 0;
  musepackdec->rate = 0;
  musepackdec->bps = 0;

  musepackdec->r = g_new (mpc_reader, 1);

  musepackdec->sinkpad =
      gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_activate_function (musepackdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_musepackdec_sink_activate));
  gst_pad_set_activatemode_function (musepackdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_musepackdec_sink_activate_mode));
  gst_element_add_pad (GST_ELEMENT (musepackdec), musepackdec->sinkpad);

  musepackdec->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_event_function (musepackdec->srcpad,
      GST_DEBUG_FUNCPTR (gst_musepackdec_src_event));
  gst_pad_set_query_function (musepackdec->srcpad,
      GST_DEBUG_FUNCPTR (gst_musepackdec_src_query));
  gst_pad_use_fixed_caps (musepackdec->srcpad);
  gst_element_add_pad (GST_ELEMENT (musepackdec), musepackdec->srcpad);
}

static void
gst_musepackdec_dispose (GObject * obj)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (obj);

  g_free (musepackdec->r);
  musepackdec->r = NULL;

  if (musepackdec->d) {
    mpc_demux_exit (musepackdec->d);
    musepackdec->d = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_musepackdec_send_newsegment (GstMusepackDec * dec)
{
  GstSegment os = dec->segment;

  os.format = GST_FORMAT_TIME;
  os.start = gst_util_uint64_scale_int (os.start, GST_SECOND, dec->rate);
  if (os.stop)
    os.stop = gst_util_uint64_scale_int (os.stop, GST_SECOND, dec->rate);
  os.time = gst_util_uint64_scale_int (os.time, GST_SECOND, dec->rate);

  GST_DEBUG_OBJECT (dec, "sending newsegment from %" GST_TIME_FORMAT
      " to %" GST_TIME_FORMAT ", rate = %.1f", GST_TIME_ARGS (os.start),
      GST_TIME_ARGS (os.stop), os.rate);

  gst_pad_push_event (dec->srcpad, gst_event_new_segment (&os));
}

static gboolean
gst_musepackdec_handle_seek_event (GstMusepackDec * dec, GstEvent * event)
{
  GstSeekType start_type, stop_type;
  GstSeekFlags flags;
  GstSegment segment;
  GstFormat format;
  gboolean flush;
  gdouble rate;
  gint64 start, stop;
  gint samplerate;

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  if (format != GST_FORMAT_TIME && format != GST_FORMAT_DEFAULT) {
    GST_DEBUG_OBJECT (dec, "seek failed: only TIME or DEFAULT format allowed");
    return FALSE;
  }

  samplerate = g_atomic_int_get (&dec->rate);

  if (format == GST_FORMAT_TIME) {
    if (start_type != GST_SEEK_TYPE_NONE)
      start = gst_util_uint64_scale_int (start, samplerate, GST_SECOND);
    if (stop_type != GST_SEEK_TYPE_NONE)
      stop = gst_util_uint64_scale_int (stop, samplerate, GST_SECOND);
  }

  flush = ((flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH);

  if (flush)
    gst_pad_push_event (dec->srcpad, gst_event_new_flush_start ());
  else
    gst_pad_pause_task (dec->sinkpad);  /* not _stop_task()? */

  GST_PAD_STREAM_LOCK (dec->sinkpad);

  /* operate on segment copy until we know the seek worked */
  segment = dec->segment;

  gst_segment_do_seek (&segment, rate, GST_FORMAT_DEFAULT,
      flags, start_type, start, stop_type, stop, NULL);

  gst_pad_push_event (dec->sinkpad, gst_event_new_flush_stop (TRUE));

  GST_DEBUG_OBJECT (dec, "segment: [%" G_GINT64_FORMAT "-%" G_GINT64_FORMAT
      "] = [%" GST_TIME_FORMAT "-%" GST_TIME_FORMAT "]",
      segment.start, segment.stop,
      GST_TIME_ARGS (segment.start * GST_SECOND / dec->rate),
      GST_TIME_ARGS (segment.stop * GST_SECOND / dec->rate));

  GST_DEBUG_OBJECT (dec, "performing seek to sample %" G_GINT64_FORMAT,
      segment.start);

  if (segment.start >= segment.duration) {
    GST_WARNING_OBJECT (dec, "seek out of bounds");
    goto failed;
  }
  if (mpc_demux_seek_sample (dec->d, segment.start) != MPC_STATUS_OK)
    goto failed;

  if ((flags & GST_SEEK_FLAG_SEGMENT) == GST_SEEK_FLAG_SEGMENT) {
    GST_DEBUG_OBJECT (dec, "posting SEGMENT_START message");

    gst_element_post_message (GST_ELEMENT (dec),
        gst_message_new_segment_start (GST_OBJECT (dec), GST_FORMAT_TIME,
            gst_util_uint64_scale_int (segment.start, GST_SECOND, dec->rate)));
  }

  if (flush) {
    gst_pad_push_event (dec->srcpad, gst_event_new_flush_stop (TRUE));
  }

  segment.position = segment.start;
  dec->segment = segment;
  gst_musepackdec_send_newsegment (dec);

  GST_DEBUG_OBJECT (dec, "seek successful");

  gst_pad_start_task (dec->sinkpad,
      (GstTaskFunction) gst_musepackdec_loop, dec->sinkpad, NULL);

  GST_PAD_STREAM_UNLOCK (dec->sinkpad);

  return TRUE;

failed:
  {
    GST_WARNING_OBJECT (dec, "seek failed");
    GST_PAD_STREAM_UNLOCK (dec->sinkpad);
    return FALSE;
  }
}

static gboolean
gst_musepackdec_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMusepackDec *dec;
  gboolean res;

  dec = GST_MUSEPACK_DEC (parent);

  GST_DEBUG_OBJECT (dec, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      res = gst_musepackdec_handle_seek_event (dec, event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static gboolean
gst_musepackdec_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (parent);
  GstFormat format;
  gboolean res = FALSE;
  gint samplerate;

  samplerate = g_atomic_int_get (&musepackdec->rate);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:{
      gint64 cur, cur_off;

      if (samplerate == 0)
        goto done;

      gst_query_parse_position (query, &format, NULL);

      GST_OBJECT_LOCK (musepackdec);
      cur_off = musepackdec->segment.position;
      GST_OBJECT_UNLOCK (musepackdec);

      if (format == GST_FORMAT_TIME) {
        cur = gst_util_uint64_scale_int (cur_off, GST_SECOND, samplerate);
        gst_query_set_position (query, GST_FORMAT_TIME, cur);
        res = TRUE;
      } else if (format == GST_FORMAT_DEFAULT) {
        gst_query_set_position (query, GST_FORMAT_DEFAULT, cur_off);
        res = TRUE;
      }
      break;
    }
    case GST_QUERY_DURATION:{
      gint64 len, len_off;

      if (samplerate == 0)
        goto done;

      gst_query_parse_duration (query, &format, NULL);

      GST_OBJECT_LOCK (musepackdec);
      len_off = musepackdec->segment.duration;
      GST_OBJECT_UNLOCK (musepackdec);

      if (format == GST_FORMAT_TIME) {
        len = gst_util_uint64_scale_int (len_off, GST_SECOND, samplerate);
        gst_query_set_duration (query, GST_FORMAT_TIME, len);
        res = TRUE;
      } else if (format == GST_FORMAT_DEFAULT) {
        gst_query_set_duration (query, GST_FORMAT_DEFAULT, len_off);
        res = TRUE;
      }
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gint64 len, len_off;

      res = TRUE;
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);

      GST_OBJECT_LOCK (musepackdec);
      len_off = musepackdec->segment.duration;
      GST_OBJECT_UNLOCK (musepackdec);

      if (fmt == GST_FORMAT_TIME) {
        len = gst_util_uint64_scale_int (len_off, GST_SECOND, samplerate);
        gst_query_set_seeking (query, fmt, TRUE, 0, len);
      } else if (fmt == GST_FORMAT_DEFAULT) {
        gst_query_set_seeking (query, fmt, TRUE, 0, len_off);
      } else {
        gst_query_set_seeking (query, fmt, FALSE, -1, -1);
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

done:
  return res;
}

static gboolean
gst_musepack_stream_init (GstMusepackDec * musepackdec)
{
  mpc_streaminfo i;
  GstTagList *tags;
  GstCaps *caps;
  gchar *stream_id;

  /* set up reading */
  gst_musepack_init_reader (musepackdec->r, musepackdec);

  musepackdec->d = mpc_demux_init (musepackdec->r);
  if (!musepackdec->d) {
    GST_ELEMENT_ERROR (musepackdec, STREAM, WRONG_TYPE, (NULL), (NULL));
    return FALSE;
  }

  mpc_demux_get_info (musepackdec->d, &i);

  stream_id = gst_pad_create_stream_id (musepackdec->srcpad,
      GST_ELEMENT_CAST (musepackdec), NULL);
  gst_pad_push_event (musepackdec->srcpad,
      gst_event_new_stream_start (stream_id));
  g_free (stream_id);

  /* capsnego */
  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_MPC_FORMAT,
      "layout", G_TYPE_STRING, "interleaved",
      "channels", G_TYPE_INT, i.channels,
      "rate", G_TYPE_INT, i.sample_freq, NULL);
  gst_pad_use_fixed_caps (musepackdec->srcpad);
  if (!gst_pad_set_caps (musepackdec->srcpad, caps)) {
    GST_ELEMENT_ERROR (musepackdec, CORE, NEGOTIATION, (NULL), (NULL));
    return FALSE;
  }

  g_atomic_int_set (&musepackdec->bps, 4 * i.channels);
  g_atomic_int_set (&musepackdec->rate, i.sample_freq);

  musepackdec->segment.position = 0;
  musepackdec->segment.duration = mpc_streaminfo_get_length_samples (&i);

  /* send basic tags */
  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
      GST_TAG_AUDIO_CODEC, "Musepack", NULL);

  if (i.encoder[0] != '\0' && i.encoder_version > 0) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_ENCODER, i.encoder,
        GST_TAG_ENCODER_VERSION, i.encoder_version, NULL);
  }

  if (i.bitrate > 0) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, i.bitrate, NULL);
  } else if (i.average_bitrate > 0.0) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, (guint) i.average_bitrate, NULL);
  }

  if (i.gain_title != 0 || i.gain_album != 0) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_TRACK_GAIN, (gdouble) i.gain_title / 100.0,
        GST_TAG_ALBUM_GAIN, (gdouble) i.gain_album / 100.0, NULL);
  }

  if (i.peak_title != 0 && i.peak_title != 32767 &&
      i.peak_album != 0 && i.peak_album != 32767) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_TRACK_PEAK, (gdouble) i.peak_title / 32767.0,
        GST_TAG_ALBUM_PEAK, (gdouble) i.peak_album / 32767.0, NULL);
  }

  GST_LOG_OBJECT (musepackdec, "Posting tags: %" GST_PTR_FORMAT, tags);
  gst_pad_push_event (musepackdec->srcpad, gst_event_new_tag (tags));


  return TRUE;
}

static gboolean
gst_musepackdec_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  GstQuery *query;
  gboolean pull_mode;

  query = gst_query_new_scheduling ();

  if (!gst_pad_peer_query (sinkpad, query)) {
    gst_query_unref (query);
    return FALSE;
  }

  pull_mode = gst_query_has_scheduling_mode_with_flags (query,
      GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE);
  gst_query_unref (query);

  if (!pull_mode)
    return FALSE;

  GST_DEBUG_OBJECT (sinkpad, "activating pull");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);
}

static gboolean
gst_musepackdec_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean result;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      result = FALSE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        result = gst_pad_start_task (sinkpad,
            (GstTaskFunction) gst_musepackdec_loop, sinkpad, NULL);
      } else {
        result = gst_pad_stop_task (sinkpad);
      }
      break;
    default:
      result = FALSE;
      break;
  }

  return result;
}

static void
gst_musepackdec_loop (GstPad * sinkpad)
{
  GstMusepackDec *musepackdec;
  GstFlowReturn flow;
  GstBuffer *out;
  GstMapInfo info;
  mpc_frame_info frame;
  mpc_status err;
  gint num_samples, samplerate, bitspersample;

  musepackdec = GST_MUSEPACK_DEC (GST_PAD_PARENT (sinkpad));

  samplerate = g_atomic_int_get (&musepackdec->rate);

  if (samplerate == 0) {
    if (!gst_musepack_stream_init (musepackdec))
      goto pause_task;

    gst_musepackdec_send_newsegment (musepackdec);
    samplerate = g_atomic_int_get (&musepackdec->rate);
  }

  bitspersample = g_atomic_int_get (&musepackdec->bps);

  out = gst_buffer_new_allocate (NULL, MPC_DECODER_BUFFER_LENGTH * 4, NULL);

  gst_buffer_map (out, &info, GST_MAP_READWRITE);
  frame.buffer = (MPC_SAMPLE_FORMAT *) info.data;
  err = mpc_demux_decode (musepackdec->d, &frame);
  gst_buffer_unmap (out, &info);

  if (err != MPC_STATUS_OK) {
    GST_ERROR_OBJECT (musepackdec, "Failed to decode sample");
    GST_ELEMENT_ERROR (musepackdec, STREAM, DECODE, (NULL), (NULL));
    goto pause_task;
  } else if (frame.bits == -1) {
    goto eos_and_pause;
  }

  num_samples = frame.samples;

  gst_buffer_set_size (out, num_samples * bitspersample);

  GST_BUFFER_OFFSET (out) = musepackdec->segment.position;
  GST_BUFFER_PTS (out) =
      gst_util_uint64_scale_int (musepackdec->segment.position,
      GST_SECOND, samplerate);
  GST_BUFFER_DURATION (out) =
      gst_util_uint64_scale_int (num_samples, GST_SECOND, samplerate);

  musepackdec->segment.position += num_samples;

  GST_LOG_OBJECT (musepackdec, "Pushing buffer, timestamp %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (out)));

  flow = gst_pad_push (musepackdec->srcpad, out);
  if (flow != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (musepackdec, "Flow: %s", gst_flow_get_name (flow));
    goto pause_task;
  }

  /* check if we're at the end of a configured segment */
  if (musepackdec->segment.stop != -1 &&
      musepackdec->segment.position >= musepackdec->segment.stop) {
    gint64 stop_time;

    GST_DEBUG_OBJECT (musepackdec, "Reached end of configured segment");

    if ((musepackdec->segment.flags & GST_SEEK_FLAG_SEGMENT) == 0)
      goto eos_and_pause;

    GST_DEBUG_OBJECT (musepackdec, "Posting SEGMENT_DONE message");

    stop_time = gst_util_uint64_scale_int (musepackdec->segment.stop,
        GST_SECOND, samplerate);

    gst_element_post_message (GST_ELEMENT (musepackdec),
        gst_message_new_segment_done (GST_OBJECT (musepackdec),
            GST_FORMAT_TIME, stop_time));
    gst_pad_push_event (musepackdec->srcpad,
        gst_event_new_segment_done (GST_FORMAT_TIME, stop_time));

    goto pause_task;
  }

  return;

eos_and_pause:
  {
    GST_DEBUG_OBJECT (musepackdec, "sending EOS event");
    gst_pad_push_event (musepackdec->srcpad, gst_event_new_eos ());
    /* fall through to pause */
  }

pause_task:
  {
    GST_DEBUG_OBJECT (musepackdec, "Pausing task");
    gst_pad_pause_task (sinkpad);
    return;
  }
}

static GstStateChangeReturn
gst_musepackdec_change_state (GstElement * element, GstStateChange transition)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_segment_init (&musepackdec->segment, GST_FORMAT_DEFAULT);
      musepackdec->segment.position = 0;
      break;
    default:
      break;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_segment_init (&musepackdec->segment, GST_FORMAT_UNDEFINED);
      musepackdec->offset = 0;
      musepackdec->rate = 0;
      musepackdec->bps = 0;
      break;
    default:
      break;
  }

  return ret;

}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (musepackdec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    musepack,
    "Musepack decoder", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
