/* Generic video aggregator plugin
 * Copyright (C) 2004, 2008 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * SECTION:gstvideoaggregator
 * @title: GstVideoAggregator
 * @short_description: Base class for video aggregators
 *
 * VideoAggregator can accept AYUV, ARGB and BGRA video streams. For each of the requested
 * sink pads it will compare the incoming geometry and framerate to define the
 * output parameters. Indeed output video frames will have the geometry of the
 * biggest incoming video stream and the framerate of the fastest incoming one.
 *
 * VideoAggregator will do colorspace conversion.
 *
 * Zorder for each input stream can be configured on the
 * #GstVideoAggregatorPad.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvideoaggregator.h"

GST_DEBUG_CATEGORY_STATIC (gst_video_aggregator_debug);
#define GST_CAT_DEFAULT gst_video_aggregator_debug

/* Needed prototypes */
static void gst_video_aggregator_reset_qos (GstVideoAggregator * vagg);

struct _GstVideoAggregatorPrivate
{
  /* Lock to prevent the state to change while aggregating */
  GMutex lock;

  /* Current downstream segment */
  GstClockTime ts_offset;
  guint64 nframes;

  /* QoS stuff */
  gdouble proportion;
  GstClockTime earliest_time;
  guint64 qos_processed, qos_dropped;

  /* current caps */
  GstCaps *current_caps;

  gboolean live;

  /* The (ordered) list of #GstVideoFormatInfo supported by the aggregation
     method (from the srcpad template caps). */
  GPtrArray *supported_formats;

  GstTaskPool *task_pool;
};

/****************************************
 * GstVideoAggregatorPad implementation *
 ****************************************/

#define DEFAULT_PAD_ZORDER 0
#define DEFAULT_PAD_REPEAT_AFTER_EOS FALSE
#define DEFAULT_PAD_MAX_LAST_BUFFER_REPEAT GST_CLOCK_TIME_NONE
enum
{
  PROP_PAD_0,
  PROP_PAD_ZORDER,
  PROP_PAD_REPEAT_AFTER_EOS,
  PROP_PAD_MAX_LAST_BUFFER_REPEAT,
};


struct _GstVideoAggregatorPadPrivate
{
  GstBuffer *buffer;
  GstCaps *caps;
  GstVideoFrame prepared_frame;

  /* properties */
  guint zorder;
  gboolean repeat_after_eos;
  GstClockTime max_last_buffer_repeat;

  /* Subclasses can force an alpha channel in the (input thus output)
   * colorspace format */
  gboolean needs_alpha;

  GstClockTime start_time;
  GstClockTime end_time;

  GstVideoInfo pending_vinfo;
  GstCaps *pending_caps;
};


G_DEFINE_TYPE_WITH_PRIVATE (GstVideoAggregatorPad, gst_video_aggregator_pad,
    GST_TYPE_AGGREGATOR_PAD);

static void
gst_video_aggregator_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_ZORDER:
      g_value_set_uint (value, pad->priv->zorder);
      break;
    case PROP_PAD_REPEAT_AFTER_EOS:
      g_value_set_boolean (value, pad->priv->repeat_after_eos);
      break;
    case PROP_PAD_MAX_LAST_BUFFER_REPEAT:
      g_value_set_uint64 (value, pad->priv->max_last_buffer_repeat);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static int
pad_zorder_compare (const GstVideoAggregatorPad * pad1,
    const GstVideoAggregatorPad * pad2)
{
  return pad1->priv->zorder - pad2->priv->zorder;
}

static void
gst_video_aggregator_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_ZORDER:{
      GstVideoAggregator *vagg =
          GST_VIDEO_AGGREGATOR (gst_pad_get_parent (GST_PAD (pad)));
      if (vagg) {
        GST_OBJECT_LOCK (vagg);
        pad->priv->zorder = g_value_get_uint (value);
        GST_ELEMENT (vagg)->sinkpads =
            g_list_sort (GST_ELEMENT (vagg)->sinkpads,
            (GCompareFunc) pad_zorder_compare);
        GST_OBJECT_UNLOCK (vagg);
        gst_object_unref (vagg);
      } else {
        pad->priv->zorder = g_value_get_uint (value);
      }
      break;
    }
    case PROP_PAD_REPEAT_AFTER_EOS:
      pad->priv->repeat_after_eos = g_value_get_boolean (value);
      break;
    case PROP_PAD_MAX_LAST_BUFFER_REPEAT:
      pad->priv->max_last_buffer_repeat = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
_flush_pad (GstAggregatorPad * aggpad, GstAggregator * aggregator)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (aggregator);
  GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (aggpad);

  gst_video_aggregator_reset_qos (vagg);
  gst_buffer_replace (&pad->priv->buffer, NULL);
  gst_caps_replace (&pad->priv->caps, NULL);
  pad->priv->start_time = -1;
  pad->priv->end_time = -1;

  return GST_FLOW_OK;
}

static gboolean
gst_video_aggregator_pad_skip_buffer (GstAggregatorPad * aggpad,
    GstAggregator * agg, GstBuffer * buffer)
{
  gboolean ret = FALSE;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  if (agg_segment->position != GST_CLOCK_TIME_NONE
      && GST_BUFFER_DURATION (buffer) != GST_CLOCK_TIME_NONE) {
    GstClockTime start_time =
        gst_segment_to_running_time (&aggpad->segment, GST_FORMAT_TIME,
        GST_BUFFER_PTS (buffer));
    GstClockTime end_time = start_time + GST_BUFFER_DURATION (buffer);
    GstClockTime output_start_running_time =
        gst_segment_to_running_time (agg_segment, GST_FORMAT_TIME,
        agg_segment->position);

    ret = end_time < output_start_running_time;
  }

  return ret;
}

static gboolean
gst_video_aggregator_pad_prepare_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  if (!gst_video_frame_map (prepared_frame, &pad->info, buffer, GST_MAP_READ)) {
    GST_WARNING_OBJECT (vagg, "Could not map input buffer");
    return FALSE;
  }

  return TRUE;
}

static void
gst_video_aggregator_pad_clean_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  if (prepared_frame->buffer) {
    gst_video_frame_unmap (prepared_frame);
    memset (prepared_frame, 0, sizeof (GstVideoFrame));
  }
}

static GstSample *
gst_video_aggregator_peek_next_sample (GstAggregator * agg,
    GstAggregatorPad * aggpad)
{
  GstVideoAggregatorPad *vaggpad = GST_VIDEO_AGGREGATOR_PAD (aggpad);
  GstSample *res = NULL;

  if (vaggpad->priv->buffer) {
    res = gst_sample_new (vaggpad->priv->buffer, vaggpad->priv->caps,
        &aggpad->segment, NULL);
  }

  return res;
}

static void
gst_video_aggregator_pad_class_init (GstVideoAggregatorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstAggregatorPadClass *aggpadclass = (GstAggregatorPadClass *) klass;

  gobject_class->set_property = gst_video_aggregator_pad_set_property;
  gobject_class->get_property = gst_video_aggregator_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_ZORDER,
      g_param_spec_uint ("zorder", "Z-Order", "Z Order of the picture",
          0, G_MAXUINT, DEFAULT_PAD_ZORDER,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_REPEAT_AFTER_EOS,
      g_param_spec_boolean ("repeat-after-eos", "Repeat After EOS",
          "Repeat the " "last frame after EOS until all pads are EOS",
          DEFAULT_PAD_REPEAT_AFTER_EOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoAggregatorPad::max-last-buffer-repeat:
   *
   * Repeat last buffer for time (in ns, -1 = until EOS).
   * The default behaviour is for the last buffer received on a pad to be
   * aggregated until a new buffer is received.
   *
   * Setting this property causes the last buffer to be discarded once the
   * running time of the output buffer is `max-last-buffer-repeat` nanoseconds
   * past its end running time. When the buffer didn't have a duration, the
   * comparison is made against its running start time.
   *
   * This is useful in live scenarios: when a stream encounters a temporary
   * networking problem, a #GstVideoAggregator subclass can then fall back to
   * displaying a lower z-order stream, or the background.
   *
   * Setting this property doesn't affect the behaviour on EOS.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class,
      PROP_PAD_MAX_LAST_BUFFER_REPEAT,
      g_param_spec_uint64 ("max-last-buffer-repeat", "Max Last Buffer Repeat",
          "Repeat last buffer for time (in ns, -1=until EOS), "
          "behaviour on EOS is not affected", 0, G_MAXUINT64,
          DEFAULT_PAD_MAX_LAST_BUFFER_REPEAT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));

  aggpadclass->flush = GST_DEBUG_FUNCPTR (_flush_pad);
  aggpadclass->skip_buffer =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_pad_skip_buffer);
  klass->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_pad_prepare_frame);
  klass->clean_frame = GST_DEBUG_FUNCPTR (gst_video_aggregator_pad_clean_frame);
}

static void
gst_video_aggregator_pad_init (GstVideoAggregatorPad * vaggpad)
{
  vaggpad->priv = gst_video_aggregator_pad_get_instance_private (vaggpad);

  vaggpad->priv->zorder = DEFAULT_PAD_ZORDER;
  vaggpad->priv->repeat_after_eos = DEFAULT_PAD_REPEAT_AFTER_EOS;
  vaggpad->priv->max_last_buffer_repeat = DEFAULT_PAD_MAX_LAST_BUFFER_REPEAT;
  memset (&vaggpad->priv->prepared_frame, 0, sizeof (GstVideoFrame));
}

/**
 * gst_video_aggregator_pad_has_current_buffer:
 * @pad: a #GstVideoAggregatorPad
 *
 * Checks if the pad currently has a buffer queued that is going to be used
 * for the current output frame.
 *
 * This must only be called from the #GstVideoAggregatorClass::aggregate_frames virtual method,
 * or from the #GstVideoAggregatorPadClass::prepare_frame virtual method of the aggregator pads.
 *
 * Returns: %TRUE if the pad has currently a buffer queued
 */
gboolean
gst_video_aggregator_pad_has_current_buffer (GstVideoAggregatorPad * pad)
{
  g_return_val_if_fail (GST_IS_VIDEO_AGGREGATOR_PAD (pad), FALSE);

  return pad->priv->buffer != NULL;
}

/**
 * gst_video_aggregator_pad_get_current_buffer:
 * @pad: a #GstVideoAggregatorPad
 *
 * Returns the currently queued buffer that is going to be used
 * for the current output frame.
 *
 * This must only be called from the #GstVideoAggregatorClass::aggregate_frames virtual method,
 * or from the #GstVideoAggregatorPadClass::prepare_frame virtual method of the aggregator pads.
 *
 * The return value is only valid until #GstVideoAggregatorClass::aggregate_frames or #GstVideoAggregatorPadClass::prepare_frame
 * returns.
 *
 * Returns: (transfer none): The currently queued buffer
 */
GstBuffer *
gst_video_aggregator_pad_get_current_buffer (GstVideoAggregatorPad * pad)
{
  g_return_val_if_fail (GST_IS_VIDEO_AGGREGATOR_PAD (pad), NULL);

  return pad->priv->buffer;
}

/**
 * gst_video_aggregator_pad_get_prepared_frame:
 * @pad: a #GstVideoAggregatorPad
 *
 * Returns the currently prepared video frame that has to be aggregated into
 * the current output frame.
 *
 * This must only be called from the #GstVideoAggregatorClass::aggregate_frames virtual method,
 * or from the #GstVideoAggregatorPadClass::prepare_frame virtual method of the aggregator pads.
 *
 * The return value is only valid until #GstVideoAggregatorClass::aggregate_frames or #GstVideoAggregatorPadClass::prepare_frame
 * returns.
 *
 * Returns: (transfer none): The currently prepared video frame
 */
GstVideoFrame *
gst_video_aggregator_pad_get_prepared_frame (GstVideoAggregatorPad * pad)
{
  g_return_val_if_fail (GST_IS_VIDEO_AGGREGATOR_PAD (pad), NULL);

  return pad->priv->prepared_frame.buffer ? &pad->priv->prepared_frame : NULL;
}

/**
 * gst_video_aggregator_pad_set_needs_alpha:
 * @pad: a #GstVideoAggregatorPad
 * @needs_alpha: %TRUE if this pad requires alpha output
 *
 * Allows selecting that this pad requires an output format with alpha
 *
 */
void
gst_video_aggregator_pad_set_needs_alpha (GstVideoAggregatorPad * pad,
    gboolean needs_alpha)
{
  g_return_if_fail (GST_IS_VIDEO_AGGREGATOR_PAD (pad));

  if (needs_alpha != pad->priv->needs_alpha) {
    GstAggregator *agg =
        GST_AGGREGATOR (gst_object_get_parent (GST_OBJECT (pad)));
    pad->priv->needs_alpha = needs_alpha;
    if (agg) {
      gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (agg));
      gst_object_unref (agg);
    }
  }
}

/****************************************
 * GstVideoAggregatorConvertPad implementation *
 ****************************************/

enum
{
  PROP_CONVERT_PAD_0,
  PROP_CONVERT_PAD_CONVERTER_CONFIG,
};

