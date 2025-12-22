/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d11utils.h"
#include "gstd3d11device.h"
#include "gstd3d11-private.h"

#include <windows.h>
#include <versionhelpers.h>
#include <mutex>
#include <atomic>

/**
 * SECTION:gstd3d11utils
 * @title: GstD3D11Utils
 * @short_description: Direct3D11 specific utility methods
 *
 * Since: 1.22
 */

/* *INDENT-OFF* */
static std::recursive_mutex _context_lock;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static GstDebugCategory *cat = nullptr;

  GST_D3D11_CALL_ONCE_BEGIN {
    cat = _gst_debug_category_new ("d3d11utils", 0, "d3d11 utility functions");
  } GST_D3D11_CALL_ONCE_END;

  return cat;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static void
_init_context_debug (void)
{
  GST_D3D11_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
  } GST_D3D11_CALL_ONCE_END;
}

/**
 * gst_d3d11_handle_set_context:
 * @element: a #GstElement
 * @context: a #GstContext
 * @adapter_index: a DXGI adapter index
 * @device: (inout) (transfer full): location of a #GstD3D11Device
 *
 * Helper function for implementing #GstElementClass.set_context() in
 * D3D11 capable elements.
 *
 * Retrieve's the #GstD3D11Device in @context and places the result in @device.
 * @device is accepted if @adapter_index is equal to -1 (accept any device)
 * or equal to that of @device
 *
 * Returns: whether the @device could be set successfully
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_handle_set_context (GstElement * element, GstContext * context,
    gint adapter_index, GstD3D11Device ** device)
{
  const gchar *context_type;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  _init_context_debug ();

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);
  if (g_strcmp0 (context_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) == 0) {
    const GstStructure *str;
    GstD3D11Device *other_device = NULL;
    guint other_adapter_index = 0;

    /* If we had device already, will not replace it */
    if (*device)
      return TRUE;

    str = gst_context_get_structure (context);

    if (gst_structure_get (str, "device", GST_TYPE_D3D11_DEVICE,
            &other_device, "adapter", G_TYPE_UINT, &other_adapter_index,
            NULL)) {
      if (adapter_index == -1 || (guint) adapter_index == other_adapter_index) {
        GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT,
            element, "Found D3D11 device context");
        *device = other_device;

        return TRUE;
      }

      gst_object_unref (other_device);
    }
  }

  return FALSE;
}

/**
 * gst_d3d11_handle_set_context_for_adapter_luid:
 * @element: a #GstElement
 * @context: a #GstContext
 * @adapter_luid: an int64 representation of DXGI adapter LUID
 * @device: (inout) (transfer full): location of a #GstD3D11Device
 *
 * Helper function for implementing #GstElementClass.set_context() in
 * D3D11 capable elements.
 *
 * Retrieve's the #GstD3D11Device in @context and places the result in @device.
 * @device is accepted only when @adapter_index is equal to that of @device
 *
 * Returns: whether the @device could be set successfully
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_handle_set_context_for_adapter_luid (GstElement * element,
    GstContext * context, gint64 adapter_luid, GstD3D11Device ** device)
{
  const gchar *context_type;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  _init_context_debug ();

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);
  if (g_strcmp0 (context_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) == 0) {
    const GstStructure *str;
    GstD3D11Device *other_device = NULL;
    gint64 other_adapter_luid = 0;

    /* If we had device already, will not replace it */
    if (*device)
      return TRUE;

    str = gst_context_get_structure (context);

    if (gst_structure_get (str, "device", GST_TYPE_D3D11_DEVICE,
            &other_device, "adapter-luid", G_TYPE_INT64,
            &other_adapter_luid, NULL)) {
      if (adapter_luid == other_adapter_luid) {
        GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT,
            element, "Found D3D11 device context");
        *device = other_device;

        return TRUE;
      }

      gst_object_unref (other_device);
    }
  }

  return FALSE;
}

static void
context_set_d3d11_device (GstContext * context, GstD3D11Device * device)
{
  GstStructure *s;
  guint adapter = 0;
  guint device_id = 0;
  guint vendor_id = 0;
  gboolean hardware = FALSE;
  gchar *desc = NULL;
  gint64 adapter_luid = 0;

  g_return_if_fail (context != NULL);

  g_object_get (G_OBJECT (device), "adapter", &adapter, "device-id", &device_id,
      "vendor-id", &vendor_id, "hardware", &hardware, "description", &desc,
      "adapter-luid", &adapter_luid, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT,
      "setting GstD3D11Device(%" GST_PTR_FORMAT
      ") with adapter %d on context(%" GST_PTR_FORMAT ")",
      device, adapter, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, "device", GST_TYPE_D3D11_DEVICE, device,
      "adapter", G_TYPE_UINT, adapter,
      "adapter-luid", G_TYPE_INT64, adapter_luid,
      "device-id", G_TYPE_UINT, device_id,
      "vendor-id", G_TYPE_UINT, vendor_id,
      "hardware", G_TYPE_BOOLEAN, hardware,
      "description", G_TYPE_STRING, GST_STR_NULL (desc), NULL);
  g_free (desc);
}

