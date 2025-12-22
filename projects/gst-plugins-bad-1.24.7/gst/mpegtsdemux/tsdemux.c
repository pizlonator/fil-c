/*
 * tsdemux.c
 * Copyright (C) 2009 Zaheer Abbas Merali
 *               2010 Edward Hervey
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *  Author: Edward Hervey <bilboed@bilboed.com>, Collabora Ltd.
 *
 * Authors:
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
 *   Edward Hervey <edward.hervey@collabora.co.uk>
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/pbutils.h>
#include <gst/base/base.h>
#include <gst/audio/audio.h>

#include "mpegtsbase.h"
#include "tsdemux.h"
#include "gstmpegdesc.h"
#include "gstmpegdefs.h"
#include "mpegtspacketizer.h"
#include "pesparse.h"
#include <gst/codecparsers/gsth264parser.h>
#include <gst/codecparsers/gstmpegvideoparser.h>
#include <gst/video/video-color.h>

#include <math.h>

/*
 * tsdemux
 *
 * See TODO for explanations on improvements needed
 */

#define CONTINUITY_UNSET 255
#define MAX_CONTINUITY 15

/* Length of metadata_AU_cell header, see ISO/IEC 13818-1:2018 Section 2.12.4 */
#define PES_PACKET_METADATA_AU_HEADER_LEN 5

/* Seeking/Scanning related variables */

/* seek to SEEK_TIMESTAMP_OFFSET before the desired offset and search then
 * either accurately or for the next timestamp
 */
#define SEEK_TIMESTAMP_OFFSET (2500 * GST_MSECOND)

#define GST_FLOW_REWINDING GST_FLOW_CUSTOM_ERROR

/* latency in msecs */
#define DEFAULT_LATENCY (700)

/* Limit PES packet collection to a maximum of 32MB
 * which is more than large enough to support an H264 frame at
 * maximum profile/level/bitrate at 30fps or above.
 * PES bigger than this limit will be output in buffers of
 * up to this size */
#define MAX_PES_PAYLOAD (32 * 1024 * 1024)

GST_DEBUG_CATEGORY_STATIC (ts_demux_debug);
#define GST_CAT_DEFAULT ts_demux_debug

#define ABSDIFF(a,b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))

static GQuark QUARK_TSDEMUX;
static GQuark QUARK_PID;
static GQuark QUARK_PCR;
static GQuark QUARK_OPCR;
static GQuark QUARK_PTS;
static GQuark QUARK_DTS;
static GQuark QUARK_OFFSET;

typedef enum
{
  PENDING_PACKET_EMPTY = 0,     /* No pending packet/buffer
                                 * Push incoming buffers to the array */
  PENDING_PACKET_HEADER,        /* PES header needs to be parsed
                                 * Push incoming buffers to the array */
  PENDING_PACKET_BUFFER,        /* Currently filling up output buffer
                                 * Push incoming buffers to the bufferlist */
  PENDING_PACKET_DISCONT        /* Discontinuity in incoming packets
                                 * Drop all incoming buffers */
} PendingPacketState;

/* Pending buffer */
typedef struct
{
  /* The fully reconstructed buffer */
  GstBuffer *buffer;

  /* Raw PTS/DTS (in 90kHz units) */
  guint64 pts, dts;
} PendingBuffer;

typedef struct _TSDemuxStream TSDemuxStream;

typedef struct _TSDemuxH264ParsingInfos TSDemuxH264ParsingInfos;
typedef struct _TSDemuxJP2KParsingInfos TSDemuxJP2KParsingInfos;
typedef struct _TSDemuxADTSParsingInfos TSDemuxADTSParsingInfos;

/* Returns TRUE if a keyframe was found */
typedef gboolean (*GstTsDemuxKeyFrameScanFunction) (TSDemuxStream * stream,
    guint8 * data, const gsize data_size, const gsize max_frame_offset);

typedef struct
{
  guint8 *data;
  gsize size;
} SimpleBuffer;

struct _TSDemuxH264ParsingInfos
{
  /* H264 parsing data */
  GstH264NalParser *parser;
  GstByteWriter *sps;
  GstByteWriter *pps;
  GstByteWriter *sei;
  SimpleBuffer framedata;
};

struct _TSDemuxJP2KParsingInfos
{
  /* J2K parsing data */
  gboolean interlace;
};

struct _TSDemuxADTSParsingInfos
{
  guint mpegversion;
};

struct _TSDemuxStream
{
  MpegTSBaseStream stream;

  GstPad *pad;

  /* Whether the pad was added or not */
  gboolean active;

  /* Whether this is a sparse stream (subtitles or metadata) */
  gboolean sparse;

  /* TRUE if we are waiting for a valid timestamp */
  gboolean pending_ts;

  /* Output data */
  PendingPacketState state;

  /* PES header being reconstructed (optional, allocated) */
  guint8 *pending_header_data;
  guint pending_header_size;

  /* Data being reconstructed (allocated) */
  guint8 *data;

  /* Size of data being reconstructed (if known, else 0) */
  guint expected_size;

  /* Amount of bytes in current ->data */
  guint current_size;
  /* Size of ->data */
  guint allocated_size;

  /* Current PTS/DTS for this stream (in running time) */
  GstClockTime pts;
  GstClockTime dts;

  /* Reference PTS used to detect gaps */
  GstClockTime gap_ref_pts;
  /* Number of outputted buffers */
  guint32 nb_out_buffers;
  /* Reference number of buffers for gaps */
  guint32 gap_ref_buffers;

  /* Current PTS/DTS for this stream (in 90kHz unit) */
  guint64 raw_pts, raw_dts;

  /* Whether this stream needs to send a newsegment */
  gboolean need_newsegment;

  /* Whether the next output buffer should be DISCONT */
  gboolean discont;

  /* The value to use when calculating the newsegment */
  GstClockTime first_pts;

  GstTagList *taglist;

  gint continuity_counter;

  /* List of pending buffers */
  GList *pending;

  /* if != 0, output only PES from that substream */
  guint8 target_pes_substream;
  gboolean needs_keyframe;

  GstClockTime seeked_pts, seeked_dts;

  GstTsDemuxKeyFrameScanFunction scan_function;
  TSDemuxH264ParsingInfos h264infos;
  TSDemuxJP2KParsingInfos jp2kInfos;
  TSDemuxADTSParsingInfos atdsInfos;
};

#define VIDEO_CAPS \
  GST_STATIC_CAPS (\
    "video/mpeg, " \
      "mpegversion = (int) { 1, 2, 4 }, " \
      "systemstream = (boolean) FALSE; " \
    "video/x-h264,stream-format=(string)byte-stream;" \
    "video/x-h265,stream-format=(string)byte-stream;" \
    "video/x-dirac;" \
    "video/x-cavs;" \
    "video/x-wmv," \
      "wmvversion = (int) 3, " \
      "format = (string) WVC1;" \
      "image/x-jpc;" \
)

#define AUDIO_CAPS \
  GST_STATIC_CAPS ( \
    "audio/mpeg, " \
      "mpegversion = (int) 1;" \
    "audio/mpeg, " \
      "mpegversion = (int) { 2, 4 }, " \
      "stream-format = (string) adts; " \
    "audio/mpeg, " \
      "mpegversion = (int) 4, " \
      "stream-format = (string) loas; " \
    "audio/x-lpcm, " \
      "width = (int) { 16, 20, 24 }, " \
      "rate = (int) { 48000, 96000 }, " \
      "channels = (int) [ 1, 8 ], " \
      "dynamic_range = (int) [ 0, 255 ], " \
      "emphasis = (boolean) { FALSE, TRUE }, " \
      "mute = (boolean) { FALSE, TRUE }; " \
    "audio/x-ac3; audio/x-eac3;" \
    "audio/x-ac4;" \
    "audio/x-dts;" \
    "audio/x-opus;" \
    "audio/x-private-ts-lpcm" \
  )

/* Can also use the subpicture pads for text subtitles? */
#define SUBPICTURE_CAPS \
    GST_STATIC_CAPS ("subpicture/x-pgs; subpicture/x-dvd; subpicture/x-dvb")

static GstStaticPadTemplate video_template =
GST_STATIC_PAD_TEMPLATE ("video_%01x_%05x", GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    VIDEO_CAPS);

