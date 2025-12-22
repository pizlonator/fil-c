/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include <gst/codecparsers/gstvp9parser.h>
#include <gst/video/video.h>
#include "gstvideoparserselements.h"
#include "gstvp9parse.h"

#include <string.h>

GST_DEBUG_CATEGORY (vp9_parse_debug);
#define GST_CAT_DEFAULT vp9_parse_debug

typedef enum
{
  GST_VP9_PARSE_ALIGN_NONE = 0,
  GST_VP9_PARSE_ALIGN_SUPER_FRAME,
  GST_VP9_PARSE_ALIGN_FRAME,
} GstVp9ParseAligment;

struct _GstVp9Parse
{
  GstBaseParse parent;

  /* parsed from the last keyframe */
  gint width;
  gint height;
  gint subsampling_x;
  gint subsampling_y;
  GstVp9ColorSpace color_space;
  GstVp9ColorRange color_range;
  GstVP9Profile profile;
  GstVp9BitDepth bit_depth;
  gboolean codec_alpha;

  GstVp9ParseAligment in_align;
  GstVp9ParseAligment align;

  GstVp9Parser *parser;
  gboolean update_caps;

  /* per frame status */
  gboolean discont;

  GstClockTime super_frame_pts;
  GstClockTime super_frame_dts;
  GstClockTime super_frame_duration;
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9, parsed = (boolean) true, "
        "alignment=(string) { super-frame, frame }"));

#define parent_class gst_vp9_parse_parent_class
G_DEFINE_TYPE (GstVp9Parse, gst_vp9_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vp9parse, "vp9parse", GST_RANK_SECONDARY,
    GST_TYPE_VP9_PARSE, videoparsers_element_init (plugin));

static gboolean gst_vp9_parse_start (GstBaseParse * parse);
static gboolean gst_vp9_parse_stop (GstBaseParse * parse);
static GstFlowReturn gst_vp9_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);
static gboolean gst_vp9_parse_set_sink_caps (GstBaseParse * parse,
    GstCaps * caps);
static GstCaps *gst_vp9_parse_get_sink_caps (GstBaseParse * parse,
    GstCaps * filter);
static void gst_vp9_parse_update_src_caps (GstVp9Parse * self, GstCaps * caps);
static GstFlowReturn gst_vp9_parse_parse_frame (GstVp9Parse * self,
    GstBaseParseFrame * frame, GstVp9FrameHdr * frame_hdr);
static GstFlowReturn gst_vp9_parse_pre_push_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);

static void
gst_vp9_parse_class_init (GstVp9ParseClass * klass)
{
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  parse_class->start = GST_DEBUG_FUNCPTR (gst_vp9_parse_start);
  parse_class->stop = GST_DEBUG_FUNCPTR (gst_vp9_parse_stop);
  parse_class->handle_frame = GST_DEBUG_FUNCPTR (gst_vp9_parse_handle_frame);
  parse_class->pre_push_frame =
      GST_DEBUG_FUNCPTR (gst_vp9_parse_pre_push_frame);
  parse_class->set_sink_caps = GST_DEBUG_FUNCPTR (gst_vp9_parse_set_sink_caps);
  parse_class->get_sink_caps = GST_DEBUG_FUNCPTR (gst_vp9_parse_get_sink_caps);

  gst_element_class_add_static_pad_template (element_class, &srctemplate);
  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_static_metadata (element_class, "VP9 parser",
      "Codec/Parser/Converter/Video",
      "Parses VP9 streams", "Seungha Yang <seungha@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (vp9_parse_debug, "vp9parse", 0, "vp9 parser");
}

static void
gst_vp9_parse_init (GstVp9Parse * self)
{
  gst_base_parse_set_pts_interpolation (GST_BASE_PARSE (self), FALSE);
  gst_base_parse_set_infer_ts (GST_BASE_PARSE (self), FALSE);

  GST_PAD_SET_ACCEPT_INTERSECT (GST_BASE_PARSE_SINK_PAD (self));
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_BASE_PARSE_SINK_PAD (self));
}

