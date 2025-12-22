/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim@fluendo.com>
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
 * SECTION:element-simplevideomarkdetect
 * @title: simplevideomarkdetect
 * @see_also: #GstVideoMark
 *
 * This plugin detects #GstSimpleVideoMarkDetect:pattern-count squares in the bottom left
 * corner of the video frames. The squares have a width and height of
 * respectively #GstSimpleVideoMarkDetect:pattern-width and #GstSimpleVideoMarkDetect:pattern-height.
 * Even squares must be black and odd squares must be white.
 *
 * When the pattern has been found, #GstSimpleVideoMarkDetect:pattern-data-count squares
 * after the pattern squares are read as a bitarray. White squares represent a 1
 * bit and black squares a 0 bit. The bitarray will will included in the element
 * message that is posted (see below).
 *
 * After the pattern has been found and the data pattern has been read, an
 * element message called `GstSimpleVideoMarkDetect` will
 * be posted on the bus. If the pattern is no longer found in the frame, the
 * same element message is posted with the have-pattern field set to %FALSE.
 * The message is only posted if the #GstSimpleVideoMarkDetect:message property is %TRUE.
 *
 * The message's structure contains these fields:
 *
 * * #gboolean`have-pattern`: if the pattern was found. This field will be set to %TRUE for as long as
 *   the pattern was found in the frame and set to FALSE for the first frame
 *   that does not contain the pattern anymore.
 *
 * * #GstClockTime `timestamp`: the timestamp of the buffer that triggered the message.
 *
 * * #GstClockTime `stream-time`: the stream time of the buffer.
 *
 * * #GstClockTime `running-time`: the running_time of the buffer.
 *
 * * #GstClockTime `duration`: the duration of the buffer.
 *
 * * #guint64 `data`: the data-pattern found after the pattern or 0 when have-signal is %FALSE.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 videotestsrc ! simplevideomarkdetect ! videoconvert ! ximagesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstsimplevideomarkdetect.h"

GST_DEBUG_CATEGORY_STATIC (gst_video_detect_debug_category);
#define GST_CAT_DEFAULT gst_video_detect_debug_category

/* prototypes */


static void gst_video_detect_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_video_detect_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_video_detect_dispose (GObject * object);
static void gst_video_detect_finalize (GObject * object);

static gboolean gst_video_detect_start (GstBaseTransform * trans);
static gboolean gst_video_detect_stop (GstBaseTransform * trans);
static gboolean gst_video_detect_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_video_detect_transform_frame_ip (GstVideoFilter *
    filter, GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_MESSAGE,
  PROP_PATTERN_WIDTH,
  PROP_PATTERN_HEIGHT,
  PROP_PATTERN_COUNT,
  PROP_PATTERN_DATA_COUNT,
  PROP_PATTERN_CENTER,
  PROP_PATTERN_SENSITIVITY,
  PROP_LEFT_OFFSET,
  PROP_BOTTOM_OFFSET
};

#define DEFAULT_MESSAGE              TRUE
#define DEFAULT_PATTERN_WIDTH        4
#define DEFAULT_PATTERN_HEIGHT       16
#define DEFAULT_PATTERN_COUNT        4
#define DEFAULT_PATTERN_DATA_COUNT   5
#define DEFAULT_PATTERN_CENTER       0.5
#define DEFAULT_PATTERN_SENSITIVITY  0.3
#define DEFAULT_LEFT_OFFSET          0
#define DEFAULT_BOTTOM_OFFSET        0

/* pad templates */

#define VIDEO_CAPS \
    GST_VIDEO_CAPS_MAKE( \
        "{ I420, YV12, Y41B, Y42B, Y444, YUY2, UYVY, AYUV, YVYU }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstSimpleVideoMarkDetect, gst_video_detect,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_video_detect_debug_category,
        "simplevideomarkdetect", 0,
        "debug category for simplevideomarkdetect element"));
GST_ELEMENT_REGISTER_DEFINE (simplevideomarkdetect,
    "simplevideomarkdetect", GST_RANK_NONE, GST_TYPE_SIMPLE_VIDEO_MARK_DETECT);

