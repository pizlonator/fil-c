/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2003,2004 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2007-2008 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * SECTION:element-audioresample
 * @title: audioresample
 *
 * audioresample resamples raw audio buffers to different sample rates using
 * a configurable windowing function to enhance quality.
 *
 * By default, the resampler uses a reduced sinc table, with cubic interpolation filling in
 * the gaps. This ensures that the table does not become too big. However, the interpolation
 * increases the CPU usage considerably. As an alternative, a full sinc table can be used.
 * Doing so can drastically reduce CPU usage (4x faster with 44.1 -> 48 kHz conversions for
 * example), at the cost of increased memory consumption, plus the sinc table takes longer
 * to initialize when the element is created. A third mode exists, which uses the full table
 * unless said table would become too large, in which case the interpolated one is used instead.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/audio.ogg ! audioconvert ! audioresample ! audio/x-raw, rate=8000 ! autoaudiosink
 * ]|
 *  Decode an audio file and downsample it to 8Khz and play sound.
 * To create the Ogg/Vorbis file refer to the documentation of vorbisenc.
 * This assumes there is an audio sink that will accept/handle 8kHz audio.
 *
 */

/* TODO:
 *  - Enable SSE/ARM optimizations and select at runtime
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include "gstaudioresample.h"
#include <gst/gstutils.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasetransform.h>

GST_DEBUG_CATEGORY (audio_resample_debug);
#define GST_CAT_DEFAULT audio_resample_debug

#undef USE_SPEEX

#define DEFAULT_QUALITY GST_AUDIO_RESAMPLER_QUALITY_DEFAULT
#define DEFAULT_RESAMPLE_METHOD GST_AUDIO_RESAMPLER_METHOD_KAISER
#define DEFAULT_SINC_FILTER_MODE GST_AUDIO_RESAMPLER_FILTER_MODE_AUTO
#define DEFAULT_SINC_FILTER_AUTO_THRESHOLD (1*1048576)
#define DEFAULT_SINC_FILTER_INTERPOLATION GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_CUBIC

enum
{
  PROP_0,
  PROP_QUALITY,
  PROP_RESAMPLE_METHOD,
  PROP_SINC_FILTER_MODE,
  PROP_SINC_FILTER_AUTO_THRESHOLD,
  PROP_SINC_FILTER_INTERPOLATION
};

#define SUPPORTED_CAPS \
  GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL) \
  ", layout = (string) { interleaved, non-interleaved }"

static GstStaticPadTemplate gst_audio_resample_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SUPPORTED_CAPS));

static GstStaticPadTemplate gst_audio_resample_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SUPPORTED_CAPS));

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_AUDIO meta_tag_audio_quark
static GQuark meta_tag_audio_quark;

static void gst_audio_resample_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_audio_resample_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

/* vmethods */
static gboolean gst_audio_resample_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, gsize * size);
static GstCaps *gst_audio_resample_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_audio_resample_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_audio_resample_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * incaps, gsize insize,
    GstCaps * outcaps, gsize * outsize);
static gboolean gst_audio_resample_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_audio_resample_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_audio_resample_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static GstFlowReturn gst_audio_resample_submit_input_buffer (GstBaseTransform *
    base, gboolean is_discont, GstBuffer * input);
static gboolean gst_audio_resample_sink_event (GstBaseTransform * base,
    GstEvent * event);
static gboolean gst_audio_resample_start (GstBaseTransform * base);
static gboolean gst_audio_resample_stop (GstBaseTransform * base);
static gboolean gst_audio_resample_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static void gst_audio_resample_push_drain (GstAudioResample * resample,
    guint history_len);

