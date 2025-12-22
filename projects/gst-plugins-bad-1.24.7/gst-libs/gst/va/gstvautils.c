/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * SECTION:gstvautils
 * @title: GstVaUtils
 * @short_description: Utility functions for context handling
 * @sources:
 * - gstvautils.h
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvautils.h"

#ifdef G_OS_WIN32
#include "gstvadisplay_win32.h"
#else
#include "gstvadisplay_drm.h"
#endif
#include "gstvadisplay_wrapped.h"

GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

static void
_init_context_debug (void)
{
#ifndef GST_DISABLE_GST_DEBUG
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&_init, 1);
  }
#endif
}

static gboolean
gst_va_display_found (GstElement * element, GstVaDisplay * display)
{
  if (display) {
    GST_CAT_LOG_OBJECT (GST_CAT_CONTEXT, element, "already have a display (%p)",
        display);
    return TRUE;
  }

  return FALSE;
}

static gboolean
pad_query (const GValue * item, GValue * value, gpointer user_data)
{
  GstPad *pad = g_value_get_object (item);
  GstQuery *query = user_data;
  gboolean res;

  res = gst_pad_peer_query (pad, query);

  if (res) {
    g_value_set_boolean (value, TRUE);
    return FALSE;
  }

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, pad, "pad peer query failed");
  return TRUE;
}

static gboolean
_gst_va_run_query (GstElement * element, GstQuery * query,
    GstPadDirection direction)
{
  GstIterator *it;
  GstIteratorFoldFunction func = pad_query;
  GValue res = G_VALUE_INIT;

  g_value_init (&res, G_TYPE_BOOLEAN);
  g_value_set_boolean (&res, FALSE);

  if (direction == GST_PAD_SRC)
    it = gst_element_iterate_src_pads (element);
  else
    it = gst_element_iterate_sink_pads (element);

  while (gst_iterator_fold (it, func, &res, query) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);

  gst_iterator_free (it);

  return g_value_get_boolean (&res);
}

/**
 * gst_va_context_query:
 * @element: a #GstElement
 * @context_type: the #gchar string specify the context type name
 *
 * Query the specified context type name.
 *
 * Since: 1.22
 **/
void
gst_va_context_query (GstElement * element, const gchar * context_type)
{
  GstQuery *query;
  GstContext *ctxt = NULL;

  _init_context_debug ();

  /*  2a) Query downstream with GST_QUERY_CONTEXT for the context and
   *      check if downstream already has a context of the specific type
   *  2b) Query upstream as above.
   */
  query = gst_query_new_context (context_type);
  if (_gst_va_run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "found context (%p) in downstream query", ctxt);
    gst_element_set_context (element, ctxt);
  } else if (_gst_va_run_query (element, query, GST_PAD_SINK)) {
    gst_query_parse_context (query, &ctxt);
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "found context (%p) in upstream query", ctxt);
    gst_element_set_context (element, ctxt);
  } else {
    /* 3) Post a GST_MESSAGE_NEED_CONTEXT message on the bus with
     *    the required context type and afterwards check if a
     *    usable context was set now as in 1). The message could
     *    be handled by the parent bins of the element and the
     *    application.
     */
    GstMessage *msg;

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting need context message");
    msg = gst_message_new_need_context (GST_OBJECT_CAST (element),
        context_type);
    gst_element_post_message (element, msg);
  }

  /*
   * Whomever responds to the need-context message performs a
   * GstElement::set_context() with the required context in which the element
   * is required to update the display_ptr or call gst_va_handle_set_context().
   */

  gst_query_unref (query);
}

/**
 * gst_va_element_propagate_display_context:
 * @element: a #GstElement
 * @display: the #GstVaDisplay to propagate
 *
 * Propagate @display by posting it as #GstContext in the pipeline's bus.
 *
 * Since: 1.22
 **/
void
gst_va_element_propagate_display_context (GstElement * element,
    GstVaDisplay * display)
{
  GstContext *ctxt;
  GstMessage *msg;

  _init_context_debug ();

  if (!display) {
    GST_ERROR_OBJECT (element, "Could not get VA display connection");
    return;
  }

  /*  4) Create a context by itself and post a GST_MESSAGE_HAVE_CONTEXT
   *     message.
   */
  ctxt = gst_context_new (GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR, TRUE);
  gst_context_set_va_display (ctxt, display);

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
      "post have context (%p) message with display (%p)", ctxt, display);
  msg = gst_message_new_have_context (GST_OBJECT_CAST (element), ctxt);
  gst_element_post_message (element, msg);
}

