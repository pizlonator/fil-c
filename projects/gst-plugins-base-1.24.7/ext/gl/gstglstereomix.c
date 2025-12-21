/*
 * Combine video streams to 3D stereo
 *
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2014 Jan Schmidt <jan@noraisin.net>
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
 * SECTION:element-glstereomix
 * @title: glstereomix
 *
 * Combine 2 input streams to produce a stereoscopic output
 * stream. Input views are taken from the left pad and right pad
 * respectively, and mixed according to their timelines.
 *
 * If either input stream is stereoscopic, the approproriate view
 * (left or right) is taken from each stream and placed into the output.
 *
 * The multiview representation on the output is chosen according to
 * the downstream caps.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v videotestsrc pattern=ball name=left \
 *     videotestsrc name=right glstereomix name=mix \
 *     left. ! video/x-raw,width=640,height=480! glupload ! mix.  \
 *     right. ! video/x-raw,width=640,height=480! glupload ! mix.  \
 *     mix. ! video/x-raw'(memory:GLMemory)',multiview-mode=side-by-side ! \
 *     queue ! glimagesink output-multiview-mode=side-by-side
 * ]| Mix 2 different videotestsrc patterns into a side-by-side stereo image and display it.
 * |[
 * gst-launch-1.0 -ev v4l2src name=left \
 *     videotestsrc name=right \
 *     glstereomix name=mix \
 *     left. ! video/x-raw,width=640,height=480 ! glupload ! glcolorconvert ! mix.  \
 *     right. ! video/x-raw,width=640,height=480 ! glupload ! mix.  \
 *     mix. ! video/x-raw'(memory:GLMemory)',multiview-mode=top-bottom ! \
 *     glcolorconvert ! gldownload ! queue ! x264enc ! h264parse ! \
 *     mp4mux ! progressreport ! filesink location=output.mp4
 * ]| Mix the input from a camera to the left view, and videotestsrc to the right view,
 *    and encode as a top-bottom frame packed H.264 video.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglstereomix.h"
#include <gst/gl/gl.h>

#define GST_CAT_DEFAULT gst_gl_stereo_mix_debug
GST_DEBUG_CATEGORY (gst_gl_stereo_mix_debug);

G_DEFINE_TYPE (GstGLStereoMixPad, gst_gl_stereo_mix_pad, GST_TYPE_GL_MIXER_PAD);

static void
gst_gl_stereo_mix_pad_class_init (GstGLStereoMixPadClass * klass)
{
}

static void
gst_gl_stereo_mix_pad_init (GstGLStereoMixPad * pad)
{
}

static void gst_gl_stereo_mix_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define gst_gl_stereo_mix_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLStereoMix, gst_gl_stereo_mix, GST_TYPE_GL_MIXER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_gl_stereo_mix_child_proxy_init));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glstereomix, "glstereomix",
    GST_RANK_NONE, GST_TYPE_GL_STEREO_MIX, gl_element_init (plugin));

static GstCaps *_update_caps (GstVideoAggregator * vagg, GstCaps * caps);
static gboolean _negotiated_caps (GstAggregator * aggregator, GstCaps * caps);
static gboolean gst_gl_stereo_mix_make_output (GstGLStereoMix * mix);
static gboolean gst_gl_stereo_mix_process_frames (GstGLStereoMix * mixer);

#define DEFAULT_DOWNMIX GST_GL_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

/* GLStereoMix signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DOWNMIX_MODE
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ","
        "texture-target = (string) 2D"
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META,
            "RGBA")
        "; " GST_VIDEO_CAPS_MAKE (GST_GL_COLOR_CONVERT_FORMATS))
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ","
        "texture-target = (string) 2D"
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META,
            "RGBA")
        "; " GST_VIDEO_CAPS_MAKE (GST_GL_COLOR_CONVERT_FORMATS))
    );

static GstPad *gst_gl_stereo_mix_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * req_name, const GstCaps * caps);
static void gst_gl_stereo_mix_release_pad (GstElement * element, GstPad * pad);

static GstFlowReturn gst_gl_stereo_mix_create_output_buffer (GstVideoAggregator
    * videoaggregator, GstBuffer ** outbuf);
static gboolean gst_gl_stereo_mix_stop (GstAggregator * agg);
static gboolean gst_gl_stereo_mix_start (GstAggregator * agg);
static gboolean gst_gl_stereo_mix_src_query (GstAggregator * agg,
    GstQuery * query);

static void gst_gl_stereo_mix_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_stereo_mix_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_gl_stereo_mix_finalize (GObject * object);

static GstFlowReturn
gst_gl_stereo_mix_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuffer);

static void
gst_gl_stereo_mix_class_init (GstGLStereoMixClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoAggregatorClass *videoaggregator_class =
      (GstVideoAggregatorClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;
  GstGLBaseMixerClass *base_mix_class = (GstGLBaseMixerClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "glstereomixer", 0,
      "opengl stereoscopic mixer");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_gl_stereo_mix_finalize);

  gobject_class->get_property = gst_gl_stereo_mix_get_property;
  gobject_class->set_property = gst_gl_stereo_mix_set_property;

  gst_element_class_set_metadata (element_class, "OpenGL stereo video combiner",
      "Filter/Effect/Video", "OpenGL stereo video combiner",
      "Jan Schmidt <jan@centricular.com>");

  g_object_class_install_property (gobject_class, PROP_DOWNMIX_MODE,
      g_param_spec_enum ("downmix-mode", "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &src_factory, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &sink_factory, GST_TYPE_GL_STEREO_MIX_PAD);

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_gl_stereo_mix_request_new_pad);
  element_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_gl_stereo_mix_release_pad);

  agg_class->stop = gst_gl_stereo_mix_stop;
  agg_class->start = gst_gl_stereo_mix_start;
  agg_class->src_query = gst_gl_stereo_mix_src_query;
  agg_class->negotiated_src_caps = _negotiated_caps;

  videoaggregator_class->aggregate_frames = gst_gl_stereo_mix_aggregate_frames;
  videoaggregator_class->update_caps = _update_caps;
  videoaggregator_class->create_output_buffer =
      gst_gl_stereo_mix_create_output_buffer;

  base_mix_class->supported_gl_api =
      GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;

  gst_type_mark_as_plugin_api (GST_TYPE_GL_STEREO_DOWNMIX, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_STEREO_MIX_PAD, 0);
}

static void
gst_gl_stereo_mix_init (GstGLStereoMix * mix)
{
}

static void
gst_gl_stereo_mix_finalize (GObject * object)
{
  //GstGLStereoMix *mix = GST_GL_STEREO_MIX (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_gl_stereo_mix_query_caps (GstPad * pad, GstAggregator * agg,
    GstQuery * query)
{
  GstCaps *filter, *caps;

  gst_query_parse_caps (query, &filter);

  caps = gst_pad_get_current_caps (agg->srcpad);
  if (caps == NULL) {
    caps = gst_pad_get_pad_template_caps (agg->srcpad);
  }

  if (filter)
    caps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);

  gst_query_set_caps_result (query, caps);
  gst_caps_unref (caps);

  return TRUE;
}

static gboolean
gst_gl_stereo_mix_src_query (GstAggregator * agg, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      return gst_gl_stereo_mix_query_caps (agg->srcpad, agg, query);
      break;
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
}


static GstFlowReturn
gst_gl_stereo_mix_create_output_buffer (GstVideoAggregator * videoaggregator,
    GstBuffer ** outbuf)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (videoaggregator);
  GstFlowReturn ret = GST_FLOW_OK;

#if 0

  if (!mix->priv->pool_active) {
    if (!gst_buffer_pool_set_active (mix->priv->pool, TRUE)) {
      GST_ELEMENT_ERROR (mix, RESOURCE, SETTINGS,
          ("failed to activate bufferpool"), ("failed to activate bufferpool"));
      return GST_FLOW_ERROR;
    }
    mix->priv->pool_active = TRUE;
  }

  return gst_buffer_pool_acquire_buffer (mix->priv->pool, outbuf, NULL);
#endif

  if (!gst_gl_stereo_mix_make_output (mix)) {
    gst_buffer_replace (&mix->primary_out, NULL);
    gst_buffer_replace (&mix->auxilliary_out, NULL);
    GST_ELEMENT_ERROR (mix, RESOURCE, SETTINGS,
        ("Failed to generate output"), ("failed to generate output"));
    ret = GST_FLOW_ERROR;
  }

  if (mix->auxilliary_out) {
    *outbuf = mix->auxilliary_out;
    mix->auxilliary_out = NULL;
  } else {
    *outbuf = mix->primary_out;
    mix->primary_out = NULL;
  }
  return ret;
}

static gboolean
gst_gl_stereo_mix_make_output (GstGLStereoMix * mix)
{
  GList *walk;
  gboolean res = FALSE;
  GstElement *element = GST_ELEMENT (mix);
  gboolean missing_buffer = FALSE;

  GST_LOG_OBJECT (mix, "Processing buffers");

  GST_OBJECT_LOCK (mix);
  walk = element->sinkpads;
  while (walk) {
    GstVideoAggregatorPad *vaggpad = walk->data;
    GstGLStereoMixPad *pad = walk->data;
    GstBuffer *buffer = gst_video_aggregator_pad_get_current_buffer (vaggpad);

    GST_LOG_OBJECT (mix, "Checking pad %" GST_PTR_FORMAT, vaggpad);

    if (buffer != NULL) {
      pad->current_buffer = buffer;

      GST_DEBUG_OBJECT (pad, "Got buffer %" GST_PTR_FORMAT,
          pad->current_buffer);
    } else {
      GST_LOG_OBJECT (mix, "No buffer on pad %" GST_PTR_FORMAT, vaggpad);
      pad->current_buffer = NULL;
      missing_buffer = TRUE;
    }
    walk = g_list_next (walk);
  }
  if (missing_buffer) {
    /* We're still waiting for a buffer to turn up on at least one input */
    GST_WARNING_OBJECT (mix, "Not generating output - need more input buffers");
    res = TRUE;
    goto out;
  }

  /* Copy GL memory from each input frame to the output */
  if (!gst_gl_stereo_mix_process_frames (mix)) {
    GST_LOG_OBJECT (mix, "Failed to process frames to output");
    goto out;
  }

  if (mix->primary_out == NULL)
    goto out;

  res = TRUE;

