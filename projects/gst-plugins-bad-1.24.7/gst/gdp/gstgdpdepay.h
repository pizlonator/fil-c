/* GStreamer
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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

#ifndef __GST_GDP_DEPAY_H__
#define __GST_GDP_DEPAY_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_GDP_DEPAY \
  (gst_gdp_depay_get_type())
#define GST_GDP_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GDP_DEPAY,GstGDPDepay))
#define GST_GDP_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GDP_DEPAY,GstGDPDepayClass))
#define GST_IS_GDP_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GDP_DEPAY))
#define GST_IS_GDP_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GDP_DEPAY))

typedef enum {
  GST_GDP_DEPAY_STATE_HEADER = 0,
  GST_GDP_DEPAY_STATE_PAYLOAD,
  GST_GDP_DEPAY_STATE_BUFFER,
  GST_GDP_DEPAY_STATE_CAPS,
  GST_GDP_DEPAY_STATE_EVENT,
} GstGDPDepayState;


typedef struct _GstGDPDepay GstGDPDepay;
typedef struct _GstGDPDepayClass GstGDPDepayClass;

/**
 * GstGDPDepay:
 *
 * Private gdpdepay element structure.
 */
struct _GstGDPDepay
{
  GstElement element;
  GstPad *sinkpad;
  GstPad *srcpad;

  GstAdapter *adapter;
  GstGDPDepayState state;
  GstCaps *caps;

  guint8 *header;
  guint32 payload_length;
  GstDPPayloadType payload_type;

  gint64 ts_offset;

  GstAllocator *allocator;
  GstAllocationParams allocation_params;
};

struct _GstGDPDepayClass
{
  GstElementClass parent_class;
};

GType gst_gdp_depay_get_type (void);

G_END_DECLS

#endif /* __GST_GDP_DEPAY_H__ */
