/* GStreamer
 * Copyright (C) 2019 Intel Corporation
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

#ifndef _GST_CLOCK_SELECT_H_
#define _GST_CLOCK_SELECT_H_

#include <gst/gstpipeline.h>

G_BEGIN_DECLS

#define GST_TYPE_CLOCK_SELECT   (gst_clock_select_get_type())
#define GST_CLOCK_SELECT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLOCK_SELECT,GstClockSelect))
#define GST_CLOCK_SELECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLOCK_SELECT,GstClockSelectClass))
#define GST_IS_CLOCK_SELECT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLOCK_SELECT))
#define GST_IS_CLOCK_SELECT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLOCK_SELECT))

typedef struct _GstClockSelect GstClockSelect;
typedef struct _GstClockSelectClass GstClockSelectClass;
typedef enum _GstClockSelectClockId GstClockSelectClockId;

enum _GstClockSelectClockId {
  GST_CLOCK_SELECT_CLOCK_ID_DEFAULT,
  GST_CLOCK_SELECT_CLOCK_ID_MONOTONIC,
  GST_CLOCK_SELECT_CLOCK_ID_REALTIME,
  GST_CLOCK_SELECT_CLOCK_ID_PTP,
  GST_CLOCK_SELECT_CLOCK_ID_TAI,
};

struct _GstClockSelect
{
  GstPipeline base_clock_select;

  GstClockSelectClockId clock_id;
  guint8 ptp_domain;
};

struct _GstClockSelectClass
{
  GstPipelineClass base_clock_select_class;
};

GType gst_clock_select_get_type (void);

G_END_DECLS

#endif
