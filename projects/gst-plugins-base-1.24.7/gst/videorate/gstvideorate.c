/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
 * SECTION:element-videorate
 * @title: videorate
 *
 * This element takes an incoming stream of timestamped video frames.
 * It will produce a perfect stream that matches the source pad's framerate.
 *
 * The correction is performed by dropping and duplicating frames, no fancy
 * algorithm is used to interpolate frames (yet).
 *
 * By default the element will simply negotiate the same framerate on its
 * source and sink pad.
 *
 * This operation is useful to link to elements that require a perfect stream.
 * Typical examples are formats that do not store timestamps for video frames,
 * but only store a framerate, like Ogg and AVI.
 *
 * A conversion to a specific framerate can be forced by using filtered caps on
 * the source pad.
 *
 * The properties #GstVideoRate:in, #GstVideoRate:out, #GstVideoRate:duplicate
 * and #GstVideoRate:drop can be read to obtain information about number of
 * input frames, output frames, dropped frames (i.e. the number of unused input
 * frames) and duplicated frames (i.e. the number of times an input frame was
 * duplicated, beside being used normally).
 *
 * An input stream that needs no adjustments will thus never have dropped or
 * duplicated frames.
 *
 * When the #GstVideoRate:silent property is set to FALSE, a GObject property
 * notification will be emitted whenever one of the #GstVideoRate:duplicate or
 * #GstVideoRate:drop values changes.
 * This can potentially cause performance degradation.
 * Note that property notification will happen from the streaming thread, so
 * applications should be prepared for this.
 *
 * The property #GstVideoRate:rate allows the modification of video speed by a
 * certain factor. It must not be confused with framerate. Think of rate as
 * speed and framerate as flow.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/video.ogg ! videoconvert ! videoscale ! videorate ! video/x-raw,framerate=15/1 ! autovideosink
 * ]|
 *  Decode a video file and adjust the framerate to 15 fps before playing.
 * To create a test Ogg/Theora file refer to the documentation of theoraenc.
 * |[
 * gst-launch-1.0 -v v4l2src ! videorate ! video/x-raw,framerate=25/2 ! theoraenc ! oggmux ! filesink location=recording.ogg
 * ]|
 *  Capture video from a V4L device, and adjust the stream to 12.5 fps before
 * encoding to Ogg/Theora.
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/video.ogg ! videoconvert ! videoscale ! videorate ! video/x-raw,framerate=1/5 ! jpegenc ! multifilesink location=snapshot-%05d.jpg
 * ]|
 *  Decode a video file and save a snapshot every 5 seconds as consecutively numbered jpeg file.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideorate.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (video_rate_debug);
#define GST_CAT_DEFAULT video_rate_debug