#define gst_audio_resample_parent_class parent_class
G_DEFINE_TYPE (GstAudioResample, gst_audio_resample, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (audioresample, "audioresample",
    GST_RANK_PRIMARY, GST_TYPE_AUDIO_RESAMPLE,
    GST_DEBUG_CATEGORY_INIT (audio_resample_debug, "audioresample", 0,
        "audio resampling element"));
static void
gst_audio_resample_class_init (GstAudioResampleClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_audio_resample_set_property;
  gobject_class->get_property = gst_audio_resample_get_property;

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_int ("quality", "Quality", "Resample quality with 0 being "
          "the lowest and 10 being the best",
          GST_AUDIO_RESAMPLER_QUALITY_MIN, GST_AUDIO_RESAMPLER_QUALITY_MAX,
          DEFAULT_QUALITY,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RESAMPLE_METHOD,
      g_param_spec_enum ("resample-method", "Resample method to use",
          "What resample method to use",
          GST_TYPE_AUDIO_RESAMPLER_METHOD,
          DEFAULT_RESAMPLE_METHOD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINC_FILTER_MODE,
      g_param_spec_enum ("sinc-filter-mode", "Sinc filter table mode",
          "What sinc filter table mode to use",
          GST_TYPE_AUDIO_RESAMPLER_FILTER_MODE,
          DEFAULT_SINC_FILTER_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_SINC_FILTER_AUTO_THRESHOLD,
      g_param_spec_uint ("sinc-filter-auto-threshold",
          "Sinc filter auto mode threshold",
          "Memory usage threshold to use if sinc filter mode is AUTO, given in bytes",
          0, G_MAXUINT, DEFAULT_SINC_FILTER_AUTO_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_SINC_FILTER_INTERPOLATION,
      g_param_spec_enum ("sinc-filter-interpolation",
          "Sinc filter interpolation",
          "How to interpolate the sinc filter table",
          GST_TYPE_AUDIO_RESAMPLER_FILTER_INTERPOLATION,
          DEFAULT_SINC_FILTER_INTERPOLATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_audio_resample_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_audio_resample_sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "Audio resampler",
      "Filter/Converter/Audio", "Resamples audio",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  GST_BASE_TRANSFORM_CLASS (klass)->start =
      GST_DEBUG_FUNCPTR (gst_audio_resample_start);
  GST_BASE_TRANSFORM_CLASS (klass)->stop =
      GST_DEBUG_FUNCPTR (gst_audio_resample_stop);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_size =
      GST_DEBUG_FUNCPTR (gst_audio_resample_transform_size);
  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_audio_resample_get_unit_size);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_audio_resample_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_audio_resample_fixate_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_audio_resample_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_audio_resample_transform);
  GST_BASE_TRANSFORM_CLASS (klass)->sink_event =
      GST_DEBUG_FUNCPTR (gst_audio_resample_sink_event);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_meta =
      GST_DEBUG_FUNCPTR (gst_audio_resample_transform_meta);
  GST_BASE_TRANSFORM_CLASS (klass)->submit_input_buffer =
      GST_DEBUG_FUNCPTR (gst_audio_resample_submit_input_buffer);

  GST_BASE_TRANSFORM_CLASS (klass)->passthrough_on_same_caps = TRUE;

  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_RESAMPLER_METHOD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_RESAMPLER_FILTER_INTERPOLATION,
      0);
  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_RESAMPLER_FILTER_MODE, 0);

  meta_tag_audio_quark = g_quark_from_static_string (GST_META_TAG_AUDIO_STR);
}

static void
gst_audio_resample_init (GstAudioResample * resample)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (resample);

  resample->method = DEFAULT_RESAMPLE_METHOD;
  resample->quality = DEFAULT_QUALITY;
  resample->sinc_filter_mode = DEFAULT_SINC_FILTER_MODE;
  resample->sinc_filter_auto_threshold = DEFAULT_SINC_FILTER_AUTO_THRESHOLD;
  resample->sinc_filter_interpolation = DEFAULT_SINC_FILTER_INTERPOLATION;

  gst_base_transform_set_gap_aware (trans, TRUE);
  gst_pad_set_query_function (trans->srcpad, gst_audio_resample_query);
}

/* vmethods */
static gboolean
gst_audio_resample_start (GstBaseTransform * base)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);

  resample->need_discont = TRUE;

  resample->num_gap_samples = 0;
  resample->num_nongap_samples = 0;
  resample->t0 = GST_CLOCK_TIME_NONE;
  resample->in_offset0 = GST_BUFFER_OFFSET_NONE;
  resample->out_offset0 = GST_BUFFER_OFFSET_NONE;
  resample->samples_in = 0;
  resample->samples_out = 0;

  return TRUE;
}

static gboolean
gst_audio_resample_stop (GstBaseTransform * base)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);

  if (resample->converter) {
    gst_audio_converter_free (resample->converter);
    resample->converter = NULL;
  }
  return TRUE;
}

static gboolean
gst_audio_resample_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    gsize * size)
{
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_caps;

  *size = GST_AUDIO_INFO_BPF (&info);

  return TRUE;

  /* ERRORS */
invalid_caps:
  {
    GST_ERROR_OBJECT (base, "invalid caps");
    return FALSE;
  }
}

