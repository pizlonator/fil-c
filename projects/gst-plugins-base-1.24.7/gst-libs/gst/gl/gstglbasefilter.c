/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <gst/video/gstvideometa.h>

#include <gst/gl/gl.h>
#include <gst/gl/gstglutils_private.h>

/**
 * SECTION:gstglbasefilter
 * @short_description: #GstBaseTransform subclass for transforming OpenGL resources
 * @title: GstGLBaseFilter
 * @see_also: #GstBaseTransform
 *
 * #GstGLBaseFilter handles the nitty gritty details of retrieving an OpenGL
 * context.  It also provided some wrappers around #GstBaseTransform's
 * `start()`, `stop()` and `set_caps()` virtual methods that ensure an OpenGL
 * context is available and current in the calling thread.
 */

#define GST_CAT_DEFAULT gst_gl_base_filter_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

struct _GstGLBaseFilterPrivate
{
  GstGLContext *other_context;

  gboolean gl_result;
  gboolean gl_started;

  GRecMutex context_lock;
  gboolean new_gl_context;
};

/* Properties */
enum
{
  PROP_0,
  PROP_CONTEXT
};

#define gst_gl_base_filter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLBaseFilter, gst_gl_base_filter,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstGLBaseFilter)
    GST_DEBUG_CATEGORY_INIT (gst_gl_base_filter_debug,
        "glbasefilter", 0, "glbasefilter element");
    );

static void gst_gl_base_filter_finalize (GObject * object);
static void gst_gl_base_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_base_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_gl_base_filter_set_context (GstElement * element,
    GstContext * context);
static GstStateChangeReturn gst_gl_base_filter_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_gl_base_filter_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static void gst_gl_base_filter_reset (GstGLBaseFilter * filter);
static gboolean gst_gl_base_filter_start (GstBaseTransform * bt);
static gboolean gst_gl_base_filter_stop (GstBaseTransform * bt);
static gboolean gst_gl_base_filter_set_caps (GstBaseTransform * bt,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_gl_base_filter_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);

/* GstGLContextThreadFunc */
static void gst_gl_base_filter_gl_start (GstGLContext * context, gpointer data);
static void gst_gl_base_filter_gl_stop (GstGLContext * context, gpointer data);

static gboolean gst_gl_base_filter_default_gl_start (GstGLBaseFilter * filter);
static void gst_gl_base_filter_default_gl_stop (GstGLBaseFilter * filter);

static gboolean gst_gl_base_filter_find_gl_context_unlocked (GstGLBaseFilter *
    filter);
