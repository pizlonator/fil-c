/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmfconfig.h"

#include "gstmfsourceobject.h"

#if GST_MF_WINAPI_APP
#include "gstmfcapturewinrt.h"
#endif
#if GST_MF_WINAPI_DESKTOP
#include "gstmfsourcereader.h"
#endif

GST_DEBUG_CATEGORY_EXTERN (gst_mf_source_object_debug);
#define GST_CAT_DEFAULT gst_mf_source_object_debug

enum
{
  PROP_0,
  PROP_DEVICE_PATH,
  PROP_DEVICE_NAME,
  PROP_DEVICE_INDEX,
  PROP_SOURCE_TYPE,
};

#define DEFAULT_DEVICE_PATH         nullptr
#define DEFAULT_DEVICE_NAME         nullptr
#define DEFAULT_DEVICE_INDEX        -1
#define DEFAULT_SOURCE_TYPE        GST_MF_SOURCE_TYPE_VIDEO

GType
gst_mf_source_type_get_type (void)
{
  static GType source_type = 0;

  static const GEnumValue source_types[] = {
    {GST_MF_SOURCE_TYPE_VIDEO, "Video", "video"},
    {0, nullptr, nullptr}
  };

  if (!source_type) {
    source_type = g_enum_register_static ("GstMFSourceMode", source_types);
  }

  return source_type;
}

static void gst_mf_source_object_finalize (GObject * object);
static void gst_mf_source_object_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_mf_source_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

#define gst_mf_source_object_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstMFSourceObject, gst_mf_source_object,
    GST_TYPE_OBJECT);

static void
gst_mf_source_object_class_init (GstMFSourceObjectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamFlags flags =
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS);

  gobject_class->finalize = gst_mf_source_object_finalize;
  gobject_class->get_property = gst_mf_source_object_get_property;
  gobject_class->set_property = gst_mf_source_object_set_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE_PATH,
      g_param_spec_string ("device-path", "Device Path",
          "The device path", DEFAULT_DEVICE_PATH, flags));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device Name",
          "The human-readable device name", DEFAULT_DEVICE_NAME, flags));
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device Index",
          "The zero-based device index", -1, G_MAXINT, DEFAULT_DEVICE_INDEX,
          flags));
  g_object_class_install_property (gobject_class, PROP_SOURCE_TYPE,
      g_param_spec_enum ("source-type", "Source Type",
          "Source Type", GST_TYPE_MF_SOURCE_TYPE, DEFAULT_SOURCE_TYPE, flags));
}

static void
gst_mf_source_object_init (GstMFSourceObject * self)
{
  self->device_index = DEFAULT_DEVICE_INDEX;
  self->source_type = DEFAULT_SOURCE_TYPE;
  self->source_state = GST_MF_DEVICE_NOT_FOUND;

  g_weak_ref_init (&self->client, nullptr);
}

static void
gst_mf_source_object_finalize (GObject * object)
{
  GstMFSourceObject *self = GST_MF_SOURCE_OBJECT (object);

  g_free (self->device_path);
  g_free (self->device_name);

  g_weak_ref_clear (&self->client);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mf_source_object_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFSourceObject *self = GST_MF_SOURCE_OBJECT (object);

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      g_value_set_string (value, self->device_path);
      break;
    case PROP_DEVICE_NAME:
      g_value_set_string (value, self->device_name);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, self->device_index);
      break;
    case PROP_SOURCE_TYPE:
      g_value_set_enum (value, self->source_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_source_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFSourceObject *self = GST_MF_SOURCE_OBJECT (object);

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      g_free (self->device_path);
      self->device_path = g_value_dup_string (value);
      break;
    case PROP_DEVICE_NAME:
      g_free (self->device_name);
      self->device_name = g_value_dup_string (value);
      break;
    case PROP_DEVICE_INDEX:
      self->device_index = g_value_get_int (value);
      break;
    case PROP_SOURCE_TYPE:
      self->source_type = (GstMFSourceType) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_mf_source_object_start (GstMFSourceObject * object)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), FALSE);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->start != nullptr);

  return klass->start (object);
}

