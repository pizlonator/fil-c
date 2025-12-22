/* GStreamer
 * Copyright 2023 Igalia S.L.
 *  @author: Thibault Saunier <tsaunier@igalia.com>
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
#include <gst/gstbin.h>
#include "gstautoconvert.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GstAutoVideoFlip, gst_auto_video_flip, GST, AUTO_VIDEO_FLIP, GstBaseAutoConvert);
GST_ELEMENT_REGISTER_DECLARE (autovideoflip);

G_END_DECLS