struct _GstVideoAggregatorConvertPadPrivate
{
  /* The following fields are only used from the aggregate thread and when
   * initializing / finalizing */

  /* Converter, if NULL no conversion is done */
  GstVideoConverter *convert;

  /* caps used for conversion if needed */
  GstVideoInfo conversion_info;
  GstBuffer *converted_buffer;

  /* The following fields are accessed from the property setters / getters,
   * and as such are protected with the object lock */
  GstStructure *converter_config;
  gboolean converter_config_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstVideoAggregatorConvertPad,
    gst_video_aggregator_convert_pad, GST_TYPE_VIDEO_AGGREGATOR_PAD);

static void
gst_video_aggregator_convert_pad_finalize (GObject * o)
{
  GstVideoAggregatorConvertPad *vaggpad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (o);

  if (vaggpad->priv->convert)
    gst_video_converter_free (vaggpad->priv->convert);
  vaggpad->priv->convert = NULL;

  if (vaggpad->priv->converter_config)
    gst_structure_free (vaggpad->priv->converter_config);
  vaggpad->priv->converter_config = NULL;

  G_OBJECT_CLASS (gst_video_aggregator_pad_parent_class)->finalize (o);
}

static void
    gst_video_aggregator_convert_pad_update_conversion_info_internal
    (GstVideoAggregatorPad * vpad)
{
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (vpad);

  GST_OBJECT_LOCK (pad);
  pad->priv->converter_config_changed = TRUE;
  GST_OBJECT_UNLOCK (pad);
}

static gboolean
gst_video_aggregator_convert_pad_prepare_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (vpad);
  GstVideoFrame frame;

  /* Update/create converter as needed */
  GST_OBJECT_LOCK (pad);
  if (pad->priv->converter_config_changed) {
    GstVideoAggregatorConvertPadClass *klass =
        GST_VIDEO_AGGREGATOR_CONVERT_PAD_GET_CLASS (pad);
    GstVideoInfo conversion_info;

    gst_video_info_init (&conversion_info);
    klass->create_conversion_info (pad, vagg, &conversion_info);
    if (conversion_info.finfo == NULL) {
      GST_OBJECT_UNLOCK (pad);
      return FALSE;
    }
    pad->priv->converter_config_changed = FALSE;

    pad->priv->conversion_info = conversion_info;

    if (pad->priv->convert)
      gst_video_converter_free (pad->priv->convert);
    pad->priv->convert = NULL;

    if (!gst_video_info_is_equal (&vpad->info, &pad->priv->conversion_info)
        || pad->priv->converter_config) {
      pad->priv->convert =
          gst_video_converter_new_with_pool (&vpad->info,
          &pad->priv->conversion_info,
          pad->priv->converter_config ? gst_structure_copy (pad->
              priv->converter_config) : NULL, vagg->priv->task_pool);
      if (!pad->priv->convert) {
        GST_WARNING_OBJECT (pad, "No path found for conversion");
        GST_OBJECT_UNLOCK (pad);
        return FALSE;
      }

      GST_DEBUG_OBJECT (pad, "This pad will be converted from %s to %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&vpad->info)),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&pad->priv->
                  conversion_info)));
    } else {
      GST_DEBUG_OBJECT (pad, "This pad will not need conversion");
    }
  }
  GST_OBJECT_UNLOCK (pad);

  if (!gst_video_frame_map (&frame, &vpad->info, buffer, GST_MAP_READ)) {
    GST_WARNING_OBJECT (vagg, "Could not map input buffer");
    return FALSE;
  }

  if (pad->priv->convert) {
    GstVideoFrame converted_frame;
    GstBuffer *converted_buf = NULL;
    static GstAllocationParams params = { 0, 15, 0, 0, };
    gint converted_size;
    guint outsize;

    /* We wait until here to set the conversion infos, in case vagg->info changed */
    converted_size = pad->priv->conversion_info.size;
    outsize = GST_VIDEO_INFO_SIZE (&vagg->info);
    converted_size = converted_size > outsize ? converted_size : outsize;
    converted_buf = gst_buffer_new_allocate (NULL, converted_size, &params);

    if (!gst_video_frame_map (&converted_frame, &(pad->priv->conversion_info),
            converted_buf, GST_MAP_READWRITE)) {
      GST_WARNING_OBJECT (vagg, "Could not map converted frame");

      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    gst_video_converter_frame (pad->priv->convert, &frame, &converted_frame);
    pad->priv->converted_buffer = converted_buf;
    gst_video_frame_unmap (&frame);
    *prepared_frame = converted_frame;
  } else {
    *prepared_frame = frame;
  }

  return TRUE;
}

static void
gst_video_aggregator_convert_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (vpad);

  if (prepared_frame->buffer) {
    gst_video_frame_unmap (prepared_frame);
    memset (prepared_frame, 0, sizeof (GstVideoFrame));
  }

  if (pad->priv->converted_buffer) {
    gst_buffer_unref (pad->priv->converted_buffer);
    pad->priv->converted_buffer = NULL;
  }
}

static void
    gst_video_aggregator_convert_pad_create_conversion_info
    (GstVideoAggregatorConvertPad * pad, GstVideoAggregator * agg,
    GstVideoInfo * convert_info)
{
  GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD (pad);
  gchar *colorimetry, *best_colorimetry;
  gchar *chroma, *best_chroma;

  g_return_if_fail (GST_IS_VIDEO_AGGREGATOR_CONVERT_PAD (pad));
  g_return_if_fail (convert_info != NULL);

  if (!vpad->info.finfo
      || GST_VIDEO_INFO_FORMAT (&vpad->info) == GST_VIDEO_FORMAT_UNKNOWN) {
    return;
  }

  if (!agg->info.finfo
      || GST_VIDEO_INFO_FORMAT (&agg->info) == GST_VIDEO_FORMAT_UNKNOWN) {
    return;
  }

  colorimetry = gst_video_colorimetry_to_string (&vpad->info.colorimetry);
  chroma = gst_video_chroma_site_to_string (vpad->info.chroma_site);

  best_colorimetry = gst_video_colorimetry_to_string (&agg->info.colorimetry);
  best_chroma = gst_video_chroma_site_to_string (agg->info.chroma_site);

  if (GST_VIDEO_INFO_FORMAT (&agg->info) != GST_VIDEO_INFO_FORMAT (&vpad->info)
      || g_strcmp0 (colorimetry, best_colorimetry)
      || g_strcmp0 (chroma, best_chroma)) {
    GstVideoInfo tmp_info;

    /* Initialize with the wanted video format and our original width and
     * height as we don't want to rescale. Then copy over the wanted
     * colorimetry, and chroma-site and our current pixel-aspect-ratio
     * and other relevant fields.
     */
    gst_video_info_set_format (&tmp_info, GST_VIDEO_INFO_FORMAT (&agg->info),
        vpad->info.width, vpad->info.height);
    tmp_info.chroma_site = agg->info.chroma_site;
    tmp_info.colorimetry = agg->info.colorimetry;
    tmp_info.par_n = vpad->info.par_n;
    tmp_info.par_d = vpad->info.par_d;
    tmp_info.fps_n = vpad->info.fps_n;
    tmp_info.fps_d = vpad->info.fps_d;
    tmp_info.flags = vpad->info.flags;
    tmp_info.interlace_mode = vpad->info.interlace_mode;

    *convert_info = tmp_info;
  } else {
    *convert_info = vpad->info;
  }

  g_free (colorimetry);
  g_free (best_colorimetry);
  g_free (chroma);
  g_free (best_chroma);
}

static void
gst_video_aggregator_convert_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (object);

  switch (prop_id) {
    case PROP_CONVERT_PAD_CONVERTER_CONFIG:
      GST_OBJECT_LOCK (pad);
      if (pad->priv->converter_config)
        g_value_set_boxed (value, pad->priv->converter_config);
      GST_OBJECT_UNLOCK (pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_aggregator_convert_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (object);

  switch (prop_id) {
    case PROP_CONVERT_PAD_CONVERTER_CONFIG:
      GST_OBJECT_LOCK (pad);
      if (pad->priv->converter_config)
        gst_structure_free (pad->priv->converter_config);
      pad->priv->converter_config = g_value_dup_boxed (value);
      pad->priv->converter_config_changed = TRUE;
      GST_OBJECT_UNLOCK (pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_aggregator_convert_pad_class_init (GstVideoAggregatorConvertPadClass *
    klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorPadClass *vaggpadclass =
      (GstVideoAggregatorPadClass *) klass;

  gobject_class->finalize = gst_video_aggregator_convert_pad_finalize;
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_convert_pad_get_property);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_convert_pad_set_property);

  g_object_class_install_property (gobject_class,
      PROP_CONVERT_PAD_CONVERTER_CONFIG, g_param_spec_boxed ("converter-config",
          "Converter configuration",
          "A GstStructure describing the configuration that should be used "
          "when scaling and converting this pad's video frames",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  vaggpadclass->update_conversion_info =
      GST_DEBUG_FUNCPTR
      (gst_video_aggregator_convert_pad_update_conversion_info_internal);
  vaggpadclass->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_convert_pad_prepare_frame);
  vaggpadclass->clean_frame =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_convert_pad_clean_frame);

  klass->create_conversion_info =
      gst_video_aggregator_convert_pad_create_conversion_info;
}

static void
gst_video_aggregator_convert_pad_init (GstVideoAggregatorConvertPad * vaggpad)
{
  vaggpad->priv =
      gst_video_aggregator_convert_pad_get_instance_private (vaggpad);

  vaggpad->priv->converted_buffer = NULL;
  vaggpad->priv->convert = NULL;
  vaggpad->priv->converter_config = NULL;
  vaggpad->priv->converter_config_changed = FALSE;
}

/**
 * gst_video_aggregator_convert_pad_update_conversion_info:
 * @pad: a #GstVideoAggregatorPad
 *
 * Requests the pad to check and update the converter before the next usage to
 * update for any changes that have happened.
 *
 */
void gst_video_aggregator_convert_pad_update_conversion_info
    (GstVideoAggregatorConvertPad * pad)
{
  g_return_if_fail (GST_IS_VIDEO_AGGREGATOR_CONVERT_PAD (pad));

  GST_OBJECT_LOCK (pad);
  pad->priv->converter_config_changed = TRUE;
  GST_OBJECT_UNLOCK (pad);
}

struct _GstVideoAggregatorParallelConvertPadPrivate
{
  GstVideoFrame src_frame;
  gboolean is_converting;
};

typedef struct _GstVideoAggregatorParallelConvertPadPrivate
    GstVideoAggregatorParallelConvertPadPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GstVideoAggregatorParallelConvertPad,
    gst_video_aggregator_parallel_convert_pad,
    GST_TYPE_VIDEO_AGGREGATOR_CONVERT_PAD);
#define PARALLEL_CONVERT_PAD_GET_PRIVATE(o) \
    gst_video_aggregator_parallel_convert_pad_get_instance_private (o)

static void
    gst_video_aggregator_parallel_convert_pad_prepare_frame_start
    (GstVideoAggregatorPad * vpad, GstVideoAggregator * vagg,
    GstBuffer * buffer, GstVideoFrame * prepared_frame)
{
  GstVideoAggregatorParallelConvertPad *ppad =
      GST_VIDEO_AGGREGATOR_PARALLEL_CONVERT_PAD (vpad);
  GstVideoAggregatorParallelConvertPadPrivate *pcp_priv =
      PARALLEL_CONVERT_PAD_GET_PRIVATE (ppad);
  GstVideoAggregatorConvertPad *pad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (vpad);

  memset (&pcp_priv->src_frame, 0, sizeof (pcp_priv->src_frame));

  pcp_priv->is_converting = FALSE;

  /* Update/create converter as needed */
  GST_OBJECT_LOCK (pad);
  if (pad->priv->converter_config_changed) {
    GstVideoAggregatorConvertPadClass *klass =
        GST_VIDEO_AGGREGATOR_CONVERT_PAD_GET_CLASS (pad);
    GstVideoInfo conversion_info;

    gst_video_info_init (&conversion_info);
    klass->create_conversion_info (pad, vagg, &conversion_info);
    if (conversion_info.finfo == NULL) {
      GST_OBJECT_UNLOCK (pad);
      return;
    }
    pad->priv->converter_config_changed = FALSE;

    pad->priv->conversion_info = conversion_info;

    if (pad->priv->convert)
      gst_video_converter_free (pad->priv->convert);
    pad->priv->convert = NULL;

    if (!gst_video_info_is_equal (&vpad->info, &pad->priv->conversion_info)
        || pad->priv->converter_config) {
      GstStructure *conv_config;

      if (pad->priv->converter_config) {
        conv_config = gst_structure_copy (pad->priv->converter_config);
      } else {
        conv_config = gst_structure_new_empty ("GstVideoConverterConfig");
      }
      gst_structure_set (conv_config, GST_VIDEO_CONVERTER_OPT_ASYNC_TASKS,
          G_TYPE_BOOLEAN, TRUE, NULL);

      pad->priv->convert =
          gst_video_converter_new_with_pool (&vpad->info,
          &pad->priv->conversion_info, conv_config, vagg->priv->task_pool);
      if (!pad->priv->convert) {
        GST_WARNING_OBJECT (pad, "No path found for conversion");
        GST_OBJECT_UNLOCK (pad);
        return;
      }

      GST_DEBUG_OBJECT (pad, "This pad will be converted from %s to %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&vpad->info)),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&pad->priv->
                  conversion_info)));
    } else {
      GST_DEBUG_OBJECT (pad, "This pad will not need conversion");
    }
  }
  GST_OBJECT_UNLOCK (pad);

  if (!gst_video_frame_map (&pcp_priv->src_frame, &vpad->info, buffer,
          GST_MAP_READ)) {
    GST_WARNING_OBJECT (vagg, "Could not map input buffer");
    return;
  }

  if (pad->priv->convert) {
    GstBuffer *converted_buf = NULL;
    static GstAllocationParams params = { 0, 15, 0, 0, };
    gint converted_size;
    guint outsize;

    /* We wait until here to set the conversion infos, in case vagg->info changed */
    converted_size = pad->priv->conversion_info.size;
    outsize = GST_VIDEO_INFO_SIZE (&vagg->info);
    converted_size = converted_size > outsize ? converted_size : outsize;
    converted_buf = gst_buffer_new_allocate (NULL, converted_size, &params);

    if (!gst_video_frame_map (prepared_frame, &(pad->priv->conversion_info),
            converted_buf, GST_MAP_READWRITE)) {
      GST_WARNING_OBJECT (vagg, "Could not map converted frame");

      gst_clear_buffer (&converted_buf);
      gst_video_frame_unmap (&pcp_priv->src_frame);
      memset (&pcp_priv->src_frame, 0, sizeof (pcp_priv->src_frame));
      return;
    }

    gst_video_converter_frame (pad->priv->convert, &pcp_priv->src_frame,
        prepared_frame);
    pad->priv->converted_buffer = converted_buf;
    pcp_priv->is_converting = TRUE;
  } else {
    *prepared_frame = pcp_priv->src_frame;
    memset (&pcp_priv->src_frame, 0, sizeof (pcp_priv->src_frame));
  }
}

