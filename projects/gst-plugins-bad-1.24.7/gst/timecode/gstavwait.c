/*
 * GStreamer
 * Copyright (C) 2016 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * Based on gstvideoframe-audiolevel.c:
 * Copyright (C) 2015 Vivia Nikolaidou <vivia@toolsonair.com>
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
 * SECTION:element-avwait
 * @title: avwait
 *
 * This element will drop all buffers until a specific timecode or running
 * time has been reached. It will then pass-through both audio and video,
 * starting from that specific timecode or running time, making sure that
 * audio starts as early as possible after the video (or at the same time as
 * the video). In the "video-first" mode, it only drops audio buffers until
 * video has started.
 *
 * The "recording" property acts essentially like a valve connected before
 * everything else. If recording is FALSE, all buffers are dropped regardless
 * of settings. If recording is TRUE, the other settings (mode,
 * target-timecode, target-running-time, etc) are taken into account. Audio
 * will always start and end together with the video, as long as the stream
 * itself doesn't start too late or end too early.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location="my_file" ! decodebin name=d ! "audio/x-raw" ! avwait name=l target-timecode-str="00:00:04:00" ! autoaudiosink d. ! "video/x-raw" ! timecodestamper ! l. l. ! queue ! timeoverlay time-mode=time-code ! autovideosink
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstavwait.h"

#define GST_CAT_DEFAULT gst_avwait_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("asink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw")
    );

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE ("asrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw")
    );

static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("vsink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("vsrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

#define parent_class gst_avwait_parent_class
G_DEFINE_TYPE (GstAvWait, gst_avwait, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (avwait, "avwait", GST_RANK_NONE, GST_TYPE_AVWAIT);

enum
{
  PROP_0,
  PROP_TARGET_TIME_CODE,
  PROP_TARGET_TIME_CODE_STRING,
  PROP_TARGET_RUNNING_TIME,
  PROP_END_TIME_CODE,
  PROP_END_RUNNING_TIME,
  PROP_RECORDING,
  PROP_MODE
};

#define DEFAULT_TARGET_TIMECODE_STR "00:00:00:00"
#define DEFAULT_TARGET_RUNNING_TIME GST_CLOCK_TIME_NONE
#define DEFAULT_END_RUNNING_TIME GST_CLOCK_TIME_NONE
#define DEFAULT_MODE MODE_TIMECODE

/* flags for self->must_send_end_message */
enum
{
  END_MESSAGE_NORMAL = 0,
  END_MESSAGE_STREAM_ENDED = 1,
  END_MESSAGE_VIDEO_PUSHED = 2,
  END_MESSAGE_AUDIO_PUSHED = 4
};

static void gst_avwait_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_avwait_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_avwait_asink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);
static GstFlowReturn gst_avwait_vsink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);
static gboolean gst_avwait_asink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_avwait_vsink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstIterator *gst_avwait_iterate_internal_links (GstPad *
    pad, GstObject * parent);

static void gst_avwait_finalize (GObject * gobject);

static GstStateChangeReturn gst_avwait_change_state (GstElement *
    element, GstStateChange transition);

static GType
gst_avwait_mode_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {MODE_TIMECODE, "time code (default)", "timecode"},
      {MODE_RUNNING_TIME, "running time", "running-time"},
      {MODE_VIDEO_FIRST, "video first", "video-first"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstAvWaitMode", values);
  }
  return gtype;
}

