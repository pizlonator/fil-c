/*
 * gstvp82json.c - VP8 parsed bistream to json
 *
 * Copyright (C) 2023 Collabora
 *   Author: Benjamin Gaignard <benjamin.gaignard@collabora.com>
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
 * SECTION:element-vp82json
 * @title: vp82json
 *
 * Convert VP8 bitstream parameters to JSON formated text.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/vp8/file ! parsebin ! vp82json ! filesink location=/path/to/json/file
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include <json-glib/json-glib.h>

#include "gstvp82json.h"

GST_DEBUG_CATEGORY (gst_vp8_2_json_debug);
#define GST_CAT_DEFAULT gst_vp8_2_json_debug

struct _GstVp82json
{
  GstElement parent;

  GstPad *sinkpad, *srcpad;
  guint frame_counter;
  GstVp8Parser parser;

  JsonObject *json;
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-json,format=vp8"));


#define gst_vp8_2_json_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVp82json, gst_vp8_2_json,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_vp8_2_json_debug, "vp82json", 0,
        "VP8 to json"));

static void
gst_vp8_2_json_finalize (GObject * object)
{
  GstVp82json *self = GST_VP8_2_JSON (object);

  json_object_unref (self->json);
}

static gchar *
get_string_from_json_object (JsonObject * object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_indent (generator, 2);
  json_generator_set_indent_char (generator, ' ');
  json_generator_set_pretty (generator, TRUE);
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);
  return text;
}

static GstFlowReturn
gst_vp8_2_json_chain (GstPad * sinkpad, GstObject * object, GstBuffer * in_buf)
{
  GstVp82json *self = GST_VP8_2_JSON (object);
  JsonObject *json = self->json;
  GstVp8ParserResult pres;
  GstVp8FrameHdr frame_hdr;
  JsonObject *range;
  GstBuffer *out_buf;
  gchar *json_string;
  guint length;
  GstMapInfo in_map, out_map;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_buffer_map (in_buf, &in_map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map buffer");
    return GST_FLOW_ERROR;
  }

  pres =
      gst_vp8_parser_parse_frame_header (&self->parser, &frame_hdr, in_map.data,
      in_map.size);
  if (pres != GST_VP8_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Cannot parse frame header");
    goto unmap;
  }

  json_object_set_int_member (json, "frame number", self->frame_counter++);

  if (frame_hdr.key_frame)
    json_object_set_string_member (json, "frame type", "keyframe");
  else
    json_object_set_string_member (json, "frame type", "interframe");

  json_object_set_int_member (json, "version", frame_hdr.version);
  json_object_set_int_member (json, "show frame", frame_hdr.show_frame);
  json_object_set_int_member (json, "data chunk size",
      frame_hdr.data_chunk_size);
  json_object_set_int_member (json, "first part size",
      frame_hdr.first_part_size);

  if (frame_hdr.key_frame) {
    JsonArray *array, *token_probs, *mv_probs, *y_prob, *uv_prob;
    JsonObject *quant_indices, *mode_probs;
    guint i, j, k, l;

    json_object_set_int_member (json, "width", frame_hdr.width);
    json_object_set_int_member (json, "height", frame_hdr.height);
    json_object_set_int_member (json, "horizontal scale code",
        frame_hdr.horiz_scale_code);
    json_object_set_int_member (json, "vertical scale code",
        frame_hdr.vert_scale_code);
    json_object_set_int_member (json, "color space", frame_hdr.color_space);
    json_object_set_int_member (json, "clamping type", frame_hdr.clamping_type);
    json_object_set_int_member (json, "filter type", frame_hdr.filter_type);
    json_object_set_int_member (json, "loop filter level",
        frame_hdr.loop_filter_level);
    json_object_set_int_member (json, "sharpness level",
        frame_hdr.sharpness_level);
    json_object_set_int_member (json, "log2 nbr of dct partitions",
        frame_hdr.log2_nbr_of_dct_partitions);

    array = json_array_new ();
    for (i = 0; i < 8; i++)
      json_array_add_int_element (array, frame_hdr.partition_size[i]);

    json_object_set_array_member (json, "partition size", array);

    quant_indices = json_object_new ();
    json_object_set_int_member (quant_indices, "y ac qi",
        frame_hdr.quant_indices.y_ac_qi);
    json_object_set_int_member (quant_indices, "y dc delta",
        frame_hdr.quant_indices.y_dc_delta);
    json_object_set_int_member (quant_indices, "y2 dc delta",
        frame_hdr.quant_indices.y2_dc_delta);
    json_object_set_int_member (quant_indices, "y2 ac delta",
        frame_hdr.quant_indices.y2_ac_delta);
    json_object_set_int_member (quant_indices, "uv dc delta",
        frame_hdr.quant_indices.uv_dc_delta);
    json_object_set_int_member (quant_indices, "uv ac delta",
        frame_hdr.quant_indices.uv_ac_delta);
    json_object_set_object_member (json, "quant indices", quant_indices);

    token_probs = json_array_new ();
    for (i = 0; i < 4; i++)
      for (j = 0; j < 8; j++)
        for (k = 0; k < 3; k++)
          for (l = 0; l < 11; l++)
            json_array_add_int_element (token_probs,
                frame_hdr.token_probs.prob[i][j][k][l]);

    json_object_set_array_member (json, "token probabilities", token_probs);

    mv_probs = json_array_new ();
    for (i = 0; i < 2; i++)
      for (j = 0; j < 19; j++)
        json_array_add_int_element (mv_probs, frame_hdr.mv_probs.prob[i][j]);

    json_object_set_array_member (json, "motion vector probabilities",
        mv_probs);

    mode_probs = json_object_new ();
    y_prob = json_array_new ();
    for (i = 0; i < 4; i++)
      json_array_add_int_element (y_prob, frame_hdr.mode_probs.y_prob[i]);

    json_object_set_array_member (mode_probs, "y probabilities", y_prob);

    uv_prob = json_array_new ();
    for (i = 0; i < 3; i++)
      json_array_add_int_element (uv_prob, frame_hdr.mode_probs.uv_prob[i]);

    json_object_set_array_member (mode_probs, "uv probabilities", uv_prob);

    json_object_set_object_member (json, "mode probabilities", mode_probs);

    json_object_set_int_member (json, "refresh entropy probs",
        frame_hdr.refresh_entropy_probs);
    json_object_set_int_member (json, "refresh last", frame_hdr.refresh_last);
  } else {
    json_object_set_int_member (json, "refresh golden frame",
        frame_hdr.refresh_golden_frame);
    json_object_set_int_member (json, "refresh alternate frame",
        frame_hdr.refresh_alternate_frame);
    json_object_set_int_member (json, "copy buffer to golden",
        frame_hdr.copy_buffer_to_golden);
    json_object_set_int_member (json, "copy buffer to alternate",
        frame_hdr.copy_buffer_to_alternate);
    json_object_set_int_member (json, "sign bias golden",
        frame_hdr.sign_bias_golden);
    json_object_set_int_member (json, "sign bias alternate",
        frame_hdr.sign_bias_alternate);
    json_object_set_int_member (json, "mb no skip coeff",
        frame_hdr.mb_no_skip_coeff);
    json_object_set_int_member (json, "prob skip false",
        frame_hdr.prob_skip_false);
    json_object_set_int_member (json, "prob intra", frame_hdr.prob_intra);
    json_object_set_int_member (json, "prob last", frame_hdr.prob_last);
    json_object_set_int_member (json, "prob gf", frame_hdr.prob_gf);
  }

  range = json_object_new ();

  json_object_set_int_member (range, "rd range", frame_hdr.rd_range);
  json_object_set_int_member (range, "rd value", frame_hdr.rd_value);
  json_object_set_int_member (range, "rd count", frame_hdr.rd_count);

  json_object_set_object_member (json, "range decoder", range);

  json_object_set_int_member (json, "header size", frame_hdr.header_size);

  json_string = get_string_from_json_object (json);
  length = strlen (json_string);
  out_buf = gst_buffer_new_allocate (NULL, length, NULL);
  gst_buffer_map (out_buf, &out_map, GST_MAP_WRITE);
  if (length)
    memcpy (&out_map.data[0], json_string, length);
  gst_buffer_unmap (out_buf, &out_map);

  g_free (json_string);

  gst_buffer_copy_into (out_buf, in_buf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS |
      GST_BUFFER_COPY_METADATA, 0, -1);
  gst_pad_push (self->srcpad, out_buf);

unmap:
  gst_buffer_unmap (in_buf, &in_map);
  gst_buffer_unref (in_buf);

  return ret;
}

static gboolean
gst_vp8_2_json_set_caps (GstVp82json * self, GstCaps * caps)
{
  GstCaps *src_caps =
      gst_caps_new_simple ("text/x-json", "format", G_TYPE_STRING, "vp8", NULL);
  GstEvent *event;

  event = gst_event_new_caps (src_caps);
  gst_caps_unref (src_caps);

  return gst_pad_push_event (self->srcpad, event);
}

static gboolean
gst_vp8_2_json_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstVp82json *self = GST_VP8_2_JSON (parent);
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_vp8_2_json_set_caps (self, caps);
      gst_event_unref (event);
      break;
    }
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static GstStateChangeReturn
gst_vp8_2_json_change_state (GstElement * element, GstStateChange transition)
{
  GstVp82json *self = GST_VP8_2_JSON (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->frame_counter = 0;
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_vp8_2_json_class_init (GstVp82jsonClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state = gst_vp8_2_json_change_state;
  gobject_class->finalize = gst_vp8_2_json_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "Vp82json",
      "Transform",
      "VP8 to json element",
      "Benjamin Gaignard <benjamin.gaignard@collabora.com>");
}

static void
gst_vp8_2_json_init (GstVp82json * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_vp8_2_json_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_vp8_2_json_sink_event));

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->json = json_object_new ();
  self->frame_counter = 0;

  gst_vp8_parser_init (&self->parser);
}
