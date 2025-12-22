/*
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

#include <gst/video/video.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/pbutils.h>

#include "gstmpegdefs.h"
#include "gstmpegdemux.h"

#define SEGMENT_THRESHOLD (300*GST_MSECOND)
#define VIDEO_SEGMENT_THRESHOLD (500*GST_MSECOND)

/* The SCR_MUNGE value is used to offset the scr_adjust value, to avoid
 * ever generating a negative timestamp */
#define SCR_MUNGE (10 * GST_SECOND)

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
  ARG_0,
  /* FILL ME */
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-resin-dvd")
    );

static GstStaticPadTemplate video_template =
    GST_STATIC_PAD_TEMPLATE ("video_%02x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2, 4 }, " "systemstream = (boolean) FALSE, "
        "parsed = (boolean) FALSE; " "video/x-h264")
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

static void gst_flups_demux_base_init (GstFluPSDemuxClass * klass);
static void gst_flups_demux_class_init (GstFluPSDemuxClass * klass);
static void gst_flups_demux_init (GstFluPSDemux * demux);
static void gst_flups_demux_finalize (GstFluPSDemux * demux);
static void gst_flups_demux_reset (GstFluPSDemux * demux);

static gboolean gst_flups_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_flups_demux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_flups_demux_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_flups_demux_sink_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);
// static void gst_flups_demux_loop (GstPad * pad);

static gboolean gst_flups_demux_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_flups_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static GstStateChangeReturn gst_flups_demux_change_state (GstElement * element,
    GstStateChange transition);

static inline void gst_flups_demux_send_gap_updates (GstFluPSDemux * demux,
    GstClockTime new_time, gboolean no_threshold);
static inline void gst_flups_demux_clear_times (GstFluPSDemux * demux);

static void gst_flups_demux_reset_psm (GstFluPSDemux * demux);
static void gst_flups_demux_flush (GstFluPSDemux * demux);

static GstElementClass *parent_class = NULL;

static void gst_segment_set_position (GstSegment * segment, GstFormat format,
    guint64 position);
//static void gst_segment_set_duration (GstSegment * segment, GstFormat format,
//    guint64 duration);

/*static guint gst_flups_demux_signals[LAST_SIGNAL] = { 0 };*/

GType
gst_flups_demux_get_type (void)
{
  static GType flups_demux_type = 0;

  if (!flups_demux_type) {
    static const GTypeInfo flups_demux_info = {
      sizeof (GstFluPSDemuxClass),
      (GBaseInitFunc) gst_flups_demux_base_init,
      NULL,
      (GClassInitFunc) gst_flups_demux_class_init,
      NULL,
      NULL,
      sizeof (GstFluPSDemux),
      0,
      (GInstanceInitFunc) gst_flups_demux_init,
      NULL
    };

    flups_demux_type =
        g_type_register_static (GST_TYPE_ELEMENT, "RsnDVDDemux",
        &flups_demux_info, 0);

    GST_DEBUG_CATEGORY_INIT (gstflupsdemux_debug, "rsndvddemux", 0,
        "MPEG program stream demultiplexer element");
  }

  return flups_demux_type;
}

static void
gst_flups_demux_base_init (GstFluPSDemuxClass * klass)
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
      "MPEG Program Demuxer", "Codec/Demuxer",
      "Demultiplexes MPEG Program Streams",
      "Jan Schmidt <thaytan@noraisin.net>");
}

static void
gst_flups_demux_class_init (GstFluPSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = (GObjectFinalizeFunc) gst_flups_demux_finalize;

  gstelement_class->change_state = gst_flups_demux_change_state;
}

static void
gst_flups_demux_init (GstFluPSDemux * demux)
{
  GstFluPSDemuxClass *klass = GST_FLUPS_DEMUX_GET_CLASS (demux);

  demux->sinkpad = gst_pad_new_from_template (klass->sink_template, "sink");
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_event));
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_chain));
  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_activate));
  gst_pad_set_activatemode_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_sink_activate_mode));

  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  demux->streams =
      g_malloc0 (sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->streams_found =
      g_malloc0 (sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;

  demux->adapter = gst_adapter_new ();
  demux->rev_adapter = gst_adapter_new ();

  demux->scr_adjust = GSTTIME_TO_MPEGTIME (SCR_MUNGE);

  gst_flups_demux_reset (demux);
}

static void
gst_flups_demux_finalize (GstFluPSDemux * demux)
{
  gst_flups_demux_reset (demux);
  g_free (demux->streams);
  g_free (demux->streams_found);

  g_object_unref (demux->adapter);
  g_object_unref (demux->rev_adapter);

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (demux));
}

