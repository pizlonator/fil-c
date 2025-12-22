/* GStreamer
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
 * Copyright (C) 2013 Rdio Inc <ingestions@rdio.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-ivtc
 * @title: gstivtc
 *
 * The ivtc element is an inverse telecine filter.  It takes interlaced
 * video that was created from progressive content using a telecine
 * filter, and reconstructs the original progressive content.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc pattern=ball ! video/x-raw,framerate=24/1 !
 *     interlace !
 *     ivtc ! video/x-raw,framerate=24/1 ! fakesink
 * ]|
 *
 * This pipeline creates a progressive video stream at 24 fps, and
 * converts it to a 60 fields per second interlaced stream.  Then the
 * stream is inversed telecine'd back to 24 fps, yielding approximately
 * the original videotestsrc content.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include "gstivtc.h"
#include <string.h>
#include <math.h>

/* only because element registration is in this file */
#include "gstcombdetect.h"

GST_DEBUG_CATEGORY_STATIC (gst_ivtc_debug_category);
#define GST_CAT_DEFAULT gst_ivtc_debug_category

/* prototypes */


static GstCaps *gst_ivtc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_ivtc_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_ivtc_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps);
static gboolean gst_ivtc_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_ivtc_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_ivtc_flush (GstIvtc * ivtc);
static void gst_ivtc_retire_fields (GstIvtc * ivtc, int n_fields);
static void gst_ivtc_construct_frame (GstIvtc * itvc, GstBuffer * outbuf);

static int get_comb_score (GstVideoFrame * top, GstVideoFrame * bottom);

enum
{
  PROP_0
};

/* pad templates */

#define MAX_WIDTH 2048
#define VIDEO_CAPS \
  "video/x-raw, " \
  "format = (string) { I420, Y444, Y42B }, " \
  "width = [1, 2048], " \
  "height = " GST_VIDEO_SIZE_RANGE ", " \
  "framerate = " GST_VIDEO_FPS_RANGE

static GstStaticPadTemplate gst_ivtc_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS)
    );

static GstStaticPadTemplate gst_ivtc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS)
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstIvtc, gst_ivtc, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_ivtc_debug_category, "ivtc", 0,
        "debug category for ivtc element"));
GST_ELEMENT_REGISTER_DEFINE (ivtc, "ivtc", GST_RANK_NONE, GST_TYPE_IVTC);

static void
gst_ivtc_class_init (GstIvtcClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ivtc_sink_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_ivtc_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Inverse Telecine", "Video/Filter", "Inverse Telecine Filter",
      "David Schleef <ds@schleef.org>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ivtc_transform_caps);
  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_ivtc_fixate_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_ivtc_set_caps);
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR (gst_ivtc_sink_event);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_ivtc_transform);
}

static void
gst_ivtc_init (GstIvtc * ivtc)
{
}

static GstCaps *
gst_ivtc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *othercaps;
  int i;

  othercaps = gst_caps_copy (caps);

  if (direction == GST_PAD_SRC) {
    GValue value = G_VALUE_INIT;
    GValue v = G_VALUE_INIT;

    g_value_init (&value, GST_TYPE_LIST);
    g_value_init (&v, G_TYPE_STRING);

    g_value_set_string (&v, "interleaved");
    gst_value_list_append_value (&value, &v);
    g_value_set_string (&v, "mixed");
    gst_value_list_append_value (&value, &v);
    g_value_set_string (&v, "progressive");
    gst_value_list_append_value (&value, &v);

    for (i = 0; i < gst_caps_get_size (othercaps); i++) {
      GstStructure *structure = gst_caps_get_structure (othercaps, i);
      gst_structure_set_value (structure, "interlace-mode", &value);
      gst_structure_remove_field (structure, "framerate");
    }
    g_value_unset (&value);
    g_value_unset (&v);
  } else {
    for (i = 0; i < gst_caps_get_size (othercaps); i++) {
      GstStructure *structure = gst_caps_get_structure (othercaps, i);
      gst_structure_set (structure, "interlace-mode", G_TYPE_STRING,
          "progressive", NULL);
      gst_structure_remove_field (structure, "framerate");
    }
  }

  if (filter) {
    GstCaps *intersect;

    intersect = gst_caps_intersect (othercaps, filter);
    gst_caps_unref (othercaps);
    othercaps = intersect;
  }

  return othercaps;
}

