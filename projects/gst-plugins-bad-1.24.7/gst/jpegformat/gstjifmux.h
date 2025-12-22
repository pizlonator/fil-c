/* GStreamer
 *
 * jifmux: JPEG interchange format muxer
 *
 * Copyright (C) 2010 Stefan Kost <stefan.kost@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_JFIF_MUX_H__
#define __GST_JFIF_MUX_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_JIF_MUX \
  (gst_jif_mux_get_type())
#define GST_JIF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JIF_MUX,GstJifMux))
#define GST_JIF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JIF_MUX,GstJifMuxClass))
#define GST_IS_JIF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JIF_MUX))
#define GST_IS_JIF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JIF_MUX))
#define GST_JIF_MUX_CAST(obj) ((GstJifMux *) (obj))

typedef struct _GstJifMux           GstJifMux;
typedef struct _GstJifMuxClass      GstJifMuxClass;

struct _GstJifMux {
  GstElement element;

  GstPad *srcpad;

  /* list of GstJifMuxMarker */
  GList *markers;
  guint scan_size;
  const guint8 *scan_data;
};

struct _GstJifMuxClass {
  GstElementClass  parent_class;
};

GType gst_jif_mux_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (jifmux);

G_END_DECLS

#endif /* __GST_JFIF_MUX_H__ */