static GstCaps *
gst_audio_resample_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  const GValue *val;
  GstStructure *s;
  GstCaps *res;
  gint i, n;

  /* transform single caps into input_caps + input_caps with the rate
   * field set to our supported range. This ensures that upstream knows
   * about downstream's preferred rate(s) and can negotiate accordingly. */
  res = gst_caps_new_empty ();
  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure (res, s))
      continue;

    /* first, however, check if the caps contain a range for the rate field, in
     * which case that side isn't going to care much about the exact sample rate
     * chosen and we should just assume things will get fixated to something sane
     * and we may just as well offer our full range instead of the range in the
     * caps. If the rate is not an int range value, it's likely to express a
     * real preference or limitation and we should maintain that structure as
     * preference by putting it first into the transformed caps, and only add
     * our full rate range as second option  */
    s = gst_structure_copy (s);
    val = gst_structure_get_value (s, "rate");
    if (val == NULL || GST_VALUE_HOLDS_INT_RANGE (val)) {
      /* overwrite existing range, or add field if it doesn't exist yet */
      gst_structure_set (s, "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    } else {
      /* append caps with full range to existing caps with non-range rate field */
      gst_caps_append_structure (res, gst_structure_copy (s));
      gst_structure_set (s, "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    }
    gst_caps_append_structure (res, s);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = intersection;
  }

  return res;
}

/* Fixate rate to the allowed rate that has the smallest difference */
static GstCaps *
gst_audio_resample_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *s;
  gint rate;

  s = gst_caps_get_structure (caps, 0);
  if (G_UNLIKELY (!gst_structure_get_int (s, "rate", &rate)))
    return othercaps;

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);
  s = gst_caps_get_structure (othercaps, 0);
  gst_structure_fixate_field_nearest_int (s, "rate", rate);

  return gst_caps_fixate (othercaps);
}

static GstStructure *
make_options (GstAudioResample * resample, GstAudioInfo * in,
    GstAudioInfo * out)
{
  GstStructure *options;

  options = gst_structure_new_empty ("resampler-options");
  if (in != NULL && out != NULL)
    gst_audio_resampler_options_set_quality (resample->method,
        resample->quality, in->rate, out->rate, options);

  gst_structure_set (options,
      GST_AUDIO_CONVERTER_OPT_RESAMPLER_METHOD, GST_TYPE_AUDIO_RESAMPLER_METHOD,
      resample->method,
      GST_AUDIO_RESAMPLER_OPT_FILTER_MODE, GST_TYPE_AUDIO_RESAMPLER_FILTER_MODE,
      resample->sinc_filter_mode, GST_AUDIO_RESAMPLER_OPT_FILTER_MODE_THRESHOLD,
      G_TYPE_UINT, resample->sinc_filter_auto_threshold,
      GST_AUDIO_RESAMPLER_OPT_FILTER_INTERPOLATION,
      GST_TYPE_AUDIO_RESAMPLER_FILTER_INTERPOLATION,
      resample->sinc_filter_interpolation, NULL);

  return options;
}

static gboolean
gst_audio_resample_update_state (GstAudioResample * resample, GstAudioInfo * in,
    GstAudioInfo * out)
{
  gboolean updated_latency = FALSE;
  gsize old_latency = -1;
  GstStructure *options;

  if (resample->converter == NULL && in == NULL && out == NULL)
    return TRUE;

  options = make_options (resample, in, out);

  if (resample->converter)
    old_latency = gst_audio_converter_get_max_latency (resample->converter);

  /* if channels and layout changed, destroy existing resampler */
  if (in != NULL && (in->finfo != resample->in.finfo ||
          in->channels != resample->in.channels ||
          in->layout != resample->in.layout) && resample->converter) {
    gst_audio_converter_free (resample->converter);
    resample->converter = NULL;
  }
  if (resample->converter == NULL) {
    resample->converter =
        gst_audio_converter_new (GST_AUDIO_CONVERTER_FLAG_VARIABLE_RATE, in,
        out, options);
    if (resample->converter == NULL)
      goto resampler_failed;
  } else if (in && out) {
    gboolean ret;

    ret =
        gst_audio_converter_update_config (resample->converter, in->rate,
        out->rate, options);
    if (!ret)
      goto update_failed;
  } else {
    gst_structure_free (options);
  }
  if (old_latency != -1)
    updated_latency =
        old_latency !=
        gst_audio_converter_get_max_latency (resample->converter);

  if (updated_latency)
    gst_element_post_message (GST_ELEMENT (resample),
        gst_message_new_latency (GST_OBJECT (resample)));

  return TRUE;

  /* ERRORS */
resampler_failed:
  {
    GST_ERROR_OBJECT (resample, "failed to create resampler");
    return FALSE;
  }
update_failed:
  {
    GST_ERROR_OBJECT (resample, "failed to update resampler");
    return FALSE;
  }
}