static GstStaticPadTemplate audio_template =
GST_STATIC_PAD_TEMPLATE ("audio_%01x_%05x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    AUDIO_CAPS);

static GstStaticPadTemplate subpicture_template =
GST_STATIC_PAD_TEMPLATE ("subpicture_%01x_%05x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    SUBPICTURE_CAPS);

static GstStaticPadTemplate private_template =
GST_STATIC_PAD_TEMPLATE ("private_%01x_%05x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_PROGRAM_NUMBER,
  PROP_EMIT_STATS,
  PROP_LATENCY,
  PROP_SEND_SCTE35_EVENTS,
  /* FILL ME */
};

/* Pad functions */


/* mpegtsbase methods */
static void
gst_ts_demux_update_program (MpegTSBase * base, MpegTSBaseProgram * program);
static void
gst_ts_demux_program_started (MpegTSBase * base, MpegTSBaseProgram * program);
static void
gst_ts_demux_program_stopped (MpegTSBase * base, MpegTSBaseProgram * program);
static gboolean
gst_ts_demux_can_remove_program (MpegTSBase * base,
    MpegTSBaseProgram * program);
static void gst_ts_demux_reset (MpegTSBase * base);
static GstFlowReturn
gst_ts_demux_push (MpegTSBase * base, MpegTSPacketizerPacket * packet,
    GstMpegtsSection * section);
static void gst_ts_demux_flush (MpegTSBase * base, gboolean hard);
static GstFlowReturn gst_ts_demux_drain (MpegTSBase * base);
static gboolean
gst_ts_demux_stream_added (MpegTSBase * base, MpegTSBaseStream * stream,
    MpegTSBaseProgram * program);
static void
gst_ts_demux_stream_removed (MpegTSBase * base, MpegTSBaseStream * stream);
static GstFlowReturn gst_ts_demux_do_seek (MpegTSBase * base, GstEvent * event);
static void gst_ts_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ts_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_ts_demux_flush_streams (GstTSDemux * tsdemux, gboolean hard);
static GstFlowReturn
gst_ts_demux_push_pending_data (GstTSDemux * demux, TSDemuxStream * stream,
    MpegTSBaseProgram * program);
static void gst_ts_demux_stream_flush (TSDemuxStream * stream,
    GstTSDemux * demux, gboolean hard);

static gboolean push_event (MpegTSBase * base, GstEvent * event);
static gboolean sink_query (MpegTSBase * base, GstQuery * query);
static void gst_ts_demux_check_and_sync_streams (GstTSDemux * demux,
    GstClockTime time);
static void handle_psi (MpegTSBase * base, GstMpegtsSection * section);

static void
_extra_init (void)
{
  QUARK_TSDEMUX = g_quark_from_string ("tsdemux");
  QUARK_PID = g_quark_from_string ("pid");
  QUARK_PCR = g_quark_from_string ("pcr");
  QUARK_OPCR = g_quark_from_string ("opcr");
  QUARK_PTS = g_quark_from_string ("pts");
  QUARK_DTS = g_quark_from_string ("dts");
  QUARK_OFFSET = g_quark_from_string ("offset");
}

#define gst_ts_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTSDemux, gst_ts_demux, GST_TYPE_MPEGTS_BASE,
    _extra_init ());
#define _do_element_init \
  GST_DEBUG_CATEGORY_INIT (ts_demux_debug, "tsdemux", 0, \
      "MPEG transport stream demuxer");\
  init_pes_parser ();
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (tsdemux, "tsdemux",
    GST_RANK_PRIMARY, GST_TYPE_TS_DEMUX, _do_element_init);

static void
gst_ts_demux_dispose (GObject * object)
{
  GstTSDemux *demux = GST_TS_DEMUX_CAST (object);

  gst_flow_combiner_free (demux->flowcombiner);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_ts_demux_finalize (GObject * object)
{
  GstTSDemux *demux = GST_TS_DEMUX_CAST (object);

  gst_event_replace (&demux->segment_event, NULL);
  g_mutex_clear (&demux->lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_ts_demux_class_init (GstTSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  MpegTSBaseClass *ts_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = gst_ts_demux_set_property;
  gobject_class->get_property = gst_ts_demux_get_property;
  gobject_class->dispose = gst_ts_demux_dispose;
  gobject_class->finalize = gst_ts_demux_finalize;

  g_object_class_install_property (gobject_class, PROP_PROGRAM_NUMBER,
      g_param_spec_int ("program-number", "Program number",
          "Program Number to demux for (-1 to ignore)", -1, G_MAXINT,
          -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EMIT_STATS,
      g_param_spec_boolean ("emit-stats", "Emit statistics",
          "Emit messages for every pcr/opcr/pts/dts", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * tsdemux:send-scte35-events:
   *
   * Whether SCTE 35 sections should be forwarded as events.
   *
   * When forwarding those, potential splice times are converted
   * to running time, and can be used by a downstream muxer to reinject
   * the sections.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_SEND_SCTE35_EVENTS,
      g_param_spec_boolean ("send-scte35-events", "Send SCTE 35 events",
          "Whether SCTE 35 sections should be forwarded as events", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_int ("latency", "Latency",
          "Latency to add for smooth demuxing (in ms)", -1,
          G_MAXINT, DEFAULT_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&audio_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&subpicture_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&private_template));

  gst_element_class_set_static_metadata (element_class,
      "MPEG transport stream demuxer",
      "Codec/Demuxer",
      "Demuxes MPEG2 transport streams",
      "Zaheer Abbas Merali <zaheerabbas at merali dot org>, "
      "Edward Hervey <edward.hervey@collabora.co.uk>");

  ts_class = GST_MPEGTS_BASE_CLASS (klass);
  ts_class->reset = GST_DEBUG_FUNCPTR (gst_ts_demux_reset);
  ts_class->push = GST_DEBUG_FUNCPTR (gst_ts_demux_push);
  ts_class->push_event = GST_DEBUG_FUNCPTR (push_event);
  ts_class->handle_psi = GST_DEBUG_FUNCPTR (handle_psi);
  ts_class->sink_query = GST_DEBUG_FUNCPTR (sink_query);
  ts_class->program_started = GST_DEBUG_FUNCPTR (gst_ts_demux_program_started);
  ts_class->program_stopped = GST_DEBUG_FUNCPTR (gst_ts_demux_program_stopped);
  ts_class->update_program = GST_DEBUG_FUNCPTR (gst_ts_demux_update_program);
  ts_class->can_remove_program = gst_ts_demux_can_remove_program;
  ts_class->stream_added = gst_ts_demux_stream_added;
  ts_class->stream_removed = gst_ts_demux_stream_removed;
  ts_class->seek = GST_DEBUG_FUNCPTR (gst_ts_demux_do_seek);
  ts_class->flush = GST_DEBUG_FUNCPTR (gst_ts_demux_flush);
  ts_class->drain = GST_DEBUG_FUNCPTR (gst_ts_demux_drain);
}

static void
gst_ts_demux_reset (MpegTSBase * base)
{
  GstTSDemux *demux = (GstTSDemux *) base;

  demux->rate = 1.0;
  g_mutex_lock (&demux->lock);
  gst_event_replace (&demux->segment_event, NULL);
  g_mutex_unlock (&demux->lock);

  if (demux->global_tags) {
    gst_tag_list_unref (demux->global_tags);
    demux->global_tags = NULL;
  }

  if (demux->previous_program) {
    mpegts_base_deactivate_and_free_program (base, demux->previous_program);
    demux->previous_program = NULL;
  }

  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;

  demux->last_seek_offset = -1;
  demux->program_generation = 0;

  demux->mpeg_pts_offset = 0;
}

static void
gst_ts_demux_init (GstTSDemux * demux)
{
  MpegTSBase *base = (MpegTSBase *) demux;

  base->stream_size = sizeof (TSDemuxStream);
  base->parse_private_sections = TRUE;
  /* We are not interested in sections (all handled by mpegtsbase) */
  base->push_section = FALSE;

  demux->flowcombiner = gst_flow_combiner_new ();
  demux->requested_program_number = -1;
  demux->program_number = -1;
  demux->latency = DEFAULT_LATENCY;
  gst_ts_demux_reset (base);

  g_mutex_init (&demux->lock);
}


static void
gst_ts_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTSDemux *demux = GST_TS_DEMUX (object);

  switch (prop_id) {
    case PROP_PROGRAM_NUMBER:
      /* FIXME: do something if program is switched as opposed to set at
       * beginning */
      demux->requested_program_number = g_value_get_int (value);
      break;
    case PROP_EMIT_STATS:
      demux->emit_statistics = g_value_get_boolean (value);
      break;
    case PROP_SEND_SCTE35_EVENTS:
      demux->send_scte35_events = g_value_get_boolean (value);
      break;
    case PROP_LATENCY:
      demux->latency = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_ts_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTSDemux *demux = GST_TS_DEMUX (object);

  switch (prop_id) {
    case PROP_PROGRAM_NUMBER:
      g_value_set_int (value, demux->requested_program_number);
      break;
    case PROP_EMIT_STATS:
      g_value_set_boolean (value, demux->emit_statistics);
      break;
    case PROP_SEND_SCTE35_EVENTS:
      g_value_set_boolean (value, demux->send_scte35_events);
      break;
    case PROP_LATENCY:
      g_value_set_int (value, demux->latency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
gst_ts_demux_get_duration (GstTSDemux * demux, GstClockTime * dur)
{
  MpegTSBase *base = (MpegTSBase *) demux;
  gboolean res = FALSE;
  gint64 val;

  if (!demux->program) {
    GST_DEBUG_OBJECT (demux, "No active program yet, can't provide duration");
    return FALSE;
  }

  /* Get total size in bytes */
  if (gst_pad_peer_query_duration (base->sinkpad, GST_FORMAT_BYTES, &val)) {
    /* Convert it to duration */
    *dur =
        mpegts_packetizer_offset_to_ts (base->packetizer, val,
        demux->program->pcr_pid);
    if (GST_CLOCK_TIME_IS_VALID (*dur))
      res = TRUE;
  }
  return res;
}

static gboolean
gst_ts_demux_srcpad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
  GstFormat format;
  GstTSDemux *demux;
  MpegTSBase *base;

  demux = GST_TS_DEMUX (parent);
  base = GST_MPEGTS_BASE (demux);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GST_DEBUG_OBJECT (pad, "query duration");
      gst_query_parse_duration (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        if (!gst_pad_peer_query (base->sinkpad, query)) {
          GstClockTime dur;
          if (gst_ts_demux_get_duration (demux, &dur))
            gst_query_set_duration (query, GST_FORMAT_TIME, dur);
          else
            res = FALSE;
        }
      } else {
        GST_DEBUG_OBJECT (demux, "only query duration on TIME is supported");
        res = FALSE;
      }
      break;
    }
    case GST_QUERY_LATENCY:
    {
      GST_DEBUG_OBJECT (pad, "query latency");
      res = gst_pad_peer_query (base->sinkpad, query);
      if (res) {
        GstClockTime min_lat, max_lat;
        gboolean live;
        gint latency;

        /* According to H.222.0
           Annex D.0.3 (System Time Clock recovery in the decoder)
           and D.0.2 (Audio and video presentation synchronization)

           We can end up with an interval of up to 700ms between valid
           PTS/DTS. We therefore allow a latency of 700ms for that.
         */
        latency = demux->latency;
        if (latency < 0)
          latency = 700;
        gst_query_parse_latency (query, &live, &min_lat, &max_lat);
        min_lat += latency * GST_MSECOND;
        if (GST_CLOCK_TIME_IS_VALID (max_lat))
          max_lat += latency * GST_MSECOND;
        gst_query_set_latency (query, live, min_lat, max_lat);
      }
      break;
    }
    case GST_QUERY_SEEKING:
    {
      GST_DEBUG_OBJECT (pad, "query seeking");
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      GST_DEBUG_OBJECT (pad, "asked for format %s",
          gst_format_get_name (format));
      if (format == GST_FORMAT_TIME) {
        gboolean seekable = FALSE;

        if (gst_pad_peer_query (base->sinkpad, query))
          gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);

        /* If upstream is not seekable in TIME format we use
         * our own values here */
        if (!seekable) {
          GstClockTime dur;
          if (gst_ts_demux_get_duration (demux, &dur)) {
            gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, dur);
            GST_DEBUG_OBJECT (pad, "Gave duration: %" GST_TIME_FORMAT,
                GST_TIME_ARGS (dur));
          }
        }
      } else {
        GST_DEBUG_OBJECT (demux, "only TIME is supported for query seeking");
        res = FALSE;
      }
      break;
    }
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = base->out_segment.format;

      start =
          gst_segment_to_stream_time (&base->out_segment, format,
          base->out_segment.start);
      if ((stop = base->out_segment.stop) == -1)
        stop = base->out_segment.duration;
      else
        stop = gst_segment_to_stream_time (&base->out_segment, format, stop);

      gst_query_set_segment (query, base->out_segment.rate, format, start,
          stop);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
  }

  return res;

}

static void
clear_simple_buffer (SimpleBuffer * sbuf)
{
  if (!sbuf->data)
    return;

  g_free (sbuf->data);
  sbuf->size = 0;
  sbuf->data = NULL;
}

static gboolean
scan_keyframe_h264 (TSDemuxStream * stream, const guint8 * data,
    const gsize data_size, const gsize max_frame_offset)
{
  gint offset = 0;
  GstH264NalUnit unit, frame_unit = { 0, };
  GstH264ParserResult res = GST_H264_PARSER_OK;
  TSDemuxH264ParsingInfos *h264infos = &stream->h264infos;

  GstH264NalParser *parser = h264infos->parser;

  if (G_UNLIKELY (parser == NULL)) {
    parser = h264infos->parser = gst_h264_nal_parser_new ();
    h264infos->sps = gst_byte_writer_new ();
    h264infos->pps = gst_byte_writer_new ();
    h264infos->sei = gst_byte_writer_new ();
  }

  while (res == GST_H264_PARSER_OK) {
    res =
        gst_h264_parser_identify_nalu (parser, data, offset, data_size, &unit);

    if (res != GST_H264_PARSER_OK && res != GST_H264_PARSER_NO_NAL_END) {
      GST_INFO_OBJECT (stream->pad, "Error identifying nalu: %i", res);
      break;
    }

    res = gst_h264_parser_parse_nal (parser, &unit);
    if (res != GST_H264_PARSER_OK) {
      break;
    }

    switch (unit.type) {
      case GST_H264_NAL_SEI:
        if (frame_unit.size)
          break;

        if (gst_byte_writer_put_data (h264infos->sei,
                unit.data + unit.sc_offset,
                unit.size + unit.offset - unit.sc_offset)) {
          GST_DEBUG_OBJECT (stream->pad, "adding SEI %u",
              unit.size + unit.offset - unit.sc_offset);
        } else {
          GST_WARNING_OBJECT (stream->pad, "Could not write SEI");
        }
        break;
      case GST_H264_NAL_PPS:
        if (frame_unit.size)
          break;

        if (gst_byte_writer_put_data (h264infos->pps,
                unit.data + unit.sc_offset,
                unit.size + unit.offset - unit.sc_offset)) {
          GST_DEBUG_OBJECT (stream->pad, "adding PPS %u",
              unit.size + unit.offset - unit.sc_offset);
        } else {
          GST_WARNING_OBJECT (stream->pad, "Could not write PPS");
        }
        break;
      case GST_H264_NAL_SPS:
        if (frame_unit.size)
          break;

        if (gst_byte_writer_put_data (h264infos->sps,
                unit.data + unit.sc_offset,
                unit.size + unit.offset - unit.sc_offset)) {
          GST_DEBUG_OBJECT (stream->pad, "adding SPS %u",
              unit.size + unit.offset - unit.sc_offset);
        } else {
          GST_WARNING_OBJECT (stream->pad, "Could not write SPS");
        }
        break;
        /* these units are considered keyframes in h264parse */
      case GST_H264_NAL_SLICE:
      case GST_H264_NAL_SLICE_DPA:
      case GST_H264_NAL_SLICE_DPB:
      case GST_H264_NAL_SLICE_DPC:
      case GST_H264_NAL_SLICE_IDR:
      {
        GstH264SliceHdr slice;

        if (h264infos->framedata.size)
          break;

        res = gst_h264_parser_parse_slice_hdr (parser, &unit, &slice,
            FALSE, FALSE);

        if (GST_H264_IS_I_SLICE (&slice) || GST_H264_IS_SI_SLICE (&slice)) {
          if (*(unit.data + unit.offset + 1) & 0x80) {
            /* means first_mb_in_slice == 0 */
            /* real frame data */
            GST_DEBUG_OBJECT (stream->pad, "Found keyframe at: %u",
                unit.sc_offset);
            frame_unit = unit;
          }
        }

        break;
      }
      default:
        break;
    }

    if (offset == unit.sc_offset + unit.size)
      break;

    offset = unit.sc_offset + unit.size;
  }

  /* We've got all the infos we need (SPS / PPS and a keyframe, plus
   * and possibly SEI units. We can stop rewinding the stream
   */
  if (gst_byte_writer_get_size (h264infos->sps) &&
      gst_byte_writer_get_size (h264infos->pps) &&
      (h264infos->framedata.size || frame_unit.size)) {
    guint8 *data = NULL;

    gsize tmpsize = gst_byte_writer_get_size (h264infos->pps);

    /*  We know that the SPS is first so just put all our data in there */
    data = gst_byte_writer_reset_and_get_data (h264infos->pps);
    gst_byte_writer_put_data (h264infos->sps, data, tmpsize);
    g_free (data);

    tmpsize = gst_byte_writer_get_size (h264infos->sei);
    if (tmpsize) {
      GST_DEBUG_OBJECT (stream->pad, "Adding SEI");
      data = gst_byte_writer_reset_and_get_data (h264infos->sei);
      gst_byte_writer_put_data (h264infos->sps, data, tmpsize);
      g_free (data);
    }

    if (frame_unit.size) {      /*  We found the everything in one go! */
      GST_DEBUG_OBJECT (stream->pad, "Adding Keyframe");
      gst_byte_writer_put_data (h264infos->sps,
          frame_unit.data + frame_unit.sc_offset,
          stream->current_size - frame_unit.sc_offset);
    } else {
      GST_DEBUG_OBJECT (stream->pad, "Adding Keyframe");
      gst_byte_writer_put_data (h264infos->sps,
          h264infos->framedata.data, h264infos->framedata.size);
      clear_simple_buffer (&h264infos->framedata);
    }

    g_free (stream->data);
    stream->current_size = gst_byte_writer_get_size (h264infos->sps);
    stream->data = gst_byte_writer_reset_and_get_data (h264infos->sps);
    gst_byte_writer_init (h264infos->sps);
    gst_byte_writer_init (h264infos->pps);
    gst_byte_writer_init (h264infos->sei);

    return TRUE;
  }

  if (frame_unit.size) {
    GST_DEBUG_OBJECT (stream->pad, "Keep the keyframe as this is the one"
        " we will push later");

    h264infos->framedata.data =
        g_memdup2 (frame_unit.data + frame_unit.sc_offset,
        stream->current_size - frame_unit.sc_offset);
    h264infos->framedata.size = stream->current_size - frame_unit.sc_offset;
  }

  return FALSE;
}

/* We merge data from TS packets so that the scanning methods get a continuous chunk,
 however the scanning method will return keyframe offset which needs to be translated
 back to actual offset in file */
typedef struct
{
  gint64 real_offset;           /* offset of TS packet */
  gint merged_offset;           /* offset of merged data in buffer */
} OffsetInfo;

static gboolean
gst_ts_demux_adjust_seek_offset_for_keyframe (TSDemuxStream * stream,
    guint8 * data, guint64 size)
{
  int scan_pid = -1;

  if (!stream->scan_function)
    return TRUE;

  scan_pid = ((MpegTSBaseStream *) stream)->pid;

  if (scan_pid != -1) {
    return stream->scan_function (stream, data, size, size);
  }

  return TRUE;
}

static GstFlowReturn
gst_ts_demux_do_seek (MpegTSBase * base, GstEvent * event)
{
  GList *tmp;

  GstTSDemux *demux = (GstTSDemux *) base;
  GstFlowReturn res = GST_FLOW_ERROR;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  guint64 start_offset;
  gboolean update = FALSE;
  GstSegment seeksegment;

  GST_DEBUG_OBJECT (demux, "seek event, %" GST_PTR_FORMAT, event);

  if (base->out_segment.format == GST_FORMAT_UNDEFINED) {
    GST_DEBUG_OBJECT (demux, "Cannot process seek event now, delaying");
    gst_event_replace (&base->seek_event, event);
    res = GST_FLOW_OK;
    goto done;
  }

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  if (rate <= 0.0) {
    GST_WARNING_OBJECT (demux, "Negative rate not supported");
    goto done;
  }

  /* configure the segment with the seek variables */
  memcpy (&seeksegment, &base->out_segment, sizeof (GstSegment));
  GST_LOG_OBJECT (demux, "Before seek, output segment %" GST_SEGMENT_FORMAT,
      &seeksegment);

  /* record offset and rate */
  demux->rate = rate;
  if (!gst_segment_do_seek (&seeksegment, rate, format, flags, start_type,
          start, stop_type, stop, &update)) {
    GST_DEBUG_OBJECT (demux, "Seek failed in gst_segment_do_seek()");
    goto done;
  }

  GST_DEBUG_OBJECT (demux,
      "After seek, update %d output segment now %" GST_SEGMENT_FORMAT, update,
      &seeksegment);

  /* If the position actually changed, update == TRUE */
  g_mutex_lock (&demux->lock);
  if (update) {
    GstClockTime target = seeksegment.start;
    if (target >= SEEK_TIMESTAMP_OFFSET)
      target -= SEEK_TIMESTAMP_OFFSET;
    else
      target = 0;

    start_offset =
        mpegts_packetizer_ts_to_offset (base->packetizer, target,
        demux->program->pcr_pid);
    if (G_UNLIKELY (start_offset == -1)) {
      GST_WARNING_OBJECT (demux,
          "Couldn't convert start position to an offset");
      g_mutex_unlock (&demux->lock);
      goto done;
    }

    base->seek_offset = start_offset;
    demux->last_seek_offset = base->seek_offset;
    /* Reset segment if we're not doing an accurate seek */
    demux->reset_segment = (!(flags & GST_SEEK_FLAG_ACCURATE));

    /* Clear any existing segment - it will be recalculated after streaming recommences */
    gst_event_replace (&demux->segment_event, NULL);

    for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
      TSDemuxStream *stream = tmp->data;

      if (flags & GST_SEEK_FLAG_ACCURATE)
        stream->needs_keyframe = TRUE;

      stream->seeked_pts = GST_CLOCK_TIME_NONE;
      stream->seeked_dts = GST_CLOCK_TIME_NONE;
      stream->first_pts = GST_CLOCK_TIME_NONE;
      stream->need_newsegment = TRUE;
    }
  } else {
    /* Position didn't change, just update the output segment based on
     * our new one */
    gst_event_take (&demux->segment_event,
        gst_event_new_segment (&seeksegment));
    if (base->last_seek_seqnum)
      gst_event_set_seqnum (demux->segment_event, base->last_seek_seqnum);
    for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
      TSDemuxStream *stream = tmp->data;
      stream->need_newsegment = TRUE;
    }
  }
  g_mutex_unlock (&demux->lock);

  /* Commit the new segment */
  memcpy (&base->out_segment, &seeksegment, sizeof (GstSegment));
  /* And prepare to restart */
  gst_flow_combiner_reset (demux->flowcombiner);
  res = GST_FLOW_OK;

done:
  return res;
}

static gboolean
gst_ts_demux_srcpad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstTSDemux *demux = GST_TS_DEMUX (parent);

  GST_DEBUG_OBJECT (pad, "Got event %s",
      gst_event_type_get_name (GST_EVENT_TYPE (event)));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      res = mpegts_base_handle_seek_event ((MpegTSBase *) demux, pad, event);
      if (!res)
        GST_WARNING_OBJECT (pad, "seeking failed");
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
  }

  return res;
}

static void
clean_global_taglist (GstTagList * taglist)
{
  gst_tag_list_remove_tag (taglist, GST_TAG_CONTAINER_FORMAT);
  gst_tag_list_remove_tag (taglist, GST_TAG_CODEC);
}

static gboolean
push_event (MpegTSBase * base, GstEvent * event)
{
  GstTSDemux *demux = (GstTSDemux *) base;
  GList *tmp;
  gboolean early_ret = FALSE;

  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    if (base->segment.format == GST_FORMAT_TIME && base->ignore_pcr) {
      /* Shift start/stop values by 2s */
      base->packetizer->extra_shift = 2 * GST_SECOND;
      if (GST_CLOCK_TIME_IS_VALID (base->segment.start))
        base->segment.start += 2 * GST_SECOND;
      if (GST_CLOCK_TIME_IS_VALID (base->segment.stop))
        base->segment.stop += 2 * GST_SECOND;
      if (GST_CLOCK_TIME_IS_VALID (base->segment.position))
        base->segment.position += 2 * GST_SECOND;
    }
    GST_DEBUG_OBJECT (base, "Ignoring segment event (recreated later)");
    gst_event_unref (event);
    return TRUE;

  } else if (GST_EVENT_TYPE (event) == GST_EVENT_TAG) {
    /* In case we receive tags before data, store them to send later
     * If we already have the program, send it right away */
    GstTagList *taglist;

    gst_event_parse_tag (event, &taglist);

    if (demux->global_tags == NULL) {
      demux->global_tags = gst_tag_list_copy (taglist);

      /* Tags that are stream specific for the container should be considered
       * global for the container streams */
      if (gst_tag_list_get_scope (taglist) == GST_TAG_SCOPE_STREAM) {
        gst_tag_list_set_scope (demux->global_tags, GST_TAG_SCOPE_GLOBAL);
      }
    } else {
      demux->global_tags = gst_tag_list_make_writable (demux->global_tags);
      gst_tag_list_insert (demux->global_tags, taglist, GST_TAG_MERGE_REPLACE);
    }
    clean_global_taglist (demux->global_tags);

    /* tags are stored to be used after if there are no streams yet,
     * so we should never reject */
    early_ret = TRUE;
  }

  if (G_UNLIKELY (demux->program == NULL)) {
    gst_event_unref (event);
    return early_ret;
  }

  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
    if (stream->pad) {
      /* If we are pushing out EOS or segment-done, flush out pending data first */
      if ((GST_EVENT_TYPE (event) == GST_EVENT_EOS ||
              GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT_DONE) &&
          gst_pad_is_active (stream->pad)) {
        gst_ts_demux_push_pending_data (demux, stream, NULL);
      }

      gst_event_ref (event);
      gst_pad_push_event (stream->pad, event);
    }
  }

  gst_event_unref (event);

  return TRUE;
}