static gboolean gst_gl_base_filter_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static void
gst_gl_base_filter_class_init (GstGLBaseFilterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_gl_base_filter_finalize;
  gobject_class->set_property = gst_gl_base_filter_set_property;
  gobject_class->get_property = gst_gl_base_filter_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->query = gst_gl_base_filter_query;
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps = gst_gl_base_filter_set_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->start = gst_gl_base_filter_start;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_base_filter_stop;
  GST_BASE_TRANSFORM_CLASS (klass)->decide_allocation =
      gst_gl_base_filter_decide_allocation;
  GST_BASE_TRANSFORM_CLASS (klass)->transform_meta =
      gst_gl_base_filter_transform_meta;

  element_class->set_context = gst_gl_base_filter_set_context;
  element_class->change_state = gst_gl_base_filter_change_state;

  g_object_class_install_property (gobject_class, PROP_CONTEXT,
      g_param_spec_object ("context",
          "OpenGL context",
          "Get OpenGL context",
          GST_TYPE_GL_CONTEXT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  klass->supported_gl_api = GST_GL_API_ANY;
  klass->gl_start = gst_gl_base_filter_default_gl_start;
  klass->gl_stop = gst_gl_base_filter_default_gl_stop;

  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static void
gst_gl_base_filter_init (GstGLBaseFilter * filter)
{
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (filter), TRUE);

  filter->priv = gst_gl_base_filter_get_instance_private (filter);

  g_rec_mutex_init (&filter->priv->context_lock);
}

static void
gst_gl_base_filter_finalize (GObject * object)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (object);

  gst_caps_replace (&filter->in_caps, NULL);
  gst_caps_replace (&filter->out_caps, NULL);

  g_rec_mutex_clear (&filter->priv->context_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_base_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_base_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (object);

  switch (prop_id) {
    case PROP_CONTEXT:
      g_value_set_object (value, filter->context);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
_find_local_gl_context_unlocked (GstGLBaseFilter * filter)
{
  GstGLContext *context, *prev_context;
  gboolean ret;

  if (filter->context && filter->context->display == filter->display)
    return TRUE;

  context = prev_context = filter->context;
  g_rec_mutex_unlock (&filter->priv->context_lock);
  /* we need to drop the lock to query as another element may also be
   * performing a context query on us which would also attempt to take the
   * context_lock. Our query could block on the same lock in the other element.
   */
  ret =
      gst_gl_query_local_gl_context (GST_ELEMENT (filter), GST_PAD_SRC,
      &context);
  g_rec_mutex_lock (&filter->priv->context_lock);
  if (ret) {
    if (filter->context != prev_context) {
      /* we need to recheck everything since we dropped the lock and the
       * context has changed */
      if (filter->context && filter->context->display == filter->display) {
        if (context != filter->context)
          gst_clear_object (&context);
        return TRUE;
      }
    }

    if (context->display == filter->display) {
      filter->context = context;
      return TRUE;
    }
    if (context != filter->context)
      gst_clear_object (&context);
  }

  context = prev_context = filter->context;
  g_rec_mutex_unlock (&filter->priv->context_lock);
  /* we need to drop the lock to query as another element may also be
   * performing a context query on us which would also attempt to take the
   * context_lock. Our query could block on the same lock in the other element.
   */
  ret =
      gst_gl_query_local_gl_context (GST_ELEMENT (filter), GST_PAD_SINK,
      &context);
  g_rec_mutex_lock (&filter->priv->context_lock);
  if (ret) {
    if (filter->context != prev_context) {
      /* we need to recheck everything now that we dropped the lock */
      if (filter->context && filter->context->display == filter->display) {
        if (context != filter->context)
          gst_clear_object (&context);
        return TRUE;
      }
    }

    if (context->display == filter->display) {
      filter->context = context;
      return TRUE;
    }
    if (context != filter->context)
      gst_clear_object (&context);
  }

  return FALSE;
}

static gboolean
_find_local_gl_context (GstGLBaseFilter * filter)
{
  gboolean ret;
  g_rec_mutex_lock (&filter->priv->context_lock);
  ret = _find_local_gl_context_unlocked (filter);
  g_rec_mutex_unlock (&filter->priv->context_lock);
  return ret;
}

static gboolean
gst_gl_base_filter_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
    {
      if (direction == GST_PAD_SINK) {
        /* Ensure we have a GL context before running the query either
         * downstream or through subclasses */
        _find_local_gl_context (filter);

        if (gst_base_transform_is_passthrough (trans)) {
          return gst_pad_peer_query (GST_BASE_TRANSFORM_SRC_PAD (trans), query);
        }
      }
      break;
    }
    case GST_QUERY_CONTEXT:
    {
      GstGLDisplay *display = NULL;
      GstGLContext *other = NULL, *local = NULL;
      gboolean ret;

      g_rec_mutex_lock (&filter->priv->context_lock);
      if (filter->display)
        display = gst_object_ref (filter->display);
      if (filter->context)
        local = gst_object_ref (filter->context);
      if (filter->priv->other_context)
        other = gst_object_ref (filter->priv->other_context);
      g_rec_mutex_unlock (&filter->priv->context_lock);

      ret = gst_gl_handle_context_query ((GstElement *) filter, query,
          display, local, other);

      gst_clear_object (&display);
      gst_clear_object (&other);
      gst_clear_object (&local);

      if (ret)
        return TRUE;
      break;
    }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static void
gst_gl_base_filter_reset (GstGLBaseFilter * filter)
{
  g_rec_mutex_lock (&filter->priv->context_lock);
  if (filter->context) {
    gst_gl_context_thread_add (filter->context, gst_gl_base_filter_gl_stop,
        filter);
    gst_object_unref (filter->context);
    filter->context = NULL;
  }
  g_rec_mutex_unlock (&filter->priv->context_lock);
}

static gboolean
gst_gl_base_filter_start (GstBaseTransform * bt)
{
  return TRUE;
}

static gboolean
gst_gl_base_filter_stop (GstBaseTransform * bt)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (bt);

  gst_gl_base_filter_reset (filter);

  return TRUE;
}

static gboolean
gst_gl_base_filter_default_gl_start (GstGLBaseFilter * filter)
{
  return TRUE;
}

static void
gst_gl_base_filter_gl_start (GstGLContext * context, gpointer data)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (data);
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);

  GST_INFO_OBJECT (filter, "starting");
  gst_gl_insert_debug_marker (filter->context,
      "starting element %s", GST_OBJECT_NAME (filter));

  filter->priv->gl_started = filter_class->gl_start (filter);
}

static void
gst_gl_base_filter_default_gl_stop (GstGLBaseFilter * filter)
{
}

static void
gst_gl_base_filter_gl_stop (GstGLContext * context, gpointer data)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (data);
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);

  GST_INFO_OBJECT (filter, "stopping");
  gst_gl_insert_debug_marker (filter->context,
      "stopping element %s", GST_OBJECT_NAME (filter));

  if (filter->priv->gl_started)
    filter_class->gl_stop (filter);

  filter->priv->gl_started = FALSE;
}

