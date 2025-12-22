/*
 * Copyright (C) 2012, Collabora Ltd.
 * Copyright (C) 2012, Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Justin Kim <justin.kim@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_ANDROID_GRAPHICS_IMAGEFORMAT_H__
#define __GST_ANDROID_GRAPHICS_IMAGEFORMAT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* android.graphics.ImageFormat */
extern gint ImageFormat_JPEG;
extern gint ImageFormat_NV16;
extern gint ImageFormat_NV21;
extern gint ImageFormat_RGB_565;
extern gint ImageFormat_UNKNOWN;
extern gint ImageFormat_YUY2;
extern gint ImageFormat_YV12;

gboolean gst_android_graphics_imageformat_init (void);
void gst_android_graphics_imageformat_deinit (void);

gint gst_ag_imageformat_get_bits_per_pixel (gint format);

G_END_DECLS

#endif /* __GST_ANDROID_GRAPHICS_IMAGEFORMAT_H__ */

