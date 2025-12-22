/* GStreamer
 * Copyright (C) 2023 Benjamin Gaignard <benjamin.gaignard@collabora.Com>
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

#ifndef __GST_H265_2_JSON_H__
#define __GST_H265_2_JSON_H__

#include <gst/codecparsers/gsth265parser.h>

G_BEGIN_DECLS

#define GST_TYPE_H265_2_JSON (gst_h265_2_json_get_type())
G_DECLARE_FINAL_TYPE (GstH2652json,
    gst_h265_2_json, GST, H265_2_JSON, GstElement);

G_END_DECLS

#endif /* __GST_H265_2_TXT_H__ */