static void
handle_psi (MpegTSBase * base, GstMpegtsSection * section)
{
  GstTSDemux *demux = (GstTSDemux *) base;

  if (section->section_type == GST_MPEGTS_SECTION_SCTE_SIT) {
    GList *tmp;
    gboolean forward = FALSE;

    if (demux->send_scte35_events) {
      for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
        TSDemuxStream *stream = (TSDemuxStream *) tmp->data;

        if (stream->stream.pid == section->pid) {
          forward = TRUE;
          break;
        }
      }
    }

    /* Create a new section to travel through the pipeline, with splice
     * times translated from local time to running time */
    if (forward) {
      GstEvent *event;
      GstStructure *s;
      GstStructure *rtime_map;
      GstClockTime pts;
      guint i = 0;
      GstMpegtsSection *new_section =
          (GstMpegtsSection *) gst_mini_object_copy ((GstMiniObject *) section);
      GstMpegtsSCTESIT *sit =
          (GstMpegtsSCTESIT *) gst_mpegts_section_get_scte_sit (new_section);

      rtime_map = gst_structure_new_empty ("running-time-map");

      if (sit->fully_parsed) {
        if (sit->splice_time_specified) {
          pts =
              mpegts_packetizer_pts_to_ts (base->packetizer,
              MPEGTIME_TO_GSTTIME (sit->splice_time +
                  sit->pts_adjustment), demux->program->pcr_pid);
          gst_structure_set (rtime_map, "splice-time", G_TYPE_UINT64,
              gst_segment_to_running_time (&base->out_segment, GST_FORMAT_TIME,
                  pts), NULL);
        }

        for (i = 0; i < sit->splices->len; i++) {
          gchar *field_name;
          GstMpegtsSCTESpliceEvent *sevent =
              g_ptr_array_index (sit->splices, i);

          if (sevent->program_splice_time_specified) {
            pts =
                mpegts_packetizer_pts_to_ts_unchecked (base->packetizer,
                MPEGTIME_TO_GSTTIME (sevent->program_splice_time +
                    sit->pts_adjustment), demux->program->pcr_pid);
            field_name =
                g_strdup_printf ("event-%u-splice-time",
                sevent->splice_event_id);
            gst_structure_set (rtime_map, field_name, G_TYPE_UINT64,
                gst_segment_to_running_time (&base->out_segment,
                    GST_FORMAT_TIME, pts), NULL);
            g_free (field_name);
          }
        }
      }

      event = gst_event_new_mpegts_section (new_section);
      gst_mpegts_section_unref (new_section);

      s = gst_event_writable_structure (event);
      gst_structure_set (s, "mpeg-pts-offset", G_TYPE_UINT64,
          demux->mpeg_pts_offset, "running-time-map", GST_TYPE_STRUCTURE,
          rtime_map, NULL);
      gst_structure_free (rtime_map);

      push_event (base, event);
    }
  }
}

static gboolean
sink_query (MpegTSBase * base, GstQuery * query)
{
  GstTSDemux *demux = (GstTSDemux *) base;
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_BITRATE:{
      gint64 size_bytes;
      GstClockTime duration;

      if (gst_pad_peer_query_duration (base->sinkpad, GST_FORMAT_BYTES,
              &size_bytes) && size_bytes > 0) {
        if (gst_ts_demux_get_duration (demux, &duration) && duration > 0
            && duration != GST_CLOCK_TIME_NONE) {
          guint bitrate =
              gst_util_uint64_scale (8 * size_bytes, GST_SECOND, duration);

          GST_LOG_OBJECT (demux, "bitrate query byte length: %" G_GINT64_FORMAT
              " duration %" GST_TIME_FORMAT " resulting in a bitrate of %u",
              size_bytes, GST_TIME_ARGS (duration), bitrate);
          gst_query_set_bitrate (query, bitrate);
          res = TRUE;
        }
      }
      break;
    }
    default:
      res = GST_MPEGTS_BASE_CLASS (parent_class)->sink_query (base, query);
      break;
  }

  return res;
}

static inline void
add_iso639_language_to_tags (TSDemuxStream * stream, gchar * lang_code)
{
  const gchar *lc;

  GST_LOG_OBJECT (stream->pad, "Add language code for stream: '%s'", lang_code);

  if (!stream->taglist)
    stream->taglist = gst_tag_list_new_empty ();

  /* descriptor contains ISO 639-2 code, we want the ISO 639-1 code */
  lc = gst_tag_get_language_code (lang_code);

  /* Only set tag if we have a valid one */
  if (lc || (lang_code[0] && lang_code[1]))
    gst_tag_list_add (stream->taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_LANGUAGE_CODE, (lc) ? lc : lang_code, NULL);
}

static void
gst_ts_demux_create_tags (TSDemuxStream * stream)
{
  MpegTSBaseStream *bstream = (MpegTSBaseStream *) stream;
  const GstMpegtsDescriptor *desc = NULL;
  int i, nb;

  if (bstream->stream_type == ST_PS_AUDIO_AC3 &&
      !mpegts_get_descriptor_from_stream (bstream,
          GST_MTS_DESC_DVB_ENHANCED_AC3)) {
    const GstMpegtsDescriptor *ac3_desc =
        mpegts_get_descriptor_from_stream (bstream,
        GST_MTS_DESC_AC3_AUDIO_STREAM);
    if (ac3_desc && DESC_AC_AUDIO_STREAM_has_lang1 (ac3_desc->data)) {
      gchar lang_code[4];

      memcpy (lang_code, DESC_AC_AUDIO_STREAM_lang1_code (ac3_desc->data), 3);
      lang_code[3] = '\0';
      add_iso639_language_to_tags (stream, lang_code);
    }
  }

  desc =
      mpegts_get_descriptor_from_stream (bstream,
      GST_MTS_DESC_ISO_639_LANGUAGE);
  if (desc) {
    gchar *lang_code;

    nb = gst_mpegts_descriptor_parse_iso_639_language_nb (desc);

    GST_DEBUG_OBJECT (stream->pad, "Found ISO 639 descriptor (%d entries)", nb);

    for (i = 0; i < nb; i++)
      if (gst_mpegts_descriptor_parse_iso_639_language_idx (desc, i, &lang_code,
              NULL)) {
        add_iso639_language_to_tags (stream, lang_code);
        g_free (lang_code);
      }

    return;
  }

  desc =
      mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_DVB_SUBTITLING);

  if (desc) {
    gchar *lang_code;

    nb = gst_mpegts_descriptor_parse_dvb_subtitling_nb (desc);

    GST_DEBUG_OBJECT (stream->pad, "Found SUBTITLING descriptor (%d entries)",
        nb);

    for (i = 0; i < nb; i++)
      if (gst_mpegts_descriptor_parse_dvb_subtitling_idx (desc, i, &lang_code,
              NULL, NULL, NULL)) {
        add_iso639_language_to_tags (stream, lang_code);
        g_free (lang_code);
      }
  }

  if (bstream->stream_type == GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS) {
    desc = mpegts_get_descriptor_from_stream_with_extension (bstream,
        GST_MTS_DESC_DVB_EXTENSION, GST_MTS_DESC_EXT_DVB_AUDIO_PRESELECTION);

    if (desc) {
      GPtrArray *list;
      GstMpegtsAudioPreselectionDescriptor *item;

      if (gst_mpegts_descriptor_parse_audio_preselection_list (desc, &list)) {
        GST_DEBUG ("Found AUDIO PRESELECTION descriptor (%d entries)",
            list->len);

        for (i = 0; i < list->len; i++) {
          item = g_ptr_array_index (list, i);
          gst_mpegts_descriptor_parse_audio_preselection_dump (item);

          if (item->language_code_present) {
            add_iso639_language_to_tags (stream, item->language_code);
            break;
          }
        }
        g_ptr_array_unref (list);
      }
    }
  }
}

