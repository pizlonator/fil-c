/* Copyright (C) 2016 SurroundIO
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

#ifndef _GST_AHSCAPS_H__
#define _GST_AHSCAPS_H__

G_BEGIN_DECLS

#define GST_SENSOR_FORMATS_ALL "{" \
     "accelerometer, " \
     "ambient-temperature, " \
     "game-rotation-vector, " \
     "geomagnetic-rotation-vector, " \
     "gravity, " \
     "gyroscope, " \
     "gyroscope-uncalibrated, " \
     "heart-rate, " \
     "light, " \
     "linear-acceleration, " \
     "magnetic-field, " \
     "magnetic-field-uncalibrated, " \
     "orientation, " \
     "pressure, " \
     "proximity, " \
     "relative-humidity, " \
     "rotation-vector, " \
     "significant-motion, " \
     "step-counter, " \
     "step-detector" \
   "}"

#define GST_SENSOR_CAPS_MAKE(format)                             \
    "application/sensor, " \
    "type = (string) " format

typedef struct GstAHSAccelerometerValues
{
  gfloat x;
  gfloat y;
  gfloat z;
} GstAHSAccelerometerValues;

typedef struct GstAHSAmbientTemperatureValues
{
  gfloat temperature;
} GstAHSAmbientTemperatureValues;

typedef struct GstAHSGameRotationVectorValues
{
  gfloat x;
  gfloat y;
  gfloat z;
  gfloat cos;
  gfloat accuracy;
} GstAHSGameRotationVectorValues;

typedef struct GstAHSGeomagneticRotationVectorValues
{
  gfloat x;
  gfloat y;
  gfloat z;
  gfloat cos;
  gfloat accuracy;
} GstAHSGeomagneticRotationVectorValues;

typedef struct GstAHSGravityValues
{
  gfloat x;
  gfloat y;
  gfloat z;
} GstAHSGravityValues;

typedef struct GstAHSGyroscopeValues
{
  gfloat x;
  gfloat y;
  gfloat z;
} GstAHSGyroscopeValues;

typedef struct GstAHSGyroscopeUncalibratedValues
{
  gfloat x_speed;
  gfloat y_speed;
  gfloat z_speed;
  gfloat x_drift;
  gfloat y_drift;
  gfloat z_drift;
} GstAHSGyroscopeUncalibratedValues;

typedef struct GstAHSHeartRateValues
{
  gfloat bpm;
} GstAHSHeartRateValues;

typedef struct GstAHSLightValues
{
  gfloat lux;
} GstAHSLightValues;

typedef struct GstAHSLinearAccelerationValues
{
  gfloat x;
  gfloat y;
  gfloat z;
} GstAHSLinearAccelerationValues;

typedef struct GstAHSMagneticFieldValues
{
  gfloat x;
  gfloat y;
  gfloat z;
} GstAHSMagneticFieldValues;

typedef struct GstAHSMagneticFieldUncalibratedValues
{
  gfloat x_uncalib;
  gfloat y_uncalib;
  gfloat z_uncalib;
  gfloat x_bias;
  gfloat y_bias;
  gfloat z_bias;
} GstAHSMagneticFieldUncalibratedValues;

typedef struct GstAHSOrientationValues
{
  gfloat azimuth;
  gfloat pitch;
  gfloat roll;
} GstAHSOrientationValues;

typedef struct GstAHSProximity
{
  gfloat distance;
} GstAHSProximityValues;

typedef struct GstAHSPressureValues
{
  gfloat pressure;
} GstAHSPressureValues;

typedef struct GstAHSRelativeHumidityValues
{
  gfloat humidity;
} GstAHSRelativeHumidityValues;

typedef struct GstAHSRotationVectorValues
{
  gfloat x;
  gfloat y;
  gfloat z;
  gfloat cos;
  gfloat accuracy;
} GstAHSRotationVectorValues;

typedef struct GstAHSStepCounterValues
{
  gfloat count;
} GstAHSStepCounterValues;

typedef struct GstAHSStepDetectorValues
{
  gfloat one;
} GstAHSStepDetectorValues;

G_END_DECLS
#endif