/**
 * gst_va_ensure_element_data:
 * @element: a #GstElement
 * @render_device_path: the #gchar string of render device path
 * @display_ptr: (out) (transfer full): The #GstVaDisplay to ensure
 *
 * Called by the va element to ensure a valid #GstVaDisplay.
 *
 * Returns: whether a #GstVaDisplay exists in @display_ptr
 *
 * Since: 1.22
 **/
gboolean
gst_va_ensure_element_data (gpointer element, const gchar * render_device_path,
    GstVaDisplay ** display_ptr)
{
  GstVaDisplay *display;

  _init_context_debug ();

  g_return_val_if_fail (element, FALSE);
  g_return_val_if_fail (render_device_path, FALSE);
  g_return_val_if_fail (display_ptr, FALSE);

  /*  1) Check if the element already has a context of the specific
   *     type.
   */
  if (gst_va_display_found (element, g_atomic_pointer_get (display_ptr)))
    goto done;

  gst_va_context_query (element, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR);

  /* Neighbour found and it updated the display. */
  if (gst_va_display_found (element, g_atomic_pointer_get (display_ptr)))
    goto done;

#ifdef G_OS_WIN32
  display = gst_va_display_win32_new (render_device_path);
#else
  /* If no neighbor, or application not interested, use drm. */
  display = gst_va_display_drm_new_from_path (render_device_path);
#endif

  gst_object_replace ((GstObject **) display_ptr, (GstObject *) display);

  gst_va_element_propagate_display_context (element, display);

  gst_clear_object (&display);

done:
  return g_atomic_pointer_get (display_ptr) != NULL;
}

/**
 * gst_va_handle_set_context:
 * @element: a #GstElement
 * @context: a #GstContext may contain the display
 * @render_device_path: the #gchar string of render device path
 * @display_ptr: (out) (transfer full): The #GstVaDisplay to set
 *
 * Called by elements in their #GstElementClass::set_context vmethod.
 * It gets a valid #GstVaDisplay if @context has it.
 *
 * Returns: whether the @display_ptr could be successfully set to a
 * valid #GstVaDisplay in the @context
 *
 * Since: 1.22
 **/
gboolean
gst_va_handle_set_context (GstElement * element, GstContext * context,
    const gchar * render_device_path, GstVaDisplay ** display_ptr)
{
  GstVaDisplay *display_replacement = NULL;
  const gchar *context_type, *type_name;

  _init_context_debug ();

  g_return_val_if_fail (display_ptr, FALSE);

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);

  if (g_strcmp0 (context_type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) == 0) {
    type_name = G_OBJECT_TYPE_NAME (element);
    if (!gst_context_get_va_display (context, type_name, render_device_path,
            &display_replacement)) {
      GST_CAT_WARNING_OBJECT (GST_CAT_CONTEXT, element,
          "Failed to get display from context");
      return FALSE;
    }
  }

  if (display_replacement) {
    gst_object_replace ((GstObject **) display_ptr,
        (GstObject *) display_replacement);
    gst_object_unref (display_replacement);
  }

  return TRUE;
}

/**
 * gst_va_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery to query the context
 * @display: a #GstVaDisplay to answer the query
 *
 * Used by elements when processing their pad's queries, propagating
 * element's #GstVaDisplay if the processed query requests it.
 *
 * Returns: whether we can handle the context query successfully
 *
 * Since: 1.22
 **/