static GstPad *
create_pad_for_stream (MpegTSBase * base, MpegTSBaseStream * bstream,
    MpegTSBaseProgram * program)
{
  GstTSDemux *demux = GST_TS_DEMUX (base);
  TSDemuxStream *stream = (TSDemuxStream *) bstream;
  gchar *name = NULL;
  GstCaps *caps = NULL;
  GstPadTemplate *template = NULL;
  const GstMpegtsDescriptor *desc = NULL;
  GstPad *pad = NULL;
  gboolean sparse = FALSE;
  gboolean is_audio = FALSE, is_video = FALSE, is_subpicture = FALSE,
      is_private = FALSE;

  gst_ts_demux_create_tags (stream);

  GST_LOG_OBJECT (demux,
      "Attempting to create pad for stream 0x%04x with stream_type %d",
      bstream->pid, bstream->stream_type);

  /* First handle BluRay-specific stream types since there is some overlap
   * between BluRay and non-BluRay streay type identifiers */
  if (program->registration_id == DRF_ID_HDMV) {
    switch (bstream->stream_type) {
      case ST_BD_AUDIO_AC3:
      {
        const GstMpegtsDescriptor *ac3_desc;

        /* ATSC ac3 audio descriptor */
        ac3_desc =
            mpegts_get_descriptor_from_stream (bstream,
            GST_MTS_DESC_AC3_AUDIO_STREAM);
        if (ac3_desc && DESC_AC_AUDIO_STREAM_bsid (ac3_desc->data) != 16) {
          GST_LOG_OBJECT (demux, "ac3 audio");
          is_audio = TRUE;
          caps = gst_caps_new_empty_simple ("audio/x-ac3");
        } else {
          is_audio = TRUE;
          caps = gst_caps_new_empty_simple ("audio/x-eac3");
        }
        break;
      }
      case ST_BD_AUDIO_EAC3:
      case ST_BD_AUDIO_AC3_PLUS:
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-eac3");
        break;
      case ST_BD_AUDIO_AC3_TRUE_HD:
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-true-hd");
        stream->target_pes_substream = 0x72;
        break;
      case ST_BD_AUDIO_LPCM:
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-private-ts-lpcm");
        break;
      case ST_BD_PGS_SUBPICTURE:
        is_subpicture = TRUE;
        caps = gst_caps_new_empty_simple ("subpicture/x-pgs");
        sparse = TRUE;
        break;
      case ST_BD_AUDIO_DTS_HD:
      case ST_BD_AUDIO_DTS_HD_MASTER_AUDIO:
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-dts");
        stream->target_pes_substream = 0x71;
        break;
    }
  }

  if (caps)
    goto done;

  /* Handle non-BluRay stream types */
  switch (bstream->stream_type) {
    case GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG1:
    case GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG2:
    case ST_PS_VIDEO_MPEG2_DCII:
      /* FIXME : Use DCII registration code (ETV1 ?) to handle that special
       * Stream type (ST_PS_VIDEO_MPEG2_DCII) */
      /* FIXME : Use video descriptor (0x1) to refine caps with:
       * * frame_rate
       * * profile_and_level
       */
      GST_LOG_OBJECT (demux, "mpeg video");
      is_video = TRUE;
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT,
          bstream->stream_type == GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG1 ? 1 : 2,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);

      break;
    case GST_MPEGTS_STREAM_TYPE_AUDIO_MPEG1:
    case GST_MPEGTS_STREAM_TYPE_AUDIO_MPEG2:
      GST_LOG_OBJECT (demux, "mpeg audio");
      is_audio = TRUE;
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          NULL);
      /* HDV is always mpeg 1 audio layer 2 */
      if (program->registration_id == DRF_ID_TSHV)
        gst_caps_set_simple (caps, "layer", G_TYPE_INT, 2, NULL);
      break;
    case GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS:
      GST_LOG_OBJECT (demux, "private data");
      /* FIXME: Move all of this into a common method (there might be other
       * types also, depending on registratino descriptors also
       */

      desc = mpegts_get_descriptor_from_stream_with_extension (bstream,
          GST_MTS_DESC_DVB_EXTENSION, GST_MTS_DESC_EXT_DVB_AC4);
      if (desc) {
        GST_LOG_OBJECT (demux, "ac4 audio");
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-ac4");
        break;
      }

      desc = mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_DVB_AC3);
      if (desc) {
        GST_LOG_OBJECT (demux, "ac3 audio");
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-ac3");
        break;
      }

      desc =
          mpegts_get_descriptor_from_stream (bstream,
          GST_MTS_DESC_DVB_ENHANCED_AC3);
      if (desc) {
        GST_LOG_OBJECT (demux, "ac3 audio");
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-eac3");
        break;
      }
      desc =
          mpegts_get_descriptor_from_stream (bstream,
          GST_MTS_DESC_DVB_TELETEXT);
      if (desc) {
        GST_LOG_OBJECT (demux, "teletext");
        is_private = TRUE;
        caps = gst_caps_new_empty_simple ("application/x-teletext");
        sparse = TRUE;
        break;
      }
      desc =
          mpegts_get_descriptor_from_stream (bstream,
          GST_MTS_DESC_DVB_SUBTITLING);
      if (desc) {
        GST_LOG_OBJECT (demux, "subtitling");
        is_subpicture = TRUE;
        caps = gst_caps_new_empty_simple ("subpicture/x-dvb");
        sparse = TRUE;
        break;
      }

      switch (bstream->registration_id) {
        case DRF_ID_DTS1:
        case DRF_ID_DTS2:
        case DRF_ID_DTS3:
          /* SMPTE registered DTS */
          is_private = TRUE;
          caps = gst_caps_new_empty_simple ("audio/x-dts");
          break;
        case DRF_ID_S302M:
          is_audio = TRUE;
          caps = gst_caps_new_empty_simple ("audio/x-smpte-302m");
          break;
        case DRF_ID_OPUS:
          desc = mpegts_get_descriptor_from_stream (bstream,
              GST_MTS_DESC_DVB_EXTENSION);
          if (desc != NULL && desc->tag_extension == 0x80 && desc->length >= 1) {       /* User defined (provisional Opus) */
            guint8 channel_config_code;
            GstByteReader br;

            /* skip tag, length and tag_extension */
            gst_byte_reader_init (&br, desc->data + 3, desc->length - 1);
            channel_config_code = gst_byte_reader_get_uint8_unchecked (&br);

            if (channel_config_code < 0x89) {
              static const guint8 coupled_stream_counts[9] = {
                1, 0, 1, 1, 2, 2, 2, 3, 3
              };
              static const guint8 channel_map_a[8][8] = {
                {0},
                {0, 1},
                {0, 2, 1},
                {0, 1, 2, 3},
                {0, 4, 1, 2, 3},
                {0, 4, 1, 2, 3, 5},
                {0, 4, 1, 2, 3, 5, 6},
                {0, 6, 1, 2, 3, 4, 5, 7},
              };
              static const guint8 channel_map_b[8][8] = {
                {0},
                {0, 1},
                {0, 1, 2},
                {0, 1, 2, 3},
                {0, 1, 2, 3, 4},
                {0, 1, 2, 3, 4, 5},
                {0, 1, 2, 3, 4, 5, 6},
                {0, 1, 2, 3, 4, 5, 6, 7},
              };

              gint channels = -1, stream_count, coupled_count, mapping_family;
              guint8 channel_mapping[255] = { 0, };

              channels = channel_config_code ? (channel_config_code & 0x0f) : 2;
              if (channel_config_code == 0 || channel_config_code == 0x80) {
                /* Dual Mono */
                channels = 2;
                mapping_family = 255;
                if (channel_config_code == 0) {
                  stream_count = 1;
                  coupled_count = 1;
                } else {
                  stream_count = 2;
                  coupled_count = 0;
                }
                memcpy (channel_mapping, &channel_map_a[1], channels);
              } else if (channel_config_code <= 8) {
                mapping_family = (channels > 2) ? 1 : 0;
                stream_count =
                    channel_config_code -
                    coupled_stream_counts[channel_config_code];
                coupled_count = coupled_stream_counts[channel_config_code];
                if (mapping_family != 0) {
                  memcpy (channel_mapping, &channel_map_a[channels - 1],
                      channels);
                }
              } else if (channel_config_code >= 0x82
                  && channel_config_code <= 0x88) {
                mapping_family = 1;
                stream_count = channels;
                coupled_count = 0;
                memcpy (channel_mapping, &channel_map_b[channels - 1],
                    channels);
              } else if (channel_config_code == 0x81) {
                if (gst_byte_reader_get_remaining (&br) < 2) {
                  GST_WARNING_OBJECT (demux,
                      "Invalid Opus descriptor with extended channel configuration");
                  channels = -1;
                  break;
                }

                channels = gst_byte_reader_get_uint8_unchecked (&br);
                mapping_family = gst_byte_reader_get_uint8_unchecked (&br);

                /* Overwrite values from above */
                if (channels == 0) {
                  GST_WARNING_OBJECT (demux,
                      "Invalid Opus descriptor with extended channel configuration");
                  channels = -1;
                  break;
                }

                if (mapping_family == 0 && channels <= 2) {
                  stream_count = channels - coupled_stream_counts[channels];
                  coupled_count = coupled_stream_counts[channels];
                } else {
                  GstBitReader breader;
                  guint8 stream_count_minus_one, coupled_stream_count;
                  gint stream_count_minus_one_len, coupled_stream_count_len;
                  gint channel_mapping_len, i;
                  guint remaining_bytes;

                  remaining_bytes = gst_byte_reader_get_remaining (&br);
                  gst_bit_reader_init (&breader,
                      gst_byte_reader_get_data_unchecked
                      (&br, remaining_bytes), remaining_bytes);

                  stream_count_minus_one_len = g_bit_storage (channels);
                  if (!gst_bit_reader_get_bits_uint8 (&breader,
                          &stream_count_minus_one,
                          stream_count_minus_one_len)) {
                    GST_WARNING_OBJECT (demux,
                        "Invalid Opus descriptor with extended channel configuration");
                    channels = -1;
                    break;
                  }

                  stream_count = stream_count_minus_one + 1;
                  coupled_stream_count_len = g_bit_storage (stream_count + 1);

                  if (!gst_bit_reader_get_bits_uint8 (&breader,
                          &coupled_stream_count, coupled_stream_count_len)) {
                    GST_WARNING_OBJECT (demux,
                        "Invalid Opus descriptor with extended channel configuration");
                    channels = -1;
                    break;
                  }

                  coupled_count = coupled_stream_count;

                  channel_mapping_len =
                      g_bit_storage (stream_count + coupled_stream_count + 1);
                  for (i = 0; i < channels; i++) {
                    if (!gst_bit_reader_get_bits_uint8 (&breader,
                            &channel_mapping[i], channel_mapping_len)) {
                      GST_WARNING_OBJECT (demux,
                          "Invalid Opus descriptor with extended channel configuration");
                      break;
                    }
                  }

                  /* error above */
                  if (i != channels) {
                    channels = -1;
                    break;
                  }
                }
              } else {
                g_assert_not_reached ();
              }

              if (channels != -1) {
                is_audio = TRUE;
                caps =
                    gst_codec_utils_opus_create_caps (48000, channels,
                    mapping_family, stream_count, coupled_count,
                    channel_mapping);
              }
            } else {
              GST_WARNING_OBJECT (demux,
                  "unexpected channel config code 0x%02x", channel_config_code);
            }
          } else {
            GST_WARNING_OBJECT (demux, "Opus, but no extension descriptor");
          }
          break;
        case DRF_ID_HEVC:
          is_video = TRUE;
          caps = gst_caps_new_simple ("video/x-h265",
              "stream-format", G_TYPE_STRING, "byte-stream", NULL);
          break;
        case DRF_ID_KLVA:
          sparse = TRUE;
          is_private = TRUE;
          caps = gst_caps_new_simple ("meta/x-klv",
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
          break;
        case DRF_ID_AC4:
          is_audio = TRUE;
          caps = gst_caps_new_empty_simple ("audio/x-ac4");
          break;
      }
      if (caps)
        break;

      /* hack for itv hd (sid 10510, video pid 3401 */
      if (program->program_number == 10510 && bstream->pid == 3401) {
        is_video = TRUE;
        caps = gst_caps_new_simple ("video/x-h264",
            "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      }
      break;
    case ST_HDV_AUX_V:
      /* FIXME : Should only be used with specific PMT registration_descriptor */
      /* We don't expose those streams since they're only helper streams */
      /* template = gst_static_pad_template_get (&private_template); */
      /* name = g_strdup_printf ("private_%04x", bstream->pid); */
      /* caps = gst_caps_new_simple ("hdv/aux-v", NULL); */
      break;
    case ST_HDV_AUX_A:
      /* FIXME : Should only be used with specific PMT registration_descriptor */
      /* We don't expose those streams since they're only helper streams */
      /* template = gst_static_pad_template_get (&private_template); */
      /* name = g_strdup_printf ("private_%04x", bstream->pid); */
      /* caps = gst_caps_new_simple ("hdv/aux-a", NULL); */
      break;
    case GST_MPEGTS_STREAM_TYPE_AUDIO_AAC_ADTS:
      is_audio = TRUE;
      /* prefer mpegversion 4 since it's more commonly supported one */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "adts", NULL);
      /* we will set caps later once parsing adts header is done */
      stream->atdsInfos.mpegversion = 4;
      break;
    case GST_MPEGTS_STREAM_TYPE_AUDIO_AAC_LATM:
      is_audio = TRUE;
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "loas", NULL);
      break;
    case GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG4:
      is_video = TRUE;
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      break;
    case GST_MPEGTS_STREAM_TYPE_METADATA_PES_PACKETS:
      desc = mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_METADATA);
      if (desc) {
        GstMpegtsMetadataDescriptor *metadataDescriptor;
        if (gst_mpegts_descriptor_parse_metadata (desc, &metadataDescriptor)) {
          if ((metadataDescriptor->metadata_format ==
                  GST_MPEGTS_METADATA_FORMAT_IDENTIFIER_FIELD)
              && (metadataDescriptor->metadata_format_identifier ==
                  DRF_ID_KLVA)) {
            sparse = TRUE;
            is_private = TRUE;
            /* registration_id is not correctly set or parsed for some streams */
            bstream->registration_id = DRF_ID_KLVA;

            caps = gst_caps_new_simple ("meta/x-klv",
                "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
          }
          g_free (metadataDescriptor);
        }
      }
      break;
    case GST_MPEGTS_STREAM_TYPE_VIDEO_H264:
      is_video = TRUE;
      caps = gst_caps_new_simple ("video/x-h264",
          "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      break;
    case GST_MPEGTS_STREAM_TYPE_VIDEO_HEVC:
      is_video = TRUE;
      caps = gst_caps_new_simple ("video/x-h265",
          "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      break;
    case GST_MPEGTS_STREAM_TYPE_VIDEO_JP2K:
      is_video = TRUE;
      desc =
          mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_J2K_VIDEO);
      if (desc == NULL) {
        caps = gst_caps_new_empty_simple ("image/x-jpc");
        break;
      } else {
        GstByteReader br;
        guint16 DEN_frame_rate = 0;
        guint16 NUM_frame_rate = 0;
        guint8 color_specification = 0;
        guint8 remaining_8b = 0;
        gboolean interlaced_video = 0;
        const gchar *interlace_mode = NULL;
        const gchar *colorspace = NULL;
        const gchar *colorimetry_mode = NULL;
        guint16 profile_and_level G_GNUC_UNUSED;
        guint32 horizontal_size G_GNUC_UNUSED;
        guint32 vertical_size G_GNUC_UNUSED;
        guint32 max_bit_rate G_GNUC_UNUSED;
        guint32 max_buffer_size G_GNUC_UNUSED;
        const guint desc_min_length = 24;

        if (desc->length < desc_min_length) {
          GST_ERROR_OBJECT (demux,
              "GST_MPEGTS_STREAM_TYPE_VIDEO_JP2K: descriptor length %d too short",
              desc->length);
          return NULL;
        }

        /* Skip the descriptor tag and length */
        gst_byte_reader_init (&br, desc->data + 2, desc->length);

        profile_and_level = gst_byte_reader_get_uint16_be_unchecked (&br);
        horizontal_size = gst_byte_reader_get_uint32_be_unchecked (&br);
        vertical_size = gst_byte_reader_get_uint32_be_unchecked (&br);
        max_bit_rate = gst_byte_reader_get_uint32_be_unchecked (&br);
        max_buffer_size = gst_byte_reader_get_uint32_be_unchecked (&br);
        DEN_frame_rate = gst_byte_reader_get_uint16_be_unchecked (&br);
        NUM_frame_rate = gst_byte_reader_get_uint16_be_unchecked (&br);
        color_specification = gst_byte_reader_get_uint8_unchecked (&br);
        remaining_8b = gst_byte_reader_get_uint8_unchecked (&br);
        interlaced_video = remaining_8b & 0x40;
        /* we don't support demuxing interlaced at the moment */
        if (interlaced_video) {
          GST_ERROR_OBJECT (demux,
              "GST_MPEGTS_STREAM_TYPE_VIDEO_JP2K: interlaced video not supported");
          return NULL;
        } else {
          interlace_mode = "progressive";
          stream->jp2kInfos.interlace = FALSE;
        }
        switch (color_specification) {
          case GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_SRGB:
            colorspace = "sRGB";
            colorimetry_mode = GST_VIDEO_COLORIMETRY_SRGB;
            break;
          case GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_REC601:
            colorspace = "sYUV";
            colorimetry_mode = GST_VIDEO_COLORIMETRY_BT601;
            break;
          case GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_REC709:
          case GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_CIELUV:
            colorspace = "sYUV";
            colorimetry_mode = GST_VIDEO_COLORIMETRY_BT709;
            break;
          default:
            break;
        }
        caps = gst_caps_new_simple ("image/x-jpc",
            "framerate", GST_TYPE_FRACTION, NUM_frame_rate, DEN_frame_rate,
            "interlace-mode", G_TYPE_STRING, interlace_mode,
            "colorimetry", G_TYPE_STRING, colorimetry_mode,
            "colorspace", G_TYPE_STRING, colorspace, NULL);
      }
      break;
    case ST_VIDEO_DIRAC:
      if (bstream->registration_id == 0x64726163) {
        GST_LOG_OBJECT (demux, "dirac");
        /* dirac in hex */
        is_video = TRUE;
        caps = gst_caps_new_empty_simple ("video/x-dirac");
      }
      break;
    case ST_PRIVATE_EA:        /* Try to detect a VC1 stream */
    {
      gboolean is_vc1 = FALSE;

      /* Note/FIXME: RP-227 specifies that the registration descriptor
       * for vc1 can also contain other information, such as profile,
       * level, alignment, buffer_size, .... */
      if (bstream->registration_id == DRF_ID_VC1)
        is_vc1 = TRUE;
      if (!is_vc1) {
        GST_WARNING_OBJECT (demux, "0xea private stream type found but "
            "no descriptor for VC1. Assuming plain VC1.");
      }

      is_video = TRUE;
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "WVC1", NULL);

      break;
    }
    case ST_PS_AUDIO_AC3:
      /* DVB_ENHANCED_AC3 */
      desc =
          mpegts_get_descriptor_from_stream (bstream,
          GST_MTS_DESC_DVB_ENHANCED_AC3);
      if (desc) {
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-eac3");
        break;
      }

      /* If stream has ac3 descriptor
       * OR program is ATSC (GA94)
       * OR stream registration is AC-3
       * then it's regular AC3 */
      if (bstream->registration_id == DRF_ID_AC3 ||
          program->registration_id == DRF_ID_GA94 ||
          mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_DVB_AC3)) {
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-ac3");
        break;
      }

      GST_WARNING_OBJECT (demux,
          "AC3 stream type found but no guaranteed "
          "way found to differentiate between AC3 and EAC3. "
          "Assuming plain AC3.");
      is_audio = TRUE;
      caps = gst_caps_new_empty_simple ("audio/x-ac3");
      break;
    case ST_PS_AUDIO_EAC3:
    {
      /* ATSC_ENHANCED_AC3 */
      if (bstream->registration_id == DRF_ID_EAC3 ||
          mpegts_get_descriptor_from_stream (bstream, GST_MTS_DESC_ATSC_EAC3)) {
        is_audio = TRUE;
        caps = gst_caps_new_empty_simple ("audio/x-eac3");
        break;
      }

      GST_ELEMENT_WARNING (demux, STREAM, DEMUX,
          ("Assuming ATSC E-AC3 audio stream."),
          ("ATSC E-AC3 stream type found but no guarantee way found to "
              "differentiate among other standards (DVB, ISDB and etc..)"));

      is_audio = TRUE;
      caps = gst_caps_new_empty_simple ("audio/x-eac3");
      break;
    }
    case ST_PS_AUDIO_LPCM2:
      is_audio = TRUE;
      caps = gst_caps_new_empty_simple ("audio/x-private2-lpcm");
      break;
    case ST_PS_AUDIO_DTS:
      is_audio = TRUE;
      caps = gst_caps_new_empty_simple ("audio/x-dts");
      break;
    case ST_PS_AUDIO_LPCM:
      is_audio = TRUE;
      caps = gst_caps_new_empty_simple ("audio/x-lpcm");
      break;
    case ST_PS_DVD_SUBPICTURE:
      is_subpicture = TRUE;
      caps = gst_caps_new_empty_simple ("subpicture/x-dvd");
      sparse = TRUE;
      break;
    case 0x42:
      /* hack for Chinese AVS video stream which use 0x42 as stream_id
       * NOTE: this is unofficial and within the ISO reserved range. */
      is_video = TRUE;
      caps = gst_caps_new_empty_simple ("video/x-cavs");
      break;
    default:
      GST_DEBUG_OBJECT (demux,
          "Non-media stream (stream_type:0x%x). Not creating pad",
          bstream->stream_type);
      break;
  }

done:
  if (caps) {
    if (is_audio) {
      template = gst_static_pad_template_get (&audio_template);
      name =
          g_strdup_printf ("audio_%01x_%04x", demux->program_generation,
          bstream->pid);
      gst_stream_set_stream_type (bstream->stream_object,
          GST_STREAM_TYPE_AUDIO);
    } else if (is_video) {
      template = gst_static_pad_template_get (&video_template);
      name =
          g_strdup_printf ("video_%01x_%04x", demux->program_generation,
          bstream->pid);
      gst_stream_set_stream_type (bstream->stream_object,
          GST_STREAM_TYPE_VIDEO);
    } else if (is_private) {
      template = gst_static_pad_template_get (&private_template);
      name =
          g_strdup_printf ("private_%01x_%04x", demux->program_generation,
          bstream->pid);
    } else if (is_subpicture) {
      template = gst_static_pad_template_get (&subpicture_template);
      name =
          g_strdup_printf ("subpicture_%01x_%04x", demux->program_generation,
          bstream->pid);
      gst_stream_set_stream_type (bstream->stream_object, GST_STREAM_TYPE_TEXT);
    } else
      g_assert_not_reached ();

  }

  if (template && name && caps) {
    GstEvent *event;
    const gchar *stream_id;

    GST_LOG_OBJECT (demux,
        "stream:%p creating pad with name %s and caps %" GST_PTR_FORMAT,
        stream, name, caps);
    pad = gst_pad_new_from_template (template, name);
    gst_pad_set_active (pad, TRUE);
    gst_pad_use_fixed_caps (pad);
    stream_id = gst_stream_get_stream_id (bstream->stream_object);

    event = gst_pad_get_sticky_event (base->sinkpad, GST_EVENT_STREAM_START, 0);
    if (event) {
      if (gst_event_parse_group_id (event, &demux->group_id))
        demux->have_group_id = TRUE;
      else
        demux->have_group_id = FALSE;
      gst_event_unref (event);
    } else if (!demux->have_group_id) {
      demux->have_group_id = TRUE;
      demux->group_id = gst_util_group_id_next ();
    }
    event = gst_event_new_stream_start (stream_id);
    gst_event_set_stream (event, bstream->stream_object);
    if (demux->have_group_id)
      gst_event_set_group_id (event, demux->group_id);
    if (sparse) {
      gst_event_set_stream_flags (event, GST_STREAM_FLAG_SPARSE);
      gst_stream_set_stream_flags (bstream->stream_object,
          GST_STREAM_FLAG_SPARSE);
    }
    stream->sparse = sparse;
    gst_stream_set_caps (bstream->stream_object, caps);
    if (!stream->taglist)
      stream->taglist = gst_tag_list_new_empty ();
    gst_pb_utils_add_codec_description_to_tag_list (stream->taglist, NULL,
        caps);
    gst_stream_set_tags (bstream->stream_object, stream->taglist);

    gst_pad_push_event (pad, event);
    gst_pad_set_caps (pad, caps);
    gst_pad_set_query_function (pad, gst_ts_demux_srcpad_query);
    gst_pad_set_event_function (pad, gst_ts_demux_srcpad_event);
  }

  g_free (name);
  if (template)
    gst_object_unref (template);
  if (caps)
    gst_caps_unref (caps);

  return pad;
}

