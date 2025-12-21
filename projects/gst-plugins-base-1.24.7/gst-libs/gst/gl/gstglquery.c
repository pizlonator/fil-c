/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstglquery
 * @short_description: OpenGL query abstraction
 * @title: GstGLQuery
 *
 * A #GstGLQuery represents and holds an OpenGL query object.  Various types of
 * queries can be run or counters retrieved.
 *
 * Since: 1.10
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstglquery.h"

#include "gstglcontext.h"
#include "gstglfuncs.h"

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

#ifndef GL_TIMESTAMP
#define GL_TIMESTAMP 0x8E28
#endif

#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif

#define GST_CAT_DEFAULT gst_gl_query_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "glquery", 0, "glquery element");
    g_once_init_leave (&_init, 1);
  }
}

static const gchar *
_query_type_to_string (guint query_type)
{
  switch (query_type) {
    case GST_GL_QUERY_TIME_ELAPSED:
    case GL_TIME_ELAPSED:
      return "time elapsed";
    case GL_TIMESTAMP:
    case GST_GL_QUERY_TIMESTAMP:
      return "timestamp";
    default:
      return "unknown";
  }
}

static guint
_gst_gl_query_type_to_gl (GstGLQueryType query_type)
{
  if (query_type == GST_GL_QUERY_TIME_ELAPSED)
    return GL_TIME_ELAPSED;
  if (query_type == GST_GL_QUERY_TIMESTAMP)
    return GL_TIMESTAMP;

  return 0;
}

static gboolean
_query_type_supports_counter (guint gl_query_type)
{
  return gl_query_type == GL_TIMESTAMP;
}

static gboolean
_query_type_supports_begin_end (guint gl_query_type)
{
  return gl_query_type == GL_TIME_ELAPSED;
}

static gboolean
_context_supports_query_type (GstGLContext * context, guint gl_query_type)
{
  return gl_query_type != 0 && context->gl_vtable->GenQueries != NULL;
}

static gchar *
_log_time (gpointer user_data)
{
  GstGLQuery *query = user_data;
  guint64 result = 0;

  result = gst_gl_query_result (query);

  return gst_info_strdup_printf ("%" GST_TIME_FORMAT, GST_TIME_ARGS (result));
}

/**
 * gst_gl_query_init:
 * @query: a #GstGLQuery
 * @context: a #GstGLContext
 * @query_type: the #GstGLQueryType
 *
 * Since: 1.10
 */
void
gst_gl_query_init (GstGLQuery * query, GstGLContext * context,
    GstGLQueryType query_type)
{
  const GstGLFuncs *gl;
  GLenum gl_query_type;

  g_return_if_fail (query != NULL);
  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  gl = context->gl_vtable;
  gl_query_type = _gst_gl_query_type_to_gl (query_type);
  g_return_if_fail (gl_query_type != GL_NONE);

  memset (query, 0, sizeof (*query));

  _init_debug ();

  query->query_type = gl_query_type;
  query->context = gst_object_ref (context);
  query->supported = _context_supports_query_type (context, query->query_type);

  if (query->supported)
    gl->GenQueries (1, &query->query_id);

  gst_gl_async_debug_init (&query->debug);
  query->debug.callback = _log_time;
  query->debug.user_data = query;
}

/**
 * gst_gl_query_unset:
 * @query: a #GstGLQuery
 *
 * Free any dynamically allocated resources
 *
 * Since: 1.10
 */
void
gst_gl_query_unset (GstGLQuery * query)
{
  const GstGLFuncs *gl;

  g_return_if_fail (query != NULL);
  if (query->start_called)
    g_critical ("Unsetting a running query. This may not be what you wanted."
        "Be sure to pair calls to gst_gl_query_start() and gst_gl_query_end()");

  GST_TRACE ("%p unsetting query %u", query, query->query_id);

  gl = query->context->gl_vtable;

  /* unset the debug object as it may callback to print the last message */
  gst_gl_async_debug_unset (&query->debug);

  if (query->query_id)
    gl->DeleteQueries (1, &query->query_id);

  gst_object_unref (query->context);
}

