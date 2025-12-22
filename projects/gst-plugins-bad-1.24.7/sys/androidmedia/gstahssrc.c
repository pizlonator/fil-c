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
/**
 * SECTION:element-ahssrc
 * @title: gstahssrc
 *
 * The ahssrc element reads data from Android device sensors
 * (android.hardware.Sensor).
 *
 * ## Example launch line
 * |[
 * gst-launch -v ahssrc ! fakesink
 * ]|
 * Push Android sensor data into a fakesink.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/gst.h>
#include <gst/gstclock.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>
#include "gstjniutils.h"
#include "gst-android-hardware-sensor.h"
#include "gstahssrc.h"
#include "gstsensors.h"

GST_DEBUG_CATEGORY_STATIC (gst_ahs_src_debug);
#define GST_CAT_DEFAULT gst_ahs_src_debug

#define parent_class gst_ahs_src_parent_class

/* GObject */
static void gst_ahs_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ahs_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_ahs_src_dispose (GObject * object);

/* GstBaseSrc */
static gboolean gst_ahs_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_ahs_src_start (GstBaseSrc * src);
static gboolean gst_ahs_src_stop (GstBaseSrc * src);
static gboolean gst_ahs_src_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_ahs_src_is_seekable (GstBaseSrc * src);
static gboolean gst_ahs_src_unlock (GstBaseSrc * src);
static gboolean gst_ahs_src_unlock_stop (GstBaseSrc * src);

/* GstPushSrc */
static GstFlowReturn gst_ahs_src_create (GstPushSrc * src, GstBuffer ** buf);

/* GstAHSSrc */
static void gst_ahs_src_on_sensor_changed (jobject sensor_event,
    gpointer user_data);
static void gst_ahs_src_on_accuracy_changed (jobject sensor, gint accuracy,
    gpointer user_data);
static gboolean gst_ahs_src_register_callback (GstAHSSrc * self);

enum
{
  PROP_0,
  PROP_SENSOR_DELAY,
  PROP_ALPHA,
  PROP_SAMPLE_INTERVAL,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

#define GST_AHS_SRC_CAPS_STR GST_SENSOR_CAPS_MAKE (GST_SENSOR_FORMATS_ALL)

static GstStaticPadTemplate gst_ahs_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AHS_SRC_CAPS_STR));


G_DEFINE_TYPE_WITH_CODE (GstAHSSrc, gst_ahs_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_ahs_src_debug, "ahssrc", 0,
        "Android hardware sensors"));

#define GST_TYPE_AHS_SENSOR_DELAY (gst_ahs_src_get_sensor_delay ())
static GType
gst_ahs_src_get_sensor_delay (void)
{
  static GType ahs_src_sensor_delay = 0;

  if (!ahs_src_sensor_delay) {
    static GEnumValue sensor_delay[5];
    sensor_delay[0].value = AHS_SENSOR_DELAY_FASTEST;
    sensor_delay[0].value_name = "fastest";
    sensor_delay[0].value_nick = "fastest";
    sensor_delay[1].value = AHS_SENSOR_DELAY_GAME;
    sensor_delay[1].value_name = "game";
    sensor_delay[1].value_nick = "game";
    sensor_delay[2].value = AHS_SENSOR_DELAY_NORMAL;
    sensor_delay[2].value_name = "normal";
    sensor_delay[2].value_nick = "normal";
    sensor_delay[3].value = AHS_SENSOR_DELAY_UI;
    sensor_delay[3].value_name = "ui";
    sensor_delay[3].value_nick = "ui";
    sensor_delay[4].value = 0;
    sensor_delay[4].value_name = NULL;
    sensor_delay[4].value_nick = NULL;

    ahs_src_sensor_delay =
        g_enum_register_static ("GstAhsSrcSensorDelay", sensor_delay);
  }

  return ahs_src_sensor_delay;
}