static gboolean
gst_ts_demux_stream_added (MpegTSBase * base, MpegTSBaseStream * bstream,
    MpegTSBaseProgram * program)
{
  GstTSDemux *demux = (GstTSDemux *) base;
  TSDemuxStream *stream = (TSDemuxStream *) bstream;

  if (!stream->pad) {
    /* Create the pad */
    if (bstream->stream_type != 0xff) {
      stream->pad = create_pad_for_stream (base, bstream, program);
      if (stream->pad)
        gst_flow_combiner_add_pad (demux->flowcombiner, stream->pad);
    }

    if (base->mode != BASE_MODE_PUSHING
        && bstream->stream_type == GST_MPEGTS_STREAM_TYPE_VIDEO_H264) {
      stream->scan_function =
          (GstTsDemuxKeyFrameScanFunction) scan_keyframe_h264;
    } else {
      stream->scan_function = NULL;
    }

    stream->active = FALSE;

    stream->need_newsegment = TRUE;
    /* Reset segment if we're not doing an accurate seek */
    demux->reset_segment =
        (!(base->out_segment.flags & GST_SEEK_FLAG_ACCURATE));
    stream->needs_keyframe = FALSE;
    stream->discont = TRUE;
    stream->pts = GST_CLOCK_TIME_NONE;
    stream->dts = GST_CLOCK_TIME_NONE;
    stream->first_pts = GST_CLOCK_TIME_NONE;
    stream->raw_pts = -1;
    stream->raw_dts = -1;
    stream->pending_ts = TRUE;
    stream->nb_out_buffers = 0;
    stream->gap_ref_buffers = 0;
    stream->gap_ref_pts = GST_CLOCK_TIME_NONE;
    /* Only wait for a valid timestamp if we have a PCR_PID */
    stream->pending_ts = program->pcr_pid < 0x1fff;
    stream->continuity_counter = CONTINUITY_UNSET;
  }

  return (stream->pad != NULL);
}

static void
tsdemux_h264_parsing_info_clear (TSDemuxH264ParsingInfos * h264infos)
{
  clear_simple_buffer (&h264infos->framedata);

  if (h264infos->parser) {
    gst_h264_nal_parser_free (h264infos->parser);
    gst_byte_writer_free (h264infos->sps);
    gst_byte_writer_free (h264infos->pps);
    gst_byte_writer_free (h264infos->sei);
  }
}

static void
gst_ts_demux_stream_removed (MpegTSBase * base, MpegTSBaseStream * bstream)
{
  TSDemuxStream *stream = (TSDemuxStream *) bstream;

  if (stream->pad) {
    gst_flow_combiner_remove_pad (GST_TS_DEMUX_CAST (base)->flowcombiner,
        stream->pad);
    if (stream->active) {

      if (gst_pad_is_active (stream->pad)) {
        /* Flush out all data */
        GST_DEBUG_OBJECT (stream->pad, "Flushing out pending data");
        gst_ts_demux_push_pending_data ((GstTSDemux *) base, stream, NULL);

        GST_DEBUG_OBJECT (stream->pad, "Pushing out EOS");
        gst_pad_push_event (stream->pad, gst_event_new_eos ());
        gst_pad_set_active (stream->pad, FALSE);
      }

      GST_DEBUG_OBJECT (stream->pad, "Removing pad");
      gst_element_remove_pad (GST_ELEMENT_CAST (base), stream->pad);
      stream->active = FALSE;
    } else {
      gst_object_unref (stream->pad);
    }
    stream->pad = NULL;
  }

  gst_ts_demux_stream_flush (stream, GST_TS_DEMUX_CAST (base), TRUE);

  if (stream->taglist != NULL) {
    gst_tag_list_unref (stream->taglist);
    stream->taglist = NULL;
  }

  tsdemux_h264_parsing_info_clear (&stream->h264infos);
}

static void
activate_pad_for_stream (GstTSDemux * tsdemux, TSDemuxStream * stream)
{
  if (stream->pad) {
    GST_DEBUG_OBJECT (tsdemux, "Activating pad %s:%s for stream %p",
        GST_DEBUG_PAD_NAME (stream->pad), stream);
    gst_element_add_pad ((GstElement *) tsdemux, stream->pad);
    stream->active = TRUE;
    GST_DEBUG_OBJECT (stream->pad, "done adding pad");
  } else if (((MpegTSBaseStream *) stream)->stream_type != 0xff) {
    GST_DEBUG_OBJECT (tsdemux,
        "stream %p (pid 0x%04x, type:0x%02x) has no pad", stream,
        ((MpegTSBaseStream *) stream)->pid,
        ((MpegTSBaseStream *) stream)->stream_type);
  }
}

static void
gst_ts_demux_stream_flush (TSDemuxStream * stream, GstTSDemux * tsdemux,
    gboolean hard)
{
  GST_DEBUG ("flushing stream %p", stream);

  g_free (stream->data);
  stream->data = NULL;
  g_free (stream->pending_header_data);
  stream->pending_header_data = NULL;
  stream->pending_header_size = 0;
  stream->state = PENDING_PACKET_EMPTY;
  stream->expected_size = 0;
  stream->allocated_size = 0;
  stream->current_size = 0;
  stream->discont = TRUE;
  stream->pts = GST_CLOCK_TIME_NONE;
  stream->dts = GST_CLOCK_TIME_NONE;
  stream->raw_pts = -1;
  stream->raw_dts = -1;
  stream->pending_ts = TRUE;
  stream->nb_out_buffers = 0;
  stream->gap_ref_buffers = 0;
  stream->gap_ref_pts = GST_CLOCK_TIME_NONE;
  stream->continuity_counter = CONTINUITY_UNSET;

  if (G_UNLIKELY (stream->pending)) {
    GList *tmp;

    GST_DEBUG ("clearing pending %p", stream);
    for (tmp = stream->pending; tmp; tmp = tmp->next) {
      PendingBuffer *pend = (PendingBuffer *) tmp->data;
      gst_buffer_unref (pend->buffer);
      g_free (pend);
    }
    g_list_free (stream->pending);
    stream->pending = NULL;
  }

  if (hard) {
    stream->first_pts = GST_CLOCK_TIME_NONE;
    stream->need_newsegment = TRUE;
  }
}

static void
gst_ts_demux_flush_streams (GstTSDemux * demux, gboolean hard)
{
  GList *walk;
  if (!demux->program)
    return;

  for (walk = demux->program->stream_list; walk; walk = g_list_next (walk))
    gst_ts_demux_stream_flush (walk->data, demux, hard);
}

static gboolean
gst_ts_demux_can_remove_program (MpegTSBase * base, MpegTSBaseProgram * program)
{
  GstTSDemux *demux = GST_TS_DEMUX (base);

  /* If it's our current active program, we return FALSE, we'll deactivate it
   * ourselves when the next program gets activated */
  if (demux->program == program) {
    GST_DEBUG
        ("Attempting to remove current program, delaying until new program gets activated");
    demux->previous_program = program;
    demux->program_number = -1;
    return FALSE;
  }
  return TRUE;
}

static void
gst_ts_demux_update_program (MpegTSBase * base, MpegTSBaseProgram * program)
{
  GstTSDemux *demux = GST_TS_DEMUX (base);
  GList *tmp;

  GST_DEBUG ("Updating program %d", program->program_number);
  /* Emit collection message */
  gst_element_post_message ((GstElement *) base,
      gst_message_new_stream_collection ((GstObject *) base,
          program->collection));

  /* Add all streams, then fire no-more-pads */
  for (tmp = program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
    if (!stream->pad) {
      activate_pad_for_stream (demux, stream);
      if (stream->sparse) {
        /* force sending of pending sticky events which have been stored on the
         * pad already and which otherwise would only be sent on the first buffer
         * or serialized event (which means very late in case of subtitle streams),
         * and playsink waits for stream-start or another serialized event */
        GST_DEBUG_OBJECT (stream->pad, "sparse stream, pushing GAP event");
        gst_pad_push_event (stream->pad, gst_event_new_gap (0, 0));
      }
    }
    if (stream->pad)
      gst_pad_push_event (stream->pad,
          gst_event_new_stream_collection (program->collection));
  }
}

static void
gst_ts_demux_program_started (MpegTSBase * base, MpegTSBaseProgram * program)
{
  GstTSDemux *demux = GST_TS_DEMUX (base);

  GST_DEBUG ("Current program %d, new program %d requested program %d",
      (gint) demux->program_number, program->program_number,
      demux->requested_program_number);

  if (demux->requested_program_number == program->program_number ||
      (demux->requested_program_number == -1 && demux->program_number == -1)) {
    GList *tmp;
    gboolean have_pads = FALSE;

    GST_LOG ("program %d started", program->program_number);
    demux->program_number = program->program_number;
    demux->program = program;

    /* Increment the program_generation counter */
    demux->program_generation = (demux->program_generation + 1) & 0xf;

    /* Emit collection message */
    gst_element_post_message ((GstElement *) base,
        gst_message_new_stream_collection ((GstObject *) base,
            program->collection));

    /* If this is not the initial program, we need to calculate
     * a new segment */
    g_mutex_lock (&demux->lock);
    gst_event_replace (&demux->segment_event, NULL);
    g_mutex_unlock (&demux->lock);

    /* DRAIN ALL STREAMS FIRST ! */
    if (demux->previous_program) {
      GList *tmp;
      GST_DEBUG_OBJECT (demux, "Draining previous program");
      for (tmp = demux->previous_program->stream_list; tmp; tmp = tmp->next) {
        TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
        if (stream->pad)
          gst_ts_demux_push_pending_data (demux, stream,
              demux->previous_program);
      }
    }

    /* Add all streams, then fire no-more-pads */
    for (tmp = program->stream_list; tmp; tmp = tmp->next) {
      TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
      activate_pad_for_stream (demux, stream);
      if (stream->pad)
        have_pads = TRUE;
    }

    /* If there was a previous program, now is the time to deactivate it
     * and remove old pads (including pushing EOS) */
    if (demux->previous_program) {
      GST_DEBUG ("Deactivating previous program");
      mpegts_base_deactivate_and_free_program (base, demux->previous_program);
      demux->previous_program = NULL;
    }

    if (!have_pads) {
      /* If we had no pads, this stream is likely corrupted or unsupported and
       * there's not much we can do at this point */
      GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE,
          ("This stream contains no valid or supported streams."),
          ("activating program but got no pads"));
      return;
    }

    /* If any of the stream is sparse, push a GAP event before anything else
     * This is done here, and not in activate_pad_for_stream() because pushing
     * a GAP event *is* considering data, and we want to ensure the (potential)
     * old pads are all removed before we push any data on the new ones */
    for (tmp = program->stream_list; tmp; tmp = tmp->next) {
      TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
      if (stream->sparse) {
        /* force sending of pending sticky events which have been stored on the
         * pad already and which otherwise would only be sent on the first buffer
         * or serialized event (which means very late in case of subtitle streams),
         * and playsink waits for stream-start or another serialized event */
        GST_DEBUG_OBJECT (stream->pad, "sparse stream, pushing GAP event");
        gst_pad_push_event (stream->pad, gst_event_new_gap (0, 0));
      }
      if (stream->pad)
        gst_pad_push_event (stream->pad,
            gst_event_new_stream_collection (program->collection));
    }

    gst_element_no_more_pads ((GstElement *) demux);
  }
}

static void
gst_ts_demux_program_stopped (MpegTSBase * base, MpegTSBaseProgram * program)
{
  GstTSDemux *demux = GST_TS_DEMUX (base);

  if (demux->program == program) {
    demux->program = NULL;
    demux->program_number = -1;
  }
}


static inline void
gst_ts_demux_record_pts (GstTSDemux * demux, TSDemuxStream * stream,
    guint64 pts, guint64 offset)
{
  MpegTSBaseStream *bs = (MpegTSBaseStream *) stream;
  MpegTSBase *base = GST_MPEGTS_BASE (demux);

  stream->raw_pts = pts;
  if (pts == -1) {
    stream->pts = GST_CLOCK_TIME_NONE;
    return;
  }

  GST_LOG ("pid 0x%04x raw pts:%" G_GUINT64_FORMAT " at offset %"
      G_GUINT64_FORMAT, bs->pid, pts, offset);

  /* Compute PTS in GstClockTime */
  stream->pts =
      mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
      MPEGTIME_TO_GSTTIME (pts), demux->program->pcr_pid);

  if (base->out_segment.format == GST_FORMAT_TIME)
    demux->mpeg_pts_offset =
        (GSTTIME_TO_MPEGTIME (gst_segment_to_running_time (&base->out_segment,
                GST_FORMAT_TIME, stream->pts)) - pts) & 0x1ffffffff;

  /* Sanity check, some stream have completely different PTS vs DTS. If that
   * happens we only keep the DTS */
  if (GST_CLOCK_TIME_IS_VALID (stream->pts) &&
      GST_CLOCK_TIME_IS_VALID (stream->dts) &&
      ABSDIFF (stream->pts, stream->dts) > 5 * GST_SECOND) {
    GST_WARNING ("pid 0x%04x PTS %" GST_TIME_FORMAT
        " differs too much against DTS %" GST_TIME_FORMAT ", discarding it",
        bs->pid, GST_TIME_ARGS (stream->pts), GST_TIME_ARGS (stream->dts));
    stream->raw_pts = stream->raw_dts;
    stream->pts = stream->dts;
    return;
  }

  GST_LOG ("pid 0x%04x Stored PTS %" G_GUINT64_FORMAT, bs->pid, stream->pts);

  if (G_UNLIKELY (demux->emit_statistics)) {
    GstStructure *st;
    st = gst_structure_new_id_empty (QUARK_TSDEMUX);
    gst_structure_id_set (st,
        QUARK_PID, G_TYPE_UINT, bs->pid,
        QUARK_OFFSET, G_TYPE_UINT64, offset, QUARK_PTS, G_TYPE_UINT64, pts,
        NULL);
    gst_element_post_message (GST_ELEMENT_CAST (demux),
        gst_message_new_element (GST_OBJECT (demux), st));
  }
}

static inline void
gst_ts_demux_record_dts (GstTSDemux * demux, TSDemuxStream * stream,
    guint64 dts, guint64 offset)
{
  MpegTSBaseStream *bs = (MpegTSBaseStream *) stream;

  stream->raw_dts = dts;
  if (dts == -1) {
    stream->dts = GST_CLOCK_TIME_NONE;
    return;
  }

  GST_LOG ("pid 0x%04x raw dts:%" G_GUINT64_FORMAT " at offset %"
      G_GUINT64_FORMAT, bs->pid, dts, offset);

  /* Compute DTS in GstClockTime */
  stream->dts =
      mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
      MPEGTIME_TO_GSTTIME (dts), demux->program->pcr_pid);

  GST_LOG ("pid 0x%04x Stored DTS %" G_GUINT64_FORMAT, bs->pid, stream->dts);

  if (G_UNLIKELY (demux->emit_statistics)) {
    GstStructure *st;
    st = gst_structure_new_id_empty (QUARK_TSDEMUX);
    gst_structure_id_set (st,
        QUARK_PID, G_TYPE_UINT, bs->pid,
        QUARK_OFFSET, G_TYPE_UINT64, offset, QUARK_DTS, G_TYPE_UINT64, dts,
        NULL);
    gst_element_post_message (GST_ELEMENT_CAST (demux),
        gst_message_new_element (GST_OBJECT (demux), st));
  }
}