/* GstVideoRate signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_SILENT          TRUE
#define DEFAULT_NEW_PREF        1.0
#define DEFAULT_SKIP_TO_FIRST   FALSE
#define DEFAULT_DROP_ONLY       FALSE
#define DEFAULT_AVERAGE_PERIOD  0
#define DEFAULT_MAX_RATE        G_MAXINT
#define DEFAULT_RATE            1.0
#define DEFAULT_MAX_DUPLICATION_TIME      0
#define DEFAULT_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION   GST_SECOND
#define DEFAULT_DROP_OUT_OF_SEGMENT       FALSE

enum
{
  PROP_0,
  PROP_IN,
  PROP_OUT,
  PROP_DUP,
  PROP_DROP,
  PROP_SILENT,
  PROP_NEW_PREF,
  PROP_SKIP_TO_FIRST,
  PROP_DROP_ONLY,
  PROP_AVERAGE_PERIOD,
  PROP_MAX_RATE,
  PROP_RATE,
  PROP_MAX_DUPLICATION_TIME,
  PROP_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION,
  PROP_DROP_OUT_OF_SEGMENT
};

static GstStaticPadTemplate gst_video_rate_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY);" "video/x-bayer(ANY);"
        "image/jpeg(ANY);" "image/png(ANY)")
    );

static GstStaticPadTemplate gst_video_rate_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY);" "video/x-bayer(ANY);"
        "image/jpeg(ANY);" "image/png(ANY)")
    );

static void gst_video_rate_swap_prev (GstVideoRate * videorate,
    GstBuffer * buffer, gint64 time);
static gboolean gst_video_rate_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_video_rate_src_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_video_rate_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);

static gboolean gst_video_rate_setcaps (GstBaseTransform * trans,
    GstCaps * in_caps, GstCaps * out_caps);

static GstCaps *gst_video_rate_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static GstCaps *gst_video_rate_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

static GstFlowReturn gst_video_rate_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

static gboolean gst_video_rate_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

static gboolean gst_video_rate_start (GstBaseTransform * trans);
static gboolean gst_video_rate_stop (GstBaseTransform * trans);


static void gst_video_rate_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_video_rate_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GParamSpec *pspec_drop = NULL;
static GParamSpec *pspec_duplicate = NULL;

#define gst_video_rate_parent_class parent_class
G_DEFINE_TYPE (GstVideoRate, gst_video_rate, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (videorate, "videorate",
    GST_RANK_NONE, GST_TYPE_VIDEO_RATE);

static void
gst_video_rate_class_init (GstVideoRateClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS (klass);

  object_class->set_property = gst_video_rate_set_property;
  object_class->get_property = gst_video_rate_get_property;

  base_class->set_caps = GST_DEBUG_FUNCPTR (gst_video_rate_setcaps);
  base_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_video_rate_transform_caps);
  base_class->transform_ip = GST_DEBUG_FUNCPTR (gst_video_rate_transform_ip);
  base_class->sink_event = GST_DEBUG_FUNCPTR (gst_video_rate_sink_event);
  base_class->src_event = GST_DEBUG_FUNCPTR (gst_video_rate_src_event);
  base_class->start = GST_DEBUG_FUNCPTR (gst_video_rate_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_video_rate_stop);
  base_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_video_rate_fixate_caps);
  base_class->query = GST_DEBUG_FUNCPTR (gst_video_rate_query);
  base_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_video_rate_propose_allocation);

  g_object_class_install_property (object_class, PROP_IN,
      g_param_spec_uint64 ("in", "In",
          "Number of input frames", 0, G_MAXUINT64, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_OUT,
      g_param_spec_uint64 ("out", "Out", "Number of output frames", 0,
          G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  pspec_duplicate = g_param_spec_uint64 ("duplicate", "Duplicate",
      "Number of duplicated frames", 0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DUP, pspec_duplicate);
  pspec_drop = g_param_spec_uint64 ("drop", "Drop", "Number of dropped frames",
      0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DROP, pspec_drop);
  g_object_class_install_property (object_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "silent",
          "Don't emit notify for dropped and duplicated frames", DEFAULT_SILENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NEW_PREF,
      g_param_spec_double ("new-pref", "New Pref",
          "Value indicating how much to prefer new frames (unused)", 0.0, 1.0,
          DEFAULT_NEW_PREF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:skip-to-first:
   *
   * Don't produce buffers before the first one we receive.
   */
  g_object_class_install_property (object_class, PROP_SKIP_TO_FIRST,
      g_param_spec_boolean ("skip-to-first", "Skip to first buffer",
          "Don't produce buffers before the first one we receive",
          DEFAULT_SKIP_TO_FIRST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:drop-only:
   *
   * Only drop frames, no duplicates are produced.
   */
  g_object_class_install_property (object_class, PROP_DROP_ONLY,
      g_param_spec_boolean ("drop-only", "Only Drop",
          "Only drop frames, no duplicates are produced",
          DEFAULT_DROP_ONLY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:average-period:
   *
   * Arrange for maximum framerate by dropping frames beyond a certain framerate,
   * where the framerate is calculated using a moving average over the
   * configured.
   */
  g_object_class_install_property (object_class, PROP_AVERAGE_PERIOD,
      g_param_spec_uint64 ("average-period", "Period over which to average",
          "Period over which to average the framerate (in ns) (0 = disabled)",
          0, G_MAXINT64, DEFAULT_AVERAGE_PERIOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:max-rate:
   *
   * maximum framerate to pass through
   */
  g_object_class_install_property (object_class, PROP_MAX_RATE,
      g_param_spec_int ("max-rate", "maximum framerate",
          "Maximum framerate allowed to pass through "
          "(in frames per second, implies drop-only)",
          1, G_MAXINT, DEFAULT_MAX_RATE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:rate:
   *
   * Factor of speed for frame displaying
   *
   * Since: 1.12
   */
  g_object_class_install_property (object_class, PROP_RATE,
      g_param_spec_double ("rate", "Rate",
          "Factor of speed for frame displaying", 0.0, G_MAXDOUBLE,
          DEFAULT_RATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstVideoRate:max-duplication-time:
   *
   * Duplicate frames only if the gap between two consecutive frames does not
   * exceed this duration.
   *
   * Since: 1.16
   */
  g_object_class_install_property (object_class, PROP_MAX_DUPLICATION_TIME,
      g_param_spec_uint64 ("max-duplication-time",
          "Maximum time to duplicate a frame",
          "Do not duplicate frames if the gap exceeds this period "
          "(in ns) (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_MAX_DUPLICATION_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:max-closing-segment-duplication-duration:
   *
   * Limits the maximum duration for which the last buffer is duplicated when
   * finalizing a segment or on EOS. When receiving an EOS event or a new
   * segment, videorate duplicates the last frame to close the configured
   * segment (copying the last buffer until its #GstSegment.stop time (or
   * #GstSegment.start time for reverse playback) is reached), this property
   * ensures that it won't push buffers covering a duration longer than
   * specified.
   *
   * Since: 1.22
   */
  g_object_class_install_property (object_class,
      PROP_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION,
      g_param_spec_uint64 ("max-closing-segment-duplication-duration",
          "Maximum closing segment duplication duration",
          "Maximum duration of duplicated buffers to close current segment", 0,
          G_MAXUINT64, DEFAULT_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoRate:drop-out-of-segment:
   *
   * Drop all frames that are out of segment
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_DROP_OUT_OF_SEGMENT,
      g_param_spec_boolean ("drop-out-of-segment",
          "Drop out of segment buffers", "Drop out of segment buffers",
          DEFAULT_DROP_OUT_OF_SEGMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Video rate adjuster", "Filter/Effect/Video",
      "Drops/duplicates/adjusts timestamps on video frames to make a perfect stream",
      "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_video_rate_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_video_rate_src_template);
}

static void
gst_value_fraction_get_extremes (const GValue * v,
    gint * min_num, gint * min_denom, gint * max_num, gint * max_denom)
{
  if (GST_VALUE_HOLDS_FRACTION (v)) {
    *min_num = *max_num = gst_value_get_fraction_numerator (v);
    *min_denom = *max_denom = gst_value_get_fraction_denominator (v);
  } else if (GST_VALUE_HOLDS_FRACTION_RANGE (v)) {
    const GValue *min, *max;

    min = gst_value_get_fraction_range_min (v);
    *min_num = gst_value_get_fraction_numerator (min);
    *min_denom = gst_value_get_fraction_denominator (min);

    max = gst_value_get_fraction_range_max (v);
    *max_num = gst_value_get_fraction_numerator (max);
    *max_denom = gst_value_get_fraction_denominator (max);
  } else if (GST_VALUE_HOLDS_LIST (v)) {
    gint min_n = G_MAXINT, min_d = 1, max_n = 0, max_d = 1;
    int i, n;

    *min_num = G_MAXINT;
    *min_denom = 1;
    *max_num = 0;
    *max_denom = 1;

    n = gst_value_list_get_size (v);

    g_assert (n > 0);

    for (i = 0; i < n; i++) {
      const GValue *t = gst_value_list_get_value (v, i);

      gst_value_fraction_get_extremes (t, &min_n, &min_d, &max_n, &max_d);
      if (gst_util_fraction_compare (min_n, min_d, *min_num, *min_denom) < 0) {
        *min_num = min_n;
        *min_denom = min_d;
      }

      if (gst_util_fraction_compare (max_n, max_d, *max_num, *max_denom) > 0) {
        *max_num = max_n;
        *max_denom = max_d;
      }
    }
  } else {
    g_warning ("Unknown type for framerate");
    *min_num = 0;
    *min_denom = 1;
    *max_num = G_MAXINT;
    *max_denom = 1;
  }
}

/* Clamp the framerate in a caps structure to be a smaller range then
 * [1...max_rate], otherwise return false */
static gboolean
gst_video_max_rate_clamp_structure (GstStructure * s, gint maxrate,
    gint * min_num, gint * min_denom, gint * max_num, gint * max_denom)
{
  gboolean ret = FALSE;

  if (!gst_structure_has_field (s, "framerate")) {
    /* No framerate field implies any framerate, clamping would result in
     * [1..max_rate] so not a real subset */
    goto out;
  } else {
    const GValue *v;
    GValue intersection = { 0, };
    GValue clamp = { 0, };
    gint tmp_num, tmp_denom;

    g_value_init (&clamp, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full (&clamp, 0, 1, maxrate, 1);

    v = gst_structure_get_value (s, "framerate");
    ret = gst_value_intersect (&intersection, v, &clamp);
    g_value_unset (&clamp);

    if (!ret)
      goto out;

    gst_value_fraction_get_extremes (&intersection,
        min_num, min_denom, max_num, max_denom);

    gst_value_fraction_get_extremes (v,
        &tmp_num, &tmp_denom, max_num, max_denom);

    if (gst_util_fraction_compare (*max_num, *max_denom, maxrate, 1) > 0) {
      *max_num = maxrate;
      *max_denom = 1;
    }

    gst_structure_take_value (s, "framerate", &intersection);
  }

out:
  return ret;
}

static GstCaps *
gst_video_rate_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVideoRate *videorate = GST_VIDEO_RATE (trans);
  GstCaps *ret;
  GstStructure *s, *s1, *s2, *s3 = NULL;
  int maxrate = g_atomic_int_get (&videorate->max_rate);
  gint i;

  ret = gst_caps_new_empty ();

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    s = gst_caps_get_structure (caps, i);

    s1 = gst_structure_copy (s);

    if (videorate->updating_caps && direction == GST_PAD_SINK) {
      GST_INFO_OBJECT (trans,
          "Only updating caps %" GST_PTR_FORMAT " with framerate" " %d/%d",
          caps, videorate->to_rate_numerator, videorate->to_rate_denominator);

      gst_structure_set (s1, "framerate", GST_TYPE_FRACTION,
          videorate->to_rate_numerator, videorate->to_rate_denominator, NULL);
      ret = gst_caps_merge_structure (ret, s1);

      continue;
    }

    s2 = gst_structure_copy (s);
    s3 = NULL;

    if (videorate->drop_only) {
      gint min_num = 0, min_denom = 1;
      gint max_num = G_MAXINT, max_denom = 1;

      /* Clamp the caps to our maximum rate as the first caps if possible */
      if (!gst_video_max_rate_clamp_structure (s1, maxrate,
              &min_num, &min_denom, &max_num, &max_denom)) {
        min_num = 0;
        min_denom = 1;
        max_num = maxrate;
        max_denom = 1;

        /* clamp wouldn't be a real subset of 1..maxrate, in this case the sink
         * caps should become [1..maxrate], [1..maxint] and the src caps just
         * [1..maxrate].  In case there was a caps incompatibility things will
         * explode later as appropriate :)
         *
         * In case [X..maxrate] == [X..maxint], skip as we'll set it later
         */
        if (direction == GST_PAD_SRC && maxrate != G_MAXINT)
          gst_structure_set (s1, "framerate", GST_TYPE_FRACTION_RANGE,
              min_num, min_denom, maxrate, 1, NULL);
        else {
          gst_structure_free (s1);
          s1 = NULL;
        }
      }

      if (direction == GST_PAD_SRC) {
        /* We can accept anything as long as it's at least the minimal framerate
         * the the sink needs */
        gst_structure_set (s2, "framerate", GST_TYPE_FRACTION_RANGE,
            min_num, min_denom, G_MAXINT, 1, NULL);

        /* Also allow unknown framerate, if it isn't already */
        if (min_num != 0 || min_denom != 1) {
          s3 = gst_structure_copy (s);
          gst_structure_set (s3, "framerate", GST_TYPE_FRACTION, 0, 1, NULL);
        }
      } else if (max_num != 0 || max_denom != 1) {
        /* We can provide everything up to the maximum framerate at the src */
        gst_structure_set (s2, "framerate", GST_TYPE_FRACTION_RANGE,
            0, 1, max_num, max_denom, NULL);
      } else if (min_num == 0) {
        /* if provided with variable framerate input, then we don't have a
         * restriction on the output framerate currently */
        gst_structure_set (s2, "framerate", GST_TYPE_FRACTION_RANGE,
            min_num, 1, maxrate, 1, NULL);
      }
    } else if (direction == GST_PAD_SINK) {
      gint min_num = 0, min_denom = 1;
      gint max_num = G_MAXINT, max_denom = 1;

      if (!gst_video_max_rate_clamp_structure (s1, maxrate,
              &min_num, &min_denom, &max_num, &max_denom)) {
        gst_structure_free (s1);
        s1 = NULL;
      }
      gst_structure_set (s2, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
          maxrate, 1, NULL);
    } else {
      /* set the framerate as a range */
      gst_structure_set (s2, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
          G_MAXINT, 1, NULL);
    }
    if (s1 != NULL)
      ret = gst_caps_merge_structure_full (ret, s1,
          gst_caps_features_copy (gst_caps_get_features (caps, i)));
    ret = gst_caps_merge_structure_full (ret, s2,
        gst_caps_features_copy (gst_caps_get_features (caps, i)));
    if (s3 != NULL)
      ret = gst_caps_merge_structure_full (ret, s3,
          gst_caps_features_copy (gst_caps_get_features (caps, i)));
  }
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;
  }
  return ret;
}

static GstCaps *
gst_video_rate_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *s;
  gint num, denom;
  const GValue *par;

  s = gst_caps_get_structure (caps, 0);
  if (G_UNLIKELY (!gst_structure_get_fraction (s, "framerate", &num, &denom)))
    return othercaps;

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);
  s = gst_caps_get_structure (othercaps, 0);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", num, denom);

  if ((par = gst_structure_get_value (s, "pixel-aspect-ratio")))
    gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio", 1, 1);

  return gst_caps_fixate (othercaps);
}

static gboolean
gst_video_rate_setcaps (GstBaseTransform * trans, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstVideoRate *videorate = GST_VIDEO_RATE (trans);
  GstStructure *structure;
  gboolean ret = TRUE;
  gint rate_numerator, rate_denominator;

  GST_DEBUG_OBJECT (trans, "setcaps called in: %" GST_PTR_FORMAT
      " out: %" GST_PTR_FORMAT, in_caps, out_caps);

  structure = gst_caps_get_structure (in_caps, 0);
  if (!gst_structure_get_fraction (structure, "framerate",
          &rate_numerator, &rate_denominator))
    goto no_framerate;

  videorate->from_rate_numerator = rate_numerator;
  videorate->from_rate_denominator = rate_denominator;

  structure = gst_caps_get_structure (out_caps, 0);
  if (!gst_structure_get_fraction (structure, "framerate",
          &rate_numerator, &rate_denominator))
    goto no_framerate;

  /* out_frame_count is scaled by the frame rate caps when calculating next_ts.
   * when the frame rate caps change, we must update base_ts and reset
   * out_frame_count */
  if (videorate->to_rate_numerator) {
    videorate->base_ts +=
        gst_util_uint64_scale (videorate->out_frame_count +
        (videorate->segment.rate < 0.0 ? 1 : 0),
        videorate->to_rate_denominator * GST_SECOND,
        videorate->to_rate_numerator);
  }
  videorate->out_frame_count = 0;
  videorate->to_rate_numerator = rate_numerator;
  videorate->to_rate_denominator = rate_denominator;

  if (rate_numerator)
    videorate->wanted_diff = gst_util_uint64_scale_int (GST_SECOND,
        rate_denominator, rate_numerator);
  else
    videorate->wanted_diff = 0;

done:
  if (ret) {
    gst_caps_replace (&videorate->in_caps, in_caps);
  }

  return ret;

no_framerate:
  {
    GST_DEBUG_OBJECT (videorate, "no framerate specified");
    ret = FALSE;
    goto done;
  }
}

static void
gst_video_rate_reset (GstVideoRate * videorate, gboolean on_flush)
{
  GST_DEBUG_OBJECT (videorate, "resetting internal variables");

  videorate->in = 0;
  videorate->out = 0;
  videorate->base_ts = 0;
  videorate->out_frame_count = 0;
  videorate->drop = 0;
  videorate->dup = 0;
  videorate->next_ts = GST_CLOCK_TIME_NONE;
  videorate->last_ts = GST_CLOCK_TIME_NONE;
  videorate->discont = TRUE;
  videorate->average = 0;
  videorate->force_variable_rate = FALSE;
  if (!on_flush) {
    /* Do not clear caps on flush events as those are still valid */
    gst_clear_caps (&videorate->in_caps);
  }
  gst_video_rate_swap_prev (videorate, NULL, 0);

  gst_segment_init (&videorate->segment, GST_FORMAT_TIME);
}

static void
gst_video_rate_init (GstVideoRate * videorate)
{
  gst_video_rate_reset (videorate, FALSE);
  videorate->silent = DEFAULT_SILENT;
  videorate->new_pref = DEFAULT_NEW_PREF;
  videorate->drop_only = DEFAULT_DROP_ONLY;
  videorate->drop_out_of_segment = DEFAULT_DROP_OUT_OF_SEGMENT;
  videorate->average_period = DEFAULT_AVERAGE_PERIOD;
  videorate->average_period_set = DEFAULT_AVERAGE_PERIOD;
  videorate->max_rate = DEFAULT_MAX_RATE;
  videorate->rate = DEFAULT_RATE;
  videorate->pending_rate = DEFAULT_RATE;
  videorate->max_duplication_time = DEFAULT_MAX_DUPLICATION_TIME;
  videorate->max_closing_segment_duplication_duration =
      DEFAULT_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION;

  videorate->from_rate_numerator = 0;
  videorate->from_rate_denominator = 0;
  videorate->to_rate_numerator = 0;
  videorate->to_rate_denominator = 0;

  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM (videorate), TRUE);
}

/* @outbuf: (transfer full) needs to be writable */
static GstFlowReturn
gst_video_rate_push_buffer (GstVideoRate * videorate, GstBuffer * outbuf,
    gboolean duplicate, GstClockTime next_intime, gboolean invalid_duration)
{
  GstFlowReturn res = GST_FLOW_OK;
  GstClockTime push_ts;

  GST_BUFFER_OFFSET (outbuf) = videorate->out;
  GST_BUFFER_OFFSET_END (outbuf) = videorate->out + 1;

  if (videorate->discont) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    videorate->discont = FALSE;
  } else
    GST_BUFFER_FLAG_UNSET (outbuf, GST_BUFFER_FLAG_DISCONT);

  if (duplicate)
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
  else
    GST_BUFFER_FLAG_UNSET (outbuf, GST_BUFFER_FLAG_GAP);

  /* this is the timestamp we put on the buffer */
  push_ts = videorate->next_ts;

  videorate->out++;
  videorate->out_frame_count++;
  if (videorate->segment.rate < 0.0) {
    if (videorate->to_rate_numerator) {
      /* interpolate next expected timestamp in the segment */
      GstClockTimeDiff next_ts = videorate->base_ts -
          gst_util_uint64_scale (videorate->out_frame_count + 1,
          videorate->to_rate_denominator * GST_SECOND,
          videorate->to_rate_numerator);

      videorate->next_ts = next_ts < 0 ? GST_CLOCK_TIME_NONE : next_ts;

      GST_BUFFER_DURATION (outbuf) =
          gst_util_uint64_scale (videorate->out_frame_count,
          videorate->to_rate_denominator * GST_SECOND,
          videorate->to_rate_numerator) -
          gst_util_uint64_scale (videorate->out_frame_count - 1,
          videorate->to_rate_denominator * GST_SECOND,
          videorate->to_rate_numerator);
    } else if (next_intime != GST_CLOCK_TIME_NONE) {
      videorate->next_ts = next_intime;
    } else {
      GST_FIXME_OBJECT (videorate, "No next intime for reverse playback");
    }
  } else {
    if (videorate->to_rate_numerator) {
      /* interpolate next expected timestamp in the segment */
      videorate->next_ts = videorate->base_ts +
          gst_util_uint64_scale (videorate->out_frame_count,
          videorate->to_rate_denominator * GST_SECOND,
          videorate->to_rate_numerator);
      GST_BUFFER_DURATION (outbuf) = videorate->next_ts - push_ts;
    } else if (!invalid_duration) {
      /* There must always be a valid duration on prevbuf if rate > 0,
       * it is ensured in the transform_ip function */
      g_assert (GST_BUFFER_PTS_IS_VALID (outbuf));
      g_assert (GST_BUFFER_DURATION_IS_VALID (outbuf));
      g_assert (GST_BUFFER_DURATION (outbuf) != 0);

      videorate->next_ts
          = GST_BUFFER_PTS (outbuf) + GST_BUFFER_DURATION (outbuf);
    }
  }

  /* We do not need to update time in VFR (variable frame rate) mode */
  if (!videorate->drop_only) {
    GST_BUFFER_PTS (outbuf) = push_ts;
  }

  GST_LOG_OBJECT (videorate,
      "old is best, dup, pushing buffer outgoing ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (push_ts));

  if (videorate->drop_out_of_segment
      && !gst_segment_clip (&videorate->segment, GST_FORMAT_TIME,
          GST_BUFFER_PTS (outbuf),
          GST_BUFFER_PTS (outbuf) + GST_BUFFER_DURATION (outbuf), NULL, NULL)) {
    GST_INFO_OBJECT (videorate, "Buffer is out of segment, dropping");

    gst_buffer_unref (outbuf);

    return res;
  }

  res = gst_pad_push (GST_BASE_TRANSFORM_SRC_PAD (videorate), outbuf);

  return res;
}

/* flush the oldest buffer */
static GstFlowReturn
gst_video_rate_flush_prev (GstVideoRate * videorate, gboolean duplicate,
    GstClockTime next_intime, gboolean invalid_duration)
{
  GstBuffer *outbuf;

  if (!videorate->prevbuf)
    goto eos_before_buffers;

  outbuf = gst_buffer_ref (videorate->prevbuf);
  /* make sure we can write to the metadata */
  outbuf = gst_buffer_make_writable (outbuf);

  return gst_video_rate_push_buffer (videorate, outbuf, duplicate, next_intime,
      invalid_duration);

  /* WARNINGS */
eos_before_buffers:
  {
    GST_INFO_OBJECT (videorate, "got EOS before any buffer was received");
    return GST_FLOW_OK;
  }
}

static void
gst_video_rate_swap_prev (GstVideoRate * videorate, GstBuffer * buffer,
    gint64 time)
{
  GST_LOG_OBJECT (videorate, "swap_prev: storing buffer %p in prev", buffer);

  gst_buffer_replace (&videorate->prevbuf, buffer);
  /* Ensure that ->prev_caps always match ->prevbuf */
  if (!buffer)
    gst_caps_replace (&videorate->prev_caps, NULL);
  else if (videorate->prev_caps != videorate->in_caps)
    gst_caps_replace (&videorate->prev_caps, videorate->in_caps);

  videorate->prev_ts = time;
}

static void
gst_video_rate_notify_drop (GstVideoRate * videorate)
{
  g_object_notify_by_pspec ((GObject *) videorate, pspec_drop);
}

static void
gst_video_rate_notify_duplicate (GstVideoRate * videorate)
{
  g_object_notify_by_pspec ((GObject *) videorate, pspec_duplicate);
}

static gboolean
gst_video_rate_check_duplicate_to_close_segment (GstVideoRate * videorate,
    GstClockTime last_input_ts, gboolean is_first)
{
  GstClockTime next_stream_time = videorate->next_ts;
  GstClockTime max_closing_segment_duplication_duration =
      videorate->max_closing_segment_duplication_duration;

  if (!GST_CLOCK_TIME_IS_VALID (videorate->next_ts))
    return FALSE;

  if (videorate->segment.rate > 0.0) {

    if (!GST_CLOCK_TIME_IS_VALID (videorate->segment.stop)) {
      /* Ensure that if no 'stop' is set, we push the last frame anyway */
      return is_first;
    }

    if (next_stream_time >= videorate->segment.stop)
      return FALSE;

    if (GST_CLOCK_TIME_IS_VALID (max_closing_segment_duplication_duration)) {
      if (last_input_ts > videorate->next_ts)
        return TRUE;

      return (videorate->next_ts - last_input_ts <
          max_closing_segment_duplication_duration);
    }

    return TRUE;
  }

  /* Reverse playback */

  if (!GST_CLOCK_TIME_IS_VALID (videorate->segment.start)) {
    /* Ensure that if no 'start' is set, we push the last frame anyway */
    return is_first;
  }

  if (next_stream_time < videorate->segment.start)
    return FALSE;

  if (GST_CLOCK_TIME_IS_VALID (max_closing_segment_duplication_duration)) {
    if (last_input_ts < videorate->next_ts)
      return TRUE;

    return (last_input_ts - videorate->next_ts <
        max_closing_segment_duplication_duration);
  }

  return TRUE;
}

static gint
gst_video_rate_duplicate_to_close_segment (GstVideoRate * videorate)
{
  gint count = 0;
  GstFlowReturn res;
  GstClockTime last_input_ts = videorate->prev_ts;

  if (videorate->drop_only)
    return count;

  if (!videorate->prevbuf) {
    GST_INFO_OBJECT (videorate, "got EOS before any buffer was received");

    return count;
  }

  GST_DEBUG_OBJECT (videorate, "Pushing buffers to close segment");

  res = GST_FLOW_OK;
  /* fill up to the end of current segment */
  while (res == GST_FLOW_OK
      && gst_video_rate_check_duplicate_to_close_segment (videorate,
          last_input_ts, count < 1)) {
    res =
        gst_video_rate_flush_prev (videorate, count > 0, GST_CLOCK_TIME_NONE,
        FALSE);

    count++;
  }
  GST_DEBUG_OBJECT (videorate, "----> Pushed %d buffers to close segment",
      count);

  return count;
}

/* WORKAROUND: This works around BaseTransform limitation as instead of rolling
 * back caps, we should be able to push caps only when we are sure we are ready
 * to do so. Right now, BaseTransform doesn't let us do anything like that
 * so we rollback to previous caps when strictly required (though we now it
 * might not be so safe).
 *
 * To be used only when wanting to 'close' a segment, this function will reset
 * caps to previous caps, which will match the content of `prevbuf` in that case
 *
 * Returns: The previous GstCaps if we rolled back to previous buffers, NULL
 * otherwise.
 *
 * NOTE: When some caps are returned, we should reset them back after
 * closing the segment is done.
 */
static GstCaps *
gst_video_rate_rollback_to_prev_caps_if_needed (GstVideoRate * videorate)
{
  GstCaps *prev_caps = NULL;

  if (videorate->prev_caps && videorate->prev_caps != videorate->in_caps) {
    if (videorate->in_caps)
      prev_caps = gst_caps_ref (videorate->in_caps);

    GST_DEBUG_OBJECT (videorate, "rollback to previous caps %" GST_PTR_FORMAT,
        prev_caps);

    if (!gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD (videorate),
            gst_event_new_caps (videorate->prev_caps)
        )) {

      GST_WARNING_OBJECT (videorate, "Could not send previous caps to close "
          " segment, not closing it");

      gst_video_rate_swap_prev (videorate, NULL, GST_CLOCK_TIME_NONE);
      videorate->last_ts = GST_CLOCK_TIME_NONE;
      videorate->average = 0;
    }

    gst_clear_caps (&videorate->prev_caps);
  }

  return prev_caps;
}

static gboolean
gst_video_rate_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVideoRate *videorate;

  videorate = GST_VIDEO_RATE (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;
      gint seqnum;
      GstCaps *rolled_back_caps;

      gst_event_copy_segment (event, &segment);
      if (segment.format != GST_FORMAT_TIME)
        goto format_error;

      segment.start = (gint64) (segment.start / videorate->rate);
      segment.position = (gint64) (segment.position / videorate->rate);
      if (GST_CLOCK_TIME_IS_VALID (segment.stop))
        segment.stop = (gint64) (segment.stop / videorate->rate);
      segment.time = (gint64) (segment.time / videorate->rate);

      if (!gst_segment_is_equal (&segment, &videorate->segment)) {
        rolled_back_caps =
            gst_video_rate_rollback_to_prev_caps_if_needed (videorate);

        /* close up the previous segment, if appropriate */
        if (videorate->prevbuf) {
          /* fill up to the end of current segment */
          gint count = gst_video_rate_duplicate_to_close_segment (videorate);
          if (count > 1) {
            videorate->dup += count - 1;
            if (!videorate->silent)
              gst_video_rate_notify_duplicate (videorate);
          }
          /* clean up for the new one; _chain will resume from the new start */
          gst_video_rate_swap_prev (videorate, NULL, 0);
        }

        if (rolled_back_caps) {
          GST_DEBUG_OBJECT (videorate,
              "Resetting rolled back caps %" GST_PTR_FORMAT, rolled_back_caps);
          if (!gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD (videorate),
                  gst_event_new_caps (rolled_back_caps)
              )) {

            GST_WARNING_OBJECT (videorate,
                "Could not resend caps after closing " " segment");

            GST_ELEMENT_ERROR (videorate, CORE, NEGOTIATION,
                ("Could not resend caps after closing segment"), (NULL));
            gst_caps_unref (rolled_back_caps);

            return FALSE;
          }

          gst_caps_unref (rolled_back_caps);
        }
        if (segment.rate < 0)
          videorate->base_ts = segment.stop;
        else
          videorate->base_ts = segment.start;
        videorate->out_frame_count = 0;
        videorate->next_ts = GST_CLOCK_TIME_NONE;
        videorate->last_ts = GST_CLOCK_TIME_NONE;
        gst_buffer_replace (&videorate->prevbuf, NULL);
      }

      gst_segment_copy_into (&segment, &videorate->segment);
      GST_DEBUG_OBJECT (videorate, "updated segment: %" GST_SEGMENT_FORMAT,
          &videorate->segment);
      seqnum = gst_event_get_seqnum (event);
      gst_event_unref (event);
      event = gst_event_new_segment (&segment);
      gst_event_set_seqnum (event, seqnum);

      break;
    }
    case GST_EVENT_SEGMENT_DONE:
    case GST_EVENT_EOS:{
      gint count = 0;
      GstFlowReturn res = GST_FLOW_OK;
      GstCaps *rolled_back_caps;

      GST_DEBUG_OBJECT (videorate, "Got %s",
          gst_event_type_get_name (GST_EVENT_TYPE (event)));

      rolled_back_caps =
          gst_video_rate_rollback_to_prev_caps_if_needed (videorate);

      /* If the segment has a stop position, fill the segment */
      if (GST_CLOCK_TIME_IS_VALID (videorate->segment.stop)) {
        /* fill up to the end of current segment */
        count = gst_video_rate_duplicate_to_close_segment (videorate);
      } else if (!videorate->drop_only && videorate->prevbuf) {
        /* Output at least one frame but if the buffer duration is valid, output
         * enough frames to use the complete buffer duration */
        if (GST_BUFFER_DURATION_IS_VALID (videorate->prevbuf)) {
          GstClockTime end_ts, duration =
              GST_BUFFER_DURATION (videorate->prevbuf);

          if (GST_CLOCK_TIME_IS_VALID
              (videorate->max_closing_segment_duplication_duration))
            duration =
                MIN (videorate->max_closing_segment_duplication_duration,
                duration);

          end_ts = videorate->next_ts + duration;
          while (res == GST_FLOW_OK && ((videorate->segment.rate > 0.0
                      && GST_CLOCK_TIME_IS_VALID (videorate->segment.stop)
                      && GST_CLOCK_TIME_IS_VALID (videorate->next_ts)
                      && videorate->next_ts < end_ts)
                  || count < 1)) {
            res =
                gst_video_rate_flush_prev (videorate, count > 0,
                GST_CLOCK_TIME_NONE, FALSE);
            count++;
          }
        } else {
          /* allow the duration to be invalid as there is no way to infer it if we
           * received a single buffer and not output framerate was set. */
          res =
              gst_video_rate_flush_prev (videorate, FALSE, GST_CLOCK_TIME_NONE,
              TRUE);
          count = 1;
        }
      }

      if (rolled_back_caps) {
        GST_DEBUG_OBJECT (videorate,
            "Resetting rolled back caps %" GST_PTR_FORMAT, rolled_back_caps);

        if (!gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD (videorate),
                gst_event_new_caps (rolled_back_caps)
            )) {

          /* Not erroring out on EOS as it won't be too bad in any case */
          GST_WARNING_OBJECT (videorate, "Could not resend caps after closing "
              " segment on EOS (ignoring the error)");
        }

        gst_caps_unref (rolled_back_caps);
      }

      if (count > 1) {
        videorate->dup += count - 1;
        if (!videorate->silent)
          gst_video_rate_notify_duplicate (videorate);
      } else if (count == 0
          && !GST_CLOCK_TIME_IS_VALID (videorate->segment.stop)) {
        videorate->drop++;
        if (!videorate->silent)
          gst_video_rate_notify_drop (videorate);
      }

      break;
    }
    case GST_EVENT_FLUSH_STOP:
      /* also resets the segment */
      GST_DEBUG_OBJECT (videorate, "Got FLUSH_STOP");
      gst_video_rate_reset (videorate, TRUE);
      break;
    case GST_EVENT_GAP:
      /* no gaps after videorate, ignore the event */
      gst_event_unref (event);
      return TRUE;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);

  /* ERRORS */
format_error:
  {
    GST_WARNING_OBJECT (videorate,
        "Got segment but doesn't have GST_FORMAT_TIME value");
    return FALSE;
  }
}

static gboolean
gst_video_rate_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVideoRate *videorate;
  GstPad *sinkpad;
  gboolean res = FALSE;

  videorate = GST_VIDEO_RATE (trans);
  sinkpad = GST_BASE_TRANSFORM_SINK_PAD (trans);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      gdouble srate;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gint seqnum = gst_event_get_seqnum (event);

      gst_event_parse_seek (event, &srate, NULL, &flags, &start_type, &start,
          &stop_type, &stop);

      start = (gint64) (start * videorate->rate);
      if (GST_CLOCK_TIME_IS_VALID (stop)) {
        stop = (gint64) (stop * videorate->rate);
      }

      gst_event_unref (event);
      event = gst_event_new_seek (srate, GST_FORMAT_TIME,
          flags, start_type, start, stop_type, stop);
      gst_event_set_seqnum (event, seqnum);

      res = gst_pad_push_event (sinkpad, event);
      break;
    }
    case GST_EVENT_QOS:
    {
      GstQOSType type;
      gdouble proportion;
      GstClockTimeDiff diff;
      GstClockTime timestamp;

      gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp);

      if (GST_CLOCK_TIME_IS_VALID (timestamp) && videorate->rate != 1.0) {
        GST_OBJECT_LOCK (trans);
        GST_DEBUG_OBJECT (trans, "Rescaling QoS event taking our rate into"
            "account. Timestamp:  %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT
            " - diff %" G_GINT64_FORMAT "-> %" G_GINT64_FORMAT,
            GST_TIME_ARGS (timestamp),
            GST_TIME_ARGS (videorate->base_ts + ((timestamp -
                        videorate->base_ts) * videorate->rate)), diff,
            (GstClockTimeDiff) (diff * videorate->rate));

        if (videorate->segment.rate < 0.0)
          timestamp =
              (videorate->segment.stop - videorate->base_ts) -
              ((videorate->segment.stop - videorate->base_ts -
                  timestamp) * videorate->rate);
        else
          timestamp =
              videorate->base_ts + ((timestamp -
                  videorate->base_ts) * videorate->rate);

        diff *= videorate->rate;
        GST_OBJECT_UNLOCK (trans);

        gst_event_unref (event);
        event = gst_event_new_qos (type, proportion, diff, timestamp);
      }
      /* Fallthrough */
    }
    default:
      res = gst_pad_push_event (sinkpad, event);
      break;
  }
  return res;
}

static gboolean
gst_video_rate_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstVideoRate *videorate = GST_VIDEO_RATE (trans);
  gboolean res = FALSE;
  GstPad *otherpad;

  otherpad = (direction == GST_PAD_SRC) ?
      GST_BASE_TRANSFORM_SINK_PAD (trans) : GST_BASE_TRANSFORM_SRC_PAD (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      GstClockTime min, max;
      gboolean live;
      guint64 latency;
      guint64 avg_period;
      gboolean drop_only;
      GstPad *peer;

      GST_OBJECT_LOCK (videorate);
      avg_period = videorate->average_period_set;
      drop_only = videorate->drop_only;
      GST_OBJECT_UNLOCK (videorate);

      if (avg_period == 0 && (peer = gst_pad_get_peer (otherpad))) {
        if ((res = gst_pad_query (peer, query))) {
          gst_query_parse_latency (query, &live, &min, &max);

          GST_DEBUG_OBJECT (videorate, "Peer latency: min %"
              GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
              GST_TIME_ARGS (min), GST_TIME_ARGS (max));

          /* Drop only has no latency, other modes have one frame latency */
          if (!drop_only && videorate->from_rate_numerator != 0) {
            /* add latency. We don't really know since we hold on to the frames
             * until we get a next frame, which can be anything. We assume
             * however that this will take from_rate time. */
            latency = gst_util_uint64_scale (GST_SECOND,
                videorate->from_rate_denominator,
                videorate->from_rate_numerator);
          } else {
            /* no input framerate, we don't know */
            latency = 0;
          }

          GST_DEBUG_OBJECT (videorate, "Our latency: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (latency));

          min += latency;
          if (max != -1)
            max += latency;

          GST_DEBUG_OBJECT (videorate, "Calculated total latency : min %"
              GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
              GST_TIME_ARGS (min), GST_TIME_ARGS (max));

          gst_query_set_latency (query, live, min, max);
        }
        gst_object_unref (peer);
        break;
      }
      /* Simple fall back if we don't have a latency or a peer that we
       * can ask about its latency yet.. */
      res =
          GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
          query);
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      gint64 duration;
      gdouble rate;

      res =
          GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
          query);

      if (!res)
        break;

      GST_OBJECT_LOCK (videorate);
      rate = videorate->pending_rate;
      GST_OBJECT_UNLOCK (videorate);

      if (rate == 1.0)
        break;

      gst_query_parse_duration (query, &format, &duration);

      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (videorate, "not TIME format");
        break;
      }
      GST_LOG_OBJECT (videorate, "upstream duration: %" G_GINT64_FORMAT,
          duration);
      /* Shouldn't this be a multiplication if the direction is downstream? */
      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        duration = (gint64) (duration / rate);
      }
      GST_LOG_OBJECT (videorate, "our duration: %" G_GINT64_FORMAT, duration);
      gst_query_set_duration (query, format, duration);
      break;
    }
    case GST_QUERY_POSITION:
    {
      GstFormat dst_format;
      gint64 dst_value;
      gdouble rate;

      GST_OBJECT_LOCK (videorate);
      rate = videorate->rate;
      GST_OBJECT_UNLOCK (videorate);

      gst_query_parse_position (query, &dst_format, NULL);

      if (dst_format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (videorate, "not TIME format");
        break;
      }
      /* Shouldn't this be a multiplication if the direction is downstream? */
      dst_value =
          (gint64) (gst_segment_to_stream_time (&videorate->segment,
              GST_FORMAT_TIME, videorate->last_ts / rate));
      GST_LOG_OBJECT (videorate, "our position: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (dst_value));
      gst_query_set_position (query, dst_format, dst_value);
      res = TRUE;
      break;
    }
    default:
      res =
          GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
          query);
      break;
  }

  return res;
}