static void
gst_audio_resample_reset_state (GstAudioResample * resample)
{
  if (resample->converter)
    gst_audio_converter_reset (resample->converter);
}

static gboolean
gst_audio_resample_transform_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);
  gboolean ret = TRUE;
  gint bpf;

  GST_LOG_OBJECT (base, "asked to transform size %" G_GSIZE_FORMAT
      " in direction %s", size, direction == GST_PAD_SINK ? "SINK" : "SRC");

  /* Number of samples in either buffer is size / (width*channels) ->
   * calculate the factor */
  bpf = GST_AUDIO_INFO_BPF (&resample->in);

  /* Convert source buffer size to samples */
  size /= bpf;

  if (direction == GST_PAD_SINK) {
    /* asked to convert size of an incoming buffer */
    *othersize = gst_audio_converter_get_out_frames (resample->converter, size);
    *othersize *= bpf;
  } else {
    /* asked to convert size of an outgoing buffer */
    *othersize = gst_audio_converter_get_in_frames (resample->converter, size);
    *othersize *= bpf;
  }

  GST_LOG_OBJECT (base,
      "transformed size %" G_GSIZE_FORMAT " to %" G_GSIZE_FORMAT,
      size * bpf, *othersize);

  return ret;
}

static gboolean
gst_audio_resample_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);
  GstAudioInfo in, out;

  GST_LOG ("incaps %" GST_PTR_FORMAT ", outcaps %"
      GST_PTR_FORMAT, incaps, outcaps);

  if (!gst_audio_info_from_caps (&in, incaps))
    goto invalid_incaps;
  if (!gst_audio_info_from_caps (&out, outcaps))
    goto invalid_outcaps;

  /* Reset timestamp tracking and drain the resampler if the audio format is
   * changing. Especially when changing the sample rate our timestamp tracking
   * will be completely off, but even otherwise we would usually lose the last
   * few samples if we don't drain here */
  if (!gst_audio_info_is_equal (&in, &resample->in) ||
      !gst_audio_info_is_equal (&out, &resample->out)) {
    if (resample->converter) {
      gsize latency = gst_audio_converter_get_max_latency (resample->converter);
      gst_audio_resample_push_drain (resample, latency);
    }
    gst_audio_resample_reset_state (resample);
    resample->num_gap_samples = 0;
    resample->num_nongap_samples = 0;
    resample->t0 = GST_CLOCK_TIME_NONE;
    resample->in_offset0 = GST_BUFFER_OFFSET_NONE;
    resample->out_offset0 = GST_BUFFER_OFFSET_NONE;
    resample->samples_in = 0;
    resample->samples_out = 0;
    resample->need_discont = TRUE;
  }

  gst_audio_resample_update_state (resample, &in, &out);

  resample->in = in;
  resample->out = out;

  return TRUE;

  /* ERROR */
invalid_incaps:
  {
    GST_ERROR_OBJECT (base, "invalid incaps");
    return FALSE;
  }
invalid_outcaps:
  {
    GST_ERROR_OBJECT (base, "invalid outcaps");
    return FALSE;
  }
}

/* Push history_len zeros into the filter, but discard the output. */
static void
gst_audio_resample_dump_drain (GstAudioResample * resample, guint history_len)
{
  gsize out_len, outsize;
  GstBuffer *outbuf;
  GstAudioBuffer abuf;

  out_len =
      gst_audio_converter_get_out_frames (resample->converter, history_len);
  if (out_len == 0)
    return;

  outsize = out_len * resample->out.bpf;
  outbuf = gst_buffer_new_and_alloc (outsize);

  if (GST_AUDIO_INFO_LAYOUT (&resample->out) ==
      GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (outbuf, &resample->out, out_len, NULL);
  }

  gst_audio_buffer_map (&abuf, &resample->out, outbuf, GST_MAP_WRITE);
  gst_audio_converter_samples (resample->converter, 0, NULL, history_len,
      abuf.planes, out_len);
  gst_audio_buffer_unmap (&abuf);

  gst_buffer_unref (outbuf);
}