/* This is called when we haven't got a valid initial PTS/DTS on all streams */
static gboolean
check_pending_buffers (GstTSDemux * demux)
{
  gboolean have_observation = FALSE;
  /* The biggest offset */
  guint64 offset = 0;
  GList *tmp;
  gboolean have_only_sparse = TRUE;
  gboolean exceeded_threshold = FALSE;

  /* 0. Do we only have sparse stream */
  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *tmpstream = (TSDemuxStream *) tmp->data;

    if (!tmpstream->sparse) {
      have_only_sparse = FALSE;
      break;
    }
  }

  /* 1. Go over all streams */
  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *tmpstream = (TSDemuxStream *) tmp->data;
    /* 1.1 check if at least one stream got a valid DTS */
    if (have_only_sparse || !tmpstream->sparse) {
      if ((tmpstream->raw_dts != -1 && tmpstream->dts != GST_CLOCK_TIME_NONE) ||
          (tmpstream->raw_pts != -1 && tmpstream->pts != GST_CLOCK_TIME_NONE)) {
        have_observation = TRUE;
        break;
      }
      /* 1.2 Check if we exceeded the maximum threshold of pending data */
      if (tmpstream->pending && (tmpstream->raw_dts != -1
              || tmpstream->raw_pts != -1)) {
        PendingBuffer *pend = tmpstream->pending->data;
        guint64 lastval =
            tmpstream->raw_dts != -1 ? tmpstream->raw_dts : tmpstream->raw_pts;
        guint64 firstval = pend->dts != -1 ? pend->dts : pend->pts;
        GstClockTime dur;
        g_assert (firstval != -1);
        dur = MPEGTIME_TO_GSTTIME (lastval - firstval);
        GST_DEBUG_OBJECT (tmpstream->pad,
            "Pending content duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
        if (dur > 500 * GST_MSECOND) {
          exceeded_threshold = TRUE;
          break;
        }
      }
    }
  }

  if (have_observation == FALSE) {
    /* 2. If we don't have a valid value yet, break out */
    if (!exceeded_threshold)
      return FALSE;

    /* Except if we've exceed the maximum amount of pending buffers, in which
     * case we ignore PCR from now on */
    GST_DEBUG_OBJECT (demux,
        "Saw more than 500ms of data without PCR. Ignoring PCR from now on");
    GST_MPEGTS_BASE (demux)->ignore_pcr = TRUE;
    demux->program->pcr_pid = 0x1fff;
    g_object_notify (G_OBJECT (demux), "ignore-pcr");
  }

  /* 3. Go over all streams that have current/pending data */
  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *tmpstream = (TSDemuxStream *) tmp->data;
    PendingBuffer *pend;
    guint64 firstval, lastval, ts;

    /* 3.1 Calculate the offset between current DTS and first DTS */
    if (tmpstream->pending == NULL || tmpstream->state == PENDING_PACKET_EMPTY)
      continue;
    /* If we don't have any pending data, the offset is 0 for this stream */
    if (tmpstream->pending == NULL)
      break;
    if (tmpstream->raw_dts != -1)
      lastval = tmpstream->raw_dts;
    else if (tmpstream->raw_pts != -1)
      lastval = tmpstream->raw_pts;
    else {
      GST_WARNING ("Don't have a last DTS/PTS to use for offset recalculation");
      continue;
    }
    pend = tmpstream->pending->data;
    if (pend->dts != -1)
      firstval = pend->dts;
    else if (pend->pts != -1)
      firstval = pend->pts;
    else {
      GST_WARNING
          ("Don't have a first DTS/PTS to use for offset recalculation");
      continue;
    }
    /* 3.2 Add to the offset the report TS for the current DTS */
    ts = mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
        MPEGTIME_TO_GSTTIME (lastval), demux->program->pcr_pid);
    if (ts == GST_CLOCK_TIME_NONE) {
      GST_WARNING ("THIS SHOULD NOT HAPPEN !");
      continue;
    }
    ts += MPEGTIME_TO_GSTTIME (lastval - firstval);
    /* 3.3 If that offset is bigger than the current offset, store it */
    if (ts > offset)
      offset = ts;
  }

  GST_DEBUG ("New initial pcr_offset %" GST_TIME_FORMAT,
      GST_TIME_ARGS (offset));

  /* 4. Set the offset on the packetizer */
  mpegts_packetizer_set_current_pcr_offset (MPEG_TS_BASE_PACKETIZER (demux),
      offset, demux->program->pcr_pid);

  /* 4. Go over all streams */
  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *stream = (TSDemuxStream *) tmp->data;

    stream->pending_ts = FALSE;
    /* 4.1 Set pending_ts for FALSE */

    /* 4.2 Recalculate PTS/DTS (in running time) for pending data */
    if (stream->pending) {
      GList *tmp2;
      for (tmp2 = stream->pending; tmp2; tmp2 = tmp2->next) {
        PendingBuffer *pend = (PendingBuffer *) tmp2->data;
        if (pend->pts != -1)
          GST_BUFFER_PTS (pend->buffer) =
              mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
              MPEGTIME_TO_GSTTIME (pend->pts), demux->program->pcr_pid);
        if (pend->dts != -1)
          GST_BUFFER_DTS (pend->buffer) =
              mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
              MPEGTIME_TO_GSTTIME (pend->dts), demux->program->pcr_pid);
        /* 4.2.2 Set first_pts to TS of lowest PTS (for segment) */
        if (stream->first_pts == GST_CLOCK_TIME_NONE) {
          if (GST_BUFFER_PTS (pend->buffer) != GST_CLOCK_TIME_NONE)
            stream->first_pts = GST_BUFFER_PTS (pend->buffer);
          else if (GST_BUFFER_DTS (pend->buffer) != GST_CLOCK_TIME_NONE)
            stream->first_pts = GST_BUFFER_DTS (pend->buffer);
        }
      }
    }
    /* Recalculate PTS/DTS (in running time) for current data */
    if (stream->state != PENDING_PACKET_EMPTY) {
      if (stream->raw_pts != -1) {
        stream->pts =
            mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
            MPEGTIME_TO_GSTTIME (stream->raw_pts), demux->program->pcr_pid);
        if (stream->first_pts == GST_CLOCK_TIME_NONE)
          stream->first_pts = stream->pts;
      }
      if (stream->raw_dts != -1) {
        stream->dts =
            mpegts_packetizer_pts_to_ts (MPEG_TS_BASE_PACKETIZER (demux),
            MPEGTIME_TO_GSTTIME (stream->raw_dts), demux->program->pcr_pid);
        if (stream->first_pts == GST_CLOCK_TIME_NONE)
          stream->first_pts = stream->dts;
      }
    }
  }

  return TRUE;
}

static void
gst_ts_demux_parse_pes_header (GstTSDemux * demux, TSDemuxStream * stream,
    guint8 * data, guint32 length, guint64 bufferoffset)
{
  PESHeader header;
  PESParsingResult parseres;

  GST_MEMDUMP ("Header buffer", data, MIN (length, 32));

  if (G_UNLIKELY (stream->pending_header_data)) {
    /* Accumulate with previous header if present */
    stream->pending_header_data =
        g_realloc (stream->pending_header_data,
        stream->pending_header_size + length);
    memcpy (stream->pending_header_data + stream->pending_header_size, data,
        length);
    data = stream->pending_header_data;
    length = stream->pending_header_size + length;
  }

  parseres = mpegts_parse_pes_header (data, length, &header);

  if (G_UNLIKELY (parseres == PES_PARSING_NEED_MORE)) {
    /* This can happen if PES header is bigger than a packet. */
    if (!stream->pending_header_data)
      stream->pending_header_data = g_memdup2 (data, length);
    stream->pending_header_size = length;
    return;
  }
  if (G_UNLIKELY (parseres == PES_PARSING_BAD)) {
    GST_WARNING ("Error parsing PES header. pid: 0x%x stream_type: 0x%x",
        stream->stream.pid, stream->stream.stream_type);
    goto discont;
  }

  if (stream->target_pes_substream != 0
      && header.stream_id_extension != stream->target_pes_substream) {
    GST_DEBUG ("Skipping unwanted substream");
    goto discont;
  }

  gst_ts_demux_record_dts (demux, stream, header.DTS, bufferoffset);
  gst_ts_demux_record_pts (demux, stream, header.PTS, bufferoffset);
  if (G_UNLIKELY (stream->pending_ts &&
          (stream->pts != GST_CLOCK_TIME_NONE
              || stream->dts != GST_CLOCK_TIME_NONE))) {
    GST_DEBUG ("Got pts/dts update, rechecking all streams");
    check_pending_buffers (demux);
  } else if (stream->first_pts == GST_CLOCK_TIME_NONE) {
    if (GST_CLOCK_TIME_IS_VALID (stream->pts))
      stream->first_pts = stream->pts;
    else if (GST_CLOCK_TIME_IS_VALID (stream->dts))
      stream->first_pts = stream->dts;
  }

  GST_DEBUG_OBJECT (demux,
      "stream PTS %" GST_TIME_FORMAT " DTS %" GST_TIME_FORMAT,
      GST_TIME_ARGS (stream->pts), GST_TIME_ARGS (stream->dts));

  /* Remove PES headers */
  GST_DEBUG ("Moving data forward by %d bytes (packet_size:%d, have:%d)",
      header.header_size, header.packet_length, length);
  stream->expected_size = header.packet_length;
  if (stream->expected_size) {
    if (G_LIKELY (stream->expected_size > header.header_size)) {
      stream->expected_size -= header.header_size;
    } else {
      /* next packet will have to complete this one */
      GST_WARNING ("invalid header and packet size combination, empty packet");
      stream->expected_size = 0;
    }
  }
  data += header.header_size;
  length -= header.header_size;

  /* Create the output buffer */
  if (stream->expected_size)
    stream->allocated_size = MAX (stream->expected_size, length);
  else
    stream->allocated_size = MAX (8192, length);

  g_assert (stream->data == NULL);
  stream->data = g_malloc (stream->allocated_size);
  memcpy (stream->data, data, length);
  stream->current_size = length;

  stream->state = PENDING_PACKET_BUFFER;

  if (stream->pending_header_data) {
    g_free (stream->pending_header_data);
    stream->pending_header_data = NULL;
    stream->pending_header_size = 0;
  }

  return;

discont:
  if (stream->pending_header_data) {
    g_free (stream->pending_header_data);
    stream->pending_header_data = NULL;
    stream->pending_header_size = 0;
  }
  stream->state = PENDING_PACKET_DISCONT;
  return;
}

 /* ONLY CALL THIS:
  * * WITH packet->payload != NULL
  * * WITH pending/current flushed out if beginning of new PES packet
  */
static inline void
gst_ts_demux_queue_data (GstTSDemux * demux, TSDemuxStream * stream,
    MpegTSPacketizerPacket * packet)
{
  guint8 *data;
  guint size;
  guint8 cc = FLAGS_CONTINUITY_COUNTER (packet->scram_afc_cc);

  GST_LOG_OBJECT (demux, "pid: 0x%04x state:%d", stream->stream.pid,
      stream->state);

  size = packet->data_end - packet->payload;
  data = packet->payload;

  if (stream->continuity_counter == CONTINUITY_UNSET) {
    GST_DEBUG_OBJECT (demux, "CONTINUITY: Initialize to %d", cc);
  } else if ((cc == stream->continuity_counter + 1 ||
          (stream->continuity_counter == MAX_CONTINUITY && cc == 0))) {
    GST_LOG_OBJECT (demux, "CONTINUITY: Got expected %d", cc);
  } else {
    if (stream->state != PENDING_PACKET_EMPTY) {
      if (packet->payload_unit_start_indicator) {
        /* A mismatch is fatal, except if this is the beginning of a new
         * frame (from which we can recover) */
        if (G_UNLIKELY (stream->data)) {
          g_free (stream->data);
          stream->data = NULL;
        }
        if (G_UNLIKELY (stream->pending_header_data)) {
          g_free (stream->pending_header_data);
          stream->pending_header_data = NULL;
        }
        stream->state = PENDING_PACKET_HEADER;
      } else {
        gchar *pad_name = gst_pad_get_name (stream->pad);
        GST_ELEMENT_WARNING_WITH_DETAILS (demux, STREAM, DEMUX,
            ("CONTINUITY: Mismatch packet %d, stream %d (pid 0x%04x)", cc,
                stream->continuity_counter, stream->stream.pid), (NULL),
            ("warning-type", G_TYPE_STRING, "continuity-mismatch",
                "packet", G_TYPE_INT, cc,
                "stream", G_TYPE_INT, stream->continuity_counter,
                "pid", G_TYPE_UINT, stream->stream.pid,
                "pad-name", G_TYPE_STRING, pad_name, NULL));
        g_free (pad_name);
        stream->state = PENDING_PACKET_DISCONT;
      }
    }
  }
  stream->continuity_counter = cc;

  if (stream->state == PENDING_PACKET_EMPTY) {
    if (G_UNLIKELY (!packet->payload_unit_start_indicator)) {
      stream->state = PENDING_PACKET_DISCONT;
      GST_DEBUG_OBJECT (demux, "Didn't get the first packet of this PES");
    } else {
      GST_LOG_OBJECT (demux, "EMPTY=>HEADER");
      stream->state = PENDING_PACKET_HEADER;
    }
  }

  switch (stream->state) {
    case PENDING_PACKET_HEADER:
    {
      GST_LOG_OBJECT (demux, "HEADER: Parsing PES header");

      /* parse the header */
      gst_ts_demux_parse_pes_header (demux, stream, data, size, packet->offset);
      break;
    }
    case PENDING_PACKET_BUFFER:
    {
      GST_LOG_OBJECT (demux, "BUFFER: appending data");
      if (G_UNLIKELY (stream->current_size + size > stream->allocated_size)) {
        GST_LOG_OBJECT (demux, "resizing buffer");
        do {
          stream->allocated_size = MAX (8192, 2 * stream->allocated_size);
        } while (stream->current_size + size > stream->allocated_size);
        stream->data = g_realloc (stream->data, stream->allocated_size);
      }
      memcpy (stream->data + stream->current_size, data, size);
      stream->current_size += size;
      break;
    }
    case PENDING_PACKET_DISCONT:
    {
      GST_LOG_OBJECT (demux, "DISCONT: not storing/pushing");
      if (G_UNLIKELY (stream->data)) {
        g_free (stream->data);
        stream->data = NULL;
      }
      if (G_UNLIKELY (stream->pending_header_data)) {
        g_free (stream->pending_header_data);
        stream->pending_header_data = NULL;
      }
      stream->continuity_counter = CONTINUITY_UNSET;
      break;
    }
    default:
      break;
  }

  return;
}

static void
calculate_and_push_newsegment (GstTSDemux * demux, TSDemuxStream * stream,
    MpegTSBaseProgram * target_program)
{
  MpegTSBase *base = (MpegTSBase *) demux;
  GstClockTime lowest_pts = GST_CLOCK_TIME_NONE;
  GstClockTime firstts = 0;
  GList *tmp;

  GST_DEBUG_OBJECT (demux, "Creating new newsegment for stream %p", stream);

  if (target_program == NULL)
    target_program = demux->program;

  /* Speedup : if we don't need to calculate anything, go straight to pushing */
  g_mutex_lock (&demux->lock);
  if (demux->segment_event) {
    g_mutex_unlock (&demux->lock);
    goto push_new_segment;
  }
  g_mutex_unlock (&demux->lock);

  /* Calculate the 'new_start' value, used for newsegment */
  for (tmp = target_program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *pstream = (TSDemuxStream *) tmp->data;

    if (GST_CLOCK_TIME_IS_VALID (pstream->first_pts)) {
      if (!GST_CLOCK_TIME_IS_VALID (lowest_pts)
          || pstream->first_pts < lowest_pts)
        lowest_pts = pstream->first_pts;
    }
  }
  if (GST_CLOCK_TIME_IS_VALID (lowest_pts))
    firstts = lowest_pts;
  GST_DEBUG_OBJECT (demux, "lowest_pts %" G_GUINT64_FORMAT " => clocktime %"
      GST_TIME_FORMAT, lowest_pts, GST_TIME_ARGS (firstts));

  if (base->out_segment.format != GST_FORMAT_TIME || demux->reset_segment) {
    /* It will happen only if it's first program or after flushes. */
    GST_DEBUG_OBJECT (demux, "Calculating actual segment");
    if (base->segment.format == GST_FORMAT_TIME) {
      /* Try to recover segment info from base if it's in TIME format */
      base->out_segment = base->segment;
    } else {
      /* Start from the first ts/pts */
      GstSegment *seg = &base->out_segment;
      GstClockTime base =
          seg->base + seg->position - (seg->start + seg->offset);
      GstClockTime stop = seg->stop;

      gst_segment_init (seg, GST_FORMAT_TIME);
      seg->start = firstts;
      seg->stop = MAX (seg->start, stop);
      seg->position = firstts;
      seg->time = firstts;
      seg->rate = demux->rate;
      seg->base = base;
    }
  } else if (base->out_segment.start < firstts) {
    /* Take into account the offset to the first buffer timestamp */
    if (base->out_segment.rate > 0) {
      if (GST_CLOCK_TIME_IS_VALID (base->out_segment.stop))
        base->out_segment.stop += firstts - base->out_segment.start;
      base->out_segment.start = firstts;
      base->out_segment.position = firstts;
    }
  }

  GST_LOG_OBJECT (demux, "Output segment now %" GST_SEGMENT_FORMAT,
      &base->out_segment);

  g_mutex_lock (&demux->lock);
  if (!demux->segment_event) {
    gst_event_take (&demux->segment_event,
        gst_event_new_segment (&base->out_segment));

    if (base->last_seek_seqnum != GST_SEQNUM_INVALID)
      gst_event_set_seqnum (demux->segment_event, base->last_seek_seqnum);
  }
  g_mutex_unlock (&demux->lock);

push_new_segment:
  for (tmp = target_program->stream_list; tmp; tmp = tmp->next) {
    stream = (TSDemuxStream *) tmp->data;
    if (stream->pad == NULL)
      continue;

    g_mutex_lock (&demux->lock);
    if (demux->segment_event) {
      GstEvent *evt = gst_event_ref (demux->segment_event);
      GST_DEBUG_OBJECT (stream->pad, "Pushing newsegment event");

      g_mutex_unlock (&demux->lock);
      gst_pad_push_event (stream->pad, evt);
    } else {
      g_mutex_unlock (&demux->lock);
    }

    if (demux->global_tags) {
      gst_pad_push_event (stream->pad,
          gst_event_new_tag (gst_tag_list_ref (demux->global_tags)));
    }

    /* Push pending tags */
    if (stream->taglist) {
      GST_DEBUG_OBJECT (stream->pad, "Sending tags %" GST_PTR_FORMAT,
          stream->taglist);
      gst_pad_push_event (stream->pad, gst_event_new_tag (stream->taglist));
      stream->taglist = NULL;
    }

    stream->need_newsegment = FALSE;
  }
  if (base->seek_event) {
    g_assert (base->out_segment.format != GST_FORMAT_UNDEFINED);
    gst_ts_demux_do_seek (base, base->seek_event);
    gst_event_replace (&base->seek_event, NULL);
  }
}