gboolean
gst_mf_source_object_stop (GstMFSourceObject * object)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), FALSE);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->stop != nullptr);

  return klass->stop (object);
}

GstFlowReturn
gst_mf_source_object_fill (GstMFSourceObject * object, GstBuffer * buffer)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->fill != nullptr);

  return klass->fill (object, buffer);
}

GstFlowReturn
gst_mf_source_object_create (GstMFSourceObject * object, GstBuffer ** buffer)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), GST_FLOW_ERROR);
  g_return_val_if_fail (buffer != nullptr, GST_FLOW_ERROR);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->create != nullptr);

  return klass->create (object, buffer);
}

GstFlowReturn
gst_mf_source_object_get_sample (GstMFSourceObject * object,
    GstSample ** sample)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), GST_FLOW_ERROR);
  g_return_val_if_fail (sample != nullptr, GST_FLOW_ERROR);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->get_sample != nullptr);

  return klass->get_sample (object, sample);
}

void
gst_mf_source_object_set_flushing (GstMFSourceObject * object,
    gboolean flushing)
{
  GstMFSourceObjectClass *klass;

  g_return_if_fail (GST_IS_MF_SOURCE_OBJECT (object));

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);

  if (flushing) {
    if (klass->unlock)
      klass->unlock (object);
  } else {
    if (klass->unlock_stop)
      klass->unlock_stop (object);
  }
}

gboolean
gst_mf_source_object_set_caps (GstMFSourceObject * object, GstCaps * caps)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), FALSE);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->set_caps != nullptr);

  return klass->set_caps (object, caps);
}

GstCaps *
gst_mf_source_object_get_caps (GstMFSourceObject * object)
{
  GstMFSourceObjectClass *klass;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), nullptr);

  klass = GST_MF_SOURCE_OBJECT_GET_CLASS (object);
  g_assert (klass->get_caps != nullptr);

  return klass->get_caps (object);
}

gboolean
gst_mf_source_object_set_client (GstMFSourceObject * object,
    GstElement * client)
{
  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), FALSE);

  g_weak_ref_set (&object->client, client);

  return TRUE;
}

GstClockTime
gst_mf_source_object_get_running_time (GstMFSourceObject * object)
{
  GstElement *client = nullptr;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;

  g_return_val_if_fail (GST_IS_MF_SOURCE_OBJECT (object), GST_CLOCK_TIME_NONE);

  client = (GstElement *) g_weak_ref_get (&object->client);
  if (client) {
    GstClockTime basetime = client->base_time;
    GstClock *clock;

    clock = gst_element_get_clock (client);
    if (clock) {
      GstClockTime now;

      now = gst_clock_get_time (clock);
      timestamp = now - basetime;
      gst_object_unref (clock);
    }

    gst_object_unref (client);
  }

  return timestamp;
}

static gboolean
gst_mf_source_object_use_winrt_api (void)
{
  static gsize check_once = 0;
  static gboolean ret = FALSE;

  if (g_once_init_enter (&check_once)) {
#if (!GST_MF_WINAPI_APP)
    /* WinRT is not supported, always false */
    ret = FALSE;
#else
#if (!GST_MF_WINAPI_DESKTOP)
    /* WinRT is supported but desktop API was disabled,
     * always true */
    ret = TRUE;
#else
    /* Both app and desktop APIs were enabled, check user choice */
    {
      const gchar *env;

      env = g_getenv ("GST_USE_MF_WINRT_CAPTURE");
      if (env && g_str_has_prefix (env, "1"))
        ret = TRUE;
      else
        ret = FALSE;
    }
#endif
#endif
    g_once_init_leave (&check_once, 1);
  }

  return ret;
}

