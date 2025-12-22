/* -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GStreamer AIFF parser
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *               <2006> Nokia Corporation, Stefan Kost <stefan.kost@nokia.com>.
 *               <2008> Pioneers of the Inevitable <songbird@songbirdnest.com>
 *
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
 * SECTION:element-aiffparse
 * @title: aiffparse
 *
 * Parse a .aiff file into raw or compressed audio.
 *
 * The aiffparse element supports both push and pull mode operations, making it
 * possible to stream from a network source.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 filesrc location=sine.aiff ! aiffparse ! audioconvert ! alsasink
 * ]|
 * Read a aiff file and output to the soundcard using the ALSA element. The
 * aiff file is assumed to contain raw uncompressed samples.
 *
 * |[
 * gst-launch-1.0 souphttpsrc location=http://www.example.org/sine.aiff ! queue ! aiffparse ! audioconvert ! alsasink
 * ]|
 * Stream data from a network url.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include "aiffelements.h"
#include "aiffparse.h"
#include <gst/audio/audio.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/descriptions.h>
#include <glib/gi18n-lib.h>

GST_DEBUG_CATEGORY (aiffparse_debug);
#define GST_CAT_DEFAULT (aiffparse_debug)

static void gst_aiff_parse_dispose (GObject * object);

static gboolean gst_aiff_parse_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_aiff_parse_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static gboolean gst_aiff_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * buf);
static gboolean gst_aiff_parse_send_event (GstElement * element,
    GstEvent * event);
static GstStateChangeReturn gst_aiff_parse_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_aiff_parse_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_aiff_parse_pad_convert (GstPad * pad,
    GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);

static GstFlowReturn gst_aiff_parse_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static void gst_aiff_parse_loop (GstPad * pad);
static gboolean gst_aiff_parse_srcpad_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-aiff")
    );

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE ("{ S8, S16BE, S16LE, S24BE, S24LE, "
            "S32LE, S32BE, F32BE, F64BE }"))
    );

#define MAX_BUFFER_SIZE 4096

#define gst_aiff_parse_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAiffParse, gst_aiff_parse, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (aiffparse_debug, "aiffparse", 0, "AIFF parser"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (aiffparse, "aiffparse", GST_RANK_PRIMARY,
    GST_TYPE_AIFF_PARSE, aiff_element_init (plugin));

static void
gst_aiff_parse_class_init (GstAiffParseClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *object_class;

  gstelement_class = (GstElementClass *) klass;
  object_class = (GObjectClass *) klass;

  object_class->dispose = gst_aiff_parse_dispose;

  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "AIFF audio demuxer", "Codec/Demuxer/Audio",
      "Parse a .aiff file into raw audio",
      "Pioneers of the Inevitable <songbird@songbirdnest.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_aiff_parse_change_state);
  gstelement_class->send_event = GST_DEBUG_FUNCPTR (gst_aiff_parse_send_event);
}

static void
gst_aiff_parse_reset (GstAiffParse * aiff)
{
  aiff->state = AIFF_PARSE_START;

  /* These will all be set correctly in the fmt chunk */
  aiff->rate = 0;
  aiff->width = 0;
  aiff->depth = 0;
  aiff->channels = 0;
  aiff->bps = 0;
  aiff->offset = 0;
  aiff->end_offset = 0;
  aiff->dataleft = 0;
  aiff->datasize = 0;
  aiff->datastart = 0;
  aiff->duration = 0;
  aiff->got_comm = FALSE;

  if (aiff->seek_event)
    gst_event_unref (aiff->seek_event);
  aiff->seek_event = NULL;
  if (aiff->adapter) {
    gst_adapter_clear (aiff->adapter);
    aiff->adapter = NULL;
  }

  if (aiff->tags != NULL) {
    gst_tag_list_unref (aiff->tags);
    aiff->tags = NULL;
  }
}

static void
gst_aiff_parse_dispose (GObject * object)
{
  GstAiffParse *aiff = GST_AIFF_PARSE (object);

  GST_DEBUG_OBJECT (aiff, "AIFF: Dispose");
  gst_aiff_parse_reset (aiff);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_aiff_parse_init (GstAiffParse * aiffparse)
{
  gst_aiff_parse_reset (aiffparse);

  /* sink */
  aiffparse->sinkpad =
      gst_pad_new_from_static_template (&sink_template_factory, "sink");
  gst_pad_set_activate_function (aiffparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_sink_activate));
  gst_pad_set_activatemode_function (aiffparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_sink_activate_mode));
  gst_pad_set_event_function (aiffparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_sink_event));
  gst_pad_set_chain_function (aiffparse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_chain));
  gst_element_add_pad (GST_ELEMENT_CAST (aiffparse), aiffparse->sinkpad);

  /* source */
  aiffparse->srcpad =
      gst_pad_new_from_static_template (&src_template_factory, "src");
  gst_pad_use_fixed_caps (aiffparse->srcpad);
  gst_pad_set_query_function (aiffparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_pad_query));
  gst_pad_set_event_function (aiffparse->srcpad,
      GST_DEBUG_FUNCPTR (gst_aiff_parse_srcpad_event));
  gst_element_add_pad (GST_ELEMENT_CAST (aiffparse), aiffparse->srcpad);
}

static gboolean
gst_aiff_parse_parse_file_header (GstAiffParse * aiff, GstBuffer * buf)
{
  guint32 header, type = 0;
  GstMapInfo info;

  if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
    GST_WARNING_OBJECT (aiff, "Could not map buffer");
    goto not_aiff;
  }

  if (info.size < 12) {
    GST_WARNING_OBJECT (aiff, "Buffer too short");
    gst_buffer_unmap (buf, &info);
    goto not_aiff;
  }

  header = GST_READ_UINT32_LE (info.data);
  type = GST_READ_UINT32_LE (info.data + 8);
  gst_buffer_unmap (buf, &info);

  if (header != GST_MAKE_FOURCC ('F', 'O', 'R', 'M'))
    goto not_aiff;

  if (type == GST_MAKE_FOURCC ('A', 'I', 'F', 'F'))
    aiff->is_aifc = FALSE;
  else if (type == GST_MAKE_FOURCC ('A', 'I', 'F', 'C'))
    aiff->is_aifc = TRUE;
  else
    goto not_aiff;

  gst_buffer_unref (buf);
  return TRUE;

  /* ERRORS */
not_aiff:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, WRONG_TYPE, (NULL),
        ("File is not an AIFF file: 0x%" G_GINT32_MODIFIER "x", type));
    gst_buffer_unref (buf);
    return FALSE;
  }
}

static GstFlowReturn
gst_aiff_parse_stream_init (GstAiffParse * aiff)
{
  GstFlowReturn res;
  GstBuffer *buf = NULL;

  if ((res = gst_pad_pull_range (aiff->sinkpad,
              aiff->offset, 12, &buf)) != GST_FLOW_OK)
    return res;
  else if (!gst_aiff_parse_parse_file_header (aiff, buf))
    return GST_FLOW_ERROR;

  aiff->offset += 12;

  return GST_FLOW_OK;
}

static gboolean
gst_aiff_parse_time_to_bytepos (GstAiffParse * aiff, gint64 ts,
    gint64 * bytepos)
{
  /* -1 always maps to -1 */
  if (ts == -1) {
    *bytepos = -1;
    return TRUE;
  }

  /* 0 always maps to 0 */
  if (ts == 0) {
    *bytepos = 0;
    return TRUE;
  }

  if (aiff->bps > 0) {
    *bytepos = gst_util_uint64_scale_ceil (ts, (guint64) aiff->bps, GST_SECOND);
    return TRUE;
  }

  GST_WARNING_OBJECT (aiff, "No valid bps to convert position");

  return FALSE;
}