static gboolean
gst_video_rate_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstBaseTransformClass *klass = GST_BASE_TRANSFORM_CLASS (parent_class);
  gboolean res;

  /* We should always be passthrough */
  g_return_val_if_fail (decide_query == NULL, FALSE);

  res = klass->propose_allocation (trans, NULL, query);

  if (res) {
    guint i = 0;
    guint n_allocation;
    guint down_min = 0;

    n_allocation = gst_query_get_n_allocation_pools (query);

    while (i < n_allocation) {
      GstBufferPool *pool = NULL;
      guint size, min, max;

      gst_query_parse_nth_allocation_pool (query, i, &pool, &size, &min, &max);

      if (max != 0 && min == max) {
        if (pool)
          gst_object_unref (pool);
        gst_query_remove_nth_allocation_pool (query, i);
        n_allocation--;
        down_min = MAX (min, down_min);
        continue;
      }

      gst_query_set_nth_allocation_pool (query, i, pool, size, min + 1, max);
      if (pool)
        gst_object_unref (pool);
      i++;
    }

    if (n_allocation == 0) {
      GstCaps *caps;
      GstVideoInfo info;

      gst_query_parse_allocation (query, &caps, NULL);
      gst_video_info_from_caps (&info, caps);

      gst_query_add_allocation_pool (query, NULL, info.size, down_min + 1, 0);
    }
  }

  return res;
}

