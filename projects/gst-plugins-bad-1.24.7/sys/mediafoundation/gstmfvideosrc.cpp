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

/**
 * SECTION:element-mfvideosrc
 * @title: mfvideosrc
 *
 * Provides video capture from the Microsoft Media Foundation API.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v mfvideosrc ! fakesink
 * ]| Capture from the default video capture device and render to fakesink.
 *
 * |[
 * gst-launch-1.0 -v mfvideosrc device-index=1 ! fakesink
 * ]| Capture from the second video device (if available) and render to fakesink.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmfconfig.h"

#include "gstmfvideosrc.h"
#include "gstmfutils.h"
#include "gstmfsourceobject.h"
#include <string.h>

#if GST_MF_WINAPI_DESKTOP
#include "gstmfcapturedshow.h"
#endif

GST_DEBUG_CATEGORY (gst_mf_video_src_debug);
#define GST_CAT_DEFAULT gst_mf_video_src_debug

#if (GST_MF_WINAPI_APP && !GST_MF_WINAPI_DESKTOP)
/* FIXME: need support JPEG for UWP */
#define SRC_TEMPLATE_CAPS \
    GST_VIDEO_CAPS_MAKE (GST_MF_VIDEO_FORMATS)
#else
#define SRC_TEMPLATE_CAPS \
    GST_VIDEO_CAPS_MAKE (GST_MF_VIDEO_FORMATS) "; " \
        "image/jpeg, width = " GST_VIDEO_SIZE_RANGE ", " \
        "height = " GST_VIDEO_SIZE_RANGE ", " \
        "framerate = " GST_VIDEO_FPS_RANGE
#endif

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_TEMPLATE_CAPS));

struct _GstMFVideoSrc
{
  GstPushSrc parent;

  GstMFSourceObject *source;
  gboolean started;
  GstVideoInfo info;

  guint64 n_frames;
  GstClockTime latency;

  gboolean use_dshow;

  /* properties */
  gchar *device_path;
  gchar *device_name;
  gint device_index;
  gpointer dispatcher;
};

enum
{
  PROP_0,
  PROP_DEVICE_PATH,
  PROP_DEVICE_NAME,
  PROP_DEVICE_INDEX,
  PROP_DISPATCHER,
};

#define DEFAULT_DEVICE_PATH     nullptr
#define DEFAULT_DEVICE_NAME     nullptr
#define DEFAULT_DEVICE_INDEX    -1

static void gst_mf_video_src_finalize (GObject * object);
static void gst_mf_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_mf_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_mf_video_src_start (GstBaseSrc * src);
static gboolean gst_mf_video_src_stop (GstBaseSrc * src);
static gboolean gst_mf_video_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static GstCaps *gst_mf_video_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static GstCaps *gst_mf_video_src_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_mf_video_src_unlock (GstBaseSrc * src);
static gboolean gst_mf_video_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_mf_video_src_query (GstBaseSrc * src, GstQuery * query);

static GstFlowReturn gst_mf_video_src_create (GstPushSrc * pushsrc,
    GstBuffer ** buffer);

#define gst_mf_video_src_parent_class parent_class
G_DEFINE_TYPE (GstMFVideoSrc, gst_mf_video_src, GST_TYPE_PUSH_SRC);