gboolean
gst_va_handle_context_query (GstElement * element, GstQuery * query,
    GstVaDisplay * display)
{
  const gchar *context_type;
  GstContext *ctxt, *old_ctxt;

  _init_context_debug ();

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (GST_IS_QUERY (query), FALSE);
  g_return_val_if_fail (!display || GST_IS_VA_DISPLAY (display), FALSE);

  GST_CAT_LOG_OBJECT (GST_CAT_CONTEXT, element,
      "handle context query %" GST_PTR_FORMAT, query);
  gst_query_parse_context_type (query, &context_type);

  if (!display
      || g_strcmp0 (context_type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) != 0)
    return FALSE;

  gst_query_parse_context (query, &old_ctxt);

  if (old_ctxt)
    ctxt = gst_context_copy (old_ctxt);
  else
    ctxt = gst_context_new (GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR, TRUE);

  gst_context_set_va_display (ctxt, display);
  gst_query_set_context (query, ctxt);
  gst_context_unref (ctxt);
  GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT, element,
      "successfully %" GST_PTR_FORMAT " on %" GST_PTR_FORMAT, display, query);

  return TRUE;
}

/**
 * gst_context_get_va_display:
 * @context: a #GstContext may contain the display
 * @type_name: a #gchar string of the element type
 * @render_device_path: the #gchar string of render device path
 * @display_ptr: (out) (transfer full): the #GstVaDisplay we get
 *
 * Returns: whether we find a valid @display in the @context
 *
 * Since: 1.22
 **/
gboolean
gst_context_get_va_display (GstContext * context, const gchar * type_name,
    const gchar * render_device_path, GstVaDisplay ** display_ptr)
{
  const GstStructure *s;
  GstVaDisplay *display = NULL;
  gpointer dpy;
  gboolean is_devnode;

  _init_context_debug ();

  g_return_val_if_fail (display_ptr, FALSE);
  g_return_val_if_fail (context, FALSE);

  is_devnode = (g_strstr_len (type_name, -1, "renderD") != NULL);

  s = gst_context_get_structure (context);
  if (gst_structure_get (s, "gst-display", GST_TYPE_OBJECT, &display, NULL)) {
    gboolean ret;
    gchar *device_path = NULL;
#ifdef G_OS_WIN32
    if (GST_IS_VA_DISPLAY_WIN32 (display)) {
#else
    if (GST_IS_VA_DISPLAY_DRM (display)) {
#endif
      g_object_get (display, "path", &device_path, NULL);
      ret = (g_strcmp0 (device_path, render_device_path) == 0);
      g_free (device_path);
      if (ret)
        goto accept;
    } else if (GST_IS_VA_DISPLAY (display) && !is_devnode) {
      goto accept;
    }

    /* let's try other fields */
    gst_clear_object (&display);
  }

  /* if element is render device node specific, it doesn't accept
   * VADisplay from users */
  if (!is_devnode
      && gst_structure_get (s, "va-display", G_TYPE_POINTER, &dpy, NULL)) {
    if ((display = gst_va_display_wrapped_new (dpy)))
      goto accept;
  }

  GST_CAT_DEBUG (GST_CAT_CONTEXT, "No valid GstVaDisplay from context (%p)",
      context);
  return FALSE;

accept:
  {
    *display_ptr = display;

    GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVaDisplay (%p) from context (%p)",
        *display_ptr, context);
    return TRUE;
  }
}

/**
 * gst_context_set_va_display:
 * @context: a #GstContext
 * @display: the #GstVaDisplay we want to set
 *
 * Set the @display in the @context
 *
 * Since: 1.22
 */
void
gst_context_set_va_display (GstContext * context, GstVaDisplay * display)
{
  GstStructure *s;

  _init_context_debug ();

  g_return_if_fail (context != NULL);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, "gst-display", GST_TYPE_OBJECT, display, NULL);

  if (display) {
    GObjectClass *klass = G_OBJECT_GET_CLASS (display);
    gchar *vendor_desc = NULL;
    gchar *path = NULL;

    g_object_get (display, "description", &vendor_desc, NULL);
    if (g_object_class_find_property (klass, "path"))
      g_object_get (display, "path", &path, NULL);

    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVaDisplay (%" GST_PTR_FORMAT ") on context (%"
        GST_PTR_FORMAT "), description: \"%s\", path: %s", display, context,
        GST_STR_NULL (vendor_desc), GST_STR_NULL (path));

    if (vendor_desc) {
      gst_structure_set (s, "description", G_TYPE_STRING, vendor_desc, NULL);
      g_free (vendor_desc);
    }

    if (path) {
      gst_structure_set (s, "path", G_TYPE_STRING, path, NULL);
      g_free (path);
    }
  }
}
