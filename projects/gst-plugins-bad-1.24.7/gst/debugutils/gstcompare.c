/* GStreamer Element
 *
 * Copyright 2011 Collabora Ltd.
 *  @author: Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright 2011 Nokia Corp.
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
#include <string.h>

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/video/video.h>

#include "gstdebugutilsbadelements.h"
#include "gstcompare.h"

GST_DEBUG_CATEGORY_STATIC (compare_debug);
#define GST_CAT_DEFAULT   compare_debug


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate check_sink_factory =
GST_STATIC_PAD_TEMPLATE ("check",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum GstCompareMethod
{
  GST_COMPARE_METHOD_MEM,
  GST_COMPARE_METHOD_MAX,
  GST_COMPARE_METHOD_SSIM
};

#define GST_COMPARE_METHOD_TYPE (gst_compare_method_get_type())
static GType
gst_compare_method_get_type (void)
{
  static GType method_type = 0;

  static const GEnumValue method_types[] = {
    {GST_COMPARE_METHOD_MEM, "Memory", "mem"},
    {GST_COMPARE_METHOD_MAX, "Maximum metric", "max"},
    {GST_COMPARE_METHOD_SSIM, "SSIM (raw video)", "ssim"},
    {0, NULL, NULL}
  };

  if (!method_type) {
    method_type = g_enum_register_static ("GstCompareMethod", method_types);
  }
  return method_type;
}

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_META,
  PROP_OFFSET_TS,
  PROP_METHOD,
  PROP_THRESHOLD,
  PROP_UPPER
};

#define DEFAULT_META             GST_BUFFER_COPY_ALL
#define DEFAULT_OFFSET_TS        FALSE
#define DEFAULT_METHOD           GST_COMPARE_METHOD_MEM
#define DEFAULT_THRESHOLD        0
#define DEFAULT_UPPER            TRUE

static void gst_compare_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_compare_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void gst_compare_reset (GstCompare * overlay);

static gboolean gst_compare_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_compare_collect_pads (GstCollectPads * cpads,
    GstCompare * comp);

static GstStateChangeReturn gst_compare_change_state (GstElement * element,
    GstStateChange transition);

#define gst_compare_parent_class parent_class
G_DEFINE_TYPE (GstCompare, gst_compare, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (compare, "compare",
    GST_RANK_NONE, gst_compare_get_type ());

static void
gst_compare_finalize (GObject * object)
{
  GstCompare *comp = GST_COMPARE (object);

  gst_object_unref (comp->cpads);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_compare_class_init (GstCompareClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (compare_debug, "compare", 0, "Compare buffers");

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_compare_change_state);

  gobject_class->set_property = gst_compare_set_property;
  gobject_class->get_property = gst_compare_get_property;
  gobject_class->finalize = gst_compare_finalize;

  g_object_class_install_property (gobject_class, PROP_META,
      g_param_spec_flags ("meta", "Compare Meta",
          "Indicates which metadata should be compared",
          gst_buffer_copy_flags_get_type (), DEFAULT_META,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSET_TS,
      g_param_spec_boolean ("offset-ts", "Offsets Timestamps",
          "Consider OFFSET and OFFSET_END part of timestamp metadata",
          DEFAULT_OFFSET_TS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method", "Content Compare Method",
          "Method to compare buffer content",
          GST_COMPARE_METHOD_TYPE, DEFAULT_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_double ("threshold", "Content Threshold",
          "Threshold beyond which to consider content different as determined by content-method",
          0, G_MAXDOUBLE, DEFAULT_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_UPPER,
      g_param_spec_boolean ("upper", "Threshold Upper Bound",
          "Whether threshold value is upper bound or lower bound for difference measure",
          DEFAULT_UPPER, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &check_sink_factory);
  gst_element_class_set_static_metadata (gstelement_class, "Compare buffers",
      "Filter/Debug", "Compares incoming buffers",
      "Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>");

  gst_type_mark_as_plugin_api (GST_COMPARE_METHOD_TYPE, 0);
}

static void
gst_compare_init (GstCompare * comp)
{
  comp->cpads = gst_collect_pads_new ();
  gst_collect_pads_set_function (comp->cpads,
      (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (gst_compare_collect_pads),
      comp);

  comp->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  GST_PAD_SET_PROXY_CAPS (comp->sinkpad);
  gst_element_add_pad (GST_ELEMENT (comp), comp->sinkpad);

  comp->checkpad =
      gst_pad_new_from_static_template (&check_sink_factory, "check");
  gst_pad_set_query_function (comp->checkpad, gst_compare_query);
  gst_element_add_pad (GST_ELEMENT (comp), comp->checkpad);

  gst_collect_pads_add_pad (comp->cpads, comp->sinkpad,
      sizeof (GstCollectData), NULL, TRUE);
  gst_collect_pads_add_pad (comp->cpads, comp->checkpad,
      sizeof (GstCollectData), NULL, TRUE);

  comp->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_query_function (comp->srcpad, gst_compare_query);
  gst_element_add_pad (GST_ELEMENT (comp), comp->srcpad);

  /* init properties */
  comp->meta = DEFAULT_META;
  comp->offset_ts = DEFAULT_OFFSET_TS;
  comp->method = DEFAULT_METHOD;
  comp->threshold = DEFAULT_THRESHOLD;
  comp->upper = DEFAULT_UPPER;

  gst_compare_reset (comp);
}

