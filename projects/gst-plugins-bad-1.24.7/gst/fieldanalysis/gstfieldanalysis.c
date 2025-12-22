/*
 * GStreamer
 * Copyright (C) 2011 Robert Swain <robert.swain@collabora.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * SECTION:element-fieldanalysis
 * @title: fieldanalysis
 *
 * Analyse fields from video buffers to identify whether the buffers are
 * progressive/telecined/interlaced and, if telecined, the telecine pattern
 * used.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v uridecodebin uri=/path/to/foo.bar ! fieldanalysis ! deinterlace ! videoconvert ! autovideosink
 * ]| This pipeline will analyse a video stream with default metrics and thresholds and output progressive frames.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>
#include <stdlib.h>             /* for abs() */

#include "gstfieldanalysis.h"
#include "gstfieldanalysisorc.h"

GST_DEBUG_CATEGORY_STATIC (gst_field_analysis_debug);
#define GST_CAT_DEFAULT gst_field_analysis_debug

#define DEFAULT_FIELD_METRIC GST_FIELDANALYSIS_SSD
#define DEFAULT_FRAME_METRIC GST_FIELDANALYSIS_5_TAP
#define DEFAULT_NOISE_FLOOR 16
#define DEFAULT_FIELD_THRESH 0.08f
#define DEFAULT_FRAME_THRESH 0.002f
#define DEFAULT_COMB_METHOD METHOD_5_TAP
#define DEFAULT_SPATIAL_THRESH 9
#define DEFAULT_BLOCK_WIDTH 16
#define DEFAULT_BLOCK_HEIGHT 16
#define DEFAULT_BLOCK_THRESH 80
#define DEFAULT_IGNORED_LINES 2

enum
{
  PROP_0,
  PROP_FIELD_METRIC,
  PROP_FRAME_METRIC,
  PROP_NOISE_FLOOR,
  PROP_FIELD_THRESH,
  PROP_FRAME_THRESH,
  PROP_COMB_METHOD,
  PROP_SPATIAL_THRESH,
  PROP_BLOCK_WIDTH,
  PROP_BLOCK_HEIGHT,
  PROP_BLOCK_THRESH,
  PROP_IGNORED_LINES
};

static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{YUY2,UYVY,Y42B,I420,YV12}")));

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{YUY2,UYVY,Y42B,I420,YV12}")));