static void
gst_audio_resample_push_drain (GstAudioResample * resample, guint history_len)
{
  GstBuffer *outbuf;
  GstFlowReturn res;
  gint outsize;
  gsize out_len;
  GstAudioBuffer abuf;

  g_assert (resample->converter != NULL);

  /* Don't drain samples if we were reset. */
  if (!GST_CLOCK_TIME_IS_VALID (resample->t0))
    return;

  out_len =
      gst_audio_converter_get_out_frames (resample->converter, history_len);
  if (out_len == 0)
    return;

  outsize = out_len * resample->in.bpf;
  outbuf = gst_buffer_new_and_alloc (outsize);

  if (GST_AUDIO_INFO_LAYOUT (&resample->out) ==
      GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (outbuf, &resample->out, out_len, NULL);
  }

  gst_audio_buffer_map (&abuf, &resample->out, outbuf, GST_MAP_WRITE);
  gst_audio_converter_samples (resample->converter, 0, NULL, history_len,
      abuf.planes, out_len);
  gst_audio_buffer_unmap (&abuf);

  /* time */
  if (GST_CLOCK_TIME_IS_VALID (resample->t0)) {
    GST_BUFFER_TIMESTAMP (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out, GST_SECOND,
        resample->out.rate);
    GST_BUFFER_DURATION (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out + out_len,
        GST_SECOND, resample->out.rate) - GST_BUFFER_TIMESTAMP (outbuf);
  } else {
    GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  }
  /* offset */
  if (resample->out_offset0 != GST_BUFFER_OFFSET_NONE) {
    GST_BUFFER_OFFSET (outbuf) = resample->out_offset0 + resample->samples_out;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET (outbuf) + out_len;
  } else {
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_NONE;
  }
  /* move along */
  resample->samples_out += out_len;
  resample->samples_in += history_len;

  GST_LOG_OBJECT (resample,
      "Pushing drain buffer of %u bytes with timestamp %" GST_TIME_FORMAT
      " duration %" GST_TIME_FORMAT " offset %" G_GUINT64_FORMAT " offset_end %"
      G_GUINT64_FORMAT, outsize,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)), GST_BUFFER_OFFSET (outbuf),
      GST_BUFFER_OFFSET_END (outbuf));

  res = gst_pad_push (GST_BASE_TRANSFORM_SRC_PAD (resample), outbuf);

  if (G_UNLIKELY (res != GST_FLOW_OK))
    GST_WARNING_OBJECT (resample, "Failed to push drain: %s",
        gst_flow_get_name (res));

  return;
}

static gboolean
gst_audio_resample_sink_event (GstBaseTransform * base, GstEvent * event)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_audio_resample_reset_state (resample);
      resample->num_gap_samples = 0;
      resample->num_nongap_samples = 0;
      resample->t0 = GST_CLOCK_TIME_NONE;
      resample->in_offset0 = GST_BUFFER_OFFSET_NONE;
      resample->out_offset0 = GST_BUFFER_OFFSET_NONE;
      resample->samples_in = 0;
      resample->samples_out = 0;
      resample->need_discont = TRUE;
      break;
    case GST_EVENT_STREAM_START:
    case GST_EVENT_SEGMENT:
    case GST_EVENT_EOS:
      if (resample->converter) {
        gsize latency =
            gst_audio_converter_get_max_latency (resample->converter);
        gst_audio_resample_push_drain (resample, latency);
      }
      gst_audio_resample_reset_state (resample);
      resample->num_gap_samples = 0;
      resample->num_nongap_samples = 0;
      resample->t0 = GST_CLOCK_TIME_NONE;
      resample->in_offset0 = GST_BUFFER_OFFSET_NONE;
      resample->out_offset0 = GST_BUFFER_OFFSET_NONE;
      resample->samples_in = 0;
      resample->samples_out = 0;
      resample->need_discont = TRUE;
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (base, event);
}

static gboolean
gst_audio_resample_check_discont (GstAudioResample * resample, GstBuffer * buf)
{
  guint64 offset;
  guint64 delta;

  /* is the incoming buffer a discontinuity? */
  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (buf)))
    return TRUE;

  /* no valid timestamps or offsets to compare --> no discontinuity */
  if (G_UNLIKELY (!(GST_BUFFER_TIMESTAMP_IS_VALID (buf) &&
              GST_CLOCK_TIME_IS_VALID (resample->t0))))
    return FALSE;

  /* convert the inbound timestamp to an offset. */
  offset =
      gst_util_uint64_scale_int_round (GST_BUFFER_TIMESTAMP (buf) -
      resample->t0, resample->in.rate, GST_SECOND);

  /* many elements generate imperfect streams due to rounding errors, so we
   * permit a small error (up to one sample) without triggering a filter
   * flush/restart (if triggered incorrectly, this will be audible) */
  /* allow even up to more samples, since sink is not so strict anyway,
   * so give that one a chance to handle this as configured */
  delta = ABS ((gint64) (offset - resample->samples_in));
  if (delta <= (resample->in.rate >> 5))
    return FALSE;

  GST_WARNING_OBJECT (resample,
      "encountered timestamp discontinuity of %" G_GUINT64_FORMAT " samples = %"
      GST_TIME_FORMAT, delta,
      GST_TIME_ARGS (gst_util_uint64_scale_int_round (delta, GST_SECOND,
              resample->in.rate)));
  return TRUE;
}