static void
    gst_video_aggregator_parallel_convert_pad_prepare_frame_finish
    (GstVideoAggregatorPad * vpad, GstVideoAggregator * vagg,
    GstVideoFrame * prepared_frame)
{
  GstVideoAggregatorParallelConvertPad *ppad =
      GST_VIDEO_AGGREGATOR_PARALLEL_CONVERT_PAD (vpad);
  GstVideoAggregatorParallelConvertPadPrivate *pcp_priv =
      PARALLEL_CONVERT_PAD_GET_PRIVATE (ppad);
  GstVideoAggregatorConvertPad *cpad = GST_VIDEO_AGGREGATOR_CONVERT_PAD (vpad);

  if (cpad->priv->convert && pcp_priv->is_converting) {
    pcp_priv->is_converting = FALSE;
    gst_video_converter_frame_finish (cpad->priv->convert);
    if (pcp_priv->src_frame.buffer) {
      gst_video_frame_unmap (&pcp_priv->src_frame);
      memset (&pcp_priv->src_frame, 0, sizeof (pcp_priv->src_frame));
    }
  }
}

static void
gst_video_aggregator_parallel_convert_pad_finalize (GObject * object)
{
  GstVideoAggregatorParallelConvertPad *ppad =
      GST_VIDEO_AGGREGATOR_PARALLEL_CONVERT_PAD (object);
  GstVideoAggregatorParallelConvertPadPrivate *pcp_priv =
      PARALLEL_CONVERT_PAD_GET_PRIVATE (ppad);
  GstVideoAggregatorConvertPad *cpad =
      GST_VIDEO_AGGREGATOR_CONVERT_PAD (object);

  if (cpad->priv->convert && pcp_priv->is_converting) {
    pcp_priv->is_converting = FALSE;
    gst_video_converter_frame_finish (cpad->priv->convert);
    if (pcp_priv->src_frame.buffer) {
      gst_video_frame_unmap (&pcp_priv->src_frame);
      memset (&pcp_priv->src_frame, 0, sizeof (pcp_priv->src_frame));
    }
  }

  G_OBJECT_CLASS
      (gst_video_aggregator_parallel_convert_pad_parent_class)->finalize
      (object);
}

static void
    gst_video_aggregator_parallel_convert_pad_class_init
    (GstVideoAggregatorParallelConvertPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorPadClass *vaggpadclass =
      (GstVideoAggregatorPadClass *) klass;

  gobject_class->finalize =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_parallel_convert_pad_finalize);

  vaggpadclass->prepare_frame = NULL;
  vaggpadclass->prepare_frame_start =
      GST_DEBUG_FUNCPTR
      (gst_video_aggregator_parallel_convert_pad_prepare_frame_start);
  vaggpadclass->prepare_frame_finish =
      GST_DEBUG_FUNCPTR
      (gst_video_aggregator_parallel_convert_pad_prepare_frame_finish);
}

static void
    gst_video_aggregator_parallel_convert_pad_init
    (GstVideoAggregatorParallelConvertPad * vaggpad)
{
}

/**************************************
 * GstVideoAggregator implementation  *
 **************************************/

#define GST_VIDEO_AGGREGATOR_GET_LOCK(vagg) (&GST_VIDEO_AGGREGATOR(vagg)->priv->lock)

#define GST_VIDEO_AGGREGATOR_LOCK(vagg)   G_STMT_START {       \
  GST_LOG_OBJECT (vagg, "Taking EVENT lock from thread %p",    \
        g_thread_self());                                      \
  g_mutex_lock(GST_VIDEO_AGGREGATOR_GET_LOCK(vagg));           \
  GST_LOG_OBJECT (vagg, "Took EVENT lock from thread %p",      \
        g_thread_self());                                      \
  } G_STMT_END

#define GST_VIDEO_AGGREGATOR_UNLOCK(vagg)   G_STMT_START {     \
  GST_LOG_OBJECT (vagg, "Releasing EVENT lock from thread %p", \
        g_thread_self());                                      \
  g_mutex_unlock(GST_VIDEO_AGGREGATOR_GET_LOCK(vagg));         \
  GST_LOG_OBJECT (vagg, "Took EVENT lock from thread %p",      \
        g_thread_self());                                      \
  } G_STMT_END

enum
{
  PROP_0,
  PROP_FORCE_LIVE,
};

#define DEFAULT_FORCE_LIVE              FALSE

/* Can't use the G_DEFINE_TYPE macros because we need the
 * videoaggregator class in the _init to be able to set
 * the sink pad non-alpha caps. Using the G_DEFINE_TYPE there
 * seems to be no way of getting the real class being initialized */
static void gst_video_aggregator_init (GstVideoAggregator * self,
    GstVideoAggregatorClass * klass);
static void gst_video_aggregator_class_init (GstVideoAggregatorClass * klass);
static gpointer gst_video_aggregator_parent_class = NULL;
static gint video_aggregator_private_offset = 0;

GType
gst_video_aggregator_get_type (void)
{
  static gsize static_g_define_type_id = 0;

  if (g_once_init_enter (&static_g_define_type_id)) {
    GType g_define_type_id = g_type_register_static_simple (GST_TYPE_AGGREGATOR,
        g_intern_static_string ("GstVideoAggregator"),
        sizeof (GstVideoAggregatorClass),
        (GClassInitFunc) gst_video_aggregator_class_init,
        sizeof (GstVideoAggregator),
        (GInstanceInitFunc) gst_video_aggregator_init,
        (GTypeFlags) G_TYPE_FLAG_ABSTRACT);

    video_aggregator_private_offset =
        g_type_add_instance_private (g_define_type_id,
        sizeof (GstVideoAggregatorPrivate));

    g_once_init_leave (&static_g_define_type_id, g_define_type_id);
  }
  return static_g_define_type_id;
}

static inline GstVideoAggregatorPrivate *
gst_video_aggregator_get_instance_private (GstVideoAggregator * self)
{
  return (G_STRUCT_MEMBER_P (self, video_aggregator_private_offset));
}

static gboolean
gst_video_aggregator_supports_format (GstVideoAggregator * vagg,
    GstVideoFormat format)
{
  gint i;

  for (i = 0; i < vagg->priv->supported_formats->len; i++) {
    GstVideoFormatInfo *format_info = vagg->priv->supported_formats->pdata[i];

    if (GST_VIDEO_FORMAT_INFO_FORMAT (format_info) == format)
      return TRUE;
  }

  return FALSE;
}

static GstCaps *
gst_video_aggregator_get_possible_caps_for_info (GstVideoInfo * info)
{
  GstStructure *s;
  GstCaps *possible_caps = gst_video_info_to_caps (info);

  s = gst_caps_get_structure (possible_caps, 0);
  gst_structure_remove_fields (s, "width", "height", "framerate",
      "pixel-aspect-ratio", "interlace-mode", NULL);

  return possible_caps;
}

static void
gst_video_aggregator_find_best_format (GstVideoAggregator * vagg,
    GstCaps * downstream_caps, GstVideoInfo * best_info,
    gboolean * at_least_one_alpha)
{
  GList *tmp;
  GstCaps *possible_caps;
  GstVideoAggregatorPad *pad;
  gboolean need_alpha = FALSE;
  gint best_format_number = 0, i;
  GHashTable *formats_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  GST_OBJECT_LOCK (vagg);
  for (tmp = GST_ELEMENT (vagg)->sinkpads; tmp; tmp = tmp->next) {
    gint format_number = 0;

    pad = tmp->data;

    if (!pad->info.finfo)
      continue;

    if (pad->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_ALPHA)
      *at_least_one_alpha = TRUE;

    /* If we want alpha, disregard all the other formats */
    if (need_alpha && !(pad->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_ALPHA))
      continue;

    /* This can happen if we release a pad and another pad hasn't been negotiated_caps yet */
    if (GST_VIDEO_INFO_FORMAT (&pad->info) == GST_VIDEO_FORMAT_UNKNOWN)
      continue;

    /* Can downstream accept this format ? */
    if (!GST_IS_VIDEO_AGGREGATOR_CONVERT_PAD (pad)) {
      possible_caps =
          gst_video_aggregator_get_possible_caps_for_info (&pad->info);
      if (!gst_caps_can_intersect (downstream_caps, possible_caps)) {
        gst_caps_unref (possible_caps);
        continue;
      }

      gst_caps_unref (possible_caps);
    }

    /* If the format is supported, consider it very high weight */
    if (gst_video_aggregator_supports_format (vagg,
            GST_VIDEO_INFO_FORMAT (&pad->info))) {
      format_number =
          GPOINTER_TO_INT (g_hash_table_lookup (formats_table,
              GINT_TO_POINTER (GST_VIDEO_INFO_FORMAT (&pad->info))));

      format_number += pad->info.width * pad->info.height;

      g_hash_table_replace (formats_table,
          GINT_TO_POINTER (GST_VIDEO_INFO_FORMAT (&pad->info)),
          GINT_TO_POINTER (format_number));
    }

    /* If that pad is the first with alpha, set it as the new best format */
    if (!need_alpha && (pad->priv->needs_alpha
            && (!GST_VIDEO_FORMAT_INFO_HAS_ALPHA (pad->info.finfo)))) {
      need_alpha = TRUE;
      /* Just fallback to ARGB in case we require alpha but the input pad
       * does not have alpha.
       * Do not increment best_format_number in that case. */
      gst_video_info_set_format (best_info,
          GST_VIDEO_FORMAT_ARGB,
          GST_VIDEO_INFO_WIDTH (&pad->info),
          GST_VIDEO_INFO_HEIGHT (&pad->info));
    } else if (!need_alpha
        && (pad->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_ALPHA)) {
      need_alpha = TRUE;
      *best_info = pad->info;
      best_format_number = format_number;
    } else if (format_number > best_format_number) {
      *best_info = pad->info;
      best_format_number = format_number;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  g_hash_table_unref (formats_table);

  if (gst_video_aggregator_supports_format (vagg,
          GST_VIDEO_INFO_FORMAT (best_info))) {
    possible_caps = gst_video_aggregator_get_possible_caps_for_info (best_info);
    if (gst_caps_can_intersect (downstream_caps, possible_caps)) {
      gst_caps_unref (possible_caps);
      return;
    }
    gst_caps_unref (possible_caps);
  }

  for (i = 0; i < vagg->priv->supported_formats->len; i++) {
    GstVideoFormatInfo *format_info = vagg->priv->supported_formats->pdata[i];

    /* either we don't care about alpha, or the output format needs to have
     * alpha */
    if (!need_alpha || GST_VIDEO_FORMAT_INFO_HAS_ALPHA (format_info)) {
      gst_video_info_set_format (best_info, format_info->format,
          best_info->width, best_info->height);
      possible_caps =
          gst_video_aggregator_get_possible_caps_for_info (best_info);

      if (gst_caps_can_intersect (downstream_caps, possible_caps)) {
        GST_INFO_OBJECT (vagg, "Using supported caps: %" GST_PTR_FORMAT,
            possible_caps);
        gst_caps_unref (possible_caps);

        return;
      }

      gst_caps_unref (possible_caps);
    }
  }

  GST_WARNING_OBJECT (vagg, "Nothing compatible with %" GST_PTR_FORMAT,
      downstream_caps);
  gst_video_info_init (best_info);
}

static GstCaps *
gst_video_aggregator_default_fixate_src_caps (GstAggregator * agg,
    GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  gdouble best_fps = -1.;
  GstStructure *s;
  GList *l;

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *mpad = l->data;
    gint fps_n, fps_d;
    gint width, height;
    gdouble cur_fps;

    fps_n = GST_VIDEO_INFO_FPS_N (&mpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&mpad->info);
    width = GST_VIDEO_INFO_WIDTH (&mpad->info);
    height = GST_VIDEO_INFO_HEIGHT (&mpad->info);

    if (width == 0 || height == 0)
      continue;

    if (best_width < width)
      best_width = width;
    if (best_height < height)
      best_height = height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }

  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);
  gst_structure_fixate_field_nearest_int (s, "width", best_width);
  gst_structure_fixate_field_nearest_int (s, "height", best_height);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", best_fps_n,
      best_fps_d);
  if (gst_structure_has_field (s, "pixel-aspect-ratio"))
    gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio", 1, 1);
  caps = gst_caps_fixate (caps);

  return caps;
}

