/*
 * GStreamer gstreamer-ssdobjectdetector
 * Copyright (C) 2021 Collabora Ltd
 *
 * gstssdobjectdetector.h
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

#ifndef __GST_SSD_OBJECT_DETECTOR_H__
#define __GST_SSD_OBJECT_DETECTOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_SSD_OBJECT_DETECTOR            (gst_ssd_object_detector_get_type())
G_DECLARE_FINAL_TYPE (GstSsdObjectDetector, gst_ssd_object_detector, GST, SSD_OBJECT_DETECTOR, GstBaseTransform)

#define GST_SSD_OBJECT_DETECTOR_META_NAME "ssd-object-detector"
#define GST_SSD_OBJECT_DETECTOR_META_PARAM_NAME "extra-data"
#define GST_SSD_OBJECT_DETECTOR_META_FIELD_LABEL "label"
#define GST_SSD_OBJECT_DETECTOR_META_FIELD_SCORE "score"

/*
 * GstSsdObjectDetector:
 *
 * @label_file label file
 * @score_threshold score threshold
 *
 * Since: 1.20
 */
struct _GstSsdObjectDetector
{
  GstBaseTransform basetransform;
  gchar *label_file;
  GArray *labels;
  gfloat score_threshold;
  GstVideoInfo video_info;
};

/**
 * GstSsdObjectDetectorClass:
 *
 * @parent_class base transform base class
 *
 * Since: 1.20
 */
struct _GstSsdObjectDetectorClass
{
  GstBaseTransformClass parent_class;
};

GST_ELEMENT_REGISTER_DECLARE (ssd_object_detector)

G_END_DECLS

#endif /* __GST_SSD_OBJECT_DETECTOR_H__ */
