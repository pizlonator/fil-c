/* GStreamer H.265 Parser
 * Copyright (C) 2013 Intel Corporation
 *  Contact:Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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
#  include "config.h"
#endif

#include <gst/base/base.h>
#include <gst/pbutils/pbutils.h>
#include "gstvideoparserselements.h"
#include "gsth265parse.h"

#include <string.h>

GST_DEBUG_CATEGORY (h265_parse_debug);
#define GST_CAT_DEFAULT h265_parse_debug

#define DEFAULT_CONFIG_INTERVAL      (0)

enum
{
  PROP_0,
  PROP_CONFIG_INTERVAL
};

enum
{
  GST_H265_PARSE_FORMAT_NONE,
  GST_H265_PARSE_FORMAT_HVC1,
  GST_H265_PARSE_FORMAT_HEV1,
  GST_H265_PARSE_FORMAT_BYTE
};

enum
{
  GST_H265_PARSE_ALIGN_NONE = 0,
  GST_H265_PARSE_ALIGN_NAL,
  GST_H265_PARSE_ALIGN_AU
};

enum
{
  GST_H265_PARSE_STATE_GOT_SPS = 1 << 0,
  GST_H265_PARSE_STATE_GOT_PPS = 1 << 1,
  GST_H265_PARSE_STATE_GOT_SLICE = 1 << 2,

  GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS = (GST_H265_PARSE_STATE_GOT_SPS |
      GST_H265_PARSE_STATE_GOT_PPS),
  GST_H265_PARSE_STATE_VALID_PICTURE =
      (GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS |
      GST_H265_PARSE_STATE_GOT_SLICE)
};

enum
{
  GST_H265_PARSE_SEI_EXPIRED = 0,
  GST_H265_PARSE_SEI_ACTIVE = 1,
  GST_H265_PARSE_SEI_PARSED = 2,
};

#define GST_H265_PARSE_STATE_VALID(parse, expected_state) \
  (((parse)->state & (expected_state)) == (expected_state))

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, parsed = (boolean) true, "
        "stream-format=(string) { hvc1, hev1, byte-stream }, "
        "alignment=(string) { au, nal }"));

#define parent_class gst_h265_parse_parent_class
G_DEFINE_TYPE (GstH265Parse, gst_h265_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (h265parse, "h265parse",
    GST_RANK_SECONDARY, GST_TYPE_H265_PARSE,
    videoparsers_element_init (plugin));

static void gst_h265_parse_finalize (GObject * object);

static gboolean gst_h265_parse_start (GstBaseParse * parse);
static gboolean gst_h265_parse_stop (GstBaseParse * parse);
static GstFlowReturn gst_h265_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);
static GstFlowReturn gst_h265_parse_parse_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);
static GstFlowReturn gst_h265_parse_pre_push_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);

static void gst_h265_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_h265_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_h265_parse_set_caps (GstBaseParse * parse, GstCaps * caps);
static GstCaps *gst_h265_parse_get_caps (GstBaseParse * parse,
    GstCaps * filter);
static gboolean gst_h265_parse_event (GstBaseParse * parse, GstEvent * event);
static gboolean gst_h265_parse_src_event (GstBaseParse * parse,
    GstEvent * event);
static void
gst_h265_parse_process_sei_user_data (GstH265Parse * h265parse,
    GstH265RegisteredUserData * rud);
static void
gst_h265_parse_process_sei_user_data_unregistered (GstH265Parse * h265parse,
    GstH265UserDataUnregistered * urud);

static void
gst_h265_parse_class_init (GstH265ParseClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (h265_parse_debug, "h265parse", 0, "h265 parser");

  gobject_class->finalize = gst_h265_parse_finalize;
  gobject_class->set_property = gst_h265_parse_set_property;
  gobject_class->get_property = gst_h265_parse_get_property;

  g_object_class_install_property (gobject_class, PROP_CONFIG_INTERVAL,
      g_param_spec_int ("config-interval",
          "VPS SPS PPS Send Interval",
          "Send VPS, SPS and PPS Insertion Interval in seconds (sprop parameter sets "
          "will be multiplexed in the data stream when detected.) "
          "(0 = disabled, -1 = send with every IDR frame)",
          -1, 3600, DEFAULT_CONFIG_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  /* Override BaseParse vfuncs */
  parse_class->start = GST_DEBUG_FUNCPTR (gst_h265_parse_start);
  parse_class->stop = GST_DEBUG_FUNCPTR (gst_h265_parse_stop);
  parse_class->handle_frame = GST_DEBUG_FUNCPTR (gst_h265_parse_handle_frame);
  parse_class->pre_push_frame =
      GST_DEBUG_FUNCPTR (gst_h265_parse_pre_push_frame);
  parse_class->set_sink_caps = GST_DEBUG_FUNCPTR (gst_h265_parse_set_caps);
  parse_class->get_sink_caps = GST_DEBUG_FUNCPTR (gst_h265_parse_get_caps);
  parse_class->sink_event = GST_DEBUG_FUNCPTR (gst_h265_parse_event);
  parse_class->src_event = GST_DEBUG_FUNCPTR (gst_h265_parse_src_event);

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gst_element_class_set_static_metadata (gstelement_class, "H.265 parser",
      "Codec/Parser/Converter/Video",
      "Parses H.265 streams",
      "Sreerenj Balachandran <sreerenj.balachandran@intel.com>");
}

static void
gst_h265_parse_init (GstH265Parse * h265parse)
{
  h265parse->frame_out = gst_adapter_new ();
  gst_base_parse_set_pts_interpolation (GST_BASE_PARSE (h265parse), FALSE);
  gst_base_parse_set_infer_ts (GST_BASE_PARSE (h265parse), FALSE);
  GST_PAD_SET_ACCEPT_INTERSECT (GST_BASE_PARSE_SINK_PAD (h265parse));
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_BASE_PARSE_SINK_PAD (h265parse));
}


static void
gst_h265_parse_finalize (GObject * object)
{
  GstH265Parse *h265parse = GST_H265_PARSE (object);

  gst_video_clear_user_data_unregistered (&h265parse->user_data_unregistered,
      TRUE);

  g_object_unref (h265parse->frame_out);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_h265_parse_reset_frame (GstH265Parse * h265parse)
{
  GST_DEBUG_OBJECT (h265parse, "reset frame");

  /* done parsing; reset state */
  h265parse->current_off = -1;

  h265parse->update_caps = FALSE;
  h265parse->idr_pos = -1;
  h265parse->sei_pos = -1;
  h265parse->keyframe = FALSE;
  h265parse->predicted = FALSE;
  h265parse->bidirectional = FALSE;
  h265parse->header = FALSE;
  h265parse->have_vps_in_frame = FALSE;
  h265parse->have_sps_in_frame = FALSE;
  h265parse->have_pps_in_frame = FALSE;
  gst_adapter_clear (h265parse->frame_out);
  gst_video_clear_user_data (&h265parse->user_data);
  gst_video_clear_user_data_unregistered (&h265parse->user_data_unregistered,
      FALSE);
}

static void
gst_h265_parse_reset_stream_info (GstH265Parse * h265parse)
{
  gint i;

  h265parse->width = 0;
  h265parse->height = 0;
  h265parse->fps_num = 0;
  h265parse->fps_den = 0;
  h265parse->upstream_par_n = -1;
  h265parse->upstream_par_d = -1;
  h265parse->parsed_par_n = 0;
  h265parse->parsed_par_n = 0;
  h265parse->parsed_colorimetry.range = GST_VIDEO_COLOR_RANGE_UNKNOWN;
  h265parse->parsed_colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
  h265parse->parsed_colorimetry.transfer = GST_VIDEO_TRANSFER_UNKNOWN;
  h265parse->parsed_colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
  h265parse->have_pps = FALSE;
  h265parse->have_sps = FALSE;
  h265parse->have_vps = FALSE;

  h265parse->align = GST_H265_PARSE_ALIGN_NONE;
  h265parse->format = GST_H265_PARSE_FORMAT_NONE;

  h265parse->transform = FALSE;
  h265parse->nal_length_size = 4;
  h265parse->packetized = FALSE;
  h265parse->push_codec = FALSE;
  h265parse->first_frame = TRUE;

  gst_buffer_replace (&h265parse->codec_data, NULL);
  gst_buffer_replace (&h265parse->codec_data_in, NULL);

  gst_h265_parse_reset_frame (h265parse);

  for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++)
    gst_buffer_replace (&h265parse->vps_nals[i], NULL);
  for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++)
    gst_buffer_replace (&h265parse->sps_nals[i], NULL);
  for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++)
    gst_buffer_replace (&h265parse->pps_nals[i], NULL);

  gst_video_mastering_display_info_init (&h265parse->mastering_display_info);
  h265parse->mastering_display_info_state = GST_H265_PARSE_SEI_EXPIRED;

  gst_video_content_light_level_init (&h265parse->content_light_level);
  h265parse->content_light_level_state = GST_H265_PARSE_SEI_EXPIRED;
}

static void
gst_h265_parse_reset (GstH265Parse * h265parse)
{
  h265parse->last_report = GST_CLOCK_TIME_NONE;

  h265parse->pending_key_unit_ts = GST_CLOCK_TIME_NONE;
  gst_event_replace (&h265parse->force_key_unit_event, NULL);

  h265parse->discont = FALSE;
  h265parse->discard_bidirectional = FALSE;
  h265parse->marker = FALSE;

  gst_h265_parse_reset_stream_info (h265parse);
}

static gboolean
gst_h265_parse_start (GstBaseParse * parse)
{
  GstH265Parse *h265parse = GST_H265_PARSE (parse);

  GST_DEBUG_OBJECT (parse, "start");
  gst_h265_parse_reset (h265parse);

  h265parse->nalparser = gst_h265_parser_new ();
  h265parse->state = 0;

  gst_base_parse_set_min_frame_size (parse, 5);

  return TRUE;
}

static gboolean
gst_h265_parse_stop (GstBaseParse * parse)
{
  GstH265Parse *h265parse = GST_H265_PARSE (parse);

  GST_DEBUG_OBJECT (parse, "stop");
  gst_h265_parse_reset (h265parse);

  gst_h265_parser_free (h265parse->nalparser);

  return TRUE;
}

static const gchar *
gst_h265_parse_get_string (GstH265Parse * parse, gboolean format, gint code)
{
  if (format) {
    switch (code) {
      case GST_H265_PARSE_FORMAT_HVC1:
        return "hvc1";
      case GST_H265_PARSE_FORMAT_HEV1:
        return "hev1";
      case GST_H265_PARSE_FORMAT_BYTE:
        return "byte-stream";
      default:
        return "none";
    }
  } else {
    switch (code) {
      case GST_H265_PARSE_ALIGN_NAL:
        return "nal";
      case GST_H265_PARSE_ALIGN_AU:
        return "au";
      default:
        return "none";
    }
  }
}

static void
gst_h265_parse_format_from_caps (GstCaps * caps, guint * format, guint * align)
{
  g_return_if_fail (gst_caps_is_fixed (caps));

  GST_DEBUG ("parsing caps: %" GST_PTR_FORMAT, caps);

  if (format)
    *format = GST_H265_PARSE_FORMAT_NONE;

  if (align)
    *align = GST_H265_PARSE_ALIGN_NONE;

  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str = NULL;

    if (format) {
      if ((str = gst_structure_get_string (s, "stream-format"))) {
        if (strcmp (str, "hvc1") == 0)
          *format = GST_H265_PARSE_FORMAT_HVC1;
        else if (strcmp (str, "hev1") == 0)
          *format = GST_H265_PARSE_FORMAT_HEV1;
        else if (strcmp (str, "byte-stream") == 0)
          *format = GST_H265_PARSE_FORMAT_BYTE;
      }
    }

    if (align) {
      if ((str = gst_structure_get_string (s, "alignment"))) {
        if (strcmp (str, "au") == 0)
          *align = GST_H265_PARSE_ALIGN_AU;
        else if (strcmp (str, "nal") == 0)
          *align = GST_H265_PARSE_ALIGN_NAL;
      }
    }
  }
}

/* check downstream caps to configure format and alignment */
static void
gst_h265_parse_negotiate (GstH265Parse * h265parse, gint in_format,
    GstCaps * in_caps)
{
  GstCaps *caps;
  guint format = GST_H265_PARSE_FORMAT_NONE;
  guint align = GST_H265_PARSE_ALIGN_NONE;

  g_return_if_fail ((in_caps == NULL) || gst_caps_is_fixed (in_caps));

  caps = gst_pad_get_allowed_caps (GST_BASE_PARSE_SRC_PAD (h265parse));
  GST_DEBUG_OBJECT (h265parse, "allowed caps: %" GST_PTR_FORMAT, caps);

  /* concentrate on leading structure, since decodebin parser
   * capsfilter always includes parser template caps */
  if (caps) {
    caps = gst_caps_truncate (caps);
    GST_DEBUG_OBJECT (h265parse, "negotiating with caps: %" GST_PTR_FORMAT,
        caps);
  }

  if (in_caps && caps) {
    if (gst_caps_can_intersect (in_caps, caps)) {
      GST_DEBUG_OBJECT (h265parse, "downstream accepts upstream caps");
      gst_h265_parse_format_from_caps (in_caps, &format, &align);
      gst_caps_unref (caps);
      caps = NULL;
    }
  }

  /* FIXME We could fail the negotiation immediately if caps are empty */
  if (caps && !gst_caps_is_empty (caps)) {
    /* fixate to avoid ambiguity with lists when parsing */
    caps = gst_caps_fixate (caps);
    gst_h265_parse_format_from_caps (caps, &format, &align);
  }

  /* default */
  if (!format)
    format = GST_H265_PARSE_FORMAT_BYTE;
  if (!align)
    align = GST_H265_PARSE_ALIGN_AU;

  GST_DEBUG_OBJECT (h265parse, "selected format %s, alignment %s",
      gst_h265_parse_get_string (h265parse, TRUE, format),
      gst_h265_parse_get_string (h265parse, FALSE, align));

  h265parse->format = format;
  h265parse->align = align;

  h265parse->transform = in_format != h265parse->format ||
      align == GST_H265_PARSE_ALIGN_AU;

  if (caps)
    gst_caps_unref (caps);
}

static GstBuffer *
gst_h265_parse_wrap_nal (GstH265Parse * h265parse, guint format, guint8 * data,
    guint size)
{
  GstBuffer *buf;
  guint nl = h265parse->nal_length_size;
  guint32 tmp;

  GST_DEBUG_OBJECT (h265parse, "nal length %d", size);

  buf = gst_buffer_new_allocate (NULL, 4 + size, NULL);
  if (format == GST_H265_PARSE_FORMAT_HVC1
      || format == GST_H265_PARSE_FORMAT_HEV1) {
    tmp = GUINT32_TO_BE (size << (32 - 8 * nl));
  } else {
    /* HACK: nl should always be 4 here, otherwise this won't work.
     * There are legit cases where nl in hevc stream is 2, but byte-stream
     * SC is still always 4 bytes. */
    nl = 4;
    tmp = GUINT32_TO_BE (1);
  }

  gst_buffer_fill (buf, 0, &tmp, sizeof (guint32));
  gst_buffer_fill (buf, nl, data, size);
  gst_buffer_set_size (buf, size + nl);

  return buf;
}