static GstCaps *
gst_video_aggregator_default_update_caps (GstVideoAggregator * vagg,
    GstCaps * caps)
{
  GstVideoAggregatorClass *vagg_klass = GST_VIDEO_AGGREGATOR_GET_CLASS (vagg);
  GstCaps *ret, *best_format_caps;
  gboolean at_least_one_alpha = FALSE;
  GstVideoFormat best_format;
  GstVideoInfo best_info;
  gchar *color_name;
  gchar *chroma_site;

  best_format = GST_VIDEO_FORMAT_UNKNOWN;
  gst_video_info_init (&best_info);

  if (vagg_klass->find_best_format) {
    vagg_klass->find_best_format (vagg, caps, &best_info, &at_least_one_alpha);

    best_format = GST_VIDEO_INFO_FORMAT (&best_info);
  }

  if (best_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GstCaps *tmp = gst_caps_fixate (gst_caps_ref (caps));
    gst_video_info_from_caps (&best_info, tmp);
    best_format = GST_VIDEO_INFO_FORMAT (&best_info);
    gst_caps_unref (tmp);
  }

  color_name = gst_video_colorimetry_to_string (&best_info.colorimetry);
  chroma_site = gst_video_chroma_site_to_string (best_info.chroma_site);

  GST_DEBUG_OBJECT (vagg,
      "The output format will now be : %s with chroma : %s and colorimetry %s",
      gst_video_format_to_string (best_format),
      GST_STR_NULL (chroma_site), GST_STR_NULL (color_name));

  best_format_caps = gst_caps_copy (caps);
  gst_caps_set_simple (best_format_caps, "format", G_TYPE_STRING,
      gst_video_format_to_string (best_format), NULL);
  /*
   * set_simple() will likely create some invalid combination, as it may as an
   * example set format to NV12 with memory:DMABuf caps feature where DMA_DRM
   * format might be the only supported formats. Simply intersect with the
   * original to fix this.
   */
  ret = gst_caps_intersect (best_format_caps, caps);
  gst_caps_replace (&best_format_caps, ret);
  gst_clear_caps (&ret);

  if (chroma_site != NULL)
    gst_caps_set_simple (best_format_caps, "chroma-site", G_TYPE_STRING,
        chroma_site, NULL);
  if (color_name != NULL)
    gst_caps_set_simple (best_format_caps, "colorimetry", G_TYPE_STRING,
        color_name, NULL);

  g_free (color_name);
  g_free (chroma_site);
  ret = gst_caps_merge (best_format_caps, gst_caps_ref (caps));

  return ret;
}

static GstFlowReturn
gst_video_aggregator_default_update_src_caps (GstAggregator * agg,
    GstCaps * caps, GstCaps ** ret)
{
  GstVideoAggregatorClass *vagg_klass = GST_VIDEO_AGGREGATOR_GET_CLASS (agg);
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);

  g_assert (vagg_klass->update_caps);

  *ret = vagg_klass->update_caps (vagg, caps);

  return GST_FLOW_OK;
}

static gboolean
_update_conversion_info (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstVideoAggregatorPad *vaggpad = GST_VIDEO_AGGREGATOR_PAD (pad);
  GstVideoAggregatorPadClass *vaggpad_klass =
      GST_VIDEO_AGGREGATOR_PAD_GET_CLASS (vaggpad);

  if (vaggpad_klass->update_conversion_info) {
    vaggpad_klass->update_conversion_info (vaggpad);
  }

  return TRUE;
}

static gboolean
gst_video_aggregator_default_negotiated_src_caps (GstAggregator * agg,
    GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  gboolean ret = FALSE;
  GstVideoInfo info;
  GList *l;

  GST_INFO_OBJECT (agg->srcpad, "set src caps: %" GST_PTR_FORMAT, caps);

  GST_VIDEO_AGGREGATOR_LOCK (vagg);

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *mpad = l->data;

    if (GST_VIDEO_INFO_WIDTH (&mpad->info) == 0
        || GST_VIDEO_INFO_HEIGHT (&mpad->info) == 0)
      continue;
  }
  GST_OBJECT_UNLOCK (vagg);

  if (!gst_video_info_from_caps (&info, caps))
    goto unlock_and_return;

  if (GST_VIDEO_INFO_FPS_N (&vagg->info) != GST_VIDEO_INFO_FPS_N (&info) ||
      GST_VIDEO_INFO_FPS_D (&vagg->info) != GST_VIDEO_INFO_FPS_D (&info)) {
    if (GST_AGGREGATOR_PAD (agg->srcpad)->segment.position != -1) {
      vagg->priv->nframes = 0;
      /* The timestamp offset will be updated based on the
       * segment position the next time we aggregate */
      GST_DEBUG_OBJECT (vagg,
          "Resetting frame counter because of framerate change");
    }
    gst_video_aggregator_reset_qos (vagg);
  }

  GST_OBJECT_LOCK (vagg);
  vagg->info = info;
  GST_OBJECT_UNLOCK (vagg);

  /* Then browse the sinks once more, setting or unsetting conversion if needed */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg),
      _update_conversion_info, NULL);

  if (vagg->priv->current_caps == NULL ||
      gst_caps_is_equal (caps, vagg->priv->current_caps) == FALSE) {
    GstClockTime latency;

    gst_caps_replace (&vagg->priv->current_caps, caps);

    gst_aggregator_set_src_caps (agg, caps);
    latency = gst_util_uint64_scale (GST_SECOND,
        GST_VIDEO_INFO_FPS_D (&info), GST_VIDEO_INFO_FPS_N (&info));
    gst_aggregator_set_latency (agg, latency, latency);
  }

  ret = TRUE;

unlock_and_return:
  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);
  return ret;
}

static gboolean
gst_video_aggregator_get_sinkpads_interlace_mode (GstVideoAggregator * vagg,
    GstVideoAggregatorPad * skip_pad, GstVideoInterlaceMode * mode)
{
  GList *walk;

  GST_OBJECT_LOCK (vagg);
  for (walk = GST_ELEMENT (vagg)->sinkpads; walk; walk = g_list_next (walk)) {
    GstVideoAggregatorPad *vaggpad = walk->data;

    if (skip_pad && vaggpad == skip_pad)
      continue;
    if (vaggpad->info.finfo
        && GST_VIDEO_INFO_FORMAT (&vaggpad->info) != GST_VIDEO_FORMAT_UNKNOWN) {
      *mode = GST_VIDEO_INFO_INTERLACE_MODE (&vaggpad->info);
      GST_OBJECT_UNLOCK (vagg);
      return TRUE;
    }
  }
  GST_OBJECT_UNLOCK (vagg);
  return FALSE;
}

static gboolean
gst_video_aggregator_pad_sink_setcaps (GstPad * pad, GstObject * parent,
    GstCaps * caps)
{
  GstVideoAggregator *vagg;
  GstVideoAggregatorPad *vaggpad;
  GstVideoInfo info;
  gboolean ret = FALSE;

  GST_INFO_OBJECT (pad, "Setting caps %" GST_PTR_FORMAT, caps);

  vagg = GST_VIDEO_AGGREGATOR (parent);
  vaggpad = GST_VIDEO_AGGREGATOR_PAD (pad);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (pad, "Failed to parse caps");
    goto beach;
  }

  GST_VIDEO_AGGREGATOR_LOCK (vagg);
  {
    GstVideoInterlaceMode pads_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
    gboolean has_mode = FALSE;

    /* get the current output setting or fallback to other pads settings */
    if (GST_VIDEO_INFO_FORMAT (&vagg->info) != GST_VIDEO_FORMAT_UNKNOWN) {
      pads_mode = GST_VIDEO_INFO_INTERLACE_MODE (&vagg->info);
      has_mode = TRUE;
    } else {
      has_mode =
          gst_video_aggregator_get_sinkpads_interlace_mode (vagg, vaggpad,
          &pads_mode);
    }

    if (has_mode) {
      if (pads_mode != GST_VIDEO_INFO_INTERLACE_MODE (&info)) {
        GST_ERROR_OBJECT (pad,
            "got input caps %" GST_PTR_FORMAT ", but current caps are %"
            GST_PTR_FORMAT, caps, vagg->priv->current_caps);
        GST_VIDEO_AGGREGATOR_UNLOCK (vagg);
        return FALSE;
      }
    }
  }

  if (!vaggpad->info.finfo ||
      GST_VIDEO_INFO_FORMAT (&vaggpad->info) == GST_VIDEO_FORMAT_UNKNOWN) {
    /* no video info was already set, so this is the first time
     * that this pad is getting configured; configure immediately to avoid
     * problems with the initial negotiation */
    vaggpad->info = info;
    gst_caps_replace (&vaggpad->priv->caps, caps);
    gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg));
  } else {
    /* this pad already had caps but received new ones; keep the new caps
     * pending until we pick the next buffer from the queue, otherwise we
     * might use an old buffer with the new caps and crash */
    vaggpad->priv->pending_vinfo = info;
    gst_caps_replace (&vaggpad->priv->pending_caps, caps);
    GST_DEBUG_OBJECT (pad, "delaying caps change");
  }
  ret = TRUE;

  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);

beach:
  return ret;
}

static gboolean
gst_video_aggregator_caps_has_alpha (GstCaps * caps)
{
  guint size = gst_caps_get_size (caps);
  guint i;

  for (i = 0; i < size; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);
    const GValue *formats = gst_structure_get_value (s, "format");

    if (formats) {
      const GstVideoFormatInfo *info;

      if (GST_VALUE_HOLDS_LIST (formats)) {
        guint list_size = gst_value_list_get_size (formats);
        guint index;

        for (index = 0; index < list_size; index++) {
          const GValue *list_item = gst_value_list_get_value (formats, index);
          info =
              gst_video_format_get_info (gst_video_format_from_string
              (g_value_get_string (list_item)));
          if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info))
            return TRUE;
        }

      } else if (G_VALUE_HOLDS_STRING (formats)) {
        info =
            gst_video_format_get_info (gst_video_format_from_string
            (g_value_get_string (formats)));
        if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info))
          return TRUE;

      } else {
        g_assert_not_reached ();
        GST_WARNING ("Unexpected type for video 'format' field: %s",
            G_VALUE_TYPE_NAME (formats));
      }

    } else {
      return TRUE;
    }
  }
  return FALSE;
}

static GstCaps *
_get_non_alpha_caps (GstCaps * caps)
{
  GstCaps *result;
  guint i, size;

  size = gst_caps_get_size (caps);
  result = gst_caps_new_empty ();
  for (i = 0; i < size; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);
    const GValue *formats = gst_structure_get_value (s, "format");
    GValue new_formats = { 0, };
    gboolean has_format = FALSE;

    /* FIXME what to do if formats are missing? */
    if (formats) {
      const GstVideoFormatInfo *info;

      if (GST_VALUE_HOLDS_LIST (formats)) {
        guint list_size = gst_value_list_get_size (formats);
        guint index;

        g_value_init (&new_formats, GST_TYPE_LIST);

        for (index = 0; index < list_size; index++) {
          const GValue *list_item = gst_value_list_get_value (formats, index);

          info =
              gst_video_format_get_info (gst_video_format_from_string
              (g_value_get_string (list_item)));
          if (!GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info)) {
            has_format = TRUE;
            gst_value_list_append_value (&new_formats, list_item);
          }
        }

      } else if (G_VALUE_HOLDS_STRING (formats)) {
        info =
            gst_video_format_get_info (gst_video_format_from_string
            (g_value_get_string (formats)));
        if (!GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info)) {
          has_format = TRUE;
          gst_value_init_and_copy (&new_formats, formats);
        }

      } else {
        g_assert_not_reached ();
        GST_WARNING ("Unexpected type for video 'format' field: %s",
            G_VALUE_TYPE_NAME (formats));
      }

      if (has_format) {
        s = gst_structure_copy (s);
        gst_structure_take_value (s, "format", &new_formats);
        gst_caps_append_structure_full (result, s,
            gst_caps_features_copy (gst_caps_get_features (caps, i)));
      }

    }
  }

  return result;
}

