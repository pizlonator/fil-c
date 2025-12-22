/*
 * GStreamer
 * Copyright (C) 2017 Collabora Inc.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#ifndef __GST_FAKE_SINK_UTILS_H__
#define __GST_FAKE_SINK_UTILS_H__

#include <gst/gst.h>

G_BEGIN_DECLS

void
gst_util_proxy_class_properties (GObjectClass *object_class,
                                 GObjectClass *target_class,
                                 guint property_id_offset);

G_END_DECLS

#endif
