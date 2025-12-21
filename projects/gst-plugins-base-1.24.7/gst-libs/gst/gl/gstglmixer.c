/* Generic video mixer plugin
 *
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>

#include <string.h>

#include "gstglmixer.h"

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

/**
 * SECTION:gstglmixer
 * @short_description: #GstVideoAggregator subclass for transforming RGBA textures
 * @title: GstGLMixer
 * @see_also: #GstGLBaseMixer, #GstVideoAggregator
 *
 * #GstGLMixer helps implement an element that operates on RGBA textures.
 *
 * Since: 1.24
 */

#define GST_CAT_DEFAULT gst_gl_mixer_debug
GST_DEBUG_CATEGORY (gst_gl_mixer_debug);

static void gst_gl_mixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gl_mixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_gl_mixer_pad_prepare_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame);
static void gst_gl_mixer_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame);

enum
{
  PROP_PAD_0
};

struct _GstGLMixerPrivate
{
  gboolean negotiated;

  gboolean gl_resource_ready;
  GMutex gl_resource_lock;
  GCond gl_resource_cond;

  GstGLFramebuffer *fbo;
};

#define gst_gl_mixer_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstGLMixer, gst_gl_mixer,
    GST_TYPE_GL_BASE_MIXER);

G_DEFINE_TYPE (GstGLMixerPad, gst_gl_mixer_pad, GST_TYPE_GL_BASE_MIXER_PAD);

static void
gst_gl_mixer_pad_class_init (GstGLMixerPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorPadClass *vaggpad_class =
      (GstVideoAggregatorPadClass *) klass;

  gobject_class->set_property = gst_gl_mixer_pad_set_property;
  gobject_class->get_property = gst_gl_mixer_pad_get_property;

  vaggpad_class->prepare_frame = gst_gl_mixer_pad_prepare_frame;
  vaggpad_class->clean_frame = gst_gl_mixer_pad_clean_frame;
}

static void
gst_gl_mixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_mixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_mixer_pad_prepare_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  GstGLMixerPad *pad = GST_GL_MIXER_PAD (vpad);
  GstGLMixer *mix = GST_GL_MIXER (vagg);
  GstGLSyncMeta *sync_meta;

  pad->current_texture = 0;

  sync_meta = gst_buffer_get_gl_sync_meta (buffer);
  if (sync_meta)
    gst_gl_sync_meta_wait (sync_meta, GST_GL_BASE_MIXER (mix)->context);

  if (!gst_video_frame_map (prepared_frame, &vpad->info, buffer,
          GST_MAP_READ | GST_MAP_GL)) {
    GST_ERROR_OBJECT (pad, "Failed to map input frame");
    return FALSE;
  }

  pad->current_texture = *(guint *) prepared_frame->data[0];

  return TRUE;
}

static void
gst_gl_mixer_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  GstGLMixerPad *pad = GST_GL_MIXER_PAD (vpad);

  pad->current_texture = 0;
  if (prepared_frame->buffer) {
    gst_video_frame_unmap (prepared_frame);
    memset (prepared_frame, 0, sizeof (GstVideoFrame));
  }
}

static gboolean
_negotiated_caps (GstAggregator * agg, GstCaps * caps)
{
  GstGLMixer *mix = GST_GL_MIXER (agg);
  gboolean ret;

  mix->priv->negotiated = TRUE;

  gst_caps_replace (&mix->out_caps, caps);

  ret = GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps);

  return ret;
}

static void
_find_best_format (GstVideoAggregator * vagg, GstCaps * downstream_caps,
    GstVideoInfo * best_info, gboolean * at_least_one_alpha)
{
  GstVideoInfo tmp_info;

  GST_VIDEO_AGGREGATOR_CLASS (parent_class)->find_best_format (vagg,
      downstream_caps, best_info, at_least_one_alpha);

  gst_video_info_set_format (&tmp_info, GST_VIDEO_FORMAT_RGBA,
      best_info->width, best_info->height);
  tmp_info.par_n = best_info->par_n;
  tmp_info.par_d = best_info->par_d;
  tmp_info.fps_n = best_info->fps_n;
  tmp_info.fps_d = best_info->fps_d;
  tmp_info.flags = best_info->flags;
  tmp_info.interlace_mode = best_info->interlace_mode;
  *best_info = tmp_info;
}