static GstCaps *
gst_video_aggregator_pad_sink_getcaps (GstPad * pad, GstVideoAggregator * vagg,
    GstCaps * filter)
{
  GstCaps *srccaps;
  GstCaps *template_caps, *sink_template_caps;
  GstCaps *returned_caps;
  GstStructure *s;
  gint i, n;
  GstAggregator *agg = GST_AGGREGATOR (vagg);
  GstPad *srcpad = GST_PAD (agg->srcpad);
  gboolean has_alpha;
  GstVideoInterlaceMode interlace_mode;
  gboolean has_interlace_mode;

  template_caps = gst_pad_get_pad_template_caps (srcpad);

  GST_DEBUG_OBJECT (pad, "Get caps with filter: %" GST_PTR_FORMAT, filter);

  srccaps = gst_pad_peer_query_caps (srcpad, template_caps);
  srccaps = gst_caps_make_writable (srccaps);
  has_alpha = gst_video_aggregator_caps_has_alpha (srccaps);

  has_interlace_mode =
      gst_video_aggregator_get_sinkpads_interlace_mode (vagg, NULL,
      &interlace_mode);

  n = gst_caps_get_size (srccaps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (srccaps, i);
    gst_structure_set (s, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT,
        1, NULL);

    if (GST_IS_VIDEO_AGGREGATOR_CONVERT_PAD (pad)) {
      gst_structure_set (s, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
      gst_structure_remove_fields (s, "colorimetry", "chroma-site", "format",
          "pixel-aspect-ratio", NULL);
    }

    if (has_interlace_mode)
      gst_structure_set (s, "interlace-mode", G_TYPE_STRING,
          gst_video_interlace_mode_to_string (interlace_mode), NULL);
  }

  if (filter) {
    returned_caps = gst_caps_intersect (srccaps, filter);
    gst_caps_unref (srccaps);
  } else {
    returned_caps = srccaps;
  }

  sink_template_caps = gst_pad_get_pad_template_caps (pad);
  if (!has_alpha) {
    GstCaps *tmp = _get_non_alpha_caps (sink_template_caps);
    gst_caps_unref (sink_template_caps);
    sink_template_caps = tmp;
  }

  {
    GstCaps *intersect = gst_caps_intersect (returned_caps, sink_template_caps);
    gst_caps_unref (returned_caps);
    returned_caps = intersect;
  }

  gst_caps_unref (template_caps);
  gst_caps_unref (sink_template_caps);

  GST_DEBUG_OBJECT (pad, "Returning caps: %" GST_PTR_FORMAT, returned_caps);

  return returned_caps;
}

static void
gst_video_aggregator_update_qos (GstVideoAggregator * vagg, gdouble proportion,
    GstClockTimeDiff diff, GstClockTime timestamp)
{
  gboolean live;

  GST_DEBUG_OBJECT (vagg,
      "Updating QoS: proportion %lf, diff %" GST_STIME_FORMAT ", timestamp %"
      GST_TIME_FORMAT, proportion, GST_STIME_ARGS (diff),
      GST_TIME_ARGS (timestamp));

  live =
      GST_CLOCK_TIME_IS_VALID (gst_aggregator_get_latency (GST_AGGREGATOR
          (vagg)));

  GST_OBJECT_LOCK (vagg);

  vagg->priv->proportion = proportion;
  if (G_LIKELY (timestamp != GST_CLOCK_TIME_NONE)) {
    if (!live && G_UNLIKELY (diff > 0))
      vagg->priv->earliest_time =
          timestamp + 2 * diff + gst_util_uint64_scale_int_round (GST_SECOND,
          GST_VIDEO_INFO_FPS_D (&vagg->info),
          GST_VIDEO_INFO_FPS_N (&vagg->info));
    else
      vagg->priv->earliest_time = timestamp + diff;
  } else {
    vagg->priv->earliest_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (vagg);
}

static void
gst_video_aggregator_reset_qos (GstVideoAggregator * vagg)
{
  gst_video_aggregator_update_qos (vagg, 0.5, 0, GST_CLOCK_TIME_NONE);
  vagg->priv->qos_processed = vagg->priv->qos_dropped = 0;
}

static void
gst_video_aggregator_read_qos (GstVideoAggregator * vagg, gdouble * proportion,
    GstClockTime * time)
{
  GST_OBJECT_LOCK (vagg);
  *proportion = vagg->priv->proportion;
  *time = vagg->priv->earliest_time;
  GST_OBJECT_UNLOCK (vagg);
}

static void
gst_video_aggregator_reset (GstVideoAggregator * vagg)
{
  GstAggregator *agg = GST_AGGREGATOR (vagg);
  GList *l;

  GST_OBJECT_LOCK (vagg);
  gst_video_info_init (&vagg->info);
  GST_OBJECT_UNLOCK (vagg);

  vagg->priv->ts_offset = 0;
  vagg->priv->nframes = 0;
  vagg->priv->live = FALSE;

  GST_AGGREGATOR_PAD (agg->srcpad)->segment.position = -1;

  gst_video_aggregator_reset_qos (vagg);

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *p = l->data;

    gst_buffer_replace (&p->priv->buffer, NULL);
    gst_caps_replace (&p->priv->caps, NULL);
    p->priv->start_time = -1;
    p->priv->end_time = -1;

    gst_video_info_init (&p->info);
  }
  GST_OBJECT_UNLOCK (vagg);
}

static GstFlowReturn
gst_video_aggregator_fill_queues (GstVideoAggregator * vagg,
    GstClockTime output_start_running_time,
    GstClockTime output_end_running_time, gboolean timeout)
{
  GList *l;
  gboolean eos = !gst_aggregator_get_force_live (GST_AGGREGATOR (vagg));
  gboolean repeat_pad_eos = FALSE;
  gboolean has_no_repeat_pads = FALSE;
  gboolean need_more_data = FALSE;
  gboolean need_reconfigure = FALSE;

  /* get a set of buffers into pad->priv->buffer that are within output_start_running_time
   * and output_end_running_time taking into account finished and unresponsive pads */

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *pad = l->data;
    GstSegment segment;
    GstAggregatorPad *bpad;
    GstBuffer *buf;
    gboolean is_eos;

    bpad = GST_AGGREGATOR_PAD (pad);

    if (gst_aggregator_pad_is_inactive (bpad))
      continue;

    GST_OBJECT_LOCK (bpad);
    segment = bpad->segment;
    GST_OBJECT_UNLOCK (bpad);
    is_eos = gst_aggregator_pad_is_eos (bpad);

    if (!is_eos)
      eos = FALSE;
    if (!pad->priv->repeat_after_eos)
      has_no_repeat_pads = TRUE;
    buf = gst_aggregator_pad_peek_buffer (bpad);
    if (buf) {
      GstClockTime start_time, end_time;
      GstClockTime start_running_time, end_running_time;

    check_again:
      GST_TRACE_OBJECT (pad, "Next buffer %" GST_PTR_FORMAT, buf);

      start_time = GST_BUFFER_TIMESTAMP (buf);
      if (start_time == -1) {
        gst_buffer_unref (buf);
        GST_ERROR_OBJECT (pad, "Need timestamped buffers!");
        GST_OBJECT_UNLOCK (vagg);
        return GST_FLOW_ERROR;
      }

      end_time = GST_BUFFER_DURATION (buf);

      if (end_time == -1) {
        start_time = MAX (start_time, segment.start);
        start_time =
            gst_segment_to_running_time (&segment, GST_FORMAT_TIME, start_time);

        if (start_time >= output_end_running_time) {
          if (pad->priv->buffer) {
            GST_DEBUG_OBJECT (pad, "buffer duration is -1, start_time >= "
                "output_end_running_time. Keeping previous buffer");
          } else {
            GST_DEBUG_OBJECT (pad, "buffer duration is -1, start_time >= "
                "output_end_running_time. No previous buffer.");
          }
          gst_buffer_unref (buf);
          continue;
        } else if (start_time < output_start_running_time) {
          GST_DEBUG_OBJECT (pad, "buffer duration is -1, start_time < "
              "output_start_running_time.  Discarding old buffer");
          gst_buffer_replace (&pad->priv->buffer, buf);
          if (pad->priv->pending_vinfo.finfo) {
            gst_caps_replace (&pad->priv->caps, pad->priv->pending_caps);
            gst_caps_replace (&pad->priv->pending_caps, NULL);
            pad->info = pad->priv->pending_vinfo;
            need_reconfigure = TRUE;
            pad->priv->pending_vinfo.finfo = NULL;
          }
          gst_buffer_unref (buf);
          gst_aggregator_pad_drop_buffer (bpad);
          pad->priv->start_time = start_time;
          if (timeout) {
            /* If we're called for a timeout, we want to make sure we drain as
             * much as possible any late data */
            buf = gst_aggregator_pad_peek_buffer (bpad);
            if (buf)
              goto check_again;
          }
          need_more_data = TRUE;
          continue;
        }
        gst_buffer_unref (buf);
        buf = gst_aggregator_pad_pop_buffer (bpad);
        gst_buffer_replace (&pad->priv->buffer, buf);
        if (pad->priv->pending_vinfo.finfo) {
          gst_caps_replace (&pad->priv->caps, pad->priv->pending_caps);
          gst_caps_replace (&pad->priv->pending_caps, NULL);
          pad->info = pad->priv->pending_vinfo;
          need_reconfigure = TRUE;
          pad->priv->pending_vinfo.finfo = NULL;
        }
        /* FIXME: Set end_time to something here? */
        pad->priv->start_time = start_time;
        gst_buffer_unref (buf);
        GST_DEBUG_OBJECT (pad, "buffer duration is -1");
        continue;
      }

      g_assert (start_time != -1 && end_time != -1);
      end_time += start_time;   /* convert from duration to position */

      /* Check if it's inside the segment */
      if (start_time >= segment.stop || end_time < segment.start) {
        GST_DEBUG_OBJECT (pad,
            "Buffer outside the segment : segment: [%" GST_TIME_FORMAT " -- %"
            GST_TIME_FORMAT "]" " Buffer [%" GST_TIME_FORMAT " -- %"
            GST_TIME_FORMAT "]", GST_TIME_ARGS (segment.stop),
            GST_TIME_ARGS (segment.start), GST_TIME_ARGS (start_time),
            GST_TIME_ARGS (end_time));

        gst_buffer_unref (buf);
        gst_aggregator_pad_drop_buffer (bpad);

        need_more_data = TRUE;
        continue;
      }

      /* Clip to segment and convert to running time */
      start_time = MAX (start_time, segment.start);
      if (segment.stop != -1)
        end_time = MIN (end_time, segment.stop);

      if (segment.rate >= 0) {
        start_running_time =
            gst_segment_to_running_time (&segment, GST_FORMAT_TIME, start_time);
        end_running_time =
            gst_segment_to_running_time (&segment, GST_FORMAT_TIME, end_time);
      } else {
        start_running_time =
            gst_segment_to_running_time (&segment, GST_FORMAT_TIME, end_time);
        end_running_time =
            gst_segment_to_running_time (&segment, GST_FORMAT_TIME, start_time);
      }
      g_assert (start_running_time != -1 && end_running_time != -1);

      GST_TRACE_OBJECT (pad, "dealing with buffer %p start %" GST_TIME_FORMAT
          " end %" GST_TIME_FORMAT " out start %" GST_TIME_FORMAT
          " out end %" GST_TIME_FORMAT, buf, GST_TIME_ARGS (start_running_time),
          GST_TIME_ARGS (end_running_time),
          GST_TIME_ARGS (output_start_running_time),
          GST_TIME_ARGS (output_end_running_time));

      if (pad->priv->end_time != -1 && pad->priv->end_time > end_running_time) {
        GST_DEBUG_OBJECT (pad, "Buffer from the past, dropping");
        gst_buffer_unref (buf);
        gst_aggregator_pad_drop_buffer (bpad);
        continue;
      }

      if (end_running_time > output_start_running_time
          && start_running_time < output_end_running_time) {
        GST_DEBUG_OBJECT (pad,
            "Taking new buffer with start time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (start_running_time));

        if ((gst_buffer_get_size (buf) == 0 &&
                GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP) &&
                gst_buffer_get_custom_meta (buf,
                    "GstAggregatorMissingDataMeta"))) {
          GST_DEBUG_OBJECT (pad, "Consuming gap but keeping old buffer around");
        } else {
          gst_buffer_replace (&pad->priv->buffer, buf);
        }

        if (pad->priv->pending_vinfo.finfo) {
          gst_caps_replace (&pad->priv->caps, pad->priv->pending_caps);
          gst_caps_replace (&pad->priv->pending_caps, NULL);
          pad->info = pad->priv->pending_vinfo;
          need_reconfigure = TRUE;
          pad->priv->pending_vinfo.finfo = NULL;
        }
        pad->priv->start_time = start_running_time;
        pad->priv->end_time = end_running_time;

        gst_buffer_unref (buf);
        gst_aggregator_pad_drop_buffer (bpad);
        eos = FALSE;
      } else if (start_running_time >= output_end_running_time) {
        GST_DEBUG_OBJECT (pad, "Keeping buffer until %" GST_TIME_FORMAT,
            GST_TIME_ARGS (start_running_time));
        gst_buffer_unref (buf);
        eos = FALSE;
      } else {
        if ((gst_buffer_get_size (buf) == 0 &&
                GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP) &&
                gst_buffer_get_custom_meta (buf,
                    "GstAggregatorMissingDataMeta"))) {
          GST_DEBUG_OBJECT (pad, "Consuming gap but keeping old buffer around");
        } else {
          gst_buffer_replace (&pad->priv->buffer, buf);
        }

        if (pad->priv->pending_vinfo.finfo) {
          gst_caps_replace (&pad->priv->caps, pad->priv->pending_caps);
          gst_caps_replace (&pad->priv->pending_caps, NULL);
          pad->info = pad->priv->pending_vinfo;
          need_reconfigure = TRUE;
          pad->priv->pending_vinfo.finfo = NULL;
        }
        pad->priv->start_time = start_running_time;
        pad->priv->end_time = end_running_time;
        GST_DEBUG_OBJECT (pad,
            "replacing old buffer with a newer buffer, start %" GST_TIME_FORMAT
            " out end %" GST_TIME_FORMAT, GST_TIME_ARGS (start_running_time),
            GST_TIME_ARGS (output_end_running_time));
        gst_buffer_unref (buf);
        gst_aggregator_pad_drop_buffer (bpad);

        need_more_data = TRUE;
        continue;
      }
    } else {
      if (is_eos && pad->priv->repeat_after_eos) {
        repeat_pad_eos = TRUE;
        GST_DEBUG_OBJECT (pad, "ignoring EOS and re-using previous buffer");
        continue;
      }

      if (pad->priv->end_time != -1) {
        if (pad->priv->end_time <= output_start_running_time) {
          if (!is_eos) {
            GST_DEBUG_OBJECT (pad, "I just need more data");
            if (GST_CLOCK_TIME_IS_VALID (pad->priv->max_last_buffer_repeat)) {
              if (output_start_running_time - pad->priv->end_time >
                  pad->priv->max_last_buffer_repeat) {
                pad->priv->start_time = pad->priv->end_time = -1;
                gst_buffer_replace (&pad->priv->buffer, NULL);
                gst_caps_replace (&pad->priv->caps, NULL);
              }
            } else {
              pad->priv->start_time = pad->priv->end_time = -1;
            }
            need_more_data = TRUE;
          } else {
            gst_buffer_replace (&pad->priv->buffer, NULL);
            gst_caps_replace (&pad->priv->caps, NULL);
            pad->priv->start_time = pad->priv->end_time = -1;
          }
        } else if (is_eos) {
          eos = FALSE;
        }
      } else if (is_eos) {
        gst_buffer_replace (&pad->priv->buffer, NULL);
        gst_caps_replace (&pad->priv->caps, NULL);
      } else if (pad->priv->start_time != -1) {
        /* When the current buffer didn't have a duration, but
         * max-last-buffer-repeat was set, we use start_time as
         * the comparison point
         */
        if (pad->priv->start_time <= output_start_running_time) {
          if (GST_CLOCK_TIME_IS_VALID (pad->priv->max_last_buffer_repeat)) {
            if (output_start_running_time - pad->priv->start_time >
                pad->priv->max_last_buffer_repeat) {
              pad->priv->start_time = pad->priv->end_time = -1;
              gst_buffer_replace (&pad->priv->buffer, NULL);
              gst_caps_replace (&pad->priv->caps, NULL);
            }
          }
        }
      }
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (need_reconfigure)
    gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg));

  if (need_more_data)
    return GST_AGGREGATOR_FLOW_NEED_DATA;
  if (eos && !has_no_repeat_pads && repeat_pad_eos)
    eos = FALSE;
  if (eos)
    return GST_FLOW_EOS;

  return GST_FLOW_OK;
}