static void
gst_vp9_parse_reset_super_frame (GstVp9Parse * self)
{
  self->super_frame_pts = GST_CLOCK_TIME_NONE;
  self->super_frame_dts = GST_CLOCK_TIME_NONE;
  self->super_frame_duration = GST_CLOCK_TIME_NONE;
}

static void
gst_vp9_parse_reset (GstVp9Parse * self)
{
  self->width = 0;
  self->height = 0;
  self->subsampling_x = -1;
  self->subsampling_y = -1;
  self->color_space = GST_VP9_CS_UNKNOWN;
  self->color_range = GST_VP9_CR_LIMITED;
  self->profile = GST_VP9_PROFILE_UNDEFINED;
  self->bit_depth = (GstVp9BitDepth) 0;
  self->codec_alpha = FALSE;
  gst_vp9_parse_reset_super_frame (self);
}

static gboolean
gst_vp9_parse_start (GstBaseParse * parse)
{
  GstVp9Parse *self = GST_VP9_PARSE (parse);

  GST_DEBUG_OBJECT (self, "start");

  self->parser = gst_vp9_parser_new ();
  gst_vp9_parse_reset (self);

  /* short frame header with one byte */
  gst_base_parse_set_min_frame_size (parse, 1);

  return TRUE;
}

static gboolean
gst_vp9_parse_stop (GstBaseParse * parse)
{
  GstVp9Parse *self = GST_VP9_PARSE (parse);

  GST_DEBUG_OBJECT (self, "stop");
  g_clear_pointer (&self->parser, gst_vp9_parser_free);

  return TRUE;
}

static const gchar *
gst_vp9_parse_profile_to_string (GstVP9Profile profile)
{
  switch (profile) {
    case GST_VP9_PROFILE_0:
      return "0";
    case GST_VP9_PROFILE_1:
      return "1";
    case GST_VP9_PROFILE_2:
      return "2";
    case GST_VP9_PROFILE_3:
      return "3";
    default:
      break;
  }

  return NULL;
}

static GstVP9Profile
gst_vp9_parse_profile_from_string (const gchar * profile)
{
  if (!profile)
    return GST_VP9_PROFILE_UNDEFINED;

  if (g_strcmp0 (profile, "0") == 0)
    return GST_VP9_PROFILE_0;
  else if (g_strcmp0 (profile, "1") == 0)
    return GST_VP9_PROFILE_1;
  else if (g_strcmp0 (profile, "2") == 0)
    return GST_VP9_PROFILE_2;
  else if (g_strcmp0 (profile, "3") == 0)
    return GST_VP9_PROFILE_3;

  return GST_VP9_PROFILE_UNDEFINED;
}

static const gchar *
gst_vp9_parse_alignment_to_string (GstVp9ParseAligment align)
{
  switch (align) {
    case GST_VP9_PARSE_ALIGN_SUPER_FRAME:
      return "super-frame";
    case GST_VP9_PARSE_ALIGN_FRAME:
      return "frame";
    default:
      break;
  }

  return NULL;
}

static GstVp9ParseAligment
gst_vp9_parse_alignment_from_string (const gchar * align)
{
  if (!align)
    return GST_VP9_PARSE_ALIGN_NONE;

  if (g_strcmp0 (align, "super-frame") == 0)
    return GST_VP9_PARSE_ALIGN_SUPER_FRAME;
  else if (g_strcmp0 (align, "frame") == 0)
    return GST_VP9_PARSE_ALIGN_FRAME;

  return GST_VP9_PARSE_ALIGN_NONE;
}

static void
gst_vp9_parse_alignment_from_caps (GstCaps * caps, GstVp9ParseAligment * align)
{
  *align = GST_VP9_PARSE_ALIGN_NONE;

  GST_DEBUG ("parsing caps: %" GST_PTR_FORMAT, caps);

  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str = NULL;

    if ((str = gst_structure_get_string (s, "alignment"))) {
      *align = gst_vp9_parse_alignment_from_string (str);
    }
  }
}

/* implement custom semantic for codec-alpha */
static gboolean
gst_vp9_parse_check_codec_alpha (GstStructure * s, gboolean codec_alpha)
{
  gboolean value;

  if (gst_structure_get_boolean (s, "codec-alpha", &value))
    return value == codec_alpha;

  return codec_alpha == FALSE;
}

