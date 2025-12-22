 /*
  * This library is licensed under 2 different licenses and you
  * can choose to use it under the terms of either one of them. The
  * two licenses are the MPL 1.1 and the LGPL.
  *
  * MPL:
  *
  * The contents of this file are subject to the Mozilla Public License
  * Version 1.1 (the "License"); you may not use this file except in
  * compliance with the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/.
  *
  * Software distributed under the License is distributed on an "AS IS"
  * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  * License for the specific language governing rights and limitations
  * under the License.
  *
  * LGPL:
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
  *
  * The Original Code is Fluendo MPEG Demuxer plugin.
  *
  * The Initial Developer of the Original Code is Fluendo, S.L.
  * Portions created by Fluendo, S.L. are Copyright (C) 2005
  * Fluendo, S.L. All Rights Reserved.
  *
  * Contributor(s): Wim Taymans <wim@fluendo.com>
  *                 Jan Schmidt <thaytan@noraisin.net>
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/tag/tag.h>
#include <gst/pbutils/pbutils.h>
#include <gst/base/gstbytereader.h>

#include "gstmpegdefs.h"
#include "gstmpegdemux.h"

#define BLOCK_SZ                    32768
#define SCAN_SCR_SZ                 12
#define SCAN_PTS_SZ                 80

#define DEFAULT_GAP_THRESHOLD (300*GST_MSECOND)
#define VIDEO_GAP_THRESHOLD   (500*GST_MSECOND)

#define DURATION_SCAN_LIMIT         4 * 1024 * 1024

typedef enum
{
  SCAN_SCR,
  SCAN_DTS,
  SCAN_PTS
} SCAN_MODE;

/* We clamp scr delta with 0 so negative bytes won't be possible */
#define GSTTIME_TO_BYTES(time) \
  ((time != -1) ? gst_util_uint64_scale (MAX(0,(gint64) (GSTTIME_TO_MPEGTIME(time))), demux->scr_rate_n, demux->scr_rate_d) : -1)
#define BYTES_TO_GSTTIME(bytes) ((bytes != -1) ? MPEGTIME_TO_GSTTIME(gst_util_uint64_scale (bytes, demux->scr_rate_d, demux->scr_rate_n)) : -1)

#define ADAPTER_OFFSET_FLUSH(_bytes_) demux->adapter_offset += (_bytes_)

GST_DEBUG_CATEGORY_STATIC (gstflupsdemux_debug);
#define GST_CAT_DEFAULT (gstflupsdemux_debug)

GST_DEBUG_CATEGORY_EXTERN (mpegpspesfilter_debug);

/* MPEG2Demux signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_IGNORE_SCR,
  /* FILL ME */
};

#define DEFAULT_IGNORE_SCR FALSE

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2 }, "
        "systemstream = (boolean) TRUE;" "video/x-cdxa")
    );

static GstStaticPadTemplate video_template =
    GST_STATIC_PAD_TEMPLATE ("video_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2, 4 }, " "systemstream = (boolean) FALSE, "
        "parsed = (boolean) FALSE; " "video/x-h264, "
        "stream-format=(string)byte-stream; " "video/x-h265, "
        "stream-format=(string)byte-stream;")
    );

static GstStaticPadTemplate audio_template =
    GST_STATIC_PAD_TEMPLATE ("audio_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion = (int) 1;"
        "audio/mpeg, mpegversion = (int) 4, stream-format = (string) { adts, loas };"
        "audio/x-private1-lpcm; "
        "audio/x-private1-ac3;" "audio/x-private1-dts;" "audio/ac3")
    );

static GstStaticPadTemplate subpicture_template =
GST_STATIC_PAD_TEMPLATE ("subpicture_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("subpicture/x-dvd")
    );

static GstStaticPadTemplate private_template =
GST_STATIC_PAD_TEMPLATE ("private_%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static void gst_ps_demux_base_init (GstPsDemuxClass * klass);
static void gst_ps_demux_class_init (GstPsDemuxClass * klass);
static void gst_ps_demux_init (GstPsDemux * demux);
static void gst_ps_demux_finalize (GstPsDemux * demux);
static void gst_ps_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ps_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_ps_demux_reset (GstPsDemux * demux);

static gboolean gst_ps_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_ps_demux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_ps_demux_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_ps_demux_sink_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);
static void gst_ps_demux_loop (GstPad * pad);

static gboolean gst_ps_demux_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_ps_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static GstStateChangeReturn gst_ps_demux_change_state (GstElement * element,
    GstStateChange transition);

static inline gboolean gst_ps_demux_scan_forward_ts (GstPsDemux * demux,
    guint64 * pos, SCAN_MODE mode, guint64 * rts, gint limit);
static inline gboolean gst_ps_demux_scan_backward_ts (GstPsDemux * demux,
    guint64 * pos, SCAN_MODE mode, guint64 * rts, gint limit);

static inline void gst_ps_demux_send_gap_updates (GstPsDemux * demux,
    GstClockTime new_time);
static inline void gst_ps_demux_clear_times (GstPsDemux * demux);

static void gst_ps_demux_reset_psm (GstPsDemux * demux);
static void gst_ps_demux_flush (GstPsDemux * demux);

static GstElementClass *parent_class = NULL;

static void gst_segment_set_position (GstSegment * segment, GstFormat format,
    guint64 position);
static void gst_segment_set_duration (GstSegment * segment, GstFormat format,
    guint64 duration);

/*static guint gst_ps_demux_signals[LAST_SIGNAL] = { 0 };*/

GType
gst_ps_demux_get_type (void)
{
  static GType ps_demux_type = 0;

  if (!ps_demux_type) {
    static const GTypeInfo ps_demux_info = {
      sizeof (GstPsDemuxClass),
      (GBaseInitFunc) gst_ps_demux_base_init,
      NULL,
      (GClassInitFunc) gst_ps_demux_class_init,
      NULL,
      NULL,
      sizeof (GstPsDemux),
      0,
      (GInstanceInitFunc) gst_ps_demux_init,
      NULL
    };

    ps_demux_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstMpegPSDemux",
        &ps_demux_info, 0);

    GST_DEBUG_CATEGORY_INIT (gstflupsdemux_debug, "mpegpsdemux", 0,
        "MPEG program stream demultiplexer element");
  }

  return ps_demux_type;
}

GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mpegpsdemux, "mpegpsdemux",
    GST_RANK_PRIMARY, GST_TYPE_PS_DEMUX,
    GST_DEBUG_CATEGORY_INIT (mpegpspesfilter_debug, "mpegpspesfilter", 0,
        "MPEG-PS PES filter"));

static void
gst_ps_demux_base_init (GstPsDemuxClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  klass->sink_template = gst_static_pad_template_get (&sink_template);
  klass->video_template = gst_static_pad_template_get (&video_template);
  klass->audio_template = gst_static_pad_template_get (&audio_template);
  klass->subpicture_template =
      gst_static_pad_template_get (&subpicture_template);
  klass->private_template = gst_static_pad_template_get (&private_template);

  gst_element_class_add_pad_template (element_class, klass->video_template);
  gst_element_class_add_pad_template (element_class, klass->audio_template);
  gst_element_class_add_pad_template (element_class,
      klass->subpicture_template);
  gst_element_class_add_pad_template (element_class, klass->private_template);
  gst_element_class_add_pad_template (element_class, klass->sink_template);

  gst_element_class_set_static_metadata (element_class,
      "MPEG Program Stream Demuxer", "Codec/Demuxer",
      "Demultiplexes MPEG Program Streams", "Wim Taymans <wim@fluendo.com>");
}

static void
gst_ps_demux_class_init (GstPsDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = (GObjectFinalizeFunc) gst_ps_demux_finalize;
  gobject_class->set_property = gst_ps_demux_set_property;
  gobject_class->get_property = gst_ps_demux_get_property;

  gstelement_class->change_state = gst_ps_demux_change_state;

  /**
   * GstPsDemux:ignore-scr:
   *
   * Ignore SCR (System Clock Reference) data from MPEG-PS Pack Header.
   * This can help with playback of some broken files.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_IGNORE_SCR,
      g_param_spec_boolean ("ignore-scr", "Ignore SCR data for timing",
          "Ignore SCR data for timing", DEFAULT_IGNORE_SCR,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));
}

static void
gst_ps_demux_init (GstPsDemux * demux)
{
  GstPsDemuxClass *klass = GST_PS_DEMUX_GET_CLASS (demux);

  demux->sinkpad = gst_pad_new_from_template (klass->sink_template, "sink");
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_sink_event));
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_chain));
  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_sink_activate));
  gst_pad_set_activatemode_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_sink_activate_mode));

  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  demux->streams =
      g_malloc0 (sizeof (GstPsStream *) * (GST_PS_DEMUX_MAX_STREAMS));
  demux->streams_found =
      g_malloc0 (sizeof (GstPsStream *) * (GST_PS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;

  demux->adapter = gst_adapter_new ();
  demux->rev_adapter = gst_adapter_new ();
  demux->flowcombiner = gst_flow_combiner_new ();

  gst_ps_demux_reset (demux);

  demux->ignore_scr = DEFAULT_IGNORE_SCR;
}

static void
gst_ps_demux_finalize (GstPsDemux * demux)
{
  gst_ps_demux_reset (demux);
  g_free (demux->streams);
  g_free (demux->streams_found);

  gst_flow_combiner_free (demux->flowcombiner);
  g_object_unref (demux->adapter);
  g_object_unref (demux->rev_adapter);

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (demux));
}

static void
gst_ps_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPsDemux *demux = GST_PS_DEMUX (object);

  switch (prop_id) {
    case PROP_IGNORE_SCR:
      demux->ignore_scr = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_ps_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPsDemux *demux = GST_PS_DEMUX (object);

  switch (prop_id) {
    case PROP_IGNORE_SCR:
      g_value_set_boolean (value, demux->ignore_scr);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_ps_demux_reset (GstPsDemux * demux)
{
  /* Clean up the streams and pads we allocated */
  gint i;

  for (i = 0; i < GST_PS_DEMUX_MAX_STREAMS; i++) {
    GstPsStream *stream = demux->streams[i];

    if (stream != NULL) {
      if (stream->pad && GST_PAD_PARENT (stream->pad)) {
        gst_flow_combiner_remove_pad (demux->flowcombiner, stream->pad);
        gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
      } else {
        gst_object_unref (stream->pad);
      }

      if (stream->pending_tags)
        gst_tag_list_unref (stream->pending_tags);
      g_free (stream);
      demux->streams[i] = NULL;
    }
  }
  memset (demux->streams_found, 0,
      sizeof (GstPsStream *) * (GST_PS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;

  gst_adapter_clear (demux->adapter);
  gst_adapter_clear (demux->rev_adapter);

  demux->adapter_offset = G_MAXUINT64;
  demux->first_scr = G_MAXUINT64;
  demux->last_scr = G_MAXUINT64;
  demux->current_scr = G_MAXUINT64;
  demux->base_time = G_MAXUINT64;
  demux->scr_rate_n = G_MAXUINT64;
  demux->scr_rate_d = G_MAXUINT64;
  demux->first_pts = G_MAXUINT64;
  demux->last_pts = G_MAXUINT64;
  demux->mux_rate = G_MAXUINT64;
  demux->next_pts = G_MAXUINT64;
  demux->next_dts = G_MAXUINT64;
  demux->need_no_more_pads = TRUE;
  gst_ps_demux_reset_psm (demux);
  gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&demux->src_segment, GST_FORMAT_TIME);
  gst_ps_demux_flush (demux);
  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;
}

static GstPsStream *
gst_ps_demux_create_stream (GstPsDemux * demux, gint id, gint stream_type,
    gint layer)
{
  GstPsStream *stream;
  GstPadTemplate *template;
  gchar *name;
  GstPsDemuxClass *klass = GST_PS_DEMUX_GET_CLASS (demux);
  GstCaps *caps;
  GstClockTime gap_threshold = DEFAULT_GAP_THRESHOLD;
  GstEvent *event;
  gchar *stream_id;

  name = NULL;
  template = NULL;
  caps = NULL;

  GST_DEBUG_OBJECT (demux, "create stream id 0x%02x, type 0x%02x", id,
      stream_type);

  switch (stream_type) {
    case ST_VIDEO_MPEG1:
    case ST_VIDEO_MPEG2:
    case ST_VIDEO_MPEG4:
    case ST_GST_VIDEO_MPEG1_OR_2:
    {
      gint mpeg_version = 1;
      if (stream_type == ST_VIDEO_MPEG2 ||
          (stream_type == ST_GST_VIDEO_MPEG1_OR_2 && demux->is_mpeg2_pack)) {
        mpeg_version = 2;
      }
      if (stream_type == ST_VIDEO_MPEG4) {
        mpeg_version = 4;
      }

      template = klass->video_template;
      name = g_strdup_printf ("video_%02x", id);
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, mpeg_version,
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "parsed", G_TYPE_BOOLEAN, FALSE, NULL);
      gap_threshold = VIDEO_GAP_THRESHOLD;
      break;
    }
    case ST_AUDIO_MPEG1:
    case ST_AUDIO_MPEG2:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      if (layer) {
        caps = gst_caps_new_simple ("audio/mpeg",
            "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, layer, NULL);
      } else {
        caps = gst_caps_new_simple ("audio/mpeg",
            "mpegversion", G_TYPE_INT, 1, NULL);
      }
      break;
    case ST_PRIVATE_SECTIONS:
    case ST_PRIVATE_DATA:
    case ST_MHEG:
    case ST_DSMCC:
      break;
    case ST_AUDIO_AAC_ADTS:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "adts", NULL);
      break;
    case ST_AUDIO_AAC_LOAS:    // LATM/LOAS AAC syntax
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "stream-format", G_TYPE_STRING, "loas", NULL);
      break;
    case ST_VIDEO_H264:
      template = klass->video_template;
      name = g_strdup_printf ("video_%02x", id);
      caps = gst_caps_new_simple ("video/x-h264",
          "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      gap_threshold = VIDEO_GAP_THRESHOLD;
      break;
    case ST_VIDEO_H265:
      template = klass->video_template;
      name = g_strdup_printf ("video_%02x", id);
      caps = gst_caps_new_simple ("video/x-h265",
          "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      gap_threshold = VIDEO_GAP_THRESHOLD;
      break;

    case ST_PS_AUDIO_AC3:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_empty_simple ("audio/x-private1-ac3");
      break;
    case ST_PS_AUDIO_DTS:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_empty_simple ("audio/x-private1-dts");
      break;
    case ST_PS_AUDIO_LPCM:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_empty_simple ("audio/x-private1-lpcm");
      break;
    case ST_PS_DVD_SUBPICTURE:
      template = klass->subpicture_template;
      name = g_strdup_printf ("subpicture_%02x", id);
      caps = gst_caps_new_empty_simple ("subpicture/x-dvd");
      break;
    case ST_GST_AUDIO_RAWA52:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_empty_simple ("audio/ac3");
      break;
    default:
      break;
  }

  if (name == NULL || template == NULL || caps == NULL) {
    g_free (name);
    if (caps)
      gst_caps_unref (caps);
    return FALSE;
  }

  stream = g_new0 (GstPsStream, 1);
  stream->id = id;
  stream->discont = TRUE;
  stream->need_segment = TRUE;
  stream->notlinked = FALSE;
  stream->type = stream_type;
  stream->pending_tags = NULL;
  stream->pad = gst_pad_new_from_template (template, name);
  stream->gap_threshold = gap_threshold;
  stream->nb_out_buffers = 0;
  stream->gap_ref_buffers = 0;
  stream->gap_ref_pts = GST_CLOCK_TIME_NONE;

  gst_pad_set_event_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_src_event));
  gst_pad_set_query_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_ps_demux_src_query));
  gst_pad_use_fixed_caps (stream->pad);

  /* needed for set_caps to work */
  if (!gst_pad_set_active (stream->pad, TRUE)) {
    GST_WARNING_OBJECT (demux, "Failed to activate pad %" GST_PTR_FORMAT,
        stream->pad);
  }

  stream_id =
      gst_pad_create_stream_id_printf (stream->pad, GST_ELEMENT_CAST (demux),
      "%02x", id);

  event = gst_pad_get_sticky_event (demux->sinkpad, GST_EVENT_STREAM_START, 0);
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
  if (demux->have_group_id)
    gst_event_set_group_id (event, demux->group_id);

  gst_pad_push_event (stream->pad, event);
  g_free (stream_id);

  gst_pad_set_caps (stream->pad, caps);

  if (!stream->pending_tags)
    stream->pending_tags = gst_tag_list_new_empty ();
  gst_pb_utils_add_codec_description_to_tag_list (stream->pending_tags, NULL,
      caps);

  GST_DEBUG_OBJECT (demux, "create pad %s, caps %" GST_PTR_FORMAT, name, caps);
  gst_caps_unref (caps);
  g_free (name);

  return stream;
}