static void
gst_h265_parser_store_nal (GstH265Parse * h265parse, guint id,
    GstH265NalUnitType naltype, GstH265NalUnit * nalu)
{
  GstBuffer *buf, **store;
  guint size = nalu->size, store_size;

  if (naltype == GST_H265_NAL_VPS) {
    store_size = GST_H265_MAX_VPS_COUNT;
    store = h265parse->vps_nals;
    GST_DEBUG_OBJECT (h265parse, "storing vps %u", id);
  } else if (naltype == GST_H265_NAL_SPS) {
    store_size = GST_H265_MAX_SPS_COUNT;
    store = h265parse->sps_nals;
    GST_DEBUG_OBJECT (h265parse, "storing sps %u", id);
  } else if (naltype == GST_H265_NAL_PPS) {
    store_size = GST_H265_MAX_PPS_COUNT;
    store = h265parse->pps_nals;
    GST_DEBUG_OBJECT (h265parse, "storing pps %u", id);
  } else
    return;

  if (id >= store_size) {
    GST_DEBUG_OBJECT (h265parse, "unable to store nal, id out-of-range %d", id);
    return;
  }

  buf = gst_buffer_new_allocate (NULL, size, NULL);
  gst_buffer_fill (buf, 0, nalu->data + nalu->offset, size);

  /* Indicate that buffer contain a header needed for decoding */
  if (naltype >= GST_H265_NAL_VPS && naltype <= GST_H265_NAL_PPS)
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);

  if (store[id])
    gst_buffer_unref (store[id]);

  store[id] = buf;
}

#ifndef GST_DISABLE_GST_DEBUG
static const gchar *nal_names[] = {
  "Slice_TRAIL_N",
  "Slice_TRAIL_R",
  "Slice_TSA_N",
  "Slice_TSA_R",
  "Slice_STSA_N",
  "Slice_STSA_R",
  "Slice_RADL_N",
  "Slice_RADL_R",
  "SLICE_RASL_N",
  "SLICE_RASL_R",
  "Invalid (10)",
  "Invalid (11)",
  "Invalid (12)",
  "Invalid (13)",
  "Invalid (14)",
  "Invalid (15)",
  "SLICE_BLA_W_LP",
  "SLICE_BLA_W_RADL",
  "SLICE_BLA_N_LP",
  "SLICE_IDR_W_RADL",
  "SLICE_IDR_N_LP",
  "SLICE_CRA_NUT",
  "Invalid (22)",
  "Invalid (23)",
  "Invalid (24)",
  "Invalid (25)",
  "Invalid (26)",
  "Invalid (27)",
  "Invalid (28)",
  "Invalid (29)",
  "Invalid (30)",
  "Invalid (31)",
  "VPS",
  "SPS",
  "PPS",
  "AUD",
  "EOS",
  "EOB",
  "FD",
  "PREFIX_SEI",
  "SUFFIX_SEI"
};

static const gchar *
_nal_name (GstH265NalUnitType nal_type)
{
  if (nal_type <= GST_H265_NAL_SUFFIX_SEI)
    return nal_names[nal_type];
  return "Invalid";
}
#endif

static void
gst_h265_parse_process_sei (GstH265Parse * h265parse, GstH265NalUnit * nalu)
{
  GstH265SEIMessage sei;
  GstH265Parser *nalparser = h265parse->nalparser;
  GstH265ParserResult pres;
  GArray *messages;
  guint i;

  pres = gst_h265_parser_parse_sei (nalparser, nalu, &messages);
  if (pres != GST_H265_PARSER_OK)
    GST_WARNING_OBJECT (h265parse, "failed to parse one or more SEI message");

  /* Even if pres != GST_H265_PARSER_OK, some message could have been parsed and
   * stored in messages.
   */
  for (i = 0; i < messages->len; i++) {
    sei = g_array_index (messages, GstH265SEIMessage, i);
    switch (sei.payloadType) {
      case GST_H265_SEI_RECOVERY_POINT:
        GST_LOG_OBJECT (h265parse, "recovery point found: %u %u %u",
            sei.payload.recovery_point.recovery_poc_cnt,
            sei.payload.recovery_point.exact_match_flag,
            sei.payload.recovery_point.broken_link_flag);
        h265parse->keyframe = TRUE;
        break;
      case GST_H265_SEI_TIME_CODE:
        memcpy (&h265parse->time_code, &sei.payload.time_code,
            sizeof (GstH265TimeCode));
        break;
      case GST_H265_SEI_PIC_TIMING:
        h265parse->sei_pic_struct = sei.payload.pic_timing.pic_struct;
        break;
      case GST_H265_SEI_REGISTERED_USER_DATA:
        gst_h265_parse_process_sei_user_data (h265parse,
            &sei.payload.registered_user_data);
        break;
      case GST_H265_SEI_USER_DATA_UNREGISTERED:
        gst_h265_parse_process_sei_user_data_unregistered (h265parse,
            &sei.payload.user_data_unregistered);
        break;
      case GST_H265_SEI_BUF_PERIOD:
        /* FIXME */
        break;
      case GST_H265_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
      {
        /* Precision defined by spec.
         * See D.3.28 Mastering display colour volume SEI message semantics */
        GstVideoMasteringDisplayInfo minfo;
        gint j, k;

        /* GstVideoMasteringDisplayInfo::display_primaries is rgb order but
         * HEVC uses gbr order
         * See spec D.3.28 display_primaries_x and display_primaries_y
         */
        for (j = 0, k = 2; j < G_N_ELEMENTS (minfo.display_primaries); j++, k++) {
          minfo.display_primaries[j].x =
              sei.payload.
              mastering_display_colour_volume.display_primaries_x[k % 3];
          minfo.display_primaries[j].y =
              sei.payload.
              mastering_display_colour_volume.display_primaries_y[k % 3];
        }

        minfo.white_point.x =
            sei.payload.mastering_display_colour_volume.white_point_x;
        minfo.white_point.y =
            sei.payload.mastering_display_colour_volume.white_point_y;
        minfo.max_display_mastering_luminance =
            sei.payload.mastering_display_colour_volume.
            max_display_mastering_luminance;
        minfo.min_display_mastering_luminance =
            sei.payload.mastering_display_colour_volume.
            min_display_mastering_luminance;

        GST_LOG_OBJECT (h265parse, "mastering display info found: "
            "Red(%u, %u) "
            "Green(%u, %u) "
            "Blue(%u, %u) "
            "White(%u, %u) "
            "max_luminance(%u) "
            "min_luminance(%u) ",
            minfo.display_primaries[0].x, minfo.display_primaries[0].y,
            minfo.display_primaries[1].x, minfo.display_primaries[1].y,
            minfo.display_primaries[2].x, minfo.display_primaries[2].y,
            minfo.white_point.x, minfo.white_point.y,
            minfo.max_display_mastering_luminance,
            minfo.min_display_mastering_luminance);

        if (h265parse->mastering_display_info_state ==
            GST_H265_PARSE_SEI_EXPIRED) {
          h265parse->update_caps = TRUE;
        } else if (!gst_video_mastering_display_info_is_equal
            (&h265parse->mastering_display_info, &minfo)) {
          h265parse->update_caps = TRUE;
        }

        h265parse->mastering_display_info_state = GST_H265_PARSE_SEI_PARSED;
        h265parse->mastering_display_info = minfo;

        break;
      }
      case GST_H265_SEI_CONTENT_LIGHT_LEVEL:
      {
        GstVideoContentLightLevel cll;

        cll.max_content_light_level =
            sei.payload.content_light_level.max_content_light_level;
        cll.max_frame_average_light_level =
            sei.payload.content_light_level.max_pic_average_light_level;

        GST_LOG_OBJECT (h265parse, "content light level found: "
            "maxCLL:(%u), maxFALL:(%u)", cll.max_content_light_level,
            cll.max_frame_average_light_level);

        if (h265parse->content_light_level_state == GST_H265_PARSE_SEI_EXPIRED) {
          h265parse->update_caps = TRUE;
        } else if (cll.max_content_light_level !=
            h265parse->content_light_level.max_content_light_level ||
            cll.max_frame_average_light_level !=
            h265parse->content_light_level.max_frame_average_light_level) {
          h265parse->update_caps = TRUE;
        }

        h265parse->content_light_level_state = GST_H265_PARSE_SEI_PARSED;
        h265parse->content_light_level = cll;

        break;
      }
      default:
        break;
    }
  }
  g_array_free (messages, TRUE);
}

static void
gst_h265_parse_process_sei_user_data (GstH265Parse * h265parse,
    GstH265RegisteredUserData * rud)
{
  guint16 provider_code;
  GstByteReader br;
  GstVideoParseUtilsField field = GST_VIDEO_PARSE_UTILS_FIELD_1;

  /* only US country code is currently supported */
  switch (rud->country_code) {
    case ITU_T_T35_COUNTRY_CODE_US:
      break;
    default:
      GST_LOG_OBJECT (h265parse, "Unsupported country code %d",
          rud->country_code);
      return;
  }

  if (rud->data == NULL || rud->size < 2)
    return;

  gst_byte_reader_init (&br, rud->data, rud->size);

  provider_code = gst_byte_reader_get_uint16_be_unchecked (&br);

  if (h265parse->sei_pic_struct ==
      (guint8) GST_H265_SEI_PIC_STRUCT_BOTTOM_FIELD)
    field = GST_VIDEO_PARSE_UTILS_FIELD_1;
  gst_video_parse_user_data ((GstElement *) h265parse, &h265parse->user_data,
      &br, field, provider_code);

}

static void
gst_h265_parse_process_sei_user_data_unregistered (GstH265Parse * h265parse,
    GstH265UserDataUnregistered * urud)
{
  GstByteReader br;

  if (urud->data == NULL || urud->size < 1)
    return;

  gst_byte_reader_init (&br, urud->data, urud->size);

  gst_video_parse_user_data_unregistered ((GstElement *) h265parse,
      &h265parse->user_data_unregistered, &br, urud->uuid);
}