static void
gst_avwait_class_init (GstAvWaitClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_avwait_debug, "avwait", 0, "avwait");

  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Timecode Wait", "Filter/Audio/Video",
      "Drops all audio/video until a specific timecode or running time has been reached",
      "Vivia Nikolaidou <vivia@toolsonair.com>");

  gobject_class->set_property = gst_avwait_set_property;
  gobject_class->get_property = gst_avwait_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET_TIME_CODE_STRING,
      g_param_spec_string ("target-timecode-string", "Target timecode (string)",
          "Timecode to wait for in timecode mode (string). Must take the form 00:00:00:00",
          DEFAULT_TARGET_TIMECODE_STR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TARGET_TIME_CODE,
      g_param_spec_boxed ("target-timecode", "Target timecode (object)",
          "Timecode to wait for in timecode mode (object)",
          GST_TYPE_VIDEO_TIME_CODE,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TARGET_RUNNING_TIME,
      g_param_spec_uint64 ("target-running-time", "Target running time",
          "Running time to wait for in running-time mode",
          0, G_MAXUINT64,
          DEFAULT_TARGET_RUNNING_TIME,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Mode",
          "Operation mode: What to wait for",
          GST_TYPE_AVWAIT_MODE,
          DEFAULT_MODE,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_END_TIME_CODE,
      g_param_spec_boxed ("end-timecode", "End timecode (object)",
          "Timecode to end at in timecode mode (object)",
          GST_TYPE_VIDEO_TIME_CODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_END_RUNNING_TIME,
      g_param_spec_uint64 ("end-running-time", "End running time",
          "Running time to end at in running-time mode",
          0, G_MAXUINT64,
          DEFAULT_END_RUNNING_TIME,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RECORDING,
      g_param_spec_boolean ("recording",
          "Recording state",
          "Whether the element is stopped or recording. "
          "If set to FALSE, all buffers will be dropped regardless of settings.",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_avwait_finalize;
  gstelement_class->change_state = gst_avwait_change_state;

  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_sink_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &video_sink_template);

  gst_type_mark_as_plugin_api (GST_TYPE_AVWAIT_MODE, 0);
}

static void
gst_avwait_init (GstAvWait * self)
{
  self->asinkpad =
      gst_pad_new_from_static_template (&audio_sink_template, "asink");
  gst_pad_set_chain_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_asink_chain));
  gst_pad_set_event_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_asink_event));
  gst_pad_set_iterate_internal_links_function (self->asinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->asinkpad);

  self->vsinkpad =
      gst_pad_new_from_static_template (&video_sink_template, "vsink");
  gst_pad_set_chain_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_vsink_chain));
  gst_pad_set_event_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_vsink_event));
  gst_pad_set_iterate_internal_links_function (self->vsinkpad,
      GST_DEBUG_FUNCPTR (gst_avwait_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->vsinkpad);

  self->asrcpad =
      gst_pad_new_from_static_template (&audio_src_template, "asrc");
  gst_pad_set_iterate_internal_links_function (self->asrcpad,
      GST_DEBUG_FUNCPTR (gst_avwait_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->asrcpad);

  self->vsrcpad =
      gst_pad_new_from_static_template (&video_src_template, "vsrc");
  gst_pad_set_iterate_internal_links_function (self->vsrcpad,
      GST_DEBUG_FUNCPTR (gst_avwait_iterate_internal_links));
  gst_element_add_pad (GST_ELEMENT (self), self->vsrcpad);

  GST_PAD_SET_PROXY_CAPS (self->asinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->asinkpad);

  GST_PAD_SET_PROXY_CAPS (self->asrcpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->asrcpad);

  GST_PAD_SET_PROXY_CAPS (self->vsinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->vsinkpad);

  GST_PAD_SET_PROXY_CAPS (self->vsrcpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->vsrcpad);

  self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
  self->last_seen_video_running_time = GST_CLOCK_TIME_NONE;
  self->first_audio_running_time = GST_CLOCK_TIME_NONE;
  self->last_seen_tc = NULL;

  self->video_eos_flag = FALSE;
  self->audio_eos_flag = FALSE;
  self->video_flush_flag = FALSE;
  self->audio_flush_flag = FALSE;
  self->shutdown_flag = FALSE;
  self->dropping = TRUE;
  self->tc = gst_video_time_code_new_empty ();
  self->end_tc = NULL;
  self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
  self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
  self->audio_running_time_to_end_at = GST_CLOCK_TIME_NONE;
  self->recording = TRUE;

  self->target_running_time = DEFAULT_TARGET_RUNNING_TIME;
  self->end_running_time = DEFAULT_TARGET_RUNNING_TIME;
  self->mode = DEFAULT_MODE;

  gst_video_info_init (&self->vinfo);
  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);
  g_cond_init (&self->audio_cond);
}

static void
gst_avwait_send_element_message (GstAvWait * self, gboolean dropping,
    GstClockTime running_time)
{
  if (!gst_element_post_message (GST_ELEMENT (self),
          gst_message_new_element (GST_OBJECT (self),
              gst_structure_new ("avwait-status",
                  "dropping", G_TYPE_BOOLEAN, dropping,
                  "running-time", GST_TYPE_CLOCK_TIME, running_time, NULL)))) {
    GST_ERROR_OBJECT (self, "Unable to send element message!");
    g_assert_not_reached ();
  }
}

static GstStateChangeReturn
gst_avwait_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstAvWait *self = GST_AVWAIT (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&self->mutex);
      self->shutdown_flag = TRUE;
      g_cond_signal (&self->cond);
      g_cond_signal (&self->audio_cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock (&self->mutex);
      self->shutdown_flag = FALSE;
      self->video_eos_flag = FALSE;
      self->audio_eos_flag = FALSE;
      self->video_flush_flag = FALSE;
      self->audio_flush_flag = FALSE;
      self->must_send_end_message = END_MESSAGE_NORMAL;
      g_mutex_unlock (&self->mutex);
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      gboolean send_message = FALSE;

      g_mutex_lock (&self->mutex);
      if (self->mode != MODE_RUNNING_TIME) {
        GST_DEBUG_OBJECT (self, "First time reset in paused to ready");
        self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
        self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
        self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
        self->audio_running_time_to_end_at = GST_CLOCK_TIME_NONE;
      }
      if (!self->dropping) {
        self->dropping = TRUE;
        send_message = TRUE;
      }
      gst_segment_init (&self->asegment, GST_FORMAT_UNDEFINED);
      self->asegment.position = GST_CLOCK_TIME_NONE;
      gst_segment_init (&self->vsegment, GST_FORMAT_UNDEFINED);
      self->vsegment.position = GST_CLOCK_TIME_NONE;
      gst_video_info_init (&self->vinfo);
      self->last_seen_video_running_time = GST_CLOCK_TIME_NONE;
      self->first_audio_running_time = GST_CLOCK_TIME_NONE;
      if (self->last_seen_tc)
        gst_video_time_code_free (self->last_seen_tc);
      self->last_seen_tc = NULL;
      g_mutex_unlock (&self->mutex);

      if (send_message)
        gst_avwait_send_element_message (self, TRUE, GST_CLOCK_TIME_NONE);
      break;
    }
    default:
      break;
  }

  return ret;
}

