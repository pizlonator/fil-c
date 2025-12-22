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

#pragma once

#include <gst/wayland/wayland.h>

G_BEGIN_DECLS

/* The type of GstContext used to pass the wl_display pointer
 * from the application to the sink */
#define GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE "GstWlDisplayHandleContextType"

GST_WL_API
gboolean gst_is_wl_display_handle_need_context_message (GstMessage * msg);

GST_WL_API
GstContext *
gst_wl_display_handle_context_new (struct wl_display * display);

GST_WL_API
struct wl_display *
gst_wl_display_handle_context_get_handle (GstContext * context);

G_END_DECLS