G_DEFINE_TYPE (GstFieldAnalysis, gst_field_analysis, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (fieldanalysis, "fieldanalysis",
    GST_RANK_NONE, GST_TYPE_FIELDANALYSIS,
    GST_DEBUG_CATEGORY_INIT (gst_field_analysis_debug, "fieldanalysis", 0,
        "Video field analysis");
    );

#define parent_class gst_field_analysis_parent_class

static void gst_field_analysis_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_field_analysis_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_field_analysis_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_field_analysis_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static GstStateChangeReturn gst_field_analysis_change_state (GstElement *
    element, GstStateChange transition);
static void gst_field_analysis_finalize (GObject * self);

static GQueue *gst_field_analysis_flush_frames (GstFieldAnalysis * filter);

typedef enum
{
  GST_FIELDANALYSIS_SAD,
  GST_FIELDANALYSIS_SSD,
  GST_FIELDANALYSIS_3_TAP
} GstFieldAnalysisFieldMetric;

#define GST_TYPE_FIELDANALYSIS_FIELD_METRIC (gst_fieldanalysis_field_metric_get_type())
static GType
gst_fieldanalysis_field_metric_get_type (void)
{
  static GType fieldanalysis_field_metric_type = 0;

  if (!fieldanalysis_field_metric_type) {
    static const GEnumValue fieldanalysis_field_metrics[] = {
      {GST_FIELDANALYSIS_SAD, "Sum of Absolute Differences", "sad"},
      {GST_FIELDANALYSIS_SSD, "Sum of Squared Differences", "ssd"},
      {GST_FIELDANALYSIS_3_TAP, "Difference of 3-tap [1,4,1] Horizontal Filter",
          "3-tap"},
      {0, NULL, NULL},
    };

    fieldanalysis_field_metric_type =
        g_enum_register_static ("GstFieldAnalysisFieldMetric",
        fieldanalysis_field_metrics);
  }

  return fieldanalysis_field_metric_type;
}

typedef enum
{
  GST_FIELDANALYSIS_5_TAP,
  GST_FIELDANALYSIS_WINDOWED_COMB
} GstFieldAnalysisFrameMetric;

#define GST_TYPE_FIELDANALYSIS_FRAME_METRIC (gst_fieldanalysis_frame_metric_get_type())
static GType
gst_fieldanalysis_frame_metric_get_type (void)
{
  static GType fieldanalysis_frame_metric_type = 0;

  if (!fieldanalysis_frame_metric_type) {
    static const GEnumValue fieldanalyis_frame_metrics[] = {
      {GST_FIELDANALYSIS_5_TAP, "5-tap [1,-3,4,-3,1] Vertical Filter", "5-tap"},
      {GST_FIELDANALYSIS_WINDOWED_COMB,
            "Windowed Comb Detection (not optimised)",
          "windowed-comb"},
      {0, NULL, NULL},
    };

    fieldanalysis_frame_metric_type =
        g_enum_register_static ("GstFieldAnalysisFrameMetric",
        fieldanalyis_frame_metrics);
  }

  return fieldanalysis_frame_metric_type;
}

#define GST_TYPE_FIELDANALYSIS_COMB_METHOD (gst_fieldanalysis_comb_method_get_type())
static GType
gst_fieldanalysis_comb_method_get_type (void)
{
  static GType fieldanalysis_comb_method_type = 0;

  if (!fieldanalysis_comb_method_type) {
    static const GEnumValue fieldanalyis_comb_methods[] = {
      {METHOD_32DETECT,
            "Difference to above sample in same field small and difference to sample in other field large",
          "32-detect"},
      {METHOD_IS_COMBED,
            "Differences between current sample and the above/below samples in other field multiplied together, larger than squared spatial threshold (from Tritical's isCombed)",
          "isCombed"},
      {METHOD_5_TAP,
            "5-tap [1,-3,4,-3,1] vertical filter result is larger than spatial threshold*6",
          "5-tap"},
      {0, NULL, NULL},
    };

    fieldanalysis_comb_method_type =
        g_enum_register_static ("FieldAnalysisCombMethod",
        fieldanalyis_comb_methods);
  }

  return fieldanalysis_comb_method_type;
}

static void
gst_field_analysis_class_init (GstFieldAnalysisClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_field_analysis_set_property;
  gobject_class->get_property = gst_field_analysis_get_property;
  gobject_class->finalize = gst_field_analysis_finalize;

  g_object_class_install_property (gobject_class, PROP_FIELD_METRIC,
      g_param_spec_enum ("field-metric", "Field Metric",
          "Metric to be used for comparing same parity fields to decide if they are a repeated field for telecine",
          GST_TYPE_FIELDANALYSIS_FIELD_METRIC, DEFAULT_FIELD_METRIC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAME_METRIC,
      g_param_spec_enum ("frame-metric", "Frame Metric",
          "Metric to be used for comparing opposite parity fields to decide if they are a progressive frame",
          GST_TYPE_FIELDANALYSIS_FRAME_METRIC, DEFAULT_FRAME_METRIC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_NOISE_FLOOR,
      g_param_spec_uint ("noise-floor", "Noise Floor",
          "Noise floor for appropriate metrics (per-pixel metric values with a score less than this will be ignored)",
          0, G_MAXUINT32,
          DEFAULT_NOISE_FLOOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FIELD_THRESH,
      g_param_spec_float ("field-threshold", "Field Threshold",
          "Threshold for field metric decisions", 0.0f, G_MAXFLOAT,
          DEFAULT_FIELD_THRESH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAME_THRESH,
      g_param_spec_float ("frame-threshold", "Frame Threshold",
          "Threshold for frame metric decisions", 0.0f, G_MAXFLOAT,
          DEFAULT_FRAME_THRESH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_COMB_METHOD,
      g_param_spec_enum ("comb-method", "Comb-detection Method",
          "Metric to be used for identifying comb artifacts if using windowed comb detection",
          GST_TYPE_FIELDANALYSIS_COMB_METHOD, DEFAULT_COMB_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPATIAL_THRESH,
      g_param_spec_int64 ("spatial-threshold", "Spatial Combing Threshold",
          "Threshold for combing metric decisions", 0, G_MAXINT64,
          DEFAULT_SPATIAL_THRESH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BLOCK_WIDTH,
      g_param_spec_uint64 ("block-width", "Block width",
          "Block width for windowed comb detection", 1, G_MAXUINT64,
          DEFAULT_BLOCK_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BLOCK_HEIGHT,
      g_param_spec_uint64 ("block-height", "Block height",
          "Block height for windowed comb detection", 0, G_MAXUINT64,
          DEFAULT_BLOCK_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BLOCK_THRESH,
      g_param_spec_uint64 ("block-threshold", "Block threshold",
          "Block threshold for windowed comb detection", 0, G_MAXUINT64,
          DEFAULT_BLOCK_THRESH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IGNORED_LINES,
      g_param_spec_uint64 ("ignored-lines", "Ignored lines",
          "Ignore this many lines from the top and bottom for windowed comb detection",
          2, G_MAXUINT64, DEFAULT_IGNORED_LINES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_field_analysis_change_state);

  gst_element_class_set_static_metadata (gstelement_class,
      "Video field analysis",
      "Filter/Analysis/Video",
      "Analyse fields from video frames to identify if they are progressive/telecined/interlaced",
      "Robert Swain <robert.swain@collabora.co.uk>");

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_FIELDANALYSIS_COMB_METHOD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_FIELDANALYSIS_FIELD_METRIC, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_FIELDANALYSIS_FRAME_METRIC, 0);
}

static gfloat same_parity_sad (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2]);
static gfloat same_parity_ssd (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2]);
static gfloat same_parity_3_tap (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2]);
static gfloat opposite_parity_5_tap (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2]);
static guint64 block_score_for_row_32detect (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1);
static guint64 block_score_for_row_iscombed (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1);
static guint64 block_score_for_row_5_tap (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1);
static gfloat opposite_parity_windowed_comb (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2]);

static void
gst_field_analysis_clear_frames (GstFieldAnalysis * filter)
{
  GST_DEBUG_OBJECT (filter, "Clearing %d frames", filter->nframes);
  while (filter->nframes) {
    gst_video_frame_unmap (&filter->frames[filter->nframes - 1].frame);
    filter->nframes--;
  }
}

static void
gst_field_analysis_reset (GstFieldAnalysis * filter)
{
  gst_field_analysis_clear_frames (filter);
  GST_DEBUG_OBJECT (filter, "Resetting context");
  memset (filter->frames, 0, 2 * sizeof (FieldAnalysisHistory));
  filter->is_telecine = FALSE;
  filter->first_buffer = TRUE;
  gst_video_info_init (&filter->vinfo);
  g_free (filter->comb_mask);
  filter->comb_mask = NULL;
  g_free (filter->block_scores);
  filter->block_scores = NULL;
}

static void
gst_field_analysis_init (GstFieldAnalysis * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_field_analysis_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_field_analysis_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->nframes = 0;
  gst_field_analysis_reset (filter);
  filter->same_field = &same_parity_ssd;
  filter->field_thresh = DEFAULT_FIELD_THRESH;
  filter->same_frame = &opposite_parity_5_tap;
  filter->frame_thresh = DEFAULT_FRAME_THRESH;
  filter->noise_floor = DEFAULT_NOISE_FLOOR;
  filter->block_score_for_row = &block_score_for_row_5_tap;
  filter->spatial_thresh = DEFAULT_SPATIAL_THRESH;
  filter->block_width = DEFAULT_BLOCK_WIDTH;
  filter->block_height = DEFAULT_BLOCK_HEIGHT;
  filter->block_thresh = DEFAULT_BLOCK_THRESH;
  filter->ignored_lines = DEFAULT_IGNORED_LINES;
}

static void
gst_field_analysis_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFieldAnalysis *filter = GST_FIELDANALYSIS (object);

  switch (prop_id) {
    case PROP_FIELD_METRIC:
      switch (g_value_get_enum (value)) {
        case GST_FIELDANALYSIS_SAD:
          filter->same_field = &same_parity_sad;
          break;
        case GST_FIELDANALYSIS_SSD:
          filter->same_field = &same_parity_ssd;
          break;
        case GST_FIELDANALYSIS_3_TAP:
          filter->same_field = &same_parity_3_tap;
          break;
        default:
          break;
      }
      break;
    case PROP_FRAME_METRIC:
      switch (g_value_get_enum (value)) {
        case GST_FIELDANALYSIS_5_TAP:
          filter->same_frame = &opposite_parity_5_tap;
          break;
        case GST_FIELDANALYSIS_WINDOWED_COMB:
          filter->same_frame = &opposite_parity_windowed_comb;
          break;
        default:
          break;
      }
      break;
    case PROP_NOISE_FLOOR:
      filter->noise_floor = g_value_get_uint (value);
      break;
    case PROP_FIELD_THRESH:
      filter->field_thresh = g_value_get_float (value);
      break;
    case PROP_FRAME_THRESH:
      filter->frame_thresh = g_value_get_float (value);
      break;
    case PROP_COMB_METHOD:
      switch (g_value_get_enum (value)) {
        case METHOD_32DETECT:
          filter->block_score_for_row = &block_score_for_row_32detect;
          break;
        case METHOD_IS_COMBED:
          filter->block_score_for_row = &block_score_for_row_iscombed;
          break;
        case METHOD_5_TAP:
          filter->block_score_for_row = &block_score_for_row_5_tap;
          break;
        default:
          break;
      }
      break;
    case PROP_SPATIAL_THRESH:
      filter->spatial_thresh = g_value_get_int64 (value);
      break;
    case PROP_BLOCK_WIDTH:
      filter->block_width = g_value_get_uint64 (value);
      if (GST_VIDEO_FRAME_WIDTH (&filter->frames[0].frame)) {
        const gint frame_width =
            GST_VIDEO_FRAME_WIDTH (&filter->frames[0].frame);
        if (filter->block_scores) {
          gsize nbytes = (frame_width / filter->block_width) * sizeof (guint);
          filter->block_scores = g_realloc (filter->block_scores, nbytes);
          memset (filter->block_scores, 0, nbytes);
        } else {
          filter->block_scores =
              g_malloc0 ((frame_width / filter->block_width) * sizeof (guint));
        }
      }
      break;
    case PROP_BLOCK_HEIGHT:
      filter->block_height = g_value_get_uint64 (value);
      break;
    case PROP_BLOCK_THRESH:
      filter->block_thresh = g_value_get_uint64 (value);
      break;
    case PROP_IGNORED_LINES:
      filter->ignored_lines = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_field_analysis_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFieldAnalysis *filter = GST_FIELDANALYSIS (object);

  switch (prop_id) {
    case PROP_FIELD_METRIC:
    {
      GstFieldAnalysisFieldMetric metric = DEFAULT_FIELD_METRIC;
      if (filter->same_field == &same_parity_sad) {
        metric = GST_FIELDANALYSIS_SAD;
      } else if (filter->same_field == &same_parity_ssd) {
        metric = GST_FIELDANALYSIS_SSD;
      } else if (filter->same_field == &same_parity_3_tap) {
        metric = GST_FIELDANALYSIS_3_TAP;
      }
      g_value_set_enum (value, metric);
      break;
    }
    case PROP_FRAME_METRIC:
    {
      GstFieldAnalysisFrameMetric metric = DEFAULT_FRAME_METRIC;
      if (filter->same_frame == &opposite_parity_5_tap) {
        metric = GST_FIELDANALYSIS_5_TAP;
      } else if (filter->same_frame == &opposite_parity_windowed_comb) {
        metric = GST_FIELDANALYSIS_WINDOWED_COMB;
      }
      g_value_set_enum (value, metric);
      break;
    }
    case PROP_NOISE_FLOOR:
      g_value_set_uint (value, filter->noise_floor);
      break;
    case PROP_FIELD_THRESH:
      g_value_set_float (value, filter->field_thresh);
      break;
    case PROP_FRAME_THRESH:
      g_value_set_float (value, filter->frame_thresh);
      break;
    case PROP_COMB_METHOD:
    {
      FieldAnalysisCombMethod method = DEFAULT_COMB_METHOD;
      if (filter->block_score_for_row == &block_score_for_row_32detect) {
        method = METHOD_32DETECT;
      } else if (filter->block_score_for_row == &block_score_for_row_iscombed) {
        method = METHOD_IS_COMBED;
      } else if (filter->block_score_for_row == &block_score_for_row_5_tap) {
        method = METHOD_5_TAP;
      }
      g_value_set_enum (value, method);
      break;
    }
    case PROP_SPATIAL_THRESH:
      g_value_set_int64 (value, filter->spatial_thresh);
      break;
    case PROP_BLOCK_WIDTH:
      g_value_set_uint64 (value, filter->block_width);
      break;
    case PROP_BLOCK_HEIGHT:
      g_value_set_uint64 (value, filter->block_height);
      break;
    case PROP_BLOCK_THRESH:
      g_value_set_uint64 (value, filter->block_thresh);
      break;
    case PROP_IGNORED_LINES:
      g_value_set_uint64 (value, filter->ignored_lines);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_field_analysis_update_format (GstFieldAnalysis * filter, GstCaps * caps)
{
  gint width;
  GQueue *outbufs;
  GstVideoInfo vinfo;

  if (!gst_video_info_from_caps (&vinfo, caps)) {
    GST_ERROR_OBJECT (filter, "Invalid caps: %" GST_PTR_FORMAT, caps);
    return;
  }

  /* if format is unchanged in our eyes, don't update the context */
  if ((GST_VIDEO_INFO_WIDTH (&filter->vinfo) == GST_VIDEO_INFO_WIDTH (&vinfo))
      && (GST_VIDEO_INFO_HEIGHT (&filter->vinfo) ==
          GST_VIDEO_INFO_HEIGHT (&vinfo))
      && (GST_VIDEO_INFO_COMP_OFFSET (&filter->vinfo, 0) ==
          GST_VIDEO_INFO_COMP_OFFSET (&vinfo, 0))
      && (GST_VIDEO_INFO_COMP_PSTRIDE (&filter->vinfo, 0) ==
          GST_VIDEO_INFO_COMP_PSTRIDE (&vinfo, 0))
      && (GST_VIDEO_INFO_COMP_STRIDE (&filter->vinfo, 0) ==
          GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0)))
    return;

  /* format changed - process and push buffers before updating context */

  GST_OBJECT_LOCK (filter);
  filter->flushing = TRUE;
  outbufs = gst_field_analysis_flush_frames (filter);
  GST_OBJECT_UNLOCK (filter);

  if (outbufs) {
    while (g_queue_get_length (outbufs))
      gst_pad_push (filter->srcpad, g_queue_pop_head (outbufs));
  }

  GST_OBJECT_LOCK (filter);
  filter->flushing = FALSE;

  filter->vinfo = vinfo;
  width = GST_VIDEO_INFO_WIDTH (&filter->vinfo);

  /* update allocations for metric scores */
  if (filter->comb_mask) {
    filter->comb_mask = g_realloc (filter->comb_mask, width);
  } else {
    filter->comb_mask = g_malloc (width);
  }
  if (filter->block_scores) {
    gsize nbytes = (width / filter->block_width) * sizeof (guint);
    filter->block_scores = g_realloc (filter->block_scores, nbytes);
    memset (filter->block_scores, 0, nbytes);
  } else {
    filter->block_scores =
        g_malloc0 ((width / filter->block_width) * sizeof (guint));
  }

  GST_OBJECT_UNLOCK (filter);
  return;
}

#define FIELD_ANALYSIS_TOP_BOTTOM   (1 << 0)
#define FIELD_ANALYSIS_BOTTOM_TOP   (1 << 1)
#define FIELD_ANALYSIS_TOP_MATCH    (1 << 2)
#define FIELD_ANALYSIS_BOTTOM_MATCH (1 << 3)

/* decorate removes a buffer from the internal queue, on which we have a ref,
 * then makes its metadata writable (could be the same buffer, could be a new
 * buffer, but either way we have a ref on it), decorates this buffer and
 * returns it */
static GstBuffer *
gst_field_analysis_decorate (GstFieldAnalysis * filter, gboolean tff,
    gboolean onefield, FieldAnalysisConclusion conclusion, gboolean drop)
{
  GstBuffer *buf = NULL;
  GstCaps *caps;
  GstVideoInfo srcpadvinfo, vinfo = filter->vinfo;

  /* deal with incoming buffer */
  if (conclusion > FIELD_ANALYSIS_PROGRESSIVE || filter->is_telecine == TRUE) {
    filter->is_telecine = conclusion != FIELD_ANALYSIS_INTERLACED;
    if (conclusion >= FIELD_ANALYSIS_TELECINE_PROGRESSIVE
        || filter->is_telecine == TRUE) {
      GST_VIDEO_INFO_INTERLACE_MODE (&vinfo) = GST_VIDEO_INTERLACE_MODE_MIXED;
    } else {
      GST_VIDEO_INFO_INTERLACE_MODE (&vinfo) =
          GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
    }
  } else {
    GST_VIDEO_INFO_INTERLACE_MODE (&vinfo) =
        GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  }

  caps = gst_pad_get_current_caps (filter->srcpad);
  gst_video_info_from_caps (&srcpadvinfo, caps);
  gst_caps_unref (caps);
  /* push a caps event on the src pad before pushing the buffer */
  if (!gst_video_info_is_equal (&vinfo, &srcpadvinfo)) {
    gboolean ret = TRUE;

    caps = gst_video_info_to_caps (&vinfo);
    GST_OBJECT_UNLOCK (filter);
    ret = gst_pad_set_caps (filter->srcpad, caps);
    GST_OBJECT_LOCK (filter);
    gst_caps_unref (caps);

    if (!ret) {
      GST_ERROR_OBJECT (filter, "Could not set pad caps");
      return NULL;
    }
  }

  buf = filter->frames[filter->nframes - 1].frame.buffer;
  gst_video_frame_unmap (&filter->frames[filter->nframes - 1].frame);
  filter->nframes--;

  /* set buffer flags */
  if (!tff) {
    GST_BUFFER_FLAG_UNSET (buf, GST_VIDEO_BUFFER_FLAG_TFF);
  } else if (tff == 1 || (tff == -1
          && GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_TFF))) {
    GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_TFF);
  }

  if (onefield) {
    GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_ONEFIELD);
  } else {
    GST_BUFFER_FLAG_UNSET (buf, GST_VIDEO_BUFFER_FLAG_ONEFIELD);
  }

  if (drop) {
    GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_RFF);
  } else {
    GST_BUFFER_FLAG_UNSET (buf, GST_VIDEO_BUFFER_FLAG_RFF);
  }

  if (conclusion == FIELD_ANALYSIS_PROGRESSIVE
      || conclusion == FIELD_ANALYSIS_TELECINE_PROGRESSIVE) {
    GST_BUFFER_FLAG_UNSET (buf, GST_VIDEO_BUFFER_FLAG_INTERLACED);
  } else {
    GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_INTERLACED);
  }

  GST_DEBUG_OBJECT (filter,
      "Pushing buffer with flags: %p, i %d, tff %d, 1f %d, drop %d; conc %d",
      buf, GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_INTERLACED),
      GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_TFF),
      GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_ONEFIELD),
      GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_RFF), conclusion);

  return buf;
}

