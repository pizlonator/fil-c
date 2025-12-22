/* GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
#ifndef _GST_VIEWFINDER_BIN_H_
#define _GST_VIEWFINDER_BIN_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_VIEWFINDER_BIN   (gst_viewfinder_bin_get_type())
#define GST_VIEWFINDER_BIN(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIEWFINDER_BIN,GstViewfinderBin))
#define GST_VIEWFINDER_BIN_CAST(obj)   ((GstViewfinderBin *) obj)
#define GST_VIEWFINDER_BIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIEWFINDER_BIN,GstViewfinderBinClass))
#define GST_IS_VIEWFINDER_BIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIEWFINDER_BIN))
#define GST_IS_VIEWFINDER_BIN_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIEWFINDER_BIN))

typedef struct _GstViewfinderBin GstViewfinderBin;
typedef struct _GstViewfinderBinClass GstViewfinderBinClass;

struct _GstViewfinderBin
{
  GstBin bin;

  GstPad *ghostpad;

  GstElement *video_sink;
  GstElement *user_video_sink;

  gboolean elements_created;

  gboolean disable_converters;
};

struct _GstViewfinderBinClass
{
  GstBinClass bin_class;
};

GType gst_viewfinder_bin_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (viewfinderbin);

G_END_DECLS

#endif