static GstPsStream *
gst_ps_demux_get_stream (GstPsDemux * demux, gint id, gint type, gint layer)
{
  GstPsStream *stream = demux->streams[id];

  if (stream == NULL) {
    if (!(stream = gst_ps_demux_create_stream (demux, id, type, layer)))
      goto unknown_stream;

    GST_DEBUG_OBJECT (demux, "adding pad for stream id 0x%02x type 0x%02x", id,
        type);

    demux->streams[id] = stream;
    demux->streams_found[demux->found_count++] = stream;

    if (demux->need_no_more_pads) {
      gst_element_add_pad (GST_ELEMENT (demux), stream->pad);
      gst_flow_combiner_add_pad (demux->flowcombiner, stream->pad);
    } else {
      /* only likely to confuse decodebin etc, so discard */
      /* FIXME should perform full switch protocol:
       * add a whole new set of pads, drop old and no-more-pads again */
      GST_DEBUG_OBJECT (demux,
          "but already signalled no-more-pads; not adding");
      gst_object_ref_sink (stream->pad);
    }
  }
  return stream;

  /* ERROR */
unknown_stream:
  {
    GST_DEBUG_OBJECT (demux, "unknown stream id 0x%02x type 0x%02x", id, type);
    return NULL;
  }
}

static GstPsStream *
gst_ps_demux_get_stream_from_pad (GstPsDemux * demux, GstPad * srcpad)
{
  gint i, count;

  count = demux->found_count;
  for (i = 0; i < count; i++) {
    GstPsStream *stream = demux->streams_found[i];

    if (stream && stream->pad == srcpad)
      return stream;
  }

  GST_DEBUG_OBJECT (srcpad, "no stream found for pad!");
  return NULL;
}

static inline void
gst_ps_demux_send_segment (GstPsDemux * demux, GstPsStream * stream,
    GstClockTime pts)
{
  /* discont */
  if (G_UNLIKELY (stream->need_segment)) {
    GstSegment segment;
    GstEvent *segment_event;

    GST_DEBUG ("PTS timestamp:%" GST_TIME_FORMAT " base_time %" GST_TIME_FORMAT
        " src_segment.start:%" GST_TIME_FORMAT " .stop:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (pts), GST_TIME_ARGS (demux->base_time),
        GST_TIME_ARGS (demux->src_segment.start),
        GST_TIME_ARGS (demux->src_segment.stop));

    /* we should be in sync with downstream, so start from our segment notion,
     * which also includes proper base_time etc, tweak it a bit and send */
    gst_segment_copy_into (&demux->src_segment, &segment);
    if (GST_CLOCK_TIME_IS_VALID (demux->base_time)) {
      if (GST_CLOCK_TIME_IS_VALID (segment.start))
        segment.start += demux->base_time;
      if (GST_CLOCK_TIME_IS_VALID (segment.stop))
        segment.stop += demux->base_time;
      segment.time = segment.start - demux->base_time;
    }

    segment_event = gst_event_new_segment (&segment);
    if (demux->segment_seqnum)
      gst_event_set_seqnum (segment_event, demux->segment_seqnum);
    else
      demux->segment_seqnum = gst_event_get_seqnum (segment_event);
    GST_INFO_OBJECT (demux, "sending segment event %" GST_SEGMENT_FORMAT
        " to pad %" GST_PTR_FORMAT, &segment, stream->pad);

    gst_pad_push_event (stream->pad, segment_event);

    stream->need_segment = FALSE;
  }

  if (G_UNLIKELY (stream->pending_tags)) {
    GST_DEBUG_OBJECT (demux, "Sending pending_tags %p for pad %s:%s : %"
        GST_PTR_FORMAT, stream->pending_tags,
        GST_DEBUG_PAD_NAME (stream->pad), stream->pending_tags);
    gst_pad_push_event (stream->pad, gst_event_new_tag (stream->pending_tags));
    stream->pending_tags = NULL;
  }
}

static GstFlowReturn
gst_ps_demux_send_data (GstPsDemux * demux, GstPsStream * stream,
    GstBuffer * buf)
{
  GstFlowReturn result;
  GstClockTime pts = GST_CLOCK_TIME_NONE, dts = GST_CLOCK_TIME_NONE;
  GstClockTime ts;

  if (stream == NULL)
    goto no_stream;

  /* timestamps */
  if (G_UNLIKELY (demux->next_pts != G_MAXUINT64))
    pts = MPEGTIME_TO_GSTTIME (demux->next_pts);
  if (G_UNLIKELY (demux->next_dts != G_MAXUINT64))
    dts = MPEGTIME_TO_GSTTIME (demux->next_dts);

  gst_ps_demux_send_segment (demux, stream, pts);

  /* Ignores DTS if PTS < DTS. Maybe additional sanity checking is possible
   * by comparing 33bits timestap rollover case, but PTS < DTS is already
   * invalid case */
  if (GST_CLOCK_TIME_IS_VALID (pts) && GST_CLOCK_TIME_IS_VALID (dts) &&
      dts > pts) {
    GST_WARNING_OBJECT (demux, "PTS (%" GST_TIME_FORMAT ") < DTS (%"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (pts), GST_TIME_ARGS (dts));
    dts = GST_CLOCK_TIME_NONE;
  }

  /* OK, sent new segment now prepare the buffer for sending */
  GST_BUFFER_PTS (buf) = pts;
  GST_BUFFER_DTS (buf) = dts;

  /* If we have no DTS but a PTS that means both are the same,
   * if we have neither than we don't know the current position */
  ts = dts;
  if (ts == GST_CLOCK_TIME_NONE)
    ts = pts;

  /* update position in the segment */
  if (ts != GST_CLOCK_TIME_NONE && (stream->last_ts == GST_CLOCK_TIME_NONE
          || stream->last_ts < ts)) {
    GST_LOG_OBJECT (demux,
        "last_ts update on pad %s to time %" GST_TIME_FORMAT
        ", current scr is %" GST_TIME_FORMAT, GST_PAD_NAME (stream->pad),
        GST_TIME_ARGS (ts),
        GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->current_scr)));
    stream->last_ts = ts;
    if (demux->src_segment.position == GST_CLOCK_TIME_NONE
        || stream->last_ts > demux->src_segment.position)
      gst_segment_set_position (&demux->src_segment, GST_FORMAT_TIME,
          stream->last_ts);
  }

  /* Set the buffer discont flag, and clear discont state on the stream */
  if (stream->discont) {
    GST_DEBUG_OBJECT (demux, "discont buffer to pad %" GST_PTR_FORMAT
        " with PTS %" GST_TIME_FORMAT " DTS %" GST_TIME_FORMAT,
        stream->pad, GST_TIME_ARGS (pts), GST_TIME_ARGS (dts));
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);

    stream->discont = FALSE;
  } else {
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);
  }

  demux->next_pts = G_MAXUINT64;
  demux->next_dts = G_MAXUINT64;

  GST_LOG_OBJECT (demux, "pushing stream id 0x%02x type 0x%02x, pts time: %"
      GST_TIME_FORMAT ", size %" G_GSIZE_FORMAT,
      stream->id, stream->type, GST_TIME_ARGS (pts), gst_buffer_get_size (buf));
  result = gst_pad_push (stream->pad, buf);
  stream->nb_out_buffers += 1;

  GST_LOG_OBJECT (demux, "result: %s", gst_flow_get_name (result));

  return result;

  /* ERROR */
no_stream:
  {
    GST_DEBUG_OBJECT (demux, "no stream given");
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }
}

static inline void
gst_ps_demux_mark_discont (GstPsDemux * demux, gboolean discont,
    gboolean need_segment)
{
  gint i, count = demux->found_count;

  /* mark discont on all streams */
  for (i = 0; i < count; i++) {
    GstPsStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->discont |= discont;
      stream->need_segment |= need_segment;
      if (need_segment)
        demux->segment_seqnum = 0;
      GST_DEBUG_OBJECT (demux, "marked stream as discont %d, need_segment %d",
          stream->discont, stream->need_segment);
    }
  }
}

static gboolean
gst_ps_demux_send_event (GstPsDemux * demux, GstEvent * event)
{
  gint i, count = demux->found_count;
  gboolean ret = FALSE;

  for (i = 0; i < count; i++) {
    GstPsStream *stream = demux->streams_found[i];

    if (stream) {
      if (!gst_pad_push_event (stream->pad, gst_event_ref (event))) {
        GST_DEBUG_OBJECT (stream->pad, "%s event was not handled",
            GST_EVENT_TYPE_NAME (event));
      } else {
        /* If at least one push returns TRUE, then we return TRUE. */
        GST_DEBUG_OBJECT (stream->pad, "%s event was handled",
            GST_EVENT_TYPE_NAME (event));
        ret = TRUE;
      }
    }
  }

  gst_event_unref (event);
  return ret;
}