static GstFlowReturn
gst_audio_resample_process (GstAudioResample * resample, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstAudioBuffer srcabuf, dstabuf;
  gsize outsize;
  gsize in_len;
  gsize out_len;
  guint filt_len =
      gst_audio_converter_get_max_latency (resample->converter) * 2;
  gboolean inbuf_writable;

  inbuf_writable = gst_buffer_is_writable (inbuf)
      && gst_buffer_n_memory (inbuf) == 1
      && gst_memory_is_writable (gst_buffer_peek_memory (inbuf, 0));

  gst_audio_buffer_map (&srcabuf, &resample->in, inbuf,
      inbuf_writable ? GST_MAP_READWRITE : GST_MAP_READ);

  in_len = srcabuf.n_samples;
  out_len = gst_audio_converter_get_out_frames (resample->converter, in_len);

  GST_DEBUG_OBJECT (resample, "in %" G_GSIZE_FORMAT " frames, out %"
      G_GSIZE_FORMAT " frames", in_len, out_len);

  /* ensure that the output buffer is not bigger than what we need */
  gst_buffer_set_size (outbuf, out_len * resample->in.bpf);

  if (GST_AUDIO_INFO_LAYOUT (&resample->out) ==
      GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (outbuf, &resample->out, out_len, NULL);
  }

  gst_audio_buffer_map (&dstabuf, &resample->out, outbuf, GST_MAP_WRITE);

  if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP)) {
    resample->num_nongap_samples = 0;
    if (resample->num_gap_samples < filt_len) {
      guint zeros_to_push;
      if (in_len >= filt_len - resample->num_gap_samples)
        zeros_to_push = filt_len - resample->num_gap_samples;
      else
        zeros_to_push = in_len;

      gst_audio_resample_push_drain (resample, zeros_to_push);
      in_len -= zeros_to_push;
      resample->num_gap_samples += zeros_to_push;
    }

    {
      guint num, den;
      gint i;

      num = resample->in.rate;
      den = resample->out.rate;

      if (resample->samples_in + in_len >= filt_len / 2)
        out_len =
            gst_util_uint64_scale_int_ceil (resample->samples_in + in_len -
            filt_len / 2, den, num) - resample->samples_out;
      else
        out_len = 0;

      for (i = 0; i < dstabuf.n_planes; i++)
        memset (dstabuf.planes[i], 0, GST_AUDIO_BUFFER_PLANE_SIZE (&dstabuf));

      GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
      resample->num_gap_samples += in_len;
    }
  } else {                      /* not a gap */
    if (resample->num_gap_samples > filt_len) {
      /* push in enough zeros to restore the filter to the right offset */
      guint num;

      num = resample->in.rate;

      gst_audio_resample_dump_drain (resample,
          (resample->num_gap_samples - filt_len) % num);
    }
    resample->num_gap_samples = 0;
    if (resample->num_nongap_samples < filt_len) {
      resample->num_nongap_samples += in_len;
      if (resample->num_nongap_samples > filt_len)
        resample->num_nongap_samples = filt_len;
    }
    {
      /* process */
      GstAudioConverterFlags flags;

      flags = 0;
      if (inbuf_writable)
        flags |= GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE;

      gst_audio_converter_samples (resample->converter, flags, srcabuf.planes,
          in_len, dstabuf.planes, out_len);
    }
  }

  /* time */
  if (GST_CLOCK_TIME_IS_VALID (resample->t0)) {
    GST_BUFFER_TIMESTAMP (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out, GST_SECOND,
        resample->out.rate);
    GST_BUFFER_DURATION (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out + out_len,
        GST_SECOND, resample->out.rate) - GST_BUFFER_TIMESTAMP (outbuf);
  } else {
    GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  }
  /* offset */
  if (resample->out_offset0 != GST_BUFFER_OFFSET_NONE) {
    GST_BUFFER_OFFSET (outbuf) = resample->out_offset0 + resample->samples_out;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET (outbuf) + out_len;
  } else {
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_NONE;
  }
  /* move along */
  resample->samples_out += out_len;
  resample->samples_in += in_len;

  gst_audio_buffer_unmap (&srcabuf);
  gst_audio_buffer_unmap (&dstabuf);

  outsize = out_len * resample->in.bpf;

  GST_LOG_OBJECT (resample,
      "Converted to buffer of %" G_GSIZE_FORMAT
      " samples (%" G_GSIZE_FORMAT " bytes) with timestamp %" GST_TIME_FORMAT
      ", duration %" GST_TIME_FORMAT ", offset %" G_GUINT64_FORMAT
      ", offset_end %" G_GUINT64_FORMAT, out_len, outsize,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
      GST_BUFFER_OFFSET (outbuf), GST_BUFFER_OFFSET_END (outbuf));

  if (outsize == 0)
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  else
    return GST_FLOW_OK;
}

