/* GStreamer
 * Copyright (C) 2020 Collabora Ltd.
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

#ifndef _GST_V4L2_CODEC_DEVICE_H_
#define _GST_V4L2_CODEC_DEVICE_H_

#include <gst/gst.h>

#define GST_TYPE_V4L2_CODEC_DEVICE     (gst_v4l2_codec_device_get_type())
#define GST_IS_V4L2_CODEC_DEVICE(obj)  (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_V4L2_CODEC_DEVICE))
#define GST_V4L2_CODEC_DEVICE(obj)     ((GstV4l2CodecDevice *)(obj))

typedef struct {
  GstMiniObject mini_object;

  gchar *name;
  guint32 function;
  gchar *media_device_path;
  gchar *video_device_path;
} GstV4l2CodecDevice;

GType  gst_v4l2_codec_device_get_type (void);
GList *gst_v4l2_codec_find_devices (void);
void   gst_v4l2_codec_device_list_free (GList *devices);

#endif /* _GST_V4L2_CODECS_DEVICE_H_ */