static GstCaps *
gst_ivtc_fixate_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *result;

  GST_DEBUG_OBJECT (trans, "fixating caps %" GST_PTR_FORMAT, othercaps);

  result = gst_caps_make_writable (othercaps);
  if (direction == GST_PAD_SINK) {
    GstVideoInfo info;
    if (gst_video_info_from_caps (&info, caps)) {
      /* Smarter decision */
      GST_DEBUG_OBJECT (trans, "Input framerate is %d/%d", info.fps_n,
          info.fps_d);
      if (info.fps_n == 30000 && info.fps_d == 1001)
        gst_caps_set_simple (result, "framerate", GST_TYPE_FRACTION, 24000,
            1001, NULL);
      else
        gst_caps_set_simple (result, "framerate", GST_TYPE_FRACTION, 24, 1,
            NULL);
    } else {
      gst_caps_set_simple (result, "framerate", GST_TYPE_FRACTION, 24, 1, NULL);
    }
  }

  result = gst_caps_fixate (result);

  return result;
}

static gboolean
gst_ivtc_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstIvtc *ivtc = GST_IVTC (trans);

  gst_video_info_from_caps (&ivtc->sink_video_info, incaps);
  gst_video_info_from_caps (&ivtc->src_video_info, outcaps);

  ivtc->field_duration = gst_util_uint64_scale_int (GST_SECOND,
      ivtc->sink_video_info.fps_d, ivtc->sink_video_info.fps_n * 2);
  GST_DEBUG_OBJECT (trans, "field duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (ivtc->field_duration));

  return TRUE;
}

/* sink and src pad event handlers */
static gboolean
gst_ivtc_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstIvtc *ivtc = GST_IVTC (trans);

  GST_DEBUG_OBJECT (ivtc, "sink_event");

  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    const GstSegment *seg;

    gst_ivtc_flush (ivtc);

    /* FIXME this should handle update events */

    gst_event_parse_segment (event, &seg);
    gst_segment_copy_into (seg, &ivtc->segment);
    ivtc->current_ts = ivtc->segment.start;
  }

  return GST_BASE_TRANSFORM_CLASS (gst_ivtc_parent_class)->sink_event (trans,
      event);
}

static void
gst_ivtc_flush (GstIvtc * ivtc)
{
  if (ivtc->n_fields > 0) {
    GST_FIXME_OBJECT (ivtc, "not sending flushed fields to srcpad");
  }

  gst_ivtc_retire_fields (ivtc, ivtc->n_fields);
}

enum
{
  TOP_FIELD = 0,
  BOTTOM_FIELD = 1
};

static void
add_field (GstIvtc * ivtc, GstBuffer * buffer, int parity, int index)
{
  int i = ivtc->n_fields;
  GstClockTime ts;
  GstIvtcField *field = &ivtc->fields[i];

  g_return_if_fail (i < GST_IVTC_MAX_FIELDS);

  ts = GST_BUFFER_PTS (buffer) + index * ivtc->field_duration;
  if (ts + ivtc->field_duration < ivtc->segment.start) {
    /* drop, it's before our segment */
    return;
  }

  GST_DEBUG ("adding field %d", i);

  field->buffer = gst_buffer_ref (buffer);
  field->parity = parity;
  field->ts = ts;

  gst_video_frame_map (&ivtc->fields[i].frame, &ivtc->sink_video_info,
      buffer, GST_MAP_READ);

  ivtc->n_fields++;
}

static int
similarity (GstIvtc * ivtc, int i1, int i2)
{
  GstIvtcField *f1, *f2;
  int score;

  g_return_val_if_fail (i1 >= 0 && i1 < ivtc->n_fields, 0);
  g_return_val_if_fail (i2 >= 0 && i2 < ivtc->n_fields, 0);

  f1 = &ivtc->fields[i1];
  f2 = &ivtc->fields[i2];

  if (f1->parity == TOP_FIELD) {
    score = get_comb_score (&f1->frame, &f2->frame);
  } else {
    score = get_comb_score (&f2->frame, &f1->frame);
  }

  GST_DEBUG ("score %d", score);

  return score;
}

#define GET_LINE(frame,comp,line) (((unsigned char *)(frame)->data[k]) + \
      (line) * GST_VIDEO_FRAME_COMP_STRIDE((frame), (comp)))
