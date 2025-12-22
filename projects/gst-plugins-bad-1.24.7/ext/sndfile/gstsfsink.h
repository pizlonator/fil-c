/* GStreamer libsndfile plugin
 * Copyright (C) 2003,2007 Andy Wingo <wingo at pobox dot com>
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


#ifndef __GST_SF_SINK_H__
#define __GST_SF_SINK_H__


#include "gstsf.h"
#include <gst/base/gstbasesink.h>


G_BEGIN_DECLS


#define GST_TYPE_SF_SINK \
  (gst_sf_sink_get_type())
#define GST_SF_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SF_SINK,GstSFSink))
#define GST_SF_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SF_SINK,GstSFSinkClass))
#define GST_IS_SF_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SF_SINK))
#define GST_IS_SF_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SF_SINK))

typedef struct _GstSFSink GstSFSink;
typedef struct _GstSFSinkClass GstSFSinkClass;

typedef sf_count_t (*GstSFWriter)(SNDFILE *f, void *data, sf_count_t nframes);

struct _GstSFSink {
  GstBaseSink parent;

  gchar *location;
  SNDFILE *file;
  GstSFWriter writer;
  gint bytes_per_frame;

  gint channels;
  gint rate;

  gint format_major;
  gint format_subtype;
  gint format;
  gint buffer_frames;
};

struct _GstSFSinkClass {
  GstBaseSinkClass parent_class;
};

#define GST_TYPE_SF_MAJOR_TYPES (gst_sf_major_types_get_type())
#define GST_TYPE_SF_MINOR_TYPES (gst_sf_minor_types_get_type())

GType gst_sf_major_types_get_type (void);
GType gst_sf_minor_types_get_type (void);

G_END_DECLS


#endif /* __GST_SF_SINK_H__ */