static gboolean
gst_gl_mixer_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstQuery * decide_query, GstQuery * query)
{
  GstGLMixer *mix = GST_GL_MIXER (agg);
  GstGLBaseMixer *base_mix = GST_GL_BASE_MIXER (agg);
  GstGLContext *context;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  guint size = 0;
  gboolean need_pool;

  if (!GST_AGGREGATOR_CLASS (gst_gl_mixer_parent_class)->propose_allocation
      (agg, agg_pad, decide_query, query))
    return FALSE;

  context = base_mix->context;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (mix, "create new pool");
    pool = gst_gl_buffer_pool_new (context);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      g_object_unref (pool);
      goto config_failed;
    }
  }

  gst_query_add_allocation_pool (query, pool, size, 1, 0);
  if (pool)
    g_object_unref (pool);

  /* we also support various metadata */
  if (context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (mix, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (mix, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (mix, "failed setting config");
    return FALSE;
  }
}

static gboolean
gst_gl_mixer_pad_sink_acceptcaps (GstPad * pad, GstGLMixer * mix,
    GstCaps * caps)
{
  gboolean ret;
  GstCaps *template_caps;

  GST_DEBUG_OBJECT (pad, "try accept caps of %" GST_PTR_FORMAT, caps);

  template_caps = gst_pad_get_pad_template_caps (pad);
  template_caps = gst_caps_make_writable (template_caps);

  ret = gst_caps_can_intersect (caps, template_caps);
  GST_DEBUG_OBJECT (pad, "%saccepted caps %" GST_PTR_FORMAT,
      (ret ? "" : "not "), caps);
  gst_caps_unref (template_caps);

  return ret;
}

static GstCaps *
gst_gl_mixer_pad_sink_getcaps (GstPad * pad, GstGLMixer * mix, GstCaps * filter)
{
  GstCaps *sinkcaps;
  GstCaps *template_caps;
  GstCaps *filtered_caps;
  GstCaps *returned_caps;

  template_caps = gst_pad_get_pad_template_caps (pad);

  sinkcaps = gst_pad_get_current_caps (pad);
  if (sinkcaps == NULL) {
    sinkcaps = gst_caps_ref (template_caps);
  } else {
    sinkcaps = gst_caps_merge (sinkcaps, gst_caps_ref (template_caps));
  }

  if (filter) {
    filtered_caps = gst_caps_intersect (sinkcaps, filter);
    gst_caps_unref (sinkcaps);
  } else {
    filtered_caps = sinkcaps;   /* pass ownership */
  }

  returned_caps = gst_caps_intersect (filtered_caps, template_caps);

  gst_caps_unref (template_caps);
  gst_caps_unref (filtered_caps);

  GST_DEBUG_OBJECT (pad, "returning %" GST_PTR_FORMAT, returned_caps);

  return returned_caps;
}

static gboolean
gst_gl_mixer_sink_query (GstAggregator * agg, GstAggregatorPad * bpad,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstGLMixer *mix = GST_GL_MIXER (agg);

  GST_TRACE ("QUERY %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_gl_mixer_pad_sink_getcaps (GST_PAD (bpad), mix, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;

      gst_query_parse_accept_caps (query, &caps);
      ret = gst_gl_mixer_pad_sink_acceptcaps (GST_PAD (bpad), mix, caps);
      gst_query_set_accept_caps_result (query, ret);
      ret = TRUE;
      break;
    }
    default:
      ret = GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, bpad, query);
      break;
  }

  return ret;
}

static void
gst_gl_mixer_pad_init (GstGLMixerPad * mixerpad)
{
}

