/* GStreamer
 * Copyright (C) 2013 Rdio, Inc. <ingestions@rdio.com>
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
 * SECTION:element-combdetect
 * @title: gstcombdetect
 *
 * The combdetect element detects if combing artifacts are present in
 * a raw video stream, and if so, marks them with a zebra stripe
 * pattern.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=file.mov ! decodebin ! combdetect !
 *     xvimagesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstcombdetect.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_comb_detect_debug_category);
#define GST_CAT_DEFAULT gst_comb_detect_debug_category

/* prototypes */


static GstCaps *gst_comb_detect_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_comb_detect_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_comb_detect_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);

enum
{
  PROP_0
};

/* pad templates */

/* Yeah, the max width is hard-coded 2048. */
#define MAX_WIDTH 2048
#define VIDEO_CAPS \
  "video/x-raw, " \
  "format = (string) { I420, Y444, Y42B }, " \
  "width = [1, 2048], " \
  "height = " GST_VIDEO_SIZE_RANGE ", " \
  "framerate = " GST_VIDEO_FPS_RANGE

static GstStaticPadTemplate gst_comb_detect_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS)
    );

static GstStaticPadTemplate gst_comb_detect_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS)
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstCombDetect, gst_comb_detect, GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_comb_detect_debug_category, "combdetect", 0,
        "debug category for combdetect element"));
GST_ELEMENT_REGISTER_DEFINE (combdetect, "combdetect", GST_RANK_NONE,
    GST_TYPE_COMB_DETECT);

static void
gst_comb_detect_class_init (GstCombDetectClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_comb_detect_sink_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_comb_detect_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Comb Detect", "Video/Filter", "Detect combing artifacts in video stream",
      "David Schleef <ds@schleef.org>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_comb_detect_transform_caps);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_comb_detect_set_info);
  video_filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_comb_detect_transform_frame);

}

static void
gst_comb_detect_init (GstCombDetect * combdetect)
{
}


static GstCaps *
gst_comb_detect_transform_caps (GstBaseTransform * trans,
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
    }
    g_value_unset (&value);
    g_value_unset (&v);
  } else {
    for (i = 0; i < gst_caps_get_size (othercaps); i++) {
      GstStructure *structure = gst_caps_get_structure (othercaps, i);
      gst_structure_set (structure, "interlace-mode", G_TYPE_STRING,
          "progressive", NULL);
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


static gboolean
gst_comb_detect_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstCombDetect *combdetect = GST_COMB_DETECT (filter);

  memcpy (&combdetect->vinfo, in_info, sizeof (GstVideoInfo));

  return TRUE;
}

static GstFlowReturn
gst_comb_detect_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe)
{
  static int z;
  int k;
  int height;
  int width;

#define GET_LINE(frame,comp,line) (((unsigned char *)(frame)->data[k]) + \
      (line) * GST_VIDEO_FRAME_COMP_STRIDE((frame), (comp)))

  z++;

  for (k = 1; k < 3; k++) {
    int i;
    height = GST_VIDEO_FRAME_COMP_HEIGHT (outframe, k);
    width = GST_VIDEO_FRAME_COMP_WIDTH (outframe, k);
    for (i = 0; i < height; i++) {
      memcpy (GET_LINE (outframe, k, i), GET_LINE (inframe, k, i), width);
    }
  }

  {
    int j;
    int thisline[MAX_WIDTH];
    int score = 0;

    height = GST_VIDEO_FRAME_COMP_HEIGHT (outframe, 0);
    width = GST_VIDEO_FRAME_COMP_WIDTH (outframe, 0);

    memset (thisline, 0, sizeof (thisline));

    k = 0;
    for (j = 0; j < height; j++) {
      int i;
      if (j < 2 || j >= height - 2) {
        guint8 *dest = GET_LINE (outframe, 0, j);
        guint8 *src = GET_LINE (inframe, 0, j);
        for (i = 0; i < width; i++) {
          dest[i] = src[i] / 2;
        }
      } else {
        guint8 *dest = GET_LINE (outframe, 0, j);
        guint8 *src1 = GET_LINE (inframe, 0, j - 1);
        guint8 *src2 = GET_LINE (inframe, 0, j);
        guint8 *src3 = GET_LINE (inframe, 0, j + 1);

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
            dest[i] = ((i + j + z) & 0x4) ? 235 : 16;
            score++;
          } else {
            dest[i] = src2[i];
          }
        }
      }
    }

    if (score > 10)
      GST_DEBUG ("score %d", score);
  }

  return GST_FLOW_OK;
}