#define GET_LINE_IL(top,bottom,comp,line) \
  (((unsigned char *)(((line)&1)?(bottom):(top))->data[k]) + \
      (line) * GST_VIDEO_FRAME_COMP_STRIDE((top), (comp)))

static void
reconstruct (GstIvtc * ivtc, GstVideoFrame * dest_frame, int i1, int i2)
{
  GstVideoFrame *top, *bottom;
  int width, height;
  int j, k;

  g_return_if_fail (i1 >= 0 && i1 < ivtc->n_fields);
  g_return_if_fail (i2 >= 0 && i2 < ivtc->n_fields);

  if (ivtc->fields[i1].parity == TOP_FIELD) {
    top = &ivtc->fields[i1].frame;
    bottom = &ivtc->fields[i2].frame;
  } else {
    bottom = &ivtc->fields[i1].frame;
    top = &ivtc->fields[i2].frame;
  }

  for (k = 0; k < 3; k++) {
    height = GST_VIDEO_FRAME_COMP_HEIGHT (top, k);
    width = GST_VIDEO_FRAME_COMP_WIDTH (top, k);
    for (j = 0; j < height; j++) {
      guint8 *dest = GET_LINE (dest_frame, k, j);
      guint8 *src = GET_LINE_IL (top, bottom, k, j);

      memcpy (dest, src, width);
    }
  }

}

static int
reconstruct_line (guint8 * line1, guint8 * line2, int i, int a, int b, int c,
    int d)
{
  int x;

  x = line1[i - 3] * a;
  x += line1[i - 2] * b;
  x += line1[i - 1] * c;
  x += line1[i - 0] * d;
  x += line2[i + 0] * d;
  x += line2[i + 1] * c;
  x += line2[i + 2] * b;
  x += line2[i + 3] * a;
  return (x + 16) >> 5;
}


static void
reconstruct_single (GstIvtc * ivtc, GstVideoFrame * dest_frame, int i1)
{
  int j;
  int k;
  int height;
  int width;
  GstIvtcField *field = &ivtc->fields[i1];

  for (k = 0; k < 1; k++) {
    height = GST_VIDEO_FRAME_COMP_HEIGHT (dest_frame, k);
    width = GST_VIDEO_FRAME_COMP_WIDTH (dest_frame, k);
    for (j = 0; j < height; j++) {
      if ((j & 1) == field->parity) {
        memcpy (GET_LINE (dest_frame, k, j),
            GET_LINE (&field->frame, k, j), width);
      } else {
        if (j == 0 || j == height - 1) {
          memcpy (GET_LINE (dest_frame, k, j),
              GET_LINE (&field->frame, k, (j ^ 1)), width);
        } else {
          guint8 *dest = GET_LINE (dest_frame, k, j);
          guint8 *line1 = GET_LINE (&field->frame, k, j - 1);
          guint8 *line2 = GET_LINE (&field->frame, k, j + 1);
          int i;

#define MARGIN 3
          for (i = MARGIN; i < width - MARGIN; i++) {
            int dx, dy;

            dx = -line1[i - 1] - line2[i - 1] + line1[i + 1] + line2[i + 1];
            dx *= 2;

            dy = -line1[i - 1] - 2 * line1[i] - line1[i + 1]
                + line2[i - 1] + 2 * line2[i] + line2[i + 1];
            if (dy < 0) {
              dy = -dy;
              dx = -dx;
            }

            if (dx == 0 && dy == 0) {
              dest[i] = (line1[i] + line2[i] + 1) >> 1;
            } else if (dx < 0) {
              if (dx < -2 * dy) {
                dest[i] = reconstruct_line (line1, line2, i, 0, 0, 0, 16);
              } else if (dx < -dy) {
                dest[i] = reconstruct_line (line1, line2, i, 0, 0, 8, 8);
              } else if (2 * dx < -dy) {
                dest[i] = reconstruct_line (line1, line2, i, 0, 4, 8, 4);
              } else if (3 * dx < -dy) {
                dest[i] = reconstruct_line (line1, line2, i, 1, 7, 7, 1);
              } else {
                dest[i] = reconstruct_line (line1, line2, i, 4, 8, 4, 0);
              }
            } else {
              if (dx > 2 * dy) {
                dest[i] = reconstruct_line (line2, line1, i, 0, 0, 0, 16);
              } else if (dx > dy) {
                dest[i] = reconstruct_line (line2, line1, i, 0, 0, 8, 8);
              } else if (2 * dx > dy) {
                dest[i] = reconstruct_line (line2, line1, i, 0, 4, 8, 4);
              } else if (3 * dx > dy) {
                dest[i] = reconstruct_line (line2, line1, i, 1, 7, 7, 1);
              } else {
                dest[i] = reconstruct_line (line2, line1, i, 4, 8, 4, 0);
              }
            }
          }

          for (i = 0; i < MARGIN; i++) {
            dest[i] = (line1[i] + line2[i] + 1) >> 1;
          }
          for (i = width - MARGIN; i < width; i++) {
            dest[i] = (line1[i] + line2[i] + 1) >> 1;
          }
        }
      }
    }
  }
  for (k = 1; k < 3; k++) {
    height = GST_VIDEO_FRAME_COMP_HEIGHT (dest_frame, k);
    width = GST_VIDEO_FRAME_COMP_WIDTH (dest_frame, k);
    for (j = 0; j < height; j++) {
      if ((j & 1) == field->parity) {
        memcpy (GET_LINE (dest_frame, k, j),
            GET_LINE (&field->frame, k, j), width);
      } else {
        if (j == 0 || j == height - 1) {
          memcpy (GET_LINE (dest_frame, k, j),
              GET_LINE (&field->frame, k, (j ^ 1)), width);
        } else {
          guint8 *dest = GET_LINE (dest_frame, k, j);
          guint8 *line1 = GET_LINE (&field->frame, k, j - 1);
          guint8 *line2 = GET_LINE (&field->frame, k, j + 1);
          int i;
          for (i = 0; i < width; i++) {
            dest[i] = (line1[i] + line2[i] + 1) >> 1;
          }
        }
      }
    }
  }
}