/* This function is used to perform seeks on the element in
 * pull mode.
 *
 * It also works when event is NULL, in which case it will just
 * start from the last configured segment. This technique is
 * used when activating the element and to perform the seek in
 * READY.
 */
static gboolean
gst_aiff_parse_perform_seek (GstAiffParse * aiff, GstEvent * event,
    gboolean starting)
{
  gboolean res;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type = GST_SEEK_TYPE_NONE, stop_type;
  gint64 start, stop, upstream_size;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment = { 0, };
  gint64 position;

  if (event) {
    GST_DEBUG_OBJECT (aiff, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &start_type, &start, &stop_type, &stop);

    /* no negative rates yet */
    if (rate < 0.0)
      goto negative_rate;

    if (format != aiff->segment.format) {
      GST_INFO_OBJECT (aiff, "converting seek-event from %s to %s",
          gst_format_get_name (format),
          gst_format_get_name (aiff->segment.format));
      res = TRUE;
      if (start_type != GST_SEEK_TYPE_NONE)
        res =
            gst_pad_query_convert (aiff->srcpad, format, start,
            aiff->segment.format, &start);
      if (res && stop_type != GST_SEEK_TYPE_NONE)
        res =
            gst_pad_query_convert (aiff->srcpad, format, stop,
            aiff->segment.format, &stop);
      if (!res)
        goto no_format;

      format = aiff->segment.format;
    }
  } else {
    GST_DEBUG_OBJECT (aiff, "doing seek without event");
    flags = 0;
    rate = 1.0;
    start = 0;
    start_type = GST_SEEK_TYPE_SET;
    stop = -1;
    stop_type = GST_SEEK_TYPE_SET;
  }

  /* get flush flag */
  flush = flags & GST_SEEK_FLAG_FLUSH;

  if (aiff->streaming && !starting) {
    GstEvent *new_event;

    /* streaming seek */
    if ((start_type != GST_SEEK_TYPE_NONE)) {
      /* bring offset to bytes, if the bps is 0, we have the segment in BYTES and
       * we can just copy the position. If not, we use the bps to convert TIME to
       * bytes. */
      if (aiff->bps > 0)
        start =
            gst_util_uint64_scale_ceil (start, (guint64) aiff->bps, GST_SECOND);
      start -= (start % aiff->bytes_per_sample);
      start += aiff->datastart;
    }

    if (stop_type != GST_SEEK_TYPE_NONE) {
      if (aiff->bps > 0)
        stop =
            gst_util_uint64_scale_ceil (stop, (guint64) aiff->bps, GST_SECOND);
      stop -= (stop % aiff->bytes_per_sample);
      stop += aiff->datastart;
    }

    /* make sure filesize is not exceeded due to rounding errors or so,
     * same precaution as in _stream_headers */
    if (gst_pad_peer_query_duration (aiff->sinkpad, GST_FORMAT_BYTES,
            &upstream_size))
      stop = MIN (stop, upstream_size);

    if (stop >= 0 && stop <= start)
      stop = start;

    new_event = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags,
        start_type, start, stop_type, stop);

    res = gst_pad_push_event (aiff->sinkpad, new_event);
  } else {
    /* now we need to make sure the streaming thread is stopped. We do this by
     * either sending a FLUSH_START event downstream which will cause the
     * streaming thread to stop with a FLUSHING.
     * For a non-flushing seek we simply pause the task, which will happen as soon
     * as it completes one iteration (and thus might block when the sink is
     * blocking in preroll). */
    if (flush) {
      GST_DEBUG_OBJECT (aiff, "sending flush start");
      gst_pad_push_event (aiff->srcpad, gst_event_new_flush_start ());
    } else {
      gst_pad_pause_task (aiff->sinkpad);
    }

    /* we should now be able to grab the streaming thread because we stopped it
     * with the above flush/pause code */
    GST_PAD_STREAM_LOCK (aiff->sinkpad);

    /* save current position */
    position = aiff->segment.position;

    GST_DEBUG_OBJECT (aiff, "stopped streaming at %" G_GINT64_FORMAT, position);

    /* copy segment, we need this because we still need the old
     * segment when we close the current segment. */
    memcpy (&seeksegment, &aiff->segment, sizeof (GstSegment));

    /* configure the seek parameters in the seeksegment. We will then have the
     * right values in the segment to perform the seek */
    if (event) {
      GST_DEBUG_OBJECT (aiff, "configuring seek");
      gst_segment_do_seek (&seeksegment, rate, format, flags,
          start_type, start, stop_type, stop, &update);
    }

    /* figure out the last position we need to play. If it's configured (stop !=
     * -1), use that, else we play until the total duration of the file */
    if ((stop = seeksegment.stop) == -1)
      stop = seeksegment.duration;

    GST_DEBUG_OBJECT (aiff, "start_type =%d", start_type);
    if ((start_type != GST_SEEK_TYPE_NONE)) {
      /* bring offset to bytes, if the bps is 0, we have the segment in BYTES and
       * we can just copy the position. If not, we use the bps to convert TIME to
       * bytes. */
      if (aiff->bps > 0)
        aiff->offset =
            gst_util_uint64_scale_ceil (seeksegment.position,
            (guint64) aiff->bps, GST_SECOND);
      else
        aiff->offset = seeksegment.position;
      GST_LOG_OBJECT (aiff, "offset=%" G_GUINT64_FORMAT, aiff->offset);
      aiff->offset -= (aiff->offset % aiff->bytes_per_sample);
      GST_LOG_OBJECT (aiff, "offset=%" G_GUINT64_FORMAT, aiff->offset);
      aiff->offset += aiff->datastart;
      GST_LOG_OBJECT (aiff, "offset=%" G_GUINT64_FORMAT, aiff->offset);
    } else {
      GST_LOG_OBJECT (aiff, "continue from offset=%" G_GUINT64_FORMAT,
          aiff->offset);
    }

    if (stop_type != GST_SEEK_TYPE_NONE) {
      if (aiff->bps > 0)
        aiff->end_offset =
            gst_util_uint64_scale_ceil (stop, (guint64) aiff->bps, GST_SECOND);
      else
        aiff->end_offset = stop;
      GST_LOG_OBJECT (aiff, "end_offset=%" G_GUINT64_FORMAT, aiff->end_offset);
      aiff->end_offset -= (aiff->end_offset % aiff->bytes_per_sample);
      GST_LOG_OBJECT (aiff, "end_offset=%" G_GUINT64_FORMAT, aiff->end_offset);
      aiff->end_offset += aiff->datastart;
      GST_LOG_OBJECT (aiff, "end_offset=%" G_GUINT64_FORMAT, aiff->end_offset);
    } else {
      GST_LOG_OBJECT (aiff, "continue to end_offset=%" G_GUINT64_FORMAT,
          aiff->end_offset);
    }

    /* make sure filesize is not exceeded due to rounding errors or so,
     * same precaution as in _stream_headers */
    if (gst_pad_peer_query_duration (aiff->sinkpad, GST_FORMAT_BYTES,
            &upstream_size))
      aiff->end_offset = MIN (aiff->end_offset, upstream_size);

    /* this is the range of bytes we will use for playback */
    aiff->offset = MIN (aiff->offset, aiff->end_offset);
    aiff->dataleft = aiff->end_offset - aiff->offset;

    GST_DEBUG_OBJECT (aiff,
        "seek: rate %lf, offset %" G_GUINT64_FORMAT ", end %" G_GUINT64_FORMAT
        ", segment %" GST_TIME_FORMAT " -- %" GST_TIME_FORMAT, rate,
        aiff->offset, aiff->end_offset, GST_TIME_ARGS (seeksegment.start),
        GST_TIME_ARGS (stop));

    /* prepare for streaming again */
    if (flush) {
      /* if we sent a FLUSH_START, we now send a FLUSH_STOP */
      GST_DEBUG_OBJECT (aiff, "sending flush stop");
      gst_pad_push_event (aiff->srcpad, gst_event_new_flush_stop (TRUE));
    }

    /* now we did the seek and can activate the new segment values */
    memcpy (&aiff->segment, &seeksegment, sizeof (GstSegment));

    /* if we're doing a segment seek, post a SEGMENT_START message */
    if (aiff->segment.flags & GST_SEEK_FLAG_SEGMENT) {
      gst_element_post_message (GST_ELEMENT_CAST (aiff),
          gst_message_new_segment_start (GST_OBJECT_CAST (aiff),
              aiff->segment.format, aiff->segment.position));
    }

    /* now create the segment */
    GST_DEBUG_OBJECT (aiff, "Creating segment from %" G_GINT64_FORMAT
        " to %" G_GINT64_FORMAT, aiff->segment.position, stop);

    /* store the segment event so it can be sent from the streaming thread. */
    if (aiff->start_segment)
      gst_event_unref (aiff->start_segment);
    aiff->start_segment = gst_event_new_segment (&aiff->segment);

    /* mark discont if we are going to stream from another position. */
    if (position != aiff->segment.position) {
      GST_DEBUG_OBJECT (aiff,
          "mark DISCONT, we did a seek to another position");
      aiff->discont = TRUE;
    }

    /* and start the streaming task again */
    aiff->segment_running = TRUE;
    if (!aiff->streaming) {
      gst_pad_start_task (aiff->sinkpad, (GstTaskFunction) gst_aiff_parse_loop,
          aiff->sinkpad, NULL);
    }

    GST_PAD_STREAM_UNLOCK (aiff->sinkpad);

    res = TRUE;
  }

  return res;

  /* ERRORS */