/* _flush_one does not touch the buffer ref counts directly but _decorate ()
 * has some influence on ref counts - see its annotation for details */
static GstBuffer *
gst_field_analysis_flush_one (GstFieldAnalysis * filter, GQueue * outbufs)
{
  GstBuffer *buf = NULL;
  FieldAnalysis results;

  if (!filter->nframes)
    return NULL;

  GST_DEBUG_OBJECT (filter, "Flushing last frame (nframes %d)",
      filter->nframes);
  results = filter->frames[filter->nframes - 1].results;
  if (results.holding == 1 + TOP_FIELD || results.holding == 1 + BOTTOM_FIELD) {
    /* should be only one field needed */
    buf =
        gst_field_analysis_decorate (filter, results.holding == 1 + TOP_FIELD,
        TRUE, results.conclusion, FALSE);
  } else {
    /* possibility that both fields are needed */
    buf =
        gst_field_analysis_decorate (filter, -1, FALSE, results.conclusion,
        !results.holding);
  }
  if (buf) {
    filter->nframes--;
    if (outbufs)
      g_queue_push_tail (outbufs, buf);
  } else {
    GST_DEBUG_OBJECT (filter, "Error occurred during decoration");
  }
  return buf;
}

/* _flush_frames () has no direct influence on refcounts and nor does _flush_one,
 * but _decorate () does and so this function does indirectly */