static void
gst_ivtc_retire_fields (GstIvtc * ivtc, int n_fields)
{
  int i;

  if (n_fields == 0)
    return;

  for (i = 0; i < n_fields; i++) {
    gst_video_frame_unmap (&ivtc->fields[i].frame);
    gst_buffer_unref (ivtc->fields[i].buffer);
  }

  memmove (ivtc->fields, ivtc->fields + n_fields,
      sizeof (GstIvtcField) * (ivtc->n_fields - n_fields));
  ivtc->n_fields -= n_fields;
}

static GstFlowReturn
gst_ivtc_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstIvtc *ivtc = GST_IVTC (trans);
  GstFlowReturn ret;

  GST_DEBUG_OBJECT (ivtc, "transform");

  if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_TFF)) {
    add_field (ivtc, inbuf, TOP_FIELD, 0);
    if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_ONEFIELD)) {
      add_field (ivtc, inbuf, BOTTOM_FIELD, 1);
      if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_RFF)) {
        add_field (ivtc, inbuf, TOP_FIELD, 2);
      }
    }
  } else {
    add_field (ivtc, inbuf, BOTTOM_FIELD, 0);
    if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_ONEFIELD)) {
      add_field (ivtc, inbuf, TOP_FIELD, 1);
      if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_RFF)) {
        add_field (ivtc, inbuf, BOTTOM_FIELD, 2);
      }
    }
  }

  while (ivtc->n_fields > 0 &&
      ivtc->fields[0].ts + GST_MSECOND * 50 < ivtc->current_ts) {
    GST_DEBUG ("retiring early field");
    gst_ivtc_retire_fields (ivtc, 1);
  }

  GST_DEBUG ("n_fields %d", ivtc->n_fields);
  if (ivtc->n_fields < 4) {
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  gst_ivtc_construct_frame (ivtc, outbuf);
  while (ivtc->n_fields >= 4) {
    GstBuffer *buf;
    buf = gst_buffer_copy (outbuf);
    GST_DEBUG ("pushing extra frame");
    ret = gst_pad_push (GST_BASE_TRANSFORM_SRC_PAD (trans), buf);
    if (ret != GST_FLOW_OK) {
      return ret;
    }

    gst_ivtc_construct_frame (ivtc, outbuf);
  }

  return GST_FLOW_OK;
}