negative_rate:
  {
    GST_DEBUG_OBJECT (aiff, "negative playback rates are not supported yet.");
    return FALSE;
  }
no_format:
  {
    GST_DEBUG_OBJECT (aiff, "unsupported format given, seek aborted.");
    return FALSE;
  }
}

/*
 * gst_aiff_parse_peek_chunk_info:
 * @aiff AIFFparse object
 * @tag holder for tag
 * @size holder for tag size
 *
 * Peek next chunk info (tag and size)
 *
 * Returns: %TRUE when the chunk info (header) is available
 */
static gboolean
gst_aiff_parse_peek_chunk_info (GstAiffParse * aiff, guint32 * tag,
    guint32 * size)
{
  const guint8 *data = NULL;

  if (gst_adapter_available (aiff->adapter) < 8)
    return FALSE;

  data = gst_adapter_map (aiff->adapter, 8);
  *tag = GST_READ_UINT32_LE (data);
  *size = GST_READ_UINT32_BE (data + 4);
  gst_adapter_unmap (aiff->adapter);

  GST_DEBUG_OBJECT (aiff,
      "Next chunk size is %d bytes, type %" GST_FOURCC_FORMAT, *size,
      GST_FOURCC_ARGS (*tag));

  return TRUE;
}

/*
 * gst_aiff_parse_peek_chunk:
 * @aiff AIFFparse object
 * @tag holder for tag
 * @size holder for tag size
 *
 * Peek enough data for one full chunk
 *
 * Returns: %TRUE when the full chunk is available
 */
static gboolean
gst_aiff_parse_peek_chunk (GstAiffParse * aiff, guint32 * tag, guint32 * size)
{
  guint32 peek_size = 0;
  guint available;

  if (!gst_aiff_parse_peek_chunk_info (aiff, tag, size))
    return FALSE;

  GST_DEBUG_OBJECT (aiff, "Need to peek chunk of %d bytes", *size);
  peek_size = (*size + 1) & ~1;

  available = gst_adapter_available (aiff->adapter);
  if (available >= (8 + peek_size)) {
    return TRUE;
  } else {
    GST_LOG_OBJECT (aiff, "but only %u bytes available now", available);
    return FALSE;
  }
}

static gboolean
gst_aiff_parse_peek_data (GstAiffParse * aiff, guint32 size,
    const guint8 ** data)
{
  if (gst_adapter_available (aiff->adapter) < size)
    return FALSE;

  *data = gst_adapter_map (aiff->adapter, size);
  return TRUE;
}

/*
 * gst_aiff_parse_calculate_duration:
 * @aiff: aiffparse object
 *
 * Calculate duration on demand and store in @aiff.
 *
 * Returns: %TRUE if duration is available.
 */