static GstFlowReturn
gst_video_rate_trans_ip_max_avg (GstVideoRate * videorate, GstBuffer * buf)
{
  GstClockTime ts = GST_BUFFER_PTS (buf);

  videorate->in++;

  if (!GST_CLOCK_TIME_IS_VALID (ts) || videorate->wanted_diff == 0)
    goto push;

  /* drop frames if they exceed our output rate */
  if (GST_CLOCK_TIME_IS_VALID (videorate->last_ts)) {
    GstClockTimeDiff diff =
        videorate->segment.rate <
        0 ? videorate->last_ts - ts : ts - videorate->last_ts;

    /* Drop buffer if its early compared to the desired frame rate and
     * the current average is higher than the desired average
     */
    if (diff < videorate->wanted_diff &&
        videorate->average < videorate->wanted_diff)
      goto drop;

    /* Update average */
    if (videorate->average) {
      GstClockTimeDiff wanted_diff;

      if (G_LIKELY (videorate->average_period > videorate->wanted_diff))
        wanted_diff = videorate->wanted_diff;
      else
        wanted_diff = videorate->average_period * 10;

      videorate->average =
          gst_util_uint64_scale_round (videorate->average,
          videorate->average_period - wanted_diff,
          videorate->average_period) +
          gst_util_uint64_scale_round (diff, wanted_diff,
          videorate->average_period);
    } else {
      videorate->average = diff;
    }
  }

  videorate->last_ts = ts;

push:
  videorate->out++;
  return GST_FLOW_OK;

drop:
  if (!videorate->silent)
    gst_video_rate_notify_drop (videorate);
  return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

/* Check if downstream forces variable framerate (0/1) and if
 * it is the case, use variable framerate ourself
 * Otherwise compute the framerate from the 2 buffers that we
 * have already received and make use of it as wanted framerate
 */
static void
gst_video_rate_check_variable_rate (GstVideoRate * videorate,
    GstBuffer * buffer)
{
  GstStructure *st;
  gint fps_d, fps_n;
  GstCaps *srcpadcaps, *tmpcaps, *downstream_caps;
  GstPad *pad = NULL;

  srcpadcaps =
      gst_pad_get_current_caps (GST_BASE_TRANSFORM_SRC_PAD (videorate));

  gst_video_guess_framerate (GST_BUFFER_PTS (buffer) -
      GST_BUFFER_PTS (videorate->prevbuf), &fps_n, &fps_d);

  tmpcaps = gst_caps_copy (srcpadcaps);
  st = gst_caps_get_structure (tmpcaps, 0);
  gst_structure_set (st, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  gst_caps_unref (srcpadcaps);

  pad = gst_pad_get_peer (GST_BASE_TRANSFORM_SRC_PAD (videorate));
  downstream_caps = gst_pad_query_caps (pad, NULL);
  if (pad && !gst_caps_can_intersect (tmpcaps, downstream_caps)) {
    videorate->force_variable_rate = TRUE;
    gst_caps_unref (downstream_caps);
    GST_DEBUG_OBJECT (videorate, "Downstream forces variable framerate"
        " respecting it");

    goto done;
  }
  gst_caps_unref (downstream_caps);

  videorate->to_rate_numerator = fps_n;
  videorate->to_rate_denominator = fps_d;

  GST_INFO_OBJECT (videorate, "Computed framerate to %d/%d",
      videorate->to_rate_numerator, videorate->to_rate_denominator);

  videorate->updating_caps = TRUE;
  gst_base_transform_update_src_caps (GST_BASE_TRANSFORM (videorate), tmpcaps);

  /* also reconfigure sink so that buffer pool can be updated again */
  gst_base_transform_reconfigure_sink (GST_BASE_TRANSFORM (videorate));

done:
  gst_caps_unref (tmpcaps);
  if (pad)
    gst_object_unref (pad);
}

static gboolean
gst_video_rate_switch_mode_if_needed (GstVideoRate * videorate)
{
  gboolean switch_mode;
  GstClockTime avg_period;
  gboolean skip = FALSE;

  GST_OBJECT_LOCK (videorate);
  avg_period = videorate->average_period_set;
  GST_OBJECT_UNLOCK (videorate);

  /* MT-safe switching between modes */
  if (G_LIKELY (avg_period == videorate->average_period))
    return skip;

  switch_mode = (avg_period == 0 || videorate->average_period == 0);

  if (!switch_mode)
    return skip;


  videorate->average_period = avg_period;
  videorate->last_ts = GST_CLOCK_TIME_NONE;
  if (avg_period) {
    /* enabling average mode */
    videorate->average = 0;
    /* make sure no cached buffers from regular mode are left */
    gst_video_rate_swap_prev (videorate, NULL, 0);
  } else {
    /* enable regular mode */
    videorate->next_ts = GST_CLOCK_TIME_NONE;
    skip = TRUE;
  }

  /* max averaging mode has no latency, normal mode does */
  gst_element_post_message (GST_ELEMENT (videorate),
      gst_message_new_latency (GST_OBJECT (videorate)));

  return skip;
}

static gboolean
gst_video_rate_do_max_duplicate (GstVideoRate * videorate, GstBuffer * buffer,
    GstClockTime intime, GstClockTime prevtime, gint * count)
{
  if (videorate->max_duplication_time <= 0)
    return TRUE;

  /* We already know that intime and prevtime are not out of order, based
   * on the previous condition. Using ABS in case rate < 0, in which case
   * the order is reversed. */
  if (ABS (GST_CLOCK_DIFF (intime, prevtime)) > videorate->max_duplication_time) {
    GST_DEBUG_OBJECT (videorate,
        "The new buffer (%" GST_TIME_FORMAT
        ") is further away from previous buffer (%" GST_TIME_FORMAT
        ") than max-duplication-time (%" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (intime), GST_TIME_ARGS (prevtime),
        GST_TIME_ARGS (videorate->max_duplication_time));
    /* First send out enough buffers to actually reach the time of the
     * previous buffer */
    if (videorate->segment.rate < 0.0) {
      while (videorate->next_ts > prevtime) {
        gst_video_rate_flush_prev (videorate, *count > 0, GST_CLOCK_TIME_NONE,
            FALSE);
        *count += 1;
      }
    } else {
      while (videorate->next_ts <= prevtime) {
        gst_video_rate_flush_prev (videorate, *count > 0, GST_CLOCK_TIME_NONE,
            FALSE);
        *count += 1;
      }
    }

    if (*count > 1) {
      videorate->dup += *count - 1;
      if (!videorate->silent)
        gst_video_rate_notify_duplicate (videorate);
    }

    /* The gap between the two buffers is too large. Don't fill it, just
     * let a discont through */
    videorate->discont = TRUE;

    if (videorate->segment.rate < 0.0) {
      videorate->base_ts -= prevtime - intime;
    } else {
      videorate->base_ts += intime - prevtime;
    }
    videorate->next_ts = intime;
    /* Swap in new buffer and get rid of old buffer so that starting with
     * the next input buffer we output from the new position */
    gst_video_rate_swap_prev (videorate, buffer, intime);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_video_rate_apply_pending_rate (GstVideoRate * videorate)
{
  gboolean ret = FALSE;

  GST_OBJECT_LOCK (videorate);
  if (videorate->pending_rate == videorate->rate)
    goto done;

  ret = TRUE;
  if (videorate->segment.rate < 0)
    videorate->base_ts -= gst_util_uint64_scale (videorate->out_frame_count,
        videorate->to_rate_denominator * GST_SECOND,
        videorate->to_rate_numerator);
  else
    videorate->base_ts += gst_util_uint64_scale (videorate->out_frame_count,
        videorate->to_rate_denominator * GST_SECOND,
        videorate->to_rate_numerator);
  videorate->rate = videorate->pending_rate;
  videorate->out_frame_count = 0;

done:
  GST_OBJECT_UNLOCK (videorate);

  return ret;
}

static GstFlowReturn
gst_video_rate_transform_ip (GstBaseTransform * trans, GstBuffer * buffer)
{
  GstVideoRate *videorate;
  GstFlowReturn res = GST_BASE_TRANSFORM_FLOW_DROPPED;
  GstClockTime in_ts, in_dur, last_ts;
  gboolean skip;

  videorate = GST_VIDEO_RATE (trans);

  if (videorate->prev_caps != videorate->in_caps) {
    /* After caps where set we didn't reset the state so we could close
     * the segment from previous caps if necessary, we got a buffer after the
     * new caps so we can reset now */
    GST_DEBUG_OBJECT (videorate, "Clearing old buffers now that we had a buffer"
        " after receiving caps");
    gst_video_rate_swap_prev (videorate, NULL, GST_CLOCK_TIME_NONE);
    gst_clear_caps (&videorate->prev_caps);
    videorate->average = 0;
  }

  /* make sure the denominators are not 0 */
  if (videorate->from_rate_denominator == 0 ||
      videorate->to_rate_denominator == 0)
    goto not_negotiated;

  if (videorate->to_rate_numerator == 0 && videorate->prevbuf &&
      !videorate->force_variable_rate) {
    if (!GST_BUFFER_PTS_IS_VALID (buffer) ||
        !GST_BUFFER_PTS_IS_VALID (videorate->prevbuf)) {
      GST_ELEMENT_ERROR (videorate, STREAM, FAILED, (NULL),
          ("videorate requires a non-variable framerate on the output caps or the"
              " two first consecutive buffers to have valid timestamps to guess the"
              " framerate."));
      return GST_FLOW_ERROR;
    }
    gst_video_rate_check_variable_rate (videorate, buffer);
  }

  skip = gst_video_rate_switch_mode_if_needed (videorate);

  if (videorate->average_period > 0)
    return gst_video_rate_trans_ip_max_avg (videorate, buffer);

  gst_video_rate_apply_pending_rate (videorate);
  in_ts = GST_BUFFER_PTS (buffer);
  in_dur = GST_BUFFER_DURATION (buffer);

  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (in_ts))) {
    /* For reverse playback, we need all input timestamps as we can't
     * guess from the previous buffers timestamp and duration */
    if (G_UNLIKELY (videorate->segment.rate < 0.0))
      goto invalid_buffer;
    in_ts = videorate->last_ts;
    if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (in_ts)))
      goto invalid_buffer;
  }

  /* get the time of the next expected buffer timestamp, we use this when the
   * next buffer has -1 as a timestamp */
  last_ts = videorate->last_ts;
  videorate->last_ts = in_ts;
  if (GST_CLOCK_TIME_IS_VALID (in_dur) && videorate->segment.rate > 0.0)
    videorate->last_ts += in_dur;

  GST_DEBUG_OBJECT (videorate, "got buffer with timestamp %" GST_TIME_FORMAT,
      GST_TIME_ARGS (in_ts));

  /* we need to have two buffers to compare */
  if (videorate->prevbuf == NULL || videorate->drop_only) {
    /* We can calculate the duration of the buffer here if not given for
     * reverse playback. We need this later */
    if (videorate->segment.rate < 0.0 && !GST_BUFFER_DURATION_IS_VALID (buffer)) {
      /* As we require valid timestamps all the time for reverse playback, we either
       * have a valid last_ts or we're at the very first buffer. */
      if (!GST_CLOCK_TIME_IS_VALID (last_ts))
        GST_BUFFER_DURATION (buffer) = videorate->segment.stop - in_ts;
      else
        GST_BUFFER_DURATION (buffer) = last_ts - in_ts;
    }

    gst_video_rate_swap_prev (videorate, buffer, in_ts);
    videorate->in++;
    if (!GST_CLOCK_TIME_IS_VALID (videorate->next_ts)) {
      /* new buffer, we expect to output a buffer that matches the first
       * timestamp in the segment */
      if (videorate->skip_to_first || skip) {
        videorate->base_ts = in_ts;
        videorate->next_ts = in_ts;
        videorate->out_frame_count = 0;
      } else {
        if (videorate->segment.rate < 0.0) {
          if (videorate->to_rate_numerator) {
            GstClockTime frame_duration = gst_util_uint64_scale (1,
                videorate->to_rate_denominator * GST_SECOND,
                videorate->to_rate_numerator);

            videorate->next_ts = videorate->segment.stop;

            if (videorate->next_ts > frame_duration)
              videorate->next_ts =
                  MAX (videorate->segment.start,
                  videorate->next_ts - frame_duration);
            else
              videorate->next_ts = videorate->segment.start;
          } else {
            /* What else can we do? */
            videorate->next_ts = in_ts;
          }
        } else {
          videorate->next_ts = videorate->segment.start;
        }
      }
    }

    /* In drop-only mode we can already decide here if we should output the
     * current frame or drop it because it's coming earlier than our minimum
     * allowed frame period. This also keeps latency down to 0 frames
     */
    if (videorate->drop_only) {
      if ((videorate->segment.rate > 0.0 && in_ts >= videorate->next_ts) ||
          (videorate->segment.rate < 0.0 && in_ts <= videorate->next_ts)) {
        GstFlowReturn r;

        /* The buffer received from basetransform is guaranteed to be writable.
         * It just needs to be reffed so the buffer won't be consumed once pushed and
         * GstBaseTransform can get its reference back. */
        if ((r = gst_video_rate_push_buffer (videorate,
                    gst_buffer_ref (buffer), FALSE,
                    GST_CLOCK_TIME_NONE, FALSE)) != GST_FLOW_OK) {
          res = r;
          goto done;
        }
      } else {
        videorate->drop++;
      }
      /* No need to keep the buffer around for longer */
      gst_buffer_replace (&videorate->prevbuf, NULL);
    }
  } else {
    GstClockTime prev_ts;
    gint count = 0;
    gint64 diff1 = 0, diff2 = 0;

    prev_ts = videorate->prev_ts;

    GST_LOG_OBJECT (videorate,
        "BEGINNING prev buf %" GST_TIME_FORMAT " new buf %" GST_TIME_FORMAT
        " outgoing ts %" GST_TIME_FORMAT, GST_TIME_ARGS (prev_ts),
        GST_TIME_ARGS (in_ts), GST_TIME_ARGS (videorate->next_ts));

    videorate->in++;

    /* drop new buffer if it's before previous one */
    if ((videorate->segment.rate > 0.0 && in_ts < prev_ts) ||
        (videorate->segment.rate < 0.0 && in_ts > prev_ts)) {
      GST_DEBUG_OBJECT (videorate,
          "The new buffer (%" GST_TIME_FORMAT
          ") is before the previous buffer (%"
          GST_TIME_FORMAT "). Dropping new buffer.",
          GST_TIME_ARGS (in_ts), GST_TIME_ARGS (prev_ts));
      videorate->drop++;
      if (!videorate->silent)
        gst_video_rate_notify_drop (videorate);
      goto done;
    }

    if (!gst_video_rate_do_max_duplicate (videorate, buffer, in_ts, prev_ts,
            &count))
      goto done;

    /* got 2 buffers, see which one is the best */
    do {
      GstClockTime next_ts;

      if (gst_video_rate_apply_pending_rate (videorate))
        goto done;

      if (videorate->segment.rate < 0.0) {
        /* Make sure that we have a duration for this buffer. The previous
         * buffer already has a duration given by either exactly this code,
         * or the code above for the very first buffer */
        g_assert (GST_BUFFER_DURATION_IS_VALID (videorate->prevbuf));
        if (!GST_BUFFER_DURATION_IS_VALID (buffer))
          GST_BUFFER_DURATION (buffer) = prev_ts > in_ts ? prev_ts - in_ts : 0;
      } else {
        /* Make sure that we have a duration for previous buffer */
        if (!GST_BUFFER_DURATION_IS_VALID (videorate->prevbuf))
          GST_BUFFER_DURATION (videorate->prevbuf) =
              in_ts > prev_ts ? in_ts - prev_ts : 0;
      }

#ifndef ABSDIFF
#define ABSDIFF(a, b) (((a) > (b)) ? (a) - (b) : (b) - (a))
#endif

      /* take absolute diffs */
      if (videorate->segment.rate < 0.0) {
        GstClockTime next_end_ts;
        GstClockTime prev_endtime;
        GstClockTime in_endtime;

        next_ts = videorate->next_ts;

        if (!GST_CLOCK_TIME_IS_VALID (next_ts)) {
          GST_DEBUG_OBJECT (videorate, "Already reached segment start,"
              "ignoring buffer");
          break;
        }

        prev_endtime = prev_ts + GST_BUFFER_DURATION (videorate->prevbuf);
        in_endtime = in_ts + GST_BUFFER_DURATION (buffer);

        if (videorate->to_rate_numerator) {
          GstClockTime frame_duration = gst_util_uint64_scale (1,
              videorate->to_rate_denominator * GST_SECOND,
              videorate->to_rate_numerator);
          next_end_ts = next_ts + frame_duration;
        } else {
          next_end_ts = next_ts + GST_BUFFER_DURATION (videorate->prevbuf);
        }

        next_ts = videorate->base_ts - (
            (videorate->base_ts - next_ts) * videorate->rate);
        next_end_ts = videorate->base_ts - (MAX (0,
                (videorate->base_ts - next_end_ts)) * videorate->rate);

        diff1 = ABSDIFF (prev_endtime, next_end_ts);
        diff2 = ABSDIFF (in_endtime, next_end_ts);

        GST_LOG_OBJECT (videorate,
            "diff with prev %" GST_TIME_FORMAT " diff with new %"
            GST_TIME_FORMAT " outgoing ts %" GST_TIME_FORMAT,
            GST_TIME_ARGS (diff1), GST_TIME_ARGS (diff2),
            GST_TIME_ARGS (next_end_ts));
      } else {
        next_ts =
            videorate->base_ts + ((videorate->next_ts -
                videorate->base_ts) * videorate->rate);

        diff1 = ABSDIFF (prev_ts, next_ts);
        diff2 = ABSDIFF (in_ts, next_ts);

        GST_LOG_OBJECT (videorate,
            "diff with prev %" GST_TIME_FORMAT " diff with new %"
            GST_TIME_FORMAT " outgoing ts %" GST_TIME_FORMAT,
            GST_TIME_ARGS (diff1), GST_TIME_ARGS (diff2),
            GST_TIME_ARGS (next_ts));
      }

      /* output first one when its the best */
      if (diff1 <= diff2) {
        GstFlowReturn r;
        count++;

        /* on error the _flush function posted a warning already */
        if ((r = gst_video_rate_flush_prev (videorate,
                    count > 1, in_ts, FALSE)) != GST_FLOW_OK) {
          res = r;
          goto done;
        }
      }

      /* continue while the first one was the best, if they were equal avoid
       * going into an infinite loop */
    }
    while (diff1 < diff2);

    /* if we outputted the first buffer more then once, we have dups */
    if (count > 1) {
      videorate->dup += count - 1;
      if (!videorate->silent)
        gst_video_rate_notify_duplicate (videorate);
    }
    /* if we didn't output the first buffer, we have a drop */
    else if (count == 0) {
      videorate->drop++;

      if (!videorate->silent)
        gst_video_rate_notify_drop (videorate);

      GST_LOG_OBJECT (videorate,
          "new is best, old never used, drop, outgoing ts %"
          GST_TIME_FORMAT, GST_TIME_ARGS (videorate->next_ts));
    }
    GST_LOG_OBJECT (videorate,
        "END, putting new in old, diff1 %" GST_TIME_FORMAT
        ", diff2 %" GST_TIME_FORMAT ", next_ts %" GST_TIME_FORMAT
        ", in %" G_GUINT64_FORMAT ", out %" G_GUINT64_FORMAT ", drop %"
        G_GUINT64_FORMAT ", dup %" G_GUINT64_FORMAT, GST_TIME_ARGS (diff1),
        GST_TIME_ARGS (diff2), GST_TIME_ARGS (videorate->next_ts),
        videorate->in, videorate->out, videorate->drop, videorate->dup);

    /* swap in new one when it's the best */
    gst_video_rate_swap_prev (videorate, buffer, in_ts);
  }
done:
  return res;

  /* ERRORS */
not_negotiated:
  {
    GST_WARNING_OBJECT (videorate, "no framerate negotiated");
    res = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }

invalid_buffer:
  {
    GST_WARNING_OBJECT (videorate,
        "Got buffer with GST_CLOCK_TIME_NONE timestamp, discarding it");
    res = GST_BASE_TRANSFORM_FLOW_DROPPED;
    goto done;
  }
}