/* caller guarantees 2 bytes of nal payload */
static gboolean
gst_h265_parse_process_nal (GstH265Parse * h265parse, GstH265NalUnit * nalu)
{
  GstH265PPS pps = { 0, };
  GstH265SPS sps = { 0, };
  GstH265VPS vps = { 0, };
  guint nal_type;
  GstH265Parser *nalparser = h265parse->nalparser;
  GstH265ParserResult pres = GST_H265_PARSER_ERROR;

  /* nothing to do for broken input */
  if (G_UNLIKELY (nalu->size < 2)) {
    GST_DEBUG_OBJECT (h265parse, "not processing nal size %u", nalu->size);
    return TRUE;
  }

  /* we have a peek as well */
  nal_type = nalu->type;

  GST_DEBUG_OBJECT (h265parse, "processing nal of type %u %s, size %u",
      nal_type, _nal_name (nal_type), nalu->size);
  switch (nal_type) {
    case GST_H265_NAL_VPS:
      /* It is not mandatory to have VPS in the stream. But it might
       * be needed for other extensions like svc */
      pres = gst_h265_parser_parse_vps (nalparser, nalu, &vps);
      if (pres != GST_H265_PARSER_OK) {
        GST_WARNING_OBJECT (h265parse, "failed to parse VPS");
        return FALSE;
      }

      GST_DEBUG_OBJECT (h265parse, "triggering src caps check");
      h265parse->update_caps = TRUE;
      h265parse->have_vps = TRUE;
      h265parse->have_vps_in_frame = TRUE;
      if (h265parse->push_codec && h265parse->have_pps) {
        /* VPS/SPS/PPS found in stream before the first pre_push_frame, no need
         * to forcibly push at start */
        GST_INFO_OBJECT (h265parse, "have VPS/SPS/PPS in stream");
        h265parse->push_codec = FALSE;
        h265parse->have_vps = FALSE;
        h265parse->have_sps = FALSE;
        h265parse->have_pps = FALSE;
      }

      gst_h265_parser_store_nal (h265parse, vps.id, nal_type, nalu);
      h265parse->header = TRUE;
      break;
    case GST_H265_NAL_SPS:
      /* reset state, everything else is obsolete */
      h265parse->state &= GST_H265_PARSE_STATE_GOT_PPS;

      pres = gst_h265_parser_parse_sps (nalparser, nalu, &sps, TRUE);


      /* arranged for a fallback sps.id, so use that one and only warn */
      if (pres != GST_H265_PARSER_OK) {
        /* try to not parse VUI */
        pres = gst_h265_parser_parse_sps (nalparser, nalu, &sps, FALSE);
        if (pres != GST_H265_PARSER_OK) {
          GST_WARNING_OBJECT (h265parse, "failed to parse SPS:");
          h265parse->state |= GST_H265_PARSE_STATE_GOT_SPS;
          h265parse->header = TRUE;
          return FALSE;
        }
        GST_WARNING_OBJECT (h265parse,
            "failed to parse VUI of SPS, ignore VUI");
      }

      GST_DEBUG_OBJECT (h265parse, "triggering src caps check");
      h265parse->update_caps = TRUE;
      h265parse->have_sps = TRUE;
      h265parse->have_sps_in_frame = TRUE;
      if (h265parse->push_codec && h265parse->have_pps) {
        /* SPS and PPS found in stream before the first pre_push_frame, no need
         * to forcibly push at start */
        GST_INFO_OBJECT (h265parse, "have SPS/PPS in stream");
        h265parse->push_codec = FALSE;
        h265parse->have_sps = FALSE;
        h265parse->have_pps = FALSE;
      }

      gst_h265_parser_store_nal (h265parse, sps.id, nal_type, nalu);
      h265parse->header = TRUE;
      h265parse->state |= GST_H265_PARSE_STATE_GOT_SPS;
      break;
    case GST_H265_NAL_PPS:
      pres = gst_h265_parser_parse_pps (nalparser, nalu, &pps);

      /* arranged for a fallback pps.id, so use that one and only warn */
      if (pres != GST_H265_PARSER_OK) {
        GST_WARNING_OBJECT (h265parse, "failed to parse PPS:");
        if (pres != GST_H265_PARSER_BROKEN_LINK)
          return FALSE;
      }

      /* parameters might have changed, force caps check */
      if (!h265parse->have_pps) {
        GST_DEBUG_OBJECT (h265parse, "triggering src caps check");
        h265parse->update_caps = TRUE;
      }
      h265parse->have_pps = TRUE;
      h265parse->have_pps_in_frame = TRUE;
      if (h265parse->push_codec && h265parse->have_sps) {
        /* SPS and PPS found in stream before the first pre_push_frame, no need
         * to forcibly push at start */
        GST_INFO_OBJECT (h265parse, "have SPS/PPS in stream");
        h265parse->push_codec = FALSE;
        h265parse->have_sps = FALSE;
        h265parse->have_pps = FALSE;
      }

      gst_h265_parser_store_nal (h265parse, pps.id, nal_type, nalu);
      h265parse->header = TRUE;
      h265parse->state |= GST_H265_PARSE_STATE_GOT_PPS;
      break;
    case GST_H265_NAL_PREFIX_SEI:
    case GST_H265_NAL_SUFFIX_SEI:
      /* expected state: got-sps */
      if (!GST_H265_PARSE_STATE_VALID (h265parse, GST_H265_PARSE_STATE_GOT_SPS))
        return FALSE;

      h265parse->header = TRUE;

      gst_h265_parse_process_sei (h265parse, nalu);

      /* mark SEI pos */
      if (nal_type == GST_H265_NAL_PREFIX_SEI && h265parse->sei_pos == -1) {
        if (h265parse->transform)
          h265parse->sei_pos = gst_adapter_available (h265parse->frame_out);
        else
          h265parse->sei_pos = nalu->sc_offset;
        GST_DEBUG_OBJECT (h265parse, "marking SEI in frame at offset %d",
            h265parse->sei_pos);
      }
      break;

    case GST_H265_NAL_SLICE_TRAIL_N:
    case GST_H265_NAL_SLICE_TRAIL_R:
    case GST_H265_NAL_SLICE_TSA_N:
    case GST_H265_NAL_SLICE_TSA_R:
    case GST_H265_NAL_SLICE_STSA_N:
    case GST_H265_NAL_SLICE_STSA_R:
    case GST_H265_NAL_SLICE_RADL_N:
    case GST_H265_NAL_SLICE_RADL_R:
    case GST_H265_NAL_SLICE_RASL_N:
    case GST_H265_NAL_SLICE_RASL_R:
    case GST_H265_NAL_SLICE_BLA_W_LP:
    case GST_H265_NAL_SLICE_BLA_W_RADL:
    case GST_H265_NAL_SLICE_BLA_N_LP:
    case GST_H265_NAL_SLICE_IDR_W_RADL:
    case GST_H265_NAL_SLICE_IDR_N_LP:
    case GST_H265_NAL_SLICE_CRA_NUT:
    {
      GstH265SliceHdr slice;
      gboolean is_irap;
      gboolean no_rasl_output_flag = FALSE;

      /* expected state: got-sps|got-pps (valid picture headers) */
      h265parse->state &= GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS;
      if (!GST_H265_PARSE_STATE_VALID (h265parse,
              GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS))
        return FALSE;

      /* This is similar to the GOT_SLICE state, but is only reset when the
       * AU is complete. This is used to keep track of AU */
      h265parse->picture_start = TRUE;

      pres = gst_h265_parser_parse_slice_hdr (nalparser, nalu, &slice);

      if (pres == GST_H265_PARSER_OK) {
        if (GST_H265_IS_I_SLICE (&slice))
          h265parse->keyframe = TRUE;
        else if (GST_H265_IS_P_SLICE (&slice))
          h265parse->predicted = TRUE;
        else if (GST_H265_IS_B_SLICE (&slice))
          h265parse->bidirectional = TRUE;

        h265parse->state |= GST_H265_PARSE_STATE_GOT_SLICE;
      }
      if (slice.first_slice_segment_in_pic_flag == 1)
        GST_DEBUG_OBJECT (h265parse,
            "frame start, first_slice_segment_in_pic_flag = 1");

      GST_DEBUG_OBJECT (h265parse,
          "parse result %d, first slice_segment: %u, slice type: %u",
          pres, slice.first_slice_segment_in_pic_flag, slice.type);

      gst_h265_slice_hdr_free (&slice);

      /* FIXME: NoRaslOutputFlag can be equal to 1 for CRA if
       * 1) the first AU in bitstream is CRA
       * 2) or the first AU following EOS nal is CRA
       * 3) or it has HandleCraAsBlaFlag equal to 1 */
      if (GST_H265_IS_NAL_TYPE_IDR (nal_type)) {
        /* NoRaslOutputFlag is equal to 1 for each IDR */
        no_rasl_output_flag = TRUE;
      } else if (GST_H265_IS_NAL_TYPE_BLA (nal_type)) {
        /* NoRaslOutputFlag is equal to 1 for each BLA */
        no_rasl_output_flag = TRUE;
      }

      is_irap = GST_H265_IS_NAL_TYPE_IRAP (nal_type);

      if (no_rasl_output_flag && is_irap
          && slice.first_slice_segment_in_pic_flag == 1) {
        if (h265parse->mastering_display_info_state ==
            GST_H265_PARSE_SEI_PARSED)
          h265parse->mastering_display_info_state = GST_H265_PARSE_SEI_ACTIVE;
        else if (h265parse->mastering_display_info_state ==
            GST_H265_PARSE_SEI_ACTIVE)
          h265parse->mastering_display_info_state = GST_H265_PARSE_SEI_EXPIRED;

        if (h265parse->content_light_level_state == GST_H265_PARSE_SEI_PARSED)
          h265parse->content_light_level_state = GST_H265_PARSE_SEI_ACTIVE;
        else if (h265parse->content_light_level_state ==
            GST_H265_PARSE_SEI_ACTIVE)
          h265parse->content_light_level_state = GST_H265_PARSE_SEI_EXPIRED;
      }
      if (G_LIKELY (!is_irap && !h265parse->push_codec))
        break;

      /* if we need to sneak codec NALs into the stream,
       * this is a good place, so fake it as IDR
       * (which should be at start anyway) */
      /* mark where config needs to go if interval expired */
      /* mind replacement buffer if applicable */
      if (h265parse->idr_pos == -1) {
        if (h265parse->transform)
          h265parse->idr_pos = gst_adapter_available (h265parse->frame_out);
        else
          h265parse->idr_pos = nalu->sc_offset;
        GST_DEBUG_OBJECT (h265parse, "marking IDR in frame at offset %d",
            h265parse->idr_pos);
      }
      /* if SEI preceeds (faked) IDR, then we have to insert config there */
      if (h265parse->sei_pos >= 0 && h265parse->idr_pos > h265parse->sei_pos) {
        h265parse->idr_pos = h265parse->sei_pos;
        GST_DEBUG_OBJECT (h265parse, "moved IDR mark to SEI position %d",
            h265parse->idr_pos);
      }
      break;
    }
    case GST_H265_NAL_FD:
      if (!GST_H265_PARSE_STATE_VALID (h265parse, GST_H265_PARSE_STATE_GOT_SPS)) {
        GST_DEBUG_OBJECT (h265parse, "dropping FD received before SPS");
        return FALSE;
      }
      pres = gst_h265_parser_parse_nal (nalparser, nalu);
      if (pres != GST_H265_PARSER_OK)
        return FALSE;
      break;
    case GST_H265_NAL_AUD:
    default:
      /* Just accumulate AU Delimiter, whether it's before SPS or not */
      pres = gst_h265_parser_parse_nal (nalparser, nalu);
      if (pres != GST_H265_PARSER_OK)
        return FALSE;
      break;
  }

  /* if HEVC output needed, collect properly prefixed nal in adapter,
   * and use that to replace outgoing buffer data later on */
  if (h265parse->transform) {
    GstBuffer *buf;

    GST_LOG_OBJECT (h265parse, "collecting NAL in HEVC frame");
    buf = gst_h265_parse_wrap_nal (h265parse, h265parse->format,
        nalu->data + nalu->offset, nalu->size);
    gst_adapter_push (h265parse->frame_out, buf);
  }

  return TRUE;
}

/* caller guarantees at least 3 bytes of nal payload for each nal
 * returns TRUE if next_nal indicates that nal terminates an AU */
static inline gboolean
gst_h265_parse_collect_nal (GstH265Parse * h265parse, const guint8 * data,
    guint size, GstH265NalUnit * nalu)
{
  GstH265NalUnitType nal_type = nalu->type;
  gboolean complete;

  /* determine if AU complete */
  GST_LOG_OBJECT (h265parse, "next nal type: %d %s (picture started %i)",
      nal_type, _nal_name (nal_type), h265parse->picture_start);

  /* consider a coded slices (IRAP or not) to start a picture,
   * (so ending the previous one) if first_slice_segment_in_pic_flag == 1*/
  complete = h265parse->picture_start && ((nal_type >= GST_H265_NAL_VPS
          && nal_type <= GST_H265_NAL_AUD)
      || nal_type == GST_H265_NAL_PREFIX_SEI || (nal_type >= 41
          && nal_type <= 44) || (nal_type >= 48 && nal_type <= 55));

  /* Any VCL Nal unit with first_slice_segment_in_pic_flag == 1 considered start of frame */
  if (nalu->size > nalu->header_bytes) {
    complete |= h265parse->picture_start
        && (((nal_type >= GST_H265_NAL_SLICE_TRAIL_N
                && nal_type <= GST_H265_NAL_SLICE_RASL_R)
            || GST_H265_IS_NAL_TYPE_IRAP (nal_type))
        && (nalu->data[nalu->offset + 2] & 0x80));
  }

  GST_LOG_OBJECT (h265parse, "au complete: %d", complete);

  if (complete)
    h265parse->picture_start = FALSE;

  return complete;
}

static GstFlowReturn
gst_h265_parse_handle_frame_packetized (GstBaseParse * parse,
    GstBaseParseFrame * frame)
{
  GstH265Parse *h265parse = GST_H265_PARSE (parse);
  GstBuffer *buffer = frame->buffer;
  GstFlowReturn ret = GST_FLOW_OK;
  GstH265ParserResult parse_res;
  GstH265NalUnit nalu;
  const guint nl = h265parse->nal_length_size;
  GstMapInfo map;
  gint left;

  if (nl < 1 || nl > 4) {
    GST_DEBUG_OBJECT (h265parse, "insufficient data to split input");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  /* need to save buffer from invalidation upon _finish_frame */
  if (h265parse->split_packetized)
    buffer = gst_buffer_copy (frame->buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  left = map.size;

  GST_LOG_OBJECT (h265parse,
      "processing packet buffer of size %" G_GSIZE_FORMAT, map.size);

  parse_res = gst_h265_parser_identify_nalu_hevc (h265parse->nalparser,
      map.data, 0, map.size, nl, &nalu);

  while (parse_res == GST_H265_PARSER_OK) {
    GST_DEBUG_OBJECT (h265parse, "HEVC nal offset %d", nalu.offset + nalu.size);

    /* either way, have a look at it */
    gst_h265_parse_process_nal (h265parse, &nalu);

    /* dispatch per NALU if needed */
    if (h265parse->split_packetized) {
      GstBaseParseFrame tmp_frame;

      gst_base_parse_frame_init (&tmp_frame);
      tmp_frame.flags |= frame->flags;
      tmp_frame.offset = frame->offset;
      tmp_frame.overhead = frame->overhead;
      tmp_frame.buffer = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL,
          nalu.offset, nalu.size);
      /* Don't lose timestamp when offset is not 0. */
      GST_BUFFER_PTS (tmp_frame.buffer) = GST_BUFFER_PTS (buffer);
      GST_BUFFER_DTS (tmp_frame.buffer) = GST_BUFFER_DTS (buffer);
      GST_BUFFER_DURATION (tmp_frame.buffer) = GST_BUFFER_DURATION (buffer);

      /* Set marker on last packet */
      if (nl + nalu.size == left) {
        if (GST_BUFFER_FLAG_IS_SET (frame->buffer, GST_BUFFER_FLAG_MARKER))
          h265parse->marker = TRUE;
      }

      /* note we don't need to come up with a sub-buffer, since
       * subsequent code only considers input buffer's metadata.
       * Real data is either taken from input by baseclass or
       * a replacement output buffer is provided anyway. */
      gst_h265_parse_parse_frame (parse, &tmp_frame);
      ret = gst_base_parse_finish_frame (parse, &tmp_frame, nl + nalu.size);
      left -= nl + nalu.size;
    }

    parse_res = gst_h265_parser_identify_nalu_hevc (h265parse->nalparser,
        map.data, nalu.offset + nalu.size, map.size, nl, &nalu);
  }

  gst_buffer_unmap (buffer, &map);

  if (!h265parse->split_packetized) {
    h265parse->marker = TRUE;
    gst_h265_parse_parse_frame (parse, frame);
    ret = gst_base_parse_finish_frame (parse, frame, map.size);
  } else {
    gst_buffer_unref (buffer);
    if (G_UNLIKELY (left)) {
      /* should not be happening for nice HEVC */
      GST_WARNING_OBJECT (parse, "skipping leftover HEVC data %d", left);
      frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
      ret = gst_base_parse_finish_frame (parse, frame, map.size);
    }
  }

  if (parse_res == GST_H265_PARSER_NO_NAL_END ||
      parse_res == GST_H265_PARSER_BROKEN_DATA) {

    if (h265parse->split_packetized) {
      GST_ELEMENT_ERROR (h265parse, STREAM, FAILED, (NULL),
          ("invalid HEVC input data"));

      return GST_FLOW_ERROR;
    } else {
      /* do not meddle to much in this case */
      GST_DEBUG_OBJECT (h265parse, "parsing packet failed");
    }
  }

  return ret;
}

static GstFlowReturn
gst_h265_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstH265Parse *h265parse = GST_H265_PARSE (parse);
  GstBuffer *buffer = frame->buffer;
  GstMapInfo map;
  guint8 *data;
  gsize size;
  gint current_off = 0;
  gboolean drain, nonext;
  GstH265Parser *nalparser = h265parse->nalparser;
  GstH265NalUnit nalu;
  GstH265ParserResult pres;
  gint framesize;

  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (frame->buffer,
              GST_BUFFER_FLAG_DISCONT))) {
    h265parse->discont = TRUE;
  }

  /* delegate in packetized case, no skipping should be needed */
  if (h265parse->packetized)
    return gst_h265_parse_handle_frame_packetized (parse, frame);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  data = map.data;
  size = map.size;

  /* expect at least 3 bytes start_code, and 2 bytes NALU header.
   * the length of the NALU payload can be zero.
   * (e.g. EOS/EOB placed at the end of an AU.) */
  if (G_UNLIKELY (size < 5)) {
    gst_buffer_unmap (buffer, &map);
    *skipsize = 1;
    return GST_FLOW_OK;
  }

  /* need to configure aggregation */
  if (G_UNLIKELY (h265parse->format == GST_H265_PARSE_FORMAT_NONE))
    gst_h265_parse_negotiate (h265parse, GST_H265_PARSE_FORMAT_BYTE, NULL);

  /* avoid stale cached parsing state */
  if (frame->flags & GST_BASE_PARSE_FRAME_FLAG_NEW_FRAME) {
    GST_LOG_OBJECT (h265parse, "parsing new frame");
    gst_h265_parse_reset_frame (h265parse);
  } else {
    GST_LOG_OBJECT (h265parse, "resuming frame parsing");
  }

  /* Always consume the entire input buffer when in_align == ALIGN_AU */
  drain = GST_BASE_PARSE_DRAINING (parse)
      || h265parse->in_align == GST_H265_PARSE_ALIGN_AU;
  nonext = FALSE;

  current_off = h265parse->current_off;
  if (current_off < 0)
    current_off = 0;

  /* The parser is being drain, but no new data was added, just prentend this
   * AU is complete */
  if (drain && current_off == size) {
    GST_DEBUG_OBJECT (h265parse, "draining with no new data");
    nalu.size = 0;
    nalu.offset = current_off;
    goto end;
  }

  g_assert (current_off < size);
  GST_DEBUG_OBJECT (h265parse, "last parse position %d", current_off);

  /* check for initial skip */
  if (h265parse->current_off == -1) {
    pres =
        gst_h265_parser_identify_nalu_unchecked (nalparser, data, current_off,
        size, &nalu);
    switch (pres) {
      case GST_H265_PARSER_OK:
        if (nalu.sc_offset > 0) {
          *skipsize = nalu.sc_offset;
          goto skip;
        }
        break;
      case GST_H265_PARSER_NO_NAL:
        /* start code may have up to 4 bytes, and we may also get that return
         * value if only one of the two header bytes are present, make sure
         * not to skip too much */
        *skipsize = size - 5;
        goto skip;
      default:
        /* should not really occur either */
        GST_ELEMENT_ERROR (h265parse, STREAM, FORMAT,
            ("Error parsing H.265 stream"), ("Invalid H.265 stream"));
        goto invalid_stream;
    }

    /* Ensure we use the TS of the first NAL. This avoids broken timestamp in
     * the case of a miss-placed filler byte. */
    gst_base_parse_set_ts_at_offset (parse, nalu.offset);
  }

  while (TRUE) {
    pres =
        gst_h265_parser_identify_nalu (nalparser, data, current_off, size,
        &nalu);

    switch (pres) {
      case GST_H265_PARSER_OK:
        GST_DEBUG_OBJECT (h265parse, "complete nal (offset, size): (%u, %u) ",
            nalu.offset, nalu.size);
        break;
      case GST_H265_PARSER_NO_NAL_END:
        /* In NAL alignment, assume the NAL is complete */
        if (h265parse->in_align == GST_H265_PARSE_ALIGN_NAL ||
            h265parse->in_align == GST_H265_PARSE_ALIGN_AU) {
          nonext = TRUE;
          nalu.size = size - nalu.offset;
          break;
        }
        GST_DEBUG_OBJECT (h265parse, "not a complete nal found at offset %u",
            nalu.offset);
        /* if draining, accept it as complete nal */
        if (drain) {
          nonext = TRUE;
          nalu.size = size - nalu.offset;
          GST_DEBUG_OBJECT (h265parse, "draining, accepting with size %u",
              nalu.size);
          /* if it's not too short at least */
          if (nalu.size < 3)
            goto broken;
          break;
        }
        /* otherwise need more */
        goto more;
      case GST_H265_PARSER_BROKEN_LINK:
        GST_ELEMENT_ERROR (h265parse, STREAM, FORMAT,
            ("Error parsing H.265 stream"),
            ("The link to structure needed for the parsing couldn't be found"));
        goto invalid_stream;
      case GST_H265_PARSER_ERROR:
        /* should not really occur either */
        GST_ELEMENT_ERROR (h265parse, STREAM, FORMAT,
            ("Error parsing H.265 stream"), ("Invalid H.265 stream"));
        goto invalid_stream;
      case GST_H265_PARSER_NO_NAL:
        GST_ELEMENT_ERROR (h265parse, STREAM, FORMAT,
            ("Error parsing H.265 stream"), ("No H.265 NAL unit found"));
        goto invalid_stream;
      case GST_H265_PARSER_BROKEN_DATA:
        GST_WARNING_OBJECT (h265parse, "input stream is corrupt; "
            "it contains a NAL unit of length %u", nalu.size);
      broken:
        /* broken nal at start -> arrange to skip it,
         * otherwise have it terminate current au
         * (and so it will be skipped on next frame round) */
        if (current_off == 0) {
          GST_DEBUG_OBJECT (h265parse, "skipping broken nal");
          *skipsize = nalu.offset;
          goto skip;
        } else {
          GST_DEBUG_OBJECT (h265parse, "terminating au");
          nalu.size = 0;
          nalu.offset = nalu.sc_offset;
          goto end;
        }
      default:
        g_assert_not_reached ();
        break;
    }

    GST_DEBUG_OBJECT (h265parse, "%p complete nal found. Off: %u, Size: %u",
        data, nalu.offset, nalu.size);

    if (gst_h265_parse_collect_nal (h265parse, data, size, &nalu)) {
      /* complete current frame, if it exist */
      if (current_off > 0) {
        nalu.size = 0;
        nalu.offset = nalu.sc_offset;
        h265parse->marker = TRUE;
        break;
      }
    }

    if (!gst_h265_parse_process_nal (h265parse, &nalu)) {
      GST_WARNING_OBJECT (h265parse,
          "broken/invalid nal Type: %d %s, Size: %u will be dropped",
          nalu.type, _nal_name (nalu.type), nalu.size);
      *skipsize = nalu.size;
      goto skip;
    }

    /* Do not push immediatly if we don't have all headers. This ensure that
     * our caps are complete, avoiding a renegotiation */
    if (h265parse->align == GST_H265_PARSE_ALIGN_NAL &&
        !GST_H265_PARSE_STATE_VALID (h265parse,
            GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS))
      frame->flags |= GST_BASE_PARSE_FRAME_FLAG_QUEUE;

    if (nonext) {
      /* If there is a marker flag, or input is AU, we know this is complete */
      if (GST_BUFFER_FLAG_IS_SET (frame->buffer, GST_BUFFER_FLAG_MARKER) ||
          h265parse->in_align == GST_H265_PARSE_ALIGN_AU) {
        h265parse->marker = TRUE;
        break;
      }

      /* or if we are draining or producing NALs */
      if (drain || h265parse->align == GST_H265_PARSE_ALIGN_NAL)
        break;

      current_off = nalu.offset + nalu.size;
      goto more;
    }

    /* If the output is NAL, we are done */
    if (h265parse->align == GST_H265_PARSE_ALIGN_NAL)
      break;

    GST_DEBUG_OBJECT (h265parse, "Looking for more");
    current_off = nalu.offset + nalu.size;

    /* expect at least 3 bytes start_code, and 2 bytes NALU header.
     * the length of the NALU payload can be zero.
     * (e.g. EOS/EOB placed at the end of an AU.) */
    if (G_UNLIKELY (size - current_off < 5)) {
      /* Finish the frame if there is no more data in the stream */
      if (drain)
        break;

      goto more;
    }
  }

