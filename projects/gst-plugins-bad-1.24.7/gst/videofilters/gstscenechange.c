/* GStreamer
 * Copyright (C) 2011 Entropy Wave Inc <ds@entropywave.com>
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
 * SECTION:element-scenechange
 * @title: gstscenechange
 *
 * The scenechange element detects scene changes (also known as shot
 * changes) in a video stream, and sends a signal when this occurs.
 * Applications can listen to this signal and make changes to the
 * pipeline such as cutting the stream.  In addition, whenever a
 * scene change is detected, a custom downstream "GstForceKeyUnit"
 * event is sent to downstream elements.  Most video encoder elements
 * will insert synchronization points into the stream when this event
 * is received.  When used with a tee element, the scenechange element
 * can be used to align the synchronization points among multiple
 * video encoders, which is useful for segmented streaming.
 *
 * The scenechange element does not work with compressed video.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=some_file.ogv ! decodebin !
 *   scenechange ! theoraenc ! fakesink
 * ]|
 *
 */
/*
 * The algorithm used for scene change detection is a modification
 * of Jim Easterbrook's shot change detector.  I'm not aware of a
 * research paper, but the code I got the idea from is here:
 *  http://sourceforge.net/projects/shot-change/
 *
 * The method is relatively simple.  Calculate the sum of absolute
 * differences of a picture and the previous picture, and compare this
 * picture difference value with neighboring pictures.  In the original
 * algorithm, the value is compared to a configurable number of past
 * and future pictures.  However, comparing to future frames requires
 * introducing latency into the stream, which I did not want.  So this
 * implementation only compared to previous frames.
 *
 * This code is more directly derived from the scene change detection
 * implementation in Schroedinger.  Schro's implementation is closer
 * to the Easterbrook algorithm, comparing to future pictures.  In
 * terms of accuracy, schro's implementation has about 2-3 false positives
 * or false negatives per 100 scene changes.  This implementation has
 * about 5 per 100.  The threshold is tuned for minimum total false
 * positives or negatives, on the assumption that the badness of a 
 * false negative is the same as a false positive.
 *
 * This algorithm is pretty much at its limit for error rate.  I
 * recommend any future work in this area to increase the complexity
 * of detection, and then write an automatic tuning system as opposed
 * to the manual tuning I did here.
 *
 * Inside the TESTING define are some hard-coded (mostly hand-written)
 * scene change frame numbers for some easily available sequences.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <string.h>
#include "gstscenechange.h"
#include "gstscenechangeorc.h"

GST_DEBUG_CATEGORY_STATIC (gst_scene_change_debug_category);
#define GST_CAT_DEFAULT gst_scene_change_debug_category

/* prototypes */


static GstFlowReturn gst_scene_change_transform_frame_ip (GstVideoFilter *
    filter, GstVideoFrame * frame);

#undef TESTING
#ifdef TESTING
static gboolean is_shot_change (int frame_number);
#endif

enum
{
  PROP_0
};

#define VIDEO_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, Y42B, Y41B, Y444 }")

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstSceneChange, gst_scene_change,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_scene_change_debug_category, "scenechange", 0,
        "debug category for scenechange element"));
GST_ELEMENT_REGISTER_DEFINE (scenechange, "scenechange",
    GST_RANK_NONE, gst_scene_change_get_type ());

static void
gst_scene_change_class_init (GstSceneChangeClass * klass)
{
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Scene change detector",
      "Video/Filter", "Detects scene changes in video",
      "David Schleef <ds@entropywave.com>");

  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_scene_change_transform_frame_ip);

}

static void
gst_scene_change_init (GstSceneChange * scenechange)
{
}


static double
get_frame_score (GstVideoFrame * f1, GstVideoFrame * f2)
{
  guint32 score = 0;
  int width, height;

  width = f1->info.width;
  height = f1->info.height;

  orc_sad_nxm_u8 (&score, f1->data[0], f1->info.stride[0],
      f2->data[0], f2->info.stride[0], width, height);

  return ((double) score) / (width * height);
}