static void
gst_ivtc_construct_frame (GstIvtc * ivtc, GstBuffer * outbuf)
{
  int anchor_index;
  int prev_score, next_score;
  GstVideoFrame dest_frame;
  int n_retire;
  gboolean forward_ok;

  anchor_index = 1;
  if (ivtc->fields[anchor_index].ts < ivtc->current_ts) {
    forward_ok = TRUE;
  } else {
    forward_ok = FALSE;
  }

  prev_score = similarity (ivtc, anchor_index - 1, anchor_index);
  next_score = similarity (ivtc, anchor_index, anchor_index + 1);

  gst_video_frame_map (&dest_frame, &ivtc->src_video_info, outbuf,
      GST_MAP_WRITE);

#define THRESHOLD 100
  if (prev_score < THRESHOLD) {
    if (forward_ok && next_score < prev_score) {
      reconstruct (ivtc, &dest_frame, anchor_index, anchor_index + 1);
      n_retire = anchor_index + 2;
    } else {
      if (prev_score >= THRESHOLD / 2) {
        GST_INFO ("borderline prev (%d, %d)", prev_score, next_score);
      }
      reconstruct (ivtc, &dest_frame, anchor_index, anchor_index - 1);
      n_retire = anchor_index + 1;
    }
  } else if (next_score < THRESHOLD) {
    if (next_score >= THRESHOLD / 2) {
      GST_INFO ("borderline prev (%d, %d)", prev_score, next_score);
    }
    reconstruct (ivtc, &dest_frame, anchor_index, anchor_index + 1);
    if (forward_ok) {
      n_retire = anchor_index + 2;
    } else {
      n_retire = anchor_index + 1;
    }
  } else {
    if (prev_score < THRESHOLD * 2 || next_score < THRESHOLD * 2) {
      GST_INFO ("borderline single (%d, %d)", prev_score, next_score);
    }
    reconstruct_single (ivtc, &dest_frame, anchor_index);
    n_retire = anchor_index + 1;
  }

  GST_DEBUG ("retiring %d", n_retire);
  gst_ivtc_retire_fields (ivtc, n_retire);

  gst_video_frame_unmap (&dest_frame);

  GST_BUFFER_PTS (outbuf) = ivtc->current_ts;
  GST_BUFFER_DTS (outbuf) = ivtc->current_ts;
  /* FIXME this is not how to produce durations */
  GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale (GST_SECOND,
      ivtc->src_video_info.fps_d, ivtc->src_video_info.fps_n);
  GST_BUFFER_FLAG_UNSET (outbuf, GST_VIDEO_BUFFER_FLAG_INTERLACED |
      GST_VIDEO_BUFFER_FLAG_TFF | GST_VIDEO_BUFFER_FLAG_RFF |
      GST_VIDEO_BUFFER_FLAG_ONEFIELD);
  ivtc->current_ts += GST_BUFFER_DURATION (outbuf);

}

static int
get_comb_score (GstVideoFrame * top, GstVideoFrame * bottom)
{
  int j;
  int thisline[MAX_WIDTH];
  int score = 0;
  int height;
  int width;
  int k;

  height = GST_VIDEO_FRAME_COMP_HEIGHT (top, 0);
  width = GST_VIDEO_FRAME_COMP_WIDTH (top, 0);

  memset (thisline, 0, sizeof (thisline));

  k = 0;
  /* remove a few lines from top and bottom, as they sometimes contain
   * artifacts */
  for (j = 2; j < height - 2; j++) {
    guint8 *src1 = GET_LINE_IL (top, bottom, 0, j - 1);
    guint8 *src2 = GET_LINE_IL (top, bottom, 0, j);
    guint8 *src3 = GET_LINE_IL (top, bottom, 0, j + 1);
    int i;

    for (i = 0; i < width; i++) {
      if (src2[i] < MIN (src1[i], src3[i]) - 5 ||
          src2[i] > MAX (src1[i], src3[i]) + 5) {
        if (i > 0) {
          thisline[i] += thisline[i - 1];
        }
        thisline[i]++;
        if (thisline[i] > 1000)
          thisline[i] = 1000;
      } else {
        thisline[i] = 0;
      }
      if (thisline[i] > 100) {
        score++;
      }
    }
  }

  GST_DEBUG ("score %d", score);

  return score;
}



static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_ELEMENT_REGISTER (ivtc, plugin);
  GST_ELEMENT_REGISTER (combdetect, plugin);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ivtc,
    "Inverse Telecine",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