end:
  framesize = nalu.offset + nalu.size;

  gst_buffer_unmap (buffer, &map);

  gst_h265_parse_parse_frame (parse, frame);

  return gst_base_parse_finish_frame (parse, frame, framesize);

more:
  *skipsize = 0;

  /* Restart parsing from here next time */
  if (current_off > 0)
    h265parse->current_off = current_off;

  /* Fall-through. */
out:
  gst_buffer_unmap (buffer, &map);
  return GST_FLOW_OK;

skip:
  GST_DEBUG_OBJECT (h265parse, "skipping %d", *skipsize);
  /* If we are collecting access units, we need to preserve the initial
   * config headers (SPS, PPS et al.) and only reset the frame if another
   * slice NAL was received. This means that broken pictures are discarded */
  if (h265parse->align != GST_H265_PARSE_ALIGN_AU ||
      !(h265parse->state & GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS) ||
      (h265parse->state & GST_H265_PARSE_STATE_GOT_SLICE))
    gst_h265_parse_reset_frame (h265parse);
  goto out;

invalid_stream:
  gst_buffer_unmap (buffer, &map);
  return GST_FLOW_ERROR;
}

/* byte together hevc codec data based on collected pps and sps so far */
static GstBuffer *
gst_h265_parse_make_codec_data (GstH265Parse * h265parse)
{
  GstBuffer *buf, *nal;
  gint i, j, k = 0;
  guint vps_size = 0, sps_size = 0, pps_size = 0;
  guint num_vps = 0, num_sps = 0, num_pps = 0;
  gboolean found = FALSE;
  GstMapInfo map;
  guint8 *data;
  gint nl;
  guint8 num_arrays = 0;
  GstH265SPS *sps = NULL;
  guint16 min_spatial_segmentation_idc = 0;
  GstH265ProfileTierLevel *pft;

  /* only nal payload in stored nals */
  /* Fixme: Current implementation is not embedding SEI in codec_data */
  for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
    if ((nal = h265parse->vps_nals[i])) {
      num_vps++;
      /* size bytes also count */
      vps_size += gst_buffer_get_size (nal) + 2;
    }
  }
  if (num_vps > 0)
    num_arrays++;

  for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
    if ((nal = h265parse->sps_nals[i])) {
      num_sps++;
      /* size bytes also count */
      sps_size += gst_buffer_get_size (nal) + 2;
      found = TRUE;
    }
  }
  if (num_sps > 0)
    num_arrays++;

  for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
    if ((nal = h265parse->pps_nals[i])) {
      num_pps++;
      /* size bytes also count */
      pps_size += gst_buffer_get_size (nal) + 2;
    }
  }
  if (num_pps > 0)
    num_arrays++;

  GST_DEBUG_OBJECT (h265parse,
      "constructing codec_data: num_vps =%d num_sps=%d, num_pps=%d", num_vps,
      num_sps, num_pps);

  if (!found)
    return NULL;

  sps = h265parse->nalparser->last_sps;
  if (!sps)
    return NULL;

  buf =
      gst_buffer_new_allocate (NULL,
      23 + (3 * num_arrays) + vps_size + sps_size + pps_size, NULL);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;
  memset (data, 0, map.size);
  nl = h265parse->nal_length_size;

  pft = &sps->profile_tier_level;
  if (sps->vui_parameters_present_flag)
    min_spatial_segmentation_idc = sps->vui_params.min_spatial_segmentation_idc;

  /* HEVCDecoderConfigurationVersion = 1
   * profile_space | tier_flat | profile_idc |
   * profile_compatibility_flags | constraint_indicator_flags |
   * level_idc */
  data[0] = 1;
  data[1] =
      (pft->profile_space << 5) | (pft->tier_flag << 5) | pft->profile_idc;
  for (i = 2; i < 6; i++) {
    for (j = 7; j >= 0; j--) {
      data[i] |= (pft->profile_compatibility_flag[k] << j);
      k++;
    }
  }

  data[6] =
      (pft->progressive_source_flag << 7) |
      (pft->interlaced_source_flag << 6) |
      (pft->non_packed_constraint_flag << 5) |
      (pft->frame_only_constraint_flag << 4) |
      (pft->max_12bit_constraint_flag << 3) |
      (pft->max_10bit_constraint_flag << 2) |
      (pft->max_8bit_constraint_flag << 1) |
      (pft->max_422chroma_constraint_flag);

  data[7] =
      (pft->max_420chroma_constraint_flag << 7) |
      (pft->max_monochrome_constraint_flag << 6) |
      (pft->intra_constraint_flag << 5) |
      (pft->one_picture_only_constraint_flag << 4) |
      (pft->lower_bit_rate_constraint_flag << 3) |
      (pft->max_14bit_constraint_flag << 2);

  data[12] = pft->level_idc;
  /* min_spatial_segmentation_idc */
  GST_WRITE_UINT16_BE (data + 13, min_spatial_segmentation_idc);
  data[13] |= 0xf0;
  data[15] = 0xfc;              /* keeping parrallelismType as zero (unknown) */
  data[16] = 0xfc | sps->chroma_format_idc;
  data[17] = 0xf8 | sps->bit_depth_luma_minus8;
  data[18] = 0xf8 | sps->bit_depth_chroma_minus8;
  data[19] = 0x00;              /* keep avgFrameRate as unspecified */
  data[20] = 0x00;              /* keep avgFrameRate as unspecified */
  /* constFrameRate(2 bits): 0, stream may or may not be of constant framerate
   * numTemporalLayers (3 bits): number of temporal layers, value from SPS
   * TemporalIdNested (1 bit): sps_temporal_id_nesting_flag from SPS
   * lengthSizeMinusOne (2 bits): plus 1 indicates the length of the NALUnitLength */
  data[21] =
      0x00 | ((sps->max_sub_layers_minus1 +
          1) << 3) | (sps->temporal_id_nesting_flag << 2) | (nl - 1);
  GST_WRITE_UINT8 (data + 22, num_arrays);      /* numOfArrays */

  data += 23;

  /* VPS */
  if (num_vps > 0) {
    /* array_completeness | reserved_zero bit | nal_unit_type */
    data[0] = 0x00 | 0x20;
    data++;

    GST_WRITE_UINT16_BE (data, num_vps);
    data += 2;

    for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
      if ((nal = h265parse->vps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (nal);
        GST_WRITE_UINT16_BE (data, nal_size);
        gst_buffer_extract (nal, 0, data + 2, nal_size);
        data += 2 + nal_size;
      }
    }
  }

  /* SPS */
  if (num_sps > 0) {
    /* array_completeness | reserved_zero bit | nal_unit_type */
    data[0] = 0x00 | 0x21;
    data++;

    GST_WRITE_UINT16_BE (data, num_sps);
    data += 2;

    for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
      if ((nal = h265parse->sps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (nal);
        GST_WRITE_UINT16_BE (data, nal_size);
        gst_buffer_extract (nal, 0, data + 2, nal_size);
        data += 2 + nal_size;
      }
    }
  }

  /* PPS */
  if (num_pps > 0) {
    /* array_completeness | reserved_zero bit | nal_unit_type */
    data[0] = 0x00 | 0x22;
    data++;

    GST_WRITE_UINT16_BE (data, num_pps);
    data += 2;

    for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
      if ((nal = h265parse->pps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (nal);
        GST_WRITE_UINT16_BE (data, nal_size);
        gst_buffer_extract (nal, 0, data + 2, nal_size);
        data += 2 + nal_size;
      }
    }
  }
  gst_buffer_unmap (buf, &map);

  return buf;
}

static void
gst_h265_parse_get_par (GstH265Parse * h265parse, gint * num, gint * den)
{
  if (h265parse->upstream_par_n != -1 && h265parse->upstream_par_d != -1) {
    *num = h265parse->upstream_par_n;
    *den = h265parse->upstream_par_d;
  } else {
    *num = h265parse->parsed_par_n;
    *den = h265parse->parsed_par_d;
  }
}

static const gchar *
digit_to_string (guint digit)
{
  static const char itoa[][2] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
  };

  if (G_LIKELY (digit < 10))
    return itoa[digit];
  else
    return NULL;
}

static const gchar *
get_tier_string (guint8 tier_flag)
{
  const gchar *tier = NULL;

  if (tier_flag)
    tier = "high";
  else
    tier = "main";

  return tier;
}

static const gchar *
get_level_string (guint8 level_idc)
{
  if (level_idc == 0)
    return NULL;
  else if (level_idc % 30 == 0)
    return digit_to_string (level_idc / 30);
  else {
    switch (level_idc) {
      case GST_H265_LEVEL_L2_1:
        return "2.1";
        break;
      case GST_H265_LEVEL_L3_1:
        return "3.1";
        break;
      case GST_H265_LEVEL_L4_1:
        return "4.1";
        break;
      case GST_H265_LEVEL_L5_1:
        return "5.1";
        break;
      case GST_H265_LEVEL_L5_2:
        return "5.2";
        break;
      case GST_H265_LEVEL_L6_1:
        return "6.1";
        break;
      case GST_H265_LEVEL_L6_2:
        return "6.2";
        break;
      default:
        return NULL;
    }
  }
}

static inline guint64
profile_to_flag (GstH265Profile p)
{
  return (guint64) 1 << (guint64) p;
}

