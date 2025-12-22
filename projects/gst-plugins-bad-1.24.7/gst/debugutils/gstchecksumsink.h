/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
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

#ifndef _GST_CHECKSUM_SINK_H_
#define _GST_CHECKSUM_SINK_H_

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_CHECKSUM_SINK   (gst_checksum_sink_get_type())
#define GST_CHECKSUM_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CHECKSUM_SINK,GstChecksumSink))
#define GST_CHECKSUM_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CHECKSUM_SINK,GstChecksumSinkClass))
#define GST_IS_CHECKSUM_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CHECKSUM_SINK))
#define GST_IS_CHECKSUM_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CHECKSUM_SINK))

typedef struct _GstChecksumSink GstChecksumSink;
typedef struct _GstChecksumSinkClass GstChecksumSinkClass;

struct _GstChecksumSink
{
  GstBaseSink base_checksumsink;
  GChecksumType hash;
};

struct _GstChecksumSinkClass
{
  GstBaseSinkClass base_checksumsink_class;
};

GType gst_checksum_sink_get_type (void);

G_END_DECLS

#endif