static void
gst_compare_reset (GstCompare * comp)
{
}

static gboolean
gst_compare_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstCompare *comp;
  GstPad *otherpad;

  comp = GST_COMPARE (parent);
  otherpad = (pad == comp->srcpad ? comp->sinkpad : comp->srcpad);

  return gst_pad_peer_query (otherpad, query);
}

static void
gst_compare_meta (GstCompare * comp, GstBuffer * buf1, GstCaps * caps1,
    GstBuffer * buf2, GstCaps * caps2)
{
  gint flags = 0;

  if (comp->meta & GST_BUFFER_COPY_FLAGS) {
    if (GST_BUFFER_FLAGS (buf1) != GST_BUFFER_FLAGS (buf2)) {
      flags |= GST_BUFFER_COPY_FLAGS;
      GST_DEBUG_OBJECT (comp, "flags %d != flags %d", GST_BUFFER_FLAGS (buf1),
          GST_BUFFER_FLAGS (buf2));
    }
  }
  if (comp->meta & GST_BUFFER_COPY_TIMESTAMPS) {
    if (GST_BUFFER_TIMESTAMP (buf1) != GST_BUFFER_TIMESTAMP (buf2)) {
      flags |= GST_BUFFER_COPY_TIMESTAMPS;
      GST_DEBUG_OBJECT (comp,
          "ts %" GST_TIME_FORMAT " != ts %" GST_TIME_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf1)),
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf2)));
    }
    if (GST_BUFFER_DURATION (buf1) != GST_BUFFER_DURATION (buf2)) {
      flags |= GST_BUFFER_COPY_TIMESTAMPS;
      GST_DEBUG_OBJECT (comp,
          "dur %" GST_TIME_FORMAT " != dur %" GST_TIME_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_DURATION (buf1)),
          GST_TIME_ARGS (GST_BUFFER_DURATION (buf2)));
    }
    if (comp->offset_ts) {
      if (GST_BUFFER_OFFSET (buf1) != GST_BUFFER_OFFSET (buf2)) {
        flags |= GST_BUFFER_COPY_TIMESTAMPS;
        GST_DEBUG_OBJECT (comp,
            "offset %" G_GINT64_FORMAT " != offset %" G_GINT64_FORMAT,
            GST_BUFFER_OFFSET (buf1), GST_BUFFER_OFFSET (buf2));
      }
      if (GST_BUFFER_OFFSET_END (buf1) != GST_BUFFER_OFFSET_END (buf2)) {
        flags |= GST_BUFFER_COPY_TIMESTAMPS;
        GST_DEBUG_OBJECT (comp,
            "offset_end %" G_GINT64_FORMAT " != offset_end %" G_GINT64_FORMAT,
            GST_BUFFER_OFFSET_END (buf1), GST_BUFFER_OFFSET_END (buf2));
      }
    }
  }
#if 0
  /* FIXME ?? */
  if (comp->meta & GST_BUFFER_COPY_CAPS) {
    if (!gst_caps_is_equal (caps1, caps2)) {
      flags |= GST_BUFFER_COPY_CAPS;
      GST_DEBUG_OBJECT (comp,
          "caps %" GST_PTR_FORMAT " != caps %" GST_PTR_FORMAT, caps1, caps2);
    }
  }