static GstCaps *
get_compatible_profile_caps (GstH265SPS * sps, GstH265Profile profile)
{
  GstCaps *caps = NULL;
  gint i;
  GValue compat_profiles = G_VALUE_INIT;
  guint64 profiles = 0;

  g_value_init (&compat_profiles, GST_TYPE_LIST);

  /* Relaxing profiles condition based on decoder capability specified by spec */
  if (sps->profile_tier_level.profile_compatibility_flag[1])
    profiles |= profile_to_flag (GST_H265_PROFILE_MAIN);

  if (sps->profile_tier_level.profile_compatibility_flag[2])
    profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10);

  if (sps->profile_tier_level.profile_compatibility_flag[3])
    profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_STILL_PICTURE);

  switch (profile) {
    case GST_H265_PROFILE_MAIN_10:
    {
      /* A.3.5 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);

      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);

      /* H.11.1.1 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN_10);
      break;
    }
    case GST_H265_PROFILE_MAIN:
    {
      /* A.3.3 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10);

      /* A.3.5 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);

      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);

      /* G.11.1.1 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MULTIVIEW_MAIN);

      /* H.11.1.1 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN_10);

      /* I.11.1.1 */
      profiles |= profile_to_flag (GST_H265_PROFILE_3D_MAIN);
      break;
    }
    case GST_H265_PROFILE_MAIN_STILL_PICTURE:
    {
      /* A.3.2, A.3.4 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10);

      /* A.3.5 */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);

      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_16_INTRA);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_STILL_PICTURE);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_16_STILL_PICTURE);

      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);
      break;
    }
    case GST_H265_PROFILE_MONOCHROME:
    {
      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);
      break;
    }
    case GST_H265_PROFILE_MAIN_444:
    {
      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      break;
    }
    case GST_H265_PROFILE_MAIN_444_10:
    {
      /* A.3.7 */
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      break;
    }
    case GST_H265_PROFILE_HIGH_THROUGHPUT_444:
    {
      /* A.3.7 */
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);
      break;
    }
    case GST_H265_PROFILE_HIGH_THROUGHPUT_444_10:
    {
      /* A.3.7 */
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);
      break;
    }
    case GST_H265_PROFILE_HIGH_THROUGHPUT_444_14:
    {
      /* A.3.7 */
      profiles |=
          profile_to_flag
          (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);
      break;
    }
      /* All the -intra profiles can map to non-intra profiles, except
         the monochrome case for main and main-10. */
    case GST_H265_PROFILE_MAIN_INTRA:
    {
      if (sps->chroma_format_idc == 1) {
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN);

        /* Add all main compatible profiles without monochrome. */
        /* A.3.3 */
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10);

        /* A.3.5 */
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);

        /* A.3.7 */
        profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN);
        profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);
        profiles |=
            profile_to_flag
            (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444);
        profiles |=
            profile_to_flag
            (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_10);
        profiles |=
            profile_to_flag
            (GST_H265_PROFILE_SCREEN_EXTENDED_HIGH_THROUGHPUT_444_14);

        /* G.11.1.1 */
        profiles |= profile_to_flag (GST_H265_PROFILE_MULTIVIEW_MAIN);

        /* H.11.1.1 */
        profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN);
        profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN_10);

        /* I.11.1.1 */
        profiles |= profile_to_flag (GST_H265_PROFILE_3D_MAIN);
      }

      /* Add all main compatible profiles with monochrome. */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      break;
    }
    case GST_H265_PROFILE_MAIN_10_INTRA:
    {
      if (sps->chroma_format_idc == 1) {
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_10);

        /* Add all main-10 compatible profiles without monochrome. */
        /* A.3.5 */
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);
        profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);

        /* A.3.7 */
        profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_10);

        /* H.11.1.1 */
        profiles |= profile_to_flag (GST_H265_PROFILE_SCALABLE_MAIN_10);
      }

      /* Add all main-10 compatible profiles with monochrome. */
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      break;
    }
    case GST_H265_PROFILE_MAIN_12_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_12);
      break;
    case GST_H265_PROFILE_MAIN_422_10_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_10);
      break;
    case GST_H265_PROFILE_MAIN_422_12_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_422_12);
      break;
    case GST_H265_PROFILE_MAIN_444_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444);

      /* Add all main444 compatible profiles. */
      /* A.3.7 */
      profiles |= profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444);
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      break;
    case GST_H265_PROFILE_MAIN_444_10_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_10);

      /* Add all main444-10 compatible profiles. */
      /* A.3.7 */
      profiles |=
          profile_to_flag (GST_H265_PROFILE_SCREEN_EXTENDED_MAIN_444_10);
      break;
    case GST_H265_PROFILE_MAIN_444_12_INTRA:
      profiles |= profile_to_flag (GST_H265_PROFILE_MAIN_444_12);
      break;
    default:
      break;
  }

  if (profiles) {
    GValue value = G_VALUE_INIT;
    const gchar *profile_str;
    caps = gst_caps_new_empty_simple ("video/x-h265");

    for (i = GST_H265_PROFILE_MAIN; i < GST_H265_PROFILE_MAX; i++) {
      if ((profiles & profile_to_flag (i)) == profile_to_flag (i)) {
        profile_str = gst_h265_profile_to_string (i);

        if (G_UNLIKELY (profile_str == NULL)) {
          GST_FIXME ("Unhandled profile index %d", i);
          continue;
        }

        g_value_init (&value, G_TYPE_STRING);
        g_value_set_string (&value, profile_str);
        gst_value_list_append_value (&compat_profiles, &value);
        g_value_unset (&value);
      }
    }

    gst_caps_set_value (caps, "profile", &compat_profiles);
    g_value_unset (&compat_profiles);
  }

  return caps;
}

static void
fix_invalid_profile (GstH265Parse * h265parse, GstCaps * caps, GstH265SPS * sps)
{
  /* HACK: This is a work-around to identify some main profile streams
   * having wrong profile_idc. There are some wrongly encoded main profile
   * streams which doesn't have any of the profile_idc values mentioned in
   * Annex-A. Just assuming them as MAIN profile for now if they meet the
   * A.3.2 requirement. */
  if (sps->chroma_format_idc == 1 && sps->bit_depth_luma_minus8 == 0 &&
      sps->bit_depth_chroma_minus8 == 0 && sps->sps_extension_flag == 0) {
    gst_caps_set_simple (caps, "profile", G_TYPE_STRING, "main", NULL);
    GST_WARNING_OBJECT (h265parse,
        "Wrong profile_idc = 0, setting it as main profile !!");
  }
}

/* if downstream didn't support the exact profile indicated in sps header,
 * check for the compatible profiles also */
static void
ensure_caps_profile (GstH265Parse * h265parse, GstCaps * caps, GstH265SPS * sps,
    GstH265Profile profile)
{
  GstCaps *peer_caps, *compat_caps;

  if (profile == GST_H265_PROFILE_INVALID)
    fix_invalid_profile (h265parse, caps, sps);

  peer_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SRC_PAD (h265parse));
  if (!peer_caps || !gst_caps_can_intersect (caps, peer_caps)) {
    GstCaps *filter_caps = gst_caps_new_empty_simple ("video/x-h265");

    if (peer_caps)
      gst_caps_unref (peer_caps);
    peer_caps =
        gst_pad_peer_query_caps (GST_BASE_PARSE_SRC_PAD (h265parse),
        filter_caps);

    gst_caps_unref (filter_caps);
  }

  if (peer_caps && !gst_caps_can_intersect (caps, peer_caps)) {
    GstStructure *structure;

    compat_caps = get_compatible_profile_caps (sps, profile);
    if (compat_caps != NULL) {
      GstCaps *res_caps = NULL;

      res_caps = gst_caps_intersect (peer_caps, compat_caps);

      if (res_caps && !gst_caps_is_empty (res_caps)) {
        const gchar *profile_str = NULL;

        res_caps = gst_caps_fixate (res_caps);
        structure = gst_caps_get_structure (res_caps, 0);
        profile_str = gst_structure_get_string (structure, "profile");
        if (profile_str) {
          gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_str,
              NULL);
          GST_DEBUG_OBJECT (h265parse,
              "Setting compatible profile %s to the caps", profile_str);
        }
      }
      if (res_caps)
        gst_caps_unref (res_caps);
      gst_caps_unref (compat_caps);
    }
  }
  if (peer_caps)
    gst_caps_unref (peer_caps);
}

static gboolean
gst_h265_parse_is_field_interlaced (GstH265Parse * h265parse)
{
  /* FIXME: The SEI is optional, so theoretically there could be files with
   * the interlaced_source_flag set to TRUE but no SEI present, or SEI present
   * but no pic_struct. Haven't seen any such files in practice, and we don't
   * know how to interpret the data without the pic_struct, so we'll treat
   * them as progressive */

  switch (h265parse->sei_pic_struct) {
    case GST_H265_SEI_PIC_STRUCT_TOP_FIELD:
    case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_PREVIOUS_BOTTOM:
    case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_NEXT_BOTTOM:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_FIELD:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_PREVIOUS_TOP:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_NEXT_TOP:
      return TRUE;
      break;
    default:
      break;
  }

  return FALSE;
}

