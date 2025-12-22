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

#pragma once

#include <gst/gst.h>
#include <gst/va/gstva.h>
#include <gst/va/va_fwd.h>

G_BEGIN_DECLS

GST_VA_API
gboolean              gst_va_ensure_element_data          (gpointer element,
                                                           const gchar *render_device_path,
                                                           GstVaDisplay ** display_ptr);
GST_VA_API
void                  gst_va_context_query                (GstElement * element,
                                                           const gchar * context_type);
GST_VA_API
gboolean              gst_va_handle_set_context           (GstElement * element,
                                                           GstContext * context,
                                                           const gchar *render_device_path,
                                                           GstVaDisplay ** display_ptr);
GST_VA_API
gboolean              gst_va_handle_context_query         (GstElement * element,
                                                           GstQuery * query,
                                                           GstVaDisplay * display);
GST_VA_API
void                  gst_va_element_propagate_display_context (GstElement * element,
                                                           GstVaDisplay * display);
GST_VA_API
gboolean              gst_context_get_va_display          (GstContext * context,
                                                           const gchar * type_name,
                                                           const gchar * render_device_path,
                                                           GstVaDisplay ** display_ptr);
GST_VA_API
void                  gst_context_set_va_display          (GstContext * context,
                                                           GstVaDisplay * display);

G_END_DECLS