/* GLMixer signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA"))
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA"))
    );

static gboolean gst_gl_mixer_src_query (GstAggregator * agg, GstQuery * query);
static gboolean gst_gl_mixer_stop (GstAggregator * agg);
static gboolean gst_gl_mixer_start (GstAggregator * agg);

static GstFlowReturn
gst_gl_mixer_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuffer);

static void gst_gl_mixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_mixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_mixer_decide_allocation (GstAggregator * agg,
    GstQuery * query);

static gboolean gst_gl_mixer_gl_start (GstGLBaseMixer * mix);
static void gst_gl_mixer_gl_stop (GstGLBaseMixer * mix);

static void gst_gl_mixer_finalize (GObject * object);

/**
 * gst_gl_mixer_class_add_rgba_pad_templates:
 * @klass: the #GstGLMixerClass
 *
 * Adds the default RGBA pad templates to this class.  If you have any special
 * template requirements like a different pad subclass or different supported
 * caps, you should not call this function and add the pad templates yourself
 * manually.
 *
 * Since: 1.24
 */
void
gst_gl_mixer_class_add_rgba_pad_templates (GstGLMixerClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &src_factory, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &sink_factory, GST_TYPE_GL_MIXER_PAD);
}

static void
gst_gl_mixer_class_init (GstGLMixerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorClass *videoaggregator_class =
      (GstVideoAggregatorClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;
  GstGLBaseMixerClass *base_class = (GstGLBaseMixerClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "glmixer", 0, "OpenGL mixer");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_gl_mixer_finalize);

  gobject_class->get_property = gst_gl_mixer_get_property;
  gobject_class->set_property = gst_gl_mixer_set_property;

  agg_class->sink_query = gst_gl_mixer_sink_query;
  agg_class->src_query = gst_gl_mixer_src_query;
  agg_class->stop = gst_gl_mixer_stop;
  agg_class->start = gst_gl_mixer_start;
  agg_class->negotiated_src_caps = _negotiated_caps;
  agg_class->decide_allocation = gst_gl_mixer_decide_allocation;
  agg_class->propose_allocation = gst_gl_mixer_propose_allocation;

  videoaggregator_class->aggregate_frames = gst_gl_mixer_aggregate_frames;
  videoaggregator_class->find_best_format = _find_best_format;

  base_class->gl_start = gst_gl_mixer_gl_start;
  base_class->gl_stop = gst_gl_mixer_gl_stop;

  /* Register the pad class */
  g_type_class_ref (GST_TYPE_GL_MIXER_PAD);
}

static void
gst_gl_mixer_reset (GstGLMixer * mix)
{
  mix->priv->negotiated = FALSE;
}

static void
gst_gl_mixer_init (GstGLMixer * mix)
{
  mix->priv = gst_gl_mixer_get_instance_private (mix);

  mix->priv->gl_resource_ready = FALSE;
  g_mutex_init (&mix->priv->gl_resource_lock);
  g_cond_init (&mix->priv->gl_resource_cond);
  /* initialize variables */
  gst_gl_mixer_reset (mix);
}

static void
gst_gl_mixer_finalize (GObject * object)
{
  GstGLMixer *mix = GST_GL_MIXER (object);
  GstGLMixerPrivate *priv = mix->priv;

  if (mix->out_caps)
    gst_caps_unref (mix->out_caps);

  g_mutex_clear (&priv->gl_resource_lock);
  g_cond_clear (&priv->gl_resource_cond);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_gl_mixer_query_caps (GstPad * pad, GstAggregator * agg, GstQuery * query)
{
  GstCaps *filter, *current_caps, *retcaps, *template_caps;

  gst_query_parse_caps (query, &filter);

  template_caps = gst_pad_get_pad_template_caps (agg->srcpad);

  current_caps = gst_pad_get_current_caps (pad);
  if (current_caps == NULL)
    retcaps = gst_caps_ref (template_caps);
  else {
    retcaps = gst_caps_merge (current_caps, template_caps);
    template_caps = NULL;
  }

  if (filter) {
    current_caps =
        gst_caps_intersect_full (filter, retcaps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (retcaps);
    retcaps = current_caps;
  }

  gst_query_set_caps_result (query, retcaps);
  gst_caps_unref (retcaps);

  if (template_caps)
    gst_caps_unref (template_caps);

  return TRUE;
}

static gboolean
gst_gl_mixer_src_query (GstAggregator * agg, GstQuery * query)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      res = gst_gl_mixer_query_caps (agg->srcpad, agg, query);
      break;
    default:
      res = GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
      break;
  }

  return res;
}

