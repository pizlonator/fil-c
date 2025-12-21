/*
 * GStreamer
 * Copyright (C) 2015 Jan Schmidt <jan@centricular.com>
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
 * SECTION:element-glstereosplit
 * @title: glstereosplit
 *
 * Receive a stereoscopic video stream and split into left/right
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glstereosplit name=s ! queue ! glimagesink s. ! queue ! glimagesink
 * ]|
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglstereosplit.h"

#define GST_CAT_DEFAULT gst_gl_stereosplit_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define SUPPORTED_GL_APIS GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3
#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_stereosplit_debug, "glstereosplit", 0, "glstereosplit element");

G_DEFINE_TYPE_WITH_CODE (GstGLStereoSplit, gst_gl_stereosplit,
    GST_TYPE_ELEMENT, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glstereosplit, "glstereosplit",
    GST_RANK_NONE, GST_TYPE_GL_STEREOSPLIT, gl_element_init (plugin));

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, "RGBA"))
    );

static GstStaticPadTemplate src_left_template = GST_STATIC_PAD_TEMPLATE ("left",
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, "RGBA"))
    );

static GstStaticPadTemplate src_right_template =
GST_STATIC_PAD_TEMPLATE ("right",
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA"))
    );

static void stereosplit_reset (GstGLStereoSplit * self);
static void stereosplit_finalize (GstGLStereoSplit * self);
static void stereosplit_set_context (GstElement * element,
    GstContext * context);
static GstFlowReturn stereosplit_chain (GstPad * pad, GstGLStereoSplit * split,
    GstBuffer * buf);
static GstStateChangeReturn stereosplit_change_state (GstElement * element,
    GstStateChange transition);
static gboolean stereosplit_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean stereosplit_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean stereosplit_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean stereosplit_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean ensure_context (GstGLStereoSplit * self);
static gboolean ensure_context_unlocked (GstGLStereoSplit * self);

static void
gst_gl_stereosplit_class_init (GstGLStereoSplitClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "GLStereoSplit", "Codec/Converter",
      "Splits a stereoscopic stream into separate left/right streams",
      "Jan Schmidt <jan@centricular.com>, "
      "Matthew Waters <matthew@centricular.com>");

  gobject_class->finalize = (GObjectFinalizeFunc) (stereosplit_finalize);

  element_class->change_state = stereosplit_change_state;
  element_class->set_context = stereosplit_set_context;

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_left_template);
  gst_element_class_add_static_pad_template (element_class,
      &src_right_template);
}

static void
gst_gl_stereosplit_init (GstGLStereoSplit * self)
{
  GstPad *pad;

  pad = self->sink_pad =
      gst_pad_new_from_static_template (&sink_template, "sink");

  gst_pad_set_chain_function (pad, (GstPadChainFunction) (stereosplit_chain));
  gst_pad_set_query_function (pad, stereosplit_sink_query);
  gst_pad_set_event_function (pad, stereosplit_sink_event);

  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);

  pad = self->left_pad =
      gst_pad_new_from_static_template (&src_left_template, "left");
  gst_pad_set_query_function (pad, stereosplit_src_query);
  gst_pad_set_event_function (pad, stereosplit_src_event);
  gst_element_add_pad (GST_ELEMENT (self), self->left_pad);

  pad = self->right_pad =
      gst_pad_new_from_static_template (&src_right_template, "right");
  gst_pad_set_query_function (pad, stereosplit_src_query);
  gst_pad_set_event_function (pad, stereosplit_src_event);
  gst_element_add_pad (GST_ELEMENT (self), self->right_pad);

  self->viewconvert = gst_gl_view_convert_new ();

  g_rec_mutex_init (&self->context_lock);
}

static void
stereosplit_reset (GstGLStereoSplit * self)
{
  if (self->context)
    gst_object_replace ((GstObject **) & self->context, NULL);
  if (self->display)
    gst_object_replace ((GstObject **) & self->display, NULL);
}

static void
stereosplit_finalize (GstGLStereoSplit * self)
{
  GObjectClass *klass = G_OBJECT_CLASS (gst_gl_stereosplit_parent_class);

  if (self->viewconvert)
    gst_object_replace ((GstObject **) & self->viewconvert, NULL);

  g_rec_mutex_clear (&self->context_lock);

  klass->finalize ((GObject *) (self));
}

static void
stereosplit_set_context (GstElement * element, GstContext * context)
{
  GstGLStereoSplit *stereosplit = GST_GL_STEREOSPLIT (element);
  GstGLDisplay *old_display, *new_display;

  g_rec_mutex_lock (&stereosplit->context_lock);
  GST_DEBUG_OBJECT (element, "set context of %" GST_PTR_FORMAT, context);
  old_display =
      stereosplit->display ? gst_object_ref (stereosplit->display) : NULL;
  gst_gl_handle_set_context (element, context, &stereosplit->display,
      &stereosplit->other_context);

  if (stereosplit->display)
    gst_gl_display_filter_gl_api (stereosplit->display, SUPPORTED_GL_APIS);

  new_display =
      stereosplit->display ? gst_object_ref (stereosplit->display) : NULL;

  if (old_display && new_display) {
    if (old_display != new_display) {
      gst_clear_object (&stereosplit->context);
      gst_gl_view_convert_set_context (stereosplit->viewconvert, NULL);
      GST_INFO_OBJECT (stereosplit, "display changed to %" GST_PTR_FORMAT,
          new_display);
      if (ensure_context_unlocked (stereosplit)) {
        gst_gl_view_convert_set_context (stereosplit->viewconvert,
            stereosplit->context);
      }
    }
  }
  gst_clear_object (&old_display);
  gst_clear_object (&new_display);
  g_rec_mutex_unlock (&stereosplit->context_lock);

  GST_ELEMENT_CLASS (gst_gl_stereosplit_parent_class)->set_context (element,
      context);
}

static GstStateChangeReturn
stereosplit_change_state (GstElement * element, GstStateChange transition)
{
  GstGLStereoSplit *stereosplit = GST_GL_STEREOSPLIT (element);
  GstStateChangeReturn result;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      g_rec_mutex_lock (&stereosplit->context_lock);
      if (!gst_gl_ensure_element_data (element, &stereosplit->display,
              &stereosplit->other_context))
        return GST_STATE_CHANGE_FAILURE;

      gst_gl_display_filter_gl_api (stereosplit->display, SUPPORTED_GL_APIS);
      g_rec_mutex_unlock (&stereosplit->context_lock);
      break;
    default:
      break;
  }

  result =
      GST_ELEMENT_CLASS (gst_gl_stereosplit_parent_class)->change_state
      (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_rec_mutex_lock (&stereosplit->context_lock);
      gst_clear_object (&stereosplit->other_context);
      gst_clear_object (&stereosplit->display);
      g_rec_mutex_unlock (&stereosplit->context_lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      stereosplit_reset (stereosplit);
      break;
    default:
      break;
  }

  return result;
}

static GstCaps *
stereosplit_transform_caps (GstGLStereoSplit * self, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{
  GstCaps *next_caps;

  next_caps =
      gst_gl_view_convert_transform_caps (self->viewconvert, direction, caps,
      NULL);

  return next_caps;
}

static GstCaps *
strip_mview_fields (GstCaps * incaps, GstVideoMultiviewFlags keep_flags)
{
  GstCaps *outcaps = gst_caps_make_writable (incaps);

  gint i, n;

  n = gst_caps_get_size (outcaps);
  for (i = 0; i < n; i++) {
    GstStructure *st = gst_caps_get_structure (outcaps, i);
    GstVideoMultiviewFlags flags, mask;

    gst_structure_remove_field (st, "multiview-mode");
    if (gst_structure_get_flagset (st, "multiview-flags", (guint *) & flags,
            (guint *) & mask)) {
      flags &= keep_flags;
      mask = keep_flags;
      gst_structure_set (st, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, flags, mask, NULL);
    }
  }

  return outcaps;
}

static gboolean stereosplit_do_bufferpool (GstGLStereoSplit * self,
    GstCaps * caps);

static GstCaps *
stereosplit_get_src_caps (GstGLStereoSplit * split,
    GstPad * pad, GstVideoMultiviewMode preferred_mode)
{
  GstCaps *outcaps, *tmp, *templ_caps;
  GValue item = G_VALUE_INIT, list = G_VALUE_INIT;

  /* Get the template format */
  templ_caps = gst_pad_get_pad_template_caps (pad);

  /* And limit down to the preferred mode or mono */
  templ_caps = gst_caps_make_writable (templ_caps);

  g_value_init (&item, G_TYPE_STRING);
  g_value_init (&list, GST_TYPE_LIST);
  g_value_set_static_string (&item,
      gst_video_multiview_mode_to_caps_string (preferred_mode));
  gst_value_list_append_value (&list, &item);
  g_value_set_static_string (&item,
      gst_video_multiview_mode_to_caps_string (GST_VIDEO_MULTIVIEW_MODE_MONO));
  gst_value_list_append_value (&list, &item);

  gst_caps_set_value (templ_caps, "multiview-mode", &list);

  g_value_unset (&list);
  g_value_unset (&item);

  /* And intersect with the peer */
  if ((tmp = gst_pad_peer_query_caps (pad, NULL)) == NULL) {
    gst_caps_unref (templ_caps);
    return NULL;
  }

  outcaps = gst_caps_intersect_full (tmp, templ_caps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (tmp);
  gst_caps_unref (templ_caps);

  GST_DEBUG_OBJECT (split, "Src pad %" GST_PTR_FORMAT " caps %" GST_PTR_FORMAT,
      pad, outcaps);
  return outcaps;
}