static gboolean
sync_pad_values (GstElement * vagg, GstPad * pad, gpointer user_data)
{
  gint64 *out_stream_time = user_data;

  /* sync object properties on stream time */
  if (GST_CLOCK_TIME_IS_VALID (*out_stream_time))
    gst_object_sync_values (GST_OBJECT_CAST (pad), *out_stream_time);

  return TRUE;
}

static gboolean
prepare_frames_start (GstElement * agg, GstPad * pad, gpointer user_data)
{
  GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD_CAST (pad);
  GstVideoAggregatorPadClass *vaggpad_class =
      GST_VIDEO_AGGREGATOR_PAD_GET_CLASS (pad);

  memset (&vpad->priv->prepared_frame, 0, sizeof (GstVideoFrame));

  if (vpad->priv->buffer == NULL || !vaggpad_class->prepare_frame_start)
    return TRUE;

  /* GAP event, nothing to do */
  if (vpad->priv->buffer &&
      gst_buffer_get_size (vpad->priv->buffer) == 0 &&
      GST_BUFFER_FLAG_IS_SET (vpad->priv->buffer, GST_BUFFER_FLAG_GAP)) {
    return TRUE;
  }

  g_return_val_if_fail (vaggpad_class->prepare_frame_start
      && vaggpad_class->prepare_frame_finish, TRUE);

  vaggpad_class->prepare_frame_start (vpad, GST_VIDEO_AGGREGATOR_CAST (agg),
      vpad->priv->buffer, &vpad->priv->prepared_frame);

  return TRUE;
}

static gboolean
prepare_frames_finish (GstElement * agg, GstPad * pad, gpointer user_data)
{
  GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD_CAST (pad);
  GstVideoAggregatorPadClass *vaggpad_class =
      GST_VIDEO_AGGREGATOR_PAD_GET_CLASS (pad);

  if (vpad->priv->buffer == NULL || (!vaggpad_class->prepare_frame
          && !vaggpad_class->prepare_frame_start))
    return TRUE;

  /* GAP event, nothing to do */
  if (vpad->priv->buffer &&
      gst_buffer_get_size (vpad->priv->buffer) == 0 &&
      GST_BUFFER_FLAG_IS_SET (vpad->priv->buffer, GST_BUFFER_FLAG_GAP)) {
    return TRUE;
  }

  if (vaggpad_class->prepare_frame_start && vaggpad_class->prepare_frame_finish) {
    vaggpad_class->prepare_frame_finish (vpad, GST_VIDEO_AGGREGATOR_CAST (agg),
        &vpad->priv->prepared_frame);
    return TRUE;
  } else {
    return vaggpad_class->prepare_frame (vpad, GST_VIDEO_AGGREGATOR_CAST (agg),
        vpad->priv->buffer, &vpad->priv->prepared_frame);
  }
}

static gboolean
clean_pad (GstElement * agg, GstPad * pad, gpointer user_data)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR_CAST (agg);
  GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD_CAST (pad);
  GstVideoAggregatorPadClass *vaggpad_class =
      GST_VIDEO_AGGREGATOR_PAD_GET_CLASS (pad);

  if (vaggpad_class->clean_frame)
    vaggpad_class->clean_frame (vpad, vagg, &vpad->priv->prepared_frame);

  memset (&vpad->priv->prepared_frame, 0, sizeof (GstVideoFrame));

  return TRUE;
}

static GstFlowReturn
gst_video_aggregator_do_aggregate (GstVideoAggregator * vagg,
    GstClockTime output_start_time, GstClockTime output_end_time,
    GstBuffer ** outbuf)
{
  GstAggregator *agg = GST_AGGREGATOR (vagg);
  GstFlowReturn ret = GST_FLOW_OK;
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (vagg);
  GstVideoAggregatorClass *vagg_klass = (GstVideoAggregatorClass *) klass;
  GstClockTime out_stream_time;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  g_assert (vagg_klass->aggregate_frames != NULL);
  g_assert (vagg_klass->create_output_buffer != NULL);

  if ((ret = vagg_klass->create_output_buffer (vagg, outbuf)) != GST_FLOW_OK) {
    GST_WARNING_OBJECT (vagg, "Could not get an output buffer, reason: %s",
        gst_flow_get_name (ret));
    return ret;
  }
  if (*outbuf == NULL) {
    /* sub-class doesn't want to generate output right now */
    return GST_FLOW_OK;
  }

  GST_OBJECT_LOCK (agg->srcpad);
  if (agg_segment->rate >= 0) {
    GST_BUFFER_TIMESTAMP (*outbuf) = output_start_time;
    GST_BUFFER_DURATION (*outbuf) = output_end_time - output_start_time;
    out_stream_time = gst_segment_to_stream_time (agg_segment,
        GST_FORMAT_TIME, output_start_time);
  } else {
    GST_BUFFER_TIMESTAMP (*outbuf) = output_end_time;
    GST_BUFFER_DURATION (*outbuf) = output_start_time - output_end_time;
    out_stream_time = gst_segment_to_stream_time (agg_segment,
        GST_FORMAT_TIME, output_end_time);
  }
  GST_OBJECT_UNLOCK (agg->srcpad);

  /* Sync pad properties to the stream time */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg), sync_pad_values,
      &out_stream_time);

  /* Let the application know that input buffers have been staged */
  gst_aggregator_selected_samples (agg, GST_BUFFER_PTS (*outbuf),
      GST_BUFFER_DTS (*outbuf), GST_BUFFER_DURATION (*outbuf), NULL);

  /* Convert all the frames the subclass has before aggregating */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg), prepare_frames_start,
      NULL);
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg), prepare_frames_finish,
      NULL);

  ret = vagg_klass->aggregate_frames (vagg, *outbuf);

  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg), clean_pad, NULL);

  return ret;
}

/* Perform qos calculations before processing the next frame. Returns TRUE if
 * the frame should be processed, FALSE if the frame can be dropped entirely */
static gint64
gst_video_aggregator_do_qos (GstVideoAggregator * vagg, GstClockTime timestamp)
{
  GstAggregator *agg = GST_AGGREGATOR (vagg);
  GstClockTime qostime, earliest_time;
  gdouble proportion;
  gint64 jitter;

  /* no timestamp, can't do QoS => process frame */
  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (timestamp))) {
    GST_LOG_OBJECT (vagg, "invalid timestamp, can't do QoS, process frame");
    return -1;
  }

  /* get latest QoS observation values */
  gst_video_aggregator_read_qos (vagg, &proportion, &earliest_time);

  /* skip qos if we have no observation (yet) => process frame */
  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (earliest_time))) {
    GST_LOG_OBJECT (vagg, "no observation yet, process frame");
    return -1;
  }

  /* qos is done on running time */
  qostime =
      gst_segment_to_running_time (&GST_AGGREGATOR_PAD (agg->srcpad)->segment,
      GST_FORMAT_TIME, timestamp);

  /* see how our next timestamp relates to the latest qos timestamp */
  GST_LOG_OBJECT (vagg, "qostime %" GST_TIME_FORMAT ", earliest %"
      GST_TIME_FORMAT, GST_TIME_ARGS (qostime), GST_TIME_ARGS (earliest_time));

  jitter = GST_CLOCK_DIFF (qostime, earliest_time);
  if (qostime != GST_CLOCK_TIME_NONE && jitter > 0) {
    GST_DEBUG_OBJECT (vagg, "we are late, drop frame");
    return jitter;
  }

  GST_LOG_OBJECT (vagg, "process frame");
  return jitter;
}

static void
gst_video_aggregator_advance_on_timeout (GstVideoAggregator * vagg)
{
  GstAggregator *agg = GST_AGGREGATOR (vagg);
  guint64 frame_duration;
  gint fps_d, fps_n;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  GST_OBJECT_LOCK (agg);
  if (agg_segment->position == -1) {
    if (agg_segment->rate > 0.0)
      agg_segment->position = agg_segment->start;
    else
      agg_segment->position = agg_segment->stop;
  }

  /* Advance position */
  fps_d = GST_VIDEO_INFO_FPS_D (&vagg->info) ?
      GST_VIDEO_INFO_FPS_D (&vagg->info) : 1;
  fps_n = GST_VIDEO_INFO_FPS_N (&vagg->info) ?
      GST_VIDEO_INFO_FPS_N (&vagg->info) : 25;
  /* Default to 25/1 if no "best fps" is known */
  frame_duration = gst_util_uint64_scale (GST_SECOND, fps_d, fps_n);
  if (agg_segment->rate > 0.0)
    agg_segment->position += frame_duration;
  else if (agg_segment->position > frame_duration)
    agg_segment->position -= frame_duration;
  else
    agg_segment->position = 0;
  vagg->priv->nframes++;
  GST_OBJECT_UNLOCK (agg);
}

