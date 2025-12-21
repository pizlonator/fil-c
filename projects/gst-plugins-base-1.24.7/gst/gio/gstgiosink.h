/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007-2009 Sebastian Dröge <slomo@circular-chaos.org>
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

#ifndef __GST_GIO_SINK_H__
#define __GST_GIO_SINK_H__

#include "gstgiobasesink.h"

#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_GIO_SINK (gst_gio_sink_get_type())
G_DECLARE_FINAL_TYPE (GstGioSink, gst_gio_sink, GST, GIO_SINK, GstGioBaseSink)

/**
 * GstGioSink:
 *
 * Opaque data structure.
 */
struct _GstGioSink
{
  GstGioBaseSink sink;

  /*< private >*/
  GFile *file;
};

G_END_DECLS

#endif /* __GST_GIO_SINK_H__ */