static gboolean
gst_aiff_parse_calculate_duration (GstAiffParse * aiff)
{
  if (aiff->duration > 0)
    return TRUE;

  if (aiff->datasize > 0 && aiff->bps > 0) {
    aiff->duration =
        gst_util_uint64_scale_ceil (aiff->datasize, GST_SECOND,
        (guint64) aiff->bps);
    GST_INFO_OBJECT (aiff, "Got duration %" GST_TIME_FORMAT,
        GST_TIME_ARGS (aiff->duration));
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_aiff_parse_ignore_chunk (GstAiffParse * aiff, guint32 tag, guint32 size)
{
  guint flush;

  if (aiff->streaming) {
    if (!gst_aiff_parse_peek_chunk (aiff, &tag, &size)) {
      GST_LOG_OBJECT (aiff, "Not enough data to skip tag %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (tag));
      return FALSE;
    }
  }
  GST_WARNING_OBJECT (aiff, "Ignoring tag %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (tag));
  flush = 8 + ((size + 1) & ~1);
  aiff->offset += flush;
  if (aiff->streaming) {
    gst_adapter_flush (aiff->adapter, flush);
  }
  return TRUE;
}

static double
gst_aiff_parse_read_IEEE80 (guint8 * buf)
{
  int s = buf[0] & 0xff;
  int e = ((buf[0] & 0x7f) << 8) | (buf[1] & 0xff);
  double f = ((unsigned long) (buf[2] & 0xff) << 24) |
      ((buf[3] & 0xff) << 16) | ((buf[4] & 0xff) << 8) | (buf[5] & 0xff);

  if (e == 32767) {
    if (buf[2] & 0x80)
      return HUGE_VAL;          /* Really NaN, but this won't happen in reality */
    else {
      if (s)
        return -HUGE_VAL;
      else
        return HUGE_VAL;
    }
  }

  f = ldexp (f, 32);
  f += ((buf[6] & 0xff) << 24) |
      ((buf[7] & 0xff) << 16) | ((buf[8] & 0xff) << 8) | (buf[9] & 0xff);

  return ldexp (f, e - 16446);
}

static gboolean
gst_aiff_parse_parse_comm (GstAiffParse * aiff, GstBuffer * buf)
{
  int size;
  GstMapInfo info;
  guint32 fourcc;

  if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
    GST_WARNING_OBJECT (aiff, "Can't map buffer");
    gst_buffer_unref (buf);
    return FALSE;
  }

  if (aiff->is_aifc)
    size = 22;
  else
    size = 18;

  if (info.size < size)
    goto too_small;

  aiff->channels = GST_READ_UINT16_BE (info.data);
  aiff->total_frames = GST_READ_UINT32_BE (info.data + 2);
  aiff->depth = GST_READ_UINT16_BE (info.data + 6);
  aiff->width = GST_ROUND_UP_8 (aiff->depth);
  aiff->rate = (int) gst_aiff_parse_read_IEEE80 (info.data + 8);

  aiff->floating_point = FALSE;

  if (aiff->is_aifc) {
    fourcc = GST_READ_UINT32_LE (info.data + 18);

    /* We only support the 'trivial' uncompressed AIFC, but it can be
     * either big or little endian */
    switch (fourcc) {
      case GST_MAKE_FOURCC ('N', 'O', 'N', 'E'):
        aiff->endianness = G_BIG_ENDIAN;
        break;
      case GST_MAKE_FOURCC ('s', 'o', 'w', 't'):
        aiff->endianness = G_LITTLE_ENDIAN;
        break;
      case GST_MAKE_FOURCC ('F', 'L', '3', '2'):
      case GST_MAKE_FOURCC ('f', 'l', '3', '2'):
        aiff->floating_point = TRUE;
        aiff->width = aiff->depth = 32;
        aiff->endianness = G_BIG_ENDIAN;
        break;
      case GST_MAKE_FOURCC ('f', 'l', '6', '4'):
        aiff->floating_point = TRUE;
        aiff->width = aiff->depth = 64;
        aiff->endianness = G_BIG_ENDIAN;
        break;
      default:
        goto unknown_compression;
    }
  } else
    aiff->endianness = G_BIG_ENDIAN;

  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);

  return TRUE;

  /* ERRORS */
too_small:
  {
    GST_WARNING_OBJECT (aiff, "COMM chunk too short, cannot parse header");
    gst_buffer_unmap (buf, &info);
    gst_buffer_unref (buf);
    return FALSE;
  }
unknown_compression:
  {
    GST_WARNING_OBJECT (aiff, "Unsupported compression in AIFC "
        "file: %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
    gst_buffer_unmap (buf, &info);
    gst_buffer_unref (buf);
    return FALSE;
  }
}

static GstFlowReturn
gst_aiff_parse_read_chunk (GstAiffParse * aiff, guint64 * offset, guint32 * tag,
    GstBuffer ** data)
{
  guint size;
  GstFlowReturn res;
  GstBuffer *buf = NULL;
  GstMapInfo info;

  if ((res =
          gst_pad_pull_range (aiff->sinkpad, *offset, 8, &buf)) != GST_FLOW_OK)
    return res;

  gst_buffer_map (buf, &info, GST_MAP_READ);
  *tag = GST_READ_UINT32_LE (info.data);
  size = GST_READ_UINT32_BE (info.data + 4);
  gst_buffer_unmap (buf, &info);
  gst_buffer_unref (buf);
  buf = NULL;

  if ((res =
          gst_pad_pull_range (aiff->sinkpad, (*offset) + 8, size,
              &buf)) != GST_FLOW_OK)
    return res;
  else if (gst_buffer_get_size (buf) < size)
    goto too_small;

  *data = buf;
  *offset += 8 + GST_ROUND_UP_2 (size);

  return GST_FLOW_OK;

  /* ERRORS */
too_small:
  {
    /* short read, we return EOS to mark the EOS case */
    GST_DEBUG_OBJECT (aiff,
        "not enough data (available=%" G_GSIZE_FORMAT ", needed=%u)",
        gst_buffer_get_size (buf), size);
    gst_buffer_unref (buf);
    return GST_FLOW_EOS;
  }

}

#define _P(pos) (G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_ ##pos)

static GstCaps *
gst_aiff_parse_create_caps (GstAiffParse * aiff)
{
  GstCaps *caps = NULL;
  const gchar *format = NULL;
  guint64 channel_mask = 0;

  if (aiff->floating_point) {
    if (aiff->endianness == G_BIG_ENDIAN) {
      if (aiff->width == 32)
        format = "F32BE";
      else if (aiff->width == 64)
        format = "F64BE";
    }
  } else {
    if (aiff->endianness == G_BIG_ENDIAN) {
      if (aiff->width == 8)
        format = "S8";
      else if (aiff->width == 16)
        format = "S16BE";
      else if (aiff->width == 24)
        format = "S24BE";
      else if (aiff->width == 32)
        format = "S32BE";
    } else {
      if (aiff->width == 8)
        format = "S8";
      else if (aiff->width == 16)
        format = "S16LE";
      else if (aiff->width == 24)
        format = "S24LE";
      else if (aiff->width == 32)
        format = "S32LE";
    }
  }
  if (format) {
    caps = gst_caps_new_simple ("audio/x-raw",
        "format", G_TYPE_STRING, format,
        "channels", G_TYPE_INT, aiff->channels,
        "layout", G_TYPE_STRING, "interleaved",
        "rate", G_TYPE_INT, aiff->rate, NULL);
  }

  /* based on AIFF-1.3.pdf */
  switch (aiff->channels) {
    case 1:
      channel_mask = 0;
      break;
    case 2:
      channel_mask = _P (FRONT_LEFT) | _P (FRONT_RIGHT);
      break;
    case 3:
      channel_mask = _P (FRONT_LEFT) | _P (FRONT_RIGHT) | _P (FRONT_CENTER);
      break;
    case 4:
      /* lists both this and 'quad' but doesn't say how to distinguish the two */
      channel_mask =
          _P (FRONT_LEFT) | _P (FRONT_RIGHT) | _P (REAR_LEFT) | _P (REAR_RIGHT);
      break;
    case 6:
      channel_mask =
          _P (FRONT_LEFT) | _P (FRONT_LEFT_OF_CENTER) | _P (FRONT_CENTER) |
          _P (FRONT_RIGHT) | _P (FRONT_RIGHT_OF_CENTER) | _P (LFE1);
      break;
    default:
      GST_FIXME_OBJECT (aiff, "using fallback channel layout for %d channels",
          aiff->channels);
      channel_mask = gst_audio_channel_get_fallback_mask (aiff->channels);
      break;
  }

  if (channel_mask != 0) {
    gst_caps_set_simple (caps, "channel-mask", GST_TYPE_BITMASK, channel_mask,
        NULL);
  }

  GST_DEBUG_OBJECT (aiff, "Created caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstFlowReturn
gst_aiff_parse_stream_headers (GstAiffParse * aiff)
{
  GstFlowReturn res;
  GstBuffer *buf = NULL;
  guint32 tag, size;
  gboolean gotdata = FALSE;
  gboolean done = FALSE;
  GstEvent **event_p;
  gint64 upstream_size = 0;

  gst_pad_peer_query_duration (aiff->sinkpad, GST_FORMAT_BYTES, &upstream_size);
  GST_DEBUG_OBJECT (aiff, "upstream size %" G_GUINT64_FORMAT, upstream_size);

  /* loop headers until we get data */
  while (!done) {
    if (aiff->streaming) {
      if (!gst_aiff_parse_peek_chunk_info (aiff, &tag, &size))
        return GST_FLOW_OK;
    } else {
      GstMapInfo info;

      if ((res =
              gst_pad_pull_range (aiff->sinkpad, aiff->offset, 8,
                  &buf)) != GST_FLOW_OK)
        goto header_read_error;

      gst_buffer_map (buf, &info, GST_MAP_READ);
      tag = GST_READ_UINT32_LE (info.data);
      size = GST_READ_UINT32_BE (info.data + 4);
      gst_buffer_unmap (buf, &info);
      gst_buffer_unref (buf);
      buf = NULL;
    }

    GST_INFO_OBJECT (aiff,
        "Got TAG: %" GST_FOURCC_FORMAT ", offset %" G_GUINT64_FORMAT,
        GST_FOURCC_ARGS (tag), aiff->offset);

    /* We just keep reading chunks until we find the one we're interested in.
     */
    switch (tag) {
      case GST_MAKE_FOURCC ('C', 'O', 'M', 'M'):{
        GstCaps *caps;
        GstEvent *event;
        gchar *stream_id;

        if (aiff->streaming) {
          if (!gst_aiff_parse_peek_chunk (aiff, &tag, &size))
            return GST_FLOW_OK;

          gst_adapter_flush (aiff->adapter, 8);
          aiff->offset += 8;

          buf = gst_adapter_take_buffer (aiff->adapter, size);
          aiff->offset += size;
        } else {
          if ((res = gst_aiff_parse_read_chunk (aiff,
                      &aiff->offset, &tag, &buf)) != GST_FLOW_OK)
            return res;
        }

        if (!gst_aiff_parse_parse_comm (aiff, buf))
          goto parse_header_error;

        /* do sanity checks of header fields */
        if (aiff->channels == 0)
          goto no_channels;
        if (aiff->rate == 0)
          goto no_rate;

        stream_id =
            gst_pad_create_stream_id (aiff->srcpad, GST_ELEMENT_CAST (aiff),
            NULL);
        event = gst_event_new_stream_start (stream_id);
        gst_event_set_group_id (event, gst_util_group_id_next ());
        gst_pad_push_event (aiff->srcpad, event);
        g_free (stream_id);

        GST_DEBUG_OBJECT (aiff, "creating the caps");

        caps = gst_aiff_parse_create_caps (aiff);
        if (caps == NULL)
          goto unknown_format;

        gst_pad_push_event (aiff->srcpad, gst_event_new_caps (caps));
        gst_caps_unref (caps);

        aiff->bytes_per_sample = aiff->channels * aiff->width / 8;
        aiff->bps = aiff->bytes_per_sample * aiff->rate;

        if (!aiff->tags)
          aiff->tags = gst_tag_list_new_empty ();

        {
          GstCaps *templ_caps = gst_pad_get_pad_template_caps (aiff->sinkpad);
          gst_pb_utils_add_codec_description_to_tag_list (aiff->tags,
              GST_TAG_CONTAINER_FORMAT, templ_caps);
          gst_caps_unref (templ_caps);
        }

        if (aiff->bps) {
          guint bitrate = aiff->bps * 8;

          GST_DEBUG_OBJECT (aiff, "adding bitrate of %u bps to tag list",
              bitrate);

          /* At the moment, aiffparse only supports uncompressed PCM data.
           * Therefore, nominal, actual, minimum, maximum bitrate are the same.
           * XXX: If AIFF-C support is extended to include compression,
           * make sure that aiff->bps is set properly. */
          gst_tag_list_add (aiff->tags, GST_TAG_MERGE_REPLACE,
              GST_TAG_BITRATE, bitrate, GST_TAG_NOMINAL_BITRATE, bitrate,
              GST_TAG_MINIMUM_BITRATE, bitrate, GST_TAG_MAXIMUM_BITRATE,
              bitrate, NULL);
        }

        if (aiff->bytes_per_sample <= 0)
          goto no_bytes_per_sample;

        aiff->got_comm = TRUE;
        break;
      }
      case GST_MAKE_FOURCC ('S', 'S', 'N', 'D'):{
        guint32 datasize;

        GST_DEBUG_OBJECT (aiff, "Got 'SSND' TAG, size : %d", size);

        /* Now, read the 8-byte header in the SSND chunk */
        if (aiff->streaming) {
          const guint8 *ssnddata = NULL;

          if (!gst_aiff_parse_peek_data (aiff, 16, &ssnddata))
            return GST_FLOW_OK;

          aiff->ssnd_offset = GST_READ_UINT32_BE (ssnddata + 8);
          aiff->ssnd_blocksize = GST_READ_UINT32_BE (ssnddata + 12);
          gst_adapter_unmap (aiff->adapter);
          gst_adapter_flush (aiff->adapter, 16);
        } else {
          GstBuffer *ssndbuf = NULL;
          GstMapInfo info;

          if ((res =
                  gst_pad_pull_range (aiff->sinkpad, aiff->offset, 16,
                      &ssndbuf)) != GST_FLOW_OK)
            goto header_read_error;

          gst_buffer_map (ssndbuf, &info, GST_MAP_READ);
          aiff->ssnd_offset = GST_READ_UINT32_BE (info.data + 8);
          aiff->ssnd_blocksize = GST_READ_UINT32_BE (info.data + 12);
          gst_buffer_unmap (ssndbuf, &info);
          gst_buffer_unref (ssndbuf);
        }

        gotdata = TRUE;

        /* 8 byte chunk header, 8 byte SSND header */
        aiff->offset += 16;
        datasize = size - 8;

        aiff->datastart = aiff->offset + aiff->ssnd_offset;
        /* file might be truncated */
        if (upstream_size) {
          size = MIN (datasize, (upstream_size - aiff->datastart));
        }
        aiff->datasize = (guint64) datasize;
        aiff->dataleft = (guint64) datasize;
        aiff->end_offset = datasize + aiff->datastart;
        if (!aiff->streaming) {
          /* We will continue looking at chunks until the end - to read tags,
           * etc. */
          aiff->offset += datasize;
        }
        GST_DEBUG_OBJECT (aiff, "datasize = %d", datasize);
        if (aiff->streaming) {
          done = TRUE;
        }
        break;
      }
      case GST_MAKE_FOURCC ('I', 'D', '3', ' '):{
        GstTagList *tags;

        if (aiff->streaming) {
          if (!gst_aiff_parse_peek_chunk (aiff, &tag, &size))
            return GST_FLOW_OK;

          gst_adapter_flush (aiff->adapter, 8);
          aiff->offset += 8;

          buf = gst_adapter_take_buffer (aiff->adapter, size);
        } else {
          if ((res = gst_aiff_parse_read_chunk (aiff,
                      &aiff->offset, &tag, &buf)) != GST_FLOW_OK)
            return res;
        }

        GST_LOG_OBJECT (aiff, "ID3 chunk of size %" G_GSIZE_FORMAT,
            gst_buffer_get_size (buf));

        tags = gst_tag_list_from_id3v2_tag (buf);
        gst_buffer_unref (buf);

        GST_INFO_OBJECT (aiff, "ID3 tags: %" GST_PTR_FORMAT, tags);

        if (aiff->tags == NULL) {
          aiff->tags = tags;
        } else {
          gst_tag_list_insert (aiff->tags, tags, GST_TAG_MERGE_APPEND);
          gst_tag_list_unref (tags);
        }
        break;
      }
      case GST_MAKE_FOURCC ('C', 'H', 'A', 'N'):{
        GST_FIXME_OBJECT (aiff, "Handle CHAN chunk with channel layouts");
        if (!gst_aiff_parse_ignore_chunk (aiff, tag, size)) {
          return GST_FLOW_OK;
        }
        break;
      }
      default:
        if (!gst_aiff_parse_ignore_chunk (aiff, tag, size)) {
          return GST_FLOW_OK;
        }
    }

    buf = NULL;

    if (upstream_size && (aiff->offset >= upstream_size)) {
      /* Now we have gone through the whole file */
      done = TRUE;
    }
  }

  /* We read all the chunks (in pull mode) or reached the SSND chunk
   * (in push mode). We must have both COMM and SSND now; error out 
   * otherwise.
   */
  if (!aiff->got_comm) {
    GST_WARNING_OBJECT (aiff, "Failed to find COMM chunk");
    goto no_header;
  }
  if (!gotdata) {
    GST_WARNING_OBJECT (aiff, "Failed to find SSND chunk");
    goto no_data;
  }

  GST_DEBUG_OBJECT (aiff, "Finished parsing headers");

  if (gst_aiff_parse_calculate_duration (aiff)) {
    gst_segment_init (&aiff->segment, GST_FORMAT_TIME);
    aiff->segment.duration = aiff->duration;
  } else {
    /* no bitrate, let downstream peer do the math, we'll feed it bytes. */
    gst_segment_init (&aiff->segment, GST_FORMAT_BYTES);
    aiff->segment.duration = aiff->datasize;
  }

  /* now we have all the info to perform a pending seek if any, if no
   * event, this will still do the right thing and it will also send
   * the right segment event downstream. */
  gst_aiff_parse_perform_seek (aiff, aiff->seek_event, TRUE);
  /* remove pending event */
  event_p = &aiff->seek_event;
  gst_event_replace (event_p, NULL);

  /* we just started, we are discont */
  aiff->discont = TRUE;

  aiff->state = AIFF_PARSE_DATA;

  /* determine reasonable max buffer size,
   * that is, buffers not too small either size or time wise
   * so we do not end up with too many of them */
  /* var abuse */
  upstream_size = 0;
  gst_aiff_parse_time_to_bytepos (aiff, 40 * GST_MSECOND, &upstream_size);
  aiff->max_buf_size = upstream_size;
  aiff->max_buf_size = MAX (aiff->max_buf_size, MAX_BUFFER_SIZE);
  if (aiff->bytes_per_sample > 0)
    aiff->max_buf_size -= (aiff->max_buf_size % aiff->bytes_per_sample);

  GST_DEBUG_OBJECT (aiff, "max buffer size %u", aiff->max_buf_size);

  return GST_FLOW_OK;

  /* ERROR */
no_header:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, TYPE_NOT_FOUND, (NULL),
        ("Invalid AIFF header (no COMM found)"));
    return GST_FLOW_ERROR;
  }
no_data:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, TYPE_NOT_FOUND, (NULL),
        ("Invalid AIFF: no SSND found"));
    return GST_FLOW_ERROR;
  }
parse_header_error:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, DEMUX, (NULL),
        ("Couldn't parse audio header"));
    return GST_FLOW_ERROR;
  }
