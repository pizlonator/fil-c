/* GStreamer
 * Copyright (C) <2021> Collabora Ltd.
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

#ifndef __GST_CODEC_ALPHA_DEMUX_H__
#define __GST_CODEC_ALPHA_DEMUX_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_CODEC_ALPHA_DEMUX (gst_codec_alpha_demux_get_type())
G_DECLARE_FINAL_TYPE (GstCodecAlphaDemux,
    gst_codec_alpha_demux, GST, CODEC_ALPHA_DEMUX, GstElement);

GST_ELEMENT_REGISTER_DECLARE (codec_alpha_demux);

G_END_DECLS
#endif