static void
_mixer_create_fbo (GstGLContext * context, GstGLMixer * mix)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (mix);
  guint out_width = GST_VIDEO_INFO_WIDTH (&vagg->info);
  guint out_height = GST_VIDEO_INFO_HEIGHT (&vagg->info);

  g_mutex_lock (&mix->priv->gl_resource_lock);
  if (!mix->priv->fbo)
    mix->priv->fbo =
        gst_gl_framebuffer_new_with_default_depth (context, out_width,
        out_height);
  g_cond_signal (&mix->priv->gl_resource_cond);
  if (mix->priv->fbo)
    mix->priv->gl_resource_ready = TRUE;
  g_mutex_unlock (&mix->priv->gl_resource_lock);
}

static gboolean
gst_gl_mixer_gl_start (GstGLBaseMixer * base_mix)
{
  return GST_GL_BASE_MIXER_CLASS (parent_class)->gl_start (base_mix);
}

static void
gst_gl_mixer_gl_stop (GstGLBaseMixer * base_mix)
{
  GstGLMixer *mix = GST_GL_MIXER (base_mix);

  g_mutex_lock (&mix->priv->gl_resource_lock);
  gst_clear_object (&mix->priv->fbo);
  g_mutex_unlock (&mix->priv->gl_resource_lock);

  GST_GL_BASE_MIXER_CLASS (parent_class)->gl_stop (base_mix);
}

static gboolean
gst_gl_mixer_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstGLBaseMixer *base_mix = GST_GL_BASE_MIXER (agg);
  GstGLMixer *mix = GST_GL_MIXER (base_mix);
  GstGLContext *context;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;

  if (!GST_AGGREGATOR_CLASS (gst_gl_mixer_parent_class)->decide_allocation (agg,
          query))
    return FALSE;

  context = gst_gl_base_mixer_get_gl_context (base_mix);
  if (!context) {
    GST_WARNING_OBJECT (agg, "No OpenGL context");
    return FALSE;
  }

  g_mutex_lock (&mix->priv->gl_resource_lock);
  mix->priv->gl_resource_ready = FALSE;
  gst_clear_object (&mix->priv->fbo);
  g_mutex_unlock (&mix->priv->gl_resource_lock);

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _mixer_create_fbo, mix);

  g_mutex_lock (&mix->priv->gl_resource_lock);
  if (!mix->priv->fbo) {
    mix->priv->gl_resource_ready = FALSE;
    g_mutex_unlock (&mix->priv->gl_resource_lock);
    goto context_error;
  }

  mix->priv->gl_resource_ready = TRUE;
  g_cond_signal (&mix->priv->gl_resource_cond);
  g_mutex_unlock (&mix->priv->gl_resource_lock);

  gst_query_parse_allocation (query, &caps, NULL);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool)
    pool = gst_gl_buffer_pool_new (context);
  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (gst_query_find_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL))
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  gst_buffer_pool_set_config (pool, config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  gst_clear_object (&context);

  return TRUE;

context_error:
  {
    GST_ELEMENT_ERROR (mix, RESOURCE, NOT_FOUND, ("Context error"), (NULL));
    return FALSE;
  }
}

/**
 * gst_gl_mixer_process_textures:
 * @mix: the #GstGLMixer
 * @outbuf: output @GstBuffer
 *
 * Perform processing required and call #GstGLMixerClass::process_textures().
 * Intended for use within implementations of
 * #GstGLMixerClass::process_buffers().
 *
 * Returns: whether processing of textures succeeded
 *
 * Since: 1.24
 */