GstMFSourceObject *
gst_mf_source_object_new (GstMFSourceType type, gint device_index,
    const gchar * device_name, const gchar * device_path, gpointer dispatcher)
{
#if (!GST_MF_WINAPI_APP)
  GST_INFO ("Try IMFSourceReader implementation");
  return gst_mf_source_reader_new (type,
      device_index, device_name, device_path);
#else
#if (!GST_MF_WINAPI_DESKTOP)
  GST_INFO ("Try WinRT implementation");
  return gst_mf_capture_winrt_new (type,
      device_index, device_name, device_path, dispatcher);
#else
  if (gst_mf_source_object_use_winrt_api ()) {
    GST_INFO ("Both Desktop and WinRT APIs were enabled, user choice: WinRT");
    return gst_mf_capture_winrt_new (type,
        device_index, device_name, device_path, dispatcher);
  } else {
    GST_INFO
        ("Both Desktop and WinRT APIs were enabled, default: IMFSourceReader");
    return gst_mf_source_reader_new (type,
        device_index, device_name, device_path);
  }
#endif
#endif
  g_assert_not_reached ();

  return nullptr;
}

GstMFSourceResult
gst_mf_source_object_enumerate (gint device_index, GstMFSourceObject ** object)
{
#if (!GST_MF_WINAPI_APP)
  return gst_mf_source_reader_enumerate (device_index, object);
#else
#if (!GST_MF_WINAPI_DESKTOP)
  return gst_mf_capture_winrt_enumerate (device_index, object);
#else
  if (gst_mf_source_object_use_winrt_api ())
    return gst_mf_capture_winrt_enumerate (device_index, object);

  return gst_mf_source_reader_enumerate (device_index, object);
#endif
#endif
  g_assert_not_reached ();

  return GST_MF_DEVICE_NOT_FOUND;
}

gint
gst_mf_source_object_caps_compare (GstCaps * caps1, GstCaps * caps2)
{
  GstStructure *s1, *s2;
  const gchar *n1, *n2;
  gboolean m1_is_raw, m2_is_raw;
  gint w1 = 0, h1 = 0, w2 = 0, h2 = 0;
  gint r1, r2;
  gint num1 = 0, den1 = 1, num2 = 0, den2 = 1;
  gint fraction_cmp;

  /* sorting priority
   * - raw video > comprssed
   *   - raw video format
   * - higher resolution
   * - higher framerate
   */
  s1 = gst_caps_get_structure (caps1, 0);
  n1 = gst_structure_get_name (s1);

  s2 = gst_caps_get_structure (caps2, 0);
  n2 = gst_structure_get_name (s2);

  m1_is_raw = g_strcmp0 (n1, "video/x-raw") == 0;
  m2_is_raw = g_strcmp0 (n2, "video/x-raw") == 0;

  if (m1_is_raw && !m2_is_raw)
    return -1;
  else if (!m1_is_raw && m2_is_raw)
    return 1;

  /* if both are raw formats */
  if (m1_is_raw) {
    gint format_cmp = g_strcmp0 (gst_structure_get_string (s1, "format"),
        gst_structure_get_string (s2, "format"));
    if (format_cmp)
      return format_cmp;
  }

  /* resolution */
  gst_structure_get_int (s1, "width", &w1);
  gst_structure_get_int (s1, "height", &h1);
  gst_structure_get_int (s2, "width", &w2);
  gst_structure_get_int (s2, "height", &h2);

  r1 = w1 * h1;
  r2 = w2 * h2;

  /* higher resolution first */
  if (r1 != r2)
    return r2 - r1;

  gst_structure_get_fraction (s1, "framerate", &num1, &den1);
  gst_structure_get_fraction (s2, "framerate", &num2, &den2);

  fraction_cmp = gst_util_fraction_compare (num1, den1, num2, den2);

  /* higher framerate first */
  return fraction_cmp * -1;
}