out:
  GST_OBJECT_UNLOCK (mix);

  return res;
}

static GstFlowReturn
gst_gl_stereo_mix_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuf)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (vagg);
  /* If we're operating in frame-by-frame mode, push
   * the primary view now, and let the parent class
   * push the remaining auxiliary view */
  if (GST_VIDEO_INFO_MULTIVIEW_MODE (&vagg->info) ==
      GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    /* Transfer the timestamps video-agg put on the aux buffer */
    gst_buffer_copy_into (mix->primary_out, outbuf,
        GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
    gst_aggregator_finish_buffer (GST_AGGREGATOR (vagg), mix->primary_out);
    mix->primary_out = NULL;

    /* And actually, we don't want timestamps on the aux buffer */
    GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  }
  return GST_FLOW_OK;
}

static void
gst_gl_stereo_mix_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (object);

  switch (prop_id) {
    case PROP_DOWNMIX_MODE:
      g_value_set_enum (value, mix->downmix_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_stereo_mix_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (object);

  switch (prop_id) {
    case PROP_DOWNMIX_MODE:
      mix->downmix_mode = g_value_get_enum (value);
      if (mix->viewconvert)
        g_object_set_property (G_OBJECT (mix->viewconvert), "downmix-mode",
            value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstPad *
gst_gl_stereo_mix_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL)
    goto could_not_create;

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return GST_PAD_CAST (newpad);

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return NULL;
  }
}

static void
gst_gl_stereo_mix_release_pad (GstElement * element, GstPad * pad)
{
  GST_DEBUG_OBJECT (element, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}

static gboolean
gst_gl_stereo_mix_start (GstAggregator * agg)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (agg);

  if (!GST_AGGREGATOR_CLASS (parent_class)->start (agg))
    return FALSE;

  GST_OBJECT_LOCK (mix);
  mix->viewconvert = gst_gl_view_convert_new ();
  g_object_set (G_OBJECT (mix->viewconvert), "downmix-mode",
      mix->downmix_mode, NULL);
  GST_OBJECT_UNLOCK (mix);

  return TRUE;
}

static gboolean
gst_gl_stereo_mix_stop (GstAggregator * agg)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (agg);

  if (!GST_AGGREGATOR_CLASS (parent_class)->stop (agg))
    return FALSE;

  if (mix->viewconvert) {
    gst_object_unref (mix->viewconvert);
    mix->viewconvert = NULL;
  }

  return TRUE;
}