static void
gst_ts_demux_check_and_sync_streams (GstTSDemux * demux, GstClockTime time)
{
  GList *tmp;

  GST_DEBUG_OBJECT (demux,
      "Recheck streams and sync to at least: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (time));

  if (G_UNLIKELY (demux->program == NULL))
    return;

  /* Go over each stream and update it to at least 'time' time.
   * For each stream, the pad stores the buffer counter the last time
   * a gap check occurred (gap_ref_buffers) and a gap_ref_pts timestamp
   * that is either the PTS from the stream or the PCR the pad was updated
   * to.
   *
   * We can check nb_out_buffers to see if any buffers were pushed since then.
   * This means we can detect buffers passing without PTSes fine and still generate
   * gaps.
   *
   * If there haven't been any buffers pushed on this stream since the last gap
   * check *AND* there is no pending data (stream->current_size), push a gap
   * event updating to the indicated input PCR time and update the pad's
   * tracking.
   *
   * If there have been buffers pushed, update the reference buffer count
   * and but don't push a gap event
   */
  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *ps = (TSDemuxStream *) tmp->data;
    GST_DEBUG_OBJECT (ps->pad,
        "0x%04x, PTS:%" GST_TIME_FORMAT " REFPTS:%" GST_TIME_FORMAT " Gap:%"
        GST_TIME_FORMAT " nb_buffers: %d (ref:%d) pending_data size %u",
        ((MpegTSBaseStream *) ps)->pid, GST_TIME_ARGS (ps->pts),
        GST_TIME_ARGS (ps->gap_ref_pts),
        GST_TIME_ARGS (ps->pts - ps->gap_ref_pts), ps->nb_out_buffers,
        ps->gap_ref_buffers, ps->current_size);
    if (ps->pad == NULL)
      continue;

    if (ps->nb_out_buffers == ps->gap_ref_buffers && ps->current_size == 0
        && ps->gap_ref_pts != ps->pts) {
      /* Do initial setup of pad if needed - segment etc */
      GST_DEBUG_OBJECT (ps->pad,
          "Stream needs update. Pushing GAP event to TS %" GST_TIME_FORMAT,
          GST_TIME_ARGS (time));
      if (G_UNLIKELY (ps->need_newsegment))
        calculate_and_push_newsegment (demux, ps, NULL);

      /* Now send gap event */
      gst_pad_push_event (ps->pad, gst_event_new_gap (time, 0));

      /* And do an EOS check */
      MpegTSBase *base = GST_MPEGTS_BASE (demux);
      if (base->out_segment.stop != -1 && time >= base->out_segment.stop) {
        gst_flow_combiner_update_pad_flow (demux->flowcombiner, ps->pad,
            GST_FLOW_EOS);
      }
    }

    /* Update GAP tracking vars so we don't re-check this stream for a while */
    ps->gap_ref_pts = time;
    if (ps->pts != GST_CLOCK_TIME_NONE && ps->pts > time)
      ps->gap_ref_pts = ps->pts;
    ps->gap_ref_buffers = ps->nb_out_buffers;
  }
}

static GstBufferList *
parse_opus_access_unit (TSDemuxStream * stream)
{
  GstByteReader reader;
  GstBufferList *buffer_list = NULL;

  buffer_list = gst_buffer_list_new ();
  gst_byte_reader_init (&reader, stream->data, stream->current_size);

  do {
    GstBuffer *buffer;
    guint16 id;
    guint au_size = 0;
    guint8 b;
    gboolean start_trim_flag, end_trim_flag, control_extension_flag;
    guint16 start_trim = 0, end_trim = 0;
    guint8 *packet_data;
    guint packet_size;

    if (!gst_byte_reader_get_uint16_be (&reader, &id))
      goto error;

    /* No control header */
    if ((id >> 5) != 0x3ff)
      goto error;

    do {
      if (!gst_byte_reader_get_uint8 (&reader, &b))
        goto error;
      au_size += b;
    } while (b == 0xff);

    start_trim_flag = (id >> 4) & 0x1;
    end_trim_flag = (id >> 3) & 0x1;
    control_extension_flag = (id >> 2) & 0x1;

    if (start_trim_flag) {
      if (!gst_byte_reader_get_uint16_be (&reader, &start_trim))
        goto error;
    }

    if (end_trim_flag) {
      if (!gst_byte_reader_get_uint16_be (&reader, &end_trim))
        goto error;
    }

    if (control_extension_flag) {
      if (!gst_byte_reader_get_uint8 (&reader, &b))
        goto error;

      if (!gst_byte_reader_skip (&reader, b))
        goto error;
    }

    packet_size = au_size;

    /* FIXME: this should be
     *   packet_size = au_size - gst_byte_reader_get_pos (&reader);
     * but ffmpeg and the only available sample stream from obe.tv
     * are not including the control header size in au_size
     */
    if (gst_byte_reader_get_remaining (&reader) < packet_size)
      goto error;
    if (!gst_byte_reader_dup_data (&reader, packet_size, &packet_data))
      goto error;

    buffer = gst_buffer_new_wrapped (packet_data, packet_size);

    if (start_trim != 0 || end_trim != 0) {
      gst_buffer_add_audio_clipping_meta (buffer, GST_FORMAT_DEFAULT,
          start_trim, end_trim);
    }

    gst_buffer_list_add (buffer_list, buffer);
  } while (gst_byte_reader_get_remaining (&reader) > 0);

  g_free (stream->data);
  stream->data = NULL;
  stream->current_size = 0;

  return buffer_list;

error:
  {
    GST_ERROR ("Failed to parse Opus access unit");
    g_free (stream->data);
    stream->data = NULL;
    stream->current_size = 0;
    if (buffer_list)
      gst_buffer_list_unref (buffer_list);
    return NULL;
  }
}

/* interlaced mode is disabled at the moment */
/*#define TSDEMUX_JP2K_SUPPORT_INTERLACE */
static GstBuffer *
parse_jp2k_access_unit (TSDemuxStream * stream)
{
  GstByteReader reader;
  /* header tag */
  guint32 header_tag;
  /* Framerate box */
  guint16 den G_GNUC_UNUSED;
  guint16 num G_GNUC_UNUSED;
  /* Maximum bitrate box */
  guint32 MaxBr G_GNUC_UNUSED;
  guint32 AUF[2] = { 0, 0 };
#ifdef TSDEMUX_JP2K_SUPPORT_INTERLACE
  /* Field Coding Box */
  guint8 Fic G_GNUC_UNUSED = 1;
  guint8 Fio G_GNUC_UNUSED = 0;
  /* header size equals 38 for non-interlaced, and 48 for interlaced */
  guint header_size = stream->jp2kInfos.interlace ? 48 : 38;
#else
  /* header size equals 38 for non-interlaced, and 48 for interlaced */
  guint header_size = 38;
#endif
  /* Time Code box */
  guint32 HHMMSSFF G_GNUC_UNUSED;
  /* Broadcast color box */
  guint8 CollC G_GNUC_UNUSED;
  guint8 b G_GNUC_UNUSED;

  guint data_location;
  GstBuffer *retbuf = NULL;

  if (stream->current_size < header_size) {
    GST_ERROR_OBJECT (stream->pad, "Not enough data for header");
    goto error;
  }

  gst_byte_reader_init (&reader, stream->data, stream->current_size);

  /* Check for the location of the jp2k magic */
  data_location =
      gst_byte_reader_masked_scan_uint32 (&reader, 0xffffffff, 0xff4fff51, 0,
      stream->current_size);
  GST_DEBUG_OBJECT (stream->pad, "data location %d", data_location);
  if (data_location == -1) {
    GST_ERROR_OBJECT (stream->pad, "Stream does not contain jp2k magic header");
    goto error;
  }

  /* Elementary stream header box 'elsm' == 0x656c736d */
  header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (header_tag != 0x656c736d) {
    GST_ERROR_OBJECT (stream->pad, "Expected ELSM box but found box %x instead",
        header_tag);
    goto error;
  }
  /* Frame rate box 'frat' == 0x66726174 */
  header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (header_tag != 0x66726174) {
    GST_ERROR_OBJECT (stream->pad,
        "Expected frame rate box, but found box %x instead", header_tag);
    goto error;

  }
  den = gst_byte_reader_get_uint16_be_unchecked (&reader);
  num = gst_byte_reader_get_uint16_be_unchecked (&reader);
  /* Maximum bit rate box 'brat' == 0x62726174 */
  header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (header_tag != 0x62726174) {
    GST_ERROR_OBJECT (stream->pad, "Expected brat box but read box %x instead",
        header_tag);
    goto error;

  }
  MaxBr = gst_byte_reader_get_uint32_be_unchecked (&reader);
  AUF[0] = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (stream->jp2kInfos.interlace) {
#ifdef TSDEMUX_JP2K_SUPPORT_INTERLACE
    AUF[1] = gst_byte_reader_get_uint32_be_unchecked (&reader);
    /*  Field Coding Box 'fiel' == 0x6669656c */
    header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
    if (header_tag != 0x6669656c) {
      GST_ERROR_OBJECT (stream->pad,
          "Expected Field Coding box but found box %x instead", header_tag);
      goto error;
    }
    Fic = gst_byte_reader_get_uint8_unchecked (&reader);
    Fio = gst_byte_reader_get_uint8_unchecked (&reader);
#else
    GST_ERROR_OBJECT (stream->pad, "interlaced mode not supported");
    goto error;
#endif
  }

  /* Time Code Box 'tcod' == 0x74636f64 */
  /* Some progressive streams might have a AUF[1] of value 0 present */
  header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (header_tag == 0 && !stream->jp2kInfos.interlace) {
    AUF[1] = header_tag;
    header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
    /* Bump up header size and recheck */
    header_size += 4;
    if (stream->current_size < header_size) {
      GST_ERROR_OBJECT (stream->pad, "Not enough data for header");
      goto error;
    }
  }
  if (header_tag != 0x74636f64) {
    GST_ERROR_OBJECT (stream->pad,
        "Expected Time code box but found %d box instead", header_tag);
    goto error;
  }
  HHMMSSFF = gst_byte_reader_get_uint32_be_unchecked (&reader);
  /* Broadcast Color Box 'bcol' == 0x6263686c */
  header_tag = gst_byte_reader_get_uint32_be_unchecked (&reader);
  if (header_tag != 0x62636f6c) {
    GST_ERROR_OBJECT (stream->pad,
        "Expected Broadcast color box but found %x box instead", header_tag);
    goto error;
  }
  CollC = gst_byte_reader_get_uint8_unchecked (&reader);
  b = gst_byte_reader_get_uint8_unchecked (&reader);

  /* Check if we have enough data to create a valid buffer */
  if ((stream->current_size - data_location) < (AUF[0] + AUF[1])) {
    GST_ERROR_OBJECT
        (stream->pad,
        "Required size (%d) greater than remaining size in buffer (%d)",
        AUF[0] + AUF[1], (stream->current_size - data_location));
    if (stream->expected_size && stream->current_size != stream->expected_size) {
      /* warn if buffer is truncated due to draining */
      GST_WARNING_OBJECT
          (stream->pad,
          "Truncated buffer: current size (%d) doesn't match expected size (%d)",
          stream->current_size, stream->expected_size);
    } else {
      /* kill pipeline if either we don't know what expected size is, or
       * we know the expected size, and thus are sure that the buffer is not
       *  truncated due to draining */
      goto error;
    }
  }

  retbuf = gst_buffer_new_wrapped_full (0, stream->data, stream->current_size,
      data_location, stream->current_size - data_location,
      stream->data, g_free);
  stream->data = NULL;
  stream->current_size = 0;
  return retbuf;

error:
  GST_ERROR ("Failed to parse JP2K access unit");
  g_free (stream->data);
  stream->data = NULL;
  stream->current_size = 0;
  return NULL;
}

static GstBuffer *
parse_aac_adts_frame (TSDemuxStream * stream)
{
  gint data_location = -1;
  guint frame_len;
  guint crc_size;
  guint mpegversion = 4;
  gint i;

  if (stream->current_size < 6) {
    GST_DEBUG_OBJECT (stream->pad, "Not enough data for header");
    goto out;
  }

  /* check syncword */
  for (i = 0; i < stream->current_size - 2; i++) {
    if ((stream->data[i] == 0xff) && ((stream->data[i + 1] & 0xf6) == 0xf0)) {
      data_location = i;
      break;
    }
  }

  GST_TRACE_OBJECT (stream->pad, "data location %d", data_location);

  if (data_location == -1) {
    GST_DEBUG_OBJECT (stream->pad, "Stream does not contain adts syncword");
    goto out;
  }

  if (stream->current_size - data_location < 6) {
    GST_DEBUG_OBJECT (stream->pad, "Not enough data for header");
    goto out;
  }

  frame_len = ((stream->data[data_location + 3] & 0x03) << 11) |
      (stream->data[data_location + 4] << 3) | ((stream->data[data_location +
              5] & 0xe0) >> 5);

  crc_size = (stream->data[data_location + 1] & 0x01) ? 0 : 2;

  if (frame_len < 7 + crc_size) {
    GST_DEBUG_OBJECT (stream->pad, "Invalid frame len %d", frame_len);
    goto out;
  }

  /* this seems to be valid adts header, check mpeg version now
   *
   * TODO: check channels, rate, and profile and then update caps too?
   */
  mpegversion = (stream->data[data_location + 1] & 0x08) ? 2 : 4;

out:
  if (mpegversion != stream->atdsInfos.mpegversion) {
    GstCaps *caps;
    MpegTSBaseStream *bstream = (MpegTSBaseStream *) stream;

    GST_DEBUG_OBJECT (stream->pad, "Update mpegversion from %d to %d",
        stream->atdsInfos.mpegversion, mpegversion);
    stream->atdsInfos.mpegversion = mpegversion;

    caps = gst_stream_get_caps (bstream->stream_object);
    caps = gst_caps_make_writable (caps);

    gst_caps_set_simple (caps, "mpegversion", G_TYPE_INT, mpegversion, NULL);
    gst_stream_set_caps (bstream->stream_object, caps);
    gst_pad_set_caps (stream->pad, caps);
    gst_caps_unref (caps);
  }

  return gst_buffer_new_wrapped (stream->data, stream->current_size);
}

static GstBufferList *
parse_pes_metadata_frame (TSDemuxStream * stream)
{
  GstByteReader reader;
  GstBufferList *buffer_list = NULL;

  buffer_list = gst_buffer_list_new ();
  gst_byte_reader_init (&reader, stream->data, stream->current_size);

  do {
    GstBuffer *buffer;
    GstMpegtsPESMetadataMeta *meta;
    guint8 *au_data;
    guint16 au_size;
    guint8 service_id;
    guint8 sequence_number;
    guint8 flags;

    if (gst_byte_reader_get_remaining (&reader) <
        PES_PACKET_METADATA_AU_HEADER_LEN)
      goto error;

    if (!gst_byte_reader_get_uint8 (&reader, &service_id))
      goto error;

    if (!gst_byte_reader_get_uint8 (&reader, &sequence_number))
      goto error;

    if (!gst_byte_reader_get_uint8 (&reader, &flags))
      goto error;

    if (!gst_byte_reader_get_uint16_be (&reader, &au_size))
      goto error;

    if (gst_byte_reader_get_remaining (&reader) < au_size)
      goto error;

    if (!gst_byte_reader_dup_data (&reader, au_size, &au_data))
      goto error;

    buffer = gst_buffer_new_wrapped (au_data, au_size);
    meta = gst_buffer_add_mpegts_pes_metadata_meta (buffer);
    meta->metadata_service_id = service_id;
    meta->flags = flags;
    GST_DEBUG_OBJECT (stream->pad,
        "metadata_service_id: 0x%02x, flags: 0x%02x, cell_data_length: 0x%04x",
        meta->metadata_service_id, meta->flags, au_size);

    gst_buffer_list_add (buffer_list, buffer);
  } while (gst_byte_reader_get_remaining (&reader) > 0);

  g_free (stream->data);
  stream->data = NULL;
  stream->current_size = 0;

  return buffer_list;

error:
  {
    GST_ERROR ("Failed to parse PES metadata access units");
    g_free (stream->data);
    stream->data = NULL;
    stream->current_size = 0;
    if (buffer_list)
      gst_buffer_list_unref (buffer_list);
    return NULL;
  }
}