static void
gst_flups_demux_reset (GstFluPSDemux * demux)
{
  /* Clean up the streams and pads we allocated */
  gint i;

  for (i = 0; i < GST_FLUPS_DEMUX_MAX_STREAMS; i++) {
    GstFluPSStream *stream = demux->streams[i];

    if (stream != NULL) {
      if (stream->pad && GST_PAD_PARENT (stream->pad))
        gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);

      if (stream->pending_tags)
        gst_tag_list_unref (stream->pending_tags);
      g_free (stream);
      demux->streams[i] = NULL;
    }
  }
  memset (demux->streams_found, 0,
      sizeof (GstFluPSStream *) * (GST_FLUPS_DEMUX_MAX_STREAMS));
  demux->found_count = 0;

  gst_adapter_clear (demux->adapter);
  gst_adapter_clear (demux->rev_adapter);

  demux->adapter_offset = G_MAXUINT64;
  demux->first_scr = G_MAXUINT64;
  demux->current_scr = G_MAXUINT64;
  demux->base_time = G_MAXUINT64;
  demux->scr_rate_n = G_MAXUINT64;
  demux->scr_rate_d = G_MAXUINT64;
  demux->mux_rate = G_MAXUINT64;
  demux->next_pts = G_MAXUINT64;
  demux->next_dts = G_MAXUINT64;
  demux->need_no_more_pads = TRUE;
  gst_flups_demux_reset_psm (demux);
  gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&demux->src_segment, GST_FORMAT_TIME);
  gst_flups_demux_flush (demux);
  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;

  demux->scr_adjust = GSTTIME_TO_MPEGTIME (SCR_MUNGE);
}

static GstFluPSStream *
gst_flups_demux_create_stream (GstFluPSDemux * demux, gint id, gint stream_type)
{
  GstFluPSStream *stream;
  GstPadTemplate *template;
  gchar *name;
  GstFluPSDemuxClass *klass = GST_FLUPS_DEMUX_GET_CLASS (demux);
  GstCaps *caps;
  GstClockTime threshold = SEGMENT_THRESHOLD;
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
      threshold = VIDEO_SEGMENT_THRESHOLD;
      break;
    }
    case ST_AUDIO_MPEG1:
    case ST_AUDIO_MPEG2:
      template = klass->audio_template;
      name = g_strdup_printf ("audio_%02x", id);
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, NULL);
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
      caps = gst_caps_new_empty_simple ("video/x-h264");
      threshold = VIDEO_SEGMENT_THRESHOLD;
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

  stream = g_new0 (GstFluPSStream, 1);
  stream->id = id;
  stream->discont = TRUE;
  stream->need_segment = TRUE;
  stream->notlinked = FALSE;
  stream->last_flow = GST_FLOW_OK;
  stream->type = stream_type;
  stream->pending_tags = NULL;
  stream->pad = gst_pad_new_from_template (template, name);
  stream->segment_thresh = threshold;
  gst_pad_set_event_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_src_event));
  gst_pad_set_query_function (stream->pad,
      GST_DEBUG_FUNCPTR (gst_flups_demux_src_query));
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

static GstFluPSStream *
gst_flups_demux_get_stream (GstFluPSDemux * demux, gint id, gint type)
{
  GstFluPSStream *stream = demux->streams[id];

  if (stream == NULL && !demux->disable_stream_creation) {
    if (!(stream = gst_flups_demux_create_stream (demux, id, type)))
      goto unknown_stream;

    GST_DEBUG_OBJECT (demux, "adding pad for stream id 0x%02x type 0x%02x", id,
        type);

    gst_element_add_pad (GST_ELEMENT (demux), stream->pad);

    demux->streams[id] = stream;
    demux->streams_found[demux->found_count++] = stream;
  }
  return stream;

  /* ERROR */
unknown_stream:
  {
    GST_DEBUG_OBJECT (demux, "unknown stream id 0x%02x type 0x%02x", id, type);
    return NULL;
  }
}

static GstFlowReturn
gst_flups_demux_send_data (GstFluPSDemux * demux, GstFluPSStream * stream,
    GstBuffer * buf)
{
  GstFlowReturn result;
  GstClockTime pts = GST_CLOCK_TIME_NONE, dts = GST_CLOCK_TIME_NONE;

  if (stream == NULL)
    goto no_stream;

  /* timestamps */
  if (G_UNLIKELY (demux->next_pts != G_MAXUINT64))
    pts = MPEGTIME_TO_GSTTIME (demux->next_pts);
  if (G_UNLIKELY (demux->next_dts != G_MAXUINT64))
    dts = MPEGTIME_TO_GSTTIME (demux->next_dts);

  if (G_UNLIKELY (stream->pending_tags)) {
    GST_DEBUG_OBJECT (demux, "Sending pending_tags %p for pad %s:%s : %"
        GST_PTR_FORMAT, stream->pending_tags,
        GST_DEBUG_PAD_NAME (stream->pad), stream->pending_tags);
    gst_pad_push_event (stream->pad, gst_event_new_tag (stream->pending_tags));
    stream->pending_tags = NULL;
  }

  if (demux->current_scr != G_MAXUINT64) {
    GstClockTime cur_scr_time = MPEGTIME_TO_GSTTIME (demux->current_scr);

    if (stream->last_ts == GST_CLOCK_TIME_NONE ||
        stream->last_ts < cur_scr_time) {
#if 0
      g_print ("last_ts update on pad %s to time %" GST_TIME_FORMAT "\n",
          GST_PAD_NAME (stream->pad), GST_TIME_ARGS (cur_scr_time));
#endif
      stream->last_ts = cur_scr_time;
    }
  }

  /* OK, sent new segment now prepare the buffer for sending */
  GST_BUFFER_PTS (buf) = pts;
  GST_BUFFER_DTS (buf) = dts;

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
  stream->last_flow = result = gst_pad_push (stream->pad, buf);
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
gst_flups_demux_mark_discont (GstFluPSDemux * demux, gboolean discont,
    gboolean need_segment)
{
  gint i, count = demux->found_count;

  /* mark discont on all streams */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->discont |= discont;
      stream->need_segment |= need_segment;
      GST_DEBUG_OBJECT (demux, "marked stream as discont %d, need_segment %d",
          stream->discont, stream->need_segment);
    }
  }
}