#endif

  /* signal mismatch by debug and message */
  if (flags) {
    GST_WARNING_OBJECT (comp, "buffers %p and %p failed metadata match %d",
        buf1, buf2, flags);

    gst_element_post_message (GST_ELEMENT (comp),
        gst_message_new_element (GST_OBJECT (comp),
            gst_structure_new ("delta", "meta", G_TYPE_INT, flags, NULL)));
  }
}

/* when comparing contents, it is already ensured sizes are equal */

static gint
gst_compare_mem (GstCompare * comp, GstBuffer * buf1, GstCaps * caps1,
    GstBuffer * buf2, GstCaps * caps2)
{
  GstMapInfo map1, map2;
  gint c;

  gst_buffer_map (buf1, &map1, GST_MAP_READ);
  gst_buffer_map (buf2, &map2, GST_MAP_READ);

  c = memcmp (map1.data, map2.data, map1.size);

  gst_buffer_unmap (buf1, &map1);
  gst_buffer_unmap (buf2, &map2);

  return c ? 1 : 0;
}

static gint
gst_compare_max (GstCompare * comp, GstBuffer * buf1, GstCaps * caps1,
    GstBuffer * buf2, GstCaps * caps2)
{
  gint i, delta = 0;
  gint8 *data1, *data2;
  GstMapInfo map1, map2;

  gst_buffer_map (buf1, &map1, GST_MAP_READ);
  gst_buffer_map (buf2, &map2, GST_MAP_READ);

  data1 = (gint8 *) map1.data;
  data2 = (gint8 *) map2.data;

  /* primitive loop */
  for (i = 0; i < map1.size; i++) {
    gint diff = ABS (*data1 - *data2);
    if (diff > 0)
      GST_LOG_OBJECT (comp, "diff at %d = %d", i, diff);
    delta = MAX (delta, ABS (*data1 - *data2));
    data1++;
    data2++;
  }

  gst_buffer_unmap (buf1, &map1);
  gst_buffer_unmap (buf2, &map2);

  return delta;
}

static double
gst_compare_ssim_window (GstCompare * comp, guint8 * data1, guint8 * data2,
    gint width, gint height, gint step, gint stride)
{
  gint count = 0, i, j;
  gint sum1 = 0, sum2 = 0, ssum1 = 0, ssum2 = 0, acov = 0;
  gdouble avg1, avg2, var1, var2, cov;

  const gdouble k1 = 0.01;
  const gdouble k2 = 0.03;
  const gdouble L = 255.0;
  const gdouble c1 = (k1 * L) * (k1 * L);
  const gdouble c2 = (k2 * L) * (k2 * L);

  /* For empty images, return maximum similarity */
  if (height <= 0 || width <= 0)
    return 1.0;

  /* plain and simple; no fancy optimizations */
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      sum1 += *data1;
      sum2 += *data2;
      ssum1 += *data1 * *data1;
      ssum2 += *data2 * *data2;
      acov += *data1 * *data2;
      count++;
      data1 += step;
      data2 += step;
    }
    data1 -= j * step;
    data2 -= j * step;
    data1 += stride;
    data2 += stride;
  }

  avg1 = sum1 / count;
  avg2 = sum2 / count;
  var1 = ssum1 / count - avg1 * avg1;
  var2 = ssum2 / count - avg2 * avg2;
  cov = acov / count - avg1 * avg2;

  return (2 * avg1 * avg2 + c1) * (2 * cov + c2) /
      ((avg1 * avg1 + avg2 * avg2 + c1) * (var1 + var2 + c2));
}

/* @width etc are for the particular component */
static gdouble
gst_compare_ssim_component (GstCompare * comp, guint8 * data1, guint8 * data2,
    gint width, gint height, gint step, gint stride)
{
  const gint window = 16;
  gdouble ssim_sum = 0;
  gint count = 0, i, j;

  for (j = 0; j + (window / 2) < height; j += (window / 2)) {
    for (i = 0; i + (window / 2) < width; i += (window / 2)) {
      gdouble ssim;

      ssim = gst_compare_ssim_window (comp, data1 + step * i + j * stride,
          data2 + step * i + j * stride,
          MIN (window, width - i), MIN (window, height - j), step, stride);
      GST_LOG_OBJECT (comp, "ssim for %dx%d at (%d, %d) = %f", window, window,
          i, j, ssim);
      ssim_sum += ssim;
      count++;
    }
  }

  /* For empty images, return maximum similarity */
  if (count == 0)
    return 1.0;

  return (ssim_sum / count);
}