static GstFlowReturn
gst_scene_change_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstSceneChange *scenechange = GST_SCENE_CHANGE (filter);
  GstVideoFrame oldframe;
  double score_min;
  double score_max;
  double threshold;
  double score;
  gboolean change;
  gboolean ret;
  int i;

  GST_DEBUG_OBJECT (scenechange, "transform_frame_ip");

  if (!scenechange->oldbuf) {
    scenechange->n_diffs = 0;
    memset (scenechange->diffs, 0, sizeof (double) * SC_N_DIFFS);
    scenechange->oldbuf = gst_buffer_ref (frame->buffer);
    memcpy (&scenechange->oldinfo, &frame->info, sizeof (GstVideoInfo));
    return GST_FLOW_OK;
  }

  ret =
      gst_video_frame_map (&oldframe, &scenechange->oldinfo,
      scenechange->oldbuf, GST_MAP_READ);
  if (!ret) {
    GST_ERROR_OBJECT (scenechange, "failed to map old video frame");
    return GST_FLOW_ERROR;
  }

  score = get_frame_score (&oldframe, frame);

  gst_video_frame_unmap (&oldframe);

  gst_buffer_unref (scenechange->oldbuf);
  scenechange->oldbuf = gst_buffer_ref (frame->buffer);
  memcpy (&scenechange->oldinfo, &frame->info, sizeof (GstVideoInfo));

  memmove (scenechange->diffs, scenechange->diffs + 1,
      sizeof (double) * (SC_N_DIFFS - 1));
  scenechange->diffs[SC_N_DIFFS - 1] = score;
  scenechange->n_diffs++;

  score_min = scenechange->diffs[0];
  score_max = scenechange->diffs[0];
  for (i = 1; i < SC_N_DIFFS - 1; i++) {
    score_min = MIN (score_min, scenechange->diffs[i]);
    score_max = MAX (score_max, scenechange->diffs[i]);
  }

  threshold = 1.8 * score_max - 0.8 * score_min;

  if (scenechange->n_diffs > (SC_N_DIFFS - 1)) {
    if (score < 5) {
      change = FALSE;
    } else if (score / threshold < 1.0) {
      change = FALSE;
    } else if ((score > 30)
        && (score / scenechange->diffs[SC_N_DIFFS - 2] > 1.4)) {
      change = TRUE;
    } else if (score / threshold > 2.3) {
      change = TRUE;
    } else if (score > 50) {
      change = TRUE;
    } else {
      change = FALSE;
    }
  } else {
    change = FALSE;
  }

  if (change == TRUE) {
    memset (scenechange->diffs, 0, sizeof (double) * SC_N_DIFFS);
    scenechange->n_diffs = 0;
  }
#ifdef TESTING
  if (change != is_shot_change (scenechange->n_diffs)) {
    g_print ("%d %g %g %g %d\n", scenechange->n_diffs, score / threshold,
        score, threshold, change);
  }
#endif

  if (change) {
    GstEvent *event;

    GST_INFO_OBJECT (scenechange, "%d %g %g %g %d",
        scenechange->n_diffs, score / threshold, score, threshold, change);

    event =
        gst_video_event_new_downstream_force_key_unit (GST_BUFFER_PTS
        (frame->buffer), GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE,
        scenechange->count++);

    gst_pad_push_event (GST_BASE_TRANSFORM_SRC_PAD (scenechange), event);
  }

  return GST_FLOW_OK;
}






#ifdef TESTING
/* This is from ds's personal collection.  No, you can't have it. */
int showreel_changes[] = {
  242, 483, 510, 550, 579, 603, 609, 1056, 1067, 1074, 1079, 1096,
  1106, 1113, 1127, 1145, 1156, 1170, 1212, 1228, 1243, 1269, 1274,
  1322, 1349, 1370, 1378, 1423, 1456, 1458, 1508, 1519, 1542, 1679,
  1767, 1837, 1895, 1962, 2006, 2035, 2102, 2139, 2196, 2561, 2664,
  2837, 2895, 2985, 3035, 3077, 3128, 3176, 3218, 3306, 3351, 3388,
  3421, 3470, 3711, 3832, 4029, 4184, 4444, 4686, 4719, 4825, 4941,
  5009, 5091, 5194, 5254, 5286, 5287, 5343, 5431, 5501, 5634, 5695, 5788,
  5839, 5861, 5930, 6030, 6168, 6193, 6237, 6336, 6376, 6421, 6495,
  6550, 6611, 6669, 6733, 6819, 6852, 6944, 7087, 7148, 7189, 7431,
  7540, 7599, 7632, 7661, 7693, 7930, 7963, 8003, 8076, 8109, 8147,
  8177, 8192, 8219, 8278, 8322, 8370, 8409, 8566, 8603, 8747, 8775,
  8873, 8907, 8955, 8969, 8983, 8997, 9026, 9079, 9140, 9165, 9206,
  9276, 9378, 9449, 9523, 9647, 9703, 9749, 9790, 9929, 10056, 10216,
  10307, 10411, 10487, 10557, 10695, 10770, 10854, 11095, 11265, 11517, 11589,
  11686, 11825, 11940, 12004, 12047, 12113, 12179, 12233, 12532, 12586, 12708,
  12793, 12877, 12954, 13030, 13105, 13177, 13279, 13396, 13486, 13538, 13561,
  13591, 13627, 13656, 13709, 13763, 13815, 13842, 13876, 13906, 13929, 13955,
  14003, 14070, 14097, 14127, 14153, 14198, 14269, 14348, 14367, 14440, 14488,
  14548, 14573, 14599, 14630, 14665, 14907, 14962, 15013, 15089, 15148, 15227,
  15314, 15355, 15369, 15451, 15470, 15542, 15570, 15640, 15684, 15781, 15869,
  15938, 16172, 16266, 16429, 16479, 16521, 16563, 16612, 16671, 16692, 16704,
  16720, 16756, 16789, 16802, 16815, 16867, 16908, 16939, 16953, 16977, 17006,
  17014, 17026, 17040, 17062, 17121, 17176, 17226, 17322, 17444, 17496, 17641,
  17698, 17744, 17826, 17913, 17993, 18073, 18219, 18279, 18359, 18475, 18544,
  18587, 18649, 18698, 18756, 18826, 18853, 18866, 19108, 19336, 19481, 19544,
  19720, 19816, 19908, 19982, 20069, 20310, 20355, 20374, 20409, 20469, 20599,
  20607, 20652, 20805, 20822, 20882, 20982, 21029, 21433, 21468, 21561, 21602,
  21661, 21720, 21909, 22045, 22166, 22225, 22323, 22362, 22433, 22477, 22529,
  22571, 22617, 22642, 22676, 22918, 22978, 23084, 23161, 23288, 23409, 23490,
  23613, 23721, 23815, 24131, 24372, 24468, 24507, 24555, 24568, 24616, 24634,
  24829, 24843, 24919, 24992, 25040, 25160, 25288, 25607, 25684, 25717, 25764,
  25821, 25866, 25901, 25925, 25941, 25978, 25998, 26011, 26030, 26055, 26118,
  26133, 26145, 26159, 26175, 26182, 26195, 26205, 26238, 26258, 26316, 26340,
  26581, 26725, 26834, 26874, 26995, 27065, 27178, 27238, 27365, 27607, 27669,
  27694,
  27774, 27800, 27841, 27930, 27985, 28057, 28091, 28132, 28189, 28270, 28545,
  28653, 28711, 28770, 28886, 28966, 29139, 29241, 29356, 29415, 29490, 29576,
  29659, 29776, 29842, 29910, 30029, 30056, 30100, 30129, 30175, 30316, 30376,
  30441, 30551, 30666, 30784, 30843, 30948, 31045, 31286, 31315, 31534, 31607,
  31742,
  31817, 31853, 31984, 32009, 32112, 32162, 32210, 32264
};

