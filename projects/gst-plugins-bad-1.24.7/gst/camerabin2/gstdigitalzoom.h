/*
 * GStreamer
 * Copyright (C) 2015 Thiago Santos <thiagoss@osg.samsung.com>
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


#ifndef __GST_DIGITAL_ZOOM_H__
#define __GST_DIGITAL_ZOOM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DIGITAL_ZOOM \
  (gst_digital_zoom_get_type())
#define GST_DIGITAL_ZOOM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DIGITAL_ZOOM,GstDigitalZoom))
#define GST_DIGITAL_ZOOM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DIGITAL_ZOOM,GstDigitalZoomClass))
#define GST_IS_DIGITAL_ZOOM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DIGITAL_ZOOM))
#define GST_IS_DIGITAL_ZOOM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DIGITAL_ZOOM))
#define GST_DIGITAL_ZOOM_CAST(d) ((GstDigitalZoom *)(d))

GType gst_digital_zoom_get_type (void);

typedef struct _GstDigitalZoom GstDigitalZoom;
typedef struct _GstDigitalZoomClass GstDigitalZoomClass;

/**
 * GstDigitalZoom:
 *
 */
struct _GstDigitalZoom
{
  GstBin parent;

  GstPad *srcpad;
  GstPad *sinkpad;

  gboolean elements_created;
  GstElement *videocrop;
  GstElement *videoscale;
  GstElement *capsfilter;

  GstPad *capsfilter_sinkpad;

  gfloat zoom;
};


/**
 * GstDigitalZoomClass:
 *
 */
struct _GstDigitalZoomClass
{
  GstBinClass parent;
};

G_END_DECLS

#endif /* __GST_DIGITAL_ZOOM_H__ */