/* Convert to caps that can be accepted by this element... */
static GstCaps *
get_converted_caps (GstGLStereoMix * mix, GstCaps * caps)
{
#if 0
  GstGLContext *context = GST_GL_BASE_MIXER (mix)->context;
  GstCaps *result, *tmp;

  GST_LOG_OBJECT (mix, "Converting caps %" GST_PTR_FORMAT, caps);
  result = gst_gl_upload_transform_caps (context, GST_PAD_SINK, caps, NULL);
  tmp = result;
  GST_TRACE_OBJECT (mix, "transfer returned caps %" GST_PTR_FORMAT, tmp);

  result =
      gst_gl_color_convert_transform_caps (context, GST_PAD_SINK, tmp, NULL);
  gst_caps_unref (tmp);
  GST_TRACE_OBJECT (mix, "convert returned caps %" GST_PTR_FORMAT, tmp);

  tmp = result;
  result = gst_gl_view_convert_transform_caps (mix->viewconvert,
      GST_PAD_SINK, tmp, NULL);
  gst_caps_unref (tmp);
#else
  GstCaps *result;

  GST_LOG_OBJECT (mix, "Converting caps %" GST_PTR_FORMAT, caps);
  result = gst_gl_view_convert_transform_caps (mix->viewconvert,
      GST_PAD_SINK, caps, NULL);
#endif

  GST_LOG_OBJECT (mix, "returning caps %" GST_PTR_FORMAT, result);

  return result;
}