/* Sintel */
int sintel_changes[] = {
  752, 1018, 1036, 1056, 1078, 1100, 1169, 1319, 1339, 1370,
  1425, 1455, 1494, 1552, 1572, 1637, 1663, 1777, 1955, 2060,
  2125, 2429, 2624, 2780, 2835, 2881, 2955, 3032, 3144, 3217,
  3315, 3384, 3740, 3890, 4234, 4261, 4322, 4368, 4425, 4481,
  4555, 4605, 4671, 4714, 4743, 4875, 4920, 5082, 5158, 5267,
  5379, 5956, 6021, 6071, 6112, 6139, 6221, 6318, 6374, 6519,
  6558, 6615, 6691, 6803, 6900, 6944, 7134, 7266, 7351, 7414,
  7467, 7503, 7559, 7573, 7656, 7733, 7876, 7929, 7971, 7985,
  8047, 8099, 8144, 8215, 8394, 8435, 8480, 9133, 9190, 9525,
  9962,
};

/* Breathe Out video, http://media.xiph.org/video/misc/ */
int breatheout_changes[] = {
  143, 263, 334, 426, 462, 563, 583, 618, 655, 707,
  818, 823, 858, 913, 956, 977, 999, 1073, 1124, 1144,
  1166, 1187, 1206, 1227, 1240, 1264, 1289, 1312, 1477, 1535,
  1646, 1692, 1739, 1757, 1798, 1855, 1974, 2048, 2129, 2212,
  2369, 2412, 2463, 2578, 2649, 2699, 2778, 2857, 2923, 3014,
  3107, 3246, 3321, 3350, 3459, 3498, 3541, 3567, 3613, 3636,
  3673, 3709, 3747, 3834, 3862, 3902, 3922, 4022, 4117, 4262,
  4303, 4357, 4556, 4578, 4617, 4716, 4792, 4873, 4895, 4917,
  4932, 4972, 5015, 5034, 5058, 5090, 5162, 5180, 5202, 5222,
  5239, 5258, 5281, 5298, 5397, 5430,
  485, 507, 534, 665, 685, 755, 1023, 1379, 1441, 1503,
  1584, 1621, 1903, 2081, 2281, 2511, 2958, 3071, 3185, 3214,
  3271, 3424, 3479, 3588, 3879, 3979, 4043, 4062, 4143, 4207,
  4237, 4336, 4461, 4476, 4533, 4647, 4815, 4853, 4949, 5075,
  5142, 5316, 5376,
  3514, 3952, 4384, 5337
};

#define changes showreel_changes

static gboolean
is_shot_change (int frame_number)
{
  int i;
  for (i = 0; i < sizeof (changes) / sizeof (changes[0]); i++) {
    if (changes[i] == frame_number)
      return TRUE;
  }
  return FALSE;
}
#endif
