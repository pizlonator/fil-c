/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel Corporation
 * Copyright (c) 2018, Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGDECE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gstmsdkcontextutil.h"
#ifndef _WIN32
#include <gst/va/gstvadisplay.h>
#include <gst/va/gstvautils.h>
#else
#include <gst/d3d11/gstd3d11device.h>
#endif

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

#ifdef _WIN32
static gboolean
_pad_query (const GValue * item, GValue * value, gpointer user_data)
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
_run_query (GstElement * element, GstQuery * query, GstPadDirection direction)
{
  GstIterator *it;
  GstIteratorFoldFunction func = _pad_query;
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

static void
_context_query (GstElement * element, const gchar * context_type)
{
  GstQuery *query;
  GstContext *ctxt = NULL;

  /*  2a) Query downstream with GST_QUERY_CONTEXT for the context and
   *      check if downstream already has a context of the specific type
   *  2b) Query upstream as above.
   */
  query = gst_query_new_context (context_type);
  if (_run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "found context (%p) in downstream query", ctxt);
    gst_element_set_context (element, ctxt);
  } else if (_run_query (element, query, GST_PAD_SINK)) {
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
#endif

/* Find whether the other elements already have a msdk context. */
gboolean
gst_msdk_context_find (GstElement * element, GstMsdkContext ** context_ptr)
{
  _init_context_debug ();

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (context_ptr != NULL, FALSE);

  /* 1) Check if the element already has a context of the specific type. */
  if (*context_ptr) {
    GST_LOG_OBJECT (element, "already have a context %" GST_PTR_FORMAT,
        *context_ptr);
    return TRUE;
  }

  /* This may indirectly set *context_ptr, see function body */
#ifndef _WIN32
  gst_va_context_query (element, GST_MSDK_CONTEXT_TYPE_NAME);
#else
  _context_query (element, GST_MSDK_CONTEXT_TYPE_NAME);
#endif

  if (*context_ptr) {
    GST_LOG_OBJECT (element, "found a context %" GST_PTR_FORMAT, *context_ptr);
    return TRUE;
  }

  return *context_ptr != NULL;
}

gboolean
gst_msdk_context_get_context (GstContext * context,
    GstMsdkContext ** msdk_context)
{
  const GstStructure *structure;
  const gchar *type;

  _init_context_debug ();

  g_return_val_if_fail (GST_IS_CONTEXT (context), FALSE);

  type = gst_context_get_context_type (context);

  if (!g_strcmp0 (type, GST_MSDK_CONTEXT_TYPE_NAME)) {
    structure = gst_context_get_structure (context);
    return gst_structure_get (structure, GST_MSDK_CONTEXT_TYPE_NAME,
        GST_TYPE_MSDK_CONTEXT, msdk_context, NULL);
  }

  return FALSE;
}

static void
gst_msdk_context_propagate (GstElement * element, GstMsdkContext * msdk_context)
{
  GstContext *context;
  GstStructure *structure;
  GstMessage *msg;

  context = gst_context_new (GST_MSDK_CONTEXT_TYPE_NAME, FALSE);

  structure = gst_context_writable_structure (context);
  gst_structure_set (structure, GST_MSDK_CONTEXT_TYPE_NAME,
      GST_TYPE_MSDK_CONTEXT, msdk_context, NULL);

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
      "posting `have-context' message with MSDK context %" GST_PTR_FORMAT,
      msdk_context);

  msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
  if (!gst_element_post_message (element, msg))
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element, "No bus attached");
}

/* When we can not find a suitable context from others, we ensure to create
   a new context. */
gboolean
gst_msdk_ensure_new_context (GstElement * element, gboolean hardware,
    GstMsdkContextJobType job, GstMsdkContext ** context_ptr)
{
  GstMsdkContext *msdk_context;
  gboolean propagate_display = FALSE;
  gboolean ret = FALSE;

  g_return_val_if_fail (element, FALSE);
  g_return_val_if_fail (context_ptr, FALSE);

  _init_context_debug ();

  /* 1) Already have. */
  if (g_atomic_pointer_get (context_ptr))
    return TRUE;

#ifndef _WIN32
  /* 2) Query the neighbour the VA display. If already a valid VA display,
     using it by gst_msdk_context_from_external_va_display() in set_context(). */
  gst_va_context_query (element, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR);
  msdk_context = g_atomic_pointer_get (context_ptr);
  if (msdk_context) {
    gst_object_ref (msdk_context);
    propagate_display = FALSE;
    ret = TRUE;
    goto done;
  }
#else
  /* 2) Query the neighbour the D3D11 device. If already a valid D3D11 device,
     using it by gst_msdk_context_from_external_d3d11_device() in set_context(). */
  _context_query (element, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE);
  msdk_context = g_atomic_pointer_get (context_ptr);
  if (msdk_context) {
    gst_object_ref (msdk_context);
    propagate_display = FALSE;
    ret = TRUE;
    goto done;
  }
#endif

  /* 3) Create a MSDK context from scratch. Currently we use environment variable
     to enable user to choose GPU device in multi-GPU environment. This variable
     is only valid when there's no context returned by upstream or downstream.
     Otherwise it will use the device that created by upstream or downstream. */
  msdk_context = gst_msdk_context_new_with_job_type (hardware, job);
  if (!msdk_context) {
    GST_ERROR_OBJECT (element, "Context creation failed");
    return FALSE;
  }
  propagate_display = TRUE;
  ret = TRUE;

  GST_INFO_OBJECT (element, "New MSDK Context %p", msdk_context);

  gst_object_replace ((GstObject **) context_ptr, (GstObject *) msdk_context);

done:
  if (propagate_display) {
#ifndef _WIN32
    GstVaDisplay *display =
        (GstVaDisplay *) gst_msdk_context_get_va_display (msdk_context);
    gst_va_element_propagate_display_context (element, display);
    gst_clear_object (&display);
#endif
  }

  gst_msdk_context_propagate (element, msdk_context);
  gst_object_unref (msdk_context);

  return ret;
}