#define GST_TYPE_AHS_SENSOR_TYPE (gst_ahs_src_get_sensor_type ())
static GType
gst_ahs_src_get_sensor_type (void)
{
  static GType ahs_src_sensor_type = 0;

  if (!ahs_src_sensor_type) {
    static const GEnumValue sensor_types[] = {
      {AHS_SENSOR_TYPE_ACCELEROMETER, "accelerometer"},
      {AHS_SENSOR_TYPE_AMBIENT_TEMPERATURE, "ambient-temperature"},
      {AHS_SENSOR_TYPE_GAME_ROTATION_VECTOR, "game-rotation-vector"},
      {AHS_SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
          "geomagnetic-rotation-vector"},
      {AHS_SENSOR_TYPE_GRAVITY, "gravity"},
      {AHS_SENSOR_TYPE_GYROSCOPE, "gyroscope"},
      {AHS_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED, "gyroscope-uncalibrated"},
      {AHS_SENSOR_TYPE_HEART_RATE, "heart-rate"},
      {AHS_SENSOR_TYPE_LIGHT, "light"},
      {AHS_SENSOR_TYPE_LINEAR_ACCELERATION, "linear-acceleration"},
      {AHS_SENSOR_TYPE_MAGNETIC_FIELD, "magnetic-field"},
      {AHS_SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
          "magnetic-field-uncalibrated"},
      {AHS_SENSOR_TYPE_ORIENTATION, "orientation"},
      {AHS_SENSOR_TYPE_PRESSURE, "pressure"},
      {AHS_SENSOR_TYPE_PROXIMITY, "proximity"},
      {AHS_SENSOR_TYPE_RELATIVE_HUMIDITY, "relative-humidity"},
      {AHS_SENSOR_TYPE_ROTATION_VECTOR, "rotation-vector"},
      {AHS_SENSOR_TYPE_STEP_COUNTER, "step-counter"},
      {AHS_SENSOR_TYPE_STEP_DETECTOR, "step-detector"},
      {0, NULL, NULL}
    };

    ahs_src_sensor_type =
        g_enum_register_static ("GstAhsSrcSensorType", sensor_types);
  }

  return ahs_src_sensor_type;
}

static void
gst_ahs_src_class_init (GstAHSSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_ahs_src_set_property;
  gobject_class->get_property = gst_ahs_src_get_property;
  gobject_class->dispose = gst_ahs_src_dispose;

  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_ahs_src_set_caps);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_ahs_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_ahs_src_stop);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_ahs_src_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_ahs_src_is_seekable);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_ahs_src_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_ahs_src_unlock_stop);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_ahs_src_create);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_ahs_src_template));

  properties[PROP_SENSOR_DELAY] = g_param_spec_enum ("sensor-delay",
      "Sensor delay", "Configure the sensor rate", GST_TYPE_AHS_SENSOR_DELAY,
      AHS_SENSOR_DELAY_NORMAL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SENSOR_DELAY,
      properties[PROP_SENSOR_DELAY]);

  properties[PROP_ALPHA] = g_param_spec_double ("alpha", "Alpha",
      "Alpha value used for exponential smoothing (between 0.0 and 1.0)", 0.0,
      1.0, 0.2, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_ALPHA,
      properties[PROP_ALPHA]);

  properties[PROP_SAMPLE_INTERVAL] = g_param_spec_uint ("sample-interval",
      "Sample interval",
      "Sample interval (for interval n, will output a smoothed average every "
      "nth sample)", 1, G_MAXUINT, 1,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SAMPLE_INTERVAL,
      properties[PROP_SAMPLE_INTERVAL]);

  gst_element_class_set_static_metadata (element_class,
      "Android hardware sensors", "Source/Sensor/Device",
      "Source for Android hardware sensor data",
      "Martin Kelly <martin@surround.io>");
}

static gboolean
_data_queue_check_full (GstDataQueue * queue, guint visible,
    guint bytes, guint64 time, gpointer checkdata)
{
  return FALSE;
}

static void
gst_ahs_src_init (GstAHSSrc * self)
{
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (self), FALSE);

  self->sensor_enum_class = g_type_class_ref (GST_TYPE_AHS_SENSOR_TYPE);
  self->sensor_type_name = NULL;

  self->manager = NULL;
  self->sensor = NULL;
  self->listener = NULL;
  self->callback_registered = FALSE;

  self->queue = gst_data_queue_new (_data_queue_check_full, NULL, NULL, NULL);

  self->previous_time = GST_CLOCK_TIME_NONE;
  self->sample_index = 0;
  self->current_sample = NULL;
}