static GQueue *
gst_field_analysis_flush_frames (GstFieldAnalysis * filter)
{
  GQueue *outbufs;

  if (filter->nframes < 2)
    return NULL;

  outbufs = g_queue_new ();

  while (filter->nframes)
    gst_field_analysis_flush_one (filter, outbufs);

  return outbufs;
}

static gboolean
gst_field_analysis_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstFieldAnalysis *filter = GST_FIELDANALYSIS (parent);
  gboolean forward;             /* should we forward the event? */
  gboolean ret = TRUE;

  GST_LOG_OBJECT (pad, "received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    case GST_EVENT_EOS:
    {
      /* for both SEGMENT and EOS it is safest to process and push queued
       * buffers */
      GQueue *outbufs;

      forward = TRUE;

      GST_OBJECT_LOCK (filter);
      filter->flushing = TRUE;
      outbufs = gst_field_analysis_flush_frames (filter);
      GST_OBJECT_UNLOCK (filter);

      if (outbufs) {
        while (g_queue_get_length (outbufs))
          gst_pad_push (filter->srcpad, g_queue_pop_head (outbufs));
      }

      GST_OBJECT_LOCK (filter);
      filter->flushing = FALSE;
      GST_OBJECT_UNLOCK (filter);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      /* if we have any buffers left in the queue, unref them until the queue
       * is empty */

      forward = TRUE;

      GST_OBJECT_LOCK (filter);
      gst_field_analysis_reset (filter);
      GST_OBJECT_UNLOCK (filter);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      forward = FALSE;

      gst_event_parse_caps (event, &caps);
      gst_field_analysis_update_format (filter, caps);
      ret = gst_pad_set_caps (filter->srcpad, caps);
      gst_event_unref (event);
      break;
    }
    default:
      forward = TRUE;
      break;
  }

  if (forward) {
    ret = gst_pad_event_default (pad, parent, event);
  }

  return ret;
}


static gfloat
same_parity_sad (GstFieldAnalysis * filter, FieldAnalysisFields (*history)[2])
{
  gint j;
  gfloat sum;
  guint8 *f1j, *f2j;

  const gint width = GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame);
  const gint height = GST_VIDEO_FRAME_HEIGHT (&(*history)[0].frame);
  const gint stride0x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const gint stride1x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0) << 1;
  const guint32 noise_floor = filter->noise_floor;

  f1j =
      GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
      0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame,
      0) +
      (*history)[0].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame,
      0);
  f2j =
      GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
      0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame,
      0) +
      (*history)[1].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame,
      0);

  sum = 0.0f;
  for (j = 0; j < (height >> 1); j++) {
    guint32 tempsum = 0;
    fieldanalysis_orc_same_parity_sad_planar_yuv (&tempsum, f1j, f2j,
        noise_floor, width);
    sum += tempsum;
    f1j += stride0x2;
    f2j += stride1x2;
  }

  return sum / (0.5f * width * height);
}

static gfloat
same_parity_ssd (GstFieldAnalysis * filter, FieldAnalysisFields (*history)[2])
{
  gint j;
  gfloat sum;
  guint8 *f1j, *f2j;

  const gint width = GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame);
  const gint height = GST_VIDEO_FRAME_HEIGHT (&(*history)[0].frame);
  const gint stride0x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const gint stride1x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0) << 1;
  /* noise floor needs to be squared for SSD */
  const guint32 noise_floor = filter->noise_floor * filter->noise_floor;

  f1j =
      GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
      0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame,
      0) +
      (*history)[0].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame,
      0);
  f2j =
      GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
      0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame,
      0) +
      (*history)[1].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame,
      0);

  sum = 0.0f;
  for (j = 0; j < (height >> 1); j++) {
    guint32 tempsum = 0;
    fieldanalysis_orc_same_parity_ssd_planar_yuv (&tempsum, f1j, f2j,
        noise_floor, width);
    sum += tempsum;
    f1j += stride0x2;
    f2j += stride1x2;
  }

  return sum / (0.5f * width * height); /* field is half height */
}

/* horizontal [1,4,1] diff between fields - is this a good idea or should the
 * current sample be emphasised more or less? */
static gfloat
same_parity_3_tap (GstFieldAnalysis * filter, FieldAnalysisFields (*history)[2])
{
  gint i, j;
  gfloat sum;
  guint8 *f1j, *f2j;

  const gint width = GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame);
  const gint height = GST_VIDEO_FRAME_HEIGHT (&(*history)[0].frame);
  const gint stride0x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const gint stride1x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0) << 1;
  const gint incr = GST_VIDEO_FRAME_COMP_PSTRIDE (&(*history)[0].frame, 0);
  /* noise floor needs to be *6 for [1,4,1] */
  const guint32 noise_floor = filter->noise_floor * 6;

  f1j = GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame, 0) +
      GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame, 0) +
      (*history)[0].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame,
      0);
  f2j =
      GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
      0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame,
      0) +
      (*history)[1].parity * GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame,
      0);

  sum = 0.0f;
  for (j = 0; j < (height >> 1); j++) {
    guint32 tempsum = 0;
    guint32 diff;

    /* unroll first as it is a special case */
    diff = abs (((f1j[0] << 2) + (f1j[incr] << 1))
        - ((f2j[0] << 2) + (f2j[incr] << 1)));
    if (diff > noise_floor)
      sum += diff;

    fieldanalysis_orc_same_parity_3_tap_planar_yuv (&tempsum, f1j, &f1j[incr],
        &f1j[incr << 1], f2j, &f2j[incr], &f2j[incr << 1], noise_floor,
        width - 1);
    sum += tempsum;

    /* unroll last as it is a special case */
    i = width - 1;
    diff = abs (((f1j[i - incr] << 1) + (f1j[i] << 2))
        - ((f2j[i - incr] << 1) + (f2j[i] << 2)));
    if (diff > noise_floor)
      sum += diff;

    f1j += stride0x2;
    f2j += stride1x2;
  }

  return sum / ((6.0f / 2.0f) * width * height);        /* 1 + 4 + 1 = 6; field is half height */
}