static void
gst_video_detect_class_init (GstSimpleVideoMarkDetectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Video detecter", "Filter/Effect/Video",
      "Detect patterns in a video signal", "Wim Taymans <wim@fluendo.com>");

  gobject_class->set_property = gst_video_detect_set_property;
  gobject_class->get_property = gst_video_detect_get_property;
  gobject_class->dispose = gst_video_detect_dispose;
  gobject_class->finalize = gst_video_detect_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_video_detect_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_video_detect_stop);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_video_detect_set_info);
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_video_detect_transform_frame_ip);

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MESSAGE,
      g_param_spec_boolean ("message", "Message",
          "Post detected data as bus messages",
          DEFAULT_MESSAGE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_WIDTH,
      g_param_spec_int ("pattern-width", "Pattern width",
          "The width of the pattern markers", 1, G_MAXINT,
          DEFAULT_PATTERN_WIDTH,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_HEIGHT,
      g_param_spec_int ("pattern-height", "Pattern height",
          "The height of the pattern markers", 1, G_MAXINT,
          DEFAULT_PATTERN_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_COUNT,
      g_param_spec_int ("pattern-count", "Pattern count",
          "The number of pattern markers", 0, G_MAXINT,
          DEFAULT_PATTERN_COUNT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_DATA_COUNT,
      g_param_spec_int ("pattern-data-count", "Pattern data count",
          "The number of extra data pattern markers", 0, G_MAXINT,
          DEFAULT_PATTERN_DATA_COUNT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_CENTER,
      g_param_spec_double ("pattern-center", "Pattern center",
          "The center of the black/white separation (0.0 = lowest, 1.0 highest)",
          0.0, 1.0, DEFAULT_PATTERN_CENTER,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATTERN_SENSITIVITY,
      g_param_spec_double ("pattern-sensitivity", "Pattern sensitivity",
          "The sensitivity around the center for detecting the markers "
          "(0.0 = lowest, 1.0 highest)", 0.0, 1.0, DEFAULT_PATTERN_SENSITIVITY,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LEFT_OFFSET,
      g_param_spec_int ("left-offset", "Left Offset",
          "The offset from the left border where the pattern starts", 0,
          G_MAXINT, DEFAULT_LEFT_OFFSET,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BOTTOM_OFFSET,
      g_param_spec_int ("bottom-offset", "Bottom Offset",
          "The offset from the bottom border where the pattern starts", 0,
          G_MAXINT, DEFAULT_BOTTOM_OFFSET,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void
gst_video_detect_init (GstSimpleVideoMarkDetect * simplevideomarkdetect)
{
}

void
gst_video_detect_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (object);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "set_property");

  switch (property_id) {
    case PROP_MESSAGE:
      simplevideomarkdetect->message = g_value_get_boolean (value);
      break;
    case PROP_PATTERN_WIDTH:
      simplevideomarkdetect->pattern_width = g_value_get_int (value);
      break;
    case PROP_PATTERN_HEIGHT:
      simplevideomarkdetect->pattern_height = g_value_get_int (value);
      break;
    case PROP_PATTERN_COUNT:
      simplevideomarkdetect->pattern_count = g_value_get_int (value);
      break;
    case PROP_PATTERN_DATA_COUNT:
      simplevideomarkdetect->pattern_data_count = g_value_get_int (value);
      break;
    case PROP_PATTERN_CENTER:
      simplevideomarkdetect->pattern_center = g_value_get_double (value);
      break;
    case PROP_PATTERN_SENSITIVITY:
      simplevideomarkdetect->pattern_sensitivity = g_value_get_double (value);
      break;
    case PROP_LEFT_OFFSET:
      simplevideomarkdetect->left_offset = g_value_get_int (value);
      break;
    case PROP_BOTTOM_OFFSET:
      simplevideomarkdetect->bottom_offset = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_video_detect_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (object);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "get_property");

  switch (property_id) {
    case PROP_MESSAGE:
      g_value_set_boolean (value, simplevideomarkdetect->message);
      break;
    case PROP_PATTERN_WIDTH:
      g_value_set_int (value, simplevideomarkdetect->pattern_width);
      break;
    case PROP_PATTERN_HEIGHT:
      g_value_set_int (value, simplevideomarkdetect->pattern_height);
      break;
    case PROP_PATTERN_COUNT:
      g_value_set_int (value, simplevideomarkdetect->pattern_count);
      break;
    case PROP_PATTERN_DATA_COUNT:
      g_value_set_int (value, simplevideomarkdetect->pattern_data_count);
      break;
    case PROP_PATTERN_CENTER:
      g_value_set_double (value, simplevideomarkdetect->pattern_center);
      break;
    case PROP_PATTERN_SENSITIVITY:
      g_value_set_double (value, simplevideomarkdetect->pattern_sensitivity);
      break;
    case PROP_LEFT_OFFSET:
      g_value_set_int (value, simplevideomarkdetect->left_offset);
      break;
    case PROP_BOTTOM_OFFSET:
      g_value_set_int (value, simplevideomarkdetect->bottom_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_video_detect_dispose (GObject * object)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (object);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_video_detect_parent_class)->dispose (object);
}

void
gst_video_detect_finalize (GObject * object)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (object);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_video_detect_parent_class)->finalize (object);
}

static gboolean
gst_video_detect_start (GstBaseTransform * trans)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (trans);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "start");

  return TRUE;
}

static gboolean
gst_video_detect_stop (GstBaseTransform * trans)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (trans);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "stop");

  return TRUE;
}

static gboolean
gst_video_detect_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (filter);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "set_info");

  return TRUE;
}