static void
gst_ahs_src_dispose (GObject * object)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GstAHSSrc *self = GST_AHS_SRC (object);

  if (self->manager) {
    gst_amc_jni_object_unref (env, self->manager->object);
    g_slice_free (GstAHSensorManager, self->manager);
    self->manager = NULL;
  }

  if (self->sensor) {
    gst_amc_jni_object_unref (env, self->sensor->object);
    g_slice_free (GstAHSensor, self->sensor);
    self->sensor = NULL;
  }

  if (self->listener) {
    gst_amc_jni_object_unref (env, self->listener->object);
    g_slice_free (GstAHSensorEventListener, self->listener);
    self->listener = NULL;
  }

  if (self->current_sample) {
    g_free (self->current_sample);
    self->current_sample = NULL;
  }

  if (self->sensor_enum_class) {
    g_type_class_unref (self->sensor_enum_class);
    self->sensor_enum_class = NULL;
  }

  if (self->sensor_type_name) {
    g_free ((gpointer) self->sensor_type_name);
    self->sensor_type_name = NULL;
  }

  if (self->queue) {
    g_object_unref (self->queue);
    self->queue = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_ahs_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAHSSrc *self = GST_AHS_SRC (object);

  /*
   * Take the mutex to protect against callbacks or changes to the properties
   * that the callback uses (e.g. caps changes).
   */
  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_SENSOR_DELAY:
      self->sensor_delay = g_value_get_enum (value);
      /*
       * If we already have a callback running, reregister with the new delay.
       * Otherwise, wait for the pipeline to start before we register.
       */
      if (self->callback_registered)
        gst_ahs_src_register_callback (self);
      break;
    case PROP_ALPHA:
      self->alpha = g_value_get_double (value);
      break;
    case PROP_SAMPLE_INTERVAL:
      self->sample_interval = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_ahs_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAHSSrc *self = GST_AHS_SRC (object);

  switch (prop_id) {
    case PROP_SENSOR_DELAY:
      g_value_set_enum (value, self->sensor_delay);
    case PROP_ALPHA:
      g_value_set_double (value, self->alpha);
      break;
    case PROP_SAMPLE_INTERVAL:
      g_value_set_uint (value, self->sample_interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
gst_ahs_src_register_callback (GstAHSSrc * self)
{
  if (self->callback_registered) {
    gst_ah_sensor_unregister_listener (self->manager, self->listener);
    self->callback_registered = FALSE;
  }
  if (!gst_ah_sensor_register_listener (self->manager, self->listener,
          self->sensor, self->sensor_delay)) {
    return FALSE;
  }
  self->callback_registered = TRUE;

  return TRUE;
}

static gboolean
gst_ahs_src_change_sensor_type (GstAHSSrc * self, const gchar * type_str,
    gint type)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  /* Replace sensor type. */
  if (self->sensor_type_name)
    g_free ((gpointer) self->sensor_type_name);
  self->sensor_type_name = type_str;
  self->sensor_type = type;

  /* Adjust buffer and buffer size. */
  self->buffer_size = gst_ah_sensor_get_sensor_data_size (self->sensor_type);
  g_assert (self->buffer_size != 0);
  self->sample_length = self->buffer_size / sizeof (*self->current_sample);
  self->current_sample = g_realloc (self->current_sample, self->buffer_size);

  /* Make sure we have a manager. */
  if (!self->manager) {
    self->manager = gst_ah_sensor_get_manager ();
    if (!self->manager) {
      GST_ERROR_OBJECT (self, "Failed to get sensor manager");
      goto error_sensor_type_name;
    }
  }

  /* Replace sensor object. */
  if (self->sensor) {
    gst_amc_jni_object_unref (env, self->sensor->object);
    g_slice_free (GstAHSensor, self->sensor);
  }
  self->sensor = gst_ah_sensor_get_default_sensor (self->manager,
      self->sensor_type);
  if (!self->sensor) {
    GST_ERROR_OBJECT (self, "Failed to get sensor type %s",
        self->sensor_type_name);
    goto error_manager;
  }

  /* Register for the callback, unregistering first if necessary. */
  if (!gst_ahs_src_register_callback (self))
    goto error_sensor;

  return TRUE;

error_sensor:
  gst_amc_jni_object_unref (env, self->sensor->object);
  g_slice_free (GstAHSensor, self->sensor);
  self->sensor = NULL;
error_manager:
  gst_amc_jni_object_unref (env, self->manager->object);
  g_slice_free (GstAHSensorManager, self->manager);
  self->manager = NULL;
error_sensor_type_name:
  g_free ((gpointer) self->sensor_type_name);
  self->sensor_type_name = NULL;
  return FALSE;
}

static gboolean
gst_ahs_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  const GstStructure *caps_struct;
  GstAHSSrc *self = GST_AHS_SRC (src);
  gboolean success;
  gint type;
  const gchar *type_str;
  GEnumValue *value;

  caps_struct = gst_caps_get_structure (caps, 0);
  type_str = gst_structure_get_string (caps_struct, "type");
  value = g_enum_get_value_by_name (self->sensor_enum_class, type_str);
  if (!value) {
    GST_ERROR_OBJECT (self, "Failed to lookup sensor type %s", type_str);
    return FALSE;
  }
  type_str = g_strdup (type_str);
  type = value->value;

  /*
   * Take the mutex while changing the sensor type in case there are concurrent
   * callbacks being processed.
   */
  GST_OBJECT_LOCK (self);
  success = gst_ahs_src_change_sensor_type (self, type_str, type);
  GST_OBJECT_UNLOCK (self);

  if (!success)
    return FALSE;

  return TRUE;
}

static gboolean
gst_ahs_src_start (GstBaseSrc * src)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GstAHSSrc *self = GST_AHS_SRC (src);

  g_assert_null (self->manager);
  g_assert_null (self->listener);

  self->manager = gst_ah_sensor_get_manager ();
  if (!self->manager) {
    GST_ERROR_OBJECT (self, "Failed to get sensor manager");
    goto error;
  }

  self->previous_time = GST_CLOCK_TIME_NONE;

  self->listener = gst_ah_sensor_create_listener (gst_ahs_src_on_sensor_changed,
      gst_ahs_src_on_accuracy_changed, self);
  if (!self->listener) {
    GST_ERROR_OBJECT (self, "Failed to create sensor listener");
    goto error_manager;
  }

  return TRUE;

error_manager:
  gst_amc_jni_object_unref (env, self->manager->object);
  g_slice_free (GstAHSensorManager, self->manager);
  self->manager = NULL;
error:
  return FALSE;
}

