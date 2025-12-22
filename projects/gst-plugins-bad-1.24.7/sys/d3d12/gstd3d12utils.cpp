/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d12.h"
#include <mutex>

/* *INDENT-OFF* */
static std::recursive_mutex context_lock_;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);
GST_DEBUG_CATEGORY_EXTERN (gst_d3d12_utils_debug);
#define GST_CAT_DEFAULT gst_d3d12_utils_debug

static void
init_context_debug (void)
{
  GST_D3D12_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
  } GST_D3D12_CALL_ONCE_END;
}

gboolean
gst_d3d12_handle_set_context (GstElement * element, GstContext * context,
    gint adapter_index, GstD3D12Device ** device)
{
  const gchar *context_type;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (device != nullptr, FALSE);

  init_context_debug ();

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);
  if (g_strcmp0 (context_type, GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE) == 0) {
    const GstStructure *str;
    GstD3D12Device *other_device = nullptr;
    guint other_adapter = 0;

    /* If we had device already, will not replace it */
    if (*device)
      return TRUE;

    str = gst_context_get_structure (context);

    if (gst_structure_get (str, "device", GST_TYPE_D3D12_DEVICE,
            &other_device, "adapter-index", G_TYPE_UINT, &other_adapter,
            nullptr)) {
      if (adapter_index == -1 || (guint) adapter_index == other_adapter) {
        GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT,
            element, "Found D3D12 device context");
        *device = other_device;

        return TRUE;
      }

      gst_object_unref (other_device);
    }
  }

  return FALSE;
}

gboolean
gst_d3d12_handle_set_context_for_adapter_luid (GstElement * element,
    GstContext * context, gint64 adapter_luid, GstD3D12Device ** device)
{
  const gchar *context_type;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (device != nullptr, FALSE);

  init_context_debug ();

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);
  if (g_strcmp0 (context_type, GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE) == 0) {
    const GstStructure *str;
    GstD3D12Device *other_device = nullptr;
    gint64 other_adapter = 0;

    /* If we had device already, will not replace it */
    if (*device)
      return TRUE;

    str = gst_context_get_structure (context);

    if (gst_structure_get (str, "device", GST_TYPE_D3D12_DEVICE,
            &other_device, "adapter-luid", G_TYPE_INT64, &other_adapter,
            nullptr)) {
      if (adapter_luid == other_adapter) {
        GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT,
            element, "Found D3D12 device context");
        *device = other_device;

        return TRUE;
      }

      gst_object_unref (other_device);
    }
  }

  return FALSE;
}

static void
context_set_d3d12_device (GstContext * context, GstD3D12Device * device)
{
  GstStructure *s;
  guint adapter_index = 0;
  guint device_id = 0;
  guint vendor_id = 0;
  gchar *desc = nullptr;
  gint64 adapter_luid = 0;

  g_return_if_fail (context != nullptr);

  g_object_get (G_OBJECT (device), "adapter-index", &adapter_index,
      "device-id", &device_id, "vendor-id", &vendor_id, "description", &desc,
      "adapter-luid", &adapter_luid, nullptr);

  GST_CAT_LOG (GST_CAT_CONTEXT,
      "setting GstD3D12Device(%" GST_PTR_FORMAT
      ") with adapter index %d on context(%" GST_PTR_FORMAT ")",
      device, adapter_index, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, "device", GST_TYPE_D3D12_DEVICE, device,
      "adapter-index", G_TYPE_UINT, adapter_index,
      "adapter-luid", G_TYPE_INT64, adapter_luid,
      "device-id", G_TYPE_UINT, device_id,
      "vendor-id", G_TYPE_UINT, vendor_id,
      "description", G_TYPE_STRING, GST_STR_NULL (desc), nullptr);
  g_free (desc);
}

