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

#pragma once

#include <gst/va/va_fwd.h>
#include <gst/va/va-prelude.h>
#include <gst/va/gstvadisplay.h>

G_BEGIN_DECLS

GST_VA_API
G_DECLARE_FINAL_TYPE (GstVaDisplayWin32, gst_va_display_win32,
    GST, VA_DISPLAY_WIN32, GstVaDisplay);

GST_VA_API
GstVaDisplay * gst_va_display_win32_new	(const gchar * adapter_luid);

G_END_DECLS