static gdouble
gst_compare_ssim (GstCompare * comp, GstBuffer * buf1, GstCaps * caps1,
    GstBuffer * buf2, GstCaps * caps2)
{
  GstVideoInfo info1, info2;
  GstVideoFrame frame1, frame2;
  gint i, comps;
  gdouble cssim[4], ssim, c[4] = { 1.0, 0.0, 0.0, 0.0 };

  if (!caps1)
    goto invalid_input;

  if (!gst_video_info_from_caps (&info1, caps1))
    goto invalid_input;

  if (!caps2)
    goto invalid_input;

  if (!gst_video_info_from_caps (&info2, caps1))
    goto invalid_input;

  if (GST_VIDEO_INFO_FORMAT (&info1) != GST_VIDEO_INFO_FORMAT (&info2) ||
      GST_VIDEO_INFO_WIDTH (&info1) != GST_VIDEO_INFO_WIDTH (&info2) ||
      GST_VIDEO_INFO_HEIGHT (&info1) != GST_VIDEO_INFO_HEIGHT (&info2))
    return comp->threshold + 1;

  comps = GST_VIDEO_INFO_N_COMPONENTS (&info1);
  /* note that some are reported both yuv and gray */
  for (i = 0; i < comps; ++i)
    c[i] = 1.0;
  /* increase luma weight if yuv */
  if (GST_VIDEO_INFO_IS_YUV (&info1) && (comps > 1))
    c[0] = comps - 1;
  for (i = 0; i < comps; ++i)
    c[i] /= (GST_VIDEO_INFO_IS_YUV (&info1) && (comps > 1)) ?
        2 * (comps - 1) : comps;

  gst_video_frame_map (&frame1, &info1, buf1, GST_MAP_READ);
  gst_video_frame_map (&frame2, &info2, buf2, GST_MAP_READ);

  for (i = 0; i < comps; i++) {
    gint cw, ch, step, stride;

    /* only support most common formats */
    if (GST_VIDEO_INFO_COMP_DEPTH (&info1, i) != 8)
      goto unsupported_input;
    cw = GST_VIDEO_FRAME_COMP_WIDTH (&frame1, i);
    ch = GST_VIDEO_FRAME_COMP_HEIGHT (&frame1, i);
    step = GST_VIDEO_FRAME_COMP_PSTRIDE (&frame1, i);
    stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame1, i);

    GST_LOG_OBJECT (comp, "component %d", i);
    cssim[i] = gst_compare_ssim_component (comp,
        GST_VIDEO_FRAME_COMP_DATA (&frame1, i),
        GST_VIDEO_FRAME_COMP_DATA (&frame2, i), cw, ch, step, stride);
    GST_LOG_OBJECT (comp, "ssim[%d] = %f", i, cssim[i]);
  }

  gst_video_frame_unmap (&frame1);
  gst_video_frame_unmap (&frame2);

#ifndef GST_DISABLE_GST_DEBUG
  for (i = 0; i < 4; i++) {
    GST_DEBUG_OBJECT (comp, "ssim[%d] = %f, c[%d] = %f", i, cssim[i], i, c[i]);
  }
#endif

  ssim = cssim[0] * c[0] + cssim[1] * c[1] + cssim[2] * c[2] + cssim[3] * c[3];

  return ssim;

  /* ERRORS */
invalid_input:
  {
    GST_ERROR_OBJECT (comp, "ssim method needs raw video input");
    return 0;
  }
unsupported_input:
  {
    GST_ERROR_OBJECT (comp, "raw video format not supported %" GST_PTR_FORMAT,
        caps1);
    return 0;
  }
}