gboolean
gst_gl_mixer_process_textures (GstGLMixer * mix, GstBuffer * outbuf)
{
  GstGLMemory *out_tex;
  gboolean res = TRUE;
  GstVideoFrame out_frame;
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (mix);
  GstGLMixerClass *mix_class = GST_GL_MIXER_GET_CLASS (mix);

  GST_TRACE ("Processing buffers");

  if (!gst_video_frame_map (&out_frame, &vagg->info, outbuf,
          GST_MAP_WRITE | GST_MAP_GL)) {
    return FALSE;
  }

  out_tex = (GstGLMemory *) out_frame.map[0].memory;

  g_mutex_lock (&mix->priv->gl_resource_lock);
  if (!mix->priv->gl_resource_ready)
    g_cond_wait (&mix->priv->gl_resource_cond, &mix->priv->gl_resource_lock);

  if (!mix->priv->gl_resource_ready) {
    g_mutex_unlock (&mix->priv->gl_resource_lock);
    GST_ERROR_OBJECT (mix,
        "fbo used to render can't be created, do not run process_textures");
    res = FALSE;
    goto out;
  }

  g_mutex_unlock (&mix->priv->gl_resource_lock);

  mix_class->process_textures (mix, out_tex);

out:
  gst_video_frame_unmap (&out_frame);

  return res;
}

static gboolean
gst_gl_mixer_process_buffers (GstGLMixer * mix, GstBuffer * outbuf)
{
  GstGLMixerClass *mix_class = GST_GL_MIXER_GET_CLASS (mix);

  return mix_class->process_buffers (mix, outbuf);
}

static GstFlowReturn
gst_gl_mixer_aggregate_frames (GstVideoAggregator * vagg, GstBuffer * outbuf)
{
  GstGLBaseMixer *base_mix = GST_GL_BASE_MIXER (vagg);
  gboolean res = FALSE;
  GstGLMixer *mix = GST_GL_MIXER (vagg);
  GstGLMixerClass *mix_class = GST_GL_MIXER_GET_CLASS (vagg);
  GstGLContext *context = gst_gl_base_mixer_get_gl_context (base_mix);
  GstGLSyncMeta *sync_meta;

  if (!context) {
    GST_DEBUG_OBJECT (vagg, "No OpenGL context, try again later");
    return GST_AGGREGATOR_FLOW_NEED_DATA;
  }

  if (mix_class->process_buffers)
    res = gst_gl_mixer_process_buffers (mix, outbuf);
  else if (mix_class->process_textures)
    res = gst_gl_mixer_process_textures (mix, outbuf);

  sync_meta = gst_buffer_get_gl_sync_meta (outbuf);
  if (sync_meta)
    gst_gl_sync_meta_set_sync_point (sync_meta, context);

  gst_clear_object (&context);

  return res ? GST_FLOW_OK : GST_FLOW_ERROR;
}

static void
gst_gl_mixer_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_mixer_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_mixer_start (GstAggregator * agg)
{
  return GST_AGGREGATOR_CLASS (parent_class)->start (agg);
}

static gboolean
gst_gl_mixer_stop (GstAggregator * agg)
{
  GstGLMixer *mix = GST_GL_MIXER (agg);

  gst_gl_mixer_reset (mix);

  return GST_AGGREGATOR_CLASS (parent_class)->stop (agg);
}

/**
 * gst_gl_mixer_get_framebuffer:
 * @mix: the #GstGLMixer
 *
 * Returns: (transfer full): (nullable): The #GstGLFramebuffer in use by this @mix
 *
 * Since: 1.24
 */
GstGLFramebuffer *
gst_gl_mixer_get_framebuffer (GstGLMixer * mix)
{
  GstGLFramebuffer *fbo = NULL;
  g_mutex_lock (&mix->priv->gl_resource_lock);
  if (mix->priv->fbo)
    fbo = gst_object_ref (mix->priv->fbo);
  g_mutex_unlock (&mix->priv->gl_resource_lock);
  return fbo;
}
