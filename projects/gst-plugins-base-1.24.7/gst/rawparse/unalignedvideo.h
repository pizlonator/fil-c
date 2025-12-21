/* GStreamer
 * Copyright (C) 2016 Carlos Rafael Giani <dv@pseudoterminal.org>
 *
 * unalignedvideo.h:
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

#ifndef __GST_UNALIGNED_VIDEO_H__
#define __GST_UNALIGNED_VIDEO_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#define GST_UNALIGNED_RAW_VIDEO_CAPS \
  "video/x-unaligned-raw" \
  ", format = (string) " GST_VIDEO_FORMATS_ALL \
  ", width = " GST_VIDEO_SIZE_RANGE \
  ", height = " GST_VIDEO_SIZE_RANGE \
  ", framerate = " GST_VIDEO_FPS_RANGE

#endif /* __GST_UNALIGNED_VIDEO_H__ */