static gboolean
gst_flups_demux_send_event (GstFluPSDemux * demux, GstEvent * event)
{
  gint i, count = demux->found_count;
  gboolean ret = FALSE;

  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

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
gst_flups_demux_handle_dvd_event (GstFluPSDemux * demux, GstEvent * event)
{
  const GstStructure *structure = gst_event_get_structure (event);
  const char *type = gst_structure_get_string (structure, "event");
  gint i;
  gchar cur_stream_name[32];
  GstFluPSStream *temp = NULL;
  const gchar *lang_code;
  gboolean ret = TRUE;

  if (strcmp (type, "dvd-lang-codes") == 0) {
    GST_DEBUG_OBJECT (demux, "Handling language codes event");

    demux->disable_stream_creation = FALSE;

    /* Create a video pad to ensure it exists before emitting no more pads */
    temp = gst_flups_demux_get_stream (demux, 0xe0, ST_VIDEO_MPEG2);
    /* Send a video format event downstream */
    {
      gboolean is_widescreen, is_pal;

      if (gst_structure_get_boolean (structure,
              "video-widescreen", &is_widescreen) &&
          gst_structure_get_boolean (structure, "video-pal-format", &is_pal)) {
        GstEvent *v_format;
        GstStructure *v_struct;

        v_struct = gst_structure_new ("application/x-gst-dvd",
            "event", G_TYPE_STRING, "dvd-video-format",
            "video-widescreen", G_TYPE_BOOLEAN, is_widescreen,
            "video-pal-format", G_TYPE_BOOLEAN, is_pal, NULL);
        v_format = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, v_struct);
        gst_pad_push_event (temp->pad, v_format);
      }
    }

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
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_AC3);
          break;
        case 0x2:
        case 0x3:
          /* MPEG audio without and with extension stream are
           * treated the same */
          stream_id += 0xC0;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - MPEG audio", i,
              stream_format, stream_id);
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_AUDIO_MPEG1);
          break;
        case 0x4:
          /* LPCM */
          stream_id += 0xA0;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - DVD LPCM", i,
              stream_format, stream_id);
          temp =
              gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_LPCM);
          break;
        case 0x6:
          /* DTS */
          stream_id += 0x88;
          GST_DEBUG_OBJECT (demux,
              "Audio stream %d format %d ID 0x%02x - DTS", i,
              stream_format, stream_id);
          temp = gst_flups_demux_get_stream (demux, stream_id, ST_PS_AUDIO_DTS);
          break;
        case 0x7:
          /* FIXME: What range is SDDS? */
          GST_WARNING_OBJECT (demux,
              "Unsupported audio stream format in language code event: %d",
              stream_format);
          temp = NULL;
          continue;
        default:
          GST_WARNING_OBJECT (demux,
              "Unknown audio stream format in language code event: %d",
              stream_format);
          temp = NULL;
          continue;
      }

      demux->audio_stream_map[i] = stream_id;

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
      temp = gst_flups_demux_get_stream (demux, 0x20 + stream_id,
          ST_PS_DVD_SUBPICTURE);

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

    demux->disable_stream_creation = TRUE;

    GST_DEBUG_OBJECT (demux, "Created all pads from Language Codes event, "
        "signalling no-more-pads");

    gst_element_no_more_pads (GST_ELEMENT (demux));
    demux->need_no_more_pads = FALSE;
    gst_event_unref (event);
  } else if (strcmp (type, "dvd-set-subpicture-track") == 0) {
    gint stream_id;
    gboolean forced_only;

    gst_structure_get_boolean (structure, "forced-only", &forced_only);

    if (gst_structure_get_int (structure, "physical-id", &stream_id)) {
      temp = demux->streams[(0x20 + stream_id) % GST_FLUPS_DEMUX_MAX_STREAMS];
      if (temp != NULL && temp->pad != NULL) {
        /* Send event to the selector to activate the desired pad */
        GstStructure *s = gst_structure_new ("application/x-gst-dvd",
            "event", G_TYPE_STRING, "select-pad", NULL);
        GstEvent *sel_event =
            gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);

        temp->notlinked = FALSE;
        gst_pad_push_event (temp->pad, sel_event);

        gst_event_ref (event);
        ret = gst_pad_push_event (temp->pad, event);
        GST_INFO_OBJECT (demux,
            "Subpicture physical ID change to %d, forced %d", stream_id,
            forced_only);
      }
    }
    gst_event_unref (event);
  } else if (strcmp (type, "dvd-set-audio-track") == 0) {
    gint stream_id;

    if (gst_structure_get_int (structure, "physical-id", &stream_id)) {
      stream_id = demux->audio_stream_map[stream_id % MAX_DVD_AUDIO_STREAMS];
      temp = demux->streams[stream_id];

      GST_INFO_OBJECT (demux, "Have DVD audio stream select event: "
          "stream 0x%02x", stream_id);
      if (temp != NULL && temp->pad != NULL) {
        /* Send event to the selector to activate the desired pad */
        GstStructure *s = gst_structure_new ("application/x-gst-dvd",
            "event", G_TYPE_STRING, "select-pad", NULL);
        GstEvent *sel_event =
            gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);
        gst_pad_push_event (temp->pad, sel_event);

        gst_event_ref (event);
        ret = gst_pad_push_event (temp->pad, event);
      }
    }
    gst_event_unref (event);
  } else {
    /* forward to all pads, e.g. dvd clut event */
    ret = gst_flups_demux_send_event (demux, event);
  }
  return ret;
}

