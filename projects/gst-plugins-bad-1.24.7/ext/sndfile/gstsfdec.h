/* GStreamer libsndfile plugin
 * Copyright (C) 2013 Stefan Sauer <ensonic@users.sf.net>
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


#ifndef __GST_SF_DEC_H__
#define __GST_SF_DEC_H__


#include <gst/base/gstbasesrc.h>


G_BEGIN_DECLS


#define GST_TYPE_SF_DEC \
  (gst_sf_dec_get_type())
#define GST_SF_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SF_DEC,GstSFDec))
#define GST_SF_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SF_DEC,GstSFDecClass))
#define GST_IS_SF_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SF_DEC))
#define GST_IS_SF_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SF_DEC))

typedef struct _GstSFDec GstSFDec;
typedef struct _GstSFDecClass GstSFDecClass;

typedef sf_count_t (*GstSFReader)(SNDFILE *f, void *data, sf_count_t nframes);

struct _GstSFDec {
  GstElement parent;

  GstPad *sinkpad;
  GstPad *srcpad;
  
  guint64 pos;      /* in bytes */
  guint64 duration; /* in frames */
  
  gboolean seekable;

  SNDFILE *file;
  sf_count_t offset;
  GstSFReader reader;
  gint bytes_per_frame;

  gint channels;
  gint rate;
};

struct _GstSFDecClass {
  GstElementClass parent_class;
};

GType gst_sf_dec_get_type (void);

G_END_DECLS


#endif /* __GST_SF_DEC_H__ */
