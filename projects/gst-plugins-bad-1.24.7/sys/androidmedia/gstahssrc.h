/* GStreamer android.hardware.Sensor Source
 * Copyright (C) 2016 SurroundIO
 *   Author: Martin Kelly <martin@surround.io>
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

#ifndef _GST_AHSSRC_H__
#define _GST_AHSSRC_H__

#include <gst/base/gstdataqueue.h>
#include <gst/base/gstpushsrc.h>

#include "gst-android-hardware-sensor.h"

G_BEGIN_DECLS
#define GST_TYPE_AHS_SRC   (gst_ahs_src_get_type())
#define GST_AHS_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AHS_SRC,GstAHSSrc))
#define GST_AHS_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AHS_SRC,GstAHSSrcClass))
#define GST_IS_AHS_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AHS_SRC))
#define GST_IS_AHS_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AHS_SRC))
typedef struct _GstAHSSrc GstAHSSrc;
typedef struct _GstAHSSrcClass GstAHSSrcClass;

struct _GstAHSSrc
{
  /* < private > */
  GstPushSrc parent;

  /* properties */
  gint32 sensor_delay;
  gdouble alpha;
  guint sample_interval;

  /* sensor type information */
  GEnumClass *sensor_enum_class;
  gint sensor_type;
  const gchar *sensor_type_name;

  /* JNI wrapper classes */
  GstAHSensorManager *manager;
  GstAHSensor *sensor;
  GstAHSensorEventListener *listener;

  /* timestamping */
  GstClockTime previous_time;
  gfloat *current_sample;

  /* buffers */
  gboolean callback_registered;
  gint sample_index;
  gint sample_length;
  gint buffer_size;

  /* multiprocessing */
  GstDataQueue *queue;
};

struct _GstAHSSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_ahs_src_get_type (void);

G_END_DECLS
#endif