/**
 * gst_d3d11_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type %GST_QUERY_CONTEXT
 * @device: (transfer none) (nullable): a #GstD3D11Device
 *
 * Returns: Whether the @query was successfully responded to from the passed
 *          @device.
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_handle_context_query (GstElement * element, GstQuery * query,
    GstD3D11Device * device)
{
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (GST_IS_QUERY (query), FALSE);

  _init_context_debug ();

  GST_LOG_OBJECT (element, "handle context query %" GST_PTR_FORMAT, query);

  if (!device)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);
  if (g_strcmp0 (context_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
    return FALSE;

  gst_query_parse_context (query, &old_context);
  if (old_context)
    context = gst_context_copy (old_context);
  else
    context = gst_context_new (GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);

  context_set_d3d11_device (context, device);
  gst_query_set_context (query, context);
  gst_context_unref (context);

  GST_DEBUG_OBJECT (element, "successfully set %" GST_PTR_FORMAT
      " on %" GST_PTR_FORMAT, device, query);

  return TRUE;
}

static gboolean
pad_query (const GValue * item, GValue * value, gpointer user_data)
{
  GstPad *pad = (GstPad *) g_value_get_object (item);
  GstQuery *query = (GstQuery *) user_data;
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
run_query (GstElement * element, GstQuery * query, GstPadDirection direction)
{
  GstIterator *it;
  GstIteratorFoldFunction func = pad_query;
  GValue res = { 0 };

  g_value_init (&res, G_TYPE_BOOLEAN);
  g_value_set_boolean (&res, FALSE);

  /* Ask neighbor */
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
run_d3d11_context_query (GstElement * element, GstD3D11Device ** device)
{
  GstQuery *query;
  GstContext *ctxt = NULL;

  /* 1) Query downstream with GST_QUERY_CONTEXT for the context and
   *    check if downstream already has a context of the specific type
   */
  query = gst_query_new_context (GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE);
  if (run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%" GST_PTR_FORMAT ") in downstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  /* 2) although we found d3d11 device context above, the context might not be
   *    expected/wanted one by the element (e.g., belongs to the other GPU).
   *    Then try to find it from the other direction */
  if (*device == NULL && run_query (element, query, GST_PAD_SINK)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%" GST_PTR_FORMAT ") in upstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  if (*device == NULL) {
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
        GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE);
    gst_element_post_message (element, msg);
  }

  gst_query_unref (query);
}

/**
 * gst_d3d11_ensure_element_data:
 * @element: the #GstElement running the query
 * @adapter: preferred DXGI adapter index, pass adapter >=0 when
 *           the adapter explicitly required. Otherwise, set -1.
 * @device: (inout): the resulting #GstD3D11Device
 *
 * Perform the steps necessary for retrieving a #GstD3D11Device
 * from the surrounding elements or from the application using the #GstContext mechanism.
 *
 * If the contents of @device is not %NULL, then no #GstContext query is
 * necessary for #GstD3D11Device retrieval is performed.
 *
 * Returns: whether a #GstD3D11Device exists in @device
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_ensure_element_data (GstElement * element, gint adapter,
    GstD3D11Device ** device)
{
  guint target_adapter = 0;
  /* *INDENT-OFF* */
  std::lock_guard<std::recursive_mutex> lk (_context_lock);
  /* *INDENT-ON* */

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  _init_context_debug ();

  if (*device) {
    GST_LOG_OBJECT (element, "already have a device %" GST_PTR_FORMAT, *device);
    return TRUE;
  }

  run_d3d11_context_query (element, device);
  if (*device)
    return TRUE;

  if (adapter > 0)
    target_adapter = adapter;

  /* Needs D3D11_CREATE_DEVICE_BGRA_SUPPORT flag for Direct2D interop */
  *device = gst_d3d11_device_new (target_adapter,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT);

  if (*device == NULL) {
    GST_ERROR_OBJECT (element,
        "Couldn't create new device with adapter index %d", target_adapter);
    return FALSE;
  } else {
    GstContext *context;
    GstMessage *msg;

    /* Propagate new D3D11 device context */

    context = gst_context_new (GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
    context_set_d3d11_device (context, *device);

    gst_element_set_context (element, context);

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting have context (%p) message with D3D11 device context (%p)",
        context, *device);
    msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }

  return TRUE;
}