static void
_gl_set_caps (GstGLContext * context, GstGLBaseFilter * filter)
{
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);

  GST_INFO_OBJECT (filter, "set GL caps input %" GST_PTR_FORMAT,
      filter->in_caps);
  GST_INFO_OBJECT (filter, "set GL caps output %" GST_PTR_FORMAT,
      filter->out_caps);

  if (filter_class->gl_set_caps)
    filter->priv->gl_result =
        filter_class->gl_set_caps (filter, filter->in_caps, filter->out_caps);
}

static gboolean
gl_set_caps_unlocked (GstGLBaseFilter * filter)
{
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);

  if (filter_class->gl_set_caps) {
    gst_gl_context_thread_add (filter->context,
        (GstGLContextThreadFunc) _gl_set_caps, filter);
    return filter->priv->gl_result;
  }

  return TRUE;
}

static gboolean
gst_gl_base_filter_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (trans);

  g_rec_mutex_lock (&filter->priv->context_lock);
  if (!gst_gl_base_filter_find_gl_context_unlocked (filter)) {
    g_rec_mutex_unlock (&filter->priv->context_lock);
    return FALSE;
  }

  if (!gl_set_caps_unlocked (filter))
    goto error;

  g_rec_mutex_unlock (&filter->priv->context_lock);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);

error:
  {
    g_rec_mutex_unlock (&filter->priv->context_lock);
    GST_ELEMENT_ERROR (trans, LIBRARY, INIT,
        ("Subclass failed to initialize."), (NULL));
    return FALSE;
  }
}

static gboolean
gst_gl_base_filter_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (bt);

  gst_caps_replace (&filter->in_caps, incaps);
  gst_caps_replace (&filter->out_caps, outcaps);

  return TRUE;
}