no_channels:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, FAILED, (NULL),
        ("Stream claims to contain no channels - invalid data"));
    return GST_FLOW_ERROR;
  }
no_rate:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, FAILED, (NULL),
        ("Stream with sample_rate == 0 - invalid data"));
    return GST_FLOW_ERROR;
  }
no_bytes_per_sample:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, FAILED, (NULL),
        ("Could not calculate bytes per sample - invalid data"));
    return GST_FLOW_ERROR;
  }
unknown_format:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, TYPE_NOT_FOUND, (NULL),
        ("No caps found for format 0x%x, %d channels, %d Hz",
            aiff->format, aiff->channels, aiff->rate));
    return GST_FLOW_ERROR;
  }
header_read_error:
  {
    GST_ELEMENT_ERROR (aiff, STREAM, DEMUX, (NULL),
        ("Couldn't read in header"));
    return GST_FLOW_ERROR;
  }
}

/*
 * Read AIFF file tag when streaming
 */
static GstFlowReturn
gst_aiff_parse_parse_stream_init (GstAiffParse * aiff)
{
  if (gst_adapter_available (aiff->adapter) >= 12) {
    GstBuffer *tmp;

    /* _take flushes the data */
    tmp = gst_adapter_take_buffer (aiff->adapter, 12);

    GST_DEBUG_OBJECT (aiff, "Parsing aiff header");
    if (!gst_aiff_parse_parse_file_header (aiff, tmp))
      return GST_FLOW_ERROR;

    aiff->offset += 12;
    /* Go to next state */
    aiff->state = AIFF_PARSE_HEADER;
  }
  return GST_FLOW_OK;
}

