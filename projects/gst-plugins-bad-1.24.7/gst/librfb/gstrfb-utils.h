/* GStreamer
 * Copyright (C) <2022> Marc Leeman <marc.leeman@gmail.com>
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
 *
 * See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/226
 */

#ifndef __GST_RFB_UTILS_H__
#define __GST_RFB_UTILS_H__

#include <gst/gst.h>

void gst_rfb_utils_set_properties_from_uri_query (GObject * obj, const GstUri * uri);

#endif
