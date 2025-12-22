/* GStreamer
 * Copyright (C) 2010 FIXME <fixme@example.com>
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

#ifndef _GST_RGB_2_BAYER_H_
#define _GST_RGB_2_BAYER_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_RGB_2_BAYER   (gst_rgb2bayer_get_type())
#define GST_RGB_2_BAYER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RGB_2_BAYER,GstRGB2Bayer))
#define GST_RGB_2_BAYER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RGB_2_BAYER,GstRGB2BayerClass))
#define GST_IS_RGB_2_BAYER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RGB_2_BAYER))
#define GST_IS_RGB_2_BAYER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RGB_2_BAYER))

typedef struct _GstRGB2Bayer GstRGB2Bayer;
typedef struct _GstRGB2BayerClass GstRGB2BayerClass;

enum {
  GST_RGB_2_BAYER_FORMAT_BGGR = 0,
  GST_RGB_2_BAYER_FORMAT_GBRG,
  GST_RGB_2_BAYER_FORMAT_GRBG,
  GST_RGB_2_BAYER_FORMAT_RGGB
};

struct _GstRGB2Bayer
{
  GstBaseTransform base_rgb2bayer;

  GstVideoInfo info;
  int width, height;
  int format;
  int bpp;
  int bigendian;
};

struct _GstRGB2BayerClass
{
  GstBaseTransformClass base_rgb2bayer_class;
};

GType gst_rgb2bayer_get_type (void);

G_END_DECLS

#endif