static gboolean
gst_ps_demux_handle_dvd_event (GstPsDemux * demux, GstEvent * event)
{
  const GstStructure *structure = gst_event_get_structure (event);
  const char *type = gst_structure_get_string (structure, "event");
  gint i;
  gchar cur_stream_name[32];
  GstPsStream *temp = NULL;
  const gchar *lang_code;

  if (strcmp (type, "dvd-lang-codes") == 0) {
    GST_DEBUG_OBJECT (demux, "Handling language codes event");

    /* Create a video pad to ensure have it before emit no more pads */
    (void) gst_ps_demux_get_stream (demux, 0xe0, ST_VIDEO_MPEG2, 0);

    /* Read out the languages for audio streams and request each one that
     * is present */
    for (i = 0; i < MAX_DVD_AUDIO_STREAMS; i++) {
      gint stream_format;
      gint stream_id;

      g_snprintf (cur_stream_name, 32, "audio-%d-format", i);
      if (!gst_structure_get_int (structure, cur_stream_name, &stream_format))
        continue;

      g_snprintf (cur_stream_name, 32, "audio-%d-stream", i);
      if (!gst_structure_get_int (structure, cur_stream_name, &stream_id))
        continue;
      if (stream_id < 0 || stream_id >= MAX_DVD_AUDIO_STREAMS)
        continue;

      switch (stream_format) {
        case 0x0:
          /* AC3 */
          stream_id += 0x80;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - AC3", i,
              stream_format, stream_id);
          temp = gst_ps_demux_get_stream (demux, stream_id, ST_PS_AUDIO_AC3, 0);
          break;
        case 0x2:
        case 0x3:
          /* MPEG audio without and with extension stream are
           * treated the same */
          stream_id += 0xC0;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - MPEG audio", i,
              stream_format, stream_id);
          temp = gst_ps_demux_get_stream (demux, stream_id, ST_AUDIO_MPEG1, 0);
          break;
        case 0x4:
          /* LPCM */
          stream_id += 0xA0;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - DVD LPCM", i,
              stream_format, stream_id);
          temp =
              gst_ps_demux_get_stream (demux, stream_id, ST_PS_AUDIO_LPCM, 0);
          break;
        case 0x6:
          /* DTS */
          stream_id += 0x88;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - DTS", i,
              stream_format, stream_id);
          temp = gst_ps_demux_get_stream (demux, stream_id, ST_PS_AUDIO_DTS, 0);
          break;
        case 0x7:
          /* FIXME: What range is SDDS? */
        default:
          GST_WARNING_OBJECT (demux,
              "Unknown audio stream format in language code event: %d",
              stream_format);
          temp = NULL;
          continue;
      }

      if (temp == NULL)
        continue;

      g_snprintf (cur_stream_name, 32, "audio-%d-language", i);
      lang_code = gst_structure_get_string (structure, cur_stream_name);
      if (lang_code) {
        GstTagList *list = temp->pending_tags;

        if (!list)
          list = gst_tag_list_new_empty ();
        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
            GST_TAG_LANGUAGE_CODE, lang_code, NULL);
        temp->pending_tags = list;
      }
    }

    /* And subtitle streams */
    for (i = 0; i < MAX_DVD_SUBPICTURE_STREAMS; i++) {
      gint stream_id;

      g_snprintf (cur_stream_name, 32, "subpicture-%d-format", i);
      if (!gst_structure_get_int (structure, cur_stream_name, &stream_id))
        continue;

      g_snprintf (cur_stream_name, 32, "subpicture-%d-stream", i);
      if (!gst_structure_get_int (structure, cur_stream_name, &stream_id))
        continue;
      if (stream_id < 0 || stream_id >= MAX_DVD_SUBPICTURE_STREAMS)
        continue;

      GST_DEBUG_OBJECT (demux, "Subpicture stream %d ID 0x%02x", i,
          0x20 + stream_id);

      /* Retrieve the subpicture stream to force pad creation */
      temp = gst_ps_demux_get_stream (demux, 0x20 + stream_id,
          ST_PS_DVD_SUBPICTURE, 0);
      if (temp == NULL)
        continue;

      g_snprintf (cur_stream_name, 32, "subpicture-%d-language", i);
      lang_code = gst_structure_get_string (structure, cur_stream_name);
      if (lang_code) {
        GstTagList *list = temp->pending_tags;

        if (!list)
          list = gst_tag_list_new_empty ();
        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
            GST_TAG_LANGUAGE_CODE, lang_code, NULL);
        temp->pending_tags = list;
      }
    }

    GST_DEBUG_OBJECT (demux, "Created all pads from Language Codes event, "
        "signalling no-more-pads");

    gst_element_no_more_pads (GST_ELEMENT (demux));
    demux->need_no_more_pads = FALSE;
  } else {
    /* forward to all pads, e.g. dvd clut event */
    gst_event_ref (event);
    gst_ps_demux_send_event (demux, event);
  }

  gst_event_unref (event);
  return TRUE;
}

static void
gst_ps_demux_flush (GstPsDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "flushing demuxer");
  gst_adapter_clear (demux->adapter);
  gst_adapter_clear (demux->rev_adapter);
  gst_pes_filter_drain (&demux->filter);
  gst_ps_demux_clear_times (demux);
  demux->adapter_offset = G_MAXUINT64;
  demux->current_scr = G_MAXUINT64;
  demux->bytes_since_scr = 0;
}

static inline void
gst_ps_demux_clear_times (GstPsDemux * demux)
{
  gint i, count = demux->found_count;

  gst_flow_combiner_reset (demux->flowcombiner);
  /* Clear the last ts for all streams */
  for (i = 0; i < count; i++) {
    GstPsStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->last_ts = GST_CLOCK_TIME_NONE;
    }
  }
}

static inline void
gst_ps_demux_send_gap_updates (GstPsDemux * demux, GstClockTime time)
{
  GstClockTime base_time, stop;
  gint i, count = demux->found_count;
  GstEvent *event = NULL;

  if (time == GST_CLOCK_TIME_NONE)
    return;

  /* Advance all lagging streams by sending a gap event */
  if ((base_time = demux->base_time) == GST_CLOCK_TIME_NONE)
    base_time = 0;

  stop = demux->src_segment.stop;
  if (stop != GST_CLOCK_TIME_NONE)
    stop += base_time;

  if (time > stop)
    return;

  /* FIXME: Handle reverse playback */
  for (i = 0; i < count; i++) {
    GstPsStream *stream = demux->streams_found[i];

    if (stream) {
      if (stream->last_ts == GST_CLOCK_TIME_NONE ||
          stream->last_ts < demux->src_segment.start + base_time)
        stream->last_ts = demux->src_segment.start + base_time;

      if (stream->last_ts + stream->gap_threshold < time &&
          stream->nb_out_buffers == stream->gap_ref_buffers &&
          stream->gap_ref_pts != stream->last_ts) {
        /* should send segment info before gap event */
        gst_ps_demux_send_segment (demux, stream, GST_CLOCK_TIME_NONE);

        GST_LOG_OBJECT (demux,
            "Sending gap update to pad %s from time %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT, GST_PAD_NAME (stream->pad),
            GST_TIME_ARGS (stream->last_ts), GST_TIME_ARGS (time));
        event = gst_event_new_gap (stream->last_ts, time - stream->last_ts);
        gst_pad_push_event (stream->pad, event);
        stream->last_ts = time;
      }

      /* Update GAP tracking vars so we don't re-check this stream for a while */
      stream->gap_ref_pts = time;
      if (stream->last_ts != GST_CLOCK_TIME_NONE && stream->last_ts > time)
        stream->gap_ref_pts = stream->last_ts;
    }
  }
}

static inline gboolean
have_open_streams (GstPsDemux * demux)
{
  return (demux->streams_found[0] != NULL);
}

static gboolean
gst_ps_demux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstPsDemux *demux = GST_PS_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_ps_demux_send_event (demux, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_ps_demux_send_event (demux, event);
      gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
      gst_ps_demux_flush (demux);
      break;
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);
      gst_segment_copy_into (segment, &demux->sink_segment);

      GST_INFO_OBJECT (demux, "received segment %" GST_SEGMENT_FORMAT, segment);

      /* we need to emit a new segment */
      gst_ps_demux_mark_discont (demux, TRUE, TRUE);

      if (segment->format == GST_FORMAT_BYTES
          && demux->scr_rate_n != G_MAXUINT64
          && demux->scr_rate_d != G_MAXUINT64) {
        demux->src_segment.rate = segment->rate;
        demux->src_segment.applied_rate = segment->applied_rate;
        demux->src_segment.format = GST_FORMAT_TIME;
        demux->src_segment.start = BYTES_TO_GSTTIME (segment->start);
        demux->src_segment.stop = BYTES_TO_GSTTIME (segment->stop);
        demux->src_segment.time = BYTES_TO_GSTTIME (segment->time);
      } else if (segment->format == GST_FORMAT_TIME) {
        /* we expect our timeline (SCR, PTS) to match the one from upstream,
         * if not, will adjust with offset later on */
        gst_segment_copy_into (segment, &demux->src_segment);
      }

      gst_event_unref (event);

      break;
    }
    case GST_EVENT_EOS:
      GST_INFO_OBJECT (demux, "Received EOS");
      if (!gst_ps_demux_send_event (demux, event)
          && !have_open_streams (demux)) {
        GST_WARNING_OBJECT (demux, "EOS and no streams open");
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            ("Internal data stream error."), ("No valid streams detected"));
      }
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
      const GstStructure *structure = gst_event_get_structure (event);

      if (structure != NULL
          && gst_structure_has_name (structure, "application/x-gst-dvd")) {
        res = gst_ps_demux_handle_dvd_event (demux, event);
      } else {
        gst_ps_demux_send_event (demux, event);
      }
      break;
    }
    case GST_EVENT_CAPS:
      gst_event_unref (event);
      break;
    default:
      gst_ps_demux_send_event (demux, event);
      break;
  }

  return res;
}

static gboolean
gst_ps_demux_handle_seek_push (GstPsDemux * demux, GstEvent * event)
{
  gboolean res = FALSE;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gint64 bstart, bstop;
  GstEvent *bevent;

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  GST_DEBUG_OBJECT (demux, "seek event, rate: %f start: %" GST_TIME_FORMAT
      " stop: %" GST_TIME_FORMAT, rate, GST_TIME_ARGS (start),
      GST_TIME_ARGS (stop));

  if (format == GST_FORMAT_BYTES) {
    GST_DEBUG_OBJECT (demux, "seek not supported on format %d", format);
    goto not_supported;
  }

  GST_DEBUG_OBJECT (demux, "seek - trying directly upstream first");

  /* first try original format seek */
  (void) gst_event_ref (event);
  if ((res = gst_pad_push_event (demux->sinkpad, event)))
    goto done;

  if (format != GST_FORMAT_TIME) {
    /* From here down, we only support time based seeks */
    GST_DEBUG_OBJECT (demux, "seek not supported on format %d", format);
    goto not_supported;
  }

  /* We need to convert to byte based seek and we need a scr_rate for that. */
  if (demux->scr_rate_n == G_MAXUINT64 || demux->scr_rate_d == G_MAXUINT64) {
    GST_DEBUG_OBJECT (demux, "seek not possible, no scr_rate");
    goto not_supported;
  }

  GST_DEBUG_OBJECT (demux, "try with scr_rate interpolation");

  bstart = GSTTIME_TO_BYTES ((guint64) start);
  bstop = GSTTIME_TO_BYTES ((guint64) stop);

  GST_DEBUG_OBJECT (demux, "in bytes bstart %" G_GINT64_FORMAT " bstop %"
      G_GINT64_FORMAT, bstart, bstop);
  bevent = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags, start_type,
      bstart, stop_type, bstop);

  res = gst_pad_push_event (demux->sinkpad, bevent);

done:
  gst_event_unref (event);
  return res;

not_supported:
  {
    gst_event_unref (event);

    return FALSE;
  }
}

#define MAX_RECURSION_COUNT 100

/* Binary search for requested SCR */
static inline guint64
find_offset (GstPsDemux * demux, guint64 scr,
    guint64 min_scr, guint64 min_scr_offset,
    guint64 max_scr, guint64 max_scr_offset, int recursion_count)
{
  guint64 scr_rate_n = max_scr_offset - min_scr_offset;
  guint64 scr_rate_d = max_scr - min_scr;
  guint64 fscr = scr;
  guint64 offset;

  if (recursion_count > MAX_RECURSION_COUNT) {
    return -1;
  }

  offset = min_scr_offset +
      MIN (gst_util_uint64_scale (scr - min_scr, scr_rate_n,
          scr_rate_d), demux->sink_segment.stop);

  if (!gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &fscr, 0)) {
    gst_ps_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &fscr, 0);
  }

  if (fscr == scr || fscr == min_scr || fscr == max_scr) {
    return offset;
  }

  if (fscr < scr) {
    return find_offset (demux, scr, fscr, offset, max_scr, max_scr_offset,
        recursion_count + 1);
  } else {
    return find_offset (demux, scr, min_scr, min_scr_offset, fscr, offset,
        recursion_count + 1);
  }
}

static inline gboolean
gst_ps_demux_do_seek (GstPsDemux * demux, GstSegment * seeksegment)
{
  gboolean found;
  guint64 fscr, offset;
  guint64 scr = GSTTIME_TO_MPEGTIME (seeksegment->position + demux->base_time);

  /* In some clips the PTS values are completely unaligned with SCR values.
   * To improve the seek in that situation we apply a factor considering the
   * relationship between last PTS and last SCR */
  if (demux->last_scr > demux->last_pts)
    scr = gst_util_uint64_scale (scr, demux->last_scr, demux->last_pts);

  scr = MIN (demux->last_scr, scr);
  scr = MAX (demux->first_scr, scr);
  fscr = scr;

  GST_INFO_OBJECT (demux, "sink segment configured %" GST_SEGMENT_FORMAT
      ", trying to go at SCR: %" G_GUINT64_FORMAT, &demux->sink_segment, scr);

  offset =
      find_offset (demux, scr, demux->first_scr, demux->first_scr_offset,
      demux->last_scr, demux->last_scr_offset, 0);

  if (offset == (guint64) - 1) {
    return FALSE;
  }

  found = gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &fscr, 0);
  if (!found)
    found = gst_ps_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &fscr, 0);

  while (found && fscr < scr) {
    offset++;
    found = gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &fscr, 0);
  }

  while (found && fscr > scr && offset > 0) {
    offset--;
    found = gst_ps_demux_scan_backward_ts (demux, &offset, SCAN_SCR, &fscr, 0);
  }

  GST_INFO_OBJECT (demux, "doing seek at offset %" G_GUINT64_FORMAT
      " SCR: %" G_GUINT64_FORMAT " %" GST_TIME_FORMAT,
      offset, fscr, GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (fscr)));

  gst_segment_set_position (&demux->sink_segment, GST_FORMAT_BYTES, offset);

  return TRUE;
}

