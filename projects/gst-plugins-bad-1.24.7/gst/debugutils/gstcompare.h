/* GStreamer Element
 *
 * Copyright 2011 Collabora Ltd.
 *  @author: Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright 2011 Nokia Corp.
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


#ifndef __GST_COMPARE_H__
#define __GST_COMPARE_H__


#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_COMPARE \
  (gst_compare_get_type())
#define GST_COMPARE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_COMPARE, GstCompare))
#define GST_COMPARE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_COMPARE, GstCompareClass))
#define GST_COMPARE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_COMPARE, GstCompareClass))
#define GST_IS_COMPARE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_COMPARE))
#define GST_IS_COMPARE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_COMPARE))

typedef struct _GstCompare GstCompare;
typedef struct _GstCompareClass GstCompareClass;

struct _GstCompare {
  GstElement element;

  GstPad *srcpad;
  GstPad *sinkpad;
  GstPad *checkpad;

  GstCollectPads *cpads;

  gint count;

  /* properties */
  GstBufferCopyFlags meta;
  gboolean offset_ts;
  gint method;
  gdouble threshold;
  gboolean upper;
};

struct _GstCompareClass {
  GstElementClass parent_class;
};

GType gst_compare_get_type(void);

G_END_DECLS

#endif /* __GST_COMPARE_H__ */