static gboolean
stereosplit_set_output_caps (GstGLStereoSplit * split, GstCaps * sinkcaps)
{
  GstCaps *left = NULL, *right = NULL, *tridcaps = NULL;
  GstCaps *tmp, *combined;
  gboolean res = FALSE;

  /* Choose some preferred output caps.
   * Keep input width/height and PAR, preserve preferred output
   * multiview flags for flipping/flopping if any, and set each
   * left right pad to either left/mono and right/mono, as they prefer
   */

  if (!ensure_context (split)) {
    res = FALSE;
    goto fail;
  }

  /* Calculate what downstream can collectively support */
  left =
      stereosplit_get_src_caps (split, split->left_pad,
      GST_VIDEO_MULTIVIEW_MODE_LEFT);
  if (left == NULL)
    goto fail;
  right =
      stereosplit_get_src_caps (split, split->right_pad,
      GST_VIDEO_MULTIVIEW_MODE_RIGHT);
  if (right == NULL)
    goto fail;

  tridcaps = stereosplit_transform_caps (split, GST_PAD_SINK, sinkcaps, NULL);

  if (!tridcaps || gst_caps_is_empty (tridcaps)) {
    GST_ERROR_OBJECT (split,
        "Failed to transform input caps %" GST_PTR_FORMAT, sinkcaps);
    goto fail;
  }

  /* Preserve downstream preferred flipping/flopping */
  tmp =
      strip_mview_fields (gst_caps_ref (left),
      GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED |
      GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED);
  combined = gst_caps_intersect (tridcaps, tmp);
  gst_caps_unref (tridcaps);
  gst_caps_unref (tmp);
  tridcaps = combined;

  tmp =
      strip_mview_fields (gst_caps_ref (right),
      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED |
      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED);
  combined = gst_caps_intersect (tridcaps, tmp);
  gst_caps_unref (tridcaps);
  gst_caps_unref (tmp);
  tridcaps = combined;

  if (G_UNLIKELY (gst_caps_is_empty (tridcaps))) {
    gst_caps_unref (tridcaps);
    goto fail;
  }

  /* Now generate the version for each output pad */
  GST_DEBUG_OBJECT (split, "Attempting to set output caps %" GST_PTR_FORMAT,
      tridcaps);
  tmp = gst_caps_intersect (tridcaps, left);
  gst_caps_unref (left);
  left = tmp;
  left = gst_caps_fixate (left);
  if (!gst_pad_set_caps (split->left_pad, left)) {
    GST_ERROR_OBJECT (split,
        "Failed to set left output caps %" GST_PTR_FORMAT, left);
    goto fail;
  }

  tmp = gst_caps_intersect (tridcaps, right);
  gst_caps_unref (right);
  right = tmp;
  right = gst_caps_fixate (right);
  if (!gst_pad_set_caps (split->right_pad, right)) {
    GST_ERROR_OBJECT (split,
        "Failed to set right output caps %" GST_PTR_FORMAT, right);
    goto fail;
  }

  /* FIXME: Provide left and right caps to do_bufferpool */
  stereosplit_do_bufferpool (split, left);

  g_rec_mutex_lock (&split->context_lock);
  gst_gl_view_convert_set_context (split->viewconvert, split->context);

  tridcaps = gst_caps_make_writable (tridcaps);
  gst_caps_set_simple (tridcaps, "multiview-mode", G_TYPE_STRING,
      "separated", "views", G_TYPE_INT, 2, NULL);
  tridcaps = gst_caps_fixate (tridcaps);

  if (!gst_gl_view_convert_set_caps (split->viewconvert, sinkcaps, tridcaps)) {
    g_rec_mutex_unlock (&split->context_lock);
    GST_ERROR_OBJECT (split, "Failed to set caps on converter");
    goto fail;
  }
  g_rec_mutex_unlock (&split->context_lock);

  res = TRUE;

fail:
  if (left)
    gst_caps_unref (left);
  if (right)
    gst_caps_unref (right);
  if (tridcaps)
    gst_caps_unref (tridcaps);
  return res;
}

