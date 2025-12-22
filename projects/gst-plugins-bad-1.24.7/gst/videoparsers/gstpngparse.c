/* GStreamer PNG Parser
 * Copyright (C) <2013> Collabora Ltd
 *  @author Olivier Crete <olivier.crete@collabora.com>
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

#include "gstvideoparserselements.h"
#include "gstpngparse.h"

#include <gst/base/base.h>
#include <gst/pbutils/pbutils.h>

#define PNG_SIGNATURE G_GUINT64_CONSTANT (0x89504E470D0A1A0A)

GST_DEBUG_CATEGORY (png_parse_debug);
#define GST_CAT_DEFAULT png_parse_debug

static GstStaticPadTemplate srctemplate =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/png, width = (int)[1, MAX], height = (int)[1, MAX],"
        "parsed = (boolean) true")
    );

static GstStaticPadTemplate sinktemplate =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/png")
    );

#define parent_class gst_png_parse_parent_class
G_DEFINE_TYPE (GstPngParse, gst_png_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (pngparse, "pngparse", GST_RANK_PRIMARY,
    GST_TYPE_PNG_PARSE, videoparsers_element_init (plugin));

static gboolean gst_png_parse_start (GstBaseParse * parse);
static gboolean gst_png_parse_event (GstBaseParse * parse, GstEvent * event);
static GstFlowReturn gst_png_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);
static GstFlowReturn gst_png_parse_pre_push_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);

static void
gst_png_parse_class_init (GstPngParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (png_parse_debug, "pngparse", 0, "png parser");

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);
  gst_element_class_set_static_metadata (gstelement_class, "PNG parser",
      "Codec/Parser/Video/Image",
      "Parses PNG files", "Olivier Crete <olivier.crete@collabora.com>");

  /* Override BaseParse vfuncs */
  parse_class->start = GST_DEBUG_FUNCPTR (gst_png_parse_start);
  parse_class->sink_event = GST_DEBUG_FUNCPTR (gst_png_parse_event);
  parse_class->handle_frame = GST_DEBUG_FUNCPTR (gst_png_parse_handle_frame);
  parse_class->pre_push_frame =
      GST_DEBUG_FUNCPTR (gst_png_parse_pre_push_frame);
}

static void
gst_png_parse_init (GstPngParse * pngparse)
{
  GST_PAD_SET_ACCEPT_INTERSECT (GST_BASE_PARSE_SINK_PAD (pngparse));
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_BASE_PARSE_SINK_PAD (pngparse));
}

static gboolean
gst_png_parse_start (GstBaseParse * parse)
{
  GstPngParse *pngparse = GST_PNG_PARSE (parse);

  GST_DEBUG_OBJECT (pngparse, "start");

  /* the start code and at least 2 empty frames (IHDR and IEND) */
  gst_base_parse_set_min_frame_size (parse, 8 + 12 + 12);

  pngparse->width = 0;
  pngparse->height = 0;

  pngparse->sent_codec_tag = FALSE;

  return TRUE;
}

static gboolean
gst_png_parse_event (GstBaseParse * parse, GstEvent * event)
{
  gboolean res;

  res = GST_BASE_PARSE_CLASS (parent_class)->sink_event (parse, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      /* the start code and at least 2 empty frames (IHDR and IEND) */
      gst_base_parse_set_min_frame_size (parse, 8 + 12 + 12);
      break;
    default:
      break;
  }

  return res;
}

static GstFlowReturn
gst_png_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstPngParse *pngparse = GST_PNG_PARSE (parse);
  GstMapInfo map;
  GstByteReader reader;
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 signature;
  guint width = 0, height = 0;

  gst_buffer_map (frame->buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  if (!gst_byte_reader_peek_uint64_be (&reader, &signature))
    goto beach;

  if (signature != PNG_SIGNATURE) {
    for (;;) {
      guint offset;

      offset = gst_byte_reader_masked_scan_uint32 (&reader, 0xffffffff,
          0x89504E47, 0, gst_byte_reader_get_remaining (&reader));

      if (offset == -1) {
        *skipsize = gst_byte_reader_get_remaining (&reader) - 4;
        goto beach;
      }

      gst_byte_reader_skip (&reader, offset);

      if (!gst_byte_reader_peek_uint64_be (&reader, &signature))
        goto beach;

      if (signature == PNG_SIGNATURE) {
        /* We're skipping, go out, we'll be back */
        *skipsize = gst_byte_reader_get_pos (&reader);
        goto beach;
      }
      gst_byte_reader_skip (&reader, 4);
    }
  }

  gst_byte_reader_skip (&reader, 8);

  for (;;) {
    guint32 length;
    guint32 code;

    if (!gst_byte_reader_get_uint32_be (&reader, &length))
      goto beach;
    if (!gst_byte_reader_get_uint32_le (&reader, &code))
      goto beach;

    GST_TRACE_OBJECT (parse, "%" GST_FOURCC_FORMAT " chunk, %u bytes",
        GST_FOURCC_ARGS (code), length);

    if (code == GST_MAKE_FOURCC ('I', 'H', 'D', 'R')) {
      if (!gst_byte_reader_get_uint32_be (&reader, &width))
        goto beach;
      if (!gst_byte_reader_get_uint32_be (&reader, &height))
        goto beach;
      length -= 8;
    } else if (code == GST_MAKE_FOURCC ('I', 'D', 'A', 'T')) {
      gst_base_parse_set_min_frame_size (parse,
          gst_byte_reader_get_pos (&reader) + 4 + length + 12);
    }

    if (!gst_byte_reader_skip (&reader, length + 4))
      goto beach;

    if (code == GST_MAKE_FOURCC ('I', 'E', 'N', 'D')) {
      /* the start code and at least 2 empty frames (IHDR and IEND) */
      gst_base_parse_set_min_frame_size (parse, 8 + 12 + 12);

      if (pngparse->width != width || pngparse->height != height) {
        GstCaps *caps, *sink_caps;

        pngparse->height = height;
        pngparse->width = width;

        caps = gst_caps_new_simple ("image/png",
            "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);

        sink_caps =
            gst_pad_get_current_caps (GST_BASE_PARSE_SINK_PAD (pngparse));

        if (sink_caps) {
          GstStructure *st;
          gint fr_num, fr_denom;

          st = gst_caps_get_structure (sink_caps, 0);
          if (st
              && gst_structure_get_fraction (st, "framerate", &fr_num,
                  &fr_denom)) {
            gst_caps_set_simple (caps,
                "framerate", GST_TYPE_FRACTION, fr_num, fr_denom, NULL);
          } else {
            GST_WARNING_OBJECT (pngparse, "No framerate set");
          }

          gst_caps_unref (sink_caps);
        }

        if (!gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), caps))
          ret = GST_FLOW_NOT_NEGOTIATED;

        gst_caps_unref (caps);

        if (ret != GST_FLOW_OK)
          goto beach;
      }


      gst_buffer_unmap (frame->buffer, &map);
      return gst_base_parse_finish_frame (parse, frame,
          gst_byte_reader_get_pos (&reader));
    }
  }

beach:

  gst_buffer_unmap (frame->buffer, &map);

  return ret;
}

static GstFlowReturn
gst_png_parse_pre_push_frame (GstBaseParse * parse, GstBaseParseFrame * frame)
{
  GstPngParse *pngparse = GST_PNG_PARSE (parse);

  if (!pngparse->sent_codec_tag) {
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
    pngparse->sent_codec_tag = TRUE;
  }

  return GST_FLOW_OK;
}