gboolean
gst_d3d12_handle_context_query (GstElement * element, GstQuery * query,
    GstD3D12Device * device)
{
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (GST_IS_QUERY (query), FALSE);

  init_context_debug ();

  GST_LOG_OBJECT (element, "handle context query %" GST_PTR_FORMAT, query);

  if (!device)
    return FALSE;

  gst_query_parse_context_type (query, &context_type);
  if (g_strcmp0 (context_type, GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
    return FALSE;

  gst_query_parse_context (query, &old_context);
  if (old_context)
    context = gst_context_copy (old_context);
  else
    context = gst_context_new (GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);

  context_set_d3d12_device (context, device);
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
  GValue res = G_VALUE_INIT;

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
run_d3d12_context_query (GstElement * element, GstD3D12Device ** device)
{
  GstQuery *query;
  GstContext *ctxt = nullptr;

  query = gst_query_new_context (GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE);
  if (run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%" GST_PTR_FORMAT ") in downstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  if (*device == nullptr && run_query (element, query, GST_PAD_SINK)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%" GST_PTR_FORMAT ") in upstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  if (*device == nullptr) {
    GstMessage *msg;

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting need context message");
    msg = gst_message_new_need_context (GST_OBJECT_CAST (element),
        GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE);
    gst_element_post_message (element, msg);
  }

  gst_query_unref (query);
}

gboolean
gst_d3d12_ensure_element_data (GstElement * element, gint adapter_index,
    GstD3D12Device ** device)
{
  guint target_adapter = 0;
  std::lock_guard < std::recursive_mutex > lk (context_lock_);

  g_return_val_if_fail (element != nullptr, FALSE);
  g_return_val_if_fail (device != nullptr, FALSE);

  init_context_debug ();

  if (*device) {
    GST_LOG_OBJECT (element, "already have a device %" GST_PTR_FORMAT, *device);
    return TRUE;
  }

  run_d3d12_context_query (element, device);
  if (*device)
    return TRUE;

  if (adapter_index > 0)
    target_adapter = adapter_index;

  *device = gst_d3d12_device_new (target_adapter);

  if (*device == nullptr) {
    GST_ERROR_OBJECT (element,
        "Couldn't create new device with adapter index %d", target_adapter);
    return FALSE;
  } else {
    GstContext *context;
    GstMessage *msg;

    /* Propagate new D3D12 device context */

    context = gst_context_new (GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
    context_set_d3d12_device (context, *device);

    gst_element_set_context (element, context);

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting have context (%p) message with D3D12 device context (%p)",
        context, *device);
    msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }

  return TRUE;
}

gboolean
gst_d3d12_ensure_element_data_for_adapter_luid (GstElement * element,
    gint64 adapter_luid, GstD3D12Device ** device)
{
  std::lock_guard < std::recursive_mutex > lk (context_lock_);

  g_return_val_if_fail (element != nullptr, FALSE);
  g_return_val_if_fail (device != nullptr, FALSE);

  init_context_debug ();

  if (*device) {
    GST_LOG_OBJECT (element, "already have a device %" GST_PTR_FORMAT, *device);
    return TRUE;
  }

  run_d3d12_context_query (element, device);
  if (*device)
    return TRUE;

  *device = gst_d3d12_device_new_for_adapter_luid (adapter_luid);

  if (*device == nullptr) {
    GST_ERROR_OBJECT (element,
        "Couldn't create new device with adapter luid %" G_GINT64_FORMAT,
        adapter_luid);
    return FALSE;
  } else {
    GstContext *context;
    GstMessage *msg;

    /* Propagate new D3D12 device context */
    context = gst_context_new (GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
    context_set_d3d12_device (context, *device);

    gst_element_set_context (element, context);

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting have context (%p) message with D3D12 device context (%p)",
        context, *device);
    msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }

  return TRUE;
}

gint64
gst_d3d12_luid_to_int64 (const LUID * luid)
{
  LARGE_INTEGER val;

  g_return_val_if_fail (luid != nullptr, 0);

  val.LowPart = luid->LowPart;
  val.HighPart = luid->HighPart;

  return val.QuadPart;
}

GstContext *
gst_d3d12_context_new (GstD3D12Device * device)
{
  GstContext *context;

  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  context = gst_context_new (GST_D3D12_DEVICE_HANDLE_CONTEXT_TYPE, TRUE);
  context_set_d3d12_device (context, device);

  return context;

}

gboolean
_gst_d3d12_result (HRESULT hr, GstD3D12Device * device, GstDebugCategory * cat,
    const gchar * file, const gchar * function, gint line, GstDebugLevel level)
{
#ifndef GST_DISABLE_GST_DEBUG
  gboolean ret = TRUE;

  if (device)
    gst_d3d12_device_d3d12_debug (device, file, function, line);

  if (FAILED (hr)) {
    gchar *error_text = nullptr;

    error_text = g_win32_error_message ((guint) hr);
    /* g_win32_error_message() doesn't cover all HERESULT return code,
     * so it could be empty string, or nullptr if there was an error
     * in g_utf16_to_utf8() */
    gst_debug_log (cat, level, file, function, line,
        nullptr, "D3D12 call failed: 0x%x, %s", (guint) hr,
        GST_STR_NULL (error_text));
    g_free (error_text);

    ret = FALSE;
  }

  return ret;
#else
  return SUCCEEDED (hr);
#endif
}