static GstFlowReturn
gst_audio_resample_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);
  GstFlowReturn ret;

  GST_LOG_OBJECT (resample, "transforming buffer of %" G_GSIZE_FORMAT " bytes,"
      " ts %" GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT ", offset %"
      G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      gst_buffer_get_size (inbuf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (inbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (inbuf)),
      GST_BUFFER_OFFSET (inbuf), GST_BUFFER_OFFSET_END (inbuf));

  /* check for timestamp discontinuities;  flush/reset if needed, and set
   * flag to resync timestamp and offset counters and send event
   * downstream */
  if (G_UNLIKELY (gst_audio_resample_check_discont (resample, inbuf))) {
    if (resample->converter) {
      gsize latency = gst_audio_converter_get_max_latency (resample->converter);
      gst_audio_resample_push_drain (resample, latency);
    }

    gst_audio_resample_reset_state (resample);
    resample->need_discont = TRUE;
  }

  /* handle discontinuity */
  if (G_UNLIKELY (resample->need_discont)) {
    resample->num_gap_samples = 0;
    resample->num_nongap_samples = 0;
    /* reset */
    resample->samples_in = 0;
    resample->samples_out = 0;
    GST_DEBUG_OBJECT (resample, "found discontinuity; resyncing");
    /* resync the timestamp and offset counters if possible */
    if (GST_BUFFER_TIMESTAMP_IS_VALID (inbuf)) {
      resample->t0 = GST_BUFFER_TIMESTAMP (inbuf);
    } else {
      GST_DEBUG_OBJECT (resample, "... but new timestamp is invalid");
      resample->t0 = GST_CLOCK_TIME_NONE;
    }
    if (GST_BUFFER_OFFSET_IS_VALID (inbuf)) {
      resample->in_offset0 = GST_BUFFER_OFFSET (inbuf);
      resample->out_offset0 =
          gst_util_uint64_scale_int_round (resample->in_offset0,
          resample->out.rate, resample->in.rate);
    } else {
      GST_DEBUG_OBJECT (resample, "... but new offset is invalid");
      resample->in_offset0 = GST_BUFFER_OFFSET_NONE;
      resample->out_offset0 = GST_BUFFER_OFFSET_NONE;
    }
    /* set DISCONT flag on output buffer */
    GST_DEBUG_OBJECT (resample, "marking this buffer with the DISCONT flag");
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    resample->need_discont = FALSE;
  }

  ret = gst_audio_resample_process (resample, inbuf, outbuf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    return ret;

  GST_DEBUG_OBJECT (resample, "input = samples [%" G_GUINT64_FORMAT ", %"
      G_GUINT64_FORMAT ") = [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT
      ") ns;  output = samples [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT
      ") = [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ") ns",
      GST_BUFFER_OFFSET (inbuf), GST_BUFFER_OFFSET_END (inbuf),
      GST_BUFFER_TIMESTAMP (inbuf), GST_BUFFER_TIMESTAMP (inbuf) +
      GST_BUFFER_DURATION (inbuf), GST_BUFFER_OFFSET (outbuf),
      GST_BUFFER_OFFSET_END (outbuf), GST_BUFFER_TIMESTAMP (outbuf),
      GST_BUFFER_TIMESTAMP (outbuf) + GST_BUFFER_DURATION (outbuf));

  return GST_FLOW_OK;
}

