/* GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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

#ifndef __OPENSLESSINK_H__
#define __OPENSLESSINK_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "openslesringbuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_OPENSLES_SINK \
  (gst_opensles_sink_get_type())
#define GST_OPENSLES_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENSLES_SINK,GstOpenSLESSink))
#define GST_OPENSLES_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENSLES_SINK,GstOpenSLESSinkClass))

typedef struct _GstOpenSLESSink GstOpenSLESSink;
typedef struct _GstOpenSLESSinkClass GstOpenSLESSinkClass;

struct _GstOpenSLESSink
{
  GstAudioBaseSink sink;

  GstOpenSLESStreamType stream_type;

  gfloat volume;
  gboolean mute;
};

struct _GstOpenSLESSinkClass
{
  GstAudioBaseSinkClass parent_class;
};

GType gst_opensles_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (openslessink);

G_END_DECLS
#endif /* __OPENSLESSINK_H__ */