/* Return the possible output caps based on inputs and downstream prefs */
static GstCaps *
_update_caps (GstVideoAggregator * vagg, GstCaps * caps)
{
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (vagg);
  GList *l;
  gint best_width = -1, best_height = -1;
  gdouble best_fps = -1, cur_fps;
  gint best_fps_n = 0, best_fps_d = 1;
  GstVideoInfo *mix_info;
  GstCaps *blend_caps, *tmp_caps;
  GstCaps *out_caps;

  GST_OBJECT_LOCK (vagg);

  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *pad = l->data;
    GstVideoInfo tmp = pad->info;
    gint this_width, this_height;
    gint fps_n, fps_d;

    if (!pad->info.finfo)
      continue;

    /* This can happen if we release a pad and another pad hasn't been negotiated_caps yet */
    if (GST_VIDEO_INFO_FORMAT (&pad->info) == GST_VIDEO_FORMAT_UNKNOWN)
      continue;

    /* Convert to per-view width/height for unpacked forms */
    gst_video_multiview_video_info_change_mode (&tmp,
        GST_VIDEO_MULTIVIEW_MODE_SEPARATED, GST_VIDEO_MULTIVIEW_FLAGS_NONE);

    this_width = GST_VIDEO_INFO_WIDTH (&tmp);
    this_height = GST_VIDEO_INFO_HEIGHT (&tmp);
    fps_n = GST_VIDEO_INFO_FPS_N (&tmp);
    fps_d = GST_VIDEO_INFO_FPS_D (&tmp);

    GST_INFO_OBJECT (vagg, "Input pad %" GST_PTR_FORMAT
        " w %u h %u", pad, this_width, this_height);

    if (this_width == 0 || this_height == 0)
      continue;

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }

    /* FIXME: Preserve PAR for at least one input when different sized inputs */
  }
  GST_OBJECT_UNLOCK (vagg);

  mix_info = &mix->mix_info;
  gst_video_info_set_format (mix_info, GST_VIDEO_FORMAT_RGBA, best_width,
      best_height);

  GST_VIDEO_INFO_FPS_N (mix_info) = best_fps_n;
  GST_VIDEO_INFO_FPS_D (mix_info) = best_fps_d;

  GST_VIDEO_INFO_MULTIVIEW_MODE (mix_info) = GST_VIDEO_MULTIVIEW_MODE_SEPARATED;
  GST_VIDEO_INFO_VIEWS (mix_info) = 2;

  /* FIXME: If input is marked as flipped or flopped, preserve those flags */
  GST_VIDEO_INFO_MULTIVIEW_FLAGS (mix_info) = GST_VIDEO_MULTIVIEW_FLAGS_NONE;

  /* Choose our output format based on downstream preferences */
  blend_caps = gst_video_info_to_caps (mix_info);

  gst_caps_set_features (blend_caps, 0,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));

  tmp_caps = get_converted_caps (GST_GL_STEREO_MIX (vagg), blend_caps);
  gst_caps_unref (blend_caps);

  out_caps = gst_caps_intersect (caps, tmp_caps);
  gst_caps_unref (tmp_caps);

  GST_DEBUG_OBJECT (vagg, "Possible output caps %" GST_PTR_FORMAT, out_caps);

  return out_caps;
}

