/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
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

#ifndef _GST_INTER_SUB_SRC_H_
#define _GST_INTER_SUB_SRC_H_

#include <gst/base/gstbasesrc.h>
#include "gstintersurface.h"

G_BEGIN_DECLS

#define GST_TYPE_INTER_SUB_SRC   (gst_inter_sub_src_get_type())
#define GST_INTER_SUB_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_INTER_SUB_SRC,GstInterSubSrc))
#define GST_INTER_SUB_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_INTER_SUB_SRC,GstInterSubSrcClass))
#define GST_IS_INTER_SUB_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_INTER_SUB_SRC))
#define GST_IS_INTER_SUB_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_INTER_SUB_SRC))

typedef struct _GstInterSubSrc GstInterSubSrc;
typedef struct _GstInterSubSrcClass GstInterSubSrcClass;

struct _GstInterSubSrc
{
  GstBaseSrc base_intersubsrc;

  GstInterSurface *surface;
  char *channel;

  int rate;
  int n_frames;
};

struct _GstInterSubSrcClass
{
  GstBaseSrcClass base_intersubsrc_class;
};

GType gst_inter_sub_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (intersubsrc);

G_END_DECLS

#endif