static GstStateChangeReturn
gst_gl_base_filter_change_state (GstElement * element,
    GstStateChange transition)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (element);
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (filter, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gl_ensure_element_data (element, &filter->display,
              &filter->priv->other_context))
        return GST_STATE_CHANGE_FAILURE;

      gst_gl_display_filter_gl_api (filter->display,
          filter_class->supported_gl_api);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_rec_mutex_lock (&filter->priv->context_lock);
      gst_clear_object (&filter->priv->other_context);
      gst_clear_object (&filter->display);
      gst_clear_object (&filter->context);
      g_rec_mutex_unlock (&filter->priv->context_lock);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_gl_base_filter_set_context (GstElement * element, GstContext * context)
{
  GstGLBaseFilter *filter = GST_GL_BASE_FILTER (element);
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);
  GstGLDisplay *old_display, *new_display;

  g_rec_mutex_lock (&filter->priv->context_lock);
  old_display = filter->display ? gst_object_ref (filter->display) : NULL;
  gst_gl_handle_set_context (element, context, &filter->display,
      &filter->priv->other_context);
  if (filter->display)
    gst_gl_display_filter_gl_api (filter->display,
        filter_class->supported_gl_api);
  new_display = filter->display ? gst_object_ref (filter->display) : NULL;

  if (old_display && new_display) {
    if (old_display != new_display) {
      gst_clear_object (&filter->context);
      if (gst_gl_base_filter_find_gl_context_unlocked (filter)) {
        if (filter->in_caps && filter->out_caps) {
          gl_set_caps_unlocked (filter);
        }
      }
    }
  }
  g_rec_mutex_unlock (&filter->priv->context_lock);
  gst_clear_object (&old_display);
  gst_clear_object (&new_display);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_gl_base_filter_find_gl_context_unlocked (GstGLBaseFilter * filter)
{
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (filter);
  GError *error = NULL;
  gboolean new_context = FALSE;

  GST_DEBUG_OBJECT (filter, "attempting to find an OpenGL context, existing %"
      GST_PTR_FORMAT, filter->context);

  if (!filter->context)
    new_context = TRUE;

  _find_local_gl_context_unlocked (filter);

  if (!filter->display) {
    GST_WARNING_OBJECT (filter, "filter has NULL display.");
    return FALSE;
  }

  if (!gst_gl_display_ensure_context (filter->display,
          filter->priv->other_context, &filter->context, &error)) {
    goto context_error;
  }

  GST_INFO_OBJECT (filter, "found OpenGL context %" GST_PTR_FORMAT,
      filter->context);

  if (new_context || !filter->priv->gl_started) {
    if (filter->priv->gl_started)
      gst_gl_context_thread_add (filter->context, gst_gl_base_filter_gl_stop,
          filter);

    {
      GstGLAPI current_gl_api = gst_gl_context_get_gl_api (filter->context);
      if ((current_gl_api & filter_class->supported_gl_api) == 0)
        goto unsupported_gl_api;
    }

    gst_gl_context_thread_add (filter->context, gst_gl_base_filter_gl_start,
        filter);

    if (!filter->priv->gl_started)
      goto error;
  }

  return TRUE;

unsupported_gl_api:
  {
    GstGLAPI gl_api = gst_gl_context_get_gl_api (filter->context);
    gchar *gl_api_str = gst_gl_api_to_string (gl_api);
    gchar *supported_gl_api_str =
        gst_gl_api_to_string (filter_class->supported_gl_api);

    GST_ELEMENT_ERROR (filter, RESOURCE, BUSY,
        ("GL API's not compatible context: %s supported: %s", gl_api_str,
            supported_gl_api_str), (NULL));

    g_free (supported_gl_api_str);
    g_free (gl_api_str);
    return FALSE;
  }
context_error:
  {
    GST_ELEMENT_ERROR (filter, RESOURCE, NOT_FOUND, ("%s", error->message),
        (NULL));
    g_clear_error (&error);
    return FALSE;
  }
error:
  {
    GST_ELEMENT_ERROR (filter, LIBRARY, INIT,
        ("Subclass failed to initialize."), (NULL));
    return FALSE;
  }
}

static gboolean
gst_gl_base_filter_transform_meta (GstBaseTransform * trans, GstBuffer * outbuf,
    GstMeta * meta, GstBuffer * inbuf)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags || (g_strv_length ((gchar **) tags) == 1
          && gst_meta_api_type_has_tag (info->api, META_TAG_VIDEO))) {
    return TRUE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (trans, outbuf,
      meta, inbuf);
}

/**
 * gst_gl_base_filter_find_gl_context:
 * @filter: a #GstGLBaseFilter
 *
 * Returns: Whether an OpenGL context could be retrieved or created successfully
 *
 * Since: 1.16
 */
gboolean
gst_gl_base_filter_find_gl_context (GstGLBaseFilter * filter)
{
  gboolean ret;
  g_rec_mutex_lock (&filter->priv->context_lock);
  ret = gst_gl_base_filter_find_gl_context_unlocked (filter);
  g_rec_mutex_unlock (&filter->priv->context_lock);
  return ret;
}

/**
 * gst_gl_base_filter_get_gl_context:
 * @filter: a #GstGLBaseFilter
 *
 * Returns: (transfer full) (nullable): the #GstGLContext found by @filter
 *
 * Since: 1.18
 */
GstGLContext *
gst_gl_base_filter_get_gl_context (GstGLBaseFilter * filter)
{
  GstGLContext *ret;

  g_return_val_if_fail (GST_IS_GL_BASE_FILTER (filter), NULL);

  g_rec_mutex_lock (&filter->priv->context_lock);
  ret = filter->context ? gst_object_ref (filter->context) : NULL;
  g_rec_mutex_unlock (&filter->priv->context_lock);
  return ret;
}