static void
gst_flups_demux_flush (GstFluPSDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "flushing demuxer");
  gst_adapter_clear (demux->adapter);
  gst_adapter_clear (demux->rev_adapter);
  gst_pes_filter_drain (&demux->filter);
  gst_flups_demux_clear_times (demux);
  demux->adapter_offset = G_MAXUINT64;
  demux->current_scr = G_MAXUINT64;
  demux->bytes_since_scr = 0;
  demux->scr_adjust = GSTTIME_TO_MPEGTIME (SCR_MUNGE);
  demux->in_still = FALSE;
}

static inline void
gst_flups_demux_clear_times (GstFluPSDemux * demux)
{
  gint i, count = demux->found_count;

  /* Clear the last ts for all streams */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (G_LIKELY (stream)) {
      stream->last_ts = GST_CLOCK_TIME_NONE;
      stream->last_flow = GST_FLOW_OK;
    }
  }
}

static inline void
gst_flups_demux_send_gap_updates (GstFluPSDemux * demux, GstClockTime new_time,
    gboolean no_threshold)
{
  gint i, count = demux->found_count;
  GstEvent *event = NULL;

  /* Advance all lagging streams by sending a segment update */
  if (new_time > demux->src_segment.stop)
    return;

  /* FIXME: Handle reverse playback */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (stream) {
      GstClockTime gap_threshold = no_threshold ? 0 : stream->segment_thresh;

      if (stream->last_ts == GST_CLOCK_TIME_NONE ||
          stream->last_ts < demux->src_segment.start)
        stream->last_ts = demux->src_segment.start;
      if (stream->last_ts + gap_threshold < new_time) {
        GST_LOG_OBJECT (demux,
            "Sending gap update to pad %s time %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT, GST_PAD_NAME (stream->pad),
            GST_TIME_ARGS (stream->last_ts), GST_TIME_ARGS (new_time));
        event = gst_event_new_gap (stream->last_ts, new_time - stream->last_ts);
        gst_pad_push_event (stream->pad, event);
        stream->last_ts = new_time;
      }
    }
  }
}

static gboolean
gst_flups_demux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      gst_event_unref (event);
      break;
    case GST_EVENT_FLUSH_START:
      gst_flups_demux_send_event (demux, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_flups_demux_send_event (demux, event);
      gst_segment_init (&demux->sink_segment, GST_FORMAT_UNDEFINED);
      gst_flups_demux_flush (demux);
      break;
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gint64 start, stop, time;
      gint64 base, dur, position;
      GstClockTimeDiff adjust;

      gst_event_parse_segment (event, &segment);

      if (segment->format != GST_FORMAT_TIME)
        return FALSE;

      gst_segment_copy_into (segment, &demux->sink_segment);

      dur = segment->stop - segment->start;

      base = demux->sink_segment.base;
      start = demux->sink_segment.start;
      stop = demux->sink_segment.stop;
      time = demux->sink_segment.time;

      demux->first_scr = GSTTIME_TO_MPEGTIME (start);
      demux->current_scr = demux->first_scr + demux->scr_adjust;
      demux->base_time = time;
      demux->bytes_since_scr = 0;

      GST_DEBUG_OBJECT (demux,
          "demux: received new segment %" GST_SEGMENT_FORMAT,
          &demux->sink_segment);
#if 0
      g_print ("demux: received new segment start %" G_GINT64_FORMAT " stop %"
          G_GINT64_FORMAT " time %" G_GINT64_FORMAT
          " base %" G_GINT64_FORMAT "\n", start, stop, time, base);
#endif
      adjust = base - start + SCR_MUNGE;
      if (adjust >= 0)
        demux->scr_adjust = GSTTIME_TO_MPEGTIME (adjust);
      else
        demux->scr_adjust = -GSTTIME_TO_MPEGTIME (-adjust);

      position = start = SCR_MUNGE;
      base = 0;

      if (stop != -1)
        stop = start + dur;

      demux->src_segment.rate = segment->rate;
      demux->src_segment.applied_rate = segment->applied_rate;
      demux->src_segment.format = segment->format;
      demux->src_segment.start = start;
      demux->src_segment.stop = stop;
      demux->src_segment.time = time;
      demux->src_segment.base = base;
      demux->src_segment.position = position;

      GST_DEBUG_OBJECT (demux,
          "sending new segment %" GST_SEGMENT_FORMAT
          ", scr_adjust: %" G_GINT64_FORMAT "(%" GST_TIME_FORMAT ")",
          &demux->src_segment, demux->scr_adjust,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->scr_adjust)));