static GstFlowReturn
gst_ts_demux_push_pending_data (GstTSDemux * demux, TSDemuxStream * stream,
    MpegTSBaseProgram * target_program)
{
  MpegTSBase *base = GST_MPEGTS_BASE (demux);
  GstFlowReturn res = GST_FLOW_OK;
  MpegTSBaseStream *bs = (MpegTSBaseStream *) stream;
  GstBuffer *buffer = NULL;
  GstBufferList *buffer_list = NULL;


  GST_DEBUG_OBJECT (stream->pad,
      "stream:%p, pid:0x%04x stream_type:%d state:%d", stream, bs->pid,
      bs->stream_type, stream->state);

  if (G_UNLIKELY (stream->data == NULL)) {
    GST_LOG_OBJECT (stream->pad, "stream->data == NULL");
    goto beach;
  }

  if (G_UNLIKELY (stream->state == PENDING_PACKET_EMPTY)) {
    GST_LOG_OBJECT (stream->pad, "EMPTY: returning");
    goto beach;
  }

  if (G_UNLIKELY (stream->state != PENDING_PACKET_BUFFER)) {
    GST_LOG_OBJECT (stream->pad, "state:%d, returning", stream->state);
    goto beach;
  }

  if (G_UNLIKELY (demux->program == NULL)) {
    GST_LOG_OBJECT (demux, "No program");
    g_free (stream->data);
    goto beach;
  }

  if (stream->needs_keyframe) {
    MpegTSBase *base = (MpegTSBase *) demux;

    if ((gst_ts_demux_adjust_seek_offset_for_keyframe (stream, stream->data,
                stream->current_size)) || demux->last_seek_offset == 0) {
      GST_DEBUG_OBJECT (stream->pad,
          "Got Keyframe, ready to go at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (stream->pts));

      if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS &&
          bs->registration_id == DRF_ID_OPUS) {
        buffer_list = parse_opus_access_unit (stream);
      } else if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_VIDEO_JP2K) {
        buffer = parse_jp2k_access_unit (stream);
      } else if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_METADATA_PES_PACKETS
          && bs->registration_id == DRF_ID_KLVA) {
        buffer_list = parse_pes_metadata_frame (stream);
      } else {
        buffer = gst_buffer_new_wrapped (stream->data, stream->current_size);
      }

      if (buffer == NULL && buffer_list == NULL) {
        res = GST_FLOW_ERROR;
        goto beach;
      }

      stream->seeked_pts = stream->pts;
      stream->seeked_dts = stream->dts;
      stream->needs_keyframe = FALSE;
    } else {
      GList *tmp;
      GST_DEBUG_OBJECT (stream->pad, "Rewinding after keyframe seek failure");
      base->seek_offset = demux->last_seek_offset - 200 * base->packetsize;
      if (demux->last_seek_offset < 200 * base->packetsize)
        base->seek_offset = 0;
      demux->last_seek_offset = base->seek_offset;
      mpegts_packetizer_flush (base->packetizer, FALSE);

      /* Reset all streams accordingly */
      for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
        TSDemuxStream *cand = tmp->data;

        GST_DEBUG_OBJECT (cand->pad, "Clearing stream");
        cand->continuity_counter = CONTINUITY_UNSET;
        cand->state = PENDING_PACKET_EMPTY;
        if (cand->data)
          g_free (cand->data);
        cand->data = NULL;
        cand->allocated_size = 0;
        cand->current_size = 0;
      }
      base->mode = BASE_MODE_SEEKING;

      res = GST_FLOW_REWINDING;
      goto beach;
    }
  } else {
    if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS &&
        bs->registration_id == DRF_ID_OPUS) {
      buffer_list = parse_opus_access_unit (stream);
    } else if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_VIDEO_JP2K) {
      buffer = parse_jp2k_access_unit (stream);
    } else if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_AUDIO_AAC_ADTS) {
      buffer = parse_aac_adts_frame (stream);
    } else if (bs->stream_type == GST_MPEGTS_STREAM_TYPE_METADATA_PES_PACKETS
        && bs->registration_id == DRF_ID_KLVA) {
      buffer_list = parse_pes_metadata_frame (stream);
    } else {
      buffer = gst_buffer_new_wrapped (stream->data, stream->current_size);
    }
    if (buffer == NULL && buffer_list == NULL) {
      res = GST_FLOW_ERROR;
      goto beach;
    }

    if (G_UNLIKELY (stream->pending_ts && !check_pending_buffers (demux))) {
      if (buffer) {
        PendingBuffer *pend;
        pend = g_new0 (PendingBuffer, 1);
        pend->buffer = buffer;
        pend->pts = stream->raw_pts;
        pend->dts = stream->raw_dts;
        stream->pending = g_list_append (stream->pending, pend);
      } else {
        guint i, n;

        n = gst_buffer_list_length (buffer_list);
        for (i = 0; i < n; i++) {
          PendingBuffer *pend;
          pend = g_new0 (PendingBuffer, 1);
          pend->buffer = gst_buffer_ref (gst_buffer_list_get (buffer_list, i));
          pend->pts = i == 0 ? stream->raw_pts : -1;
          pend->dts = i == 0 ? stream->raw_dts : -1;
          stream->pending = g_list_append (stream->pending, pend);
        }
        gst_buffer_list_unref (buffer_list);
      }
      GST_DEBUG_OBJECT (demux,
          "Not enough information to push buffers yet, storing buffer");
      goto beach;
    }
  }


  if (buffer_list != NULL && gst_buffer_list_length (buffer_list) == 1) {
    buffer = gst_buffer_ref (gst_buffer_list_get (buffer_list, 0));
    gst_buffer_list_unref (buffer_list);
    buffer_list = NULL;
  }

  if (G_UNLIKELY (stream->need_newsegment))
    calculate_and_push_newsegment (demux, stream, target_program);

  /* FIXME : Push pending buffers if any */
  if (G_UNLIKELY (stream->pending)) {
    GList *tmp;
    for (tmp = stream->pending; tmp; tmp = tmp->next) {
      PendingBuffer *pend = (PendingBuffer *) tmp->data;

      GST_DEBUG_OBJECT (stream->pad,
          "Pushing pending buffer PTS:%" GST_TIME_FORMAT " DTS:%"
          GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (pend->buffer)),
          GST_TIME_ARGS (GST_BUFFER_DTS (pend->buffer)));

      if (stream->discont)
        GST_BUFFER_FLAG_SET (pend->buffer, GST_BUFFER_FLAG_DISCONT);
      stream->discont = FALSE;

      res = gst_pad_push (stream->pad, pend->buffer);
      stream->nb_out_buffers += 1;
      g_free (pend);
    }
    g_list_free (stream->pending);
    stream->pending = NULL;
  }

  if ((GST_CLOCK_TIME_IS_VALID (stream->seeked_pts)
          && stream->pts < stream->seeked_pts) ||
      (GST_CLOCK_TIME_IS_VALID (stream->seeked_dts) &&
          stream->pts < stream->seeked_dts)) {
    GST_INFO_OBJECT (stream->pad,
        "Droping with PTS: %" GST_TIME_FORMAT " DTS: %" GST_TIME_FORMAT
        " after seeking as other stream needed to be seeked further"
        "(seeked PTS: %" GST_TIME_FORMAT " DTS: %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (stream->pts), GST_TIME_ARGS (stream->dts),
        GST_TIME_ARGS (stream->seeked_pts), GST_TIME_ARGS (stream->seeked_dts));
    if (buffer)
      gst_buffer_unref (buffer);
    if (buffer_list)
      gst_buffer_list_unref (buffer_list);
    goto beach;
  }

  GST_DEBUG_OBJECT (stream->pad, "stream->pts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (stream->pts));

  /* Decorate buffer or first buffer of the buffer list */
  if (buffer_list)
    buffer = gst_buffer_list_get (buffer_list, 0);

  if (GST_CLOCK_TIME_IS_VALID (stream->pts))
    GST_BUFFER_PTS (buffer) = GST_BUFFER_DTS (buffer) = stream->pts;
  /* DTS = PTS by default, we override it if there's a real DTS */
  if (GST_CLOCK_TIME_IS_VALID (stream->dts))
    GST_BUFFER_DTS (buffer) = stream->dts;

  if (stream->discont)
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  stream->discont = FALSE;

  if (buffer_list)
    buffer = NULL;

  GST_DEBUG_OBJECT (stream->pad,
      "Pushing buffer%s with PTS: %" GST_TIME_FORMAT " , DTS: %"
      GST_TIME_FORMAT, (buffer_list ? "list" : ""), GST_TIME_ARGS (stream->pts),
      GST_TIME_ARGS (stream->dts));

  if (GST_CLOCK_TIME_IS_VALID (stream->dts)) {
    if (stream->dts > base->out_segment.position)
      base->out_segment.position = stream->dts;
  } else if (GST_CLOCK_TIME_IS_VALID (stream->pts)) {
    if (stream->pts > base->out_segment.position)
      base->out_segment.position = stream->pts;
  }

  if (buffer) {
    res = gst_pad_push (stream->pad, buffer);
    /* Record that a buffer was pushed */
    stream->nb_out_buffers += 1;
  } else {
    guint n = gst_buffer_list_length (buffer_list);
    res = gst_pad_push_list (stream->pad, buffer_list);
    /* Record that a buffer was pushed */
    stream->nb_out_buffers += n;
  }

  /* If this proceeded past the end of the configured segment, mark it as EOS */
  if (res == GST_FLOW_OK) {
    /* If the pad returned anything other than GST_FLOW_OK, the flow combiner
     * will already handle that, we only need to check for EOS */
    GstClockTime ts = stream->dts;
    if (!GST_CLOCK_TIME_IS_VALID (ts))
      ts = stream->pts;

    if (base->out_segment.stop != -1 &&
        GST_CLOCK_TIME_IS_VALID (ts) && ts >= base->out_segment.stop) {
      GST_DEBUG_OBJECT (stream->pad, "DTS %" GST_TIMEP_FORMAT
          " is past the segment stop %" GST_TIMEP_FORMAT ". Marking pad as EOS",
          &ts, &base->out_segment.stop);
      res = GST_FLOW_EOS;
    }
  }
  GST_DEBUG_OBJECT (stream->pad, "Returned %s", gst_flow_get_name (res));
  res =
      gst_flow_combiner_update_pad_flow (demux->flowcombiner, stream->pad, res);
  GST_DEBUG_OBJECT (stream->pad, "combined %s", gst_flow_get_name (res));

  /* GAP / sparse stream tracking */
  if (G_UNLIKELY (stream->gap_ref_pts == GST_CLOCK_TIME_NONE))
    stream->gap_ref_pts = stream->pts;
  else {
    /* Look if the stream PTS has advanced 2 seconds since the last
     * gap check, and sync streams if it has. The first stream to
     * hit this will trigger a gap check */
    if (G_UNLIKELY (stream->pts != GST_CLOCK_TIME_NONE &&
            stream->pts > stream->gap_ref_pts + 2 * GST_SECOND)) {
      if (demux->program->pcr_pid != 0x1fff) {
        GstClockTime curpcr =
            mpegts_packetizer_get_current_time (MPEG_TS_BASE_PACKETIZER (demux),
            demux->program->pcr_pid);
        if (curpcr == GST_CLOCK_TIME_NONE || curpcr < 800 * GST_MSECOND)
          goto beach;
        curpcr -= 800 * GST_MSECOND;
        /* Use the current PCR (with a safety margin) to sync against */
        gst_ts_demux_check_and_sync_streams (demux, curpcr);
      } else {
        /* If we don't have a PCR track, just use the current stream PTS */
        gst_ts_demux_check_and_sync_streams (demux, stream->pts);
      }
    }
  }

beach:
  /* Reset the PES payload collection, but don't clear the state,
   * we might want to keep collecting this PES */
  GST_LOG_OBJECT (demux, "Cleared PES data. returning %s",
      gst_flow_get_name (res));
  if (stream->expected_size) {
    if (stream->current_size > stream->expected_size)
      stream->expected_size = 0;
    else
      stream->expected_size -= stream->current_size;
  }
  stream->data = NULL;
  stream->allocated_size = 0;
  stream->current_size = 0;

  return res;
}

static GstFlowReturn
gst_ts_demux_handle_packet (GstTSDemux * demux, TSDemuxStream * stream,
    MpegTSPacketizerPacket * packet, GstMpegtsSection * section)
{
  GstFlowReturn res = GST_FLOW_OK;

  GST_LOG_OBJECT (demux, "pid 0x%04x pusi:%d, afc:%d, cont:%d, payload:%p",
      packet->pid, packet->payload_unit_start_indicator,
      packet->scram_afc_cc & 0x30,
      FLAGS_CONTINUITY_COUNTER (packet->scram_afc_cc), packet->payload);

  if (G_UNLIKELY (packet->payload_unit_start_indicator) &&
      FLAGS_HAS_PAYLOAD (packet->scram_afc_cc)) {
    /* Flush previous data */
    res = gst_ts_demux_push_pending_data (demux, stream, NULL);
    if (res != GST_FLOW_REWINDING) {
      /* Tell the data collecting to expect this header. We don't do this when
       * rewinding since the states will have been resetted accordingly */
      stream->state = PENDING_PACKET_HEADER;
    }
  }

  if (packet->payload && (res == GST_FLOW_OK || res == GST_FLOW_NOT_LINKED)
      && stream->pad) {
    gst_ts_demux_queue_data (demux, stream, packet);
    GST_LOG_OBJECT (demux, "current_size:%d, expected_size:%d",
        stream->current_size, stream->expected_size);
    /* Finally check if the data we queued completes a packet, or got too
     * large and needs output now */
    if ((stream->expected_size && stream->current_size >= stream->expected_size)
        || (stream->current_size >= MAX_PES_PAYLOAD)) {
      GST_LOG_OBJECT (demux, "pushing packet of size %u", stream->current_size);
      res = gst_ts_demux_push_pending_data (demux, stream, NULL);
    }
  }

  /* We are rewinding to find a keyframe,
   * and didn't want the data to be queued
   */
  if (res == GST_FLOW_REWINDING)
    res = GST_FLOW_OK;

  return res;
}

static void
gst_ts_demux_flush (MpegTSBase * base, gboolean hard)
{
  GstTSDemux *demux = GST_TS_DEMUX_CAST (base);

  gst_ts_demux_flush_streams (demux, hard);

  g_mutex_lock (&demux->lock);
  gst_event_replace (&demux->segment_event, NULL);
  g_mutex_unlock (&demux->lock);
  if (demux->global_tags) {
    gst_tag_list_unref (demux->global_tags);
    demux->global_tags = NULL;
  }
  if (hard) {
    /* For pull mode seeks the current segment needs to be preserved */
    demux->rate = 1.0;
    gst_segment_init (&base->out_segment, GST_FORMAT_UNDEFINED);
  }
}

static GstFlowReturn
gst_ts_demux_drain (MpegTSBase * base)
{
  GstTSDemux *demux = GST_TS_DEMUX_CAST (base);
  GList *tmp;
  GstFlowReturn res = GST_FLOW_OK;

  if (!demux->program)
    return res;

  for (tmp = demux->program->stream_list; tmp; tmp = tmp->next) {
    TSDemuxStream *stream = (TSDemuxStream *) tmp->data;
    if (stream->pad) {
      res = gst_ts_demux_push_pending_data (demux, stream, NULL);
      if (G_UNLIKELY (res != GST_FLOW_OK))
        break;
    }
  }

  return res;
}

static GstFlowReturn
gst_ts_demux_push (MpegTSBase * base, MpegTSPacketizerPacket * packet,
    GstMpegtsSection * section)
{
  GstTSDemux *demux = GST_TS_DEMUX_CAST (base);
  TSDemuxStream *stream = NULL;
  GstFlowReturn res = GST_FLOW_OK;

  if (G_LIKELY (demux->program)) {
    stream = (TSDemuxStream *) demux->program->streams[packet->pid];

    if (stream) {
      res = gst_ts_demux_handle_packet (demux, stream, packet, section);
    }
  }
  return res;
}
