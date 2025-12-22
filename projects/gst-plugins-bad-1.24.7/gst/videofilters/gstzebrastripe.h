/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
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

#ifndef _GST_ZEBRA_STRIPE_H_
#define _GST_ZEBRA_STRIPE_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_ZEBRA_STRIPE   (gst_zebra_stripe_get_type())
#define GST_ZEBRA_STRIPE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZEBRA_STRIPE,GstZebraStripe))
#define GST_ZEBRA_STRIPE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZEBRA_STRIPE,GstZebraStripeClass))
#define GST_IS_ZEBRA_STRIPE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZEBRA_STRIPE))
#define GST_IS_ZEBRA_STRIPE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZEBRA_STRIPE))

typedef struct _GstZebraStripe GstZebraStripe;
typedef struct _GstZebraStripeClass GstZebraStripeClass;

struct _GstZebraStripe
{
  GstVideoFilter base_zebrastripe;

  /* properties */
  int threshold;

  /* state */
  int t;
  int y_threshold;
};

struct _GstZebraStripeClass
{
  GstVideoFilterClass base_zebrastripe_class;
};

GType gst_zebra_stripe_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (zebrastripe);

G_END_DECLS

#endif