#if 0
      g_print ("sending new segment: rate %g format %d, start: %"
          G_GINT64_FORMAT ", stop: %" G_GINT64_FORMAT ", time: %"
          G_GINT64_FORMAT ", base: %" G_GINT64_FORMAT
          ", scr_adjust: %" G_GINT64_FORMAT "(%" GST_TIME_FORMAT ")\n",
          segment->rate, segment->format, start, stop, time, base,
          demux->scr_adjust,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (demux->scr_adjust)));
#endif

      gst_event_unref (event);
      event = gst_event_new_segment (&demux->src_segment);
      gst_flups_demux_send_event (demux, event);

      if (demux->in_still && stop != -1) {
        /* Generate gap buffers, due to closing segment from a still-frame.
         * Do this in the new segment with stop time. */
        GST_DEBUG_OBJECT (demux, "Advancing all streams to stop time %"
            GST_TIME_FORMAT, GST_TIME_ARGS (stop));
        gst_flups_demux_send_gap_updates (demux, stop, TRUE);
      }

      break;
    }
    case GST_EVENT_EOS:
      GST_INFO_OBJECT (demux, "Received EOS");
      if (!gst_flups_demux_send_event (demux, event)) {
        GST_WARNING_OBJECT (demux, "failed pushing EOS on streams");
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            ("Internal data stream error."), ("Can't push EOS downstream"));
      }
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
      const GstStructure *structure = gst_event_get_structure (event);
      gboolean in_still;

      if (gst_video_event_parse_still_frame (event, &in_still)) {
        /* Remember the still-frame state, so we can generate a pre-roll
         * GAP event when a segment event arrives */
        demux->in_still = in_still;
        GST_INFO_OBJECT (demux, "still-state now %d", demux->in_still);
        gst_flups_demux_send_event (demux, event);
      } else if (structure != NULL
          && gst_structure_has_name (structure, "application/x-gst-dvd")) {
        res = gst_flups_demux_handle_dvd_event (demux, event);
      } else {
        gst_flups_demux_send_event (demux, event);
      }
      break;
    }
    case GST_EVENT_CAPS:
      gst_event_unref (event);
      break;
    default:
      gst_flups_demux_send_event (demux, event);
      break;
  }

  return res;
}

static gboolean
gst_flups_demux_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = FALSE;
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
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

      bstart = GSTTIME_TO_BYTES (start);
      bstop = GSTTIME_TO_BYTES (stop);

      GST_DEBUG_OBJECT (demux, "in bytes bstart %" G_GINT64_FORMAT " bstop %"
          G_GINT64_FORMAT, bstart, bstop);
      bevent = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags, start_type,
          bstart, stop_type, bstop);

      res = gst_pad_push_event (demux->sinkpad, bevent);

    done:
      gst_event_unref (event);
      break;
    }
    default:
      res = gst_pad_push_event (demux->sinkpad, event);
      break;
  }

  return res;

not_supported:
  {
    gst_event_unref (event);

    return FALSE;
  }
}

static gboolean
gst_flups_demux_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = FALSE;
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);

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

      pos = demux->base_time;
      if (demux->current_scr != G_MAXUINT64 && demux->first_scr != G_MAXUINT64) {
        pos +=
            MPEGTIME_TO_GSTTIME (demux->current_scr - demux->scr_adjust -
            demux->first_scr);
      }

      GST_LOG_OBJECT (demux, "Position at GStreamer Time:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (pos));

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
      /* Just ask upstream */
      res = gst_pad_peer_query (demux->sinkpad, query);
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
not_supported:
  return FALSE;
}