static void
gst_mf_video_src_class_init (GstMFVideoSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE |
      GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS);

  gobject_class->finalize = gst_mf_video_src_finalize;
  gobject_class->get_property = gst_mf_video_src_get_property;
  gobject_class->set_property = gst_mf_video_src_set_property;

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
#if GST_MF_WINAPI_APP
  /**
   * GstMFVideoSrc:dispatcher:
   *
   * ICoreDispatcher COM object used for activating device from UI thread.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_DISPATCHER,
      g_param_spec_pointer ("dispatcher", "Dispatcher",
          "ICoreDispatcher COM object to use. In order for application to ask "
          "permission of capture device, device activation should be running "
          "on UI thread via ICoreDispatcher. This element will increase "
          "the reference count of given ICoreDispatcher and release it after "
          "use. Therefore, caller does not need to consider additional "
          "reference count management",
          (GParamFlags) (GST_PARAM_CONDITIONALLY_AVAILABLE |
              GST_PARAM_MUTABLE_READY | G_PARAM_WRITABLE |
              G_PARAM_STATIC_STRINGS)));
#endif

  gst_element_class_set_static_metadata (element_class,
      "Media Foundation Video Source",
      "Source/Video/Hardware",
      "Capture video stream through Windows Media Foundation",
      "Seungha Yang <seungha.yang@navercorp.com>");

  gst_element_class_add_static_pad_template (element_class, &src_template);

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_mf_video_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_mf_video_src_stop);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_mf_video_src_set_caps);
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_mf_video_src_get_caps);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_mf_video_src_fixate);
  basesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_mf_video_src_unlock);
  basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_mf_video_src_unlock_stop);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_mf_video_src_query);

  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_mf_video_src_create);

  GST_DEBUG_CATEGORY_INIT (gst_mf_video_src_debug, "mfvideosrc", 0,
      "mfvideosrc");
}

static void
gst_mf_video_src_init (GstMFVideoSrc * self)
{
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);

  self->device_index = DEFAULT_DEVICE_INDEX;
}

static void
gst_mf_video_src_finalize (GObject * object)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (object);

  g_free (self->device_name);
  g_free (self->device_path);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mf_video_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (object);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (object);

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
#if GST_MF_WINAPI_APP
    case PROP_DISPATCHER:
      self->dispatcher = g_value_get_pointer (value);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_mf_video_src_start (GstBaseSrc * src)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Start");

  self->source = gst_mf_source_object_new (GST_MF_SOURCE_TYPE_VIDEO,
      self->device_index, self->device_name, self->device_path, nullptr);

  self->n_frames = 0;
  self->latency = 0;
  self->use_dshow = FALSE;

  if (!self->source) {
#if GST_MF_WINAPI_DESKTOP
    self->use_dshow = TRUE;
    self->source = gst_mf_capture_dshow_new (GST_MF_SOURCE_TYPE_VIDEO,
        self->device_index, self->device_name, self->device_path);
#endif
  }

  if (!self->source) {
    GST_ERROR_OBJECT (self, "Couldn't create capture object");
    return FALSE;
  }

  gst_mf_source_object_set_client (self->source, GST_ELEMENT (self));

  return TRUE;
}

static gboolean
gst_mf_video_src_stop (GstBaseSrc * src)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Stop");

  if (self->source) {
    gst_mf_source_object_stop (self->source);
    gst_object_unref (self->source);
    self->source = nullptr;
  }

  self->started = FALSE;

  return TRUE;
}

static gboolean
gst_mf_video_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Set caps %" GST_PTR_FORMAT, caps);

  if (!self->source) {
    GST_ERROR_OBJECT (self, "No capture engine yet");
    return FALSE;
  }

  if (!gst_mf_source_object_set_caps (self->source, caps)) {
    GST_ERROR_OBJECT (self, "CaptureEngine couldn't accept caps");
    return FALSE;
  }

  gst_video_info_from_caps (&self->info, caps);
  if (GST_VIDEO_INFO_FORMAT (&self->info) != GST_VIDEO_FORMAT_ENCODED)
    gst_base_src_set_blocksize (src, GST_VIDEO_INFO_SIZE (&self->info));

  return TRUE;
}

static GstCaps *
gst_mf_video_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);
  GstCaps *caps = nullptr;

  if (self->source)
    caps = gst_mf_source_object_get_caps (self->source);

  if (!caps)
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));

  if (filter) {
    GstCaps *filtered =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = filtered;
  }

  GST_DEBUG_OBJECT (self, "Returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_mf_video_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstStructure *structure;
  GstCaps *fixated_caps;
  gint i;

  fixated_caps = gst_caps_make_writable (caps);

  for (i = 0; i < gst_caps_get_size (fixated_caps); ++i) {
    structure = gst_caps_get_structure (fixated_caps, i);
    gst_structure_fixate_field_nearest_int (structure, "width", G_MAXINT);
    gst_structure_fixate_field_nearest_int (structure, "height", G_MAXINT);
    gst_structure_fixate_field_nearest_fraction (structure, "framerate",
        G_MAXINT, 1);
  }

  fixated_caps = gst_caps_fixate (fixated_caps);

  return fixated_caps;
}

static gboolean
gst_mf_video_src_unlock (GstBaseSrc * src)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  if (self->source)
    gst_mf_source_object_set_flushing (self->source, TRUE);

  return TRUE;
}

static gboolean
gst_mf_video_src_unlock_stop (GstBaseSrc * src)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  if (self->source)
    gst_mf_source_object_set_flushing (self->source, FALSE);

  return TRUE;
}

static gboolean
gst_mf_video_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (src);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
      if (self->started) {
        gst_query_set_latency (query, TRUE, 0, self->latency);

        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (src, query);
}

static GstFlowReturn
gst_mf_video_src_create (GstPushSrc * pushsrc, GstBuffer ** buffer)
{
  GstMFVideoSrc *self = GST_MF_VIDEO_SRC (pushsrc);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf = nullptr;
  GstSample *sample = nullptr;
  GstClock *clock;
  GstClockTime running_time = GST_CLOCK_TIME_NONE;
  GstClockTimeDiff diff;

  if (!self->started) {
    if (!gst_mf_source_object_start (self->source)) {
      GST_ERROR_OBJECT (self, "Failed to start capture object");

      return GST_FLOW_ERROR;
    }

    self->started = TRUE;
  }

  if (self->use_dshow) {
    ret = gst_mf_source_object_get_sample (self->source, &sample);
  } else if (GST_VIDEO_INFO_FORMAT (&self->info) != GST_VIDEO_FORMAT_ENCODED) {
    ret = GST_BASE_SRC_CLASS (parent_class)->alloc (GST_BASE_SRC (self), 0,
        GST_VIDEO_INFO_SIZE (&self->info), &buf);

    if (ret != GST_FLOW_OK) {
      gst_clear_buffer (&buf);
      return ret;
    }

    ret = gst_mf_source_object_fill (self->source, buf);
  } else {
    ret = gst_mf_source_object_create (self->source, &buf);
  }

  if (ret != GST_FLOW_OK) {
    gst_clear_buffer (&buf);
    return ret;
  }

  /* DirectShow capture object will set caps if it's got updated */
  if (sample) {
    if (gst_sample_get_caps (sample)) {
      if (!gst_base_src_negotiate (GST_BASE_SRC (self))) {
        GST_ERROR_OBJECT (self, "Failed to negotiate with new caps");
        gst_sample_unref (sample);
        return GST_FLOW_NOT_NEGOTIATED;
      } else {
        GST_DEBUG_OBJECT (self, "Renegotiated");
      }
    }

    buf = gst_sample_get_buffer (sample);
    gst_buffer_ref (buf);
    gst_sample_unref (sample);
  }

  GST_BUFFER_OFFSET (buf) = self->n_frames;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET (buf) + 1;
  self->n_frames++;

  GST_LOG_OBJECT (self,
      "Captured buffer timestamp %" GST_TIME_FORMAT ", duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

  /* Update latency */
  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  if (clock) {
    GstClockTime now;

    now = gst_clock_get_time (clock);
    running_time = now - GST_ELEMENT_CAST (self)->base_time;
    gst_object_unref (clock);
  }

  diff = GST_CLOCK_DIFF (GST_BUFFER_PTS (buf), running_time);
  if (diff > self->latency) {
    self->latency = (GstClockTime) diff;
    GST_DEBUG_OBJECT (self, "Updated latency value %" GST_TIME_FORMAT,
        GST_TIME_ARGS (self->latency));
  }

  *buffer = buf;

  return GST_FLOW_OK;
}
