/* GStreamer Musepack decoder plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#ifndef __GST_MUSEPACK_DEC_H__
#define __GST_MUSEPACK_DEC_H__

#include <mpc/mpcdec.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_MUSEPACK_DEC \
  (gst_musepackdec_get_type ())
#define GST_MUSEPACK_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MUSEPACK_DEC, \
                               GstMusepackDec))
#define GST_MUSEPACK_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MUSEPACK_DEC, \
                            GstMusepackDecClass))
#define GST_IS_MUSEPACK_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MUSEPACK_DEC))
#define GST_IS_MUSEPACK_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MUSEPACK_DEC))

typedef struct _GstMusepackDec {
  GstElement    element;

  GstPad       *srcpad;
  GstPad       *sinkpad;
  guint64       offset;

  /* MUSEPACK_DEC object */
  mpc_demux    *d;
  mpc_reader   *r;

  gint          bps;     /* bytes per sample */ /* ATOMIC */
  gint          rate;    /* sample rate      */ /* ATOMIC */

  GstSegment    segment; /* configured segment in samples (DEFAULT format) */
} GstMusepackDec;

typedef struct _GstMusepackDecClass {
  GstElementClass parent_class;
} GstMusepackDecClass;

GType gst_musepackdec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (musepackdec);

G_END_DECLS

#endif /* __GST_MUSEPACK_DEC_H__ */