#ifndef _WIN32
gboolean
gst_msdk_context_from_external_va_display (GstContext * context,
    gboolean hardware, GstMsdkContextJobType job_type,
    GstMsdkContext ** msdk_context)
{
  GstObject *va_display = NULL;
  const gchar *type;
  const GstStructure *s;
  GstMsdkContext *ctx = NULL;

  _init_context_debug ();

  type = gst_context_get_context_type (context);
  if (g_strcmp0 (type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR))
    return FALSE;

  s = gst_context_get_structure (context);
  if (gst_structure_get (s, "gst-display", GST_TYPE_OBJECT, &va_display, NULL)) {
    if (GST_IS_VA_DISPLAY (va_display)) {
      /* TODO: Need to check whether the display is the DEV we want. */
      ctx =
          gst_msdk_context_new_with_va_display (va_display, hardware, job_type);
      if (ctx)
        *msdk_context = ctx;
    }

    /* let's try other fields */
    gst_clear_object (&va_display);
  }

  if (ctx)
    return TRUE;

  return FALSE;
}
#else
gboolean
gst_msdk_context_from_external_d3d11_device (GstContext * context,
    gboolean hardware, GstMsdkContextJobType job_type,
    GstMsdkContext ** msdk_context)
{
  GstD3D11Device *d3d11_device = NULL;
  const gchar *type;
  const GstStructure *s;
  GstMsdkContext *ctx = NULL;
  guint vendor_id = 0;

  _init_context_debug ();

  type = gst_context_get_context_type (context);
  if (g_strcmp0 (type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE))
    return FALSE;

  s = gst_context_get_structure (context);
  if (gst_structure_get (s, "device", GST_TYPE_D3D11_DEVICE, &d3d11_device,
          NULL)) {
    g_object_get (d3d11_device, "vendor-id", &vendor_id, NULL);
    if (vendor_id != 0x8086) {
      GST_ERROR ("Not an Intel device");
      gst_clear_object (&d3d11_device);
      return FALSE;
    }
    ctx =
        gst_msdk_context_new_with_d3d11_device (d3d11_device, hardware,
        job_type);
    if (ctx)
      *msdk_context = ctx;

    gst_clear_object (&d3d11_device);
  }

  if (ctx)
    return TRUE;

  return FALSE;
}
#endif

gboolean
gst_msdk_handle_context_query (GstElement * element, GstQuery * query,
    GstMsdkContext * msdk_context)
{
  const gchar *context_type;
  GstContext *ctxt, *old_ctxt;
  gboolean ret = FALSE;

  _init_context_debug ();

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (GST_IS_QUERY (query), FALSE);
  g_return_val_if_fail (!msdk_context
      || GST_IS_MSDK_CONTEXT (msdk_context), FALSE);

  GST_CAT_LOG_OBJECT (GST_CAT_CONTEXT, element,
      "handle context query %" GST_PTR_FORMAT, query);

  if (!msdk_context)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);

  gst_query_parse_context (query, &old_ctxt);
  if (old_ctxt)
    ctxt = gst_context_copy (old_ctxt);
  else
    ctxt = gst_context_new (context_type, TRUE);

#ifndef _WIN32
  if (g_strcmp0 (context_type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) == 0) {
    GstStructure *s;
    GstObject *display = gst_msdk_context_get_va_display (msdk_context);

    if (display) {
      GST_CAT_LOG (GST_CAT_CONTEXT,
          "setting GstVaDisplay (%" GST_PTR_FORMAT ") on context (%"
          GST_PTR_FORMAT ")", display, ctxt);

      s = gst_context_writable_structure (ctxt);
      gst_structure_set (s, "gst-display", GST_TYPE_OBJECT, display, NULL);
      /* Structure hold one ref */
      gst_object_unref (display);
      ret = TRUE;
    }
  } else
#else
  if (g_strcmp0 (context_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) == 0) {
    GstStructure *s;
    GstD3D11Device *device = gst_msdk_context_get_d3d11_device (msdk_context);

    if (device) {
      GST_CAT_LOG (GST_CAT_CONTEXT,
          "setting GstD3D11Device (%" GST_PTR_FORMAT ") on context (%"
          GST_PTR_FORMAT ")", device, ctxt);

      s = gst_context_writable_structure (ctxt);
      gst_structure_set (s, "device", GST_TYPE_D3D11_DEVICE, device, NULL);
      gst_object_unref (device);
      ret = TRUE;
    }
  } else
#endif
  if (g_strcmp0 (context_type, GST_MSDK_CONTEXT_TYPE_NAME) == 0) {
    GstStructure *s;

    s = gst_context_writable_structure (ctxt);
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstMsdkContext (%" GST_PTR_FORMAT ") on context (%"
        GST_PTR_FORMAT ")", msdk_context, ctxt);
    gst_structure_set (s, GST_MSDK_CONTEXT_TYPE_NAME, GST_TYPE_MSDK_CONTEXT,
        msdk_context, NULL);
    ret = TRUE;
  }

  if (ret)
    gst_query_set_context (query, ctxt);

  gst_context_unref (ctxt);
  return ret;
}