/* vertical [1,-3,4,-3,1] - same as is used in FieldDiff from TIVTC,
 * tritical's AVISynth IVTC filter */
/* 0th field's parity defines operation */
static gfloat
opposite_parity_5_tap (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2])
{
  gint j;
  gfloat sum;
  guint8 *fjm2, *fjm1, *fj, *fjp1, *fjp2;
  guint32 tempsum;

  const gint width = GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame);
  const gint height = GST_VIDEO_FRAME_HEIGHT (&(*history)[0].frame);
  const gint stride0x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const gint stride1x2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0) << 1;
  /* noise floor needs to be *6 for [1,-3,4,-3,1] */
  const guint32 noise_floor = filter->noise_floor * 6;

  sum = 0.0f;

  /* fj is line j of the combined frame made from the top field even lines of
   *   field 0 and the bottom field odd lines from field 1
   * fjp1 is one line down from fj
   * fjm2 is two lines up from fj
   * fj with j == 0 is the 0th line of the top field
   * fj with j == 1 is the 0th line of the bottom field or the 1st field of
   *   the frame*/

  /* unroll first line as it is a special case */
  if ((*history)[0].parity == TOP_FIELD) {
    fj = GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame, 0);
    fjp1 =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0);
    fjp2 = fj + stride0x2;
  } else {
    fj = GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame, 0);
    fjp1 =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0);
    fjp2 = fj + stride1x2;
  }

  tempsum = 0;
  fieldanalysis_orc_opposite_parity_5_tap_planar_yuv (&tempsum, fjp2, fjp1, fj,
      fjp1, fjp2, noise_floor, width);
  sum += tempsum;

  for (j = 1; j < (height >> 1) - 1; j++) {
    /* shift everything down a line in the field of interest (means += stridex2) */
    fjm2 = fj;
    fjm1 = fjp1;
    fj = fjp2;
    if ((*history)[0].parity == TOP_FIELD) {
      fjp1 += stride1x2;
      fjp2 += stride0x2;
    } else {
      fjp1 += stride0x2;
      fjp2 += stride1x2;
    }

    tempsum = 0;
    fieldanalysis_orc_opposite_parity_5_tap_planar_yuv (&tempsum, fjm2, fjm1,
        fj, fjp1, fjp2, noise_floor, width);
    sum += tempsum;
  }

  /* unroll the last line as it is a special case */
  /* shift everything down a line in the field of interest (means += stridex2) */
  fjm2 = fj;
  fjm1 = fjp1;
  fj = fjp2;

  tempsum = 0;
  fieldanalysis_orc_opposite_parity_5_tap_planar_yuv (&tempsum, fjm2, fjm1, fj,
      fjm1, fjm2, noise_floor, width);
  sum += tempsum;

  return sum / ((6.0f / 2.0f) * width * height);        /* 1 + 4 + 1 == 3 + 3 == 6; field is half height */
}

/* this metric was sourced from HandBrake but originally from transcode
 * the return value is the highest block score for the row of blocks */
static inline guint64
block_score_for_row_32detect (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1)
{
  guint64 i, j;
  guint8 *comb_mask = filter->comb_mask;
  guint *block_scores = filter->block_scores;
  guint64 block_score;
  guint8 *fjm2, *fjm1, *fj, *fjp1;
  const gint incr = GST_VIDEO_FRAME_COMP_PSTRIDE (&(*history)[0].frame, 0);
  const gint stridex2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const guint64 block_width = filter->block_width;
  const guint64 block_height = filter->block_height;
  const gint64 spatial_thresh = filter->spatial_thresh;
  const gint width =
      GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) -
      (GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) % block_width);

  fjm2 = base_fj - stridex2;
  fjm1 = base_fjp1 - stridex2;
  fj = base_fj;
  fjp1 = base_fjp1;

  for (j = 0; j < block_height; j++) {
    /* we have to work one result ahead of ourselves which results in some small
     * peculiarities below */
    gint diff1, diff2;

    diff1 = fj[0] - fjm1[0];
    diff2 = fj[0] - fjp1[0];
    /* change in the same direction */
    if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
        || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
      comb_mask[0] = abs (fj[0] - fjm2[0]) < 10 && abs (fj[0] - fjm1[0]) > 15;
    } else {
      comb_mask[0] = FALSE;
    }

    for (i = 1; i < width; i++) {
      const guint64 idx = i * incr;
      const guint64 res_idx = (i - 1) / block_width;

      diff1 = fj[idx] - fjm1[idx];
      diff2 = fj[idx] - fjp1[idx];
      if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
          || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
        comb_mask[i] = abs (fj[idx] - fjm2[idx]) < 10
            && abs (fj[idx] - fjm1[idx]) > 15;
      } else {
        comb_mask[i] = FALSE;
      }

      if (i == 1 && comb_mask[i - 1] && comb_mask[i]) {
        /* left edge */
        block_scores[res_idx]++;
      } else if (i == width - 1) {
        /* right edge */
        if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i])
          block_scores[res_idx]++;
        if (comb_mask[i - 1] && comb_mask[i])
          block_scores[i / block_width]++;
      } else if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i]) {
        block_scores[res_idx]++;
      }
    }
    /* advance down a line */
    fjm2 = fjm1;
    fjm1 = fj;
    fj = fjp1;
    fjp1 = fjm1 + stridex2;
  }

  block_score = 0;
  for (i = 0; i < width / block_width; i++) {
    if (block_scores[i] > block_score)
      block_score = block_scores[i];
  }

  g_free (block_scores);
  g_free (comb_mask);
  return block_score;
}

/* this metric was sourced from HandBrake but originally from
 * tritical's isCombedT Avisynth function
 * the return value is the highest block score for the row of blocks */
static inline guint64
block_score_for_row_iscombed (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1)
{
  guint64 i, j;
  guint8 *comb_mask = filter->comb_mask;
  guint *block_scores = filter->block_scores;
  guint64 block_score;
  guint8 *fjm1, *fj, *fjp1;
  const gint incr = GST_VIDEO_FRAME_COMP_PSTRIDE (&(*history)[0].frame, 0);
  const gint stridex2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const guint64 block_width = filter->block_width;
  const guint64 block_height = filter->block_height;
  const gint64 spatial_thresh = filter->spatial_thresh;
  const gint64 spatial_thresh_squared = spatial_thresh * spatial_thresh;
  const gint width =
      GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) -
      (GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) % block_width);

  fjm1 = base_fjp1 - stridex2;
  fj = base_fj;
  fjp1 = base_fjp1;

  for (j = 0; j < block_height; j++) {
    /* we have to work one result ahead of ourselves which results in some small
     * peculiarities below */
    gint diff1, diff2;

    diff1 = fj[0] - fjm1[0];
    diff2 = fj[0] - fjp1[0];
    /* change in the same direction */
    if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
        || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
      comb_mask[0] =
          (fjm1[0] - fj[0]) * (fjp1[0] - fj[0]) > spatial_thresh_squared;
    } else {
      comb_mask[0] = FALSE;
    }

    for (i = 1; i < width; i++) {
      const guint64 idx = i * incr;
      const guint64 res_idx = (i - 1) / block_width;

      diff1 = fj[idx] - fjm1[idx];
      diff2 = fj[idx] - fjp1[idx];
      if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
          || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
        comb_mask[i] =
            (fjm1[idx] - fj[idx]) * (fjp1[idx] - fj[idx]) >
            spatial_thresh_squared;
      } else {
        comb_mask[i] = FALSE;
      }

      if (i == 1 && comb_mask[i - 1] && comb_mask[i]) {
        /* left edge */
        block_scores[res_idx]++;
      } else if (i == width - 1) {
        /* right edge */
        if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i])
          block_scores[res_idx]++;
        if (comb_mask[i - 1] && comb_mask[i])
          block_scores[i / block_width]++;
      } else if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i]) {
        block_scores[res_idx]++;
      }
    }
    /* advance down a line */
    fjm1 = fj;
    fj = fjp1;
    fjp1 = fjm1 + stridex2;
  }

  block_score = 0;
  for (i = 0; i < width / block_width; i++) {
    if (block_scores[i] > block_score)
      block_score = block_scores[i];
  }

  g_free (block_scores);
  g_free (comb_mask);
  return block_score;
}