/* handle an event sent directly to the element.
 *
 * This event can be sent either in the READY state or the
 * >READY state. The only event of interest really is the seek
 * event.
 *
 * In the READY state we can only store the event and try to
 * respect it when going to PAUSED. We assume we are in the
 * READY state when our parsing state != AIFF_PARSE_DATA.
 *
 * When we are steaming, we can simply perform the seek right
 * away.
 */
static gboolean
gst_aiff_parse_send_event (GstElement * element, GstEvent * event)
{
  GstAiffParse *aiff = GST_AIFF_PARSE (element);
  gboolean res = FALSE;
  GstEvent **event_p;

  GST_DEBUG_OBJECT (aiff, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (aiff->state == AIFF_PARSE_DATA) {
        /* we can handle the seek directly when streaming data */
        res = gst_aiff_parse_perform_seek (aiff, event, FALSE);
      } else {
        GST_DEBUG_OBJECT (aiff, "queuing seek for later");

        event_p = &aiff->seek_event;
        gst_event_replace (event_p, event);

        /* we always return true */
        res = TRUE;
      }
      break;
    default:
      break;
  }
  gst_event_unref (event);
  return res;
}

static GstFlowReturn
gst_aiff_parse_stream_data (GstAiffParse * aiff)
{
  GstBuffer *buf = NULL;
  GstFlowReturn res = GST_FLOW_OK;
  guint64 desired, obtained;
  GstClockTime timestamp, next_timestamp, duration;
  guint64 pos, nextpos;

  if (aiff->bytes_per_sample <= 0) {
    GST_ELEMENT_ERROR (aiff, STREAM, WRONG_TYPE, (NULL),
        ("File is not a valid AIFF file (invalid bytes per sample)"));
    return GST_FLOW_ERROR;
  }

iterate_adapter:
  GST_LOG_OBJECT (aiff,
      "offset: %" G_GINT64_FORMAT " , end: %" G_GINT64_FORMAT " , dataleft: %"
      G_GINT64_FORMAT, aiff->offset, aiff->end_offset, aiff->dataleft);

  /* Get the next n bytes and output them */
  if (aiff->dataleft == 0 || aiff->dataleft < aiff->bytes_per_sample)
    goto found_eos;

  /* scale the amount of data by the segment rate so we get equal
   * amounts of data regardless of the playback rate */
  desired =
      MIN (gst_guint64_to_gdouble (aiff->dataleft),
      aiff->max_buf_size * ABS (aiff->segment.rate));

  if (desired >= aiff->bytes_per_sample)
    desired -= (desired % aiff->bytes_per_sample);

  GST_LOG_OBJECT (aiff, "Fetching %" G_GINT64_FORMAT " bytes of data "
      "from the sinkpad", desired);

  if (aiff->streaming) {
    guint avail = gst_adapter_available (aiff->adapter);

    if (avail < desired) {
      GST_LOG_OBJECT (aiff, "Got only %d bytes of data from the sinkpad",
          avail);
      return GST_FLOW_OK;
    }

    buf = gst_adapter_take_buffer (aiff->adapter, desired);
  } else {
    if ((res = gst_pad_pull_range (aiff->sinkpad, aiff->offset,
                desired, &buf)) != GST_FLOW_OK)
      goto pull_error;
  }

  /* If we have a pending close/start segment, send it now. */
  if (G_UNLIKELY (aiff->close_segment != NULL)) {
    gst_pad_push_event (aiff->srcpad, aiff->close_segment);
    aiff->close_segment = NULL;
  }
  if (G_UNLIKELY (aiff->start_segment != NULL)) {
    gst_pad_push_event (aiff->srcpad, aiff->start_segment);
    aiff->start_segment = NULL;
  }
  if (G_UNLIKELY (aiff->tags != NULL)) {
    gst_pad_push_event (aiff->srcpad, gst_event_new_tag (aiff->tags));
    aiff->tags = NULL;
  }

  obtained = gst_buffer_get_size (buf);

  /* our positions in bytes */
  pos = aiff->offset - aiff->datastart;
  nextpos = pos + obtained;

  /* update offsets, does not overflow. */
  GST_BUFFER_OFFSET (buf) = pos / aiff->bytes_per_sample;
  GST_BUFFER_OFFSET_END (buf) = nextpos / aiff->bytes_per_sample;

  if (aiff->bps > 0) {
    /* and timestamps if we have a bitrate, be careful for overflows */
    timestamp =
        gst_util_uint64_scale_ceil (pos, GST_SECOND, (guint64) aiff->bps);
    next_timestamp =
        gst_util_uint64_scale_ceil (nextpos, GST_SECOND, (guint64) aiff->bps);
    duration = next_timestamp - timestamp;

    /* update current running segment position */
    aiff->segment.position = next_timestamp;
  } else {
    /* no bitrate, all we know is that the first sample has timestamp 0, all
     * other positions and durations have unknown timestamp. */
    if (pos == 0)
      timestamp = 0;
    else
      timestamp = GST_CLOCK_TIME_NONE;
    duration = GST_CLOCK_TIME_NONE;
    /* update current running segment position with byte offset */
    aiff->segment.position = nextpos;
  }
  if (aiff->discont) {
    GST_DEBUG_OBJECT (aiff, "marking DISCONT");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    aiff->discont = FALSE;
  }

  GST_BUFFER_TIMESTAMP (buf) = timestamp;
  GST_BUFFER_DURATION (buf) = duration;

  GST_LOG_OBJECT (aiff,
      "Got buffer. timestamp:%" GST_TIME_FORMAT " , duration:%" GST_TIME_FORMAT
      ", size:%" G_GUINT64_FORMAT, GST_TIME_ARGS (timestamp),
      GST_TIME_ARGS (duration), obtained);

  if ((res = gst_pad_push (aiff->srcpad, buf)) != GST_FLOW_OK)
    goto push_error;

  if (obtained < aiff->dataleft) {
    aiff->offset += obtained;
    aiff->dataleft -= obtained;
  } else {
    aiff->offset += aiff->dataleft;
    aiff->dataleft = 0;
  }

  /* Iterate until need more data, so adapter size won't grow */
  if (aiff->streaming) {
    GST_LOG_OBJECT (aiff,
        "offset: %" G_GINT64_FORMAT " , end: %" G_GINT64_FORMAT, aiff->offset,
        aiff->end_offset);
    goto iterate_adapter;
  }
  return res;

  /* ERROR */
found_eos:
  {
    GST_DEBUG_OBJECT (aiff, "found EOS");
    return GST_FLOW_EOS;
  }
pull_error:
  {
    /* check if we got EOS */
    if (res == GST_FLOW_EOS)
      goto found_eos;

    GST_WARNING_OBJECT (aiff,
        "Error getting %" G_GINT64_FORMAT " bytes from the "
        "sinkpad (dataleft = %" G_GINT64_FORMAT ")", desired, aiff->dataleft);
    return res;
  }
push_error:
  {
    GST_INFO_OBJECT (aiff,
        "Error pushing on srcpad %s:%s, reason %s, is linked? = %d",
        GST_DEBUG_PAD_NAME (aiff->srcpad), gst_flow_get_name (res),
        gst_pad_is_linked (aiff->srcpad));
    return res;
  }
}

