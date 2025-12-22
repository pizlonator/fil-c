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


#ifndef __GST_SF_SRC_H__
#define __GST_SF_SRC_H__


#include "gstsf.h"
#include <gst/base/gstbasesrc.h>


G_BEGIN_DECLS


#define GST_TYPE_SF_SRC \
  (gst_sf_src_get_type())
#define GST_SF_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SF_SRC,GstSFSrc))
#define GST_SF_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SF_SRC,GstSFSrcClass))
#define GST_IS_SF_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SF_SRC))
#define GST_IS_SF_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SF_SRC))

typedef struct _GstSFSrc GstSFSrc;
typedef struct _GstSFSrcClass GstSFSrcClass;

typedef sf_count_t (*GstSFReader)(SNDFILE *f, void *data, sf_count_t nframes);

struct _GstSFSrc {
  GstBaseSrc parent;

  gchar *location;
  SNDFILE *file;
  sf_count_t offset;
  GstSFReader reader;
  gint bytes_per_frame;

  gint channels;
  gint rate;
};

struct _GstSFSrcClass {
  GstBaseSrcClass parent_class;
};


G_END_DECLS


#endif /* __GST_SF_SRC_H__ */
