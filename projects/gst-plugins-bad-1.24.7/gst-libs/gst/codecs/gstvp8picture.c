/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstvp8picture.h"
#include "gstcodecpicture-private.h"

GST_DEBUG_CATEGORY_EXTERN (gst_vp8_decoder_debug);
#define GST_CAT_DEFAULT gst_vp8_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstVp8Picture, gst_vp8_picture);

/**
 * gst_vp8_picture_new:
 *
 * Create new #GstVp8Picture
 *
 * Returns: a new #GstVp8Picture
 */
GstVp8Picture *
gst_vp8_picture_new (void)
{
  GstVp8Picture *pic;

  pic = g_new0 (GstVp8Picture, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (pic), 0,
      GST_TYPE_VP8_PICTURE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_codec_picture_free);

  GST_TRACE ("New picture %p", pic);

  return pic;
}