static void
gst_flups_demux_reset_psm (GstFluPSDemux * demux)
{
  gint i;

#define FILL_TYPE(start, stop, type)	\
  for (i=start; i <= stop; i++)			\
    demux->psm[i] = type;

  FILL_TYPE (0x00, 0x1f, -1);
  FILL_TYPE (0x20, 0x3f, ST_PS_DVD_SUBPICTURE);
  FILL_TYPE (0x40, 0x7f, -1);
  FILL_TYPE (0x80, 0x87, ST_PS_AUDIO_AC3);
  FILL_TYPE (0x88, 0x9f, ST_PS_AUDIO_DTS);
  FILL_TYPE (0xa0, 0xaf, ST_PS_AUDIO_LPCM);
  FILL_TYPE (0xbd, 0xbd, -1);
  FILL_TYPE (0xc0, 0xdf, ST_AUDIO_MPEG1);
  FILL_TYPE (0xe0, 0xef, ST_GST_VIDEO_MPEG1_OR_2);
  FILL_TYPE (0xf0, 0xff, -1);

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
gst_flups_demux_parse_pack_start (GstFluPSDemux * demux)
{
  const guint8 *data;
  guint length;
  guint32 scr1, scr2;
  guint64 scr, scr_adjusted, new_rate;
  guint64 scr_rate_n;
  guint64 scr_rate_d;
  GstClockTime new_time;
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
    demux->first_scr = scr;
    demux->first_scr_offset = demux->cur_scr_offset;

    if (demux->sink_segment.format == GST_FORMAT_TIME) {
      demux->base_time = demux->sink_segment.time;
    } else {
      demux->base_time = MPEGTIME_TO_GSTTIME (demux->first_scr);
    }
    /* at begin consider the new_rate as the scr rate, bytes/clock ticks */
    scr_rate_n = new_rate;
    scr_rate_d = CLOCK_FREQ;
  } else if (G_LIKELY (demux->first_scr_offset != demux->cur_scr_offset)) {
    /* estimate byte rate related to the SCR */
    scr_rate_n = demux->cur_scr_offset - demux->first_scr_offset;
    scr_rate_d = scr_adjusted - demux->first_scr;
  } else {
    scr_rate_n = demux->scr_rate_n;
    scr_rate_d = demux->scr_rate_d;
  }

  GST_DEBUG_OBJECT (demux,
      "SCR: %" G_GINT64_FORMAT " (%" G_GINT64_FORMAT "), mux_rate %"
      G_GINT64_FORMAT ", GStreamer Time:%" GST_TIME_FORMAT,
      scr, scr_adjusted, new_rate,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME ((guint64) scr - demux->first_scr)));

  GST_DEBUG_OBJECT (demux, "%s mode scr: %" G_GUINT64_FORMAT " at %"
      G_GUINT64_FORMAT ", first scr: %" G_GUINT64_FORMAT
      " at %" G_GUINT64_FORMAT ", scr rate: %" G_GUINT64_FORMAT
      "/%" G_GUINT64_FORMAT "(%f)",
      ((demux->sink_segment.rate >= 0.0) ? "forward" : "backward"),
      scr, demux->cur_scr_offset,
      demux->first_scr, demux->first_scr_offset,
      scr_rate_n, scr_rate_d, (float) scr_rate_n / scr_rate_d);

  /* adjustment of the SCR */
  if (G_LIKELY (demux->current_scr != G_MAXUINT64)) {
    gint64 diff;
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
#if 0
      demux->scr_adjust = demux->next_scr - scr;
      GST_DEBUG_OBJECT (demux, "discont found, diff: %" G_GINT64_FORMAT
          ", adjust %" G_GINT64_FORMAT, diff, demux->scr_adjust);
      scr_adjusted = demux->next_scr;
#else
      GST_WARNING_OBJECT (demux, "Unexpected SCR diff of %" G_GINT64_FORMAT,
          diff);
#endif
    } else {
      demux->next_scr = scr_adjusted;
    }
  }

  /* update the current_scr and rate members */
  demux->mux_rate = new_rate;
  demux->current_scr = scr_adjusted;
  demux->scr_rate_n = scr_rate_n;
  demux->scr_rate_d = scr_rate_d;

  new_time = MPEGTIME_TO_GSTTIME (scr_adjusted);
  if (new_time != GST_CLOCK_TIME_NONE) {
    // g_print ("SCR now %" GST_TIME_FORMAT "\n", GST_TIME_ARGS (new_time));
    gst_segment_set_position (&demux->src_segment, GST_FORMAT_TIME, new_time);
    gst_flups_demux_send_gap_updates (demux, new_time, FALSE);
  }

  /* Reset the bytes_since_scr value to count the data remaining in the
   * adapter */
  demux->bytes_since_scr = avail;

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
gst_flups_demux_parse_sys_head (GstFluPSDemux * demux)
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
gst_flups_demux_parse_psm (GstFluPSDemux * demux)
{
  guint16 length = 0, info_length = 0, es_map_length = 0;
  guint8 psm_version = 0;
  const guint8 *data, *es_map_base;
#ifndef GST_DISABLE_GST_DEBUG
  gboolean applicable;
#endif

  if (gst_adapter_available (demux->adapter) < 6)
    goto need_more_data;

  /* start code + length */
  data = gst_adapter_map (demux->adapter, 6);

  /* skip start code */
  data += 4;

  length = GST_READ_UINT16_BE (data);
  GST_DEBUG_OBJECT (demux, "length %u", length);

  if (G_UNLIKELY (length > 0x3FA))
    goto psm_len_error;

  length += 6;

  gst_adapter_unmap (demux->adapter);

  if (gst_adapter_available (demux->adapter) < length)
    goto need_more_data;

  data = gst_adapter_map (demux->adapter, length);

  /* skip start code and length */
  data += 6;

  /* Read PSM applicable bit together with version */
  psm_version = GST_READ_UINT8 (data);
#ifndef GST_DISABLE_GST_DEBUG
  applicable = (psm_version & 0x80) >> 7;
#endif
  psm_version &= 0x1F;
  GST_DEBUG_OBJECT (demux, "PSM version %u (applicable now %u)", psm_version,
      applicable);

  /* Jump over version and marker bit */
  data += 2;

  /* Read PS info length */
  info_length = GST_READ_UINT16_BE (data);
  /* Cap it to PSM length - needed bytes for ES map length and CRC */
  info_length = MIN (length - 16, info_length);
  GST_DEBUG_OBJECT (demux, "PS info length %u bytes", info_length);

  /* Jump over that section */
  data += (2 + info_length);

  /* Read ES map length */
  es_map_length = GST_READ_UINT16_BE (data);
  /* Cap it to PSM remaining length -  CRC */
  es_map_length = MIN (length - (16 + info_length), es_map_length);
  GST_DEBUG_OBJECT (demux, "ES map length %u bytes", es_map_length);

  /* Jump over the size */
  data += 2;

  /* Now read the ES map */
  es_map_base = data;
  while (es_map_base + 4 <= data + es_map_length) {
    guint8 stream_type = 0, stream_id = 0;
    guint16 stream_info_length = 0;

    stream_type = GST_READ_UINT8 (es_map_base);
    es_map_base++;
    stream_id = GST_READ_UINT8 (es_map_base);
    es_map_base++;
    stream_info_length = GST_READ_UINT16_BE (es_map_base);
    es_map_base += 2;
    /* Cap stream_info_length */
    stream_info_length = MIN (data + es_map_length - es_map_base,
        stream_info_length);

    GST_DEBUG_OBJECT (demux, "Stream type %02X with id %02X and %u bytes info",
        stream_type, stream_id, stream_info_length);
    if (G_LIKELY (stream_id != 0xbd))
      demux->psm[stream_id] = stream_type;
    else {
      /* Ignore stream type for private_stream_1 and discover it looking at
       * the stream data.
       * Fixes demuxing some clips with lpcm that was wrongly declared as
       * mpeg audio */
      GST_DEBUG_OBJECT (demux, "stream type for private_stream_1 ignored");
    }
    es_map_base += stream_info_length;
  }

  gst_adapter_unmap (demux->adapter);
  gst_adapter_flush (demux->adapter, length);
  ADAPTER_OFFSET_FLUSH (length);
  return GST_FLOW_OK;

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
gst_flups_demux_resync_cb (GstPESFilter * filter, GstFluPSDemux * demux)
{
}

static GstFlowReturn
gst_flups_demux_data_cb (GstPESFilter * filter, gboolean first,
    GstBuffer * buffer, GstFluPSDemux * demux)
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
            GST_LOG_OBJECT (demux, "private type 0x%02x, stream type %d", id,
                stream_type);
          }
        }
      }
      if (stream_type == -1)
        goto unknown_stream_type;
    }
    if (filter->pts != -1) {
      demux->next_pts = filter->pts + demux->scr_adjust;
      GST_LOG_OBJECT (demux, "PTS = %" G_GUINT64_FORMAT
          "(%" G_GUINT64_FORMAT ")", filter->pts, demux->next_pts);
    } else
      demux->next_pts = G_MAXUINT64;

    if (filter->dts != -1) {
      demux->next_dts = filter->dts + demux->scr_adjust;
    } else {
      demux->next_dts = demux->next_pts;
    }
    GST_LOG_OBJECT (demux, "DTS = orig %" G_GUINT64_FORMAT
        " (%" G_GUINT64_FORMAT ")", filter->dts, demux->next_dts);

    demux->current_stream = gst_flups_demux_get_stream (demux, id, stream_type);
  }

  if (G_UNLIKELY (demux->current_stream == NULL)) {
    GST_DEBUG_OBJECT (demux, "Dropping buffer for unknown stream id 0x%02x",
        id);
    goto done;
  }

  /* After 2 seconds of bitstream emit no more pads */
  if (demux->need_no_more_pads
      && (demux->current_scr - demux->first_scr - demux->scr_adjust) >
      2 * CLOCK_FREQ) {
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

    ret = gst_flups_demux_send_data (demux, demux->current_stream, out_buf);
    if (ret == GST_FLOW_NOT_LINKED) {
      demux->current_stream->notlinked = TRUE;
      ret = GST_FLOW_OK;
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
gst_flups_demux_resync (GstFluPSDemux * demux, gboolean save)
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
gst_flups_demux_is_pes_sync (guint32 sync)
{
  return ((sync & 0xfc) == 0xbc) ||
      ((sync & 0xe0) == 0xc0) || ((sync & 0xf0) == 0xe0);
}

/* If we can pull that's preferred */
static gboolean
gst_flups_demux_sink_activate (GstPad * sinkpad, GstObject * parent)
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
gst_flups_demux_sink_activate_push (GstPad * sinkpad, GstObject * parent,
    gboolean active)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);

  demux->random_access = FALSE;

  return TRUE;
}