/**
 * gst_d3d11_ensure_element_data_for_adapter_luid:
 * @element: the #GstElement running the query
 * @adapter_luid: an int64 representation of DXGI adapter LUID
 * @device: (inout): the resulting #GstD3D11Device
 *
 * Perform the steps necessary for retrieving a #GstD3D11Device
 * from the surrounding elements or from the application using the #GstContext mechanism.
 *
 * If the contents of @device is not %NULL, then no #GstContext query is
 * necessary for #GstD3D11Device retrieval is performed.
 *
 * Returns: whether a #GstD3D11Device exists in @device
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_ensure_element_data_for_adapter_luid (GstElement * element,
    gint64 adapter_luid, GstD3D11Device ** device)
{
  /* *INDENT-OFF* */
  std::lock_guard<std::recursive_mutex> lk (_context_lock);
  /* *INDENT-ON* */

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  _init_context_debug ();

  if (*device) {
    GST_LOG_OBJECT (element, "already have a device %" GST_PTR_FORMAT, *device);
    return TRUE;
  }

  run_d3d11_context_query (element, device);
  if (*device)
    return TRUE;

  /* Needs D3D11_CREATE_DEVICE_BGRA_SUPPORT flag for Direct2D interop */
  *device = gst_d3d11_device_new_for_adapter_luid (adapter_luid,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT);

  if (*device == NULL) {
    GST_ERROR_OBJECT (element,
        "Couldn't create new device with adapter luid %" G_GINT64_FORMAT,
        adapter_luid);
    return FALSE;
  } else {
    GstContext *context;
    GstMessage *msg;

    /* Propagate new D3D11 device context */

    context = gst_context_new (GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
    context_set_d3d11_device (context, *device);

    gst_element_set_context (element, context);

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting have context (%p) message with D3D11 device context (%p)",
        context, *device);
    msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }

  return TRUE;
}

/**
 * gst_d3d11_context_new:
 * @device: (transfer none): a #GstD3D11Device
 *
 * Creates a new #GstContext object with @device
 *
 * Returns: a #GstContext object
 *
 * Since: 1.22
 */
GstContext *
gst_d3d11_context_new (GstD3D11Device * device)
{
  GstContext *context;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);

  context = gst_context_new (GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
  context_set_d3d11_device (context, device);

  return context;
}

/**
 * gst_d3d11_luid_to_int64:
 * @luid: A pointer to LUID struct
 *
 * Converts @luid to a 64-bit signed integer.
 * See also Int64FromLuid method defined in
 * windows.devices.display.core.interop.h Windows SDK header
 *
 * Since: 1.22
 */
gint64
gst_d3d11_luid_to_int64 (const LUID * luid)
{
  LARGE_INTEGER val;

  g_return_val_if_fail (luid != nullptr, 0);

  val.LowPart = luid->LowPart;
  val.HighPart = luid->HighPart;

  return val.QuadPart;
}

#ifndef GST_DISABLE_GST_DEBUG
static void
gst_d3d11_log_gpu_remove_reason (HRESULT hr, GstD3D11Device * device,
    GstDebugCategory * cat, const gchar * file, const gchar * function,
    gint line)
{
  gchar *error_text = g_win32_error_message ((guint) hr);

  gst_debug_log (cat, GST_LEVEL_ERROR, file, function, line,
      NULL, "DeviceRemovedReason: 0x%x, %s", (guint) hr,
      GST_STR_NULL (error_text));
  g_free (error_text);

  gst_d3d11_device_log_live_objects (device, file, function, line);
}
#endif

/**
 * _gst_d3d11_result:
 * @result: HRESULT D3D11 API return code
 * @device: (nullable): Associated #GstD3D11Device
 * @cat: a #GstDebugCategory
 * @file: the file that checking the result code
 * @function: the function that checking the result code
 * @line: the line that checking the result code
 *
 * Prints debug message if @result code indicates the operation was failed.
 *
 * Returns: %TRUE if D3D11 API call result is SUCCESS
 *
 * Since: 1.22
 */
gboolean
_gst_d3d11_result (HRESULT hr, GstD3D11Device * device, GstDebugCategory * cat,
    const gchar * file, const gchar * function, gint line)
{
#ifndef GST_DISABLE_GST_DEBUG
  gboolean ret = TRUE;

  if (FAILED (hr)) {
    gchar *error_text = NULL;

    error_text = g_win32_error_message ((guint) hr);
    /* g_win32_error_message() doesn't cover all HERESULT return code,
     * so it could be empty string, or null if there was an error
     * in g_utf16_to_utf8() */
    gst_debug_log (cat, GST_LEVEL_WARNING, file, function, line,
        NULL, "D3D11 call failed: 0x%x, %s", (guint) hr,
        GST_STR_NULL (error_text));
    g_free (error_text);

    if (device) {
      ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
      hr = device_handle->GetDeviceRemovedReason ();
      if (hr != S_OK) {
        gst_d3d11_log_gpu_remove_reason (hr, device, cat, file, function, line);
      }
    }

    ret = FALSE;
  }
#if (HAVE_D3D11SDKLAYERS_H || HAVE_DXGIDEBUG_H)
  if (device) {
    gst_d3d11_device_d3d11_debug (device, file, function, line);
    gst_d3d11_device_dxgi_debug (device, file, function, line);
  }
#endif

  return ret;
#else
  return SUCCEEDED (hr);
#endif
}

/**
 * gst_d3d11_create_user_token:
 *
 * Creates new user token value
 *
 * Returns: user token value
 *
 * Since: 1.24
 */
gint64
gst_d3d11_create_user_token (void)
{
  /* *INDENT-OFF* */
  static std::atomic < gint64 > user_token { 0 };
  /* *INDENT-ON* */

  return user_token.fetch_add (1);
}