static gboolean
gst_ahs_src_stop (GstBaseSrc * src)
{
  GstAHSSrc *self = GST_AHS_SRC (src);

  g_assert_nonnull (self->manager);
  g_assert_nonnull (self->sensor);
  g_assert_nonnull (self->listener);

  gst_ah_sensor_unregister_listener (self->manager, self->listener);
  self->previous_time = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static gboolean
gst_ahs_src_get_size (GstBaseSrc * src, guint64 * size)
{
  GstAHSSrc *self = GST_AHS_SRC (src);

  return self->buffer_size;
}

static gboolean
gst_ahs_src_is_seekable (GstBaseSrc * src)
{
  return FALSE;
}

static gboolean
gst_ahs_src_unlock (GstBaseSrc * src)
{
  GstAHSSrc *self = GST_AHS_SRC (src);

  gst_data_queue_set_flushing (self->queue, TRUE);

  return TRUE;
}

static gboolean
gst_ahs_src_unlock_stop (GstBaseSrc * src)
{
  GstAHSSrc *self = GST_AHS_SRC (src);

  gst_data_queue_set_flushing (self->queue, FALSE);

  return TRUE;
}

static GstFlowReturn
gst_ahs_src_create (GstPushSrc * src, GstBuffer ** buffer)
{
  GstAHSSrc *self = GST_AHS_SRC (src);
  GstDataQueueItem *item;

  if (!gst_data_queue_pop (self->queue, &item)) {
    GST_INFO_OBJECT (self, "data queue is empty");
    return GST_FLOW_FLUSHING;
  }

  GST_DEBUG_OBJECT (self, "creating buffer %p->%p", item, item->object);

  *buffer = GST_BUFFER (item->object);
  g_slice_free (GstDataQueueItem, item);

  return GST_FLOW_OK;
}

static void
gst_ahs_src_free_data_queue_item (GstDataQueueItem * item)
{
  gst_buffer_unref (GST_BUFFER (item->object));
  g_slice_free (GstDataQueueItem, item);
}

static void
gst_ahs_src_update_smoothing (GstAHSSrc * self, const GstAHSensorEvent * event)
{
  gint i;

  /*
   * Since we're doing exponential smoothing, the first sample needs to be
   * special-cased to prevent it from being artificially lowered by the alpha
   * smoothing factor.
   */
  if (self->sample_index == 0) {
    for (i = 0; i < self->sample_length; i++) {
      self->current_sample[i] = event->data.values[i];
    }
  } else {
    for (i = 0; i < self->sample_length; i++)
      self->current_sample[i] =
          (1 - self->alpha) * self->current_sample[i] +
          self->alpha * event->data.values[i];
  }
}

static void
gst_ahs_src_on_sensor_changed (jobject event_object, gpointer user_data)
{
  GstBuffer *buffer;
  GstClockTime buffer_time;
  gfloat *data;
  GstAHSensorEvent event;
  GstDataQueueItem *item;
  GstClock *pipeline_clock;
  GstAHSSrc *self = GST_AHS_SRC (user_data);
  gboolean success;

  GST_OBJECT_LOCK (self);

  pipeline_clock = GST_ELEMENT_CLOCK (self);
  /* If the clock is NULL, the pipeline is not yet set to PLAYING. */
  if (pipeline_clock == NULL)
    goto done;

  /*
   * Unfortunately, the timestamp reported in the Android SensorEvent timestamp
   * is not guaranteed to use any particular clock. On some device models, it
   * uses system time, and on other models, it uses monotonic time. In addition,
   * in some cases, the units are microseconds, and in other cases they are
   * nanoseconds. Thus we cannot slave it to the pipeline clock or use any
   * similar strategy that would allow us to correlate the two clocks. So
   * instead, we approximate the buffer timestamp using the pipeline clock.
   *
   * See here for more details on issues with the Android SensorEvent timestamp:
   * https://code.google.com/p/android/issues/detail?id=7981
   */
  buffer_time =
      gst_clock_get_time (pipeline_clock) - GST_ELEMENT_CAST (self)->base_time;

  success =
      gst_ah_sensor_populate_event (&event, event_object, self->buffer_size);
  if (!success) {
    GST_ERROR_OBJECT (self, "Failed to populate sensor event");
    goto done;
  }

  gst_ahs_src_update_smoothing (self, &event);
  gst_ah_sensor_free_sensor_data (&event.data);
  self->sample_index++;
  if (self->sample_index < self->sample_interval)
    goto done;
  self->sample_index = 0;

  /*
   * We want to send off this sample; copy it into a separate data struct so we
   * can continue using current_sample for aggregating future samples.
   */
  data = g_malloc (self->buffer_size);
  memcpy (data, self->current_sample, self->buffer_size);

  /* Wrap the datapoint with a buffer and add it to the queue. */
  buffer = gst_buffer_new_wrapped (data, self->buffer_size);
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_PTS (buffer) = buffer_time;

  item = g_slice_new (GstDataQueueItem);
  item->object = GST_MINI_OBJECT (buffer);
  item->size = gst_buffer_get_size (buffer);
  item->duration = GST_BUFFER_DURATION (buffer);
  item->visible = TRUE;
  item->destroy = (GDestroyNotify) gst_ahs_src_free_data_queue_item;

  success = gst_data_queue_push (self->queue, item);
  if (!success) {
    GST_ERROR_OBJECT (self, "Could not add buffer to queue");
    gst_ahs_src_free_data_queue_item (item);
    goto done;
  }

done:
  GST_OBJECT_UNLOCK (self);
}

static void
gst_ahs_src_on_accuracy_changed (jobject sensor, gint accuracy,
    gpointer user_data)
{
  GstAHSSrc *self = GST_AHS_SRC (user_data);

  /* TODO: Perhaps we should do something more with this information. */
  GST_DEBUG_OBJECT (self, "Accuracy changed on sensor %p and is now %d", sensor,
      accuracy);
}
