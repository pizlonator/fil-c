/* Image Quality Assessment plugin
 * Copyright (C) 2015 Mathieu Duponchelle <mathieu.duponchelle@collabora.co.uk>
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

#ifndef __GST_IQA_H__
#define __GST_IQA_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_IQA (gst_iqa_get_type())
#define GST_IQA(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IQA, GstIqa))
#define GST_IQA_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IQA, GstIqaClass))
#define GST_IS_IQA(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IQA))
#define GST_IS_IQA_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IQA))

typedef struct _GstIqa GstIqa;
typedef struct _GstIqaClass GstIqaClass;

/**
 * GstIqa:
 *
 * The opaque #GstIqa structure.
 */
struct _GstIqa
{
  GstVideoAggregator videoaggregator;

  gboolean do_dssim;
  gdouble ssim_threshold;
  gdouble max_dssim;
  gint mode;
};

struct _GstIqaClass
{
  GstVideoAggregatorClass parent_class;
};

GType gst_iqa_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (iqa);

G_END_DECLS
#endif /* __GST_IQA_H__ */