static void
gst_video_detect_post_message (GstSimpleVideoMarkDetect * simplevideomarkdetect,
    GstBuffer * buffer, guint64 data)
{
  GstBaseTransform *trans;
  GstMessage *m;
  guint64 duration, timestamp, running_time, stream_time;

  trans = GST_BASE_TRANSFORM_CAST (simplevideomarkdetect);

  /* get timestamps */
  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      timestamp);
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      timestamp);

  /* post message */
  m = gst_message_new_element (GST_OBJECT_CAST (simplevideomarkdetect),
      gst_structure_new ("GstSimpleVideoMarkDetect",
          "have-pattern", G_TYPE_BOOLEAN, simplevideomarkdetect->in_pattern,
          "timestamp", G_TYPE_UINT64, timestamp,
          "stream-time", G_TYPE_UINT64, stream_time,
          "running-time", G_TYPE_UINT64, running_time,
          "duration", G_TYPE_UINT64, duration,
          "data", G_TYPE_UINT64, data, NULL));
  gst_element_post_message (GST_ELEMENT_CAST (simplevideomarkdetect), m);
}

static gdouble
gst_video_detect_calc_brightness (GstSimpleVideoMarkDetect *
    simplevideomarkdetect, guint8 * data, gint width, gint height,
    gint row_stride, gint pixel_stride)
{
  gint i, j;
  guint64 sum;

  sum = 0;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      sum += data[pixel_stride * j];
    }
    data += row_stride;
  }
  return sum / (255.0 * width * height);
}

static gint
calculate_pw (gint pw, gint x, gint width)
{
  if (x < 0)
    pw += x;
  else if ((x + pw) > width)
    pw = width - x;

  return pw;
}