static gboolean
gst_ps_demux_handle_seek_pull (GstPsDemux * demux, GstEvent * event)
{
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate;
  gboolean update, flush, accurate;
  GstSegment seeksegment;
  GstClockTime first_pts = MPEGTIME_TO_GSTTIME (demux->first_pts);
  guint32 seek_seqnum = gst_event_get_seqnum (event);

  gst_event_parse_seek (event, &rate, &format, &flags,
      &start_type, &start, &stop_type, &stop);

  if (format != GST_FORMAT_TIME)
    goto wrong_format;

  GST_DEBUG_OBJECT (demux, "Seek requested start %" GST_TIME_FORMAT " stop %"
      GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* We need to convert to byte based seek and we need a scr_rate for that. */
  if (demux->scr_rate_n == G_MAXUINT64 || demux->scr_rate_d == G_MAXUINT64)
    goto no_scr_rate;

  flush = flags & GST_SEEK_FLAG_FLUSH;
  accurate = flags & GST_SEEK_FLAG_ACCURATE;

  /* keyframe = flags & GST_SEEK_FLAG_KEY_UNIT; *//* FIXME */

  if (flush) {
    GstEvent *event = gst_event_new_flush_start ();
    gst_event_set_seqnum (event, seek_seqnum);
    /* Flush start up and downstream to make sure data flow and loops are
       idle */
    demux->flushing = TRUE;
    gst_ps_demux_send_event (demux, event);
    gst_pad_push_event (demux->sinkpad, gst_event_new_flush_start ());
  } else {
    /* Pause the pulling task */
    gst_pad_pause_task (demux->sinkpad);
  }

  /* Take the stream lock */
  GST_PAD_STREAM_LOCK (demux->sinkpad);

  if (flush) {
    /* Stop flushing upstream we need to pull */
    demux->flushing = FALSE;
    gst_pad_push_event (demux->sinkpad, gst_event_new_flush_stop (TRUE));
  }

  /* Work on a copy until we are sure the seek succeeded. */
  memcpy (&seeksegment, &demux->src_segment, sizeof (GstSegment));

  GST_DEBUG_OBJECT (demux, "segment before configure %" GST_SEGMENT_FORMAT,
      &demux->src_segment);

  /* Apply the seek to our segment */
  if (!gst_segment_do_seek (&seeksegment, rate, format, flags,
          start_type, start, stop_type, stop, &update))
    goto seek_error;

  GST_DEBUG_OBJECT (demux, "seek segment configured %" GST_SEGMENT_FORMAT,
      &seeksegment);

  if (flush || seeksegment.position != demux->src_segment.position) {
    /* Do the actual seeking */
    if (!gst_ps_demux_do_seek (demux, &seeksegment)) {
      return FALSE;
    }
  }

  /* check the limits */
  if (seeksegment.rate > 0.0 && first_pts != G_MAXUINT64
      && seeksegment.start < first_pts - demux->base_time) {
    seeksegment.position = first_pts - demux->base_time;
    if (!accurate)
      seeksegment.start = seeksegment.position;
  }

  /* update the rate in our src segment */
  demux->sink_segment.rate = rate;

  GST_DEBUG_OBJECT (demux, "seek segment adjusted %" GST_SEGMENT_FORMAT,
      &seeksegment);

  if (flush) {
    GstEvent *event = gst_event_new_flush_stop (TRUE);
    /* Stop flushing, the sinks are at time 0 now */
    gst_event_set_seqnum (event, seek_seqnum);
    gst_ps_demux_send_event (demux, event);
  }

  if (flush || seeksegment.position != demux->src_segment.position) {
    gst_ps_demux_flush (demux);
  }

  /* Ok seek succeeded, take the newly configured segment */
  memcpy (&demux->src_segment, &seeksegment, sizeof (GstSegment));

  /* Notify about the start of a new segment */
  if (demux->src_segment.flags & GST_SEEK_FLAG_SEGMENT) {
    gst_element_post_message (GST_ELEMENT (demux),
        gst_message_new_segment_start (GST_OBJECT (demux),
            demux->src_segment.format, demux->src_segment.position));
  }

  /* Tell all the stream a new segment is needed */
  gst_ps_demux_mark_discont (demux, TRUE, TRUE);

  /* Update the segment_seqnum with the seek event seqnum */
  demux->segment_seqnum = seek_seqnum;

  gst_pad_start_task (demux->sinkpad,
      (GstTaskFunction) gst_ps_demux_loop, demux->sinkpad, NULL);

  GST_PAD_STREAM_UNLOCK (demux->sinkpad);

  gst_event_unref (event);
  return TRUE;

  /* ERRORS */
wrong_format:
  {
    GST_WARNING_OBJECT (demux, "we only support seeking in TIME or BYTES "
        "formats");
    gst_event_unref (event);
    return FALSE;
  }
no_scr_rate:
  {
    GST_WARNING_OBJECT (demux, "seek not possible, no scr_rate");
    gst_event_unref (event);
    return FALSE;
  }
seek_error:
  {
    GST_WARNING_OBJECT (demux, "couldn't perform seek");
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_ps_demux_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = FALSE;
  GstPsDemux *demux = GST_PS_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (demux->random_access) {
        res = gst_ps_demux_handle_seek_pull (demux, event);
      } else {
        res = gst_ps_demux_handle_seek_push (demux, event);
      }
      break;
    case GST_EVENT_RECONFIGURE:{
      GstPsStream *stream;

      stream = gst_ps_demux_get_stream_from_pad (demux, pad);
      if (stream != NULL)
        stream->notlinked = FALSE;

      gst_event_unref (event);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_push_event (demux->sinkpad, event);
      break;
  }

  return res;
}

static gboolean
gst_ps_demux_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = FALSE;
  GstPsDemux *demux = GST_PS_DEMUX (parent);

  GST_LOG_OBJECT (demux, "Have query of type %d on pad %" GST_PTR_FORMAT,
      GST_QUERY_TYPE (query), pad);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstClockTime pos;
      GstFormat format;

      /* See if upstream can immediately answer */
      res = gst_pad_peer_query (demux->sinkpad, query);
      if (res)
        break;

      gst_query_parse_position (query, &format, NULL);

      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (demux, "position not supported for format: %s",
            gst_format_get_name (format));
        goto not_supported;
      }

      pos = demux->src_segment.position - demux->src_segment.start;
      GST_LOG_OBJECT (demux, "Position %" GST_TIME_FORMAT, GST_TIME_ARGS (pos));

      gst_query_set_position (query, format, pos);
      res = TRUE;
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      gint64 duration;
      GstQuery *byte_query;

      gst_query_parse_duration (query, &format, NULL);

      if (G_LIKELY (format == GST_FORMAT_TIME &&
              GST_CLOCK_TIME_IS_VALID (demux->src_segment.duration))) {
        gst_query_set_duration (query, GST_FORMAT_TIME,
            demux->src_segment.duration);
        res = TRUE;
        break;
      }

      /* For any format other than bytes, see if upstream knows first */
      if (format == GST_FORMAT_BYTES) {
        GST_DEBUG_OBJECT (demux, "duration not supported for format: %s",
            gst_format_get_name (format));
        goto not_supported;
      }

      if (gst_pad_peer_query (demux->sinkpad, query)) {
        res = TRUE;
        break;
      }

      /* Upstream didn't know, so we can only answer TIME queries from
       * here on */
      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (demux, "duration not supported for format: %s",
            gst_format_get_name (format));
        goto not_supported;
      }

      if (demux->mux_rate == -1) {
        GST_DEBUG_OBJECT (demux, "duration not possible, no mux_rate");
        goto not_supported;
      }

      byte_query = gst_query_new_duration (GST_FORMAT_BYTES);

      if (!gst_pad_peer_query (demux->sinkpad, byte_query)) {
        GST_LOG_OBJECT (demux, "query on peer pad failed");
        gst_query_unref (byte_query);
        goto not_supported;
      }

      gst_query_parse_duration (byte_query, &format, &duration);
      gst_query_unref (byte_query);

      GST_LOG_OBJECT (demux,
          "query on peer pad reported bytes %" G_GUINT64_FORMAT, duration);

      duration = BYTES_TO_GSTTIME ((guint64) duration);

      GST_LOG_OBJECT (demux, "converted to time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (duration));

      gst_query_set_duration (query, GST_FORMAT_TIME, duration);
      res = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);

      res = TRUE;
      if (demux->random_access) {
        /* In pull mode we can seek in TIME format if we have the SCR */
        if (fmt != GST_FORMAT_TIME || demux->scr_rate_n == G_MAXUINT64
            || demux->scr_rate_d == G_MAXUINT64) {
          gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        } else {
          gint64 dur = -1;
          if (GST_CLOCK_TIME_IS_VALID (demux->src_segment.duration))
            dur = demux->src_segment.duration;
          gst_query_set_seeking (query, fmt, TRUE, 0, dur);
        }
      } else {
        if (fmt == GST_FORMAT_BYTES) {
          /* Seeking in BYTES format not supported at all */
          gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        } else {
          GstQuery *peerquery;
          gboolean seekable;

          /* Then ask upstream */
          res = gst_pad_peer_query (demux->sinkpad, query);
          if (res) {
            /* If upstream can handle seeks we're done, if it
             * can't we still have our TIME->BYTES conversion seek
             */
            gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
            if (seekable || fmt != GST_FORMAT_TIME)
              goto beach;
          }

          /* We can seek if upstream supports BYTES seeks and we
           * have the SCR
           */
          peerquery = gst_query_new_seeking (GST_FORMAT_BYTES);
          res = gst_pad_peer_query (demux->sinkpad, peerquery);
          if (!res || demux->scr_rate_n == G_MAXUINT64
              || demux->scr_rate_d == G_MAXUINT64) {
            gst_query_set_seeking (query, fmt, FALSE, -1, -1);
          } else {
            gst_query_parse_seeking (peerquery, NULL, &seekable, NULL, NULL);
            if (seekable)
              gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, -1);
            else
              gst_query_set_seeking (query, fmt, FALSE, -1, -1);
          }

          gst_query_unref (peerquery);
          res = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = demux->src_segment.format;

      start =
          gst_segment_to_stream_time (&demux->src_segment, format,
          demux->src_segment.start);
      if ((stop = demux->src_segment.stop) == -1)
        stop = demux->src_segment.duration;
      else
        stop = gst_segment_to_stream_time (&demux->src_segment, format, stop);

      gst_query_set_segment (query, demux->src_segment.rate, format, start,
          stop);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

beach:
  return res;
not_supported:
  return FALSE;
}

static void
gst_ps_demux_reset_psm (GstPsDemux * demux)
{
  gint i;

#define FILL_TYPE(start, stop, type)	\
  for (i=start; i <= stop; i++)			\
    demux->psm[i] = type;

  /* Initialize all fields to -1 first */
  FILL_TYPE (0x00, GST_PS_DEMUX_MAX_PSM - 1, -1);

  FILL_TYPE (0x20, 0x3f, ST_PS_DVD_SUBPICTURE);

  FILL_TYPE (0x80, 0x87, ST_PS_AUDIO_AC3);
  FILL_TYPE (0x88, 0x9f, ST_PS_AUDIO_DTS);
  FILL_TYPE (0xa0, 0xaf, ST_PS_AUDIO_LPCM);

  FILL_TYPE (0xc0, 0xdf, ST_AUDIO_MPEG1);
  FILL_TYPE (0xe0, 0xef, ST_GST_VIDEO_MPEG1_OR_2);

#undef FILL_TYPE
}

/* ISO/IEC 13818-1:
 * pack_header() {
 *     pack_start_code                                   32  bslbf  -+
 *     '01'                                               2  bslbf   |
 *     system_clock_reference_base [32..30]               3  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     system_clock_reference_base [29..15]              15  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     system_clock_reference_base [14..0]               15  bslbf   |
 *     marker_bit                                         1  bslbf   | 112 bits
 *     system_clock_reference_extension                   9  ubslbf  |
 *     marker_bit                                         1  bslbf   |
 *     program_mux_rate                                  22  ubslbf  |
 *     marker_bit                                         1  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     reserved                                           5  bslbf   |
 *     pack_stuffing_length                               3  ubslbf -+
 *
 *     for (i = 0; i < pack_stuffing_length; i++) {
 *         stuffing_byte '1111 1111'                      8  bslbf
 *     }
 *
 * 112 bits = 14 bytes, as max value for pack_stuffing_length is 7, then
 * in total it's needed 14 + 7 = 21 bytes.
 */
#define PACK_START_SIZE     21