static GstFlowReturn
gst_video_aggregator_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstClockTime output_start_time, output_end_time;
  GstClockTime output_start_running_time, output_end_running_time;
  GstBuffer *outbuf = NULL;
  GstFlowReturn flow_ret;
  gint64 jitter;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  GST_VIDEO_AGGREGATOR_LOCK (vagg);

  if (GST_VIDEO_INFO_FORMAT (&vagg->info) == GST_VIDEO_FORMAT_UNKNOWN) {
    if (timeout)
      gst_video_aggregator_advance_on_timeout (vagg);
    flow_ret = GST_AGGREGATOR_FLOW_NEED_DATA;
    goto unlock_and_return;
  }

  if (agg_segment->rate < 0 && !GST_CLOCK_TIME_IS_VALID (agg_segment->stop)) {
    GST_ERROR_OBJECT (vagg, "Unknown segment.stop for negative rate");
    flow_ret = GST_FLOW_ERROR;
    goto unlock_and_return;
  }

  output_start_time = agg_segment->position;
  if (agg_segment->rate >= 0) {
    if (agg_segment->position == -1 ||
        agg_segment->position < agg_segment->start) {
      output_start_time = agg_segment->start;
    }
  } else {
    if (agg_segment->position == -1 ||
        agg_segment->position > agg_segment->stop) {
      output_start_time = agg_segment->stop;
    }
  }

  if (vagg->priv->nframes == 0) {
    vagg->priv->ts_offset = output_start_time;
    GST_DEBUG_OBJECT (vagg, "New ts offset %" GST_TIME_FORMAT,
        GST_TIME_ARGS (output_start_time));
  }

  if (GST_VIDEO_INFO_FPS_N (&vagg->info) == 0) {
    output_end_time = -1;
  } else {
    guint64 dur = gst_util_uint64_scale (vagg->priv->nframes + 1,
        GST_SECOND * GST_VIDEO_INFO_FPS_D (&vagg->info),
        GST_VIDEO_INFO_FPS_N (&vagg->info));

    if (agg_segment->rate >= 0)
      output_end_time = vagg->priv->ts_offset + dur;
    else if (vagg->priv->ts_offset >= dur)
      output_end_time = vagg->priv->ts_offset - dur;
    else
      output_end_time = -1;
  }

  if (agg_segment->rate >= 0) {
    if (agg_segment->stop != -1)
      output_end_time = MIN (output_end_time, agg_segment->stop);
  } else {
    if (agg_segment->start != -1)
      output_end_time = MAX (output_end_time, agg_segment->start);
  }

  output_start_running_time =
      gst_segment_to_running_time (agg_segment, GST_FORMAT_TIME,
      output_start_time);
  output_end_running_time =
      gst_segment_to_running_time (agg_segment, GST_FORMAT_TIME,
      output_end_time);

  if (output_end_time == output_start_time) {
    flow_ret = GST_FLOW_EOS;
  } else {
    flow_ret =
        gst_video_aggregator_fill_queues (vagg, output_start_running_time,
        output_end_running_time, timeout);
  }

  if (flow_ret == GST_AGGREGATOR_FLOW_NEED_DATA && !timeout) {
    GST_DEBUG_OBJECT (vagg, "Need more data for decisions");
    goto unlock_and_return;
  } else if (flow_ret == GST_FLOW_EOS) {
    GST_DEBUG_OBJECT (vagg, "All sinkpads are EOS -- forwarding");
    goto unlock_and_return;
  } else if (flow_ret == GST_FLOW_ERROR) {
    GST_WARNING_OBJECT (vagg, "Error collecting buffers");
    goto unlock_and_return;
  }

  /* It is possible that gst_video_aggregator_fill_queues() marked the pad
   * for reconfiguration. In this case we have to reconfigure before continuing
   * because we have picked a new buffer with different caps than before from
   * one one of the sink pads and continuing here may lead to a crash.
   * https://bugzilla.gnome.org/show_bug.cgi?id=780682
   */
  if (gst_pad_needs_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg))) {
    GST_DEBUG_OBJECT (vagg, "Need reconfigure");
    flow_ret = GST_AGGREGATOR_FLOW_NEED_DATA;
    goto unlock_and_return;
  }

  GST_DEBUG_OBJECT (vagg,
      "Producing buffer for %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT
      ", running time start %" GST_TIME_FORMAT ", running time end %"
      GST_TIME_FORMAT, GST_TIME_ARGS (output_start_time),
      GST_TIME_ARGS (output_end_time),
      GST_TIME_ARGS (output_start_running_time),
      GST_TIME_ARGS (output_end_running_time));

  jitter = gst_video_aggregator_do_qos (vagg, output_start_time);
  if (jitter <= 0) {
    flow_ret = gst_video_aggregator_do_aggregate (vagg, output_start_time,
        output_end_time, &outbuf);
    if (flow_ret != GST_FLOW_OK)
      goto done;
    vagg->priv->qos_processed++;
  } else {
    GstMessage *msg;

    vagg->priv->qos_dropped++;

    msg =
        gst_message_new_qos (GST_OBJECT_CAST (vagg), vagg->priv->live,
        output_start_running_time, gst_segment_to_stream_time (agg_segment,
            GST_FORMAT_TIME, output_start_time), output_start_time,
        output_end_time - output_start_time);
    gst_message_set_qos_values (msg, jitter, vagg->priv->proportion, 1000000);
    gst_message_set_qos_stats (msg, GST_FORMAT_BUFFERS,
        vagg->priv->qos_processed, vagg->priv->qos_dropped);
    gst_element_post_message (GST_ELEMENT_CAST (vagg), msg);

    flow_ret = GST_FLOW_OK;
  }

  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);
  if (outbuf) {
    GST_DEBUG_OBJECT (vagg,
        "Pushing buffer with ts %" GST_TIME_FORMAT " and duration %"
        GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)));

    flow_ret = gst_aggregator_finish_buffer (agg, outbuf);
  }

  GST_VIDEO_AGGREGATOR_LOCK (vagg);
  vagg->priv->nframes++;
  agg_segment->position = output_end_time;
  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);

  return flow_ret;

done:
  if (outbuf)
    gst_buffer_unref (outbuf);
unlock_and_return:
  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);
  return flow_ret;
}

/* FIXME, the duration query should reflect how long you will produce
 * data, that is the amount of stream time until you will emit EOS.
 *
 * For synchronized aggregating this is always the max of all the durations
 * of upstream since we emit EOS when all of them finished.
 *
 * We don't do synchronized aggregating so this really depends on where the
 * streams where punched in and what their relative offsets are against
 * each other which we can get from the first timestamps we see.
 *
 * When we add a new stream (or remove a stream) the duration might
 * also become invalid again and we need to post a new DURATION
 * message to notify this fact to the parent.
 * For now we take the max of all the upstream elements so the simple
 * cases work at least somewhat.
 */
static gboolean
gst_video_aggregator_query_duration (GstVideoAggregator * vagg,
    GstQuery * query)
{
  GValue item = { 0 };
  gint64 max;
  gboolean res;
  GstFormat format;
  GstIterator *it;
  gboolean done;

  /* parse format */
  gst_query_parse_duration (query, &format, NULL);

  max = -1;
  res = TRUE;
  done = FALSE;

  /* Take maximum of all durations */
  it = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (vagg));
  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
      case GST_ITERATOR_OK:
      {
        GstPad *pad;
        gint64 duration;

        pad = g_value_get_object (&item);

        /* ask sink peer for duration */
        res &= gst_pad_peer_query_duration (pad, format, &duration);
        /* take max from all valid return values */
        if (res) {
          /* valid unknown length, stop searching */
          if (duration == -1) {
            max = duration;
            done = TRUE;
          }
          /* else see if bigger than current max */
          else if (duration > max)
            max = duration;
        }
        g_value_reset (&item);
        break;
      }
      case GST_ITERATOR_RESYNC:
        max = -1;
        res = TRUE;
        gst_iterator_resync (it);
        break;
      default:
        res = FALSE;
        done = TRUE;
        break;
    }
  }
  g_value_unset (&item);
  gst_iterator_free (it);

  if (res) {
    /* and store the max */
    GST_DEBUG_OBJECT (vagg, "Total duration in format %s: %"
        GST_TIME_FORMAT, gst_format_get_name (format), GST_TIME_ARGS (max));
    gst_query_set_duration (query, format, max);
  }

  return res;
}

static gboolean
gst_video_aggregator_src_query (GstAggregator * agg, GstQuery * query)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  gboolean res = FALSE;
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_TIME:
          gst_query_set_position (query, format,
              gst_segment_to_stream_time (agg_segment, GST_FORMAT_TIME,
                  agg_segment->position));
          res = TRUE;
          break;
        default:
          break;
      }
      break;
    }
    case GST_QUERY_DURATION:
      res = gst_video_aggregator_query_duration (vagg, query);
      break;
    case GST_QUERY_LATENCY:
      res =
          GST_AGGREGATOR_CLASS (gst_video_aggregator_parent_class)->src_query
          (agg, query);

      if (res) {
        gst_query_parse_latency (query, &vagg->priv->live, NULL, NULL);
      }
      break;
    default:
      res =
          GST_AGGREGATOR_CLASS (gst_video_aggregator_parent_class)->src_query
          (agg, query);
      break;
  }
  return res;
}

static gboolean
gst_video_aggregator_src_event (GstAggregator * agg, GstEvent * event)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    {
      GstQOSType type;
      GstClockTimeDiff diff;
      GstClockTime timestamp;
      gdouble proportion;

      gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp);
      gst_video_aggregator_update_qos (vagg, proportion, diff, timestamp);
      break;
    }
    case GST_EVENT_SEEK:
    {
      GST_DEBUG_OBJECT (vagg, "Handling SEEK event");
    }
    default:
      break;
  }

  return
      GST_AGGREGATOR_CLASS (gst_video_aggregator_parent_class)->src_event (agg,
      event);
}

static GstFlowReturn
gst_video_aggregator_flush (GstAggregator * agg)
{
  GList *l;
  gdouble abs_rate;
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (agg->srcpad)->segment;

  GST_INFO_OBJECT (agg, "Flushing");
  GST_OBJECT_LOCK (vagg);
  abs_rate = ABS (agg_segment->rate);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *p = l->data;

    /* Convert to the output segment rate */
    if (ABS (agg_segment->rate) != abs_rate) {
      if (ABS (agg_segment->rate) != 1.0 && p->priv->buffer) {
        p->priv->start_time /= ABS (agg_segment->rate);
        p->priv->end_time /= ABS (agg_segment->rate);
      }
      if (abs_rate != 1.0 && p->priv->buffer) {
        p->priv->start_time *= abs_rate;
        p->priv->end_time *= abs_rate;
      }
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  agg_segment->position = -1;
  vagg->priv->ts_offset = 0;
  vagg->priv->nframes = 0;

  gst_video_aggregator_reset_qos (vagg);
  return GST_FLOW_OK;
}

static gboolean
gst_video_aggregator_sink_event (GstAggregator * agg, GstAggregatorPad * bpad,
    GstEvent * event)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (bpad);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (pad, "Got %s event on pad %s:%s",
      GST_EVENT_TYPE_NAME (event), GST_DEBUG_PAD_NAME (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret =
          gst_video_aggregator_pad_sink_setcaps (GST_PAD (pad),
          GST_OBJECT (vagg), caps);
      gst_event_unref (event);
      event = NULL;
      break;
    }
    case GST_EVENT_SEGMENT:{
      GstSegment seg;
      gst_event_copy_segment (event, &seg);

      g_assert (seg.format == GST_FORMAT_TIME);
      gst_video_aggregator_reset_qos (vagg);
      break;
    }
    default:
      break;
  }

  if (event != NULL)
    return GST_AGGREGATOR_CLASS (gst_video_aggregator_parent_class)->sink_event
        (agg, bpad, event);

  return ret;
}

static gboolean
gst_video_aggregator_start (GstAggregator * agg)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);

  gst_caps_replace (&vagg->priv->current_caps, NULL);

  return TRUE;
}

static gboolean
gst_video_aggregator_stop (GstAggregator * agg)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);

  gst_video_aggregator_reset (vagg);

  return TRUE;
}

/* GstElement vmethods */
static GstPad *
gst_video_aggregator_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstVideoAggregator *vagg;
  GstVideoAggregatorPad *vaggpad;

  vagg = GST_VIDEO_AGGREGATOR (element);

  vaggpad = (GstVideoAggregatorPad *)
      GST_ELEMENT_CLASS (gst_video_aggregator_parent_class)->request_new_pad
      (element, templ, req_name, caps);

  if (vaggpad == NULL)
    return NULL;

  GST_OBJECT_LOCK (vagg);
  vaggpad->priv->zorder = GST_ELEMENT (vagg)->numsinkpads;
  vaggpad->priv->start_time = -1;
  vaggpad->priv->end_time = -1;
  element->sinkpads = g_list_sort (element->sinkpads,
      (GCompareFunc) pad_zorder_compare);
  GST_OBJECT_UNLOCK (vagg);

  return GST_PAD (vaggpad);
}

