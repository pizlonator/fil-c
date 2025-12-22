/* GStreamer
 * Copyright (C) 2010 REAL_NAME <EMAIL_ADDRESS>
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

#ifndef _GST_CHOP_MY_DATA_H_
#define _GST_CHOP_MY_DATA_H_

#include <gst/gst.h>
#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_CHOP_MY_DATA   (gst_chop_my_data_get_type())
#define GST_CHOP_MY_DATA(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CHOP_MY_DATA,GstChopMyData))
#define GST_CHOP_MY_DATA_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CHOP_MY_DATA,GstChopMyDataClass))
#define GST_IS_CHOP_MY_DATA(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CHOP_MY_DATA))
#define GST_IS_CHOP_MY_DATA_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CHOP_MY_DATA))

typedef struct _GstChopMyData GstChopMyData;
typedef struct _GstChopMyDataClass GstChopMyDataClass;

struct _GstChopMyData
{
  GstElement base_chopmydata;

  GstPad *srcpad;
  GstPad *sinkpad;
  GstAdapter *adapter;
  GRand *rand;

  /* properties */
  int step_size;
  int min_size;
  int max_size;

  /* state */
  int next_size;
};

struct _GstChopMyDataClass
{
  GstElementClass base_chopmydata_class;
};

GType gst_chop_my_data_get_type (void);

G_END_DECLS

#endif