#if 0
/* this function gets called when we activate ourselves in pull mode.
 * We can perform  random access to the resource and we start a task
 * to start reading */
static gboolean
gst_flups_demux_sink_activate_pull (GstPad * sinkpad, GstObject * parent,
    gboolean active)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);

  if (active) {
    GST_DEBUG ("pull mode activated");
    demux->random_access = TRUE;
    return gst_pad_start_task (sinkpad, (GstTaskFunction) gst_flups_demux_loop,
        sinkpad, NULL);
  } else {
    demux->random_access = FALSE;
    return gst_pad_stop_task (sinkpad);
  }
}
#endif

static gboolean
gst_flups_demux_sink_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  if (mode == GST_PAD_MODE_PUSH) {
    return gst_flups_demux_sink_activate_push (pad, parent, active);
  } else if (mode == GST_PAD_MODE_PULL) {
//    return gst_flups_demux_sink_activate_pull (pad, parent, active);
  }
  return FALSE;
}

/* EOS and NOT_LINKED need to be combined. This means that we return:
*
*  GST_FLOW_NOT_LINKED: when all pads NOT_LINKED.
*  GST_FLOW_EOS: when all pads EOS or NOT_LINKED.
*/
static GstFlowReturn
gst_flups_demux_combine_flows (GstFluPSDemux * demux, GstFlowReturn ret)
{
  gint i, count = demux->found_count, streams = 0;
  gboolean unexpected = FALSE, not_linked = TRUE;

  GST_LOG_OBJECT (demux, "flow return: %s", gst_flow_get_name (ret));

  /* only return NOT_LINKED if all other pads returned NOT_LINKED */
  for (i = 0; i < count; i++) {
    GstFluPSStream *stream = demux->streams_found[i];

    if (G_UNLIKELY (!stream))
      continue;

    ret = stream->last_flow;
    streams++;

    /* some streams may still have to appear,
     * and only those ones may end up linked */
    if (G_UNLIKELY (demux->need_no_more_pads && ret == GST_FLOW_NOT_LINKED))
      ret = GST_FLOW_OK;

    /* no unexpected or unlinked, return */
    if (G_LIKELY (ret != GST_FLOW_EOS && ret != GST_FLOW_NOT_LINKED))
      goto done;

    /* we check to see if we have at least 1 unexpected or all unlinked */
    unexpected |= (ret == GST_FLOW_EOS);
    not_linked &= (ret == GST_FLOW_NOT_LINKED);
  }

  /* when we get here, we all have unlinked or unexpected */
  if (not_linked && streams)
    ret = GST_FLOW_NOT_LINKED;
  else if (unexpected)
    ret = GST_FLOW_EOS;

done:
  GST_LOG_OBJECT (demux, "combined flow return: %s", gst_flow_get_name (ret));
  return ret;
}