static gboolean
gst_video_rate_start (GstBaseTransform * trans)
{
  gst_video_rate_reset (GST_VIDEO_RATE (trans), FALSE);
  return TRUE;
}

static gboolean
gst_video_rate_stop (GstBaseTransform * trans)
{
  gst_video_rate_reset (GST_VIDEO_RATE (trans), FALSE);
  return TRUE;
}

static void
gst_videorate_update_duration (GstVideoRate * videorate)
{
  GstMessage *m;

  m = gst_message_new_duration_changed (GST_OBJECT (videorate));
  gst_element_post_message (GST_ELEMENT (videorate), m);
}

static void
gst_video_rate_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstVideoRate *videorate = GST_VIDEO_RATE (object);
  gboolean latency_changed = FALSE;

  GST_OBJECT_LOCK (videorate);
  switch (prop_id) {
    case PROP_SILENT:
      videorate->silent = g_value_get_boolean (value);
      break;
    case PROP_NEW_PREF:
      videorate->new_pref = g_value_get_double (value);
      break;
    case PROP_SKIP_TO_FIRST:
      videorate->skip_to_first = g_value_get_boolean (value);
      break;
    case PROP_DROP_ONLY:{
      gboolean new_value = g_value_get_boolean (value);

      /* Latency changes if we switch drop-only mode */
      latency_changed = new_value != videorate->drop_only;
      videorate->drop_only = g_value_get_boolean (value);
      goto reconfigure;
    }
    case PROP_AVERAGE_PERIOD:
      videorate->average_period_set = g_value_get_uint64 (value);
      break;
    case PROP_MAX_RATE:
      g_atomic_int_set (&videorate->max_rate, g_value_get_int (value));
      goto reconfigure;
    case PROP_RATE:
      videorate->pending_rate = g_value_get_double (value);
      GST_OBJECT_UNLOCK (videorate);

      gst_videorate_update_duration (videorate);
      return;
    case PROP_MAX_DUPLICATION_TIME:
      videorate->max_duplication_time = g_value_get_uint64 (value);
      break;
    case PROP_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION:
      videorate->max_closing_segment_duplication_duration =
          g_value_get_uint64 (value);
      break;
    case PROP_DROP_OUT_OF_SEGMENT:{
      videorate->drop_out_of_segment = g_value_get_boolean (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (videorate);

  return;

reconfigure:
  GST_OBJECT_UNLOCK (videorate);
  gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (videorate));

  if (latency_changed) {
    gst_element_post_message (GST_ELEMENT (videorate),
        gst_message_new_latency (GST_OBJECT (videorate)));
  }
}

static void
gst_video_rate_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstVideoRate *videorate = GST_VIDEO_RATE (object);

  GST_OBJECT_LOCK (videorate);
  switch (prop_id) {
    case PROP_IN:
      g_value_set_uint64 (value, videorate->in);
      break;
    case PROP_OUT:
      g_value_set_uint64 (value, videorate->out);
      break;
    case PROP_DUP:
      g_value_set_uint64 (value, videorate->dup);
      break;
    case PROP_DROP:
      g_value_set_uint64 (value, videorate->drop);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, videorate->silent);
      break;
    case PROP_NEW_PREF:
      g_value_set_double (value, videorate->new_pref);
      break;
    case PROP_SKIP_TO_FIRST:
      g_value_set_boolean (value, videorate->skip_to_first);
      break;
    case PROP_DROP_ONLY:
      g_value_set_boolean (value, videorate->drop_only);
      break;
    case PROP_AVERAGE_PERIOD:
      g_value_set_uint64 (value, videorate->average_period_set);
      break;
    case PROP_MAX_RATE:
      g_value_set_int (value, g_atomic_int_get (&videorate->max_rate));
      break;
    case PROP_RATE:
      g_value_set_double (value, videorate->pending_rate);
      break;
    case PROP_MAX_DUPLICATION_TIME:
      g_value_set_uint64 (value, videorate->max_duplication_time);
      break;
    case PROP_MAX_CLOSING_SEGMENT_DUPLICATION_DURATION:
      g_value_set_uint64 (value,
          videorate->max_closing_segment_duplication_duration);
      break;
    case PROP_DROP_OUT_OF_SEGMENT:
      g_value_set_boolean (value, videorate->drop_out_of_segment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (videorate);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (video_rate_debug, "videorate", 0,
      "VideoRate stream fixer");

  return GST_ELEMENT_REGISTER (videorate, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    videorate,
    "Adjusts video frames",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
