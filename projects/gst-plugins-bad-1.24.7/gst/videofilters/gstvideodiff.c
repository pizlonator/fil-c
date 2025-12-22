/* GStreamer
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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
 * SECTION:element-videodiff
 * @title: gstvideodiff
 *
 * The videodiff element highlights the difference between a frame and its
 * previous on the luma plane.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc pattern=ball ! videodiff ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstvideodiff.h"

GST_DEBUG_CATEGORY_STATIC (gst_video_diff_debug_category);
#define GST_CAT_DEFAULT gst_video_diff_debug_category

/* prototypes */

static GstFlowReturn gst_video_diff_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);

#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, Y444, Y42B, Y41B }")

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, Y444, Y42B, Y41B }")


G_DEFINE_TYPE_WITH_CODE (GstVideoDiff, gst_video_diff, GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_video_diff_debug_category, "videodiff", 0,
        "debug category for videodiff element"));
GST_ELEMENT_REGISTER_DEFINE (videodiff, "videodiff",
    GST_RANK_NONE, GST_TYPE_VIDEO_DIFF);

static void
gst_video_diff_class_init (GstVideoDiffClass * klass)
{
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Video Diff", "Video/Filter",
      "Visualize differences between adjacent video frames",
      "David Schleef <ds@schleef.org>");

  video_filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_video_diff_transform_frame);
}

static void
gst_video_diff_init (GstVideoDiff * videodiff)
{
  videodiff->threshold = 10;
}

static GstFlowReturn
gst_video_diff_transform_frame_ip_planarY (GstVideoDiff * videodiff,
    GstVideoFrame * outframe, GstVideoFrame * inframe, GstVideoFrame * oldframe)
{
  int width = inframe->info.width;
  int height = inframe->info.height;
  int i, j;
  int threshold = videodiff->threshold;
  int t = videodiff->t;

  for (j = 0; j < height; j++) {
    guint8 *d = (guint8 *) outframe->data[0] + outframe->info.stride[0] * j;
    guint8 *s1 = (guint8 *) oldframe->data[0] + oldframe->info.stride[0] * j;
    guint8 *s2 = (guint8 *) inframe->data[0] + inframe->info.stride[0] * j;
    for (i = 0; i < width; i++) {
      if ((s2[i] < s1[i] - threshold) || (s2[i] > s1[i] + threshold)) {
        if ((i + j + t) & 0x4) {
          d[i] = 16;
        } else {
          d[i] = 240;
        }
      } else {
        d[i] = s2[i];
      }
    }
  }
  for (j = 0; j < GST_VIDEO_FRAME_COMP_HEIGHT (inframe, 1); j++) {
    guint8 *d = (guint8 *) outframe->data[1] + outframe->info.stride[1] * j;
    guint8 *s = (guint8 *) inframe->data[1] + inframe->info.stride[1] * j;
    memcpy (d, s, GST_VIDEO_FRAME_COMP_WIDTH (inframe, 1));
  }
  for (j = 0; j < GST_VIDEO_FRAME_COMP_HEIGHT (inframe, 2); j++) {
    guint8 *d = (guint8 *) outframe->data[2] + outframe->info.stride[2] * j;
    guint8 *s = (guint8 *) inframe->data[2] + inframe->info.stride[2] * j;
    memcpy (d, s, GST_VIDEO_FRAME_COMP_WIDTH (inframe, 2));
  }
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_video_diff_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe)
{
  GstVideoDiff *videodiff = GST_VIDEO_DIFF (filter);

  GST_DEBUG_OBJECT (videodiff, "transform_frame_ip");

  if (videodiff->previous_buffer) {
    GstVideoFrame oldframe;

    gst_video_frame_map (&oldframe, &videodiff->oldinfo,
        videodiff->previous_buffer, GST_MAP_READ);

    switch (inframe->info.finfo->format) {
      case GST_VIDEO_FORMAT_I420:
      case GST_VIDEO_FORMAT_Y41B:
      case GST_VIDEO_FORMAT_Y444:
      case GST_VIDEO_FORMAT_Y42B:
        gst_video_diff_transform_frame_ip_planarY (videodiff, outframe,
            inframe, &oldframe);
        break;
      default:
        g_assert_not_reached ();
    }

    gst_video_frame_unmap (&oldframe);
    gst_buffer_unref (videodiff->previous_buffer);
  } else {
    int k;
    int j;
    for (k = 0; k < 3; k++) {
      for (j = 0; j < GST_VIDEO_FRAME_COMP_HEIGHT (inframe, k); j++) {
        guint8 *d = (guint8 *) outframe->data[k] + outframe->info.stride[k] * j;
        guint8 *s = (guint8 *) inframe->data[k] + inframe->info.stride[k] * j;
        memcpy (d, s, GST_VIDEO_FRAME_COMP_WIDTH (inframe, k));
      }
    }
  }

  videodiff->previous_buffer = gst_buffer_ref (inframe->buffer);
  memcpy (&videodiff->oldinfo, &inframe->info, sizeof (GstVideoInfo));

  return GST_FLOW_OK;
}