/* this metric was sourced from HandBrake but originally from
 * tritical's isCombedT Avisynth function
 * the return value is the highest block score for the row of blocks */
static inline guint64
block_score_for_row_5_tap (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2], guint8 * base_fj, guint8 * base_fjp1)
{
  guint64 i, j;
  guint8 *comb_mask = filter->comb_mask;
  guint *block_scores = filter->block_scores;
  guint64 block_score;
  guint8 *fjm2, *fjm1, *fj, *fjp1, *fjp2;
  const gint incr = GST_VIDEO_FRAME_COMP_PSTRIDE (&(*history)[0].frame, 0);
  const gint stridex2 =
      GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0) << 1;
  const guint64 block_width = filter->block_width;
  const guint64 block_height = filter->block_height;
  const gint64 spatial_thresh = filter->spatial_thresh;
  const gint64 spatial_threshx6 = 6 * spatial_thresh;
  const gint width =
      GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) -
      (GST_VIDEO_FRAME_WIDTH (&(*history)[0].frame) % block_width);


  fjm2 = base_fj - stridex2;
  fjm1 = base_fjp1 - stridex2;
  fj = base_fj;
  fjp1 = base_fjp1;
  fjp2 = fj + stridex2;

  for (j = 0; j < block_height; j++) {
    /* we have to work one result ahead of ourselves which results in some small
     * peculiarities below */
    gint diff1, diff2;

    diff1 = fj[0] - fjm1[0];
    diff2 = fj[0] - fjp1[0];
    /* change in the same direction */
    if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
        || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
      comb_mask[0] =
          abs (fjm2[0] + (fj[0] << 2) + fjp2[0] - 3 * (fjm1[0] + fjp1[0])) >
          spatial_threshx6;

      /* motion detection that needs previous and next frames
         this isn't really necessary, but acts as an optimisation if the
         additional delay isn't a problem
         if (motion_detection) {
         if (abs(fpj[idx] - fj[idx]               ) > motion_thresh &&
         abs(           fjm1[idx] - fnjm1[idx]) > motion_thresh &&
         abs(           fjp1[idx] - fnjp1[idx]) > motion_thresh)
         motion++;
         if (abs(             fj[idx]   - fnj[idx]) > motion_thresh &&
         abs(fpjm1[idx] - fjm1[idx]           ) > motion_thresh &&
         abs(fpjp1[idx] - fjp1[idx]           ) > motion_thresh)
         motion++;
         } else {
         motion = 1;
         }
       */
    } else {
      comb_mask[0] = FALSE;
    }

    for (i = 1; i < width; i++) {
      const guint64 idx = i * incr;
      const guint64 res_idx = (i - 1) / block_width;

      diff1 = fj[idx] - fjm1[idx];
      diff2 = fj[idx] - fjp1[idx];
      if ((diff1 > spatial_thresh && diff2 > spatial_thresh)
          || (diff1 < -spatial_thresh && diff2 < -spatial_thresh)) {
        comb_mask[i] =
            abs (fjm2[idx] + (fj[idx] << 2) + fjp2[idx] - 3 * (fjm1[idx] +
                fjp1[idx])) > spatial_threshx6;
      } else {
        comb_mask[i] = FALSE;
      }

      if (i == 1 && comb_mask[i - 1] && comb_mask[i]) {
        /* left edge */
        block_scores[res_idx]++;
      } else if (i == width - 1) {
        /* right edge */
        if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i])
          block_scores[res_idx]++;
        if (comb_mask[i - 1] && comb_mask[i])
          block_scores[i / block_width]++;
      } else if (comb_mask[i - 2] && comb_mask[i - 1] && comb_mask[i]) {
        block_scores[res_idx]++;
      }
    }
    /* advance down a line */
    fjm2 = fjm1;
    fjm1 = fj;
    fj = fjp1;
    fjp1 = fjp2;
    fjp2 = fj + stridex2;
  }

  block_score = 0;
  for (i = 0; i < width / block_width; i++) {
    if (block_scores[i] > block_score)
      block_score = block_scores[i];
  }

  g_free (block_scores);
  g_free (comb_mask);
  return block_score;
}

/* a pass is made over the field using one of three comb-detection metrics
   and the results are then analysed block-wise. if the samples to the left
   and right are combed, they contribute to the block score. if the block
   score is above the given threshold, the frame is combed. if the block
   score is between half the threshold and the threshold, the block is
   slightly combed. if when analysis is complete, slight combing is detected
   that is returned. if any results are observed that are above the threshold,
   the function returns immediately */
/* 0th field's parity defines operation */
static gfloat
opposite_parity_windowed_comb (GstFieldAnalysis * filter,
    FieldAnalysisFields (*history)[2])
{
  gint j;
  gboolean slightly_combed;

  const gint height = GST_VIDEO_FRAME_HEIGHT (&(*history)[0].frame);
  const gint stride = GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0);
  const guint64 block_thresh = filter->block_thresh;
  const guint64 block_height = filter->block_height;
  guint8 *base_fj, *base_fjp1;

  if ((*history)[0].parity == TOP_FIELD) {
    base_fj =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame, 0);
    base_fjp1 =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[1].frame, 0);
  } else {
    base_fj =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[1].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[1].frame, 0);
    base_fjp1 =
        GST_VIDEO_FRAME_COMP_DATA (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_OFFSET (&(*history)[0].frame,
        0) + GST_VIDEO_FRAME_COMP_STRIDE (&(*history)[0].frame, 0);
  }

  /* we operate on a row of blocks of height block_height through each iteration */
  slightly_combed = FALSE;
  for (j = 0; j <= height - filter->ignored_lines - block_height;
      j += block_height) {
    guint64 line_offset = (filter->ignored_lines + j) * stride;
    guint block_score =
        filter->block_score_for_row (filter, history, base_fj + line_offset,
        base_fjp1 + line_offset);

    if (block_score > (block_thresh >> 1)
        && block_score <= block_thresh) {
      /* blend if nothing more combed comes along */
      slightly_combed = TRUE;
    } else if (block_score > block_thresh) {
      if (GST_VIDEO_INFO_INTERLACE_MODE (&(*history)[0].frame.info) ==
          GST_VIDEO_INTERLACE_MODE_INTERLEAVED) {
        return 1.0f;            /* blend */
      } else {
        return 2.0f;            /* deinterlace */
      }
    }
  }

  return (gfloat) slightly_combed;      /* TRUE means blend, else don't */
}