static void
gst_video_aggregator_release_pad (GstElement * element, GstPad * pad)
{
  GstVideoAggregator *vagg = NULL;
  GstVideoAggregatorPad *vaggpad;
  gboolean last_pad;

  vagg = GST_VIDEO_AGGREGATOR (element);
  vaggpad = GST_VIDEO_AGGREGATOR_PAD (pad);

  GST_VIDEO_AGGREGATOR_LOCK (vagg);

  GST_OBJECT_LOCK (vagg);
  last_pad = (GST_ELEMENT (vagg)->numsinkpads - 1 == 0);
  GST_OBJECT_UNLOCK (vagg);

  if (last_pad)
    gst_video_aggregator_reset (vagg);

  gst_buffer_replace (&vaggpad->priv->buffer, NULL);
  gst_caps_replace (&vaggpad->priv->caps, NULL);
  gst_caps_replace (&vaggpad->priv->pending_caps, NULL);

  GST_ELEMENT_CLASS (gst_video_aggregator_parent_class)->release_pad
      (GST_ELEMENT (vagg), pad);

  gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg));

  GST_VIDEO_AGGREGATOR_UNLOCK (vagg);
  return;
}

static gboolean
gst_video_aggregator_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static gboolean
gst_video_aggregator_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstAllocationParams params = { 0, 15, 0, 0 };
  guint i;
  GstBufferPool *pool;
  GstAllocator *allocator;
  guint size, min, max;
  gboolean update = FALSE;
  GstStructure *config = NULL;
  GstCaps *caps = NULL;

  if (gst_query_get_n_allocation_params (query) == 0) {
    gst_query_add_allocation_param (query, NULL, &params);
  } else {
    for (i = 0; i < gst_query_get_n_allocation_params (query); i++) {
      GstAllocator *allocator;

      gst_query_parse_nth_allocation_param (query, i, &allocator, &params);
      params.align = MAX (params.align, 15);
      gst_query_set_nth_allocation_param (query, i, allocator, &params);
    }
  }

  gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    /* adjust size */
    size = MAX (size, vagg->info.size);
    update = TRUE;
  } else {
    pool = NULL;
    size = vagg->info.size;
    min = max = 0;
    update = FALSE;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  /* no downstream pool, make our own */
  if (pool == NULL)
    pool = gst_video_buffer_pool_new ();

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_set_allocator (config, allocator, &params);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }

  /* buffer pool may have to do some changes */
  if (!gst_buffer_pool_set_config (pool, config)) {
    config = gst_buffer_pool_get_config (pool);

    /* If change are not acceptable, fallback to generic pool */
    if (!gst_buffer_pool_config_validate_params (config, caps, size, min, max)) {
      GST_DEBUG_OBJECT (agg, "unsupported pool, making new pool");

      gst_object_unref (pool);
      pool = gst_video_buffer_pool_new ();
      gst_buffer_pool_config_set_params (config, caps, size, min, max);
      gst_buffer_pool_config_set_allocator (config, allocator, &params);

      if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
        gst_buffer_pool_config_add_option (config,
            GST_BUFFER_POOL_OPTION_VIDEO_META);
      }
    }

    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  if (update)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);
  if (allocator)
    gst_object_unref (allocator);

  return TRUE;

config_failed:
  if (pool)
    gst_object_unref (pool);
  if (allocator)
    gst_object_unref (allocator);

  GST_ELEMENT_ERROR (agg, RESOURCE, SETTINGS,
      ("Failed to configure the buffer pool"),
      ("Configuration is most likely invalid, please report this issue."));
  return FALSE;
}

static GstFlowReturn
gst_video_aggregator_create_output_buffer (GstVideoAggregator * videoaggregator,
    GstBuffer ** outbuf)
{
  GstAggregator *aggregator = GST_AGGREGATOR (videoaggregator);
  GstBufferPool *pool;
  GstFlowReturn ret = GST_FLOW_OK;

  pool = gst_aggregator_get_buffer_pool (aggregator);

  if (pool) {
    if (!gst_buffer_pool_is_active (pool)) {
      if (!gst_buffer_pool_set_active (pool, TRUE)) {
        GST_ELEMENT_ERROR (videoaggregator, RESOURCE, SETTINGS,
            ("failed to activate bufferpool"),
            ("failed to activate bufferpool"));
        gst_object_unref (pool);
        return GST_FLOW_ERROR;
      }
    }

    ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
    gst_object_unref (pool);
  } else {
    guint outsize;
    GstAllocator *allocator;
    GstAllocationParams params;

    gst_aggregator_get_allocator (aggregator, &allocator, &params);

    outsize = GST_VIDEO_INFO_SIZE (&videoaggregator->info);
    *outbuf = gst_buffer_new_allocate (allocator, outsize, &params);

    if (allocator)
      gst_object_unref (allocator);

    if (*outbuf == NULL) {
      GST_ELEMENT_ERROR (videoaggregator, RESOURCE, NO_SPACE_LEFT,
          (NULL), ("Could not acquire buffer of size: %d", outsize));
      ret = GST_FLOW_ERROR;
    }
  }
  return ret;
}

static gboolean
gst_video_aggregator_pad_sink_acceptcaps (GstPad * pad,
    GstVideoAggregator * vagg, GstCaps * caps)
{
  gboolean ret;
  GstCaps *accepted_caps;
  gint i, n;
  GstStructure *s;
  GstAggregator *agg = GST_AGGREGATOR (vagg);

  GST_DEBUG_OBJECT (pad, "%" GST_PTR_FORMAT, caps);

  accepted_caps = gst_pad_get_current_caps (GST_PAD (agg->srcpad));

  if (accepted_caps == NULL)
    accepted_caps = gst_pad_get_pad_template_caps (GST_PAD (agg->srcpad));

  accepted_caps = gst_caps_make_writable (accepted_caps);

  GST_LOG_OBJECT (pad, "src caps %" GST_PTR_FORMAT, accepted_caps);

  n = gst_caps_get_size (accepted_caps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (accepted_caps, i);
    gst_structure_set (s, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT,
        1, NULL);

    if (GST_IS_VIDEO_AGGREGATOR_CONVERT_PAD (pad)) {
      gst_structure_set (s, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
      gst_structure_remove_fields (s, "colorimetry", "chroma-site", "format",
          "pixel-aspect-ratio", NULL);
    }
  }

  ret = gst_caps_can_intersect (caps, accepted_caps);
  GST_DEBUG_OBJECT (pad, "%saccepted caps %" GST_PTR_FORMAT,
      (ret ? "" : "not "), caps);
  gst_caps_unref (accepted_caps);
  return ret;
}

static gboolean
gst_video_aggregator_sink_query (GstAggregator * agg, GstAggregatorPad * bpad,
    GstQuery * query)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (bpad);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps =
          gst_video_aggregator_pad_sink_getcaps (GST_PAD (pad), vagg, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;

      gst_query_parse_accept_caps (query, &caps);
      ret =
          gst_video_aggregator_pad_sink_acceptcaps (GST_PAD (pad), vagg, caps);
      gst_query_set_accept_caps_result (query, ret);
      ret = TRUE;
      break;
    }
    default:
      ret =
          GST_AGGREGATOR_CLASS (gst_video_aggregator_parent_class)->sink_query
          (agg, bpad, query);
      break;
  }
  return ret;
}

/**
 * gst_video_aggregator_get_execution_task_pool:
 * @vagg: the #GstVideoAggregator
 *
 * The returned #GstTaskPool is used internally for performing parallel
 * video format conversions/scaling/etc during the
 * #GstVideoAggregatorPadClass::prepare_frame_start() process.
 * Subclasses can add their own operation to perform using the returned
 * #GstTaskPool during #GstVideoAggregatorClass::aggregate_frames().
 *
 * Returns: (transfer full): the #GstTaskPool that can be used by subclasses
 *     for performing concurrent operations
 *
 * Since: 1.20
 */
GstTaskPool *
gst_video_aggregator_get_execution_task_pool (GstVideoAggregator * vagg)
{
  g_return_val_if_fail (GST_IS_VIDEO_AGGREGATOR (vagg), NULL);

  return gst_object_ref (vagg->priv->task_pool);
}

/* GObject vmethods */
static void
gst_video_aggregator_finalize (GObject * o)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (o);

  g_mutex_clear (&vagg->priv->lock);
  g_ptr_array_unref (vagg->priv->supported_formats);

  if (vagg->priv->task_pool)
    gst_task_pool_cleanup (vagg->priv->task_pool);
  gst_clear_object (&vagg->priv->task_pool);

  G_OBJECT_CLASS (gst_video_aggregator_parent_class)->finalize (o);
}

static void
gst_video_aggregator_dispose (GObject * o)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (o);

  gst_caps_replace (&vagg->priv->current_caps, NULL);

  G_OBJECT_CLASS (gst_video_aggregator_parent_class)->dispose (o);
}

static void
gst_video_aggregator_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_FORCE_LIVE:
      g_value_set_boolean (value,
          gst_aggregator_get_force_live (GST_AGGREGATOR (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_aggregator_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_FORCE_LIVE:
      gst_aggregator_set_force_live (GST_AGGREGATOR (object),
          g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GObject boilerplate */
static void
gst_video_aggregator_class_init (GstVideoAggregatorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_video_aggregator_debug, "videoaggregator", 0,
      "base video aggregator");

  gst_video_aggregator_parent_class = g_type_class_peek_parent (klass);

  if (video_aggregator_private_offset != 0)
    g_type_class_adjust_private_offset (klass,
        &video_aggregator_private_offset);

  gobject_class->finalize = gst_video_aggregator_finalize;
  gobject_class->dispose = gst_video_aggregator_dispose;

  gobject_class->get_property = gst_video_aggregator_get_property;
  gobject_class->set_property = gst_video_aggregator_set_property;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_video_aggregator_release_pad);

  agg_class->start = gst_video_aggregator_start;
  agg_class->stop = gst_video_aggregator_stop;
  agg_class->sink_query = gst_video_aggregator_sink_query;
  agg_class->sink_event = gst_video_aggregator_sink_event;
  agg_class->flush = gst_video_aggregator_flush;
  agg_class->aggregate = gst_video_aggregator_aggregate;
  agg_class->src_event = gst_video_aggregator_src_event;
  agg_class->src_query = gst_video_aggregator_src_query;
  agg_class->get_next_time = gst_aggregator_simple_get_next_time;
  agg_class->update_src_caps = gst_video_aggregator_default_update_src_caps;
  agg_class->fixate_src_caps = gst_video_aggregator_default_fixate_src_caps;
  agg_class->negotiated_src_caps =
      gst_video_aggregator_default_negotiated_src_caps;
  agg_class->decide_allocation = gst_video_aggregator_decide_allocation;
  agg_class->propose_allocation = gst_video_aggregator_propose_allocation;
  agg_class->peek_next_sample = gst_video_aggregator_peek_next_sample;

  klass->find_best_format = gst_video_aggregator_find_best_format;
  klass->create_output_buffer = gst_video_aggregator_create_output_buffer;
  klass->update_caps = gst_video_aggregator_default_update_caps;

  /* Register the pad class */
  g_type_class_ref (GST_TYPE_VIDEO_AGGREGATOR_PAD);

  /**
   * GstVideoAggregator:force-live:
   *
   * Causes the element to aggregate on a timeout even when no live source is
   * connected to its sinks. See #GstAggregator:min-upstream-latency for a
   * companion property: in the vast majority of cases where you plan to plug in
   * live sources with a non-zero latency, you should set it to a non-zero value.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_FORCE_LIVE,
      g_param_spec_boolean ("force-live", "Force live",
          "Always operate in live mode and aggregate on timeout regardless of "
          "whether any live sources are linked upstream",
          DEFAULT_FORCE_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_video_aggregator_init (GstVideoAggregator * vagg,
    GstVideoAggregatorClass * klass)
{
  GstCaps *src_template;
  GstPadTemplate *pad_template;
  gint i;

  vagg->priv = gst_video_aggregator_get_instance_private (vagg);
  vagg->priv->current_caps = NULL;

  g_mutex_init (&vagg->priv->lock);

  /* initialize variables */
  gst_video_aggregator_reset (vagg);

  /* Finding all supported formats */
  vagg->priv->supported_formats = g_ptr_array_new ();
  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  src_template = gst_pad_template_get_caps (pad_template);
  for (i = 0; i < gst_caps_get_size (src_template); i++) {
    const GValue *v =
        gst_structure_get_value (gst_caps_get_structure (src_template, i),
        "format");

    if (G_VALUE_HOLDS_STRING (v)) {
      GstVideoFormat f = gst_video_format_from_string (g_value_get_string (v));
      GstVideoFormatInfo *format_info =
          (GstVideoFormatInfo *) gst_video_format_get_info (f);
      g_ptr_array_add (vagg->priv->supported_formats, format_info);
      continue;
    }

    if (GST_VALUE_HOLDS_LIST (v)) {
      gint j;

      for (j = 0; j < gst_value_list_get_size (v); j++) {
        const GValue *v1 = gst_value_list_get_value (v, j);
        GstVideoFormat f =
            gst_video_format_from_string (g_value_get_string (v1));
        GstVideoFormatInfo *format_info =
            (GstVideoFormatInfo *) gst_video_format_get_info (f);
        g_ptr_array_add (vagg->priv->supported_formats, format_info);
      }
    }
  }

  gst_caps_unref (src_template);

  vagg->priv->task_pool = gst_shared_task_pool_new ();
  gst_shared_task_pool_set_max_threads (GST_SHARED_TASK_POOL (vagg->
          priv->task_pool), g_get_num_processors ());
  gst_task_pool_prepare (vagg->priv->task_pool, NULL);
}
