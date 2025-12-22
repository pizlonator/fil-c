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

#ifndef __GST_VP8_ALPHA_DECODE_BIN_H__
#define __GST_VP8_ALPHA_DECODE_BIN_H__

#include "gstalphadecodebin.h"

G_BEGIN_DECLS

#define GST_TYPE_VP8_ALPHA_DECODE_BIN (gst_vp8_alpha_decode_bin_get_type())
G_DECLARE_FINAL_TYPE (GstVp8AlphaDecodeBin, gst_vp8_alpha_decode_bin,
    GST, VP8_ALPHA_DECODE_BIN, GstAlphaDecodeBin);

GST_ELEMENT_REGISTER_DECLARE (vp8_alpha_decode_bin);

G_END_DECLS
#endif