/**
 * gst_gl_query_new: (skip)
 * @context: a #GstGLContext
 * @query_type: the #GstGLQueryType to create
 *
 * Free with gst_gl_query_free()
 *
 * Returns: a new #GstGLQuery
 *
 * Since: 1.10
 */
GstGLQuery *
gst_gl_query_new (GstGLContext * context, GstGLQueryType query_type)
{
  GstGLQuery *query = g_new0 (GstGLQuery, 1);

  gst_gl_query_init (query, context, query_type);

  return query;
}

/**
 * gst_gl_query_free:
 * @query: a #GstGLQuery
 *
 * Frees a #GstGLQuery
 *
 * Since: 1.10
 */
void
gst_gl_query_free (GstGLQuery * query)
{
  g_return_if_fail (query != NULL);

  gst_gl_query_unset (query);
  g_free (query);
}

/**
 * gst_gl_query_start:
 * @query: a #GstGLQuery
 *
 * Start counting the query
 *
 * Since: 1.10
 */
void
gst_gl_query_start (GstGLQuery * query)
{
  const GstGLFuncs *gl;

  g_return_if_fail (query != NULL);
  g_return_if_fail (_query_type_supports_begin_end (query->query_type));
  g_return_if_fail (query->start_called == FALSE);

  query->start_called = TRUE;

  if (!query->supported)
    return;

  gst_gl_async_debug_output_log_msg (&query->debug);

  GST_TRACE ("%p start query type \'%s\' id %u", query,
      _query_type_to_string (query->query_type), query->query_id);

  gl = query->context->gl_vtable;
  gl->BeginQuery (query->query_type, query->query_id);
}

/**
 * gst_gl_query_end:
 * @query: a #GstGLQuery
 *
 * End counting the query
 *
 * Since: 1.10
 */
void
gst_gl_query_end (GstGLQuery * query)
{
  const GstGLFuncs *gl;

  g_return_if_fail (query != NULL);
  g_return_if_fail (_query_type_supports_begin_end (query->query_type));
  g_return_if_fail (query->start_called);

  query->start_called = FALSE;

  if (!query->supported)
    return;

  GST_TRACE ("%p end query type \'%s\' id %u", query,
      _query_type_to_string (query->query_type), query->query_id);

  gl = query->context->gl_vtable;

  gl->EndQuery (query->query_type);
}

/**
 * gst_gl_query_counter:
 * @query: a #GstGLQuery
 *
 * Record the result of a counter
 *
 * Since: 1.10
 */
void
gst_gl_query_counter (GstGLQuery * query)
{
  const GstGLFuncs *gl;

  g_return_if_fail (query != NULL);
  g_return_if_fail (_query_type_supports_counter (query->query_type));

  if (!query->supported)
    return;

  GST_TRACE ("%p query counter type \'%s\' id %u", query,
      _query_type_to_string (query->query_type), query->query_id);

  gst_gl_async_debug_output_log_msg (&query->debug);

  gl = query->context->gl_vtable;
  gl->QueryCounter (query->query_id, query->query_type);
}

/**
 * gst_gl_query_result:
 * @query: a #GstGLQuery
 *
 * Returns: the result of the query
 *
 * Since: 1.10
 */
guint64
gst_gl_query_result (GstGLQuery * query)
{
  const GstGLFuncs *gl;
  guint64 ret;

  g_return_val_if_fail (query != NULL, 0);
  g_return_val_if_fail (!query->start_called, 0);

  if (!query->supported)
    return 0;

  gl = query->context->gl_vtable;
  if (gl->GetQueryObjectui64v) {
    GLuint64 tmp = 0;
    gl->GetQueryObjectui64v (query->query_id, GL_QUERY_RESULT, &tmp);
    ret = tmp;
  } else {
    guint tmp = 0;
    gl->GetQueryObjectuiv (query->query_id, GL_QUERY_RESULT, &tmp);
    ret = tmp;
  }

  GST_TRACE ("%p get result %" G_GUINT64_FORMAT " type \'%s\' id %u", query,
      ret, _query_type_to_string (query->query_type), query->query_id);

  return ret;
}