/* Called after videoaggregator fixates our caps */
static gboolean
_negotiated_caps (GstAggregator * agg, GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstGLStereoMix *mix = GST_GL_STEREO_MIX (vagg);
  GstCaps *in_caps;

  GST_LOG_OBJECT (mix, "Configured output caps %" GST_PTR_FORMAT, caps);

  if (GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps)
    if (!GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps))
      return FALSE;

  /* Update the glview_convert output */

  /* We can configure the view_converter now */
  gst_gl_view_convert_set_context (mix->viewconvert,
      GST_GL_BASE_MIXER (mix)->context);

  in_caps = gst_video_info_to_caps (&mix->mix_info);
  gst_caps_set_features (in_caps, 0,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
  gst_caps_set_simple (in_caps, "texture-target", G_TYPE_STRING,
      GST_GL_TEXTURE_TARGET_2D_STR, NULL);

  gst_gl_view_convert_set_caps (mix->viewconvert, in_caps, caps);
  gst_caps_unref (in_caps);

  return TRUE;
}

/* called with the object lock held */
static gboolean
gst_gl_stereo_mix_process_frames (GstGLStereoMix * mixer)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (mixer);
  GstBuffer *converted_buffer, *inbuf;
  GstVideoInfo *out_info = &vagg->info;
#ifndef G_DISABLE_ASSERT
  gint n;