/* check downstream caps to configure format and alignment */
static void
gst_vp9_parse_negotiate (GstVp9Parse * self, GstVp9ParseAligment in_align,
    GstCaps * in_caps)
{
  GstCaps *caps;
  GstVp9ParseAligment align = self->align;

  caps = gst_pad_get_allowed_caps (GST_BASE_PARSE_SRC_PAD (self));
  GST_DEBUG_OBJECT (self, "allowed caps: %" GST_PTR_FORMAT, caps);

  /* concentrate on leading structure, since decodebin parser
   * capsfilter always includes parser template caps */
  if (caps) {
    caps = gst_caps_make_writable (caps);
    while (gst_caps_get_size (caps) > 0) {
      GstStructure *s = gst_caps_get_structure (caps, 0);

      if (gst_vp9_parse_check_codec_alpha (s, self->codec_alpha))
        break;

      gst_caps_remove_structure (caps, 0);
    }

    /* this may happen if there is simply no codec alpha decoder in the
     * gstreamer installation, in this case, pick the first non-alpha decoder.
     */
    if (gst_caps_is_empty (caps)) {
      gst_caps_unref (caps);
      caps = gst_pad_get_allowed_caps (GST_BASE_PARSE_SRC_PAD (self));
    }

    caps = gst_caps_truncate (caps);
    GST_DEBUG_OBJECT (self, "negotiating with caps: %" GST_PTR_FORMAT, caps);
  }

  if (in_caps && caps) {
    if (gst_caps_can_intersect (in_caps, caps)) {
      GST_DEBUG_OBJECT (self, "downstream accepts upstream caps");
      gst_vp9_parse_alignment_from_caps (in_caps, &align);
      gst_clear_caps (&caps);
    }
  }

  /* FIXME We could fail the negotiation immediately if caps are empty */
  if (caps && !gst_caps_is_empty (caps)) {
    /* fixate to avoid ambiguity with lists when parsing */
    caps = gst_caps_fixate (caps);
    gst_vp9_parse_alignment_from_caps (caps, &align);
  }

  /* default */
  if (align == GST_VP9_PARSE_ALIGN_NONE)
    align = GST_VP9_PARSE_ALIGN_SUPER_FRAME;

  GST_DEBUG_OBJECT (self, "selected alignment %s",
      gst_vp9_parse_alignment_to_string (align));

  self->align = align;

  gst_clear_caps (&caps);
}

static gboolean
gst_vp9_parse_is_info_valid (GstVp9Parse * self)
{
  if (self->width <= 0 || self->height <= 0)
    return FALSE;

  if (self->subsampling_x < 0 || self->subsampling_y < 0)
    return FALSE;

  if (self->profile == GST_VP9_PROFILE_UNDEFINED)
    return FALSE;

  if (self->bit_depth < (GstVp9BitDepth) GST_VP9_BIT_DEPTH_8)
    return FALSE;

  return TRUE;
}