static GstFlowReturn
gst_ps_demux_parse_pack_start (GstPsDemux * demux)
{
  const guint8 *data;
  guint length;
  guint32 scr1, scr2;
  guint64 scr, scr_adjusted, new_rate;
  guint64 scr_rate_n;
  guint64 scr_rate_d;
  guint avail = gst_adapter_available (demux->adapter);

  GST_LOG ("parsing pack start");

  if (G_UNLIKELY (avail < PACK_START_SIZE))
    goto need_more_data;

  data = gst_adapter_map (demux->adapter, PACK_START_SIZE);

  /* skip start code */
  data += 4;

  scr1 = GST_READ_UINT32_BE (data);
  scr2 = GST_READ_UINT32_BE (data + 4);

  /* fixed length to begin with, start code and two scr values */
  length = 8 + 4;

  /* start parsing the stream */
  if ((*data & 0xc0) == 0x40) {
    guint32 scr_ext;
    guint32 next32;
    guint8 stuffing_bytes;

    GST_LOG ("Found MPEG2 stream");
    demux->is_mpeg2_pack = TRUE;

    /* mpeg2 has more data */
    length += 2;

    /* :2=01 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 */

    /* check markers */
    if (G_UNLIKELY ((scr1 & 0xc4000400) != 0x44000400))
      goto lost_sync;

    scr = ((guint64) scr1 & 0x38000000) << 3;
    scr |= ((guint64) scr1 & 0x03fff800) << 4;
    scr |= ((guint64) scr1 & 0x000003ff) << 5;
    scr |= ((guint64) scr2 & 0xf8000000) >> 27;

    /* marker:1==1 ! scr_ext:9 ! marker:1==1 */
    if (G_UNLIKELY ((scr2 & 0x04010000) != 0x04010000))
      goto lost_sync;

    scr_ext = (scr2 & 0x03fe0000) >> 17;
    /* We keep the offset of this scr */
    demux->cur_scr_offset = demux->adapter_offset + 12;

    GST_LOG_OBJECT (demux, "SCR: 0x%08" G_GINT64_MODIFIER "x SCRE: 0x%08x",
        scr, scr_ext);

    if (scr_ext) {
      scr = (scr * 300 + scr_ext % 300) / 300;
    }
    /* SCR has been converted into units of 90Khz ticks to make it comparable
       to DTS/PTS, that also implies 1 tick rounding error */
    data += 6;
    /* PMR:22 ! :2==11 ! reserved:5 ! stuffing_len:3 */
    next32 = GST_READ_UINT32_BE (data);
    if (G_UNLIKELY ((next32 & 0x00000300) != 0x00000300))
      goto lost_sync;

    new_rate = (next32 & 0xfffffc00) >> 10;

    stuffing_bytes = (next32 & 0x07);
    GST_LOG_OBJECT (demux, "stuffing bytes: %d", stuffing_bytes);

    data += 4;
    length += stuffing_bytes;
    while (stuffing_bytes--) {
      if (*data++ != 0xff)
        goto lost_sync;
    }
  } else {
    GST_DEBUG ("Found MPEG1 stream");
    demux->is_mpeg2_pack = FALSE;

    /* check markers */
    if (G_UNLIKELY ((scr1 & 0xf1000100) != 0x21000100))
      goto lost_sync;

    if (G_UNLIKELY ((scr2 & 0x01800001) != 0x01800001))
      goto lost_sync;

    /* :4=0010 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 ! marker:1==1 */
    scr = ((guint64) scr1 & 0x0e000000) << 5;
    scr |= ((guint64) scr1 & 0x00fffe00) << 6;
    scr |= ((guint64) scr1 & 0x000000ff) << 7;
    scr |= ((guint64) scr2 & 0xfe000000) >> 25;

    /* We keep the offset of this scr */
    demux->cur_scr_offset = demux->adapter_offset + 8;

    /* marker:1==1 ! mux_rate:22 ! marker:1==1 */
    new_rate = (scr2 & 0x007ffffe) >> 1;

    data += 8;
  }

  if (demux->ignore_scr) {
    /* update only first/current_scr with raw scr value to start streaming
     * after parsing 2 seconds long data with no-more-pad */
    if (demux->first_scr == G_MAXUINT64) {
      demux->first_scr = scr;
      demux->first_scr_offset = demux->cur_scr_offset;
    }

    demux->current_scr = scr;

    goto out;
  }

  new_rate *= MPEG_MUX_RATE_MULT;

  /* scr adjusted is the new scr found + the colected adjustment */
  scr_adjusted = scr + demux->scr_adjust;

  GST_LOG_OBJECT (demux,
      "SCR: %" G_GINT64_FORMAT " (%" G_GINT64_FORMAT "), mux_rate %"
      G_GINT64_FORMAT ", GStreamer Time:%" GST_TIME_FORMAT,
      scr, scr_adjusted, new_rate,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME ((guint64) scr)));

  /* keep the first src in order to calculate delta time */
  if (G_UNLIKELY (demux->first_scr == G_MAXUINT64)) {
    gint64 diff;

    demux->first_scr = scr;
    demux->first_scr_offset = demux->cur_scr_offset;
    demux->base_time = MPEGTIME_TO_GSTTIME (demux->first_scr);
    GST_DEBUG_OBJECT (demux, "determined base_time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (demux->base_time));
    /* at begin consider the new_rate as the scr rate, bytes/clock ticks */
    scr_rate_n = new_rate;
    scr_rate_d = CLOCK_FREQ;
    /* our SCR timeline might have offset wrt upstream timeline */
    if (demux->sink_segment.format == GST_FORMAT_TIME) {
      if (demux->sink_segment.start > demux->base_time)
        diff = -(demux->sink_segment.start - demux->base_time);
      else
        diff = demux->base_time - demux->sink_segment.start;
      if (diff > GST_SECOND) {
        GST_DEBUG_OBJECT (demux, "diff of %" GST_TIME_FORMAT
            " wrt upstream start %" GST_TIME_FORMAT "; adjusting base",
            GST_TIME_ARGS (diff), GST_TIME_ARGS (demux->sink_segment.start));
        demux->base_time += diff;
      }
    }
  } else if (G_LIKELY (demux->first_scr_offset != demux->cur_scr_offset)) {
    /* estimate byte rate related to the SCR */
    scr_rate_n = demux->cur_scr_offset - demux->first_scr_offset;
    scr_rate_d = scr_adjusted - demux->first_scr;
  } else {
    scr_rate_n = demux->scr_rate_n;
    scr_rate_d = demux->scr_rate_d;
  }

  GST_LOG_OBJECT (demux, "%s mode scr: %" G_GUINT64_FORMAT " at %"
      G_GUINT64_FORMAT ", first scr: %" G_GUINT64_FORMAT
      " at %" G_GUINT64_FORMAT ", scr rate: %" G_GUINT64_FORMAT
      "/%" G_GUINT64_FORMAT "(%f)",
      ((demux->sink_segment.rate >= 0.0) ? "forward" : "backward"),
      scr, demux->cur_scr_offset,
      demux->first_scr, demux->first_scr_offset,
      scr_rate_n, scr_rate_d, (float) scr_rate_n / scr_rate_d);

  /* adjustment of the SCR */
  if (G_LIKELY (demux->current_scr != G_MAXUINT64)) {
    guint64 diff;
    guint64 old_scr, old_mux_rate, bss, adjust = 0;

    /* keep SCR of the previous packet */
    old_scr = demux->current_scr;
    old_mux_rate = demux->mux_rate;

    /* Bytes since SCR is the amount we placed in the adapter since then
     * (demux->bytes_since_scr) minus the amount remaining in the adapter,
     * clamped to >= 0 */
    bss = MAX (0, (gint) (demux->bytes_since_scr - avail));

    /* estimate the new SCR using the previous one according the notes
       on point 2.5.2.2 of the ISO/IEC 13818-1 document */
    if (old_mux_rate != 0)
      adjust = (bss * CLOCK_FREQ) / old_mux_rate;

    if (demux->sink_segment.rate >= 0.0)
      demux->next_scr = old_scr + adjust;
    else
      demux->next_scr = old_scr - adjust;

    GST_LOG_OBJECT (demux,
        "bss: %" G_GUINT64_FORMAT ", next_scr: %" G_GUINT64_FORMAT
        ", old_scr: %" G_GUINT64_FORMAT ", scr: %" G_GUINT64_FORMAT,
        bss, demux->next_scr, old_scr, scr_adjusted);

    /* calculate the absolute deference between the last scr and
       the new one */
    if (G_UNLIKELY (old_scr > scr_adjusted))
      diff = old_scr - scr_adjusted;
    else
      diff = scr_adjusted - old_scr;

    /* if the difference is more than 1 second we need to reconfigure
       adjustment */
    if (G_UNLIKELY (diff > CLOCK_FREQ)) {
      demux->scr_adjust = demux->next_scr - scr;
      GST_LOG_OBJECT (demux, "discont found, diff: %" G_GINT64_FORMAT
          ", adjust %" G_GINT64_FORMAT, diff, demux->scr_adjust);
      scr_adjusted = demux->next_scr;
      /* don't update rate estimation on disconts */
      scr_rate_n = demux->scr_rate_n;
      scr_rate_d = demux->scr_rate_d;
    } else {
      demux->next_scr = scr_adjusted;
    }
  }

  /* update the current_scr and rate members */
  demux->mux_rate = new_rate;
  demux->current_scr = scr_adjusted;
  demux->scr_rate_n = scr_rate_n;
  demux->scr_rate_d = scr_rate_d;

  /* Reset the bytes_since_scr value to count the data remaining in the
   * adapter */
  demux->bytes_since_scr = avail;

  /* Now check for all streams if they're behind the new SCR and if
   * they are then move them forward to the SCR position */
  gst_ps_demux_send_gap_updates (demux,
      MPEGTIME_TO_GSTTIME (demux->current_scr - demux->first_scr));

out:
  gst_adapter_unmap (demux->adapter);
  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);

  return GST_FLOW_OK;

lost_sync:
  {
    GST_DEBUG_OBJECT (demux, "lost sync");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    return GST_FLOW_NEED_MORE_DATA;
  }
}

/* ISO/IEC 13818-1:
 * system_header () {
 *     system_header_start_code                          32  bslbf  -+
 *     header_length                                     16  uimsbf  |
 *     marker_bit                                         1  bslbf   |
 *     rate_bound                                        22  uimsbf  |
 *     marker_bit                                         1  bslbf   |
 *     audio_bound                                        6  uimsbf  |
 *     fixed_flag                                         1  bslbf   |
 *     CSPS_flag                                          1  bslbf   | 96 bits
 *     system_audio_lock_flag                             1  bslbf   |
 *     system_video_lock_flag                             1  bslbf   |
 *     marker_bit                                         1  bslbf   |
 *     video_bound                                        5  uimsbf  |
 *     packet_rate_restriction_flag                       1  bslbf   |
 *     reserved_bits                                      7  bslbf  -+
 *     while (nextbits () = = '1') {
 *         stream_id                                      8  uimsbf -+
 *         '11'                                           2  bslbf   | 24 bits
 *         P-STD_buffer_bound_scale                       1  bslbf   |
 *         P-STD_buffer_size_bound                       13  uimsbf -+
 *     }
 * }
 * 96 bits = 12 bytes, 24 bits = 3 bytes.
 */

static GstFlowReturn
gst_ps_demux_parse_sys_head (GstPsDemux * demux)
{
  guint16 length;
  const guint8 *data;
#ifndef GST_DISABLE_GST_DEBUG
  gboolean csps;
#endif

  if (gst_adapter_available (demux->adapter) < 6)
    goto need_more_data;

  /* start code + length */
  data = gst_adapter_map (demux->adapter, 6);

  /* skip start code */
  data += 4;

  length = GST_READ_UINT16_BE (data);
  GST_DEBUG_OBJECT (demux, "length %d", length);

  length += 6;

  gst_adapter_unmap (demux->adapter);
  if (gst_adapter_available (demux->adapter) < length)
    goto need_more_data;

  data = gst_adapter_map (demux->adapter, length);

  /* skip start code and length */
  data += 6;

  /* marker:1==1 ! rate_bound:22 | marker:1==1 */
  if ((*data & 0x80) != 0x80)
    goto marker_expected;

  {
    guint32 rate_bound;

    if ((data[2] & 0x01) != 0x01)
      goto marker_expected;

    rate_bound = ((guint32) data[0] & 0x7f) << 15;
    rate_bound |= ((guint32) data[1]) << 7;
    rate_bound |= ((guint32) data[2] & 0xfe) >> 1;
    rate_bound *= MPEG_MUX_RATE_MULT;

    GST_DEBUG_OBJECT (demux, "rate bound %u", rate_bound);

    data += 3;
  }

  /* audio_bound:6==1 ! fixed:1 | constrained:1 */
  {
#ifndef GST_DISABLE_GST_DEBUG
    guint8 audio_bound;
    gboolean fixed;

    /* max number of simultaneous audio streams active */
    audio_bound = (data[0] & 0xfc) >> 2;
    /* fixed or variable bitrate */
    fixed = (data[0] & 0x02) == 0x02;
    /* meeting constraints */
    csps = (data[0] & 0x01) == 0x01;

    GST_DEBUG_OBJECT (demux, "audio_bound %d, fixed %d, constrained %d",
        audio_bound, fixed, csps);
#endif
    data += 1;
  }

  /* audio_lock:1 | video_lock:1 | marker:1==1 | video_bound:5 */
  {
#ifndef GST_DISABLE_GST_DEBUG
    gboolean audio_lock;
    gboolean video_lock;
    guint8 video_bound;

    audio_lock = (data[0] & 0x80) == 0x80;
    video_lock = (data[0] & 0x40) == 0x40;
#endif

    if ((data[0] & 0x20) != 0x20)
      goto marker_expected;

#ifndef GST_DISABLE_GST_DEBUG
    /* max number of simultaneous video streams active */
    video_bound = (data[0] & 0x1f);

    GST_DEBUG_OBJECT (demux, "audio_lock %d, video_lock %d, video_bound %d",
        audio_lock, video_lock, video_bound);
#endif
    data += 1;
  }

  /* packet_rate_restriction:1 | reserved:7==0x7F */
  {
#ifndef GST_DISABLE_GST_DEBUG
    gboolean packet_rate_restriction;
#endif
    if ((data[0] & 0x7f) != 0x7f)
      goto marker_expected;
#ifndef GST_DISABLE_GST_DEBUG
    /* only valid if csps is set */
    if (csps) {
      packet_rate_restriction = (data[0] & 0x80) == 0x80;

      GST_DEBUG_OBJECT (demux, "packet_rate_restriction %d",
          packet_rate_restriction);
    }
#endif
  }
  data += 1;

  {
    gint stream_count = (length - 12) / 3;
    gint i;

    GST_DEBUG_OBJECT (demux, "number of streams: %d ", stream_count);

    for (i = 0; i < stream_count; i++) {
      guint8 stream_id;
#ifndef GST_DISABLE_GST_DEBUG
      gboolean STD_buffer_bound_scale;
      guint16 STD_buffer_size_bound;
      guint32 buf_byte_size_bound;
#endif
      stream_id = *data++;
      if (!(stream_id & 0x80))
        goto sys_len_error;

      /* check marker bits */
      if ((*data & 0xC0) != 0xC0)
        goto no_placeholder_bits;
#ifndef GST_DISABLE_GST_DEBUG
      STD_buffer_bound_scale = *data & 0x20;
      STD_buffer_size_bound = ((guint16) (*data++ & 0x1F)) << 8;
      STD_buffer_size_bound |= *data++;

      if (STD_buffer_bound_scale == 0) {
        buf_byte_size_bound = STD_buffer_size_bound * 128;
      } else {
        buf_byte_size_bound = STD_buffer_size_bound * 1024;
      }

      GST_DEBUG_OBJECT (demux, "STD_buffer_bound_scale %d",
          STD_buffer_bound_scale);
      GST_DEBUG_OBJECT (demux, "STD_buffer_size_bound %d or %d bytes",
          STD_buffer_size_bound, buf_byte_size_bound);
#endif
    }
  }

  gst_adapter_unmap (demux->adapter);
  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);
  return GST_FLOW_OK;

  /* ERRORS */
marker_expected:
  {
    GST_DEBUG_OBJECT (demux, "expecting marker");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_LOST_SYNC;
  }
no_placeholder_bits:
  {
    GST_DEBUG_OBJECT (demux, "expecting placeholder bit values"
        " '11' after stream id");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_LOST_SYNC;
  }
sys_len_error:
  {
    GST_DEBUG_OBJECT (demux, "error in system header length");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_NEED_MORE_DATA;
  }
}

