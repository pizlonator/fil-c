/* GStreamer Wayland Library
 *
 * Copyright (C) 2022 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwlcontext.h"

gboolean
gst_is_wl_display_handle_need_context_message (GstMessage * msg)
{
  const gchar *type = NULL;

  g_return_val_if_fail (GST_IS_MESSAGE (msg), FALSE);

  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_NEED_CONTEXT &&
      gst_message_parse_context_type (msg, &type)) {
    return !g_strcmp0 (type, GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE);
  }

  return FALSE;
}

GstContext *
gst_wl_display_handle_context_new (struct wl_display *display)
{
  GstContext *context =
      gst_context_new (GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE, TRUE);
  gst_structure_set (gst_context_writable_structure (context),
      "display", G_TYPE_POINTER, display, NULL);
  return context;
}

struct wl_display *
gst_wl_display_handle_context_get_handle (GstContext * context)
{
  const GstStructure *s;
  struct wl_display *display;

  g_return_val_if_fail (GST_IS_CONTEXT (context), NULL);

  s = gst_context_get_structure (context);
  if (gst_structure_get (s, "display", G_TYPE_POINTER, &display, NULL))
    return display;
  if (gst_structure_get (s, "handle", G_TYPE_POINTER, &display, NULL))
    return display;
  return NULL;
}