/* this is where the magic happens
 *
 * the buffer incoming to the chain function (buf_to_queue) is added to the
 * internal queue and then should no longer be used until it is popped from the
 * queue.
 *
 * analysis is performed on the incoming buffer (peeked from the queue) and the
 * previous buffer using two classes of metrics making up five individual
 * scores.
 *
 * there are two same-parity comparisons: top of current with top of previous
 * and bottom of current with bottom of previous
 *
 * there are three opposing parity comparisons: top of current with bottom of
 * _current_, top of current with bottom of previous and bottom of current with
 * top of previous.
 *
 * from the results of these comparisons we can use some rather complex logic to
 * identify the state of the previous buffer, decorate and return it and
 * identify some preliminary state of the current buffer.
 *
 * the returned buffer has a ref on it (it has come from _make_metadata_writable
 * that was called on an incoming buffer that was queued and then popped) */
static GstBuffer *
gst_field_analysis_process_buffer (GstFieldAnalysis * filter,
    GstBuffer ** buf_to_queue)
{
  /* res0/1 correspond to f0/1 */
  FieldAnalysis *res0, *res1;
  FieldAnalysisFields history[2];
  GstBuffer *outbuf = NULL;

  /* move previous result to index 1 */
  filter->frames[1] = filter->frames[0];

  if (!gst_video_frame_map (&filter->frames[0].frame, &filter->vinfo,
          *buf_to_queue, GST_MAP_READ)) {
    GST_ERROR_OBJECT (filter, "Failed to map buffer: %" GST_PTR_FORMAT,
        *buf_to_queue);
    return NULL;
  }
  filter->nframes++;
  /* note that we have a ref and mapping the buffer takes a ref so to destroy a
   * buffer we need to unmap it and unref it */

  res0 = &filter->frames[0].results;    /* results for current frame */
  res1 = &filter->frames[1].results;    /* results for previous frame */

  history[0].frame = filter->frames[0].frame;
  /* we do it like this because the first frame has no predecessor so this is
   * the only result we can get for it */
  if (filter->nframes >= 1) {
    history[1].frame = filter->frames[0].frame;
    history[0].parity = TOP_FIELD;
    history[1].parity = BOTTOM_FIELD;
    /* compare the fields within the buffer, if the buffer exhibits combing it
     * could be interlaced or a mixed telecine frame */
    res0->f = filter->same_frame (filter, &history);
    res0->t = res0->b = res0->t_b = res0->b_t = G_MAXFLOAT;
    if (filter->nframes == 1)
      GST_DEBUG_OBJECT (filter, "Scores: f %f, t , b , t_b , b_t ", res0->f);
    if (res0->f <= filter->frame_thresh) {
      res0->conclusion = FIELD_ANALYSIS_PROGRESSIVE;
    } else {
      res0->conclusion = FIELD_ANALYSIS_INTERLACED;
    }
    res0->holding = -1;         /* needed fields unknown */
    res0->drop = FALSE;
  }
  if (filter->nframes >= 2) {
    guint telecine_matches;
    gboolean first_buffer = filter->first_buffer;

    filter->first_buffer = FALSE;

    history[1].frame = filter->frames[1].frame;

    /* compare the top and bottom fields to the previous frame */
    history[0].parity = TOP_FIELD;
    history[1].parity = TOP_FIELD;
    res0->t = filter->same_field (filter, &history);
    history[0].parity = BOTTOM_FIELD;
    history[1].parity = BOTTOM_FIELD;
    res0->b = filter->same_field (filter, &history);

    /* compare the top field from this frame to the bottom of the previous for
     * for combing (and vice versa) */
    history[0].parity = TOP_FIELD;
    history[1].parity = BOTTOM_FIELD;
    res0->t_b = filter->same_frame (filter, &history);
    history[0].parity = BOTTOM_FIELD;
    history[1].parity = TOP_FIELD;
    res0->b_t = filter->same_frame (filter, &history);

    GST_DEBUG_OBJECT (filter,
        "Scores: f %f, t %f, b %f, t_b %f, b_t %f", res0->f,
        res0->t, res0->b, res0->t_b, res0->b_t);

    /* analysis */
    telecine_matches = 0;
    if (res0->t_b <= filter->frame_thresh)
      telecine_matches |= FIELD_ANALYSIS_TOP_BOTTOM;
    if (res0->b_t <= filter->frame_thresh)
      telecine_matches |= FIELD_ANALYSIS_BOTTOM_TOP;
    /* normally if there is a top or bottom field match, it is significantly
     * smaller than the other match - try 10% */
    if (res0->t <= filter->field_thresh || res0->t * (100 / 10) < res0->b)
      telecine_matches |= FIELD_ANALYSIS_TOP_MATCH;
    if (res0->b <= filter->field_thresh || res0->b * (100 / 10) < res0->t)
      telecine_matches |= FIELD_ANALYSIS_BOTTOM_MATCH;

    if (telecine_matches & (FIELD_ANALYSIS_TOP_MATCH |
            FIELD_ANALYSIS_BOTTOM_MATCH)) {
      /* we have a repeated field => some kind of telecine */
      if (res1->f <= filter->frame_thresh) {
        /* prev P */
        if ((telecine_matches & FIELD_ANALYSIS_TOP_MATCH)
            && (telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH)) {
          /* prev P, cur repeated => cur P */
          res0->conclusion = FIELD_ANALYSIS_TELECINE_PROGRESSIVE;
          res0->holding = 1 + BOTH_FIELDS;
          /* push prev P, RFF */
          res1->drop = TRUE;
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        } else {
          /* prev P, cur t xor b matches => cur TCM */
          res0->conclusion = FIELD_ANALYSIS_TELECINE_MIXED;
          /* hold non-repeated: if bottom match, hold top = 1 + 0 */
          res0->holding = 1 + !(telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH);
          /* push prev P */
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        }
      } else {
        /* prev !P */
        gboolean b, t;

        if (res0->f <= filter->frame_thresh) {
          /* cur P */
          res0->conclusion = FIELD_ANALYSIS_TELECINE_PROGRESSIVE;
          res0->holding = 1 + BOTH_FIELDS;
        } else {
          /* cur !P */
          res0->conclusion = FIELD_ANALYSIS_TELECINE_MIXED;
          if (telecine_matches & FIELD_ANALYSIS_TOP_MATCH
              && telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH) {
            /* cur t && b */
            res0->holding = 0;
          } else {
            /* cur t xor b; hold non-repeated */
            res0->holding =
                1 + !(telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH);
          }
        }

        if (res1->holding == -1) {
          b = t = TRUE;
        } else {
          b = res1->holding == 1 + BOTTOM_FIELD;
          t = res1->holding == 1 + TOP_FIELD;
        }

        if ((t && telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH) || (b
                && telecine_matches & FIELD_ANALYSIS_TOP_MATCH)) {
          if (t && telecine_matches & FIELD_ANALYSIS_BOTTOM_MATCH) {
            res1->holding = 1 + TOP_FIELD;
          } else if (b && telecine_matches & FIELD_ANALYSIS_TOP_MATCH) {
            res1->holding = 1 + BOTTOM_FIELD;
          }
          /* push 1F held field */
          outbuf =
              gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
              res1->conclusion, res1->drop);
        } else if (res0->f > filter->frame_thresh && ((t
                    && telecine_matches & FIELD_ANALYSIS_BOTTOM_TOP) || (b
                    && telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM))) {
          if (t && telecine_matches & FIELD_ANALYSIS_BOTTOM_TOP) {
            res1->holding = 1 + TOP_FIELD;
          } else if (b && telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM) {
            res1->holding = 1 + BOTTOM_FIELD;
          }
          res0->conclusion = FIELD_ANALYSIS_TELECINE_MIXED;
          /* hold the opposite field to the one held in the last frame */
          res0->holding = 1 + (res1->holding == 1 + TOP_FIELD);
          /* push 1F held field */
          outbuf =
              gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
              res1->conclusion, res1->drop);
        } else if (first_buffer && (telecine_matches & FIELD_ANALYSIS_BOTTOM_TOP
                || telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM)) {
          /* non-matched field is an orphan in the first buffer - push orphan as 1F */
          res1->conclusion = FIELD_ANALYSIS_TELECINE_MIXED;
          /* if prev b matched, prev t is orphan */
          res1->holding = 1 + !(telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM);
          /* push 1F held field */
          outbuf =
              gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
              res1->conclusion, res1->drop);
        } else if (res1->holding == 1 + BOTH_FIELDS || res1->holding == -1) {
          /* holding both fields, push prev as is */
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        } else {
          /* push prev as is with RFF */
          res1->drop = TRUE;
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        }
      }
    } else if (res0->f <= filter->frame_thresh) {
      /* cur P */
      res0->conclusion = FIELD_ANALYSIS_PROGRESSIVE;
      res0->holding = 1 + BOTH_FIELDS;
      if (res1->holding == 1 + BOTH_FIELDS || res1->holding == -1) {
        /* holding both fields, push prev as is */
        outbuf =
            gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
            res1->drop);
      } else if (res1->holding > 0) {
        /* holding one field, push prev 1F held */
        outbuf =
            gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
            res1->conclusion, res1->drop);
      } else {
        /* unknown or no fields held, push prev as is with RFF */
        /* this will push unknown as drop - should be pushed as not drop? */
        res1->drop = TRUE;
        outbuf =
            gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
            res1->drop);
      }
    } else {
      /* cur !P */
      if (telecine_matches & (FIELD_ANALYSIS_TOP_BOTTOM |
              FIELD_ANALYSIS_BOTTOM_TOP)) {
        /* cross-parity match => TCM */
        gboolean b, t;

        if (res1->holding == -1) {
          b = t = TRUE;
        } else {
          b = res1->holding == 1 + BOTTOM_FIELD;
          t = res1->holding == 1 + TOP_FIELD;
        }

        res0->conclusion = FIELD_ANALYSIS_TELECINE_MIXED;
        /* leave holding as unknown */
        if (res1->holding == 1 + BOTH_FIELDS) {
          /* prev P/TCP/I [or TCM repeated (weird case)] */
          /* push prev as is */
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        } else if ((t && telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM) || (b
                && telecine_matches & FIELD_ANALYSIS_BOTTOM_TOP)) {
          /* held is opposite to matched => need both field from prev */
          /* if t_b, hold bottom from prev and top from current, else vice-versa */
          res1->holding = 1 + !!(telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM);
          res0->holding = 1 + !(telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM);
          /* push prev TCM */
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        } else if ((res1->holding > 0 && res1->holding != 1 + BOTH_FIELDS) || (t
                && telecine_matches & FIELD_ANALYSIS_BOTTOM_TOP) || (b
                && telecine_matches & FIELD_ANALYSIS_TOP_BOTTOM)) {
          /* held field is needed, push prev 1F held */
          outbuf =
              gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
              res1->conclusion, res1->drop);
        } else {
          /* holding none or unknown */
          /* push prev as is with RFF */
          res1->drop = TRUE;
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        }
      } else {
        /* cur I */
        res0->conclusion = FIELD_ANALYSIS_INTERLACED;
        res0->holding = 1 + BOTH_FIELDS;
        /* push prev appropriately */
        res1->drop = res1->holding <= 0;
        if (res1->holding != 0) {
          res1->drop = FALSE;
          if (res1->holding == 1 + BOTH_FIELDS || res1->holding == -1) {
            /* push prev as is */
            outbuf =
                gst_field_analysis_decorate (filter, -1, FALSE,
                res1->conclusion, res1->drop);
          } else {
            /* push prev 1F held */
            outbuf =
                gst_field_analysis_decorate (filter, !(res1->holding - 1), TRUE,
                res1->conclusion, res1->drop);
          }
        } else {
          /* push prev as is with RFF */
          res1->drop = TRUE;
          outbuf =
              gst_field_analysis_decorate (filter, -1, FALSE, res1->conclusion,
              res1->drop);
        }
      }
    }
  }

  switch (res0->conclusion) {
    case FIELD_ANALYSIS_PROGRESSIVE:
      GST_DEBUG_OBJECT (filter, "Conclusion: PROGRESSIVE");
      break;
    case FIELD_ANALYSIS_INTERLACED:
      GST_DEBUG_OBJECT (filter, "Conclusion: INTERLACED");
      break;
    case FIELD_ANALYSIS_TELECINE_PROGRESSIVE:
      GST_DEBUG_OBJECT (filter, "Conclusion: TC PROGRESSIVE");
      break;
    case FIELD_ANALYSIS_TELECINE_MIXED:
      GST_DEBUG_OBJECT (filter, "Conclusion: TC MIXED %s",
          res0->holding ==
          1 + BOTH_FIELDS ? "top and bottom" : res0->holding ==
          1 + BOTTOM_FIELD ? "bottom" : "top");
      break;
    default:
      GST_DEBUG_OBJECT (filter, "Invalid conclusion! This is a bug!");
      break;
  }

  return outbuf;
}

