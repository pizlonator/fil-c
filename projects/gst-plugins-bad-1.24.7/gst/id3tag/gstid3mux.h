/* GStreamer ID3 v1 and v2 muxer
 *
 * Copyright (C) 2006 Christophe Fergeau <teuf@gnome.org>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Pioneers of the Inevitable <songbird@songbirdnest.com>
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

#ifndef GST_ID3_MUX_H
#define GST_ID3_MUX_H

#include <gst/tag/gsttagmux.h>
#include "id3tag.h"

G_BEGIN_DECLS

typedef struct _GstId3Mux GstId3Mux;
typedef struct _GstId3MuxClass GstId3MuxClass;

struct _GstId3Mux {
  GstTagMux  tagmux;

  gboolean write_v1;
  gboolean write_v2;

  gint     v2_major_version;
};

struct _GstId3MuxClass {
  GstTagMuxClass  tagmux_class;
};

#define GST_TYPE_ID3_MUX \
  (gst_id3_mux_get_type())
#define GST_ID3_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ID3_MUX,GstId3Mux))
#define GST_ID3_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ID3_MUX,GstId3MuxClass))
#define GST_IS_ID3_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ID3_MUX))
#define GST_IS_ID3_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ID3_MUX))

GType gst_id3_mux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (id3mux);
G_END_DECLS

#endif /* GST_ID3_MUX_H */