static void
gst_aiff_parse_loop (GstPad * pad)
{
  GstFlowReturn ret;
  GstAiffParse *aiff = GST_AIFF_PARSE (GST_PAD_PARENT (pad));

  GST_LOG_OBJECT (aiff, "process data");

  switch (aiff->state) {
    case AIFF_PARSE_START:
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_START");
      if ((ret = gst_aiff_parse_stream_init (aiff)) != GST_FLOW_OK)
        goto pause;

      aiff->state = AIFF_PARSE_HEADER;
      /* fall-through */

    case AIFF_PARSE_HEADER:
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_HEADER");
      if ((ret = gst_aiff_parse_stream_headers (aiff)) != GST_FLOW_OK)
        goto pause;

      aiff->state = AIFF_PARSE_DATA;
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_DATA");
      /* fall-through */

    case AIFF_PARSE_DATA:
      if ((ret = gst_aiff_parse_stream_data (aiff)) != GST_FLOW_OK)
        goto pause;
      break;
    default:
      g_assert_not_reached ();
  }
  return;

  /* ERRORS */
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_DEBUG_OBJECT (aiff, "pausing task, reason %s", reason);
    aiff->segment_running = FALSE;
    gst_pad_pause_task (pad);

    if (ret == GST_FLOW_EOS) {
      /* perform EOS logic */
      if (aiff->segment.flags & GST_SEEK_FLAG_SEGMENT) {
        GstClockTime stop;

        if ((stop = aiff->segment.stop) == -1)
          stop = aiff->segment.duration;

        gst_element_post_message (GST_ELEMENT_CAST (aiff),
            gst_message_new_segment_done (GST_OBJECT_CAST (aiff),
                aiff->segment.format, stop));
        gst_pad_push_event (aiff->srcpad,
            gst_event_new_segment_done (aiff->segment.format, stop));
      } else {
        gst_pad_push_event (aiff->srcpad, gst_event_new_eos ());
      }
    } else if (ret < GST_FLOW_EOS || ret == GST_FLOW_NOT_LINKED) {
      /* for fatal errors we post an error message, post the error
       * first so the app knows about the error first. */
      GST_ELEMENT_FLOW_ERROR (aiff, ret);
      gst_pad_push_event (aiff->srcpad, gst_event_new_eos ());
    }
    return;
  }
}

static GstFlowReturn
gst_aiff_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstAiffParse *aiff = GST_AIFF_PARSE (parent);

  GST_LOG_OBJECT (aiff, "adapter_push %" G_GSIZE_FORMAT " bytes",
      gst_buffer_get_size (buf));

  gst_adapter_push (aiff->adapter, buf);

  switch (aiff->state) {
    case AIFF_PARSE_START:
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_START");
      if ((ret = gst_aiff_parse_parse_stream_init (aiff)) != GST_FLOW_OK)
        goto done;

      if (aiff->state != AIFF_PARSE_HEADER)
        break;

      /* otherwise fall-through */
    case AIFF_PARSE_HEADER:
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_HEADER");
      if ((ret = gst_aiff_parse_stream_headers (aiff)) != GST_FLOW_OK)
        goto done;

      if (!aiff->got_comm || aiff->datastart == 0)
        break;

      aiff->state = AIFF_PARSE_DATA;
      GST_INFO_OBJECT (aiff, "AIFF_PARSE_DATA");

      /* fall-through */
    case AIFF_PARSE_DATA:
      if ((ret = gst_aiff_parse_stream_data (aiff)) != GST_FLOW_OK)
        goto done;
      break;
    default:
      g_return_val_if_reached (GST_FLOW_ERROR);
  }
done:
  return ret;
}

static gboolean
gst_aiff_parse_pad_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  GstAiffParse *aiffparse;
  gboolean res = TRUE;

  aiffparse = GST_AIFF_PARSE (GST_PAD_PARENT (pad));

  if (*dest_format == src_format) {
    *dest_value = src_value;
    return TRUE;
  }

  if (aiffparse->bytes_per_sample <= 0)
    return FALSE;

  GST_INFO_OBJECT (aiffparse, "converting value from %s to %s",
      gst_format_get_name (src_format), gst_format_get_name (*dest_format));

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_value = src_value / aiffparse->bytes_per_sample;
          break;
        case GST_FORMAT_TIME:
          if (aiffparse->bps > 0) {
            *dest_value = gst_util_uint64_scale_ceil (src_value, GST_SECOND,
                (guint64) aiffparse->bps);
            break;
          }
          /* Else fallthrough */
        default:
          res = FALSE;
          goto done;
      }
      break;

    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * aiffparse->bytes_per_sample;
          break;
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale (src_value, GST_SECOND,
              (guint64) aiffparse->rate);
          break;
        default:
          res = FALSE;
          goto done;
      }
      break;

    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          if (aiffparse->bps > 0) {
            *dest_value = gst_util_uint64_scale (src_value,
                (guint64) aiffparse->bps, GST_SECOND);
            break;
          }
          /* Else fallthrough */
          break;
        case GST_FORMAT_DEFAULT:
          *dest_value = gst_util_uint64_scale (src_value,
              (guint64) aiffparse->rate, GST_SECOND);
          break;
        default:
          res = FALSE;
          goto done;
      }
      break;

    default:
      res = FALSE;
      goto done;
  }