static gboolean
gst_vp9_parse_process_frame (GstVp9Parse * self, GstVp9FrameHdr * frame_hdr)
{
  GstVp9Parser *parser = self->parser;
  gint width, height;

  /* the resolution might be varying. Update our status per key frame */
  if (frame_hdr->frame_type != GST_VP9_KEY_FRAME ||
      frame_hdr->show_existing_frame) {
    /* Need to continue to get some valid info. */
    if (gst_vp9_parse_is_info_valid (self))
      return TRUE;
  }

  width = frame_hdr->width;
  height = frame_hdr->height;
  if (frame_hdr->display_size_enabled &&
      frame_hdr->display_width > 0 && frame_hdr->display_height) {
    width = frame_hdr->display_width;
    height = frame_hdr->display_height;
  }

  if (width != self->width || height != self->height) {
    GST_DEBUG_OBJECT (self, "resolution change from %dx%d to %dx%d",
        self->width, self->height, width, height);
    self->width = width;
    self->height = height;
    self->update_caps = TRUE;
  }

  if (self->subsampling_x != parser->subsampling_x ||
      self->subsampling_y != parser->subsampling_y) {
    GST_DEBUG_OBJECT (self,
        "subsampling changed from x: %d, y: %d to x: %d, y: %d",
        self->subsampling_x, self->subsampling_y,
        parser->subsampling_x, parser->subsampling_y);
    self->subsampling_x = parser->subsampling_x;
    self->subsampling_y = parser->subsampling_y;
    self->update_caps = TRUE;
  }

  if (parser->color_space != GST_VP9_CS_UNKNOWN &&
      parser->color_space != GST_VP9_CS_RESERVED_2 &&
      parser->color_space != self->color_space) {
    GST_DEBUG_OBJECT (self, "colorspace changed from %d to %d",
        self->color_space, parser->color_space);
    self->color_space = parser->color_space;
    self->update_caps = TRUE;
  }

  if (parser->color_range != self->color_range) {
    GST_DEBUG_OBJECT (self, "color range changed from %d to %d",
        self->color_range, parser->color_range);
    self->color_range = parser->color_range;
    self->update_caps = TRUE;
  }

  if (frame_hdr->profile != GST_VP9_PROFILE_UNDEFINED &&
      frame_hdr->profile != self->profile) {
    GST_DEBUG_OBJECT (self, "profile changed from %d to %d", self->profile,
        frame_hdr->profile);
    self->profile = frame_hdr->profile;
    self->update_caps = TRUE;
  }

  if (parser->bit_depth != self->bit_depth) {
    GST_DEBUG_OBJECT (self, "bit-depth changed from %d to %d",
        self->bit_depth, parser->bit_depth);
    self->bit_depth = parser->bit_depth;
    self->update_caps = TRUE;
  }

  return TRUE;
}