static GstFlowReturn
gst_ps_demux_parse_psm (GstPsDemux * demux)
{
  guint16 psm_length, info_length = 0, es_map_length = 0;
  guint8 psm_version = 0;
  GstByteReader br;
#ifndef GST_DISABLE_GST_DEBUG
  gboolean applicable;
#endif

  /* Need at least 6 bytes for start code + length */
  if (gst_adapter_available (demux->adapter) < 6)
    goto need_more_data;

  {
    const guint8 *data;

    /* start code + length */
    data = gst_adapter_map (demux->adapter, 6);
    /* skip start code */
    data += 4;
    psm_length = GST_READ_UINT16_BE (data);
    GST_DEBUG_OBJECT (demux, "PSM length %u", psm_length);

    if (G_UNLIKELY (psm_length > 0x3FA))
      goto psm_len_error;
    psm_length += 6;            /* Add start code + size to length */

    gst_adapter_unmap (demux->adapter);

    if (gst_adapter_available (demux->adapter) < psm_length)
      goto need_more_data;

    data = gst_adapter_map (demux->adapter, psm_length);

    gst_byte_reader_init (&br, data, psm_length);
  }

  /* skip start code and length */
  if (!gst_byte_reader_skip (&br, 6))
    goto fail_invalid;

  /* Read PSM applicable bit together with version */
  if (!gst_byte_reader_get_uint8 (&br, &psm_version))
    goto fail_invalid;
#ifndef GST_DISABLE_GST_DEBUG
  applicable = (psm_version & 0x80) >> 7;
#endif
  psm_version &= 0x1F;
  GST_DEBUG_OBJECT (demux, "PSM version %u (applicable now %u)", psm_version,
      applicable);

  /* Jump over the next byte (marker bit) */
  if (!gst_byte_reader_skip (&br, 1))
    goto fail_invalid;

  /* Read PS info length */
  if (!gst_byte_reader_get_uint16_be (&br, &info_length))
    goto fail_invalid;
  GST_DEBUG_OBJECT (demux, "PS info length %u bytes", info_length);
  /* Skip the PS info, we don't use it */
  if (!gst_byte_reader_skip (&br, info_length))
    goto fail_invalid;

  /* Read ES map length */
  if (!gst_byte_reader_get_uint16_be (&br, &es_map_length))
    goto fail_invalid;
  GST_DEBUG_OBJECT (demux, "ES map length %u bytes", es_map_length);

  /* Now read the ES map */
  {
    GstByteReader es_map_br;
    if (!gst_byte_reader_get_sub_reader (&br, &es_map_br, es_map_length))
      goto fail_invalid;

    while (gst_byte_reader_get_remaining (&es_map_br) >= 4) {
      guint8 stream_type = 0, stream_id = 0;
      guint16 stream_info_length = 0;

      if (!gst_byte_reader_get_uint8 (&es_map_br, &stream_type) ||
          !gst_byte_reader_get_uint8 (&es_map_br, &stream_id) ||
          !gst_byte_reader_get_uint16_be (&es_map_br, &stream_info_length))
        break;

      GST_DEBUG_OBJECT (demux,
          "Stream type %02X with id %02X and %u bytes info", stream_type,
          stream_id, stream_info_length);

      if (G_LIKELY (stream_id != 0xbd))
        demux->psm[stream_id] = stream_type;
      else {
        /* Ignore stream type for private_stream_1 and discover it looking at
         * the stream data.
         * Fixes demuxing some clips with lpcm that was wrongly declared as
         * mpeg audio */
        GST_DEBUG_OBJECT (demux, "stream type for private_stream_1 ignored");
      }

      /* FIXME: We could use the descriptors instead of skipping them */
      if (!gst_byte_reader_skip (&es_map_br, stream_info_length))
        break;
    }
  }
  /* We ignore the 4-byte CRC at the end */

  gst_adapter_unmap (demux->adapter);
  gst_adapter_flush (demux->adapter, psm_length);
  ADAPTER_OFFSET_FLUSH (psm_length);
  return GST_FLOW_OK;

fail_invalid:
  GST_DEBUG_OBJECT (demux, "Failed to parse PSM. Skipping");
  gst_adapter_unmap (demux->adapter);
  gst_adapter_flush (demux->adapter, psm_length);
  ADAPTER_OFFSET_FLUSH (psm_length);
  return GST_FLOW_LOST_SYNC;
psm_len_error:
  {
    GST_DEBUG_OBJECT (demux, "error in PSM length");
    gst_adapter_unmap (demux->adapter);
    return GST_FLOW_LOST_SYNC;
  }
need_more_data:
  {
    GST_DEBUG_OBJECT (demux, "need more data");
    return GST_FLOW_NEED_MORE_DATA;
  }
}

static void
gst_ps_demux_resync_cb (GstPESFilter * filter, GstPsDemux * demux)
{
}

static GstFlowReturn
gst_ps_demux_data_cb (GstPESFilter * filter, gboolean first,
    GstBuffer * buffer, GstPsDemux * demux)
{
  GstBuffer *out_buf;
  GstFlowReturn ret = GST_FLOW_OK;
  gint stream_type;
  guint32 start_code;
  guint8 id;
  GstMapInfo map;
  gsize datalen;
  guint offset = 0;
  gst_buffer_map (buffer, &map, GST_MAP_READ);
  datalen = map.size;
  start_code = filter->start_code;
  id = filter->id;
  if (first) {
    gint layer = 0;
    /* find the stream type */
    stream_type = demux->psm[id];
    if (stream_type == -1) {
      /* no stream type, if PS1, get the new id */
      if (start_code == ID_PRIVATE_STREAM_1 && datalen >= 2) {
        /* VDR writes A52 streams without any header bytes
         * (see ftp://ftp.mplayerhq.hu/MPlayer/samples/MPEG-VOB/vdr-AC3) */
        if (datalen >= 4) {
          guint hdr = GST_READ_UINT32_BE (map.data);
          if (G_UNLIKELY ((hdr & 0xffff0000) == AC3_SYNC_WORD)) {
            id = 0x80;
            stream_type = demux->psm[id] = ST_GST_AUDIO_RAWA52;
            GST_DEBUG_OBJECT (demux, "Found VDR raw A52 stream");
          }
        }

        if (G_LIKELY (stream_type == -1)) {
          /* new id is in the first byte */
          id = map.data[offset++];
          datalen--;
          /* and remap */
          stream_type = demux->psm[id];
          /* Now, if it's a subpicture stream - no more, otherwise
           * take the first byte too, since it's the frame count in audio
           * streams and our backwards compat convention is to strip it off */
          if (stream_type != ST_PS_DVD_SUBPICTURE) {
            /* Number of audio frames in this packet */
#ifndef GST_DISABLE_GST_DEBUG
            guint8 nframes;
            nframes = map.data[offset];
            GST_LOG_OBJECT (demux, "private type 0x%02x, %d frames", id,
                nframes);
#endif
            offset++;
            datalen--;
          } else {
            GST_LOG_OBJECT (demux, "private type 0x%02x, stream type %d",
                id, stream_type);
          }
        }
      }
      if (stream_type == -1)
        goto unknown_stream_type;
    } else if (stream_type == ST_AUDIO_MPEG1 || stream_type == ST_AUDIO_MPEG2) {
      if (datalen >= 2) {
        guint hdr = GST_READ_UINT16_BE (map.data);
        if ((hdr & 0xfff0) == 0xfff0) {
          switch (hdr & 0x06) {
            case 0x6:
              layer = 1;
              break;
            case 0x4:
              layer = 2;
              break;
            case 0x2:
              layer = 3;
              break;
            default:
              GST_WARNING_OBJECT (demux, "unknown mpeg audio layer");
          }
        }
      }
    }

    if (filter->pts != -1) {
      demux->next_pts = filter->pts + demux->scr_adjust;
      GST_LOG_OBJECT (demux, "stream 0x%02x PTS = orig %" G_GUINT64_FORMAT
          " (%" G_GUINT64_FORMAT ")", id, filter->pts, demux->next_pts);
    } else
      demux->next_pts = G_MAXUINT64;
    if (filter->dts != -1) {
      demux->next_dts = filter->dts + demux->scr_adjust;
      GST_LOG_OBJECT (demux, "stream 0x%02x DTS = orig %" G_GUINT64_FORMAT
          " (%" G_GUINT64_FORMAT ")", id, filter->dts, demux->next_dts);
    } else {
      demux->next_dts = demux->next_pts;
    }

    demux->current_stream =
        gst_ps_demux_get_stream (demux, id, stream_type, layer);
  }

  if (G_UNLIKELY (demux->current_stream == NULL)) {
    GST_DEBUG_OBJECT (demux, "Dropping buffer for unknown stream id 0x%02x",
        id);
    goto done;
  }

  /* After 2 seconds of bitstream emit no more pads */
  if (demux->need_no_more_pads
      && (demux->current_scr - demux->first_scr) > 2 * CLOCK_FREQ) {
    GST_DEBUG_OBJECT (demux, "no more pads, notifying");
    gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
    demux->need_no_more_pads = FALSE;
  }

  /* If the stream is not-linked, don't bother creating a sub-buffer
   * to send to it, unless we're processing a discont (which resets
   * the not-linked status and tries again */
  if (demux->current_stream->discont) {
    GST_DEBUG_OBJECT (demux, "stream is discont");
    demux->current_stream->notlinked = FALSE;
  }

  if (demux->current_stream->notlinked == FALSE) {
    out_buf =
        gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, offset, datalen);
    ret = gst_ps_demux_send_data (demux, demux->current_stream, out_buf);
    if (ret == GST_FLOW_NOT_LINKED) {
      demux->current_stream->notlinked = TRUE;
    }
  }

done:
  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);
  return ret;
  /* ERRORS */
unknown_stream_type:
  {
    GST_DEBUG_OBJECT (demux, "unknown stream type %02x", id);
    ret = GST_FLOW_OK;
    goto done;
  }
}

static gboolean
gst_ps_demux_resync (GstPsDemux * demux, gboolean save)
{
  const guint8 *data;
  gint avail;
  guint32 code;
  gint offset;
  gboolean found;
  avail = gst_adapter_available (demux->adapter);
  if (G_UNLIKELY (avail < 4))
    goto need_data;
  /* Common case, read 4 bytes an check it */
  data = gst_adapter_map (demux->adapter, 4);
  /* read currect code */
  code = GST_READ_UINT32_BE (data);
  /* The common case is that the sync code is at 0 bytes offset */
  if (G_LIKELY ((code & 0xffffff00) == 0x100L)) {
    GST_LOG_OBJECT (demux, "Found resync code %08x after 0 bytes", code);
    demux->last_sync_code = code;
    gst_adapter_unmap (demux->adapter);
    return TRUE;
  }

  /* Otherwise, we are starting at byte 4 and we need to search
     the sync code in all available data in the adapter */
  offset = 4;
  if (offset >= avail)
    goto need_data;             /* Not enough data to find sync */
  data = gst_adapter_map (demux->adapter, avail);
  do {
    code = (code << 8) | data[offset++];
    found = (code & 0xffffff00) == 0x100L;
  } while (offset < avail && !found);
  gst_adapter_unmap (demux->adapter);
  if (!save || demux->sink_segment.rate >= 0.0) {
    GST_LOG_OBJECT (demux, "flushing %d bytes", offset - 4);
    /* forward playback, we can discard and flush the skipped bytes */
    gst_adapter_flush (demux->adapter, offset - 4);
    ADAPTER_OFFSET_FLUSH (offset - 4);
  } else {
    if (found) {
      GST_LOG_OBJECT (demux, "reverse saving %d bytes", offset - 4);
      /* reverse playback, we keep the flushed bytes and we will append them to
       * the next buffer in the chain function, which is the previous buffer in
       * the stream. */
      gst_adapter_push (demux->rev_adapter,
          gst_adapter_take_buffer (demux->adapter, offset - 4));
    } else {
      GST_LOG_OBJECT (demux, "reverse saving %d bytes", avail);
      /* nothing found, keep all bytes */
      gst_adapter_push (demux->rev_adapter,
          gst_adapter_take_buffer (demux->adapter, avail));
    }
  }

  if (found) {
    GST_LOG_OBJECT (demux, "Found resync code %08x after %d bytes",
        code, offset - 4);
    demux->last_sync_code = code;
  } else {
    GST_LOG_OBJECT (demux, "No resync after skipping %d", offset);
  }

  return found;
need_data:
  {
    GST_LOG_OBJECT (demux, "we need more data for resync %d", avail);
    return FALSE;
  }
}

static inline gboolean
gst_ps_demux_is_pes_sync (guint32 sync)
{
  return ((sync & 0xfc) == 0xbc) ||
      ((sync & 0xe0) == 0xc0) || ((sync & 0xf0) == 0xe0);
}