static gboolean
_find_local_gl_context_unlocked (GstGLStereoSplit * split)
{
  GstGLContext *context, *prev_context;
  gboolean ret;

  if (split->context && split->context->display == split->display)
    return TRUE;

  context = prev_context = split->context;
  g_rec_mutex_unlock (&split->context_lock);
  /* we need to drop the lock to query as another element may also be
   * performing a context query on us which would also attempt to take the
   * context_lock. Our query could block on the same lock in the other element.
   */
  ret =
      gst_gl_query_local_gl_context (GST_ELEMENT (split), GST_PAD_SRC,
      &context);
  g_rec_mutex_lock (&split->context_lock);
  if (ret) {
    if (split->context != prev_context) {
      /* we need to recheck everything since we dropped the lock and the
       * context has changed */
      if (split->context && split->context->display == split->display) {
        if (context != split->context)
          gst_clear_object (&context);
        return TRUE;
      }
    }

    if (context->display == split->display) {
      split->context = context;
      return TRUE;
    }
    if (context != split->context)
      gst_clear_object (&context);
  }

  context = prev_context = split->context;
  g_rec_mutex_unlock (&split->context_lock);
  /* we need to drop the lock to query as another element may also be
   * performing a context query on us which would also attempt to take the
   * context_lock. Our query could block on the same lock in the other element.
   */
  ret =
      gst_gl_query_local_gl_context (GST_ELEMENT (split), GST_PAD_SINK,
      &context);
  g_rec_mutex_lock (&split->context_lock);
  if (ret) {
    if (split->context != prev_context) {
      /* we need to recheck everything now that we dropped the lock */
      if (split->context && split->context->display == split->display) {
        if (context != split->context)
          gst_clear_object (&context);
        return TRUE;
      }
    }

    if (context->display == split->display) {
      split->context = context;
      return TRUE;
    }
    if (context != split->context)
      gst_clear_object (&context);
  }

  return FALSE;
}

