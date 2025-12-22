/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_SMOOTH_H__
#define __GST_SMOOTH_H__


#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GST_TYPE_SMOOTH \
  (gst_smooth_get_type())
#define GST_SMOOTH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SMOOTH,GstSmooth))
#define GST_SMOOTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SMOOTH,GstSmoothClass))
#define GST_IS_SMOOTH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SMOOTH))
#define GST_IS_SMOOTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SMOOTH))

typedef struct _GstSmooth GstSmooth;
typedef struct _GstSmoothClass GstSmoothClass;

struct _GstSmooth {
  GstVideoFilter element;

  int format;
  int width;
  int height;

  gboolean active;
  int tolerance;
  int filtersize;
  gboolean luma_only;

  GstPad *sinkpad,*srcpad;
};

struct _GstSmoothClass {
  GstVideoFilterClass parent_class;
};

GType gst_smooth_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (smooth);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_SMOOTH_H__ */