done:
  return res;

}

/* handle queries for location and length in requested format */
static gboolean
gst_aiff_parse_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = FALSE;
  GstAiffParse *aiff = GST_AIFF_PARSE (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      gint64 duration = 0;
      GstFormat format;

      /* only if we know */
      if (aiff->state != AIFF_PARSE_DATA)
        break;

      gst_query_parse_duration (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_TIME:{
          if ((res = gst_aiff_parse_calculate_duration (aiff))) {
            duration = aiff->duration;
          }
          break;
        }
        default:
          format = GST_FORMAT_BYTES;
          duration = aiff->datasize;
          break;
      }
      gst_query_set_duration (query, format, duration);
      break;
    }
    case GST_QUERY_CONVERT:
    {
      gint64 srcvalue, dstvalue;
      GstFormat srcformat, dstformat;

      /* only if we know */
      if (aiff->state != AIFF_PARSE_DATA)
        break;

      gst_query_parse_convert (query, &srcformat, &srcvalue,
          &dstformat, &dstvalue);
      res = gst_aiff_parse_pad_convert (pad, srcformat, srcvalue,
          &dstformat, &dstvalue);
      if (res)
        gst_query_set_convert (query, srcformat, srcvalue, dstformat, dstvalue);
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      /* only if we know */
      if (aiff->state != AIFF_PARSE_DATA)
        break;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gboolean seekable = TRUE;

        if (!gst_aiff_parse_calculate_duration (aiff)) {
          seekable = FALSE;
        }
        gst_query_set_seeking (query, GST_FORMAT_TIME, seekable,
            0, aiff->duration);
        res = TRUE;
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
  return res;
}

static gboolean
gst_aiff_parse_srcpad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAiffParse *aiffparse = GST_AIFF_PARSE (parent);
  gboolean res = FALSE;

  GST_DEBUG_OBJECT (aiffparse, "%s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* can only handle events when we are in the data state */
      if (aiffparse->state == AIFF_PARSE_DATA) {
        res = gst_aiff_parse_perform_seek (aiffparse, event, FALSE);
      }
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_push_event (aiffparse->sinkpad, event);
      break;
  }
  return res;
}

static gboolean
gst_aiff_parse_sink_activate (GstPad * sinkpad, GstObject * parent)
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

  GST_DEBUG_OBJECT (sinkpad, "going to pull mode");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  {
    GST_DEBUG_OBJECT (sinkpad, "going to push (streaming) mode");
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}


static gboolean
gst_aiff_parse_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;
  GstAiffParse *aiff = GST_AIFF_PARSE (parent);

  if (aiff->adapter) {
    g_object_unref (aiff->adapter);
    aiff->adapter = NULL;
  }

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      if (active) {
        aiff->streaming = TRUE;
        aiff->adapter = gst_adapter_new ();
      }
      res = TRUE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        aiff->streaming = FALSE;
        aiff->adapter = NULL;
        aiff->segment_running = TRUE;
        res =
            gst_pad_start_task (sinkpad, (GstTaskFunction) gst_aiff_parse_loop,
            sinkpad, NULL);
      } else {
        aiff->segment_running = FALSE;
        res = gst_pad_stop_task (sinkpad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
};

static GstFlowReturn
gst_aiff_parse_flush_data (GstAiffParse * aiff)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint av;

  if ((av = gst_adapter_available (aiff->adapter)) > 0) {
    aiff->dataleft = av;
    aiff->end_offset = aiff->offset + av;
    ret = gst_aiff_parse_stream_data (aiff);
  }

  return ret;
}


static gboolean
gst_aiff_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAiffParse *aiff = GST_AIFF_PARSE (parent);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (aiff, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      /* discard, we'll come up with proper src caps */
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      gint64 start, stop, offset = 0, end_offset = -1;
      GstSegment segment;

      /* some debug output */
      gst_event_copy_segment (event, &segment);
      GST_DEBUG_OBJECT (aiff, "received segment %" GST_SEGMENT_FORMAT,
          &segment);

      /* now we are either committed to TIME or BYTE format,
       * and we only expect a BYTE segment, e.g. following a seek */
      if (segment.format == GST_FORMAT_BYTES) {
        /* handle (un)signed issues */
        start = segment.start;
        stop = segment.stop;
        if (start > 0) {
          offset = start;
          start -= aiff->datastart;
          start = MAX (start, 0);
        }
        if (stop > 0) {
          end_offset = stop;
          stop -= aiff->datastart;
          stop = MAX (stop, 0);
        }
        if (aiff->state == AIFF_PARSE_DATA &&
            aiff->segment.format == GST_FORMAT_TIME) {
          /* operating in format TIME, so we can convert */
          if (aiff->bps) {
            if (start >= 0)
              start =
                  gst_util_uint64_scale_ceil (start, GST_SECOND,
                  (guint64) aiff->bps);
            if (stop >= 0)
              stop =
                  gst_util_uint64_scale_ceil (stop, GST_SECOND,
                  (guint64) aiff->bps);
          } else {
            GST_DEBUG_OBJECT (aiff, "unable to compute segment start/stop");
            goto exit;
          }
        }
      } else {
        GST_DEBUG_OBJECT (aiff, "unsupported segment format, ignoring");
        goto exit;
      }

      segment.start = start;
      segment.stop = stop;

      /* accept upstream's notion of segment and distribute along */
      if (aiff->state == AIFF_PARSE_DATA) {
        segment.format = aiff->segment.format;
        segment.time = segment.position = segment.start;
        segment.duration = aiff->segment.duration;
      }

      gst_segment_copy_into (&segment, &aiff->segment);

      if (aiff->start_segment)
        gst_event_unref (aiff->start_segment);

      aiff->start_segment = gst_event_new_segment (&segment);

      /* If the seek is within the same SSND chunk and there is no new
       * end_offset defined keep the previous end_offset. This will avoid noise
       * at the end of playback if e.g. a metadata chunk is located at the end
       * of the file. */
      if (aiff->end_offset > 0 && offset < aiff->end_offset &&
          offset >= aiff->datastart && end_offset == -1) {
        end_offset = aiff->end_offset;
      }

      /* stream leftover data in current segment */
      if (aiff->state == AIFF_PARSE_DATA)
        gst_aiff_parse_flush_data (aiff);
      /* and set up streaming thread for next one */
      aiff->offset = offset;
      aiff->end_offset = end_offset;
      if (aiff->end_offset > 0) {
        aiff->dataleft = aiff->end_offset - aiff->offset;
      } else {
        /* infinity; upstream will EOS when done */
        aiff->dataleft = G_MAXUINT64;
      }
    exit:
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_FLUSH_START:
      ret = gst_pad_push_event (aiff->srcpad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      ret = gst_pad_push_event (aiff->srcpad, event);
      gst_adapter_clear (aiff->adapter);
      break;
    default:
      ret = gst_pad_event_default (aiff->sinkpad, parent, event);
      break;
  }

  return ret;
}

static GstStateChangeReturn
gst_aiff_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstAiffParse *aiff = GST_AIFF_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_aiff_parse_reset (aiff);
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
      gst_aiff_parse_reset (aiff);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}