static void
gst_h265_parse_update_src_caps (GstH265Parse * h265parse, GstCaps * caps)
{
  GstH265SPS *sps = NULL;
  GstCaps *sink_caps, *src_caps;
  gboolean modified = FALSE;
  GstBuffer *buf = NULL;
  GstStructure *s = NULL;
  gint width, height;

  if (G_UNLIKELY (!gst_pad_has_current_caps (GST_BASE_PARSE_SRC_PAD
              (h265parse))))
    modified = TRUE;
  else if (G_UNLIKELY (!h265parse->update_caps))
    return;

  /* if this is being called from the first _setcaps call, caps on the sinkpad
   * aren't set yet and so they need to be passed as an argument */
  if (caps)
    sink_caps = gst_caps_ref (caps);
  else
    sink_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (h265parse));

  /* carry over input caps as much as possible; override with our own stuff */
  if (!sink_caps)
    sink_caps = gst_caps_new_empty_simple ("video/x-h265");
  else
    s = gst_caps_get_structure (sink_caps, 0);

  sps = h265parse->nalparser->last_sps;
  GST_DEBUG_OBJECT (h265parse, "sps: %p", sps);

  /* only codec-data for nice-and-clean au aligned packetized hevc format */
  if ((h265parse->format == GST_H265_PARSE_FORMAT_HVC1
          || h265parse->format == GST_H265_PARSE_FORMAT_HEV1)
      && h265parse->align == GST_H265_PARSE_ALIGN_AU) {
    buf = gst_h265_parse_make_codec_data (h265parse);
    if (buf && h265parse->codec_data) {
      GstMapInfo map;

      gst_buffer_map (buf, &map, GST_MAP_READ);
      if (map.size != gst_buffer_get_size (h265parse->codec_data) ||
          gst_buffer_memcmp (h265parse->codec_data, 0, map.data, map.size))
        modified = TRUE;

      gst_buffer_unmap (buf, &map);
    } else {
      if (!buf && h265parse->codec_data_in)
        buf = gst_buffer_ref (h265parse->codec_data_in);
      modified = TRUE;
    }
  }

  caps = NULL;
  if (G_UNLIKELY (!sps)) {
    caps = gst_caps_copy (sink_caps);
  } else {
    gint crop_width, crop_height;
    const gchar *chroma_format = NULL;
    guint bit_depth_chroma;
    GstH265VPS *vps = sps->vps;
    GstH265VUIParams *vui = &sps->vui_params;
    gchar *colorimetry = NULL;

    GST_DEBUG_OBJECT (h265parse, "vps: %p", vps);

    if (sps->conformance_window_flag) {
      crop_width = sps->crop_rect_width;
      crop_height = sps->crop_rect_height;
    } else {
      crop_width = sps->width;
      crop_height = sps->height;
    }
    if (gst_h265_parse_is_field_interlaced (h265parse)) {
      crop_height *= 2;
    }

    if (G_UNLIKELY (h265parse->width != crop_width ||
            h265parse->height != crop_height)) {
      h265parse->width = crop_width;
      h265parse->height = crop_height;
      GST_INFO_OBJECT (h265parse, "resolution changed %dx%d",
          h265parse->width, h265parse->height);
      modified = TRUE;
    }

    /* 0/1 is set as the default in the codec parser */
    if (vui->timing_info_present_flag && !h265parse->framerate_from_caps) {
      gint fps_num = 0, fps_den = 1;

      if (!(sps->fps_num == 0 && sps->fps_den == 1)) {
        fps_num = sps->fps_num;
        fps_den = sps->fps_den;
      } else if (!(sps->vui_params.time_scale == 0 &&
              sps->vui_params.num_units_in_tick == 1)) {
        fps_num = sps->vui_params.time_scale;
        fps_den = sps->vui_params.num_units_in_tick;

        if (gst_h265_parse_is_field_interlaced (h265parse)) {
          gint new_fps_num, new_fps_den;

          if (!gst_util_fraction_multiply (fps_num, fps_den, 1, 2, &new_fps_num,
                  &new_fps_den)) {
            GST_WARNING_OBJECT (h265parse, "Error calculating the new framerate"
                " - integer overflow; setting it to 0/1");
            fps_num = 0;
            fps_den = 1;
          } else {
            fps_num = new_fps_num;
            fps_den = new_fps_den;
          }
        }
      }

      if (G_UNLIKELY (h265parse->fps_num != fps_num
              || h265parse->fps_den != fps_den)) {
        GST_INFO_OBJECT (h265parse, "framerate changed %d/%d",
            fps_num, fps_den);
        h265parse->fps_num = fps_num;
        h265parse->fps_den = fps_den;
        modified = TRUE;
      }
    }

    if (vui->aspect_ratio_info_present_flag) {
      if (G_UNLIKELY ((h265parse->parsed_par_n != vui->par_n)
              && (h265parse->parsed_par_d != sps->vui_params.par_d))) {
        h265parse->parsed_par_n = vui->par_n;
        h265parse->parsed_par_d = vui->par_d;
        GST_INFO_OBJECT (h265parse, "pixel aspect ratio has been changed %d/%d",
            h265parse->parsed_par_n, h265parse->parsed_par_d);
        modified = TRUE;
      }
    }

    if (vui->video_signal_type_present_flag &&
        vui->colour_description_present_flag) {
      GstVideoColorimetry ci = { 0, };
      gchar *old_colorimetry = NULL;

      if (vui->video_full_range_flag)
        ci.range = GST_VIDEO_COLOR_RANGE_0_255;
      else
        ci.range = GST_VIDEO_COLOR_RANGE_16_235;

      ci.matrix = gst_video_color_matrix_from_iso (vui->matrix_coefficients);
      ci.transfer =
          gst_video_transfer_function_from_iso (vui->transfer_characteristics);
      ci.primaries = gst_video_color_primaries_from_iso (vui->colour_primaries);

      old_colorimetry =
          gst_video_colorimetry_to_string (&h265parse->parsed_colorimetry);
      colorimetry = gst_video_colorimetry_to_string (&ci);

      if (colorimetry && g_strcmp0 (old_colorimetry, colorimetry)) {
        GST_INFO_OBJECT (h265parse,
            "colorimetry has been changed from %s to %s",
            GST_STR_NULL (old_colorimetry), colorimetry);
        h265parse->parsed_colorimetry = ci;
        modified = TRUE;
      }

      g_free (old_colorimetry);
    }

    if (G_UNLIKELY (modified || h265parse->update_caps)) {
      gint fps_num = h265parse->fps_num;
      gint fps_den = h265parse->fps_den;
      GstClockTime latency = 0;

      caps = gst_caps_copy (sink_caps);

      /* sps should give this but upstream overrides */
      if (s && gst_structure_has_field (s, "width"))
        gst_structure_get_int (s, "width", &width);
      else
        width = h265parse->width;

      if (s && gst_structure_has_field (s, "height"))
        gst_structure_get_int (s, "height", &height);
      else
        height = h265parse->height;

      gst_caps_set_simple (caps, "width", G_TYPE_INT, width,
          "height", G_TYPE_INT, height, NULL);

      h265parse->framerate_from_caps = FALSE;
      /* upstream overrides */
      if (s && gst_structure_has_field (s, "framerate"))
        gst_structure_get_fraction (s, "framerate", &fps_num, &fps_den);

      /* but not necessarily or reliably this */
      if (fps_den > 0) {
        GstStructure *s2;
        GstClockTime val;

        GST_INFO_OBJECT (h265parse, "setting framerate in caps");
        gst_caps_set_simple (caps, "framerate",
            GST_TYPE_FRACTION, fps_num, fps_den, NULL);
        s2 = gst_caps_get_structure (caps, 0);
        gst_structure_get_fraction (s2, "framerate", &h265parse->parsed_fps_n,
            &h265parse->parsed_fps_d);
        gst_base_parse_set_frame_rate (GST_BASE_PARSE (h265parse),
            fps_num, fps_den, 0, 0);
        val = gst_h265_parse_is_field_interlaced (h265parse) ? GST_SECOND / 2 :
            GST_SECOND;
        h265parse->framerate_from_caps = TRUE;

        /* If we know the frame duration, and if we are not in one of the zero
         * latency pattern, add one frame of latency */
        if (fps_num > 0 &&
            h265parse->in_align != GST_H265_PARSE_ALIGN_AU &&
            !(h265parse->in_align == GST_H265_PARSE_ALIGN_NAL &&
                h265parse->align == GST_H265_PARSE_ALIGN_NAL))
          latency = gst_util_uint64_scale (val, fps_den, fps_num);

        gst_base_parse_set_latency (GST_BASE_PARSE (h265parse), latency,
            latency);
      }

      bit_depth_chroma = sps->bit_depth_chroma_minus8 + 8;

      switch (sps->chroma_format_idc) {
        case 0:
          chroma_format = "4:0:0";
          bit_depth_chroma = 0;
          break;
        case 1:
          chroma_format = "4:2:0";
          break;
        case 2:
          chroma_format = "4:2:2";
          break;
        case 3:
          chroma_format = "4:4:4";
          break;
        default:
          break;
      }

      if (chroma_format)
        gst_caps_set_simple (caps, "chroma-format", G_TYPE_STRING,
            chroma_format, "bit-depth-luma", G_TYPE_UINT,
            sps->bit_depth_luma_minus8 + 8, "bit-depth-chroma", G_TYPE_UINT,
            bit_depth_chroma, NULL);

      if (colorimetry && (!s || !gst_structure_has_field (s, "colorimetry"))) {
        gst_caps_set_simple (caps, "colorimetry", G_TYPE_STRING, colorimetry,
            NULL);
      }
    }

    g_free (colorimetry);
  }

  if (caps) {
    gint par_n, par_d;
    const gchar *mdi_str = NULL;
    const gchar *cll_str = NULL;
    gboolean codec_data_modified = FALSE;
    GstStructure *st;

    gst_caps_set_simple (caps, "parsed", G_TYPE_BOOLEAN, TRUE,
        "stream-format", G_TYPE_STRING,
        gst_h265_parse_get_string (h265parse, TRUE, h265parse->format),
        "alignment", G_TYPE_STRING,
        gst_h265_parse_get_string (h265parse, FALSE, h265parse->align), NULL);

    gst_h265_parse_get_par (h265parse, &par_n, &par_d);

    width = 0;
    height = 0;
    st = gst_caps_get_structure (caps, 0);
    gst_structure_get_int (st, "width", &width);
    gst_structure_get_int (st, "height", &height);

    /* If no resolution info, do not consider aspect ratio */
    if (par_n != 0 && par_d != 0 && width > 0 && height > 0 &&
        (!s || !gst_structure_has_field (s, "pixel-aspect-ratio"))) {
      gint new_par_d = par_d;
      /* Special case for some encoders which provide an 1:2 pixel aspect ratio
       * for HEVC interlaced content, possibly to work around decoders that don't
       * support field-based interlacing. Add some defensive checks to check for
       * a "common" aspect ratio. */
      if (par_n == 1 && par_d == 2
          && gst_h265_parse_is_field_interlaced (h265parse)
          && !gst_video_is_common_aspect_ratio (width, height, par_n, par_d)
          && gst_video_is_common_aspect_ratio (width, height, 1, 1)) {
        GST_WARNING_OBJECT (h265parse, "PAR 1/2 makes the aspect ratio of "
            "a %d x %d frame uncommon. Switching to 1/1", width, height);
        new_par_d = 1;
      }
      GST_INFO_OBJECT (h265parse, "PAR %d/%d", par_n, new_par_d);
      gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
          par_n, new_par_d, NULL);
    }

    /* set profile and level in caps */
    if (sps) {
      const gchar *profile, *tier, *level;
      GstH265Profile p;

      p = gst_h265_get_profile_from_sps (sps);
      /* gst_h265_get_profile_from_sps() method will determine profile
       * as defined in spec, with allowing slightly broken profile-tier-level
       * bits, then it might not be able to cover all cases.
       * If it's still unknown, do guess again */
      if (p == GST_H265_PROFILE_INVALID) {
        GST_WARNING_OBJECT (h265parse, "Unknown profile, guessing");
        switch (sps->chroma_format_idc) {
          case 0:
            if (sps->bit_depth_luma_minus8 == 0) {
              p = GST_H265_PROFILE_MONOCHROME;
            } else if (sps->bit_depth_luma_minus8 <= 2) {
              p = GST_H265_PROFILE_MONOCHROME_10;
            } else if (sps->bit_depth_luma_minus8 <= 4) {
              p = GST_H265_PROFILE_MONOCHROME_12;
            } else {
              p = GST_H265_PROFILE_MONOCHROME_16;
            }
            break;
          case 1:
            if (sps->bit_depth_luma_minus8 == 0) {
              p = GST_H265_PROFILE_MAIN;
            } else if (sps->bit_depth_luma_minus8 <= 2) {
              p = GST_H265_PROFILE_MAIN_10;
            } else if (sps->bit_depth_luma_minus8 <= 4) {
              p = GST_H265_PROFILE_MAIN_12;
            } else {
              p = GST_H265_PROFILE_MAIN_444_16_INTRA;
            }
            break;
          case 2:
            if (sps->bit_depth_luma_minus8 <= 2) {
              p = GST_H265_PROFILE_MAIN_422_10;
            } else if (sps->bit_depth_luma_minus8 <= 4) {
              p = GST_H265_PROFILE_MAIN_422_12;
            } else {
              p = GST_H265_PROFILE_MAIN_444_16_INTRA;
            }
            break;
          case 3:
            if (sps->bit_depth_luma_minus8 == 0) {
              p = GST_H265_PROFILE_MAIN_444;
            } else if (sps->bit_depth_luma_minus8 <= 2) {
              p = GST_H265_PROFILE_MAIN_444_10;
            } else if (sps->bit_depth_luma_minus8 <= 4) {
              p = GST_H265_PROFILE_MAIN_444_12;
            } else {
              p = GST_H265_PROFILE_MAIN_444_16_INTRA;
            }
            break;
          default:
            break;
        }
      }

      profile = gst_h265_profile_to_string (p);

      if (s && gst_structure_has_field (s, "profile")) {
        const gchar *profile_sink = gst_structure_get_string (s, "profile");
        GstH265Profile p_sink = gst_h265_profile_from_string (profile_sink);

        if (p != p_sink) {
          const gchar *profile_src;

          p = MAX (p, p_sink);
          profile_src = (p == p_sink) ? profile_sink : profile;
          GST_INFO_OBJECT (h265parse,
              "Upstream profile (%s) is different than in SPS (%s). "
              "Using %s.", profile_sink, profile, profile_src);
          profile = profile_src;
        }
      }

      if (profile != NULL)
        gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile, NULL);

      tier = get_tier_string (sps->profile_tier_level.tier_flag);
      if (tier != NULL)
        gst_caps_set_simple (caps, "tier", G_TYPE_STRING, tier, NULL);

      level = get_level_string (sps->profile_tier_level.level_idc);
      if (level != NULL)
        gst_caps_set_simple (caps, "level", G_TYPE_STRING, level, NULL);

      /* relax the profile constraint to find a suitable decoder */
      ensure_caps_profile (h265parse, caps, sps, p);
    }

    if (s)
      mdi_str = gst_structure_get_string (s, "mastering-display-info");
    if (mdi_str) {
      gst_caps_set_simple (caps, "mastering-display-info", G_TYPE_STRING,
          mdi_str, NULL);
    } else if (h265parse->mastering_display_info_state !=
        GST_H265_PARSE_SEI_EXPIRED &&
        !gst_video_mastering_display_info_add_to_caps
        (&h265parse->mastering_display_info, caps)) {
      GST_WARNING_OBJECT (h265parse,
          "Couldn't set mastering display info to caps");
    }

    if (s)
      cll_str = gst_structure_get_string (s, "content-light-level");
    if (cll_str) {
      gst_caps_set_simple (caps, "content-light-level", G_TYPE_STRING, cll_str,
          NULL);
    } else if (h265parse->content_light_level_state !=
        GST_H265_PARSE_SEI_EXPIRED &&
        !gst_video_content_light_level_add_to_caps
        (&h265parse->content_light_level, caps)) {
      GST_WARNING_OBJECT (h265parse,
          "Couldn't set content light level to caps");
    }

    src_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SRC_PAD (h265parse));

    if (src_caps) {
      GstStructure *src_caps_str = gst_caps_get_structure (src_caps, 0);

      /* use codec data from old caps for comparison if we have pushed frame for now.
       * we don't want to resend caps if everything is same except codec data.
       * However, if the updated sps/pps is not in bitstream, we should put
       * it on bitstream */
      if (gst_structure_has_field (src_caps_str, "codec_data")) {
        const GValue *codec_data_value =
            gst_structure_get_value (src_caps_str, "codec_data");

        if (!GST_VALUE_HOLDS_BUFFER (codec_data_value)) {
          GST_WARNING_OBJECT (h265parse, "codec_data does not hold buffer");
        } else if (!h265parse->first_frame) {
          /* If there is no pushed frame before, we can update caps without worry.
           * But updating codec_data in the middle of frames
           * (especially on non-keyframe) might make downstream be confused.
           * Therefore we are setting old codec data
           * (i.e., was pushed to downstream previously) to new caps candidate
           * here for gst_caps_is_strictly_equal() to be returned TRUE if only
           * the codec_data is different, and to avoid re-sending caps it
           * that case.
           */
          gst_caps_set_value (caps, "codec_data", codec_data_value);

          /* check for codec_data update to re-send sps/pps inband data if
           * current frame has no sps/pps but upstream codec_data was updated.
           * Note that have_vps_in_frame is skipped here since it's optional  */
          if ((!h265parse->have_sps_in_frame || !h265parse->have_pps_in_frame)
              && buf) {
            GstBuffer *codec_data_buf = gst_value_get_buffer (codec_data_value);
            GstMapInfo map;

            gst_buffer_map (buf, &map, GST_MAP_READ);
            if (map.size != gst_buffer_get_size (codec_data_buf) ||
                gst_buffer_memcmp (codec_data_buf, 0, map.data, map.size)) {
              codec_data_modified = TRUE;
            }

            gst_buffer_unmap (buf, &map);
          }
        }
      } else if (!buf) {
        GstStructure *s;
        /* remove any left-over codec-data hanging around */
        s = gst_caps_get_structure (caps, 0);
        gst_structure_remove_field (s, "codec_data");
      }
    }

    if (!(src_caps && gst_caps_is_strictly_equal (src_caps, caps))) {
      /* update codec data to new value */
      if (buf) {
        gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, buf, NULL);
        gst_buffer_replace (&h265parse->codec_data, buf);
        gst_buffer_unref (buf);
        buf = NULL;
      } else {
        GstStructure *s;
        /* remove any left-over codec-data hanging around */
        s = gst_caps_get_structure (caps, 0);
        gst_structure_remove_field (s, "codec_data");
        gst_buffer_replace (&h265parse->codec_data, NULL);
      }

      gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (h265parse), caps);
    } else if (codec_data_modified) {
      GST_DEBUG_OBJECT (h265parse,
          "Only codec_data is different, need inband vps/sps/pps update");

      /* this will insert updated codec_data with next idr */
      h265parse->push_codec = TRUE;
    }

    if (src_caps)
      gst_caps_unref (src_caps);
    gst_caps_unref (caps);
  }

  gst_caps_unref (sink_caps);
  if (buf)
    gst_buffer_unref (buf);

}

static GstFlowReturn
gst_h265_parse_parse_frame (GstBaseParse * parse, GstBaseParseFrame * frame)
{
  GstH265Parse *h265parse;
  GstBuffer *buffer;
  guint av;

  h265parse = GST_H265_PARSE (parse);
  buffer = frame->buffer;

  gst_h265_parse_update_src_caps (h265parse, NULL);

  if (h265parse->fps_num > 0 && h265parse->fps_den > 0) {
    GstClockTime val =
        gst_h265_parse_is_field_interlaced (h265parse) ? GST_SECOND /
        2 : GST_SECOND;

    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (val,
        h265parse->fps_den, h265parse->fps_num);
  }

  if (h265parse->keyframe)
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  else
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  if (h265parse->discard_bidirectional && h265parse->bidirectional)
    goto discard;


  if (h265parse->header)
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_HEADER);
  else
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_HEADER);

  if (h265parse->discont) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    h265parse->discont = FALSE;
  }

  if (h265parse->marker) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_MARKER);
    h265parse->marker = FALSE;
  } else {
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_MARKER);
  }

  /* replace with transformed HEVC output if applicable */
  av = gst_adapter_available (h265parse->frame_out);
  if (av) {
    GstBuffer *buf;

    buf = gst_adapter_take_buffer (h265parse->frame_out, av);
    gst_buffer_copy_into (buf, buffer, GST_BUFFER_COPY_METADATA, 0, -1);
    gst_buffer_replace (&frame->out_buffer, buf);
    gst_buffer_unref (buf);
  }

done:
  return GST_FLOW_OK;

discard:
  GST_DEBUG_OBJECT (h265parse, "Discarding bidirectional frame");
  frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
  gst_h265_parse_reset_frame (h265parse);
  goto done;

}

/* sends a codec NAL downstream, decorating and transforming as needed.
 * No ownership is taken of @nal */
