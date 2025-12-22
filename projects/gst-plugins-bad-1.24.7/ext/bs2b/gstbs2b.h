/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2011,2014> Christoph Reiter <reiter.christoph@gmail.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __GST_BS2B_H__
#define __GST_BS2B_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <bs2b/bs2b.h>

G_BEGIN_DECLS
#define GST_TYPE_BS2B \
  (gst_bs2b_get_type())
#define GST_BS2B(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BS2B,GstBs2b))
#define GST_BS2B_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BS2B,GstBs2bClass))
#define GST_IS_BS2B(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BS2B))
#define GST_IS_BS2B_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BS2B))
typedef struct _GstBs2b GstBs2b;
typedef struct _GstBs2bClass GstBs2bClass;

typedef void (*GstBs2bProcessFunc)(t_bs2bdp bs2bdp, void *sample, int n);

struct _GstBs2b
{
  GstAudioFilter element;

  /*< private > */
  GMutex bs2b_lock;
  t_bs2bdp bs2bdp;
  GstBs2bProcessFunc func;
  guint bytes_per_sample;
};

struct _GstBs2bClass
{
  GstAudioFilterClass parent_class;
};

GType gst_bs2b_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (bs2b);

G_END_DECLS
#endif /* __GST_BS2B_H__ */