static inline gboolean
gst_ps_demux_scan_ts (GstPsDemux * demux, const guint8 * data,
    SCAN_MODE mode, guint64 * rts, const guint8 * end)
{
  gboolean ret = FALSE;
  guint32 scr1, scr2;
  guint64 scr;
  guint64 pts, dts;
  guint32 code;
  guint16 len;
  /* read the 4 bytes for the sync code */
  code = GST_READ_UINT32_BE (data);
  if (G_LIKELY (code != ID_PS_PACK_START_CODE))
    goto beach;
  if (data + 12 > end)
    goto beach;
  /* skip start code */
  data += 4;
  scr1 = GST_READ_UINT32_BE (data);
  scr2 = GST_READ_UINT32_BE (data + 4);
  /* start parsing the stream */
  if ((*data & 0xc0) == 0x40) {
    /* MPEG-2 PACK header */
    guint32 scr_ext;
    guint32 next32;
    guint8 stuffing_bytes;
    /* :2=01 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 */
    /* check markers */
    if ((scr1 & 0xc4000400) != 0x44000400)
      goto beach;
    scr = ((guint64) scr1 & 0x38000000) << 3;
    scr |= ((guint64) scr1 & 0x03fff800) << 4;
    scr |= ((guint64) scr1 & 0x000003ff) << 5;
    scr |= ((guint64) scr2 & 0xf8000000) >> 27;
    /* marker:1==1 ! scr_ext:9 ! marker:1==1 */
    if ((scr2 & 0x04010000) != 0x04010000)
      goto beach;
    scr_ext = (scr2 & 0x03fe0000) >> 17;
    if (scr_ext) {
      scr = (scr * 300 + scr_ext % 300) / 300;
    }
    /* SCR has been converted into units of 90Khz ticks to make it comparable
       to DTS/PTS, that also implies 1 tick rounding error */
    data += 6;

    if (data + 4 > end)
      goto beach;
    /* PMR:22 ! :2==11 ! reserved:5 ! stuffing_len:3 */
    next32 = GST_READ_UINT32_BE (data);
    if ((next32 & 0x00000300) != 0x00000300)
      goto beach;
    stuffing_bytes = (next32 & 0x07);
    data += 4;
    if (data + stuffing_bytes > end)
      goto beach;
    while (stuffing_bytes--) {
      if (*data++ != 0xff)
        goto beach;
    }
  } else {
    /* MPEG-1 pack header */
    /* check markers */
    if ((scr1 & 0xf1000100) != 0x21000100)
      goto beach;
    if ((scr2 & 0x01800001) != 0x01800001)
      goto beach;
    /* :4=0010 ! scr:3 ! marker:1==1 ! scr:15 ! marker:1==1 ! scr:15 ! marker:1==1 */
    scr = ((guint64) scr1 & 0x0e000000) << 5;
    scr |= ((guint64) scr1 & 0x00fffe00) << 6;
    scr |= ((guint64) scr1 & 0x000000ff) << 7;
    scr |= ((guint64) scr2 & 0xfe000000) >> 25;
    data += 8;
  }

  if (mode == SCAN_SCR) {
    *rts = scr;
    ret = TRUE;
    goto beach;
  }

  /* Possible optional System header here */
  if (data + 8 > end)
    goto beach;

  code = GST_READ_UINT32_BE (data);
  len = GST_READ_UINT16_BE (data + 4);
  if (code == ID_PS_SYSTEM_HEADER_START_CODE) {
    /* Found a system header, skip it */
    /* Check for sufficient data - system header, plus enough
     * left over for the PES packet header */
    if (data + 6 + len + 6 > end)
      return FALSE;
    data += len + 6;
    /* read the 4 bytes for the PES sync code */
    code = GST_READ_UINT32_BE (data);
    len = GST_READ_UINT16_BE (data + 4);
  }

  /* Check we have enough data left for reading the PES packet */
  if (data + 6 + len > end)
    return FALSE;
  if (!gst_ps_demux_is_pes_sync (code))
    goto beach;
  switch (code) {
    case ID_PS_PROGRAM_STREAM_MAP:
    case ID_PRIVATE_STREAM_2:
    case ID_ECM_STREAM:
    case ID_EMM_STREAM:
    case ID_PROGRAM_STREAM_DIRECTORY:
    case ID_DSMCC_STREAM:
    case ID_ITU_TREC_H222_TYPE_E_STREAM:
    case ID_PADDING_STREAM:
      goto beach;
    default:
      break;
  }

  /* skip sync code and size */
  data += 6;
  pts = dts = -1;
  /* stuffing bits, first two bits are '10' for mpeg2 pes so this code is
   * not triggered. */
  while (TRUE) {
    if (*data != 0xff)
      break;
    data++;
  }

  /* STD buffer size, never for mpeg2 */
  if ((*data & 0xc0) == 0x40)
    data += 2;
  /* PTS but no DTS, never for mpeg2 */
  if ((*data & 0xf0) == 0x20) {
    READ_TS (data, pts, beach);
  }
  /* PTS and DTS, never for mpeg2 */
  else if ((*data & 0xf0) == 0x30) {
    READ_TS (data, pts, beach);
    READ_TS (data, dts, beach);
  } else if ((*data & 0xc0) == 0x80) {
    /* mpeg2 case */
    guchar flags;
    /* 2: '10'
     * 2: PES_scrambling_control
     * 1: PES_priority
     * 1: data_alignment_indicator
     * 1: copyright
     * 1: original_or_copy
     */
    flags = *data++;
    if ((flags & 0xc0) != 0x80)
      goto beach;
    /* 2: PTS_DTS_flags
     * 1: ESCR_flag
     * 1: ES_rate_flag
     * 1: DSM_trick_mode_flag
     * 1: additional_copy_info_flag
     * 1: PES_CRC_flag
     * 1: PES_extension_flag
     */
    flags = *data++;
    /* 8: PES_header_data_length */
    data++;
    /* only DTS: this is invalid */
    if ((flags & 0xc0) == 0x40)
      goto beach;
    /* check for PTS */
    if ((flags & 0x80)) {
      READ_TS (data, pts, beach);
    }
    /* check for DTS */
    if ((flags & 0x40)) {
      READ_TS (data, dts, beach);
    }
  }

  if (mode == SCAN_DTS && dts != (guint64) - 1) {
    *rts = dts;
    ret = TRUE;
  }

  if (mode == SCAN_PTS && pts != (guint64) - 1) {
    *rts = pts;
    ret = TRUE;
  }
beach:
  return ret;
}

static inline gboolean
gst_ps_demux_scan_forward_ts (GstPsDemux * demux, guint64 * pos,
    SCAN_MODE mode, guint64 * rts, gint limit)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;
  guint64 offset = *pos;
  gboolean found = FALSE;
  guint64 ts = 0;
  guint scan_sz = (mode == SCAN_SCR ? SCAN_SCR_SZ : SCAN_PTS_SZ);
  guint cursor, to_read = BLOCK_SZ;
  guint end_scan;
  GstMapInfo map;
  do {
    /* Check we can get at least scan_sz bytes */
    if (offset + scan_sz > demux->sink_segment.stop)
      return FALSE;
    /* Don't go further than 'limit' bytes */
    if (limit && offset > *pos + limit)
      return FALSE;
    if (offset + to_read > demux->sink_segment.stop)
      to_read = demux->sink_segment.stop - offset;
    /* read some data */
    buffer = NULL;
    ret = gst_pad_pull_range (demux->sinkpad, offset, to_read, &buffer);
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      return FALSE;
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    /* may get a short buffer at the end of the file */
    if (G_UNLIKELY (map.size <= scan_sz)) {
      gst_buffer_unmap (buffer, &map);
      gst_buffer_unref (buffer);
      return FALSE;
    }

    end_scan = map.size - scan_sz;
    /* scan the block */
    for (cursor = 0; !found && cursor <= end_scan; cursor++) {
      found = gst_ps_demux_scan_ts (demux, map.data + cursor, mode, &ts,
          map.data + map.size);
    }

    /* done with the buffer, unref it */
    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
    if (found) {
      *rts = ts;
      *pos = offset + cursor - 1;
    } else {
      offset += cursor;
    }
  } while (!found && offset < demux->sink_segment.stop);
  return found;
}

static inline gboolean
gst_ps_demux_scan_backward_ts (GstPsDemux * demux, guint64 * pos,
    SCAN_MODE mode, guint64 * rts, gint limit)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;
  guint64 offset = *pos;
  gboolean found = FALSE;
  guint64 ts = 0;
  guint scan_sz = (mode == SCAN_SCR ? SCAN_SCR_SZ : SCAN_PTS_SZ);
  guint cursor, to_read = BLOCK_SZ;
  guint start_scan;
  guint8 *data;
  GstMapInfo map;
  do {
    /* Check we have at least scan_sz bytes available */
    if (offset < scan_sz - 1)
      return FALSE;
    /* Don't go backward past the start or 'limit' bytes */
    if (limit && offset + limit < *pos)
      return FALSE;
    if (offset > BLOCK_SZ)
      offset -= BLOCK_SZ;
    else {
      to_read = offset + 1;
      offset = 0;
    }
    /* read some data */
    buffer = NULL;
    ret = gst_pad_pull_range (demux->sinkpad, offset, to_read, &buffer);
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      return FALSE;
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    /* may get a short buffer at the end of the file */
    if (G_UNLIKELY (map.size <= scan_sz)) {
      gst_buffer_unmap (buffer, &map);
      gst_buffer_unref (buffer);
      return FALSE;
    }

    start_scan = map.size - scan_sz;
    data = map.data + start_scan;
    /* scan the block */
    for (cursor = (start_scan + 1); !found && cursor > 0; cursor--) {
      found = gst_ps_demux_scan_ts (demux, data--, mode, &ts,
          map.data + map.size);
    }

    /* done with the buffer, unref it */
    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
    if (found) {
      *rts = ts;
      *pos = offset + cursor;
    }

  } while (!found && offset > 0);
  return found;
}

static inline gboolean
gst_ps_sink_get_duration (GstPsDemux * demux)
{
  gboolean res = FALSE;
  GstPad *peer;
  GstFormat format = GST_FORMAT_BYTES;
  gint64 length = 0;
  guint64 offset;
  guint i;
  guint64 scr = 0;
  /* init the sink segment */
  gst_segment_init (&demux->sink_segment, format);
  /* get peer to figure out length */
  if ((peer = gst_pad_get_peer (demux->sinkpad)) == NULL)
    goto beach;
  res = gst_pad_query_duration (peer, format, &length);
  gst_object_unref (peer);
  if (!res || length <= 0)
    goto beach;
  GST_DEBUG_OBJECT (demux, "file length %" G_GINT64_FORMAT, length);
  /* update the sink segment */
  demux->sink_segment.stop = length;
  gst_segment_set_duration (&demux->sink_segment, format, length);
  gst_segment_set_position (&demux->sink_segment, format, 0);
  /* Scan for notorious SCR and PTS to calculate the duration */
  /* scan for first SCR in the stream */
  offset = demux->sink_segment.start;
  gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_SCR,
      &demux->first_scr, DURATION_SCAN_LIMIT);
  GST_DEBUG_OBJECT (demux,
      "First SCR: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT, demux->first_scr,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_scr)), offset);
  demux->first_scr_offset = offset;
  /* scan for last SCR in the stream */
  offset = demux->sink_segment.stop;
  gst_ps_demux_scan_backward_ts (demux, &offset, SCAN_SCR,
      &demux->last_scr, DURATION_SCAN_LIMIT);
  GST_DEBUG_OBJECT (demux,
      "Last SCR: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT, demux->last_scr,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->last_scr)), offset);
  demux->last_scr_offset = offset;
  /* scan for first PTS in the stream */
  offset = demux->sink_segment.start;
  gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_PTS,
      &demux->first_pts, DURATION_SCAN_LIMIT);
  GST_DEBUG_OBJECT (demux,
      "First PTS: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
      " in packet starting at %" G_GUINT64_FORMAT, demux->first_pts,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_pts)), offset);
  if (demux->first_pts != G_MAXUINT64) {
    /* scan for last PTS in the stream */
    offset = demux->sink_segment.stop;
    gst_ps_demux_scan_backward_ts (demux, &offset, SCAN_PTS,
        &demux->last_pts, DURATION_SCAN_LIMIT);
    GST_DEBUG_OBJECT (demux,
        "Last PTS: %" G_GINT64_FORMAT " %" GST_TIME_FORMAT
        " in packet starting at %" G_GUINT64_FORMAT, demux->last_pts,
        GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->last_pts)), offset);
  }
  /* Detect wrong SCR values */
  if (demux->first_scr > demux->last_scr) {
    GST_DEBUG_OBJECT (demux, "Wrong SCR values detected, searching for "
        "a better first SCR value");
    offset = demux->first_scr_offset;
    for (i = 0; i < 10; i++) {
      offset++;
      gst_ps_demux_scan_forward_ts (demux, &offset, SCAN_SCR, &scr, 0);
      if (scr < demux->last_scr) {
        demux->first_scr = scr;
        demux->first_scr_offset = offset;
        /* Start demuxing from the right place */
        demux->sink_segment.position = offset;
        GST_DEBUG_OBJECT (demux, "Replaced First SCR: %" G_GINT64_FORMAT
            " %" GST_TIME_FORMAT " in packet starting at %"
            G_GUINT64_FORMAT, demux->first_scr,
            GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->first_scr)), offset);
        break;
      }
    }
  }
  /* Set the base_time and avg rate */
  demux->base_time = MPEGTIME_TO_GSTTIME (demux->first_scr);
  demux->scr_rate_n = demux->last_scr_offset - demux->first_scr_offset;
  demux->scr_rate_d = demux->last_scr - demux->first_scr;
  if (G_LIKELY (demux->first_pts != G_MAXUINT64 &&
          demux->last_pts != G_MAXUINT64)) {
    /* update the src segment */
    demux->src_segment.format = GST_FORMAT_TIME;
    demux->src_segment.start =
        MPEGTIME_TO_GSTTIME (demux->first_pts) - demux->base_time;
    demux->src_segment.stop = -1;
    gst_segment_set_duration (&demux->src_segment, GST_FORMAT_TIME,
        MPEGTIME_TO_GSTTIME (demux->last_pts - demux->first_pts));
    gst_segment_set_position (&demux->src_segment, GST_FORMAT_TIME,
        demux->src_segment.start);
  }
  GST_INFO_OBJECT (demux, "sink segment configured %" GST_SEGMENT_FORMAT,
      &demux->sink_segment);
  GST_INFO_OBJECT (demux, "src segment configured %" GST_SEGMENT_FORMAT,
      &demux->src_segment);
  res = TRUE;
beach:
  return res;
}

static inline GstFlowReturn
gst_ps_demux_pull_block (GstPad * pad, GstPsDemux * demux,
    guint64 offset, guint size)
{
  GstFlowReturn ret;
  GstBuffer *buffer = NULL;
  ret = gst_pad_pull_range (pad, offset, size, &buffer);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (demux, "pull range at %" G_GUINT64_FORMAT
        " size %u failed", offset, size);
    goto beach;
  } else
    GST_LOG_OBJECT (demux, "pull range at %" G_GUINT64_FORMAT
        " size %u done", offset, size);
  if (demux->sink_segment.rate < 0) {
    GST_LOG_OBJECT (demux, "setting discont flag on backward rate");
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  }
  ret = gst_ps_demux_chain (pad, GST_OBJECT (demux), buffer);
beach:
  return ret;
}