static void
gst_compare_buffers (GstCompare * comp, GstBuffer * buf1, GstCaps * caps1,
    GstBuffer * buf2, GstCaps * caps2)
{
  gdouble delta = 0;
  gsize size1, size2;

  /* first check metadata */
  gst_compare_meta (comp, buf1, caps1, buf2, caps2);

  size1 = gst_buffer_get_size (buf1);
  size2 = gst_buffer_get_size (buf1);

  /* check content according to method */
  /* but at least size should match */
  if (size1 != size2) {
    delta = comp->threshold + 1;
  } else {
    GstMapInfo map1, map2;

    gst_buffer_map (buf1, &map1, GST_MAP_READ);
    gst_buffer_map (buf2, &map2, GST_MAP_READ);
    GST_MEMDUMP_OBJECT (comp, "buffer 1", map1.data, map2.size);
    GST_MEMDUMP_OBJECT (comp, "buffer 2", map2.data, map2.size);
    gst_buffer_unmap (buf1, &map1);
    gst_buffer_unmap (buf2, &map2);
    switch (comp->method) {
      case GST_COMPARE_METHOD_MEM:
        delta = gst_compare_mem (comp, buf1, caps1, buf2, caps2);
        break;
      case GST_COMPARE_METHOD_MAX:
        delta = gst_compare_max (comp, buf1, caps1, buf2, caps2);
        break;
      case GST_COMPARE_METHOD_SSIM:
        delta = gst_compare_ssim (comp, buf1, caps1, buf2, caps2);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }

  if ((comp->upper && delta > comp->threshold) ||
      (!comp->upper && delta < comp->threshold)) {
    GST_WARNING_OBJECT (comp, "buffers %p and %p failed content match %f",
        buf1, buf2, delta);

    gst_element_post_message (GST_ELEMENT (comp),
        gst_message_new_element (GST_OBJECT (comp),
            gst_structure_new ("delta", "content", G_TYPE_DOUBLE, delta,
                NULL)));
  }
}

static GstFlowReturn
gst_compare_collect_pads (GstCollectPads * cpads, GstCompare * comp)
{
  GstBuffer *buf1, *buf2;
  GstCaps *caps1, *caps2;

  buf1 = gst_collect_pads_pop (comp->cpads,
      gst_pad_get_element_private (comp->sinkpad));
  caps1 = gst_pad_get_current_caps (comp->sinkpad);

  buf2 = gst_collect_pads_pop (comp->cpads,
      gst_pad_get_element_private (comp->checkpad));
  caps2 = gst_pad_get_current_caps (comp->checkpad);

  if (!buf1 && !buf2) {
    gst_pad_push_event (comp->srcpad, gst_event_new_eos ());
    return GST_FLOW_EOS;
  } else if (buf1 && buf2) {
    gst_compare_buffers (comp, buf1, caps1, buf2, caps2);
  } else {
    GST_WARNING_OBJECT (comp, "buffer %p != NULL", buf1 ? buf1 : buf2);

    comp->count++;
    gst_element_post_message (GST_ELEMENT (comp),
        gst_message_new_element (GST_OBJECT (comp),
            gst_structure_new ("delta", "count", G_TYPE_INT, comp->count,
                NULL)));
  }

  if (buf1)
    gst_pad_push (comp->srcpad, buf1);

  if (buf2)
    gst_buffer_unref (buf2);

  if (caps1)
    gst_caps_unref (caps1);

  if (caps2)
    gst_caps_unref (caps2);

  return GST_FLOW_OK;
}

static void
gst_compare_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCompare *comp = GST_COMPARE (object);

  switch (prop_id) {
    case PROP_META:
      comp->meta = g_value_get_flags (value);
      break;
    case PROP_OFFSET_TS:
      comp->offset_ts = g_value_get_boolean (value);
      break;
    case PROP_METHOD:
      comp->method = g_value_get_enum (value);
      break;
    case PROP_THRESHOLD:
      comp->threshold = g_value_get_double (value);
      break;
    case PROP_UPPER:
      comp->upper = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_compare_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstCompare *comp = GST_COMPARE (object);

  switch (prop_id) {
    case PROP_META:
      g_value_set_flags (value, comp->meta);
      break;
    case PROP_OFFSET_TS:
      g_value_set_boolean (value, comp->offset_ts);
      break;
    case PROP_METHOD:
      g_value_set_enum (value, comp->method);
      break;
    case PROP_THRESHOLD:
      g_value_set_double (value, comp->threshold);
      break;
    case PROP_UPPER:
      g_value_set_boolean (value, comp->upper);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_compare_change_state (GstElement * element, GstStateChange transition)
{
  GstCompare *comp = GST_COMPARE (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_collect_pads_start (comp->cpads);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (comp->cpads);
      break;
    default:
      break;
  }

  ret = GST_CALL_PARENT_WITH_DEFAULT (GST_ELEMENT_CLASS, change_state,
      (element, transition), GST_STATE_CHANGE_SUCCESS);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_compare_reset (comp);
      break;
    default:
      break;
  }

  return GST_STATE_CHANGE_SUCCESS;
}