static GstFlowReturn
gst_vp9_parse_pre_push_frame (GstBaseParse * parse, GstBaseParseFrame * frame)
{
  GstVp9Parse *self = GST_VP9_PARSE (parse);

  frame->flags |= GST_BASE_PARSE_FRAME_FLAG_CLIP;

  if (!frame->buffer)
    return GST_FLOW_OK;

  /* The super frame may contain more than one frames inside its buffer.
     When splitting a super frame into frames, the base parse class only
     assign the PTS to the first frame and leave the others' PTS invalid.
     But in fact, all decode only frames should have invalid PTS while
     showable frames should have correct PTS setting. */
  if (self->align != GST_VP9_PARSE_ALIGN_FRAME)
    return GST_FLOW_OK;

  if (GST_BUFFER_FLAG_IS_SET (frame->buffer, GST_BUFFER_FLAG_DECODE_ONLY)) {
    GST_BUFFER_PTS (frame->buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (frame->buffer) = GST_CLOCK_TIME_NONE;
  } else {
    GST_BUFFER_PTS (frame->buffer) = self->super_frame_pts;
    GST_BUFFER_DURATION (frame->buffer) = self->super_frame_duration;
  }
  GST_BUFFER_DTS (frame->buffer) = self->super_frame_dts;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vp9_parse_handle_frame (GstBaseParse * parse, GstBaseParseFrame * frame,
    gint * skipsize)
{
  GstVp9Parse *self = GST_VP9_PARSE (parse);
  GstBuffer *buffer = frame->buffer;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVp9ParserResult parse_res = GST_VP9_PARSER_ERROR;
  GstMapInfo map;
  gsize offset = 0;
  GstVp9SuperframeInfo superframe_info;
  guint i;
  GstVp9FrameHdr frame_hdr;

  if (GST_BUFFER_FLAG_IS_SET (frame->buffer, GST_BUFFER_FLAG_DISCONT))
    self->discont = TRUE;
  else
    self->discont = FALSE;

  /* need to save buffer from invalidation upon _finish_frame */
  if (self->align == GST_VP9_PARSE_ALIGN_FRAME)
    buffer = gst_buffer_copy (frame->buffer);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (parse, CORE, NOT_IMPLEMENTED, (NULL),
        ("Couldn't map incoming buffer"));

    return GST_FLOW_ERROR;
  }

  GST_TRACE_OBJECT (self, "processing buffer of size %" G_GSIZE_FORMAT,
      map.size);

  /* superframe_info will be zero initialized by GstVp9Parser */
  parse_res = gst_vp9_parser_parse_superframe_info (self->parser,
      &superframe_info, map.data, map.size);

  if (parse_res != GST_VP9_PARSER_OK) {
    /* just finish this frame anyway, so that we don't too strict
     * regarding parsing vp9 stream.
     * Downstream might be able to handle this stream even though
     * it's very unlikely */
    GST_WARNING_OBJECT (self, "Couldn't parse superframe res: %d", parse_res);
    goto done;
  }

  self->super_frame_pts = GST_BUFFER_PTS (buffer);
  self->super_frame_dts = GST_BUFFER_DTS (buffer);
  self->super_frame_duration = GST_BUFFER_DURATION (buffer);

  for (i = 0; i < superframe_info.frames_in_superframe; i++) {
    guint32 frame_size;

    frame_size = superframe_info.frame_sizes[i];
    parse_res = gst_vp9_parser_parse_frame_header (self->parser,
        &frame_hdr, map.data + offset, frame_size);

    if (parse_res != GST_VP9_PARSER_OK) {
      GST_WARNING_OBJECT (self, "Parsing error %d", parse_res);
      break;
    }

    gst_vp9_parse_process_frame (self, &frame_hdr);

    if (self->align == GST_VP9_PARSE_ALIGN_FRAME) {
      GstBaseParseFrame subframe;

      gst_base_parse_frame_init (&subframe);
      subframe.flags |= frame->flags;
      subframe.offset = frame->offset;
      subframe.overhead = frame->overhead;
      subframe.buffer = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL,
          offset, frame_size);

      /* note we don't need to come up with a sub-buffer, since
       * subsequent code only considers input buffer's metadata.
       * Real data is either taken from input by baseclass or
       * a replacement output buffer is provided anyway. */
      gst_vp9_parse_parse_frame (self, &subframe, &frame_hdr);

      ret = gst_base_parse_finish_frame (parse, &subframe, frame_size);
    } else {
      /* FIXME: need to parse all frames belong to this superframe? */
      break;
    }

    offset += frame_size;
  }

  gst_vp9_parse_reset_super_frame (self);

done:
  gst_buffer_unmap (buffer, &map);

  if (self->align != GST_VP9_PARSE_ALIGN_FRAME) {
    if (parse_res == GST_VP9_PARSER_OK)
      gst_vp9_parse_parse_frame (self, frame, &frame_hdr);
    ret = gst_base_parse_finish_frame (parse, frame, map.size);
  } else {
    gst_buffer_unref (buffer);
    if (offset != map.size) {
      gsize left = map.size - offset;
      if (left != superframe_info.superframe_index_size) {
        GST_WARNING_OBJECT (parse,
            "Skipping leftover frame data %" G_GSIZE_FORMAT, left);
      }
      frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
      ret = gst_base_parse_finish_frame (parse, frame, left);
    }
  }

  return ret;
}