#endif
  gint v, views;
  gint valid_views = 0;
  GList *walk;

  inbuf = gst_buffer_new ();
  walk = GST_ELEMENT (mixer)->sinkpads;
  while (walk) {
    GstGLStereoMixPad *pad = walk->data;
    GstMemory *in_mem;

    GST_LOG_OBJECT (mixer, "Handling frame %d", valid_views);

    if (!pad || !pad->current_buffer) {
      GST_DEBUG ("skipping texture, null frame");
      walk = g_list_next (walk);
      continue;
    }

    in_mem = gst_buffer_get_memory (pad->current_buffer, 0);

    GST_LOG_OBJECT (mixer,
        "Appending memory %" GST_PTR_FORMAT " to intermediate buffer", in_mem);
    /* Appending the memory to a 2nd buffer locks it
     * exclusive a 2nd time, which will mark it for
     * copy-on-write. The ref will keep the memory
     * alive but we add a parent_buffer_meta to also
     * prevent the input buffer from returning to any buffer
     * pool it might belong to
     */
    gst_buffer_append_memory (inbuf, in_mem);
    /* Use parent buffer meta to keep input buffer alive */
    gst_buffer_add_parent_buffer_meta (inbuf, pad->current_buffer);

    valid_views++;
    walk = g_list_next (walk);
  }

  if (mixer->mix_info.views != valid_views) {
    GST_WARNING_OBJECT (mixer, "Not enough input views to process");
    return FALSE;
  }

  if (GST_VIDEO_INFO_MULTIVIEW_MODE (out_info) ==
      GST_VIDEO_MULTIVIEW_MODE_SEPARATED)
    views = out_info->views;
  else
    views = 1;

  /* We can configure the view_converter now */
  gst_gl_view_convert_set_context (mixer->viewconvert,
      GST_GL_BASE_MIXER (mixer)->context);

  if (gst_gl_view_convert_submit_input_buffer (mixer->viewconvert,
          FALSE, inbuf) != GST_FLOW_OK)
    return FALSE;

  /* Clear any existing buffers, just in case */
  gst_buffer_replace (&mixer->primary_out, NULL);
  gst_buffer_replace (&mixer->auxilliary_out, NULL);

  if (gst_gl_view_convert_get_output (mixer->viewconvert,
          &mixer->primary_out) != GST_FLOW_OK)
    return FALSE;

  if (GST_VIDEO_INFO_MULTIVIEW_MODE (out_info) ==
      GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (gst_gl_view_convert_get_output (mixer->viewconvert,
            &mixer->auxilliary_out) != GST_FLOW_OK)
      return FALSE;
  }

  if (mixer->primary_out == NULL)
    return FALSE;

  converted_buffer = mixer->primary_out;

#ifndef G_DISABLE_ASSERT
  n = gst_buffer_n_memory (converted_buffer);
  g_assert (n == GST_VIDEO_INFO_N_PLANES (out_info) * views);
#endif

  for (v = 0; v < views; v++) {
    gst_buffer_add_video_meta_full (converted_buffer, v,
        GST_VIDEO_INFO_FORMAT (out_info),
        GST_VIDEO_INFO_WIDTH (out_info),
        GST_VIDEO_INFO_HEIGHT (out_info),
        GST_VIDEO_INFO_N_PLANES (out_info), out_info->offset, out_info->stride);
    if (mixer->auxilliary_out) {
      gst_buffer_add_video_meta_full (mixer->auxilliary_out, v,
          GST_VIDEO_INFO_FORMAT (out_info),
          GST_VIDEO_INFO_WIDTH (out_info),
          GST_VIDEO_INFO_HEIGHT (out_info),
          GST_VIDEO_INFO_N_PLANES (out_info), out_info->offset,
          out_info->stride);
    }
  }

  return TRUE;
}

/* GstChildProxy implementation */
static GObject *
gst_gl_stereo_mix_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstGLStereoMix *gl_stereo_mix = GST_GL_STEREO_MIX (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (gl_stereo_mix);
  obj = g_list_nth_data (GST_ELEMENT_CAST (gl_stereo_mix)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (gl_stereo_mix);

  return obj;
}

static guint
gst_gl_stereo_mix_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstGLStereoMix *gl_stereo_mix = GST_GL_STEREO_MIX (child_proxy);

  GST_OBJECT_LOCK (gl_stereo_mix);
  count = GST_ELEMENT_CAST (gl_stereo_mix)->numsinkpads;
  GST_OBJECT_UNLOCK (gl_stereo_mix);
  GST_INFO_OBJECT (gl_stereo_mix, "Children Count: %d", count);

  return count;
}

static void
gst_gl_stereo_mix_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_gl_stereo_mix_child_proxy_get_child_by_index;
  iface->get_children_count = gst_gl_stereo_mix_child_proxy_get_children_count;
}
