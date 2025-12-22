/*
 * Copyright (C) 2023 Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef _APPLEMEDIA_HELPERS_H_
#define _APPLEMEDIA_HELPERS_H_

#include <gst/video/video.h>

G_BEGIN_DECLS

// kCVPixelFormatType_64RGBALE is only available for 11.3 +.
// See https://developer.apple.com/documentation/corevideo/1563591-pixel_format_identifiers/kcvpixelformattype_64rgbale
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 110300
#define kCVPixelFormatType_64RGBALE 'l64r'
#endif
#define GST_APPLEMEDIA_HAVE_64RGBALE __builtin_available(macOS 11.3, *)

#define GST_CVPIXELFORMAT_FOURCC_ARGS(fourcc) \
  __GST_PRINT_CHAR(((fourcc) >> 24) & 0xff),  \
  __GST_PRINT_CHAR(((fourcc) >> 16) & 0xff),  \
  __GST_PRINT_CHAR(((fourcc) >> 8) & 0xff),   \
  __GST_PRINT_CHAR((fourcc) & 0xff)

GstVideoFormat          gst_video_format_from_cvpixelformat         (int fmt);
int                     gst_video_format_to_cvpixelformat           (GstVideoFormat fmt);

G_END_DECLS
#endif /* _APPLEMEDIA_HELPERS_H_ */
