/*
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

#ifndef __GST_ANDROID_HARDWARE_SENSOR_H__
#define __GST_ANDROID_HARDWARE_SENSOR_H__

#include <gst/gst.h>
#include <jni.h>

#include "gstsensors.h"

G_BEGIN_DECLS

typedef struct GstAHSensor
{
  /* < private > */
  jobject object;
} GstAHSensor;

typedef struct GstAHSensorData
{
  jfloatArray array;
  gfloat *values;
} GstAHSensorData;

typedef struct GstAHSensorEvent
{
  /*
   * Note that we don't use the Android event timestamp, as it's not reliable.
   * See https://code.google.com/p/android/issues/detail?id=7981 for more
   * details.
   */
  gint32 accuracy;
  GstAHSensorData data;
} GstAHSensorEvent;

typedef struct GstAHSensorEventListener
{
  /* < private > */
  jobject object;

  gboolean registered;
} GstAHSensorEventListener;

typedef struct GstAHSensorManager
{
  /* < private > */
  jobject object;
} GstAHSensorManager;

gint gst_android_sensor_type_from_string (const gchar * type_str);

/* android.hardware.SensorListener onSensorChanged */
typedef void (*GstAHSensorCallback) (jobject sensor_event, gpointer user_data);

/* android.hardware.SensorListener onAccuracyChanged */
typedef void (*GstAHSAccuracyCallback) (jobject sensor, gint32 accuracy,
    gpointer user_data);

gboolean gst_android_hardware_sensor_init (void);
void gst_android_hardware_sensor_deinit (void);

/*
 * Example usage (excluding error checking):
 *
 * GstAHSensorManager *manager = gst_ah_sensor_get_manager ();
 * GstAHSensor *sensor =
 *     gst_ah_sensor_get_default_sensor (manager, * sensor_type);
 * GstAHSensorEventListener *listener =
 *     gst_ah_sensor_create_listener * (change_cb, accuracy_cb, self);
 * gst_ah_sensor_register_listener (manager, listener, sensor,
 *     SensorDelay_SENSOR_DELAY_NORMAL);
 */
GstAHSensorManager *gst_ah_sensor_get_manager (void);
GstAHSensor *gst_ah_sensor_get_default_sensor (GstAHSensorManager * manager,
    gint32 sensor_type);
GstAHSensorEventListener *gst_ah_sensor_create_listener (GstAHSensorCallback
    sensor_cb, GstAHSAccuracyCallback accuracy_cb, gpointer user_data);
gboolean gst_ah_sensor_register_listener (GstAHSensorManager * self,
    GstAHSensorEventListener * listener, GstAHSensor * sensor, gint32 delay);
void gst_ah_sensor_unregister_listener (GstAHSensorManager * self,
    GstAHSensorEventListener * listener);

gboolean gst_ah_sensor_populate_event (GstAHSensorEvent * event,
    jobject event_object, gint size);
void gst_ah_sensor_free_sensor_data (GstAHSensorData * data);

/*
 * These constants come from the matching SENSOR_DELAY_* TYPE_* constants found
 * in the Android documentation:
 *
 * SENSOR_DELAY_*:
 * https://developer.android.com/reference/android/hardware/SensorManager.html
 *
 * TYPE_*:
 * https://developer.android.com/reference/android/hardware/Sensor.html
 *
 * They are intended to be passed into the registerListener callback for
 * listener registration. Note that, although these are hardcoded, we also do
 * paranoid runtime checks during plugin init to verify that the API values
 * haven't changed. This is unlikely but seems like a good precaution. When
 * adding values, please keep the two lists in sync.
 */
enum
{
  AHS_SENSOR_DELAY_FASTEST = 0,
  AHS_SENSOR_DELAY_GAME = 1,
  AHS_SENSOR_DELAY_NORMAL = 3,
  AHS_SENSOR_DELAY_UI = 2
};

enum
{
  AHS_SENSOR_TYPE_ACCELEROMETER = 0x1,
  AHS_SENSOR_TYPE_AMBIENT_TEMPERATURE = 0xd,
  AHS_SENSOR_TYPE_GAME_ROTATION_VECTOR = 0xf,
  AHS_SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR = 0x14,
  AHS_SENSOR_TYPE_GRAVITY = 0x9,
  AHS_SENSOR_TYPE_GYROSCOPE = 0x4,
  AHS_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED = 0x10,
  AHS_SENSOR_TYPE_HEART_RATE = 0x15,
  AHS_SENSOR_TYPE_LIGHT = 0x5,
  AHS_SENSOR_TYPE_LINEAR_ACCELERATION = 0xa,
  AHS_SENSOR_TYPE_MAGNETIC_FIELD = 0x2,
  AHS_SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED = 0xe,
  AHS_SENSOR_TYPE_ORIENTATION = 0x3,
  AHS_SENSOR_TYPE_PRESSURE = 0x6,
  AHS_SENSOR_TYPE_PROXIMITY = 0x8,
  AHS_SENSOR_TYPE_RELATIVE_HUMIDITY = 0xc,
  AHS_SENSOR_TYPE_ROTATION_VECTOR = 0xb,
  AHS_SENSOR_TYPE_SIGNIFICANT_MOTION = 0x11,
  AHS_SENSOR_TYPE_STEP_COUNTER = 0x13,
  AHS_SENSOR_TYPE_STEP_DETECTOR = 0x12
};

gsize gst_ah_sensor_get_sensor_data_size (gint sensor_type);

G_END_DECLS
#endif /* __GST_ANDROID_HARDWARE_SENSOR_H__ */