static void
gst_video_detect_yuv (GstSimpleVideoMarkDetect * simplevideomarkdetect,
    GstVideoFrame * frame)
{
  gdouble brightness;
  gint i, pw, ph, row_stride, pixel_stride;
  gint width, height, offset_calc, x, y;
  guint8 *d;
  guint64 pattern_data;
  gint total_pattern;

  width = frame->info.width;
  height = frame->info.height;

  pw = simplevideomarkdetect->pattern_width;
  ph = simplevideomarkdetect->pattern_height;
  row_stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0);
  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 0);

  d = GST_VIDEO_FRAME_COMP_DATA (frame, 0);
  /* move to start of bottom left, adjust for offsets */
  offset_calc =
      row_stride * (height - ph - simplevideomarkdetect->bottom_offset) +
      pixel_stride * simplevideomarkdetect->left_offset;
  x = simplevideomarkdetect->left_offset;
  y = height - ph - simplevideomarkdetect->bottom_offset;

  total_pattern =
      simplevideomarkdetect->pattern_count +
      simplevideomarkdetect->pattern_data_count;
  /* If x and y offset values are outside the video, no need to analyze */
  if ((x + (pw * total_pattern)) < 0 || x > width || (y + height) < 0
      || y > height) {
    GST_ERROR_OBJECT (simplevideomarkdetect,
        "simplevideomarkdetect pattern is outside the video. Not Analyzing.");
    return;
  }

  /* Offset calculation less than 0, then reset to 0 */
  if (offset_calc < 0)
    offset_calc = 0;
  /* Y position of mark is negative or pattern exceeds the video height,
     then recalculate pattern height for partial display */
  if (y < 0)
    ph += y;
  else if ((y + ph) > height)
    ph = height - y;
  /* If pattern height is less than 0, need not analyze anything */
  if (ph < 0)
    return;

  /* move to start of bottom left */
  d += offset_calc;

  /* analyze the bottom left pixels */
  for (i = 0; i < simplevideomarkdetect->pattern_count; i++) {
    gint draw_pw;
    /* calc brightness of width * height box */
    brightness =
        gst_video_detect_calc_brightness (simplevideomarkdetect, d, pw, ph,
        row_stride, pixel_stride);

    GST_DEBUG_OBJECT (simplevideomarkdetect, "brightness %f", brightness);

    if (i & 1) {
      /* odd pixels must be white, all pixels darker than the center +
       * sensitivity are considered wrong. */
      if (brightness <
          (simplevideomarkdetect->pattern_center +
              simplevideomarkdetect->pattern_sensitivity))
        goto no_pattern;
    } else {
      /* even pixels must be black, pixels lighter than the center - sensitivity
       * are considered wrong. */
      if (brightness >
          (simplevideomarkdetect->pattern_center -
              simplevideomarkdetect->pattern_sensitivity))
        goto no_pattern;
    }

    /* X position of mark is negative or pattern exceeds the video width,
       then recalculate pattern width for partial display */
    draw_pw = calculate_pw (pw, x, width);
    /* If pattern width is less than 0, continue with the next pattern */
    if (draw_pw < 0)
      continue;

    /* move to i-th pattern */
    d += pixel_stride * draw_pw;
    x += draw_pw;

    if ((x + (pw * (total_pattern - i - 1))) < 0 || x >= width)
      break;
  }
  GST_DEBUG_OBJECT (simplevideomarkdetect, "found pattern");

  pattern_data = 0;

  /* get the data of the pattern */
  for (i = 0; i < simplevideomarkdetect->pattern_data_count; i++) {
    gint draw_pw;
    /* calc brightness of width * height box */
    brightness =
        gst_video_detect_calc_brightness (simplevideomarkdetect, d, pw, ph,
        row_stride, pixel_stride);
    /* update pattern, we just use the center to decide between black and white. */
    pattern_data <<= 1;
    if (brightness > simplevideomarkdetect->pattern_center)
      pattern_data |= 1;

    /* X position of mark is negative or pattern exceeds the video width,
       then recalculate pattern width for partial display */
    draw_pw = calculate_pw (pw, x, width);
    /* If pattern width is less than 0, continue with the next pattern */
    if (draw_pw < 0)
      continue;

    /* move to i-th pattern data */
    d += pixel_stride * draw_pw;
    x += draw_pw;

    if ((x + (pw * (simplevideomarkdetect->pattern_data_count - i - 1))) < 0
        || x >= width)
      break;
  }

  GST_DEBUG_OBJECT (simplevideomarkdetect, "have data %" G_GUINT64_FORMAT,
      pattern_data);

  simplevideomarkdetect->in_pattern = TRUE;
  gst_video_detect_post_message (simplevideomarkdetect, frame->buffer,
      pattern_data);

  return;

no_pattern:
  {
    GST_DEBUG_OBJECT (simplevideomarkdetect, "no pattern found");
    if (simplevideomarkdetect->in_pattern) {
      simplevideomarkdetect->in_pattern = FALSE;
      gst_video_detect_post_message (simplevideomarkdetect, frame->buffer, 0);
    }
    return;
  }
}

static GstFlowReturn
gst_video_detect_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstSimpleVideoMarkDetect *simplevideomarkdetect =
      GST_SIMPLE_VIDEO_MARK_DETECT (filter);

  GST_DEBUG_OBJECT (simplevideomarkdetect, "transform_frame_ip");

  gst_video_detect_yuv (simplevideomarkdetect, frame);

  return GST_FLOW_OK;
}
