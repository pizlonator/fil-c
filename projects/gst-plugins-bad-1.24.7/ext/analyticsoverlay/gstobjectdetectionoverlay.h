/* GStreamer object detection overlay
 * Copyright (C) <2023> Collabora Ltd.
 *  @author: Aaron Boxer <aaron.boxer@collabora.com>
 *  @author: Daniel Morin <daniel.morin@collabora.com>
 *
 * gstobjectdetectionoverlay.h
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

#ifndef __GST_OBJECT_DETECTION_OVERLAY_H__
#define __GST_OBJECT_DETECTION_OVERLAY_H__

#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_OBJECT_DETECTION_OVERLAY \
  (gst_object_detection_overlay_get_type())

G_DECLARE_FINAL_TYPE (GstObjectDetectionOverlay, gst_object_detection_overlay,
    GST, OBJECT_DETECTION_OVERLAY, GstVideoFilter)

GST_ELEMENT_REGISTER_DECLARE (objectdetectionoverlay);

G_END_DECLS
#endif /* __GST_OBJECT_DETECTION_OVERLAY_H__ */