static void
gst_vp9_parse_update_src_caps (GstVp9Parse * self, GstCaps * caps)
{
  GstCaps *sink_caps, *src_caps;
  GstCaps *final_caps = NULL;
  GstStructure *s = NULL;
  gint width, height;
  gint par_n = 0, par_d = 0;
  gint fps_n = 0, fps_d = 0;
  gint bitdepth = 0;
  gchar *colorimetry = NULL;
  const gchar *chroma_format = NULL;
  const gchar *profile = NULL;

  if (!self->update_caps)
    return;

  /* if this is being called from the first _setcaps call, caps on the sinkpad
   * aren't set yet and so they need to be passed as an argument */
  if (caps)
    sink_caps = gst_caps_ref (caps);
  else
    sink_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (self));

  /* carry over input caps as much as possible; override with our own stuff */
  if (!sink_caps)
    sink_caps = gst_caps_new_empty_simple ("video/x-vp9");
  else
    s = gst_caps_get_structure (sink_caps, 0);

  final_caps = gst_caps_copy (sink_caps);

  /* frame header should give this but upstream overrides */
  if (s && gst_structure_has_field (s, "width") &&
      gst_structure_has_field (s, "height")) {
    gst_structure_get_int (s, "width", &width);
    gst_structure_get_int (s, "height", &height);
  } else {
    width = self->width;
    height = self->height;
  }

  if (width > 0 && height > 0)
    gst_caps_set_simple (final_caps, "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, NULL);

  if (s && gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d)) {
    if (par_n != 0 && par_d != 0) {
      gst_caps_set_simple (final_caps, "pixel-aspect-ratio",
          GST_TYPE_FRACTION, par_n, par_d, NULL);
    }
  }

  if (s && gst_structure_has_field (s, "framerate")) {
    gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d);
  }

  if (fps_n > 0 && fps_d > 0) {
    gst_caps_set_simple (final_caps, "framerate",
        GST_TYPE_FRACTION, fps_n, fps_d, NULL);
    gst_base_parse_set_frame_rate (GST_BASE_PARSE (self), fps_n, fps_d, 0, 0);
  }

  if (self->color_space != GST_VP9_CS_UNKNOWN &&
      self->color_space != GST_VP9_CS_RESERVED_2) {
    GstVideoColorimetry cinfo;
    gboolean have_cinfo = TRUE;

    memset (&cinfo, 0, sizeof (GstVideoColorimetry));

    switch (self->parser->color_space) {
      case GST_VP9_CS_BT_601:
        gst_video_colorimetry_from_string (&cinfo, GST_VIDEO_COLORIMETRY_BT601);
        break;
      case GST_VP9_CS_BT_709:
        gst_video_colorimetry_from_string (&cinfo, GST_VIDEO_COLORIMETRY_BT709);
        break;
      case GST_VP9_CS_SMPTE_170:
        gst_video_colorimetry_from_string (&cinfo, GST_VIDEO_COLORIMETRY_BT601);
        break;
      case GST_VP9_CS_SMPTE_240:
        gst_video_colorimetry_from_string (&cinfo,
            GST_VIDEO_COLORIMETRY_SMPTE240M);
        break;
      case GST_VP9_CS_BT_2020:
        if (self->parser->bit_depth == GST_VP9_BIT_DEPTH_12) {
          gst_video_colorimetry_from_string (&cinfo,
              GST_VIDEO_COLORIMETRY_BT2020);
        } else {
          gst_video_colorimetry_from_string (&cinfo,
              GST_VIDEO_COLORIMETRY_BT2020_10);
        }
        break;
      case GST_VP9_CS_SRGB:
        gst_video_colorimetry_from_string (&cinfo, GST_VIDEO_COLORIMETRY_SRGB);
        break;
      default:
        have_cinfo = FALSE;
        break;
    }

    if (have_cinfo) {
      if (self->parser->color_range == GST_VP9_CR_LIMITED)
        cinfo.range = GST_VIDEO_COLOR_RANGE_16_235;
      else
        cinfo.range = GST_VIDEO_COLOR_RANGE_0_255;

      colorimetry = gst_video_colorimetry_to_string (&cinfo);
    }
  }

  if (self->parser->subsampling_x == 1 && self->parser->subsampling_y == 1)
    chroma_format = "4:2:0";
  else if (self->parser->subsampling_x == 1 && self->parser->subsampling_y == 0)
    chroma_format = "4:2:2";
  else if (self->parser->subsampling_x == 0 && self->parser->subsampling_y == 1)
    chroma_format = "4:4:0";
  else if (self->parser->subsampling_x == 0 && self->parser->subsampling_y == 0)
    chroma_format = "4:4:4";

  if (chroma_format)
    gst_caps_set_simple (final_caps,
        "chroma-format", G_TYPE_STRING, chroma_format, NULL);

  switch (self->bit_depth) {
    case GST_VP9_BIT_DEPTH_8:
      bitdepth = 8;
      break;
    case GST_VP9_BIT_DEPTH_10:
      bitdepth = 10;
      break;
    case GST_VP9_BIT_DEPTH_12:
      bitdepth = 12;
      break;
    default:
      break;
  }

  if (bitdepth) {
    gst_caps_set_simple (final_caps,
        "bit-depth-luma", G_TYPE_UINT, bitdepth,
        "bit-depth-chroma", G_TYPE_UINT, bitdepth, NULL);
  }

  if (colorimetry && (!s || !gst_structure_has_field (s, "colorimetry"))) {
    gst_caps_set_simple (final_caps,
        "colorimetry", G_TYPE_STRING, colorimetry, NULL);
  }

  g_free (colorimetry);

  gst_caps_set_simple (final_caps, "parsed", G_TYPE_BOOLEAN, TRUE,
      "alignment", G_TYPE_STRING,
      gst_vp9_parse_alignment_to_string (self->align), NULL);

  profile = gst_vp9_parse_profile_to_string (self->profile);
  if (profile)
    gst_caps_set_simple (final_caps, "profile", G_TYPE_STRING, profile, NULL);

  gst_caps_set_simple (final_caps, "codec-alpha", G_TYPE_BOOLEAN,
      self->codec_alpha, NULL);

  src_caps = gst_pad_get_current_caps (GST_BASE_PARSE_SRC_PAD (self));

  if (!(src_caps && gst_caps_is_strictly_equal (src_caps, final_caps))) {
    GST_DEBUG_OBJECT (self, "Update src caps %" GST_PTR_FORMAT, final_caps);
    gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (self), final_caps);
  }

  gst_clear_caps (&src_caps);
  gst_caps_unref (final_caps);
  gst_caps_unref (sink_caps);

  self->update_caps = FALSE;
}

