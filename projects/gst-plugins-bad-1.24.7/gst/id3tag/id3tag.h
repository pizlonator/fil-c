/* GStreamer ID3v2 tag writer
 * Copyright (C) 2009 Tim-Philipp Müller <tim centricular net>
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

#include <gst/tag/gsttagmux.h>

G_BEGIN_DECLS

#define ID3_VERSION_2_3 3
#define ID3_VERSION_2_4 4

GstBuffer * id3_mux_render_v2_tag (GstTagMux        * mux,
                                   const GstTagList * taglist,
                                   int                version);

GstBuffer * id3_mux_render_v1_tag (GstTagMux        * mux,
                                   const GstTagList * taglist);

G_END_DECLS