static void
gst_avwait_finalize (GObject * object)
{
  GstAvWait *self = GST_AVWAIT (object);

  if (self->tc) {
    gst_video_time_code_free (self->tc);
    self->tc = NULL;
  }

  if (self->end_tc) {
    gst_video_time_code_free (self->end_tc);
    self->end_tc = NULL;
  }

  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);
  g_cond_clear (&self->audio_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avwait_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvWait *self = GST_AVWAIT (object);

  switch (prop_id) {
    case PROP_TARGET_TIME_CODE_STRING:{
      g_mutex_lock (&self->mutex);
      if (self->tc)
        g_value_take_string (value, gst_video_time_code_to_string (self->tc));
      else
        g_value_set_string (value, DEFAULT_TARGET_TIMECODE_STR);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_TARGET_TIME_CODE:{
      g_mutex_lock (&self->mutex);
      g_value_set_boxed (value, self->tc);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_END_TIME_CODE:{
      g_mutex_lock (&self->mutex);
      g_value_set_boxed (value, self->end_tc);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_TARGET_RUNNING_TIME:{
      g_mutex_lock (&self->mutex);
      g_value_set_uint64 (value, self->target_running_time);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_END_RUNNING_TIME:{
      g_mutex_lock (&self->mutex);
      g_value_set_uint64 (value, self->end_running_time);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_RECORDING:{
      g_mutex_lock (&self->mutex);
      g_value_set_boolean (value, self->recording);
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_MODE:{
      g_mutex_lock (&self->mutex);
      g_value_set_enum (value, self->mode);
      g_mutex_unlock (&self->mutex);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avwait_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvWait *self = GST_AVWAIT (object);

  switch (prop_id) {
    case PROP_TARGET_TIME_CODE_STRING:{
      gchar **parts;
      const gchar *tc_str;
      guint hours, minutes, seconds, frames;

      tc_str = g_value_get_string (value);
      parts = g_strsplit (tc_str, ":", 4);
      if (!parts || parts[3] == NULL) {
        GST_ERROR_OBJECT (self,
            "Error: Could not parse timecode %s. Please input a timecode in the form 00:00:00:00",
            tc_str);
        g_strfreev (parts);
        return;
      }
      hours = g_ascii_strtoll (parts[0], NULL, 10);
      minutes = g_ascii_strtoll (parts[1], NULL, 10);
      seconds = g_ascii_strtoll (parts[2], NULL, 10);
      frames = g_ascii_strtoll (parts[3], NULL, 10);
      g_mutex_lock (&self->mutex);
      if (self->tc)
        gst_video_time_code_free (self->tc);
      self->tc = gst_video_time_code_new (0, 1, NULL, 0, hours, minutes,
          seconds, frames, 0);
      if (GST_VIDEO_INFO_FORMAT (&self->vinfo) != GST_VIDEO_FORMAT_UNKNOWN
          && self->vinfo.fps_n != 0) {
        self->tc->config.fps_n = self->vinfo.fps_n;
        self->tc->config.fps_d = self->vinfo.fps_d;
      }
      g_mutex_unlock (&self->mutex);
      g_strfreev (parts);
      break;
    }
    case PROP_TARGET_TIME_CODE:{
      g_mutex_lock (&self->mutex);
      if (self->tc)
        gst_video_time_code_free (self->tc);
      self->tc = g_value_dup_boxed (value);
      if (self->tc && self->tc->config.fps_n == 0
          && GST_VIDEO_INFO_FORMAT (&self->vinfo) !=
          GST_VIDEO_FORMAT_UNKNOWN && self->vinfo.fps_n != 0) {
        self->tc->config.fps_n = self->vinfo.fps_n;
        self->tc->config.fps_d = self->vinfo.fps_d;
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_END_TIME_CODE:{
      g_mutex_lock (&self->mutex);
      if (self->end_tc)
        gst_video_time_code_free (self->end_tc);
      self->end_tc = g_value_dup_boxed (value);
      if (self->end_tc && self->end_tc->config.fps_n == 0
          && GST_VIDEO_INFO_FORMAT (&self->vinfo) !=
          GST_VIDEO_FORMAT_UNKNOWN && self->vinfo.fps_n != 0) {
        self->end_tc->config.fps_n = self->vinfo.fps_n;
        self->end_tc->config.fps_d = self->vinfo.fps_d;
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_TARGET_RUNNING_TIME:{
      g_mutex_lock (&self->mutex);
      self->target_running_time = g_value_get_uint64 (value);
      if (self->mode == MODE_RUNNING_TIME) {
        if (self->target_running_time > self->last_seen_video_running_time) {
          self->dropping = TRUE;
        }
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_END_RUNNING_TIME:{
      g_mutex_lock (&self->mutex);
      self->end_running_time = g_value_get_uint64 (value);
      if (self->mode == MODE_RUNNING_TIME) {
        if (self->end_running_time >= self->last_seen_video_running_time) {
          self->dropping = TRUE;
        }
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_MODE:{
      GstAvWaitMode old_mode;

      g_mutex_lock (&self->mutex);
      old_mode = self->mode;
      self->mode = g_value_get_enum (value);
      if (self->mode != old_mode) {
        switch (self->mode) {
          case MODE_TIMECODE:
            if (self->last_seen_tc && self->tc &&
                gst_video_time_code_compare (self->last_seen_tc,
                    self->tc) < 0) {
              self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
              self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
              self->dropping = TRUE;
            }
            break;
          case MODE_RUNNING_TIME:
            self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
            self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
            if (self->target_running_time > self->last_seen_video_running_time
                || self->end_running_time >=
                self->last_seen_video_running_time) {
              self->dropping = TRUE;
            }
            break;
            /* Let the chain functions handle the rest */
          case MODE_VIDEO_FIRST:
            /* pass-through */
          default:
            break;
        }
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case PROP_RECORDING:{
      g_mutex_lock (&self->mutex);
      self->recording = g_value_get_boolean (value);
      g_mutex_unlock (&self->mutex);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_avwait_vsink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAvWait *self = GST_AVWAIT (parent);
  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:{
      GstSegment segment;
      gboolean send_message = FALSE;
      gboolean segment_changed;

      g_mutex_lock (&self->mutex);
      gst_event_copy_segment (event, &segment);
      segment.position = self->vsegment.position;
      segment_changed = !gst_segment_is_equal (&segment, &self->vsegment);
      self->vsegment = segment;
      if (self->vsegment.format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (self, "Invalid segment format");
        g_mutex_unlock (&self->mutex);
        gst_event_unref (event);
        return FALSE;
      }
      if (segment_changed) {
        GST_DEBUG_OBJECT (self, "First time reset in video segment");
        self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
        self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
        self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
        self->audio_running_time_to_end_at = GST_CLOCK_TIME_NONE;
        if (!self->dropping) {
          self->dropping = TRUE;
          send_message = TRUE;
        }
        self->vsegment.position = GST_CLOCK_TIME_NONE;
      }
      g_mutex_unlock (&self->mutex);

      if (send_message)
        gst_avwait_send_element_message (self, TRUE, GST_CLOCK_TIME_NONE);
      break;
    }
    case GST_EVENT_GAP:
      gst_event_unref (event);
      return TRUE;
    case GST_EVENT_EOS:{
      GstClockTime running_time;
      gboolean send_message = FALSE;
      GstClockTime audio_running_time_to_end_at;

      g_mutex_lock (&self->mutex);
      self->video_eos_flag = TRUE;

      /* If we were recording then we'd be done with it at EOS of the video
       * pad once the audio has caught up, if it has to */
      running_time = self->last_seen_video_running_time;
      if (self->was_recording) {
        GST_INFO_OBJECT (self, "Recording stopped at EOS at %" GST_TIME_FORMAT,
            GST_TIME_ARGS (running_time));

        if (running_time > self->running_time_to_wait_for
            && running_time <= self->running_time_to_end_at) {
          /* We just stopped recording: synchronise the audio */
          self->audio_running_time_to_end_at = running_time;
          self->must_send_end_message |= END_MESSAGE_STREAM_ENDED;
        } else if (running_time < self->running_time_to_wait_for
            && self->running_time_to_wait_for != GST_CLOCK_TIME_NONE) {
          self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
        }
      }

      g_cond_signal (&self->cond);

      if (self->must_send_end_message & END_MESSAGE_AUDIO_PUSHED) {
        self->must_send_end_message = END_MESSAGE_NORMAL;
        send_message = TRUE;
        audio_running_time_to_end_at = self->audio_running_time_to_end_at;
      } else if (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) {
        self->must_send_end_message |= END_MESSAGE_VIDEO_PUSHED;
      }
      g_mutex_unlock (&self->mutex);

      if (send_message)
        gst_avwait_send_element_message (self, TRUE,
            audio_running_time_to_end_at);
      break;
    }
    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&self->mutex);
      self->video_flush_flag = TRUE;
      g_cond_signal (&self->audio_cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_EVENT_FLUSH_STOP:{
      gboolean send_message = FALSE;

      g_mutex_lock (&self->mutex);
      self->video_flush_flag = FALSE;
      GST_DEBUG_OBJECT (self, "First time reset in video flush");
      self->running_time_to_wait_for = GST_CLOCK_TIME_NONE;
      self->running_time_to_end_at = GST_CLOCK_TIME_NONE;
      self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
      self->audio_running_time_to_end_at = GST_CLOCK_TIME_NONE;
      if (!self->dropping) {
        self->dropping = TRUE;
        send_message = TRUE;
      }
      gst_segment_init (&self->vsegment, GST_FORMAT_UNDEFINED);
      self->vsegment.position = GST_CLOCK_TIME_NONE;
      g_mutex_unlock (&self->mutex);

      if (send_message)
        gst_avwait_send_element_message (self, TRUE, GST_CLOCK_TIME_NONE);
      break;
    }
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (self, "Got caps %" GST_PTR_FORMAT, caps);
      g_mutex_lock (&self->mutex);
      if (!gst_video_info_from_caps (&self->vinfo, caps)) {
        gst_event_unref (event);
        g_mutex_unlock (&self->mutex);
        return FALSE;
      }
      if (self->tc && self->tc->config.fps_n == 0 && self->vinfo.fps_n != 0) {
        self->tc->config.fps_n = self->vinfo.fps_n;
        self->tc->config.fps_d = self->vinfo.fps_d;
      }
      if (self->end_tc && self->end_tc->config.fps_n == 0
          && self->vinfo.fps_n != 0) {
        self->end_tc->config.fps_n = self->vinfo.fps_n;
        self->end_tc->config.fps_d = self->vinfo.fps_d;
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    default:
      break;
  }
  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_avwait_asink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAvWait *self = GST_AVWAIT (parent);
  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:{
      GstSegment segment;
      gboolean segment_changed;

      g_mutex_lock (&self->mutex);
      gst_event_copy_segment (event, &segment);
      segment.position = self->asegment.position;
      segment_changed = !gst_segment_is_equal (&segment, &self->asegment);
      self->asegment = segment;

      if (self->asegment.format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (self, "Invalid segment format");
        g_mutex_unlock (&self->mutex);
        gst_event_unref (event);
        return FALSE;
      }

      if (segment_changed) {
        self->asegment.position = GST_CLOCK_TIME_NONE;
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&self->mutex);
      self->audio_flush_flag = TRUE;
      g_cond_signal (&self->cond);
      g_mutex_unlock (&self->mutex);
      break;
    case GST_EVENT_EOS:{
      gboolean send_message = FALSE;
      GstClockTime audio_running_time_to_end_at;

      g_mutex_lock (&self->mutex);
      self->audio_eos_flag = TRUE;
      g_cond_signal (&self->audio_cond);

      if ((self->must_send_end_message & END_MESSAGE_VIDEO_PUSHED)) {
        self->must_send_end_message = END_MESSAGE_NORMAL;
        audio_running_time_to_end_at = self->audio_running_time_to_end_at;
        send_message = TRUE;
      } else if (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) {
        self->must_send_end_message |= END_MESSAGE_AUDIO_PUSHED;
      } else {
        self->must_send_end_message = END_MESSAGE_NORMAL;
      }
      g_mutex_unlock (&self->mutex);

      if (send_message)
        gst_avwait_send_element_message (self, TRUE,
            audio_running_time_to_end_at);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&self->mutex);
      self->audio_flush_flag = FALSE;
      gst_segment_init (&self->asegment, GST_FORMAT_UNDEFINED);
      self->asegment.position = GST_CLOCK_TIME_NONE;
      g_mutex_unlock (&self->mutex);
      break;
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (self, "Got caps %" GST_PTR_FORMAT, caps);
      g_mutex_lock (&self->mutex);
      if (!gst_audio_info_from_caps (&self->ainfo, caps)) {
        g_mutex_unlock (&self->mutex);
        gst_event_unref (event);
        return FALSE;
      }
      g_mutex_unlock (&self->mutex);
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
gst_avwait_vsink_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstClockTime timestamp;
  GstAvWait *self = GST_AVWAIT (parent);
  GstClockTime running_time;
  GstVideoTimeCode *tc = NULL;
  GstVideoTimeCodeMeta *tc_meta;
  gboolean retry = FALSE;
  gboolean ret = GST_FLOW_OK;
  gboolean send_message = FALSE;
  GstClockTime message_running_time;
  gboolean message_dropping;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  if (timestamp == GST_CLOCK_TIME_NONE) {
    gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&self->mutex);
  self->vsegment.position = timestamp;
  running_time =
      gst_segment_to_running_time (&self->vsegment, GST_FORMAT_TIME,
      self->vsegment.position);
  self->last_seen_video_running_time = running_time;

  tc_meta = gst_buffer_get_video_time_code_meta (inbuf);
  if (tc_meta) {
    tc = gst_video_time_code_copy (&tc_meta->tc);
    if (self->last_seen_tc) {
      gst_video_time_code_free (self->last_seen_tc);
    }
    self->last_seen_tc = tc;
  }

  while (self->mode == MODE_VIDEO_FIRST
      && self->first_audio_running_time == GST_CLOCK_TIME_NONE
      && !self->audio_eos_flag
      && !self->shutdown_flag && !self->video_flush_flag) {
    GST_DEBUG_OBJECT (self, "Waiting for first audio buffer");
    g_cond_wait (&self->audio_cond, &self->mutex);
  }

  if (self->video_flush_flag || self->shutdown_flag) {
    GST_DEBUG_OBJECT (self, "Shutting down, ignoring buffer");
    gst_buffer_unref (inbuf);
    g_mutex_unlock (&self->mutex);
    return GST_FLOW_FLUSHING;
  }

  switch (self->mode) {
    case MODE_TIMECODE:{
      if (self->tc && self->end_tc
          && gst_video_time_code_compare (self->tc, self->end_tc) != -1) {
        gchar *tc_str, *end_tc;

        tc_str = gst_video_time_code_to_string (self->tc);
        end_tc = gst_video_time_code_to_string (self->end_tc);
        GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
            ("End timecode %s must be after start timecode %s. Start timecode rejected",
                end_tc, tc_str));
        g_free (end_tc);
        g_free (tc_str);
        gst_buffer_unref (inbuf);
        g_mutex_unlock (&self->mutex);
        return GST_FLOW_ERROR;
      }

      if (self->tc != NULL && tc != NULL) {
        gboolean emit_passthrough_signal = FALSE;

        if (gst_video_time_code_compare (tc, self->tc) < 0
            && self->running_time_to_wait_for == GST_CLOCK_TIME_NONE) {
          GST_DEBUG_OBJECT (self, "Timecode not yet reached, ignoring frame");
          gst_buffer_unref (inbuf);
          inbuf = NULL;
        } else if (self->running_time_to_wait_for == GST_CLOCK_TIME_NONE) {
          GST_INFO_OBJECT (self, "Target timecode reached at %" GST_TIME_FORMAT,
              GST_TIME_ARGS (self->vsegment.position));
          /* Don't emit a signal if we weren't dropping (e.g. settings changed
           * mid-flight) */
          emit_passthrough_signal = self->dropping;
          self->dropping = FALSE;
          self->running_time_to_wait_for = running_time;
          if (self->recording) {
            self->audio_running_time_to_wait_for =
                self->running_time_to_wait_for;
          }
        }

        if (self->end_tc && gst_video_time_code_compare (tc, self->end_tc) >= 0) {
          if (self->running_time_to_end_at == GST_CLOCK_TIME_NONE) {
            GST_INFO_OBJECT (self, "End timecode reached at %" GST_TIME_FORMAT,
                GST_TIME_ARGS (self->vsegment.position));
            self->dropping = TRUE;
            self->running_time_to_end_at = running_time;
            if (self->recording) {
              self->audio_running_time_to_end_at = self->running_time_to_end_at;
              self->must_send_end_message |= END_MESSAGE_STREAM_ENDED;
            }
          }

          if (inbuf) {
            gst_buffer_unref (inbuf);
            inbuf = NULL;
          }
        } else if (emit_passthrough_signal && self->recording) {
          send_message = TRUE;
          message_running_time = self->running_time_to_wait_for;
          message_dropping = FALSE;
        }
      }
      break;
    }
    case MODE_RUNNING_TIME:{
      gboolean emit_passthrough_signal = FALSE;

      if (self->target_running_time != GST_CLOCK_TIME_NONE
          && running_time < self->target_running_time) {
        GST_DEBUG_OBJECT (self,
            "Have %" GST_TIME_FORMAT ", waiting for %" GST_TIME_FORMAT,
            GST_TIME_ARGS (running_time),
            GST_TIME_ARGS (self->target_running_time));
        gst_buffer_unref (inbuf);
        inbuf = NULL;
      } else if (self->running_time_to_wait_for == GST_CLOCK_TIME_NONE) {
        /* Don't emit a signal if we weren't dropping (e.g. settings changed
         * mid-flight) */
        emit_passthrough_signal = self->dropping;
        self->dropping = FALSE;
        self->running_time_to_wait_for = running_time;
        if (self->recording) {
          self->audio_running_time_to_wait_for = running_time;
        }
        if (self->recording) {
          send_message = TRUE;
          message_running_time = running_time;
          message_dropping = FALSE;
        }
      }

      if (GST_CLOCK_TIME_IS_VALID (self->end_running_time)
          && running_time >= self->end_running_time) {
        if (self->running_time_to_end_at == GST_CLOCK_TIME_NONE) {
          GST_INFO_OBJECT (self,
              "End running time %" GST_TIME_FORMAT " reached at %"
              GST_TIME_FORMAT, GST_TIME_ARGS (self->end_running_time),
              GST_TIME_ARGS (self->vsegment.position));
          self->dropping = TRUE;
          self->running_time_to_end_at = running_time;
          if (self->recording) {
            self->audio_running_time_to_end_at = running_time;
            self->must_send_end_message |= END_MESSAGE_STREAM_ENDED;
          }
        }

        if (inbuf) {
          gst_buffer_unref (inbuf);
          inbuf = NULL;
        }
      } else if (emit_passthrough_signal && self->recording) {
        send_message = TRUE;
        message_running_time = self->running_time_to_wait_for;
        message_dropping = FALSE;
      }

      break;
    }
    case MODE_VIDEO_FIRST:{
      if (self->running_time_to_wait_for == GST_CLOCK_TIME_NONE) {
        self->running_time_to_wait_for = running_time;
        GST_DEBUG_OBJECT (self, "First video running time is %" GST_TIME_FORMAT,
            GST_TIME_ARGS (self->running_time_to_wait_for));
        if (self->recording) {
          self->audio_running_time_to_wait_for = self->running_time_to_wait_for;
        }
        if (self->dropping) {
          self->dropping = FALSE;
          if (self->recording) {
            send_message = TRUE;
            message_running_time = self->running_time_to_wait_for;
            message_dropping = FALSE;
          }
        }
      }
      break;
    }
  }

  if (!self->recording) {
    if (self->was_recording) {
      GST_INFO_OBJECT (self, "Recording stopped at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (running_time));

      if (running_time > self->running_time_to_wait_for
          && (running_time <= self->running_time_to_end_at
              || self->running_time_to_end_at == GST_CLOCK_TIME_NONE)) {
        /* We just stopped recording: synchronise the audio */
        if (self->running_time_to_end_at == GST_CLOCK_TIME_NONE)
          self->running_time_to_end_at = running_time;
        self->audio_running_time_to_end_at = running_time;
        self->must_send_end_message |= END_MESSAGE_STREAM_ENDED;
      } else if (running_time < self->running_time_to_wait_for
          && self->running_time_to_wait_for != GST_CLOCK_TIME_NONE) {
        self->audio_running_time_to_wait_for = GST_CLOCK_TIME_NONE;
      }
    }

    /* Recording is FALSE: we drop all buffers */
    if (inbuf) {
      gst_buffer_unref (inbuf);
      inbuf = NULL;
    }
  } else {
    if (!self->was_recording) {
      GST_INFO_OBJECT (self,
          "Recording started at %" GST_TIME_FORMAT " waiting for %"
          GST_TIME_FORMAT " inbuf %p", GST_TIME_ARGS (running_time),
          GST_TIME_ARGS (self->running_time_to_wait_for), inbuf);

      if (self->mode != MODE_VIDEO_FIRST ||
          self->first_audio_running_time <= running_time ||
          self->audio_eos_flag) {
        if (running_time < self->running_time_to_end_at ||
            self->running_time_to_end_at == GST_CLOCK_TIME_NONE) {
          /* We are before the end of the recording. Check if we just actually
           * started */
          if (self->running_time_to_wait_for != GST_CLOCK_TIME_NONE
              && running_time > self->running_time_to_wait_for) {
            /* We just started recording: synchronise the audio */
            self->audio_running_time_to_wait_for = running_time;
            send_message = TRUE;
            message_running_time = running_time;
            message_dropping = FALSE;
          } else {
            /* We will start in the future when running_time_to_wait_for is
             * reached */
            self->audio_running_time_to_wait_for =
                self->running_time_to_wait_for;
          }
          self->audio_running_time_to_end_at = self->running_time_to_end_at;
        }
      } else {
        /* We are in video-first mode and behind the first audio timestamp. We
         * should drop all video buffers until the first audio timestamp, so
         * we can catch up with it. (In timecode mode and running-time mode, we
         * don't care about when the audio starts, we start as soon as the
         * target timecode or running time has been reached) */
        if (inbuf) {
          gst_buffer_unref (inbuf);
          inbuf = NULL;
        }
        retry = TRUE;
      }
    }
  }

  if (!retry)
    self->was_recording = self->recording;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);

  if (send_message)
    gst_avwait_send_element_message (self, message_dropping,
        message_running_time);
  send_message = FALSE;

  if (inbuf) {
    GST_DEBUG_OBJECT (self,
        "Pass video buffer %" GST_TIME_FORMAT "-%" GST_TIME_FORMAT,
        GST_TIME_ARGS (gst_segment_to_running_time (&self->vsegment,
                GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (inbuf))),
        GST_TIME_ARGS (gst_segment_to_running_time (&self->vsegment,
                GST_FORMAT_TIME,
                GST_BUFFER_TIMESTAMP (inbuf) + GST_BUFFER_DURATION (inbuf))));
    ret = gst_pad_push (self->vsrcpad, inbuf);
  }

  g_mutex_lock (&self->mutex);
  if (self->must_send_end_message & END_MESSAGE_AUDIO_PUSHED) {
    self->must_send_end_message = END_MESSAGE_NORMAL;
    send_message = TRUE;
    message_dropping = TRUE;
    message_running_time = self->audio_running_time_to_end_at;
  } else if (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) {
    if (self->audio_eos_flag) {
      self->must_send_end_message = END_MESSAGE_NORMAL;
      send_message = TRUE;
      message_dropping = TRUE;
      message_running_time = self->audio_running_time_to_end_at;
    } else {
      self->must_send_end_message |= END_MESSAGE_VIDEO_PUSHED;
    }
  }
  g_mutex_unlock (&self->mutex);

  if (send_message)
    gst_avwait_send_element_message (self, message_dropping,
        message_running_time);

  return ret;
}

/*
 * assumes sign1 and sign2 are either 1 or -1
 * returns 0 if sign1*num1 == sign2*num2
 * -1 if sign1*num1 < sign2*num2
 *  1 if sign1*num1 > sign2*num2
 */
static gint
gst_avwait_compare_guint64_with_signs (gint sign1,
    guint64 num1, gint sign2, guint64 num2)
{
  if (sign1 != sign2)
    return sign1;
  else if (num1 == num2)
    return 0;
  else
    return num1 > num2 ? sign1 : -sign1;
}

static GstFlowReturn
gst_avwait_asink_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstClockTime timestamp;
  GstAvWait *self = GST_AVWAIT (parent);
  GstClockTime current_running_time;
  GstClockTime video_running_time = GST_CLOCK_TIME_NONE;
  GstClockTime duration;
  GstClockTime running_time_at_end = GST_CLOCK_TIME_NONE;
  gint asign, vsign = 1, esign = 1;
  GstFlowReturn ret = GST_FLOW_OK;
  /* Make sure the video thread doesn't send the element message before we
   * actually call gst_pad_push */
  gboolean send_element_message = FALSE;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  if (timestamp == GST_CLOCK_TIME_NONE) {
    gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&self->mutex);
  self->asegment.position = timestamp;
  asign =
      gst_segment_to_running_time_full (&self->asegment, GST_FORMAT_TIME,
      self->asegment.position, &current_running_time);
  if (asign == 0) {
    g_mutex_unlock (&self->mutex);
    gst_buffer_unref (inbuf);
    GST_ERROR_OBJECT (self, "Could not get current running time");
    return GST_FLOW_ERROR;
  }

  if (self->first_audio_running_time == GST_CLOCK_TIME_NONE) {
    self->first_audio_running_time = current_running_time;
  }

  g_cond_signal (&self->audio_cond);
  if (self->vsegment.format == GST_FORMAT_TIME) {
    vsign =
        gst_segment_to_running_time_full (&self->vsegment, GST_FORMAT_TIME,
        self->vsegment.position, &video_running_time);
    if (vsign == 0) {
      video_running_time = GST_CLOCK_TIME_NONE;
    }
  }

  duration =
      gst_util_uint64_scale (gst_buffer_get_size (inbuf) / self->ainfo.bpf,
      GST_SECOND, self->ainfo.rate);
  if (duration != GST_CLOCK_TIME_NONE) {
    esign =
        gst_segment_to_running_time_full (&self->asegment, GST_FORMAT_TIME,
        self->asegment.position + duration, &running_time_at_end);
    if (esign == 0) {
      g_mutex_unlock (&self->mutex);
      GST_ERROR_OBJECT (self, "Could not get running time at end");
      gst_buffer_unref (inbuf);
      return GST_FLOW_ERROR;
    }
  }

  while (!(self->video_eos_flag || self->audio_flush_flag
          || self->shutdown_flag) &&
      /* Start at timecode */
      /* Wait if we haven't received video yet */
      (video_running_time == GST_CLOCK_TIME_NONE
          /* Wait if audio is after the video: dunno what to do */
          || gst_avwait_compare_guint64_with_signs (asign,
              running_time_at_end, vsign, video_running_time) == 1)) {
    GST_DEBUG_OBJECT (self,
        "Waiting for video: audio at %s%" GST_TIME_FORMAT ", video at %s%"
        GST_TIME_FORMAT, asign < 0 ? "-" : "+",
        GST_TIME_ARGS (running_time_at_end), vsign < 0 ? "-" : "+",
        GST_TIME_ARGS (video_running_time));
    g_cond_wait (&self->cond, &self->mutex);
    vsign =
        gst_segment_to_running_time_full (&self->vsegment, GST_FORMAT_TIME,
        self->vsegment.position, &video_running_time);
    if (vsign == 0) {
      video_running_time = GST_CLOCK_TIME_NONE;
    }
  }

  if (self->audio_flush_flag || self->shutdown_flag) {
    GST_DEBUG_OBJECT (self, "Shutting down, ignoring frame");
    gst_buffer_unref (inbuf);
    g_mutex_unlock (&self->mutex);
    return GST_FLOW_FLUSHING;
  }

  if (self->audio_running_time_to_wait_for == GST_CLOCK_TIME_NONE
      /* Audio ends before start : drop */
      || gst_avwait_compare_guint64_with_signs (esign,
          running_time_at_end, 1, self->audio_running_time_to_wait_for) == -1
      /* Audio starts after end: drop */
      || current_running_time >= self->audio_running_time_to_end_at) {
    GST_DEBUG_OBJECT (self,
        "Dropped an audio buf at %" GST_TIME_FORMAT " waiting for %"
        GST_TIME_FORMAT " video time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (current_running_time),
        GST_TIME_ARGS (self->audio_running_time_to_wait_for),
        GST_TIME_ARGS (video_running_time));
    GST_DEBUG_OBJECT (self, "Would have ended at %i %" GST_TIME_FORMAT,
        esign, GST_TIME_ARGS (running_time_at_end));
    gst_buffer_unref (inbuf);
    inbuf = NULL;
    if (current_running_time >= self->audio_running_time_to_end_at &&
        (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) &&
        !(self->must_send_end_message & END_MESSAGE_AUDIO_PUSHED)) {
      send_element_message = TRUE;
    }
  } else if (gst_avwait_compare_guint64_with_signs (esign, running_time_at_end,
          1, self->audio_running_time_to_wait_for) >= 0
      && gst_avwait_compare_guint64_with_signs (esign, running_time_at_end, 1,
          self->audio_running_time_to_end_at) == -1) {
    /* Audio ends after start, but before end: clip */
    GstSegment asegment2 = self->asegment;
    guint64 start;
    gint ssign;

    ssign = gst_segment_position_from_running_time_full (&asegment2,
        GST_FORMAT_TIME, self->audio_running_time_to_wait_for, &start);
    if (ssign > 0) {
      asegment2.start = start;
    } else {
      /* Starting before the start of the audio segment?! */
      /* This shouldn't happen: we already know that the current audio is
       * inside the segment, and that the end is after the current audio
       * position */
      GST_ELEMENT_ERROR (self, CORE, FAILED,
          ("Failed to clip audio: it should have started before the current segment"),
          NULL);
    }

    inbuf =
        gst_audio_buffer_clip (inbuf, &asegment2, self->ainfo.rate,
        self->ainfo.bpf);
  } else if (gst_avwait_compare_guint64_with_signs (esign, running_time_at_end,
          1, self->audio_running_time_to_end_at) >= 0) {
    /* Audio starts after start, but before end: clip from the other side */
    GstSegment asegment2 = self->asegment;
    guint64 stop;
    gint ssign;

    ssign =
        gst_segment_position_from_running_time_full (&asegment2,
        GST_FORMAT_TIME, self->audio_running_time_to_end_at, &stop);
    if (ssign > 0) {
      asegment2.stop = stop;
    } else {
      /* Stopping before the start of the audio segment?! */
      /* This shouldn't happen: we already know that the current audio is
       * inside the segment, and that the end is after the current audio
       * position */
      GST_ELEMENT_ERROR (self, CORE, FAILED,
          ("Failed to clip audio: it should have ended before the current segment"),
          NULL);
    }
    inbuf =
        gst_audio_buffer_clip (inbuf, &asegment2, self->ainfo.rate,
        self->ainfo.bpf);
    if (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) {
      send_element_message = TRUE;
    }
  } else {
    /* Programming error? Shouldn't happen */
    g_assert_not_reached ();
  }
  g_mutex_unlock (&self->mutex);

  if (inbuf) {
    GstClockTime new_duration =
        gst_util_uint64_scale (gst_buffer_get_size (inbuf) / self->ainfo.bpf,
        GST_SECOND, self->ainfo.rate);
    GstClockTime new_running_time_at_end =
        gst_segment_to_running_time (&self->asegment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (inbuf) + new_duration);

    GST_DEBUG_OBJECT (self,
        "Pass audio buffer %" GST_TIME_FORMAT "-%" GST_TIME_FORMAT,
        GST_TIME_ARGS (gst_segment_to_running_time (&self->asegment,
                GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (inbuf))),
        GST_TIME_ARGS (new_running_time_at_end));
    ret = gst_pad_push (self->asrcpad, inbuf);
  }

  if (send_element_message) {
    gboolean send_message = FALSE;
    GstClockTime audio_running_time_to_end_at;

    g_mutex_lock (&self->mutex);
    if ((self->must_send_end_message & END_MESSAGE_VIDEO_PUSHED) ||
        self->video_eos_flag) {
      self->must_send_end_message = END_MESSAGE_NORMAL;
      send_message = TRUE;
      audio_running_time_to_end_at = self->audio_running_time_to_end_at;
    } else if (self->must_send_end_message & END_MESSAGE_STREAM_ENDED) {
      self->must_send_end_message |= END_MESSAGE_AUDIO_PUSHED;
    } else {
      g_assert_not_reached ();
    }
    g_mutex_unlock (&self->mutex);

    if (send_message)
      gst_avwait_send_element_message (self, TRUE,
          audio_running_time_to_end_at);
  }
  send_element_message = FALSE;
  return ret;
}

static GstIterator *
gst_avwait_iterate_internal_links (GstPad * pad, GstObject * parent)
{
  GstIterator *it = NULL;
  GstPad *opad;
  GValue val = G_VALUE_INIT;
  GstAvWait *self = GST_AVWAIT (parent);

  if (self->asinkpad == pad)
    opad = gst_object_ref (self->asrcpad);
  else if (self->asrcpad == pad)
    opad = gst_object_ref (self->asinkpad);
  else if (self->vsinkpad == pad)
    opad = gst_object_ref (self->vsrcpad);
  else if (self->vsrcpad == pad)
    opad = gst_object_ref (self->vsinkpad);
  else
    goto out;

  g_value_init (&val, GST_TYPE_PAD);
  g_value_set_object (&val, opad);
  it = gst_iterator_new_single (GST_TYPE_PAD, &val);
  g_value_unset (&val);

  gst_object_unref (opad);

out:
  return it;
}