static gboolean
gst_audio_resample_transform_meta (GstBaseTransform * trans, GstBuffer * outbuf,
    GstMeta * meta, GstBuffer * inbuf)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags || (g_strv_length ((gchar **) tags) == 1
          && gst_meta_api_type_has_tag (info->api, META_TAG_AUDIO)))
    return TRUE;

  return FALSE;
}

static GstFlowReturn
gst_audio_resample_submit_input_buffer (GstBaseTransform * base,
    gboolean is_discont, GstBuffer * input)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (base);

  if (base->segment.format == GST_FORMAT_TIME) {
    if (!GST_AUDIO_INFO_IS_VALID (&resample->in)) {
      GST_WARNING_OBJECT (resample, "Got buffer, but not negotiated yet!");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    input =
        gst_audio_buffer_clip (input, &base->segment, resample->in.rate,
        resample->in.bpf);

    if (!input)
      return GST_FLOW_OK;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->submit_input_buffer (base,
      is_discont, input);
}

static gboolean
gst_audio_resample_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstAudioResample *resample = GST_AUDIO_RESAMPLE (parent);
  GstBaseTransform *trans;
  gboolean res = TRUE;

  trans = GST_BASE_TRANSFORM (resample);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      GstClockTime min, max;
      gboolean live;
      guint64 latency;
      gint rate = resample->in.rate;
      gint resampler_latency;

      if (resample->converter)
        resampler_latency =
            gst_audio_converter_get_max_latency (resample->converter);
      else
        resampler_latency = 0;

      if (gst_base_transform_is_passthrough (trans))
        resampler_latency = 0;

      if ((res =
              gst_pad_peer_query (GST_BASE_TRANSFORM_SINK_PAD (trans),
                  query))) {
        gst_query_parse_latency (query, &live, &min, &max);

        GST_DEBUG_OBJECT (resample, "Peer latency: min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min), GST_TIME_ARGS (max));

        /* add our own latency */
        if (rate != 0 && resampler_latency != 0)
          latency = gst_util_uint64_scale_round (resampler_latency,
              GST_SECOND, rate);
        else
          latency = 0;

        GST_DEBUG_OBJECT (resample, "Our latency: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency));

        min += latency;
        if (GST_CLOCK_TIME_IS_VALID (max))
          max += latency;

        GST_DEBUG_OBJECT (resample, "Calculated total latency : min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min), GST_TIME_ARGS (max));

        gst_query_set_latency (query, live, min, max);
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
  return res;
}

static void
gst_audio_resample_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioResample *resample;

  resample = GST_AUDIO_RESAMPLE (object);

  switch (prop_id) {
    case PROP_QUALITY:
      /* FIXME locking! */
      resample->quality = g_value_get_int (value);
      GST_DEBUG_OBJECT (resample, "new quality %d", resample->quality);
      gst_audio_resample_update_state (resample, NULL, NULL);
      break;
    case PROP_RESAMPLE_METHOD:
      resample->method = g_value_get_enum (value);
      gst_audio_resample_update_state (resample, NULL, NULL);
      break;
    case PROP_SINC_FILTER_MODE:
      /* FIXME locking! */
      resample->sinc_filter_mode = g_value_get_enum (value);
      gst_audio_resample_update_state (resample, NULL, NULL);
      break;
    case PROP_SINC_FILTER_AUTO_THRESHOLD:
      /* FIXME locking! */
      resample->sinc_filter_auto_threshold = g_value_get_uint (value);
      gst_audio_resample_update_state (resample, NULL, NULL);
      break;
    case PROP_SINC_FILTER_INTERPOLATION:
      /* FIXME locking! */
      resample->sinc_filter_interpolation = g_value_get_enum (value);
      gst_audio_resample_update_state (resample, NULL, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_resample_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioResample *resample;

  resample = GST_AUDIO_RESAMPLE (object);

  switch (prop_id) {
    case PROP_QUALITY:
      g_value_set_int (value, resample->quality);
      break;
    case PROP_RESAMPLE_METHOD:
      g_value_set_enum (value, resample->method);
      break;
    case PROP_SINC_FILTER_MODE:
      g_value_set_enum (value, resample->sinc_filter_mode);
      break;
    case PROP_SINC_FILTER_AUTO_THRESHOLD:
      g_value_set_uint (value, resample->sinc_filter_auto_threshold);
      break;
    case PROP_SINC_FILTER_INTERPOLATION:
      g_value_set_enum (value, resample->sinc_filter_interpolation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (audioresample, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audioresample,
    "Resamples audio", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