static GstFlowReturn
gst_h265_parse_push_codec_buffer (GstH265Parse * h265parse, GstBuffer * nal,
    GstBuffer * buffer)
{
  GstMapInfo map;

  gst_buffer_map (nal, &map, GST_MAP_READ);
  nal = gst_h265_parse_wrap_nal (h265parse, h265parse->format,
      map.data, map.size);
  gst_buffer_unmap (nal, &map);

  if (h265parse->discont) {
    GST_BUFFER_FLAG_SET (nal, GST_BUFFER_FLAG_DISCONT);
    h265parse->discont = FALSE;
  }

  GST_BUFFER_PTS (nal) = GST_BUFFER_PTS (buffer);
  GST_BUFFER_DTS (nal) = GST_BUFFER_DTS (buffer);
  GST_BUFFER_DURATION (nal) = 0;

  return gst_pad_push (GST_BASE_PARSE_SRC_PAD (h265parse), nal);
}

static GstEvent *
check_pending_key_unit_event (GstEvent * pending_event, GstSegment * segment,
    GstClockTime timestamp, guint flags, GstClockTime pending_key_unit_ts)
{
  GstClockTime running_time, stream_time;
  gboolean all_headers;
  guint count;
  GstEvent *event = NULL;

  g_return_val_if_fail (segment != NULL, NULL);

  if (pending_event == NULL)
    goto out;

  if (GST_CLOCK_TIME_IS_VALID (pending_key_unit_ts) &&
      timestamp == GST_CLOCK_TIME_NONE)
    goto out;

  running_time = gst_segment_to_running_time (segment,
      GST_FORMAT_TIME, timestamp);

  GST_INFO ("now %" GST_TIME_FORMAT " wanted %" GST_TIME_FORMAT,
      GST_TIME_ARGS (running_time), GST_TIME_ARGS (pending_key_unit_ts));
  if (GST_CLOCK_TIME_IS_VALID (pending_key_unit_ts) &&
      running_time < pending_key_unit_ts)
    goto out;

  if (flags & GST_BUFFER_FLAG_DELTA_UNIT) {
    GST_DEBUG ("pending force key unit, waiting for keyframe");
    goto out;
  }

  stream_time = gst_segment_to_stream_time (segment,
      GST_FORMAT_TIME, timestamp);

  if (!gst_video_event_parse_upstream_force_key_unit (pending_event,
          NULL, &all_headers, &count)) {
    gst_video_event_parse_downstream_force_key_unit (pending_event, NULL,
        NULL, NULL, &all_headers, &count);
  }

  event =
      gst_video_event_new_downstream_force_key_unit (timestamp, stream_time,
      running_time, all_headers, count);
  gst_event_set_seqnum (event, gst_event_get_seqnum (pending_event));

out:
  return event;
}

static void
gst_h265_parse_prepare_key_unit (GstH265Parse * parse, GstEvent * event)
{
  GstClockTime running_time;
  guint count;
#ifndef GST_DISABLE_GST_DEBUG
  gboolean have_vps, have_sps, have_pps;
  gint i;
#endif

  parse->pending_key_unit_ts = GST_CLOCK_TIME_NONE;
  gst_event_replace (&parse->force_key_unit_event, NULL);

  gst_video_event_parse_downstream_force_key_unit (event,
      NULL, NULL, &running_time, NULL, &count);

  GST_INFO_OBJECT (parse, "pushing downstream force-key-unit event %d "
      "%" GST_TIME_FORMAT " count %d", gst_event_get_seqnum (event),
      GST_TIME_ARGS (running_time), count);
  gst_pad_push_event (GST_BASE_PARSE_SRC_PAD (parse), event);

#ifndef GST_DISABLE_GST_DEBUG
  have_vps = have_sps = have_pps = FALSE;
  for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
    if (parse->vps_nals[i] != NULL) {
      have_vps = TRUE;
      break;
    }
  }
  for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
    if (parse->sps_nals[i] != NULL) {
      have_sps = TRUE;
      break;
    }
  }
  for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
    if (parse->pps_nals[i] != NULL) {
      have_pps = TRUE;
      break;
    }
  }

  GST_INFO_OBJECT (parse,
      "preparing key unit, have vps %d have sps %d have pps %d", have_vps,
      have_sps, have_pps);
#endif

  /* set push_codec to TRUE so that pre_push_frame sends VPS/SPS/PPS again */
  parse->push_codec = TRUE;
}

static gboolean
gst_h265_parse_handle_vps_sps_pps_nals (GstH265Parse * h265parse,
    GstBuffer * buffer, GstBaseParseFrame * frame)
{
  GstBuffer *codec_nal;
  gint i;
  gboolean send_done = FALSE;

  if (h265parse->have_vps_in_frame && h265parse->have_sps_in_frame
      && h265parse->have_pps_in_frame) {
    GST_DEBUG_OBJECT (h265parse, "VPS/SPS/PPS exist in frame, will not insert");
    return TRUE;
  }

  if (h265parse->align == GST_H265_PARSE_ALIGN_NAL) {
    /* send separate config NAL buffers */
    GST_DEBUG_OBJECT (h265parse, "- sending VPS/SPS/PPS");
    for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
      if ((codec_nal = h265parse->vps_nals[i])) {
        GST_DEBUG_OBJECT (h265parse, "sending VPS nal");
        gst_h265_parse_push_codec_buffer (h265parse, codec_nal, buffer);
        send_done = TRUE;
      }
    }
    for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
      if ((codec_nal = h265parse->sps_nals[i])) {
        GST_DEBUG_OBJECT (h265parse, "sending SPS nal");
        gst_h265_parse_push_codec_buffer (h265parse, codec_nal, buffer);
        send_done = TRUE;
      }
    }
    for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
      if ((codec_nal = h265parse->pps_nals[i])) {
        GST_DEBUG_OBJECT (h265parse, "sending PPS nal");
        gst_h265_parse_push_codec_buffer (h265parse, codec_nal, buffer);
        send_done = TRUE;
      }
    }
  } else {
    /* insert config NALs into AU */
    GstByteWriter bw;
    GstBuffer *new_buf;
    const gboolean bs = h265parse->format == GST_H265_PARSE_FORMAT_BYTE;
    const gint nls = 4 - h265parse->nal_length_size;
    gboolean ok;

    gst_byte_writer_init_with_size (&bw, gst_buffer_get_size (buffer), FALSE);
    ok = gst_byte_writer_put_buffer (&bw, buffer, 0, h265parse->idr_pos);
    GST_DEBUG_OBJECT (h265parse, "- inserting VPS/SPS/PPS");
    for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
      if ((codec_nal = h265parse->vps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (codec_nal);
        GST_DEBUG_OBJECT (h265parse, "inserting VPS nal");
        if (bs) {
          ok &= gst_byte_writer_put_uint32_be (&bw, 1);
        } else {
          ok &= gst_byte_writer_put_uint32_be (&bw, (nal_size << (nls * 8)));
          ok &= gst_byte_writer_set_pos (&bw,
              gst_byte_writer_get_pos (&bw) - nls);
        }

        ok &= gst_byte_writer_put_buffer (&bw, codec_nal, 0, nal_size);
        send_done = TRUE;
      }
    }
    for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
      if ((codec_nal = h265parse->sps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (codec_nal);
        GST_DEBUG_OBJECT (h265parse, "inserting SPS nal");
        if (bs) {
          ok &= gst_byte_writer_put_uint32_be (&bw, 1);
        } else {
          ok &= gst_byte_writer_put_uint32_be (&bw, (nal_size << (nls * 8)));
          ok &= gst_byte_writer_set_pos (&bw,
              gst_byte_writer_get_pos (&bw) - nls);
        }

        ok &= gst_byte_writer_put_buffer (&bw, codec_nal, 0, nal_size);
        send_done = TRUE;
      }
    }
    for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
      if ((codec_nal = h265parse->pps_nals[i])) {
        gsize nal_size = gst_buffer_get_size (codec_nal);
        GST_DEBUG_OBJECT (h265parse, "inserting PPS nal");
        if (bs) {
          ok &= gst_byte_writer_put_uint32_be (&bw, 1);
        } else {
          ok &= gst_byte_writer_put_uint32_be (&bw, (nal_size << (nls * 8)));
          ok &= gst_byte_writer_set_pos (&bw,
              gst_byte_writer_get_pos (&bw) - nls);
        }
        ok &= gst_byte_writer_put_buffer (&bw, codec_nal, 0, nal_size);
        send_done = TRUE;
      }
    }
    ok &= gst_byte_writer_put_buffer (&bw, buffer, h265parse->idr_pos, -1);
    /* collect result and push */
    new_buf = gst_byte_writer_reset_and_get_buffer (&bw);
    gst_buffer_copy_into (new_buf, buffer, GST_BUFFER_COPY_METADATA, 0, -1);
    /* should already be keyframe/IDR, but it may not have been,
     * so mark it as such to avoid being discarded by picky decoder */
    GST_BUFFER_FLAG_UNSET (new_buf, GST_BUFFER_FLAG_DELTA_UNIT);
    gst_buffer_replace (&frame->out_buffer, new_buf);
    gst_buffer_unref (new_buf);
    /* some result checking seems to make some compilers happy */
    if (G_UNLIKELY (!ok)) {
      GST_ERROR_OBJECT (h265parse, "failed to insert SPS/PPS");
    }
  }

  return send_done;
}