static GstFlowReturn
gst_flups_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (parent);
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 avail;
  gboolean save, discont;

  discont = GST_BUFFER_IS_DISCONT (buffer);

  if (discont) {
    GST_LOG_OBJECT (demux, "Received buffer with discont flag and"
        " offset %" G_GUINT64_FORMAT, GST_BUFFER_OFFSET (buffer));

    gst_pes_filter_drain (&demux->filter);
    gst_flups_demux_mark_discont (demux, TRUE, FALSE);

    /* mark discont on all streams */
    if (demux->sink_segment.rate >= 0.0) {
      demux->current_scr = G_MAXUINT64;
      demux->bytes_since_scr = 0;
    }
  } else {
    GST_LOG_OBJECT (demux, "Received buffer with offset %" G_GUINT64_FORMAT,
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
  while (gst_flups_demux_resync (demux, save)) {
    gboolean ps_sync = TRUE;

    /* now switch on last synced byte */
    switch (demux->last_sync_code) {
      case ID_PS_PACK_START_CODE:
        ret = gst_flups_demux_parse_pack_start (demux);
        break;
      case ID_PS_SYSTEM_HEADER_START_CODE:
        ret = gst_flups_demux_parse_sys_head (demux);
        break;
      case ID_PS_END_CODE:
        ret = GST_FLOW_OK;
        goto done;
      case ID_PS_PROGRAM_STREAM_MAP:
        ret = gst_flups_demux_parse_psm (demux);
        break;
      default:
        if (gst_flups_demux_is_pes_sync (demux->last_sync_code)) {
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
        ret = gst_flups_demux_combine_flows (demux, ret);
        if (ret != GST_FLOW_OK)
          goto done;
        break;
    }
  }
done:
  return ret;
}

static GstStateChangeReturn
gst_flups_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstFluPSDemux *demux = GST_FLUPS_DEMUX (element);
  GstStateChangeReturn result;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_pes_filter_init (&demux->filter, demux->adapter,
          &demux->adapter_offset);
      gst_pes_filter_set_callbacks (&demux->filter,
          (GstPESFilterData) gst_flups_demux_data_cb,
          (GstPESFilterResync) gst_flups_demux_resync_cb, demux);
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
      gst_flups_demux_reset (demux);
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

gboolean
gst_flups_demux_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (mpegpspesfilter_debug, "rsnpesfilter", 0,
      "MPEG program stream PES filter debug");

  GST_DEBUG_CATEGORY_INIT (gstflupsdemux_debug, "rsndvddemux", 0,
      "MPEG program stream demuxer debug");

  return TRUE;
}