static gboolean
ensure_context_unlocked (GstGLStereoSplit * self)
{
  GError *error = NULL;

  GST_DEBUG_OBJECT (self, "attempting to find an OpenGL context, existing %"
      GST_PTR_FORMAT, self->context);

  if (!gst_gl_ensure_element_data (self, &self->display, &self->other_context))
    return FALSE;

  gst_gl_display_filter_gl_api (self->display, SUPPORTED_GL_APIS);

  _find_local_gl_context_unlocked (self);

  if (!gst_gl_display_ensure_context (self->display, self->other_context,
          &self->context, &error)) {
    goto context_error;
  }

  {
    GstGLAPI current_gl_api = gst_gl_context_get_gl_api (self->context);
    if ((current_gl_api & (SUPPORTED_GL_APIS)) == 0)
      goto unsupported_gl_api;
  }

  GST_INFO_OBJECT (self, "found OpenGL context %" GST_PTR_FORMAT,
      self->context);

  return TRUE;

unsupported_gl_api:
  {
    GstGLAPI gl_api = gst_gl_context_get_gl_api (self->context);
    gchar *gl_api_str = gst_gl_api_to_string (gl_api);
    gchar *supported_gl_api_str = gst_gl_api_to_string (SUPPORTED_GL_APIS);
    GST_ELEMENT_ERROR (self, RESOURCE, BUSY,
        ("GL API's not compatible context: %s supported: %s", gl_api_str,
            supported_gl_api_str), (NULL));

    g_free (supported_gl_api_str);
    g_free (gl_api_str);
    return FALSE;
  }
context_error:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND, ("%s", error->message),
        (NULL));
    g_clear_error (&error);
    return FALSE;
  }
}