/* we have a ref on buf when it comes into chain */
static GstFlowReturn
gst_field_analysis_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstFieldAnalysis *filter;
  GstBuffer *outbuf = NULL;

  filter = GST_FIELDANALYSIS (parent);

  GST_OBJECT_LOCK (filter);
  if (filter->flushing) {
    GST_DEBUG_OBJECT (filter, "We are flushing.");
    /* we have a ref on buf so it must be unreffed */
    goto unref_unlock_ret;
  }

  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT)) {
    GST_DEBUG_OBJECT (filter, "Discont: flushing");
    /* we should have a ref on outbuf, either because we had one when it entered
     * the queue and _make_metadata_writable () inside _decorate () returned
     * the same buffer or because it returned a new buffer on which we have one
     * ref */
    outbuf = gst_field_analysis_flush_one (filter, NULL);

    if (outbuf) {
      /* we give away our ref on outbuf here */
      GST_OBJECT_UNLOCK (filter);
      ret = gst_pad_push (filter->srcpad, outbuf);
      GST_OBJECT_LOCK (filter);
      if (filter->flushing) {
        GST_DEBUG_OBJECT (filter, "We are flushing. outbuf already pushed.");
        /* we have a ref on buf so it must be unreffed */
        goto unref_unlock_ret;
      }
    }

    gst_field_analysis_clear_frames (filter);

    if (ret != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (filter,
          "Pushing of flushed buffer failed with return %d", ret);
      /* we have a ref on buf so it must be unreffed */
      goto unref_unlock_ret;
    } else {
      outbuf = NULL;
    }
  }

  /* after this function, buf has been pushed to the internal queue and its ref
   * retained there and we have a ref on outbuf */
  outbuf = gst_field_analysis_process_buffer (filter, &buf);

  GST_OBJECT_UNLOCK (filter);

  /* here we give up our ref on outbuf */
  if (outbuf)
    ret = gst_pad_push (filter->srcpad, outbuf);

  return ret;

unref_unlock_ret:
  /* we must unref the input buffer here */
  gst_buffer_unref (buf);
  GST_OBJECT_UNLOCK (filter);
  return ret;
}

static GstStateChangeReturn
gst_field_analysis_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstFieldAnalysis *filter = GST_FIELDANALYSIS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_field_analysis_reset (filter);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
    default:
      break;
  }

  return ret;
}

static void
gst_field_analysis_finalize (GObject * object)
{
  GstFieldAnalysis *filter = GST_FIELDANALYSIS (object);

  gst_field_analysis_reset (filter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
fieldanalysis_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (fieldanalysis, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fieldanalysis,
    "Video field analysis",
    fieldanalysis_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