static GstFlowReturn
gst_h265_parse_pre_push_frame (GstBaseParse * parse, GstBaseParseFrame * frame)
{
  GstH265Parse *h265parse;
  GstBuffer *buffer;
  GstEvent *event;
  GstBuffer *parse_buffer = NULL;
  GstH265SPS *sps;

  h265parse = GST_H265_PARSE (parse);

  if (h265parse->first_frame) {
    GstTagList *taglist;
    GstCaps *caps;

    /* codec tag */
    caps = gst_pad_get_current_caps (GST_BASE_PARSE_SRC_PAD (parse));
    if (G_UNLIKELY (caps == NULL)) {
      if (GST_PAD_IS_FLUSHING (GST_BASE_PARSE_SRC_PAD (parse))) {
        GST_INFO_OBJECT (parse, "Src pad is flushing");
        return GST_FLOW_FLUSHING;
      } else {
        GST_INFO_OBJECT (parse, "Src pad is not negotiated!");
        return GST_FLOW_NOT_NEGOTIATED;
      }
    }

    taglist = gst_tag_list_new_empty ();
    gst_pb_utils_add_codec_description_to_tag_list (taglist,
        GST_TAG_VIDEO_CODEC, caps);
    gst_caps_unref (caps);

    gst_base_parse_merge_tags (parse, taglist, GST_TAG_MERGE_REPLACE);
    gst_tag_list_unref (taglist);

    /* also signals the end of first-frame processing */
    h265parse->first_frame = FALSE;
  }

  buffer = frame->buffer;

  if ((event = check_pending_key_unit_event (h265parse->force_key_unit_event,
              &parse->segment, GST_BUFFER_TIMESTAMP (buffer),
              GST_BUFFER_FLAGS (buffer), h265parse->pending_key_unit_ts))) {
    gst_h265_parse_prepare_key_unit (h265parse, event);
  }

  /* periodic VPS/SPS/PPS sending */
  if (h265parse->interval > 0 || h265parse->push_codec) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);
    guint64 diff;
    gboolean initial_frame = FALSE;

    /* init */
    if (!GST_CLOCK_TIME_IS_VALID (h265parse->last_report)) {
      h265parse->last_report = timestamp;
      initial_frame = TRUE;
    }

    if (h265parse->idr_pos >= 0) {
      GST_LOG_OBJECT (h265parse, "IDR nal at offset %d", h265parse->idr_pos);

      if (timestamp > h265parse->last_report)
        diff = timestamp - h265parse->last_report;
      else
        diff = 0;

      GST_LOG_OBJECT (h265parse,
          "now %" GST_TIME_FORMAT ", last VPS/SPS/PPS %" GST_TIME_FORMAT,
          GST_TIME_ARGS (timestamp), GST_TIME_ARGS (h265parse->last_report));

      GST_DEBUG_OBJECT (h265parse,
          "interval since last VPS/SPS/PPS %" GST_TIME_FORMAT,
          GST_TIME_ARGS (diff));

      if (GST_TIME_AS_SECONDS (diff) >= h265parse->interval ||
          initial_frame || h265parse->push_codec) {
        GstClockTime new_ts;

        /* avoid overwriting a perfectly fine timestamp */
        new_ts = GST_CLOCK_TIME_IS_VALID (timestamp) ? timestamp :
            h265parse->last_report;

        if (gst_h265_parse_handle_vps_sps_pps_nals (h265parse, buffer, frame)) {
          h265parse->last_report = new_ts;
        }
      }

      /* we pushed whatever we had */
      h265parse->push_codec = FALSE;
      h265parse->have_vps = FALSE;
      h265parse->have_sps = FALSE;
      h265parse->have_pps = FALSE;
      h265parse->state &= GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS;
    }
  } else if (h265parse->interval == -1) {
    if (h265parse->idr_pos >= 0) {
      GST_LOG_OBJECT (h265parse, "IDR nal at offset %d", h265parse->idr_pos);

      gst_h265_parse_handle_vps_sps_pps_nals (h265parse, buffer, frame);

      /* we pushed whatever we had */
      h265parse->push_codec = FALSE;
      h265parse->have_vps = FALSE;
      h265parse->have_sps = FALSE;
      h265parse->have_pps = FALSE;
      h265parse->state &= GST_H265_PARSE_STATE_VALID_PICTURE_HEADERS;
    }
  }

  if (frame->out_buffer) {
    parse_buffer = frame->out_buffer =
        gst_buffer_make_writable (frame->out_buffer);
  } else {
    parse_buffer = frame->buffer = gst_buffer_make_writable (frame->buffer);
  }

  /* see section D.3.3 of the spec */
  switch (h265parse->sei_pic_struct) {
    case GST_H265_SEI_PIC_STRUCT_TOP_BOTTOM:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_TOP:
    case GST_H265_SEI_PIC_STRUCT_TOP_BOTTOM_TOP:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_TOP_BOTTOM:
      GST_BUFFER_FLAG_SET (parse_buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
      break;
    case GST_H265_SEI_PIC_STRUCT_TOP_FIELD:
    case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_NEXT_BOTTOM:
    case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_PREVIOUS_BOTTOM:
      GST_BUFFER_FLAG_SET (parse_buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
      GST_BUFFER_FLAG_SET (parse_buffer, GST_VIDEO_BUFFER_FLAG_TOP_FIELD);
      break;
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_FIELD:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_PREVIOUS_TOP:
    case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_NEXT_TOP:
      GST_BUFFER_FLAG_SET (parse_buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
      GST_BUFFER_FLAG_SET (parse_buffer, GST_VIDEO_BUFFER_FLAG_BOTTOM_FIELD);
      break;
    default:
      break;
  }

  sps = h265parse->nalparser->last_sps;
  if (sps && sps->vui_parameters_present_flag &&
      sps->vui_params.timing_info_present_flag &&
      sps->vui_params.time_scale > 0 &&
      sps->vui_params.num_units_in_tick > 0 &&
      !gst_buffer_get_video_time_code_meta (parse_buffer)) {
    guint i = 0;
    GstH265VUIParams *vui = &sps->vui_params;

    for (i = 0; i < h265parse->time_code.num_clock_ts; i++) {
      gint field_count = -1;
      guint64 n_frames_tmp;
      guint n_frames = G_MAXUINT32;
      GstVideoTimeCodeFlags flags = 0;
      guint64 scale_n, scale_d;

      if (!h265parse->time_code.clock_timestamp_flag[i])
        break;

      h265parse->time_code.clock_timestamp_flag[i] = 0;

      /* Table D.2 */
      switch (h265parse->sei_pic_struct) {
        case GST_H265_SEI_PIC_STRUCT_FRAME:
        case GST_H265_SEI_PIC_STRUCT_TOP_FIELD:
        case GST_H265_SEI_PIC_STRUCT_BOTTOM_FIELD:
          field_count = h265parse->sei_pic_struct;
          break;
        case GST_H265_SEI_PIC_STRUCT_TOP_BOTTOM:
        case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_PREVIOUS_BOTTOM:
        case GST_H265_SEI_PIC_STRUCT_TOP_PAIRED_NEXT_BOTTOM:
          field_count = i + 1;
          break;
        case GST_H265_SEI_PIC_STRUCT_BOTTOM_TOP:
        case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_PREVIOUS_TOP:
        case GST_H265_SEI_PIC_STRUCT_BOTTOM_PAIRED_NEXT_TOP:
          field_count = 2 - i;
          break;
        case GST_H265_SEI_PIC_STRUCT_TOP_BOTTOM_TOP:
          field_count = i % 2 ? 2 : 1;
          break;
        case GST_H265_SEI_PIC_STRUCT_BOTTOM_TOP_BOTTOM:
          field_count = i % 2 ? 1 : 2;
          break;
        case GST_H265_SEI_PIC_STRUCT_FRAME_DOUBLING:
        case GST_H265_SEI_PIC_STRUCT_FRAME_TRIPLING:
          field_count = 0;
          break;
      }

      if (field_count == -1) {
        GST_WARNING_OBJECT (parse,
            "failed to determine field count for timecode");
        field_count = 0;
      }

      /* Dropping of the two lowest (value 0 and 1) n_frames[ i ] counts when
       * seconds_value[ i ] is equal to 0 and minutes_value[ i ] is not an integer
       * multiple of 10 */
      if (h265parse->time_code.counting_type[i] == 4)
        flags |= GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;

      if (h265parse->sei_pic_struct != GST_H265_SEI_PIC_STRUCT_FRAME)
        flags |= GST_VIDEO_TIME_CODE_FLAGS_INTERLACED;

      /* Equation D-26 (without and tOffset)
       *
       * clockTimestamp[i] = ( ( hH * 60 + mM ) * 60 + sS ) * vui_time_scale +
       *                  nFrames * ( vui_num_units_in_tick * ( 1 + unit_field_based_flag[i] ) )
       * => timestamp = clockTimestamp / time_scale
       *
       * <taking only frame part>
       * timestamp = nFrames * ( vui_num_units_in_tick * ( 1 + unit_field_based_flag ) ) / vui_time_scale
       *
       * <timecode's timestamp of frame part>
       * timecode_timestamp = n_frames * fps_d / fps_n
       *
       * <Scaling Equation>
       * n_frames = nFrames * ( vui_num_units_in_tick * ( 1 + unit_field_based_flag ) ) / vui_time_scale
       *            * fps_n / fps_d
       *
       *                       fps_n * ( vui_num_units_in_tick * ( 1 + unit_field_based_flag ) )
       *          = nFrames * ------------------------------------------------------------------
       *                       fps_d * vui_time_scale
       */
      scale_n = (guint64) h265parse->parsed_fps_n * vui->num_units_in_tick;
      scale_d = (guint64) h265parse->parsed_fps_d * vui->time_scale;

      n_frames_tmp =
          gst_util_uint64_scale_int (h265parse->time_code.n_frames[i], scale_n,
          scale_d);
      if (n_frames_tmp <= G_MAXUINT32) {
        if (h265parse->time_code.units_field_based_flag[i])
          n_frames_tmp *= 2;

        if (n_frames_tmp <= G_MAXUINT32)
          n_frames = (guint) n_frames_tmp;
      }

      if (n_frames != G_MAXUINT32) {
        gst_buffer_add_video_time_code_meta_full (parse_buffer,
            h265parse->parsed_fps_n,
            h265parse->parsed_fps_d,
            NULL,
            flags,
            h265parse->time_code.hours_flag[i] ? h265parse->time_code.
            hours_value[i] : 0,
            h265parse->time_code.minutes_flag[i] ? h265parse->time_code.
            minutes_value[i] : 0,
            h265parse->time_code.seconds_flag[i] ? h265parse->time_code.
            seconds_value[i] : 0, n_frames, field_count);
      }
    }
  }

  gst_video_push_user_data ((GstElement *) h265parse, &h265parse->user_data,
      parse_buffer);

  gst_video_push_user_data_unregistered ((GstElement *) h265parse,
      &h265parse->user_data_unregistered, parse_buffer);

  gst_h265_parse_reset_frame (h265parse);

  return GST_FLOW_OK;
}

static gboolean
gst_h265_parse_set_caps (GstBaseParse * parse, GstCaps * caps)
{
  GstH265Parse *h265parse;
  GstStructure *str;
  const GValue *value;
  GstBuffer *codec_data = NULL;
  guint format, align;
  GstH265ParserResult parseres;
  GstCaps *old_caps;
  GstH265DecoderConfigRecord *config = NULL;

  h265parse = GST_H265_PARSE (parse);

  /* reset */
  h265parse->push_codec = FALSE;

  old_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (parse));
  if (old_caps) {
    if (!gst_caps_is_equal (old_caps, caps))
      gst_h265_parse_reset_stream_info (h265parse);
    gst_caps_unref (old_caps);
  }

  str = gst_caps_get_structure (caps, 0);

  /* accept upstream info if provided */
  gst_structure_get_int (str, "width", &h265parse->width);
  gst_structure_get_int (str, "height", &h265parse->height);
  gst_structure_get_fraction (str, "framerate", &h265parse->fps_num,
      &h265parse->fps_den);
  gst_structure_get_fraction (str, "pixel-aspect-ratio",
      &h265parse->upstream_par_n, &h265parse->upstream_par_d);

  /* get upstream format and align from caps */
  gst_h265_parse_format_from_caps (caps, &format, &align);

  /* packetized video has a codec_data */
  if (format != GST_H265_PARSE_FORMAT_BYTE &&
      (value = gst_structure_get_value (str, "codec_data"))) {
    GstMapInfo map;
    guint i, j;

    GST_DEBUG_OBJECT (h265parse, "have packetized h265");
    /* make note for optional split processing */
    h265parse->packetized = TRUE;

    codec_data = gst_value_get_buffer (value);
    if (!codec_data)
      goto wrong_type;
    gst_buffer_map (codec_data, &map, GST_MAP_READ);

    parseres =
        gst_h265_parser_parse_decoder_config_record (h265parse->nalparser,
        map.data, map.size, &config);
    if (parseres != GST_H265_PARSER_OK) {
      gst_buffer_unmap (codec_data, &map);
      goto hvcc_failed;
    }

    h265parse->nal_length_size = config->length_size_minus_one + 1;
    GST_DEBUG_OBJECT (h265parse, "nal length size %u",
        h265parse->nal_length_size);

    for (i = 0; i < config->nalu_array->len; i++) {
      GstH265DecoderConfigRecordNalUnitArray *array =
          &g_array_index (config->nalu_array,
          GstH265DecoderConfigRecordNalUnitArray, i);

      for (j = 0; j < array->nalu->len; j++) {
        GstH265NalUnit *nalu = &g_array_index (array->nalu, GstH265NalUnit, j);

        gst_h265_parse_process_nal (h265parse, nalu);
      }
    }

    gst_h265_decoder_config_record_free (config);
    gst_buffer_unmap (codec_data, &map);

    /* don't confuse codec_data with inband vps/sps/pps */
    h265parse->have_vps_in_frame = FALSE;
    h265parse->have_sps_in_frame = FALSE;
    h265parse->have_pps_in_frame = FALSE;
  } else {
    GST_DEBUG_OBJECT (h265parse, "have bytestream h265");
    /* nothing to pre-process */
    h265parse->packetized = FALSE;
    /* we have 4 sync bytes */
    h265parse->nal_length_size = 4;

    if (format == GST_H265_PARSE_FORMAT_NONE) {
      format = GST_H265_PARSE_FORMAT_BYTE;
      align = GST_H265_PARSE_ALIGN_AU;
    }
  }

  {
    GstCaps *in_caps;

    /* prefer input type determined above */
    in_caps = gst_caps_new_simple ("video/x-h265",
        "parsed", G_TYPE_BOOLEAN, TRUE,
        "stream-format", G_TYPE_STRING,
        gst_h265_parse_get_string (h265parse, TRUE, format),
        "alignment", G_TYPE_STRING,
        gst_h265_parse_get_string (h265parse, FALSE, align), NULL);
    /* negotiate with downstream, sets ->format and ->align */
    gst_h265_parse_negotiate (h265parse, format, in_caps);
    gst_caps_unref (in_caps);
  }

  if (format == h265parse->format && align == h265parse->align) {
    /* do not set CAPS and passthrough mode if SPS/PPS have not been parsed */
    if (h265parse->have_sps && h265parse->have_pps) {
      /* Don't enable passthrough here. This element will parse various
       * SEI messages which would be very important/useful for downstream
       * (HDR, timecode for example)
       */
#if 0
      gst_base_parse_set_passthrough (parse, TRUE);
#endif

      /* we did parse codec-data and might supplement src caps */
      gst_h265_parse_update_src_caps (h265parse, caps);
    }
  } else if (format == GST_H265_PARSE_FORMAT_HVC1
      || format == GST_H265_PARSE_FORMAT_HEV1) {
    /* if input != output, and input is hevc, must split before anything else */
    /* arrange to insert codec-data in-stream if needed.
     * src caps are only arranged for later on */
    h265parse->push_codec = TRUE;
    h265parse->have_vps = FALSE;
    h265parse->have_sps = FALSE;
    h265parse->have_pps = FALSE;
    if (h265parse->align == GST_H265_PARSE_ALIGN_NAL)
      h265parse->split_packetized = TRUE;
    h265parse->packetized = TRUE;
  }

  h265parse->in_align = align;

  return TRUE;

  /* ERRORS */
hvcc_failed:
  {
    GST_DEBUG_OBJECT (h265parse, "Failed to parse hvcC data");
    goto refuse_caps;
  }
wrong_type:
  {
    GST_DEBUG_OBJECT (h265parse, "wrong codec-data type");
    goto refuse_caps;
  }
refuse_caps:
  {
    GST_WARNING_OBJECT (h265parse, "refused caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

static void
remove_fields (GstCaps * caps, gboolean all)
{
  guint i, n;

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);

    if (all) {
      gst_structure_remove_field (s, "alignment");
      gst_structure_remove_field (s, "stream-format");
    }
    gst_structure_remove_field (s, "parsed");
  }
}

static GstCaps *
gst_h265_parse_get_caps (GstBaseParse * parse, GstCaps * filter)
{
  GstCaps *peercaps, *templ;
  GstCaps *res, *tmp, *pcopy;

  templ = gst_pad_get_pad_template_caps (GST_BASE_PARSE_SINK_PAD (parse));
  if (filter) {
    GstCaps *fcopy = gst_caps_copy (filter);
    /* Remove the fields we convert */
    remove_fields (fcopy, TRUE);
    peercaps = gst_pad_peer_query_caps (GST_BASE_PARSE_SRC_PAD (parse), fcopy);
    gst_caps_unref (fcopy);
  } else
    peercaps = gst_pad_peer_query_caps (GST_BASE_PARSE_SRC_PAD (parse), NULL);

  pcopy = gst_caps_copy (peercaps);
  remove_fields (pcopy, TRUE);

  res = gst_caps_intersect_full (pcopy, templ, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (pcopy);
  gst_caps_unref (templ);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect_full (res, filter,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = tmp;
  }

  /* Try if we can put the downstream caps first */
  pcopy = gst_caps_copy (peercaps);
  remove_fields (pcopy, FALSE);
  tmp = gst_caps_intersect_full (pcopy, res, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (pcopy);
  if (!gst_caps_is_empty (tmp))
    res = gst_caps_merge (tmp, res);
  else
    gst_caps_unref (tmp);

  gst_caps_unref (peercaps);
  return res;
}

static gboolean
gst_h265_parse_event (GstBaseParse * parse, GstEvent * event)
{
  gboolean res;
  GstH265Parse *h265parse = GST_H265_PARSE (parse);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      GstClockTime timestamp, stream_time, running_time;
      gboolean all_headers;
      guint count;

      if (gst_video_event_is_force_key_unit (event)) {
        gst_video_event_parse_downstream_force_key_unit (event,
            &timestamp, &stream_time, &running_time, &all_headers, &count);

        GST_INFO_OBJECT (h265parse, "received downstream force key unit event, "
            "seqnum %d running_time %" GST_TIME_FORMAT
            " all_headers %d count %d", gst_event_get_seqnum (event),
            GST_TIME_ARGS (running_time), all_headers, count);
        if (h265parse->force_key_unit_event) {
          GST_INFO_OBJECT (h265parse, "ignoring force key unit event "
              "as one is already queued");
        } else {
          h265parse->pending_key_unit_ts = running_time;
          gst_event_replace (&h265parse->force_key_unit_event, event);
        }
        gst_event_unref (event);
        res = TRUE;
      } else {
        res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
        break;
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_SEGMENT_DONE:
      h265parse->push_codec = TRUE;
      res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
      break;
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment = NULL;

      gst_event_parse_segment (event, &segment);

      h265parse->last_report = GST_CLOCK_TIME_NONE;

      if (segment->flags & GST_SEEK_FLAG_TRICKMODE_FORWARD_PREDICTED) {
        GST_DEBUG_OBJECT (h265parse, "Will discard bidirectional frames");
        h265parse->discard_bidirectional = TRUE;
      }

      res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
      break;
    }
    default:
      res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);
      break;
  }
  return res;
}

static gboolean
gst_h265_parse_src_event (GstBaseParse * parse, GstEvent * event)
{
  gboolean res;
  GstH265Parse *h265parse = GST_H265_PARSE (parse);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      GstClockTime running_time;
      gboolean all_headers;
      guint count;

      if (gst_video_event_is_force_key_unit (event)) {
        gst_video_event_parse_upstream_force_key_unit (event,
            &running_time, &all_headers, &count);

        GST_INFO_OBJECT (h265parse, "received upstream force-key-unit event, "
            "seqnum %d running_time %" GST_TIME_FORMAT
            " all_headers %d count %d", gst_event_get_seqnum (event),
            GST_TIME_ARGS (running_time), all_headers, count);

        if (all_headers) {
          h265parse->pending_key_unit_ts = running_time;
          gst_event_replace (&h265parse->force_key_unit_event, event);
        }
      }
      res = GST_BASE_PARSE_CLASS (parent_class)->src_event (parse, event);
      break;
    }
    default:
      res = GST_BASE_PARSE_CLASS (parent_class)->src_event (parse, event);
      break;
  }

  return res;
}

static void
gst_h265_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstH265Parse *parse;
  parse = GST_H265_PARSE (object);

  switch (prop_id) {
    case PROP_CONFIG_INTERVAL:
      parse->interval = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_h265_parse_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstH265Parse *parse;
  parse = GST_H265_PARSE (object);

  switch (prop_id) {
    case PROP_CONFIG_INTERVAL:
      g_value_set_int (value, parse->interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