static gboolean
ensure_context (GstGLStereoSplit * self)
{
  gboolean ret;
  g_rec_mutex_lock (&self->context_lock);
  ret = ensure_context_unlocked (self);
  g_rec_mutex_unlock (&self->context_lock);
  return ret;
}

static gboolean
stereosplit_decide_allocation (GstGLStereoSplit * self, GstQuery * query)
{
  if (!ensure_context_unlocked (self))
    return FALSE;

  return TRUE;
}

static gboolean
stereosplit_propose_allocation (GstGLStereoSplit * self, GstQuery * query)
{
  if (!gst_gl_ensure_element_data (self, &self->display, &self->other_context))
    return FALSE;

  return TRUE;
}

static gboolean
stereosplit_do_bufferpool (GstGLStereoSplit * self, GstCaps * caps)
{
  GstQuery *query;

  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (self->left_pad, query)) {
    if (!gst_pad_peer_query (self->right_pad, query)) {
      GST_DEBUG_OBJECT (self, "peer ALLOCATION query failed on both src pads");
    }
  }

  if (!stereosplit_decide_allocation (self, query)) {
    gst_query_unref (query);
    return FALSE;
  }

  gst_query_unref (query);
  return TRUE;
}

static GstFlowReturn
stereosplit_chain (GstPad * pad, GstGLStereoSplit * split, GstBuffer * buf)
{
  GstBuffer *left, *right;
  GstBuffer *split_buffer = NULL;
  GstFlowReturn ret;
  gint i, n_planes;

  n_planes = GST_VIDEO_INFO_N_PLANES (&split->viewconvert->out_info);

  GST_LOG_OBJECT (split, "chaining buffer %" GST_PTR_FORMAT, buf);

  gst_buffer_ref (buf);

  g_rec_mutex_lock (&split->context_lock);

  if (gst_gl_view_convert_submit_input_buffer (split->viewconvert,
          GST_BUFFER_IS_DISCONT (buf), buf) != GST_FLOW_OK) {
    g_rec_mutex_unlock (&split->context_lock);
    GST_ELEMENT_ERROR (split, RESOURCE, NOT_FOUND, ("%s",
            "Failed to 3d convert buffer"),
        ("Could not get submit input buffer"));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  ret = gst_gl_view_convert_get_output (split->viewconvert, &split_buffer);
  g_rec_mutex_unlock (&split->context_lock);
  if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (split, RESOURCE, NOT_FOUND, ("%s",
            "Failed to 3d convert buffer"), ("Could not get output buffer"));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
  if (split_buffer == NULL) {
    gst_buffer_unref (buf);
    return GST_FLOW_OK;         /* Need another input buffer */
  }

  left = gst_buffer_new ();
  gst_buffer_copy_into (left, buf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
  GST_BUFFER_FLAG_UNSET (left, GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE);

  gst_buffer_add_parent_buffer_meta (left, split_buffer);

  for (i = 0; i < n_planes; i++) {
    GstMemory *mem = gst_buffer_get_memory (split_buffer, i);
    gst_buffer_append_memory (left, mem);
  }

  ret = gst_pad_push (split->left_pad, gst_buffer_ref (left));
  /* Allow unlinked on the first pad - as long as the 2nd isn't unlinked */
  gst_buffer_unref (left);
  if (G_UNLIKELY (ret != GST_FLOW_OK && ret != GST_FLOW_NOT_LINKED)) {
    gst_buffer_unref (split_buffer);
    gst_buffer_unref (buf);
    return ret;
  }

  right = gst_buffer_new ();
  gst_buffer_copy_into (right, buf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
  GST_BUFFER_FLAG_UNSET (left, GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE);
  gst_buffer_add_parent_buffer_meta (right, split_buffer);
  for (i = n_planes; i < n_planes * 2; i++) {
    GstMemory *mem = gst_buffer_get_memory (split_buffer, i);
    gst_buffer_append_memory (right, mem);
  }

  ret = gst_pad_push (split->right_pad, gst_buffer_ref (right));
  gst_buffer_unref (right);
  gst_buffer_unref (split_buffer);
  gst_buffer_unref (buf);
  return ret;
}

static gboolean
stereosplit_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstGLStereoSplit *split = GST_GL_STEREOSPLIT (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      GstGLDisplay *display = NULL;
      GstGLContext *other = NULL, *local = NULL;
      gboolean ret;

      g_rec_mutex_lock (&split->context_lock);
      if (split->display)
        display = gst_object_ref (split->display);
      if (split->context)
        local = gst_object_ref (split->context);
      if (split->other_context)
        other = gst_object_ref (split->other_context);
      g_rec_mutex_unlock (&split->context_lock);

      ret = gst_gl_handle_context_query ((GstElement *) split, query,
          display, local, other);

      gst_clear_object (&display);
      gst_clear_object (&other);
      gst_clear_object (&local);
      if (ret)
        return TRUE;

      return gst_pad_query_default (pad, parent, query);
    }
      /* FIXME: Handle caps query */
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static gboolean
stereosplit_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return gst_pad_event_default (pad, parent, event);
}

static gboolean
stereosplit_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstGLStereoSplit *split = GST_GL_STEREOSPLIT (parent);

  GST_DEBUG_OBJECT (split, "sink query %s",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      GstGLDisplay *display = NULL;
      GstGLContext *other = NULL, *local = NULL;
      gboolean ret;

      g_rec_mutex_lock (&split->context_lock);
      if (split->display)
        display = gst_object_ref (split->display);
      if (split->context)
        local = gst_object_ref (split->context);
      if (split->other_context)
        other = gst_object_ref (split->other_context);
      g_rec_mutex_unlock (&split->context_lock);

      ret = gst_gl_handle_context_query ((GstElement *) split, query,
          display, local, other);

      gst_clear_object (&display);
      gst_clear_object (&other);
      gst_clear_object (&local);
      if (ret)
        return TRUE;

      return gst_pad_query_default (pad, parent, query);
    }
    case GST_QUERY_ALLOCATION:
    {
      return stereosplit_propose_allocation (split, query);
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *possible, *caps;
      gboolean allowed;

      gst_query_parse_accept_caps (query, &caps);

      if (!(possible = gst_pad_query_caps (split->sink_pad, caps)))
        return FALSE;

      allowed = gst_caps_is_subset (caps, possible);
      gst_caps_unref (possible);

      gst_query_set_accept_caps_result (query, allowed);
      return allowed;
    }
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *left, *right, *combined, *ret, *templ_caps;
      gboolean result;

      gst_query_parse_caps (query, &filter);

      /* Calculate what downstream can collectively support */
      if (!(left = gst_pad_peer_query_caps (split->left_pad, NULL)))
        return FALSE;
      if (!(right = gst_pad_peer_query_caps (split->right_pad, NULL)))
        return FALSE;

      /* Strip out multiview mode and flags that might break the
       * intersection, since we can convert.
       * We could keep downstream preferred flip/flopping and list
       * separated as preferred in the future which might
       * theoretically allow us an easier conversion, but it's not essential
       */
      left = strip_mview_fields (left, GST_VIDEO_MULTIVIEW_FLAGS_NONE);
      right = strip_mview_fields (right, GST_VIDEO_MULTIVIEW_FLAGS_NONE);

      combined = gst_caps_intersect (left, right);
      gst_caps_unref (left);
      gst_caps_unref (right);

      /* Intersect peer caps with our template formats */
      templ_caps = gst_pad_get_pad_template_caps (split->left_pad);
      ret =
          gst_caps_intersect_full (combined, templ_caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (templ_caps);

      gst_caps_unref (combined);
      combined = ret;

      if (!combined || gst_caps_is_empty (combined)) {
        gst_caps_unref (combined);
        return FALSE;
      }

      /* Convert from the src pad caps to input formats we support */
      ret = stereosplit_transform_caps (split, GST_PAD_SRC, combined, filter);
      gst_caps_unref (combined);
      combined = ret;

      /* Intersect with the sink pad template then */
      templ_caps = gst_pad_get_pad_template_caps (split->sink_pad);
      ret =
          gst_caps_intersect_full (combined, templ_caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (templ_caps);
      gst_caps_unref (combined);

      GST_LOG_OBJECT (split, "Returning sink pad caps %" GST_PTR_FORMAT, ret);

      gst_query_set_caps_result (query, ret);
      result = !gst_caps_is_empty (ret);
      gst_caps_unref (ret);
      return result;
    }
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static gboolean
stereosplit_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstGLStereoSplit *split = GST_GL_STEREOSPLIT (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      return stereosplit_set_output_caps (split, caps);
    }
    default:
      return gst_pad_event_default (pad, parent, event);
  }
}