static GstFlowReturn
gst_vp9_parse_parse_frame (GstVp9Parse * self, GstBaseParseFrame * frame,
    GstVp9FrameHdr * frame_hdr)
{
  GstBuffer *buffer;

  buffer = frame->buffer;

  gst_vp9_parse_update_src_caps (self, NULL);

  if (frame_hdr->frame_type == GST_VP9_KEY_FRAME)
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  else
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  if (self->align == GST_VP9_PARSE_ALIGN_FRAME) {
    if (!frame_hdr->show_frame && !frame_hdr->show_existing_frame)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DECODE_ONLY);
    else
      GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DECODE_ONLY);
  }

  if (self->discont) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    self->discont = FALSE;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_vp9_parse_set_sink_caps (GstBaseParse * parse, GstCaps * caps)
{
  GstVp9Parse *self = GST_VP9_PARSE (parse);
  GstStructure *str;
  GstVp9ParseAligment align;
  GstCaps *in_caps = NULL;
  const gchar *profile;

  str = gst_caps_get_structure (caps, 0);

  /* accept upstream info if provided */
  gst_structure_get_int (str, "width", &self->width);
  gst_structure_get_int (str, "height", &self->height);
  profile = gst_structure_get_string (str, "profile");
  if (profile)
    self->profile = gst_vp9_parse_profile_from_string (profile);
  gst_structure_get_boolean (str, "codec-alpha", &self->codec_alpha);

  /* get upstream align from caps */
  gst_vp9_parse_alignment_from_caps (caps, &align);

  /* default */
  if (align == GST_VP9_PARSE_ALIGN_NONE)
    align = GST_VP9_PARSE_ALIGN_SUPER_FRAME;

  /* prefer alignment type determined above */
  in_caps = gst_caps_copy (caps);
  gst_caps_set_simple (in_caps, "alignment", G_TYPE_STRING,
      gst_vp9_parse_alignment_to_string (align), NULL);

  /* negotiate with downstream, set output align */
  gst_vp9_parse_negotiate (self, align, in_caps);

  self->update_caps = TRUE;

  /* if all of decoder's capability related values are provided
   * by upstream, update src caps now */
  if (self->width > 0 && self->height > 0 && profile &&
      /* Other profiles defines multiple bitdepth/subsampling
       * Delaying src caps update for non profile-0 streams */
      self->profile == GST_VP9_PROFILE_0) {
    gst_vp9_parse_update_src_caps (self, in_caps);
  }

  gst_caps_unref (in_caps);

  self->in_align = align;

  return TRUE;
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
    }
    gst_structure_remove_field (s, "parsed");
  }
}

static GstCaps *
gst_vp9_parse_get_sink_caps (GstBaseParse * parse, GstCaps * filter)
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
  } else {
    peercaps = gst_pad_peer_query_caps (GST_BASE_PARSE_SRC_PAD (parse), NULL);
  }

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