static void
gst_ps_demux_loop (GstPad * pad)
{
  GstPsDemux *demux;
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 offset = 0;
  demux = GST_PS_DEMUX (gst_pad_get_parent (pad));
  if (G_UNLIKELY (demux->flushing)) {
    ret = GST_FLOW_FLUSHING;
    goto pause;
  }

  if (G_UNLIKELY (demux->sink_segment.format == GST_FORMAT_UNDEFINED))
    gst_ps_sink_get_duration (demux);
  offset = demux->sink_segment.position;
  if (demux->sink_segment.rate >= 0) {
    guint size = BLOCK_SZ;
    if (G_LIKELY (demux->sink_segment.stop != (guint64) - 1)) {
      size = MIN (size, demux->sink_segment.stop - offset);
    }
    /* pull in data */
    ret = gst_ps_demux_pull_block (pad, demux, offset, size);
    /* pause if something went wrong */
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto pause;
    /* update our position */
    offset += size;
    gst_segment_set_position (&demux->sink_segment, GST_FORMAT_BYTES, offset);
    /* check EOS condition */
    /* FIXME: The src_segment.stop is not including the SCR after seek(set) */
    if ((demux->sink_segment.position >= demux->sink_segment.stop) ||
        (demux->src_segment.stop != (guint64) - 1 &&
            demux->src_segment.position >=
            demux->src_segment.stop + demux->base_time)) {
      GST_DEBUG_OBJECT (demux,
          "forward mode using segment reached end of " "segment pos %"
          GST_TIME_FORMAT " stop %" GST_TIME_FORMAT " pos in bytes %"
          G_GUINT64_FORMAT " stop in bytes %" G_GUINT64_FORMAT,
          GST_TIME_ARGS (demux->src_segment.position),
          GST_TIME_ARGS (demux->src_segment.stop),
          demux->sink_segment.position, demux->sink_segment.stop);
      ret = GST_FLOW_EOS;
      goto pause;
    }
  } else {                      /* Reverse playback */
    guint64 size = MIN (offset, BLOCK_SZ);
    /* pull in data */
    ret = gst_ps_demux_pull_block (pad, demux, offset - size, size);
    /* pause if something went wrong */
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto pause;
    /* update our position */
    offset -= size;
    gst_segment_set_position (&demux->sink_segment, GST_FORMAT_BYTES, offset);
    /* check EOS condition */
    if (demux->sink_segment.position <= demux->sink_segment.start ||
        demux->src_segment.position <= demux->src_segment.start) {
      GST_DEBUG_OBJECT (demux,
          "reverse mode using segment reached end of " "segment pos %"
          GST_TIME_FORMAT " stop %" GST_TIME_FORMAT " pos in bytes %"
          G_GUINT64_FORMAT " stop in bytes %" G_GUINT64_FORMAT,
          GST_TIME_ARGS (demux->src_segment.position),
          GST_TIME_ARGS (demux->src_segment.start),
          demux->sink_segment.position, demux->sink_segment.start);
      ret = GST_FLOW_EOS;
      goto pause;
    }
  }

  gst_object_unref (demux);
  return;
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);
    GST_LOG_OBJECT (demux, "pausing task, reason %s", reason);
    gst_pad_pause_task (pad);
    if (ret == GST_FLOW_EOS) {
      /* perform EOS logic */
      gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
      if (demux->src_segment.flags & GST_SEEK_FLAG_SEGMENT) {
        gint64 stop;
        /* for segment playback we need to post when (in stream time)
         * we stopped, this is either stop (when set) or the duration. */
        if ((stop = demux->src_segment.stop) == -1)
          stop = demux->src_segment.duration;
        if (demux->sink_segment.rate >= 0) {
          GST_LOG_OBJECT (demux, "Sending segment done, at end of segment");
          gst_element_post_message (GST_ELEMENT_CAST (demux),
              gst_message_new_segment_done (GST_OBJECT_CAST (demux),
                  GST_FORMAT_TIME, stop));
          gst_ps_demux_send_event (demux,
              gst_event_new_segment_done (GST_FORMAT_TIME, stop));
        } else {                /* Reverse playback */
          GST_LOG_OBJECT (demux,
              "Sending segment done, at beginning of " "segment");
          gst_element_post_message (GST_ELEMENT_CAST (demux),
              gst_message_new_segment_done (GST_OBJECT_CAST (demux),
                  GST_FORMAT_TIME, demux->src_segment.start));
          gst_ps_demux_send_event (demux,
              gst_event_new_segment_done (GST_FORMAT_TIME,
                  demux->src_segment.start));
        }
      } else {
        GstEvent *event;
        /* normal playback, send EOS to all linked pads */
        gst_element_no_more_pads (GST_ELEMENT (demux));
        GST_LOG_OBJECT (demux, "Sending EOS, at end of stream");
        event = gst_event_new_eos ();
        if (demux->segment_seqnum)
          gst_event_set_seqnum (event, demux->segment_seqnum);
        if (!gst_ps_demux_send_event (demux, event)
            && !have_open_streams (demux)) {
          GST_WARNING_OBJECT (demux, "EOS and no streams open");
          GST_ELEMENT_ERROR (demux, STREAM, FAILED,
              ("Internal data stream error."), ("No valid streams detected"));
        }
      }
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS) {
      GstEvent *event;
      GST_ELEMENT_FLOW_ERROR (demux, ret);
      event = gst_event_new_eos ();
      if (demux->segment_seqnum)
        gst_event_set_seqnum (event, demux->segment_seqnum);
      gst_ps_demux_send_event (demux, event);
    }

    gst_object_unref (demux);
    return;
  }
}

/* If we can pull that's preferred */
static gboolean
gst_ps_demux_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  gboolean res = FALSE;
  GstQuery *query = gst_query_new_scheduling ();
  if (gst_pad_peer_query (sinkpad, query)) {
    if (gst_query_has_scheduling_mode_with_flags (query,
            GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE)) {
      res = gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);
    } else {
      res = gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
    }
  } else {
    res = gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }

  gst_query_unref (query);
  return res;
}

/* This function gets called when we activate ourselves in push mode. */
static gboolean
gst_ps_demux_sink_activate_push (GstPad * sinkpad, GstObject * parent,
    gboolean active)
{
  GstPsDemux *demux = GST_PS_DEMUX (parent);
  demux->random_access = FALSE;
  return TRUE;
}

/* this function gets called when we activate ourselves in pull mode.
 * We can perform  random access to the resource and we start a task
 * to start reading */
static gboolean
gst_ps_demux_sink_activate_pull (GstPad * sinkpad, GstObject * parent,
    gboolean active)
{
  GstPsDemux *demux = GST_PS_DEMUX (parent);
  if (active) {
    GST_DEBUG ("pull mode activated");
    demux->random_access = TRUE;
    return gst_pad_start_task (sinkpad,
        (GstTaskFunction) gst_ps_demux_loop, sinkpad, NULL);
  } else {
    demux->random_access = FALSE;
    return gst_pad_stop_task (sinkpad);
  }
}

static gboolean
gst_ps_demux_sink_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  if (mode == GST_PAD_MODE_PUSH) {
    return gst_ps_demux_sink_activate_push (pad, parent, active);
  } else if (mode == GST_PAD_MODE_PULL) {
    return gst_ps_demux_sink_activate_pull (pad, parent, active);
  }
  return FALSE;
}

/* EOS and NOT_LINKED need to be combined. This means that we return:
*
*  GST_FLOW_NOT_LINKED: when all pads NOT_LINKED.
*  GST_FLOW_EOS: when all pads EOS or NOT_LINKED.
*/
static GstFlowReturn
gst_ps_demux_combine_flows (GstPsDemux * demux, GstFlowReturn ret)
{
  GST_LOG_OBJECT (demux, "flow return: %s", gst_flow_get_name (ret));
  ret = gst_flow_combiner_update_flow (demux->flowcombiner, ret);
  if (G_UNLIKELY (demux->need_no_more_pads && ret == GST_FLOW_NOT_LINKED))
    ret = GST_FLOW_OK;
  GST_LOG_OBJECT (demux, "combined flow return: %s", gst_flow_get_name (ret));
  return ret;
}

static GstFlowReturn
gst_ps_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstPsDemux *demux = GST_PS_DEMUX (parent);
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 avail;
  gboolean save, discont;
  discont = GST_BUFFER_IS_DISCONT (buffer);
  if (discont) {
    GST_LOG_OBJECT (demux,
        "Received buffer with discont flag and" " offset %"
        G_GUINT64_FORMAT, GST_BUFFER_OFFSET (buffer));
    gst_pes_filter_drain (&demux->filter);
    gst_ps_demux_mark_discont (demux, TRUE, FALSE);
    /* mark discont on all streams */
    if (demux->sink_segment.rate >= 0.0) {
      demux->current_scr = G_MAXUINT64;
      demux->bytes_since_scr = 0;
    }
  } else {
    GST_LOG_OBJECT (demux,
        "Received buffer with offset %" G_GUINT64_FORMAT,
        GST_BUFFER_OFFSET (buffer));
  }

  /* We keep the offset to interpolate SCR */
  demux->adapter_offset = GST_BUFFER_OFFSET (buffer);
  gst_adapter_push (demux->adapter, buffer);
  demux->bytes_since_scr += gst_buffer_get_size (buffer);
  avail = gst_adapter_available (demux->rev_adapter);
  if (avail > 0) {
    GST_LOG_OBJECT (demux, "appending %u saved bytes", avail);
    /* if we have a previous reverse chunk, append this now */
    /* FIXME this code assumes we receive discont buffers all thei
     * time */
    gst_adapter_push (demux->adapter,
        gst_adapter_take_buffer (demux->rev_adapter, avail));
  }

  avail = gst_adapter_available (demux->adapter);
  GST_LOG_OBJECT (demux, "avail now: %d, state %d", avail, demux->filter.state);
  switch (demux->filter.state) {
    case STATE_DATA_SKIP:
    case STATE_DATA_PUSH:
      ret = gst_pes_filter_process (&demux->filter);
      break;
    case STATE_HEADER_PARSE:
      break;
    default:
      break;
  }

  switch (ret) {
    case GST_FLOW_NEED_MORE_DATA:
      /* Go and get more data */
      ret = GST_FLOW_OK;
      goto done;
    case GST_FLOW_LOST_SYNC:
      /* for FLOW_OK or lost-sync, carry onto resync */
      ret = GST_FLOW_OK;
      break;
    case GST_FLOW_OK:
      break;
    default:
      /* Any other return value should be sent upstream immediately */
      goto done;
  }

  /* align adapter data to sync boundary, we keep the data up to the next sync
   * point. */
  save = TRUE;
  while (gst_ps_demux_resync (demux, save)) {
    gboolean ps_sync = TRUE;
    if (G_UNLIKELY (demux->flushing)) {
      ret = GST_FLOW_FLUSHING;
      goto done;
    }

    /* now switch on last synced byte */
    switch (demux->last_sync_code) {
      case ID_PS_PACK_START_CODE:
        ret = gst_ps_demux_parse_pack_start (demux);
        break;
      case ID_PS_SYSTEM_HEADER_START_CODE:
        ret = gst_ps_demux_parse_sys_head (demux);
        break;
      case ID_PS_END_CODE:
        /* Skip final 4 bytes */
        gst_adapter_flush (demux->adapter, 4);
        ADAPTER_OFFSET_FLUSH (4);
        ret = GST_FLOW_OK;
        goto done;
      case ID_PS_PROGRAM_STREAM_MAP:
        ret = gst_ps_demux_parse_psm (demux);
        break;
      default:
        if (gst_ps_demux_is_pes_sync (demux->last_sync_code)) {
          ret = gst_pes_filter_process (&demux->filter);
        } else {
          GST_DEBUG_OBJECT (demux, "sync_code=%08x, non PES sync found"
              ", continuing", demux->last_sync_code);
          ps_sync = FALSE;
          ret = GST_FLOW_LOST_SYNC;
        }
        break;
    }
    /* if we found a ps sync, we stop saving the data, any non-ps sync gets
     * saved up to the next ps sync. */
    if (ps_sync)
      save = FALSE;
    switch (ret) {
      case GST_FLOW_NEED_MORE_DATA:
        GST_DEBUG_OBJECT (demux, "need more data");
        ret = GST_FLOW_OK;
        goto done;
      case GST_FLOW_LOST_SYNC:
        if (!save || demux->sink_segment.rate >= 0.0) {
          GST_DEBUG_OBJECT (demux, "flushing 3 bytes");
          gst_adapter_flush (demux->adapter, 3);
          ADAPTER_OFFSET_FLUSH (3);
        } else {
          GST_DEBUG_OBJECT (demux, "saving 3 bytes");
          gst_adapter_push (demux->rev_adapter,
              gst_adapter_take_buffer (demux->adapter, 3));
        }
        ret = GST_FLOW_OK;
        break;
      default:
        ret = gst_ps_demux_combine_flows (demux, ret);
        if (ret != GST_FLOW_OK)
          goto done;
        break;
    }
  }
done:
  return ret;
}

static GstStateChangeReturn
gst_ps_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstPsDemux *demux = GST_PS_DEMUX (element);
  GstStateChangeReturn result;
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_pes_filter_init (&demux->filter, demux->adapter,
          &demux->adapter_offset);
      gst_pes_filter_set_callbacks (&demux->filter,
          (GstPESFilterData) gst_ps_demux_data_cb,
          (GstPESFilterResync) gst_ps_demux_resync_cb, demux);
      demux->filter.gather_pes = TRUE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_ps_demux_reset (demux);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_pes_filter_uninit (&demux->filter);
      break;
    default:
      break;
  }

  return result;
}

static void
gst_segment_set_position (GstSegment * segment, GstFormat format,
    guint64 position)
{
  if (segment->format == GST_FORMAT_UNDEFINED) {
    segment->format = format;
  }
  segment->position = position;
}

static void
gst_segment_set_duration (GstSegment * segment, GstFormat format,
    guint64 duration)
{
  if (segment->format == GST_FORMAT_UNDEFINED) {
    segment->format = format;
  }
  segment->duration = duration;
}
