/* GStreamer
 * Copyright (C) <2023> The GStreamer Contributors.
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

#include "gsth264picture.h"

G_BEGIN_DECLS

gint32 gst_h264_dpb_get_last_output_poc (GstH264Dpb * dpb);

void   gst_h264_picture_set_reference   (GstH264Picture * picture,
                                         GstH264PictureReference reference,
                                         gboolean other_field);

G_END_DECLS
