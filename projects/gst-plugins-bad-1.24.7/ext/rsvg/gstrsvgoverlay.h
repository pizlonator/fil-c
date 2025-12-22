/* GStreamer
 * Copyright (C) 2010 Olivier Aubert <olivier.aubert@liris.cnrs.fr>
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

#ifndef __GST_RSVG_OVERLAY_H__
#define __GST_RSVG_OVERLAY_H__

#include <librsvg/rsvg.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS
#define GST_TYPE_RSVG_OVERLAY 	     (gst_rsvg_overlay_get_type())
#define GST_RSVG_OVERLAY(obj) 	     (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RSVG_OVERLAY,GstRsvgOverlay))
#define GST_RSVG_OVERLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RSVG_OVERLAY,GstRsvgOverlayClass))
#define GST_IS_RSVG_OVERLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RSVG_OVERLAY))
#define GST_IS_RSVG_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RSVG_OVERLAY))
typedef struct _GstRsvgOverlay GstRsvgOverlay;
typedef struct _GstRsvgOverlayClass GstRsvgOverlayClass;

/**
 * GstRsvgOverlay:
 *
 * Opaque object data structure.
 */
struct _GstRsvgOverlay
{
  GstVideoFilter element;

  /* < private > */
  GMutex rsvg_lock;

  RsvgHandle *handle;

  /* width and height of the SVG data */
  int svg_width;
  int svg_height;

  int x_offset;
  int y_offset;
  float x_relative;
  float y_relative;

  int width;
  int height;
  float width_relative;
  float height_relative;

  GstPad *data_sinkpad;
  GstAdapter *adapter;
};

struct _GstRsvgOverlayClass
{
  GstVideoFilterClass parent_class;
};

GType gst_rsvg_overlay_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (rsvgoverlay);

G_END_DECLS
#endif /* __GST_RSVG_OVERLAY_H__ */
