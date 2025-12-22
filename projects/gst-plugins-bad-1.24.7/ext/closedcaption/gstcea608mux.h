/*
 * GStreamer
 * Copyright (C) 2023 Mathieu Duponchelle <mathieu@centricular.com>
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

#ifndef __GST_CEA608MUX_H__
#define __GST_CEA608MUX_H__

#include <gst/gst.h>
#include <gst/base/base.h>

#include "ccutils.h"

G_BEGIN_DECLS
#define GST_TYPE_CEA608MUX \
  (gst_cea608_mux_get_type())
#define GST_CEA608MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CEA608MUX,GstCea608Mux))
#define GST_CEA608MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CEA608MUX,GstCea608MuxClass))
#define GST_IS_CEA608MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CEA608MUX))
#define GST_IS_CEA608MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CEA608MUX))

typedef struct _GstCea608Mux GstCea608Mux;
typedef struct _GstCea608MuxClass GstCea608MuxClass;

struct _GstCea608Mux
{
  GstAggregator parent;

  CCBuffer *cc_buffer;
  GstClockTime earliest_input_running_time;
  GstClockTime start_time;
  gint n_output_buffers;
  const struct cdp_fps_entry *cdp_fps_entry;
};

struct _GstCea608MuxClass
{
  GstAggregatorClass parent_class;
};

GType gst_cea608_mux_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (cea608mux);

G_END_DECLS
#endif /* __GST_CEA608MUX_H__ */
