/* GStreamer
 * Copyright (C) 2008 David Schleef <ds@schleef.org>
 * Copyright (C) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
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

#ifndef __GST_VIDEO_H__
#include <gst/video/video.h>
#endif

#ifndef _GST_VIDEO_UTILS_PRIVATE_H_
#define _GST_VIDEO_UTILS_PRIVATE_H_

#include <gst/gst.h>

G_BEGIN_DECLS

/* Element utility functions */
G_GNUC_INTERNAL
GstCaps *__gst_video_element_proxy_getcaps (GstElement * element, GstPad * sinkpad,
                                            GstPad * srcpad, GstCaps * initial_caps,
                                            GstCaps * filter);

G_GNUC_INTERNAL
gboolean __gst_video_encoded_video_convert (gint64 bytes, gint64 time,
                                            GstFormat src_format, gint64 src_value,
                                            GstFormat * dest_format, gint64 * dest_value);

G_GNUC_INTERNAL
gboolean __gst_video_rawvideo_convert (GstVideoCodecState * state, GstFormat src_format,
                                       gint64 src_value, GstFormat * dest_format,
                                       gint64 * dest_value);

G_END_DECLS

#endif
