/*
 * GStreamer
 * Copyright (C) 2016 Vivia Nikolaidou <vivia@toolsonair.com>
 * Copyright (C) 2019 Sebastian Dröge <sebastian@centricular.com>
 *
 * gsttimecodestamper.c
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
 * SECTION:element-timecodestamper
 * @title: timecodestamper
 * @short_description: Attach a timecode into incoming video frames
 *
 * This element attaches a timecode into every incoming video frame. It starts
 * counting from the stream time of each segment start, which it converts into
 * a timecode.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 videotestsrc ! timecodestamper ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttimecodestamper.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <stdlib.h>
#include <string.h>

#define ABSDIFF(a,b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))

GST_DEBUG_CATEGORY_STATIC (timecodestamper_debug);
#define GST_CAT_DEFAULT timecodestamper_debug

/* GstTimeCodeStamper properties */
enum
{
  PROP_0,
  PROP_SOURCE,
  PROP_SET,
  PROP_AUTO_RESYNC,
  PROP_TIMEOUT,
  PROP_DROP_FRAME,
  PROP_POST_MESSAGES,
  PROP_SET_INTERNAL_TIMECODE,
  PROP_LTC_DAILY_JAM,
  PROP_LTC_AUTO_RESYNC,
  PROP_LTC_EXTRA_LATENCY,
  PROP_LTC_TIMEOUT,
  PROP_RTC_MAX_DRIFT,
  PROP_RTC_AUTO_RESYNC,
  PROP_TIMECODE_OFFSET
};

#define DEFAULT_SOURCE GST_TIME_CODE_STAMPER_SOURCE_INTERNAL
#define DEFAULT_SET GST_TIME_CODE_STAMPER_SET_KEEP
#define DEFAULT_AUTO_RESYNC TRUE
#define DEFAULT_TIMEOUT GST_CLOCK_TIME_NONE
#define DEFAULT_DROP_FRAME TRUE
#define DEFAULT_POST_MESSAGES FALSE
#define DEFAULT_SET_INTERNAL_TIMECODE NULL
#define DEFAULT_LTC_DAILY_JAM NULL
#define DEFAULT_LTC_AUTO_RESYNC TRUE
#define DEFAULT_LTC_TIMEOUT GST_CLOCK_TIME_NONE
#define DEFAULT_LTC_EXTRA_LATENCY (150 * GST_MSECOND)
#define DEFAULT_RTC_MAX_DRIFT 250000000
#define DEFAULT_RTC_AUTO_RESYNC TRUE
#define DEFAULT_TIMECODE_OFFSET 0

#define DEFAULT_LTC_QUEUE 100

static GstStaticPadTemplate gst_timecodestamper_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, framerate=[1/2147483647, 2147483647/1]; "
        "closedcaption/x-cea-608, framerate=[1/2147483647, 2147483647/1]; "
        "closedcaption/x-cea-708, framerate=[1/2147483647, 2147483647/1]; ")
    );

static GstStaticPadTemplate gst_timecodestamper_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, framerate=[1/2147483647, 2147483647/1]; "
        "closedcaption/x-cea-608, framerate=[1/2147483647, 2147483647/1]; "
        "closedcaption/x-cea-708, framerate=[1/2147483647, 2147483647/1]; ")
    );

static GstStaticPadTemplate gst_timecodestamper_ltc_template =
GST_STATIC_PAD_TEMPLATE ("ltc_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("audio/x-raw,format=U8,rate=[1,max],channels=1")
    );

static void gst_timecodestamper_finalize (GObject * object);
static void gst_timecodestamper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_timecodestamper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_timecodestamper_dispose (GObject * object);
static gboolean gst_timecodestamper_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_timecodestamper_src_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_timecodestamper_transform_ip (GstBaseTransform *
    vfilter, GstBuffer * buffer);
static gboolean gst_timecodestamper_stop (GstBaseTransform * trans);
static gboolean gst_timecodestamper_start (GstBaseTransform * trans);
static GstPad *gst_timecodestamper_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * unused, const GstCaps * caps);
static void gst_timecodestamper_release_pad (GstElement * element,
    GstPad * pad);

#if HAVE_LTC
typedef struct
{
  GstClockTime running_time;
  GstVideoTimeCode timecode;
} TimestampedTimecode;

static gboolean gst_timecodestamper_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);

static GstFlowReturn gst_timecodestamper_ltcpad_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_timecodestamper_ltcpad_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_timecodestamper_ltcpad_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_timecodestamper_ltcpad_activatemode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);

static gboolean gst_timecodestamper_videopad_activatemode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);

static GstIterator *gst_timecodestamper_src_iterate_internal_link (GstPad * pad,
    GstObject * parent);
#endif

static void gst_timecodestamper_update_drop_frame (GstTimeCodeStamper *
    timecodestamper);

G_DEFINE_TYPE (GstTimeCodeStamper, gst_timecodestamper,
    GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (timecodestamper, "timecodestamper",
    GST_RANK_NONE, GST_TYPE_TIME_CODE_STAMPER);

GType
gst_timecodestamper_source_get_type (void)
{
  static GType type = 0;
  static const GEnumValue values[] = {
    {GST_TIME_CODE_STAMPER_SOURCE_INTERNAL,
          "Use internal timecode counter, starting at zero or value set by property",
        "internal"},
    {GST_TIME_CODE_STAMPER_SOURCE_ZERO,
        "Always use zero", "zero"},
    {GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN,
          "Count up from the last known upstream timecode or internal if unknown",
        "last-known"},
    {GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN_OR_ZERO,
          "Count up from the last known upstream timecode or zero if unknown",
        "last-known-or-zero"},
    {GST_TIME_CODE_STAMPER_SOURCE_LTC,
        "Linear timecode from an audio device", "ltc"},
    {GST_TIME_CODE_STAMPER_SOURCE_RTC,
        "Timecode from real time clock", "rtc"},
    {0, NULL, NULL},
  };

  if (!type) {
    type = g_enum_register_static ("GstTimeCodeStamperSource", values);
  }
  return type;
}

GType
gst_timecodestamper_set_get_type (void)
{
  static GType type = 0;
  static const GEnumValue values[] = {
    {GST_TIME_CODE_STAMPER_SET_NEVER,
        "Never set timecodes", "never"},
    {GST_TIME_CODE_STAMPER_SET_KEEP,
        "Keep upstream timecodes and only set if no upstream timecode", "keep"},
    {GST_TIME_CODE_STAMPER_SET_ALWAYS,
        "Always set timecode and remove upstream timecode", "always"},
    {0, NULL, NULL},
  };

  if (!type) {
    type = g_enum_register_static ("GstTimeCodeStamperSet", values);
  }
  return type;
}

static void
gst_timecodestamper_class_init (GstTimeCodeStamperClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT (timecodestamper_debug, "timecodestamper", 0,
      "timecodestamper");
  gst_element_class_set_static_metadata (element_class, "Timecode stamper",
      "Filter/Video", "Attaches a timecode meta into each video frame",
      "Vivia Nikolaidou <vivia@toolsonair.com>");

  gobject_class->set_property = gst_timecodestamper_set_property;
  gobject_class->get_property = gst_timecodestamper_get_property;
  gobject_class->finalize = gst_timecodestamper_finalize;
  gobject_class->dispose = gst_timecodestamper_dispose;

  g_object_class_install_property (gobject_class, PROP_SOURCE,
      g_param_spec_enum ("source", "Timecode Source",
          "Choose from what source the timecode should be taken",
          GST_TYPE_TIME_CODE_STAMPER_SOURCE,
          DEFAULT_SOURCE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SET,
      g_param_spec_enum ("set", "Timecode Set",
          "Choose whether timecodes should be overridden or not",
          GST_TYPE_TIME_CODE_STAMPER_SET,
          DEFAULT_SET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_AUTO_RESYNC,
      g_param_spec_boolean ("auto-resync",
          "Auto Resync",
          "If true resync last known timecode from upstream, otherwise only "
          "count up from the last known one",
          DEFAULT_AUTO_RESYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout",
          "Timeout",
          "Time out upstream timecode if no new timecode was detected after this time",
          0, G_MAXUINT64, DEFAULT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DROP_FRAME,
      g_param_spec_boolean ("drop-frame", "Drop Frame",
          "Use drop-frame timecodes for 29.97 and 59.94 FPS",
          DEFAULT_DROP_FRAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POST_MESSAGES,
      g_param_spec_boolean ("post-messages", "Post element message",
          "Post element message containing the current timecode",
          DEFAULT_POST_MESSAGES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SET_INTERNAL_TIMECODE,
      g_param_spec_boxed ("set-internal-timecode",
          "Set Internal Timecode",
          "If set, take this timecode as the internal timecode for the first "
          "frame and increment from it. Only the values itself and daily jam are taken, "
          "flags and frame rate are always determined by timecodestamper "
          "itself. If unset, the internal timecode will start at 0 with the daily jam "
          "being the current real-time clock time",
          GST_TYPE_VIDEO_TIME_CODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LTC_DAILY_JAM,
      g_param_spec_boxed ("ltc-daily-jam",
          "LTC Daily jam",
          "The daily jam of the LTC timecode",
          G_TYPE_DATE_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LTC_AUTO_RESYNC,
      g_param_spec_boolean ("ltc-auto-resync",
          "LTC Auto Resync",
          "If true the LTC timecode will be automatically resynced if it drifts, "
          "otherwise it will only be counted up from the last known one",
          DEFAULT_LTC_AUTO_RESYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LTC_EXTRA_LATENCY,
      g_param_spec_uint64 ("ltc-extra-latency", "LTC Extra Latency",
          "Extra latency to introduce for waiting for LTC timecodes",
          0, G_MAXUINT64, DEFAULT_LTC_EXTRA_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LTC_TIMEOUT,
      g_param_spec_uint64 ("ltc-timeout", "LTC Timeout",
          "Time out LTC timecode if no new timecode was detected after this time",
          0, G_MAXUINT64, DEFAULT_LTC_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RTC_MAX_DRIFT,
      g_param_spec_uint64 ("rtc-max-drift",
          "RTC Maximum Offset",
          "Maximum number of nanoseconds the RTC clock is allowed to drift from "
          "the video before it is resynced",
          0, G_MAXUINT64, DEFAULT_RTC_MAX_DRIFT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RTC_AUTO_RESYNC,
      g_param_spec_boolean ("rtc-auto-resync",
          "RTC Auto Resync",
          "If true the RTC timecode will be automatically resynced if it drifts, "
          "otherwise it will only be counted up from the last known one",
          DEFAULT_RTC_AUTO_RESYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TIMECODE_OFFSET,
      g_param_spec_int ("timecode-offset",
          "Timecode Offset",
          "Add this offset in frames to internal, LTC or RTC timecode, "
          "useful if there is an offset between the timecode source and video",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_timecodestamper_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_timecodestamper_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_timecodestamper_ltc_template));

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_timecodestamper_request_new_pad);
  element_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_timecodestamper_release_pad);

  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_timecodestamper_sink_event);
  trans_class->src_event = GST_DEBUG_FUNCPTR (gst_timecodestamper_src_event);
#if HAVE_LTC
  trans_class->query = GST_DEBUG_FUNCPTR (gst_timecodestamper_query);
#endif
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_timecodestamper_stop);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_timecodestamper_start);

  trans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_timecodestamper_transform_ip);

  gst_type_mark_as_plugin_api (GST_TYPE_TIME_CODE_STAMPER_SOURCE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_TIME_CODE_STAMPER_SET, 0);
}

static void
gst_timecodestamper_init (GstTimeCodeStamper * timecodestamper)
{
  timecodestamper->ltcpad = NULL;

  timecodestamper->tc_source = GST_TIME_CODE_STAMPER_SOURCE_INTERNAL;
  timecodestamper->tc_set = GST_TIME_CODE_STAMPER_SET_KEEP;
  timecodestamper->tc_auto_resync = DEFAULT_AUTO_RESYNC;
  timecodestamper->tc_timeout = DEFAULT_TIMEOUT;
  timecodestamper->drop_frame = DEFAULT_DROP_FRAME;
  timecodestamper->post_messages = DEFAULT_POST_MESSAGES;
  timecodestamper->set_internal_tc = NULL;
  timecodestamper->ltc_daily_jam = DEFAULT_LTC_DAILY_JAM;
  timecodestamper->ltc_auto_resync = DEFAULT_LTC_AUTO_RESYNC;
  timecodestamper->ltc_extra_latency = DEFAULT_LTC_EXTRA_LATENCY;
  timecodestamper->ltc_timeout = DEFAULT_LTC_TIMEOUT;
  timecodestamper->rtc_max_drift = DEFAULT_RTC_MAX_DRIFT;
  timecodestamper->rtc_auto_resync = DEFAULT_RTC_AUTO_RESYNC;
  timecodestamper->timecode_offset = 0;

  timecodestamper->internal_tc = NULL;
  timecodestamper->last_tc = NULL;
  timecodestamper->last_tc_running_time = GST_CLOCK_TIME_NONE;
  timecodestamper->rtc_tc = NULL;

  timecodestamper->seeked_frames = -1;

#if HAVE_LTC
  g_mutex_init (&timecodestamper->mutex);
  g_cond_init (&timecodestamper->ltc_cond_video);
  g_cond_init (&timecodestamper->ltc_cond_audio);

  gst_segment_init (&timecodestamper->ltc_segment, GST_FORMAT_UNDEFINED);
  timecodestamper->ltc_first_running_time = GST_CLOCK_TIME_NONE;
  timecodestamper->ltc_current_running_time = GST_CLOCK_TIME_NONE;

  g_queue_init (&timecodestamper->ltc_current_tcs);
  timecodestamper->ltc_internal_tc = NULL;
  timecodestamper->ltc_internal_running_time = GST_CLOCK_TIME_NONE;
  timecodestamper->ltc_dec = NULL;
  timecodestamper->ltc_total = 0;

  timecodestamper->ltc_eos = TRUE;
  timecodestamper->ltc_flushing = TRUE;

  timecodestamper->audio_live = FALSE;
  timecodestamper->audio_latency = GST_CLOCK_TIME_NONE;
  timecodestamper->video_live = FALSE;
  timecodestamper->video_latency = GST_CLOCK_TIME_NONE;
  timecodestamper->latency = GST_CLOCK_TIME_NONE;

  timecodestamper->video_activatemode_default =
      GST_PAD_ACTIVATEMODEFUNC (GST_BASE_TRANSFORM_SINK_PAD (timecodestamper));
  GST_PAD_ACTIVATEMODEFUNC (GST_BASE_TRANSFORM_SINK_PAD (timecodestamper)) =
      gst_timecodestamper_videopad_activatemode;
  gst_pad_set_iterate_internal_links_function (GST_BASE_TRANSFORM_SRC_PAD
      (timecodestamper), gst_timecodestamper_src_iterate_internal_link);
#endif
}

static void
gst_timecodestamper_finalize (GObject * object)
{
#if HAVE_LTC
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (object);

  g_cond_clear (&timecodestamper->ltc_cond_video);
  g_cond_clear (&timecodestamper->ltc_cond_audio);
  g_mutex_clear (&timecodestamper->mutex);
#endif

  G_OBJECT_CLASS (gst_timecodestamper_parent_class)->finalize (object);
}

static void
gst_timecodestamper_dispose (GObject * object)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (object);

  if (timecodestamper->ltc_daily_jam) {
    g_date_time_unref (timecodestamper->ltc_daily_jam);
    timecodestamper->ltc_daily_jam = NULL;
  }

  if (timecodestamper->internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->internal_tc);
    timecodestamper->internal_tc = NULL;
  }

  if (timecodestamper->set_internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->set_internal_tc);
    timecodestamper->set_internal_tc = NULL;
  }

  if (timecodestamper->last_tc != NULL) {
    gst_video_time_code_free (timecodestamper->last_tc);
    timecodestamper->last_tc = NULL;
  }
  timecodestamper->last_tc_running_time = GST_CLOCK_TIME_NONE;

  if (timecodestamper->rtc_tc != NULL) {
    gst_video_time_code_free (timecodestamper->rtc_tc);
    timecodestamper->rtc_tc = NULL;
  }
#if HAVE_LTC
  g_mutex_lock (&timecodestamper->mutex);
  {
    TimestampedTimecode *tc;
    while ((tc = g_queue_pop_tail (&timecodestamper->ltc_current_tcs))) {
      gst_video_time_code_clear (&tc->timecode);
      g_free (tc);
    }
  }
  if (timecodestamper->ltc_internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->ltc_internal_tc);
    timecodestamper->ltc_internal_tc = NULL;
  }
  timecodestamper->ltc_internal_running_time = GST_CLOCK_TIME_NONE;

  if (timecodestamper->ltc_dec) {
    ltc_decoder_free (timecodestamper->ltc_dec);
    timecodestamper->ltc_dec = NULL;
  }

  if (timecodestamper->stream_align) {
    gst_audio_stream_align_free (timecodestamper->stream_align);
    timecodestamper->stream_align = NULL;
  }
  g_mutex_unlock (&timecodestamper->mutex);
#endif

  G_OBJECT_CLASS (gst_timecodestamper_parent_class)->dispose (object);
}

static void
gst_timecodestamper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (object);

  GST_OBJECT_LOCK (timecodestamper);
  switch (prop_id) {
    case PROP_SOURCE:
      timecodestamper->tc_source = (GstTimeCodeStamperSource)
          g_value_get_enum (value);
      break;
    case PROP_SET:
      timecodestamper->tc_set = (GstTimeCodeStamperSet)
          g_value_get_enum (value);
      break;
    case PROP_AUTO_RESYNC:
      timecodestamper->tc_auto_resync = g_value_get_boolean (value);
      break;
    case PROP_TIMEOUT:
      timecodestamper->tc_timeout = g_value_get_uint64 (value);
      break;
    case PROP_DROP_FRAME:
      timecodestamper->drop_frame = g_value_get_boolean (value);
      gst_timecodestamper_update_drop_frame (timecodestamper);
      break;
    case PROP_LTC_DAILY_JAM:
      if (timecodestamper->ltc_daily_jam)
        g_date_time_unref (timecodestamper->ltc_daily_jam);
      timecodestamper->ltc_daily_jam = g_value_dup_boxed (value);

#if HAVE_LTC
      {
        GList *l;

        for (l = timecodestamper->ltc_current_tcs.head; l; l = l->next) {
          TimestampedTimecode *tc = l->data;

          if (tc->timecode.config.latest_daily_jam) {
            g_date_time_unref (tc->timecode.config.latest_daily_jam);
          }
          tc->timecode.config.latest_daily_jam =
              g_date_time_ref (timecodestamper->ltc_daily_jam);
        }
      }

      if (timecodestamper->ltc_internal_tc) {
        if (timecodestamper->ltc_internal_tc->config.latest_daily_jam) {
          g_date_time_unref (timecodestamper->ltc_internal_tc->
              config.latest_daily_jam);
        }
        timecodestamper->ltc_internal_tc->config.latest_daily_jam =
            g_date_time_ref (timecodestamper->ltc_daily_jam);
      }
#endif
      break;
    case PROP_POST_MESSAGES:
      timecodestamper->post_messages = g_value_get_boolean (value);
      break;
    case PROP_SET_INTERNAL_TIMECODE:{
      if (timecodestamper->set_internal_tc)
        gst_video_time_code_free (timecodestamper->set_internal_tc);
      timecodestamper->set_internal_tc = g_value_dup_boxed (value);

      /* Reset the internal timecode on the next opportunity if a new
       * timecode was set here. If none was set we just continue counting
       * from the previous one */
      if (timecodestamper->set_internal_tc && timecodestamper->internal_tc) {
        gst_video_time_code_free (timecodestamper->internal_tc);
        timecodestamper->internal_tc = NULL;
      }
      break;
    }
    case PROP_LTC_AUTO_RESYNC:
      timecodestamper->ltc_auto_resync = g_value_get_boolean (value);
      break;
    case PROP_LTC_TIMEOUT:
      timecodestamper->ltc_timeout = g_value_get_uint64 (value);
      break;
    case PROP_LTC_EXTRA_LATENCY:
      timecodestamper->ltc_extra_latency = g_value_get_uint64 (value);
      break;
    case PROP_RTC_MAX_DRIFT:
      timecodestamper->rtc_max_drift = g_value_get_uint64 (value);
      break;
    case PROP_RTC_AUTO_RESYNC:
      timecodestamper->rtc_auto_resync = g_value_get_boolean (value);
      break;
    case PROP_TIMECODE_OFFSET:
      timecodestamper->timecode_offset = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (timecodestamper);
}

static void
gst_timecodestamper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (object);

  GST_OBJECT_LOCK (timecodestamper);
  switch (prop_id) {
    case PROP_SOURCE:
      g_value_set_enum (value, timecodestamper->tc_source);
      break;
    case PROP_SET:
      g_value_set_enum (value, timecodestamper->tc_set);
      break;
    case PROP_AUTO_RESYNC:
      g_value_set_boolean (value, timecodestamper->tc_auto_resync);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, timecodestamper->tc_timeout);
      break;
    case PROP_DROP_FRAME:
      g_value_set_boolean (value, timecodestamper->drop_frame);
      break;
    case PROP_LTC_DAILY_JAM:
      g_value_set_boxed (value, timecodestamper->ltc_daily_jam);
      break;
    case PROP_POST_MESSAGES:
      g_value_set_boolean (value, timecodestamper->post_messages);
      break;
    case PROP_SET_INTERNAL_TIMECODE:
      g_value_set_boxed (value, timecodestamper->set_internal_tc);
      break;
    case PROP_LTC_AUTO_RESYNC:
      g_value_set_boolean (value, timecodestamper->ltc_auto_resync);
      break;
    case PROP_LTC_TIMEOUT:
      g_value_set_uint64 (value, timecodestamper->ltc_timeout);
      break;
    case PROP_LTC_EXTRA_LATENCY:
      g_value_set_uint64 (value, timecodestamper->ltc_extra_latency);
      break;
    case PROP_RTC_MAX_DRIFT:
      g_value_set_uint64 (value, timecodestamper->rtc_max_drift);
      break;
    case PROP_RTC_AUTO_RESYNC:
      g_value_set_boolean (value, timecodestamper->rtc_auto_resync);
      break;
    case PROP_TIMECODE_OFFSET:
      g_value_set_int (value, timecodestamper->timecode_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (timecodestamper);
}

static gboolean
gst_timecodestamper_stop (GstBaseTransform * trans)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (trans);

#if HAVE_LTC
  g_mutex_lock (&timecodestamper->mutex);
  timecodestamper->video_flushing = TRUE;
  timecodestamper->video_current_running_time = GST_CLOCK_TIME_NONE;
  if (timecodestamper->video_clock_id)
    gst_clock_id_unschedule (timecodestamper->video_clock_id);
  timecodestamper->ltc_flushing = TRUE;
  g_cond_signal (&timecodestamper->ltc_cond_video);
  g_cond_signal (&timecodestamper->ltc_cond_audio);
  g_mutex_unlock (&timecodestamper->mutex);
#endif

  timecodestamper->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  timecodestamper->fps_n = 0;
  timecodestamper->fps_d = 1;

  if (timecodestamper->internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->internal_tc);
    timecodestamper->internal_tc = NULL;
  }

  if (timecodestamper->rtc_tc != NULL) {
    gst_video_time_code_free (timecodestamper->rtc_tc);
    timecodestamper->rtc_tc = NULL;
  }

  if (timecodestamper->last_tc != NULL) {
    gst_video_time_code_free (timecodestamper->last_tc);
    timecodestamper->last_tc = NULL;
  }
  timecodestamper->last_tc_running_time = GST_CLOCK_TIME_NONE;
#if HAVE_LTC
  g_mutex_lock (&timecodestamper->mutex);
  gst_audio_info_init (&timecodestamper->ainfo);
  gst_segment_init (&timecodestamper->ltc_segment, GST_FORMAT_UNDEFINED);

  timecodestamper->ltc_first_running_time = GST_CLOCK_TIME_NONE;
  timecodestamper->ltc_current_running_time = GST_CLOCK_TIME_NONE;

  if (timecodestamper->ltc_internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->ltc_internal_tc);
    timecodestamper->ltc_internal_tc = NULL;
  }
  timecodestamper->ltc_internal_running_time = GST_CLOCK_TIME_NONE;

  {
    TimestampedTimecode *tc;
    while ((tc = g_queue_pop_tail (&timecodestamper->ltc_current_tcs))) {
      gst_video_time_code_clear (&tc->timecode);
      g_free (tc);
    }
  }

  if (timecodestamper->ltc_dec) {
    ltc_decoder_free (timecodestamper->ltc_dec);
    timecodestamper->ltc_dec = NULL;
  }

  if (timecodestamper->stream_align) {
    gst_audio_stream_align_free (timecodestamper->stream_align);
    timecodestamper->stream_align = NULL;
  }

  timecodestamper->ltc_total = 0;
  g_mutex_unlock (&timecodestamper->mutex);
#endif

  return TRUE;
}

static gboolean
gst_timecodestamper_start (GstBaseTransform * trans)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (trans);

#if HAVE_LTC
  g_mutex_lock (&timecodestamper->mutex);
  timecodestamper->video_flushing = FALSE;
  timecodestamper->video_eos = FALSE;
  g_mutex_unlock (&timecodestamper->mutex);
#endif

  timecodestamper->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  timecodestamper->fps_n = 0;
  timecodestamper->fps_d = 1;

  return TRUE;
}

/* Must be called with object lock */
static void
gst_timecodestamper_update_drop_frame (GstTimeCodeStamper * timecodestamper)
{
  if (timecodestamper->drop_frame && timecodestamper->fps_d == 1001 &&
      (timecodestamper->fps_n == 30000 || timecodestamper->fps_n == 60000)) {
    if (timecodestamper->internal_tc)
      timecodestamper->internal_tc->config.flags |=
          GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
    if (timecodestamper->rtc_tc)
      timecodestamper->rtc_tc->config.flags |=
          GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
#if HAVE_LTC
    {
      GList *l;

      for (l = timecodestamper->ltc_current_tcs.head; l; l = l->next) {
        TimestampedTimecode *tc = l->data;

        tc->timecode.config.flags |= GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
      }
    }
    if (timecodestamper->ltc_internal_tc)
      timecodestamper->ltc_internal_tc->config.flags |=
          GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
#endif
  } else {
    if (timecodestamper->internal_tc)
      timecodestamper->internal_tc->config.flags &=
          ~GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
    if (timecodestamper->rtc_tc)
      timecodestamper->rtc_tc->config.flags &=
          ~GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
#if HAVE_LTC
    {
      GList *l;

      for (l = timecodestamper->ltc_current_tcs.head; l; l = l->next) {
        TimestampedTimecode *tc = l->data;

        tc->timecode.config.flags &= ~GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
      }
    }
    if (timecodestamper->ltc_internal_tc)
      timecodestamper->ltc_internal_tc->config.flags &=
          ~GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
#endif
  }
}

static void
gst_timecodestamper_update_timecode_framerate (GstTimeCodeStamper *
    timecodestamper, gint fps_n, gint fps_d, GstVideoTimeCode * timecode,
    gboolean is_ltc)
{
  guint64 nframes;
  GstClockTime time;
  GDateTime *jam = NULL;
  GstVideoTimeCodeFlags tc_flags = 0;

  if (!timecode)
    return;

  if (timecodestamper->interlace_mode != GST_VIDEO_INTERLACE_MODE_PROGRESSIVE)
    tc_flags |= GST_VIDEO_TIME_CODE_FLAGS_INTERLACED;

  if (timecodestamper->drop_frame && timecodestamper->fps_d == 1001 &&
      (timecodestamper->fps_n == 30000 || timecodestamper->fps_n == 60000))
    tc_flags |= GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;

  /* If this is an LTC timecode and we have no framerate yet in there then
   * just do nothing. We're going to set the framerate at a later time */
  if (timecode->config.fps_d != 0 || !is_ltc) {
    nframes = gst_video_time_code_frames_since_daily_jam (timecode);
    time =
        gst_util_uint64_scale (nframes,
        GST_SECOND * timecodestamper->fps_d, timecodestamper->fps_n);
    jam =
        timecode->config.latest_daily_jam ? g_date_time_ref (timecode->config.
        latest_daily_jam) : NULL;
    gst_video_time_code_clear (timecode);
    gst_video_time_code_init (timecode, timecodestamper->fps_n,
        timecodestamper->fps_d, jam, tc_flags, 0, 0, 0, 0, 0);
    if (jam)
      g_date_time_unref (jam);

    nframes = gst_util_uint64_scale (time, fps_n, GST_SECOND * fps_d);
    gst_video_time_code_add_frames (timecode, nframes);
  }
}

/* Must be called with object lock */
static gboolean
gst_timecodestamper_update_framerate (GstTimeCodeStamper * timecodestamper,
    gint fps_n, gint fps_d)
{
  /* Nothing changed */
  if (fps_n == timecodestamper->fps_n && fps_d == timecodestamper->fps_d)
    return FALSE;

  gst_timecodestamper_update_timecode_framerate (timecodestamper, fps_n, fps_d,
      timecodestamper->internal_tc, FALSE);
  gst_timecodestamper_update_timecode_framerate (timecodestamper, fps_n, fps_d,
      timecodestamper->last_tc, FALSE);
  gst_timecodestamper_update_timecode_framerate (timecodestamper, fps_n, fps_d,
      timecodestamper->rtc_tc, FALSE);

#if HAVE_LTC
  {
    GList *l;

    for (l = timecodestamper->ltc_current_tcs.head; l; l = l->next) {
      TimestampedTimecode *tc = l->data;

      gst_timecodestamper_update_timecode_framerate (timecodestamper, fps_n,
          fps_d, &tc->timecode, TRUE);
    }
  }
  gst_timecodestamper_update_timecode_framerate (timecodestamper, fps_n, fps_d,
      timecodestamper->ltc_internal_tc, FALSE);
#endif

  return TRUE;
}

static gboolean
gst_timecodestamper_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (trans);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (trans, "received event %" GST_PTR_FORMAT, event);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      gst_event_copy_segment (event, &segment);
      if (segment.format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (timecodestamper, "Invalid segment format");
        gst_event_unref (event);
        return FALSE;
      }

      GST_OBJECT_LOCK (timecodestamper);
      if (timecodestamper->tc_source == GST_TIME_CODE_STAMPER_SOURCE_INTERNAL
          && GST_EVENT_SEQNUM (event) == timecodestamper->prev_seek_seqnum) {
        timecodestamper->reset_internal_tc_from_seek = TRUE;
        timecodestamper->prev_seek_seqnum = GST_SEQNUM_INVALID;
      }
      GST_OBJECT_UNLOCK (timecodestamper);

      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gboolean latency_changed;
      const gchar *interlace_mode;
      GstStructure *s;
      gint fps_n, fps_d;

      GST_OBJECT_LOCK (timecodestamper);
      gst_event_parse_caps (event, &caps);

      s = gst_caps_get_structure (caps, 0);

      if (!gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d)) {
        GST_ERROR_OBJECT (timecodestamper, "Expected framerate in caps");
        GST_OBJECT_UNLOCK (timecodestamper);
        gst_event_unref (event);
        return FALSE;
      }

      if (fps_n == 0) {
        GST_ERROR_OBJECT (timecodestamper,
            "Non-constant frame rate found. Refusing to create a timecode");
        GST_OBJECT_UNLOCK (timecodestamper);
        gst_event_unref (event);
        return FALSE;
      }

      if ((interlace_mode = gst_structure_get_string (s, "interlace-mode"))) {
        timecodestamper->interlace_mode =
            gst_video_interlace_mode_from_string (interlace_mode);
      }

      latency_changed =
          gst_timecodestamper_update_framerate (timecodestamper, fps_n, fps_d);

      timecodestamper->fps_n = fps_n;
      timecodestamper->fps_d = fps_d;

      GST_OBJECT_UNLOCK (timecodestamper);

      if (latency_changed)
        gst_element_post_message (GST_ELEMENT_CAST (timecodestamper),
            gst_message_new_latency (GST_OBJECT_CAST (timecodestamper)));
      break;
    }
#if HAVE_LTC
    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->video_flushing = TRUE;
      timecodestamper->video_current_running_time = GST_CLOCK_TIME_NONE;
      if (timecodestamper->video_clock_id)
        gst_clock_id_unschedule (timecodestamper->video_clock_id);
      g_cond_signal (&timecodestamper->ltc_cond_video);
      g_mutex_unlock (&timecodestamper->mutex);
      break;
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->video_flushing = FALSE;
      timecodestamper->video_eos = FALSE;
      g_mutex_unlock (&timecodestamper->mutex);
      break;
    case GST_EVENT_EOS:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->video_eos = TRUE;
      g_cond_signal (&timecodestamper->ltc_cond_audio);
      g_mutex_unlock (&timecodestamper->mutex);
      break;
#endif
    default:
      break;
  }
  ret =
      GST_BASE_TRANSFORM_CLASS (gst_timecodestamper_parent_class)->sink_event
      (trans, event);
  return ret;
}

static gboolean
gst_timecodestamper_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (trans);

  GST_DEBUG_OBJECT (trans, "received event %" GST_PTR_FORMAT, event);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstSeekType start_type;
      gint64 start;
      GstFormat format;

      gst_event_parse_seek (event, &rate, &format, NULL, &start_type, &start,
          NULL, NULL);

      if (rate < 0) {
        GST_ERROR_OBJECT (timecodestamper, "Reverse playback is not supported");
        return FALSE;
      }

      if (format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (timecodestamper,
            "Seeking is only supported in TIME format");
        return FALSE;
      }

      GST_OBJECT_LOCK (timecodestamper);
      if (timecodestamper->fps_d && timecodestamper->fps_n) {
        timecodestamper->prev_seek_seqnum = GST_EVENT_SEQNUM (event);
        timecodestamper->seeked_frames = gst_util_uint64_scale (start,
            timecodestamper->fps_n, timecodestamper->fps_d * GST_SECOND);
      }
      GST_OBJECT_UNLOCK (timecodestamper);
      break;
    }
    default:
      break;
  }

  return
      GST_BASE_TRANSFORM_CLASS (gst_timecodestamper_parent_class)->src_event
      (trans, event);
}

#if HAVE_LTC
static gboolean
gst_timecodestamper_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (trans);

  if (direction == GST_PAD_SINK)
    return
        GST_BASE_TRANSFORM_CLASS (gst_timecodestamper_parent_class)->query
        (trans, direction, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      gboolean res;
      gboolean live;
      GstClockTime min_latency, max_latency;
      GstClockTime latency;

      res =
          gst_pad_query_default (GST_BASE_TRANSFORM_SRC_PAD (trans),
          GST_OBJECT_CAST (trans), query);
      g_mutex_lock (&timecodestamper->mutex);
      if (res && timecodestamper->fps_n && timecodestamper->fps_d) {
        gst_query_parse_latency (query, &live, &min_latency, &max_latency);
        if (live && timecodestamper->ltcpad) {
          /* Introduce additional LTC for waiting for LTC timecodes. The
           * LTC library introduces some as well as the encoding of the LTC
           * signal. */
          latency = timecodestamper->ltc_extra_latency;
          min_latency += latency;
          if (max_latency != GST_CLOCK_TIME_NONE)
            max_latency += latency;
          timecodestamper->latency = min_latency;
          GST_DEBUG_OBJECT (timecodestamper,
              "Reporting latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT
              " ours %" GST_TIME_FORMAT, GST_TIME_ARGS (min_latency),
              GST_TIME_ARGS (max_latency), GST_TIME_ARGS (latency));
          gst_query_set_latency (query, live, min_latency, max_latency);
        } else {
          timecodestamper->latency = 0;
        }
      } else if (res) {
        GST_ERROR_OBJECT (timecodestamper,
            "Need a known, non-variable framerate to answer LATENCY query");
        res = FALSE;
        timecodestamper->latency = GST_CLOCK_TIME_NONE;
      }
      g_mutex_unlock (&timecodestamper->mutex);

      return res;
    }
    default:
      return
          GST_BASE_TRANSFORM_CLASS (gst_timecodestamper_parent_class)->query
          (trans, direction, query);
  }
}
#endif

static gboolean
remove_timecode_meta (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  if (meta && *meta && (*meta)->info->api == GST_VIDEO_TIME_CODE_META_API_TYPE) {
    *meta = NULL;
  }

  return TRUE;
}

#if HAVE_LTC
static void
gst_timecodestamper_update_latency (GstTimeCodeStamper * timecodestamper,
    GstPad * pad, gboolean * live, GstClockTime * latency)
{
  GstQuery *query;

  query = gst_query_new_latency ();
  if (!gst_pad_peer_query (pad, query)) {
    GST_WARNING_OBJECT (pad, "Failed to query latency");
    gst_pad_mark_reconfigure (pad);
    gst_query_unref (query);
    return;
  }

  g_mutex_lock (&timecodestamper->mutex);
  gst_query_parse_latency (query, live, latency, NULL);
  /* If we're not live, consider a latency of 0 */
  if (!*live)
    *latency = 0;
  GST_DEBUG_OBJECT (pad,
      "Queried latency: live %d, min latency %" GST_TIME_FORMAT, *live,
      GST_TIME_ARGS (*latency));
  g_mutex_unlock (&timecodestamper->mutex);
  gst_query_unref (query);
}
#endif

static GstFlowReturn
gst_timecodestamper_transform_ip (GstBaseTransform * vfilter,
    GstBuffer * buffer)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (vfilter);
  GstClockTime running_time, base_time, clock_time;
  GstClock *clock;
  GstClockTime clock_time_now;
  GDateTime *dt_now, *dt_frame;
  GstVideoTimeCode *tc = NULL;
  gboolean free_tc = FALSE;
  GstVideoTimeCodeMeta *tc_meta;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstVideoTimeCodeFlags tc_flags = 0;

  if (timecodestamper->fps_n == 0 || timecodestamper->fps_d == 0) {
    /* This can't actually happen I think - the caps template requires a framerate,
     * so we'd have to receive a buffer without prior caps */
    GST_ELEMENT_ERROR (timecodestamper, STREAM, FAILED,
        ("Can't process without a framerate"),
        ("Received a video buffer without framerate"));
    return GST_FLOW_ERROR;
  }
  if (!GST_BUFFER_PTS_IS_VALID (buffer)) {
    GST_ELEMENT_ERROR (timecodestamper, STREAM, FAILED,
        ("Input video buffer has no timestamp"),
        ("Video buffers must have timestamps"));
    return GST_FLOW_ERROR;
  }
#if HAVE_LTC
  if (timecodestamper->video_latency == -1
      || gst_pad_check_reconfigure (GST_BASE_TRANSFORM_SINK_PAD (vfilter))) {
    gst_timecodestamper_update_latency (timecodestamper,
        GST_BASE_TRANSFORM_SINK_PAD (vfilter), &timecodestamper->video_live,
        &timecodestamper->video_latency);
  }
#endif

  /* Collect all the current times */
  base_time = gst_element_get_base_time (GST_ELEMENT (timecodestamper));
  clock = gst_element_get_clock (GST_ELEMENT (timecodestamper));
  if (clock) {
    clock_time_now = gst_clock_get_time (clock);
    gst_object_unref (clock);
  } else {
    clock_time_now = GST_CLOCK_TIME_NONE;
  }

  dt_now = g_date_time_new_now_local ();

  running_time =
      gst_segment_to_running_time (&vfilter->segment, GST_FORMAT_TIME,
      GST_BUFFER_PTS (buffer));

  if (clock_time_now != GST_CLOCK_TIME_NONE) {
    gdouble seconds_diff;

    clock_time = running_time + base_time;
    if (clock_time_now > clock_time) {
      seconds_diff = (clock_time_now - clock_time) / -1000000000.0;
    } else {
      seconds_diff = (clock_time - clock_time_now) / 1000000000.0;
    }
    dt_frame = g_date_time_add_seconds (dt_now, seconds_diff);
  } else {
    /* If we have no clock we can't really know the time of the frame */
    dt_frame = g_date_time_ref (dt_now);
  }

  GST_DEBUG_OBJECT (timecodestamper,
      "Handling video frame with running time %" GST_TIME_FORMAT,
      GST_TIME_ARGS (running_time));

  tc_meta = gst_buffer_get_video_time_code_meta (buffer);

  /* Update all our internal timecodes as needed */
  GST_OBJECT_LOCK (timecodestamper);

  if (timecodestamper->interlace_mode != GST_VIDEO_INTERLACE_MODE_PROGRESSIVE)
    tc_flags |= GST_VIDEO_TIME_CODE_FLAGS_INTERLACED;

  if (timecodestamper->drop_frame && timecodestamper->fps_d == 1001 &&
      (timecodestamper->fps_n == 30000 || timecodestamper->fps_n == 60000))
    tc_flags |= GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;

  /* If we don't have an internal timecode yet then either a new one was just
   * set via the property or we just started. Initialize it here, otherwise
   * increment it by one */
  if (!timecodestamper->internal_tc
      || timecodestamper->reset_internal_tc_from_seek) {
    gchar *tc_str;

    if (timecodestamper->internal_tc)
      gst_video_time_code_free (timecodestamper->internal_tc);

    timecodestamper->reset_internal_tc_from_seek = FALSE;
    if (timecodestamper->set_internal_tc) {
      timecodestamper->internal_tc =
          gst_video_time_code_new (timecodestamper->fps_n,
          timecodestamper->fps_d,
          timecodestamper->set_internal_tc->config.latest_daily_jam, tc_flags,
          timecodestamper->set_internal_tc->hours,
          timecodestamper->set_internal_tc->minutes,
          timecodestamper->set_internal_tc->seconds,
          timecodestamper->set_internal_tc->frames,
          timecodestamper->set_internal_tc->field_count);
    } else {
      timecodestamper->internal_tc =
          gst_video_time_code_new (timecodestamper->fps_n,
          timecodestamper->fps_d, dt_frame, tc_flags, 0, 0, 0, 0, 0);
      if (timecodestamper->seeked_frames > 0) {
        GST_DEBUG_OBJECT (timecodestamper,
            "Adding %" G_GINT64_FORMAT " frames that were seeked",
            timecodestamper->seeked_frames);
        gst_video_time_code_add_frames (timecodestamper->internal_tc,
            timecodestamper->seeked_frames);
        timecodestamper->seeked_frames = -1;
      }
    }

    tc_str = gst_video_time_code_to_string (timecodestamper->internal_tc);
    GST_DEBUG_OBJECT (timecodestamper, "Initialized internal timecode to %s",
        tc_str);
    g_free (tc_str);
  } else {
    gchar *tc_str;

    gst_video_time_code_increment_frame (timecodestamper->internal_tc);
    tc_str = gst_video_time_code_to_string (timecodestamper->internal_tc);
    GST_DEBUG_OBJECT (timecodestamper, "Incremented internal timecode to %s",
        tc_str);
    g_free (tc_str);
  }

  /* If we have a new timecode on the incoming frame, update our last known
   * timecode or otherwise increment it by one */
  if (tc_meta && (!timecodestamper->last_tc || timecodestamper->tc_auto_resync)) {
    gchar *tc_str;

    if (timecodestamper->last_tc)
      gst_video_time_code_free (timecodestamper->last_tc);
    timecodestamper->last_tc = gst_video_time_code_copy (&tc_meta->tc);
    timecodestamper->last_tc_running_time = running_time;

    tc_str = gst_video_time_code_to_string (timecodestamper->last_tc);
    GST_DEBUG_OBJECT (timecodestamper, "Updated upstream timecode to %s",
        tc_str);
    g_free (tc_str);
  } else {
    if (timecodestamper->last_tc) {
      if (timecodestamper->tc_auto_resync
          && timecodestamper->tc_timeout != GST_CLOCK_TIME_NONE
          && (running_time + timecodestamper->tc_timeout <
              timecodestamper->last_tc_running_time
              || running_time >=
              timecodestamper->last_tc_running_time +
              timecodestamper->tc_timeout)) {
        if (timecodestamper->last_tc)
          gst_video_time_code_free (timecodestamper->last_tc);
        timecodestamper->last_tc = NULL;
        timecodestamper->last_tc_running_time = GST_CLOCK_TIME_NONE;
        GST_DEBUG_OBJECT (timecodestamper, "Upstream timecode timed out");
      } else {
        gchar *tc_str;

        gst_video_time_code_increment_frame (timecodestamper->last_tc);

        tc_str = gst_video_time_code_to_string (timecodestamper->last_tc);
        GST_DEBUG_OBJECT (timecodestamper,
            "Incremented upstream timecode to %s", tc_str);
        g_free (tc_str);
      }
    } else {
      GST_DEBUG_OBJECT (timecodestamper, "Never saw an upstream timecode");
    }
  }

  /* Update RTC-based timecode */
  {
    GstVideoTimeCode rtc_timecode_now;
    gchar *tc_str, *dt_str;

    /* Create timecode for the current frame time */
    memset (&rtc_timecode_now, 0, sizeof (rtc_timecode_now));
    gst_video_time_code_init_from_date_time_full (&rtc_timecode_now,
        timecodestamper->fps_n, timecodestamper->fps_d, dt_frame, tc_flags, 0);

    tc_str = gst_video_time_code_to_string (&rtc_timecode_now);
    dt_str = g_date_time_format (dt_frame, "%F %R %z");
    GST_DEBUG_OBJECT (timecodestamper,
        "Created RTC timecode %s for %s (%06u us)", tc_str, dt_str,
        g_date_time_get_microsecond (dt_frame));
    g_free (dt_str);
    g_free (tc_str);

    /* If we don't have an RTC timecode yet, directly initialize with this one */
    if (!timecodestamper->rtc_tc) {
      timecodestamper->rtc_tc = gst_video_time_code_copy (&rtc_timecode_now);
      tc_str = gst_video_time_code_to_string (timecodestamper->rtc_tc);
      GST_DEBUG_OBJECT (timecodestamper, "Initialized RTC timecode to %s",
          tc_str);
      g_free (tc_str);
    } else {
      GstClockTime rtc_now_time, rtc_tc_time;
      GstClockTime rtc_diff;

      /* Increment the old RTC timecode to this frame */
      gst_video_time_code_increment_frame (timecodestamper->rtc_tc);

      /* Otherwise check if we drifted too much and need to resync */
      rtc_tc_time =
          gst_video_time_code_nsec_since_daily_jam (timecodestamper->rtc_tc);
      rtc_now_time =
          gst_video_time_code_nsec_since_daily_jam (&rtc_timecode_now);
      if (rtc_tc_time > rtc_now_time)
        rtc_diff = rtc_tc_time - rtc_now_time;
      else
        rtc_diff = rtc_now_time - rtc_tc_time;

      if (timecodestamper->rtc_auto_resync
          && timecodestamper->rtc_max_drift != GST_CLOCK_TIME_NONE
          && rtc_diff > timecodestamper->rtc_max_drift) {
        gst_video_time_code_free (timecodestamper->rtc_tc);
        timecodestamper->rtc_tc = gst_video_time_code_copy (&rtc_timecode_now);
        tc_str = gst_video_time_code_to_string (timecodestamper->rtc_tc);
        GST_DEBUG_OBJECT (timecodestamper,
            "Updated RTC timecode to %s (%s%" GST_TIME_FORMAT " drift)", tc_str,
            (rtc_tc_time > rtc_now_time ? "-" : "+"), GST_TIME_ARGS (rtc_diff));
        g_free (tc_str);
      } else {
        /* Else nothing to do here, we use the current one */
        tc_str = gst_video_time_code_to_string (timecodestamper->rtc_tc);
        GST_DEBUG_OBJECT (timecodestamper,
            "Incremented RTC timecode to %s (%s%" GST_TIME_FORMAT " drift)",
            tc_str, (rtc_tc_time > rtc_now_time ? "-" : "+"),
            GST_TIME_ARGS (rtc_diff));
        g_free (tc_str);
      }
    }

    gst_video_time_code_clear (&rtc_timecode_now);
  }
  GST_OBJECT_UNLOCK (timecodestamper);

  /* Update LTC-based timecode as needed */
#if HAVE_LTC
  if (timecodestamper->ltcpad) {
    GstClockTime frame_duration;
    gchar *tc_str;
    TimestampedTimecode *ltc_tc;
    gboolean updated_internal = FALSE;

    frame_duration = gst_util_uint64_scale_int_ceil (GST_SECOND,
        timecodestamper->fps_d, timecodestamper->fps_n);

    g_mutex_lock (&timecodestamper->mutex);

    timecodestamper->video_current_running_time = running_time;

    /* Wait to compensate for the latency we introduce and to allow the LTC
     * audio to provide enough audio to extract timecodes, or until the video
     * pad is flushing or the LTC pad is EOS.
     * In non-live mode we introduce 4 frames of latency compared to the LTC
     * audio, see LATENCY query handling for details. */
    if (timecodestamper->video_live) {
      GstClock *clock =
          gst_element_get_clock (GST_ELEMENT_CAST (timecodestamper));

      if (clock) {
        GstClockID clock_id;
        GstClockTime base_time =
            gst_element_get_base_time (GST_ELEMENT_CAST (timecodestamper));
        GstClockTime wait_time;

        /* If we have no latency yet then wait at least for the LTC extra
         * latency. See LATENCY query handling for details. */
        if (timecodestamper->latency == GST_CLOCK_TIME_NONE) {
          wait_time =
              base_time + running_time + timecodestamper->ltc_extra_latency;
        } else {
          wait_time = base_time + running_time + timecodestamper->latency;
        }

        GST_TRACE_OBJECT (timecodestamper,
            "Waiting for clock to reach %" GST_TIME_FORMAT
            " (base time %" GST_TIME_FORMAT
            " + running time %" GST_TIME_FORMAT
            " + latency %" GST_TIME_FORMAT
            "), now %" GST_TIME_FORMAT,
            GST_TIME_ARGS (wait_time),
            GST_TIME_ARGS (base_time),
            GST_TIME_ARGS (running_time),
            GST_TIME_ARGS (timecodestamper->latency ==
                GST_CLOCK_TIME_NONE ? timecodestamper->ltc_extra_latency :
                timecodestamper->latency),
            GST_TIME_ARGS (gst_clock_get_time (clock))
            );
        clock_id = gst_clock_new_single_shot_id (clock, wait_time);

        timecodestamper->video_clock_id = clock_id;
        g_mutex_unlock (&timecodestamper->mutex);
        gst_clock_id_wait (clock_id, NULL);
        g_mutex_lock (&timecodestamper->mutex);
        timecodestamper->video_clock_id = NULL;
        gst_clock_id_unref (clock_id);
        gst_object_unref (clock);
      } else {
        GST_WARNING_OBJECT (timecodestamper,
            "No clock in live mode, not waiting");
      }
    } else {
      while ((timecodestamper->ltc_current_running_time == GST_CLOCK_TIME_NONE
              || timecodestamper->ltc_current_running_time <
              running_time + 8 * frame_duration)
          && !timecodestamper->video_flushing && !timecodestamper->ltc_eos) {
        GST_TRACE_OBJECT (timecodestamper,
            "Waiting for LTC audio to advance, EOS or flushing");
        g_cond_wait (&timecodestamper->ltc_cond_video, &timecodestamper->mutex);
      }
    }

    if (timecodestamper->video_flushing) {
      g_mutex_unlock (&timecodestamper->mutex);
      flow_ret = GST_FLOW_FLUSHING;
      goto out;
    }

    GST_OBJECT_LOCK (timecodestamper);
    /* Take timecodes out of the queue until we're at the current video
     * position. */
    while ((ltc_tc = g_queue_pop_head (&timecodestamper->ltc_current_tcs))) {
      /* First update framerate and flags according to the video stream if not
       * done yet */
      if (ltc_tc->timecode.config.fps_d == 0) {
        gint fps_n_div =
            ((gdouble) timecodestamper->fps_n) /
            timecodestamper->fps_d > 30 ? 2 : 1;

        ltc_tc->timecode.config.flags = tc_flags;
        ltc_tc->timecode.config.fps_n = timecodestamper->fps_n / fps_n_div;
        ltc_tc->timecode.config.fps_d = timecodestamper->fps_d;
      }

      tc_str = gst_video_time_code_to_string (&ltc_tc->timecode);
      GST_INFO_OBJECT (timecodestamper,
          "Retrieved LTC timecode %s at %" GST_TIME_FORMAT
          " (%u timecodes queued)", tc_str,
          GST_TIME_ARGS (ltc_tc->running_time),
          g_queue_get_length (&timecodestamper->ltc_current_tcs));
      g_free (tc_str);

      if (!gst_video_time_code_is_valid (&ltc_tc->timecode)) {
        tc_str = gst_video_time_code_to_string (&ltc_tc->timecode);
        GST_INFO_OBJECT (timecodestamper, "Invalid LTC timecode %s", tc_str);
        g_free (tc_str);
        gst_video_time_code_clear (&ltc_tc->timecode);
        g_free (ltc_tc);
        ltc_tc = NULL;
        continue;
      }

      /* A timecode frame that starts +/- half a frame to the
       * video frame is considered belonging to that video frame.
       *
       * If it's further ahead than half a frame duration, break out of
       * the loop here and reconsider on the next frame. */
      if (ABSDIFF (running_time, ltc_tc->running_time) <= frame_duration / 2) {
        /* If we're resyncing LTC in general, directly replace the current
         * LTC timecode with the new one we read. Otherwise we'll continue
         * counting based on the previous timecode we had
         */
        if (timecodestamper->ltc_auto_resync) {
          if (timecodestamper->ltc_internal_tc)
            gst_video_time_code_free (timecodestamper->ltc_internal_tc);
          timecodestamper->ltc_internal_tc =
              gst_video_time_code_copy (&ltc_tc->timecode);
          timecodestamper->ltc_internal_running_time = ltc_tc->running_time;
          updated_internal = TRUE;
          GST_INFO_OBJECT (timecodestamper, "Resynced internal LTC counter");
        }

        /* And store it back for the next frame in case it has more or less
         * the same running time */
        g_queue_push_head (&timecodestamper->ltc_current_tcs,
            g_steal_pointer (&ltc_tc));
        break;
      } else if (ltc_tc->running_time > running_time
          && ltc_tc->running_time - running_time > frame_duration / 2) {
        /* Store it back for the next frame */
        g_queue_push_head (&timecodestamper->ltc_current_tcs,
            g_steal_pointer (&ltc_tc));
        ltc_tc = NULL;
        break;
      }

      /* otherwise it's in the past and we need to consider the next
       * timecode. Read a new one */
      gst_video_time_code_clear (&ltc_tc->timecode);
      g_free (ltc_tc);
      ltc_tc = NULL;
    }

    /* If we didn't update from LTC above, increment our internal timecode
     * for this frame */
    if (!updated_internal && timecodestamper->ltc_internal_tc) {
      gst_video_time_code_increment_frame (timecodestamper->ltc_internal_tc);
    }

    if (timecodestamper->ltc_internal_tc) {
      if (timecodestamper->ltc_auto_resync
          && timecodestamper->ltc_timeout != GST_CLOCK_TIME_NONE
          && (running_time + timecodestamper->ltc_timeout <
              timecodestamper->ltc_internal_running_time
              || running_time >=
              timecodestamper->ltc_internal_running_time +
              timecodestamper->ltc_timeout)) {
        if (timecodestamper->ltc_internal_tc)
          gst_video_time_code_free (timecodestamper->ltc_internal_tc);
        timecodestamper->ltc_internal_tc = NULL;
        GST_DEBUG_OBJECT (timecodestamper, "LTC timecode timed out");
        timecodestamper->ltc_internal_running_time = GST_CLOCK_TIME_NONE;
      } else {
        tc_str =
            gst_video_time_code_to_string (timecodestamper->ltc_internal_tc);
        GST_DEBUG_OBJECT (timecodestamper, "Updated LTC timecode to %s",
            tc_str);
        g_free (tc_str);
      }
    } else {
      GST_DEBUG_OBJECT (timecodestamper, "Have no LTC timecode yet");
    }

    GST_OBJECT_UNLOCK (timecodestamper);

    g_cond_signal (&timecodestamper->ltc_cond_audio);

    g_mutex_unlock (&timecodestamper->mutex);
  }
#endif

  GST_OBJECT_LOCK (timecodestamper);
  switch (timecodestamper->tc_source) {
    case GST_TIME_CODE_STAMPER_SOURCE_INTERNAL:
      tc = timecodestamper->internal_tc;
      break;
    case GST_TIME_CODE_STAMPER_SOURCE_ZERO:
      tc = gst_video_time_code_new (timecodestamper->fps_n,
          timecodestamper->fps_d, NULL, tc_flags, 0, 0, 0, 0, 0);
      free_tc = TRUE;
      break;
    case GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN:
      tc = timecodestamper->last_tc;
      if (!tc)
        tc = timecodestamper->internal_tc;
      break;
    case GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN_OR_ZERO:
      tc = timecodestamper->last_tc;
      if (!tc) {
        tc = gst_video_time_code_new (timecodestamper->fps_n,
            timecodestamper->fps_d, NULL, tc_flags, 0, 0, 0, 0, 0);
        free_tc = TRUE;
      }
      break;
    case GST_TIME_CODE_STAMPER_SOURCE_LTC:
#if HAVE_LTC
      if (timecodestamper->ltc_internal_tc)
        tc = timecodestamper->ltc_internal_tc;
#endif
      if (!tc) {
        tc = gst_video_time_code_new (timecodestamper->fps_n,
            timecodestamper->fps_d, NULL, tc_flags, 0, 0, 0, 0, 0);
        free_tc = TRUE;
      }
      break;
    case GST_TIME_CODE_STAMPER_SOURCE_RTC:
      tc = timecodestamper->rtc_tc;
      break;
  }

  switch (timecodestamper->tc_set) {
    case GST_TIME_CODE_STAMPER_SET_NEVER:
      break;
    case GST_TIME_CODE_STAMPER_SET_KEEP:
      if (!tc_meta && tc) {
        gchar *tc_str;

        if (timecodestamper->timecode_offset) {
          if (!free_tc) {
            tc = gst_video_time_code_copy (tc);
            free_tc = TRUE;
          }
          gst_video_time_code_add_frames (tc, timecodestamper->timecode_offset);
        }

        tc_str = gst_video_time_code_to_string (tc);
        GST_DEBUG_OBJECT (timecodestamper, "Storing timecode %s", tc_str);
        g_free (tc_str);

        gst_buffer_add_video_time_code_meta (buffer, tc);
      }
      break;
    case GST_TIME_CODE_STAMPER_SET_ALWAYS:
      gst_buffer_foreach_meta (buffer, remove_timecode_meta, NULL);
      if (tc) {
        gchar *tc_str;

        if (timecodestamper->timecode_offset) {
          if (!free_tc) {
            tc = gst_video_time_code_copy (tc);
            free_tc = TRUE;
          }
          gst_video_time_code_add_frames (tc, timecodestamper->timecode_offset);
        }

        tc_str = gst_video_time_code_to_string (tc);
        GST_DEBUG_OBJECT (timecodestamper, "Storing timecode %s", tc_str);
        g_free (tc_str);

        gst_buffer_add_video_time_code_meta (buffer, tc);
      }
      break;
  }

  GST_OBJECT_UNLOCK (timecodestamper);

  if (timecodestamper->post_messages && tc) {
    GstClockTime stream_time, running_time, duration;
    GstStructure *s;
    GstMessage *msg;

    running_time =
        gst_segment_to_running_time (&vfilter->segment, GST_FORMAT_TIME,
        GST_BUFFER_PTS (buffer));
    stream_time =
        gst_segment_to_stream_time (&vfilter->segment, GST_FORMAT_TIME,
        GST_BUFFER_PTS (buffer));
    duration =
        gst_util_uint64_scale_int (GST_SECOND, timecodestamper->fps_d,
        timecodestamper->fps_n);
    s = gst_structure_new ("timecodestamper", "timestamp", G_TYPE_UINT64,
        GST_BUFFER_PTS (buffer), "stream-time", G_TYPE_UINT64, stream_time,
        "running-time", G_TYPE_UINT64, running_time, "duration",
        G_TYPE_UINT64, duration, "timecode", GST_TYPE_VIDEO_TIME_CODE, tc,
        NULL);
    msg = gst_message_new_element (GST_OBJECT (timecodestamper), s);
    gst_element_post_message (GST_ELEMENT (timecodestamper), msg);
  }
#if HAVE_LTC
out:
#endif

  if (dt_now)
    g_date_time_unref (dt_now);
  if (dt_frame)
    g_date_time_unref (dt_frame);
  if (free_tc && tc)
    gst_video_time_code_free (tc);

  return flow_ret;
}

static GstPad *
gst_timecodestamper_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name_templ, const GstCaps * caps)
{
#if HAVE_LTC
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (element);

  GST_OBJECT_LOCK (timecodestamper);
  if (timecodestamper->ltcpad) {
    GST_OBJECT_UNLOCK (timecodestamper);
    return NULL;
  }

  if (GST_STATE (timecodestamper) > GST_STATE_READY ||
      GST_STATE_TARGET (timecodestamper) > GST_STATE_READY) {
    GST_ERROR_OBJECT (timecodestamper,
        "LTC audio pad can only be requested in NULL or READY state");
    GST_OBJECT_UNLOCK (timecodestamper);
    return NULL;
  }

  timecodestamper->ltcpad = gst_pad_new_from_static_template
      (&gst_timecodestamper_ltc_template, "ltc_sink");

  gst_pad_set_chain_function (timecodestamper->ltcpad,
      GST_DEBUG_FUNCPTR (gst_timecodestamper_ltcpad_chain));
  gst_pad_set_event_function (timecodestamper->ltcpad,
      GST_DEBUG_FUNCPTR (gst_timecodestamper_ltcpad_event));
  gst_pad_set_query_function (timecodestamper->ltcpad,
      GST_DEBUG_FUNCPTR (gst_timecodestamper_ltcpad_query));
  gst_pad_set_activatemode_function (timecodestamper->ltcpad,
      GST_DEBUG_FUNCPTR (gst_timecodestamper_ltcpad_activatemode));

  GST_OBJECT_UNLOCK (timecodestamper);

  g_mutex_lock (&timecodestamper->mutex);
  timecodestamper->audio_live = FALSE;
  timecodestamper->audio_latency = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&timecodestamper->mutex);

  gst_element_add_pad (element, timecodestamper->ltcpad);

  gst_element_post_message (GST_ELEMENT_CAST (timecodestamper),
      gst_message_new_latency (GST_OBJECT_CAST (timecodestamper)));

  return timecodestamper->ltcpad;
#else
  return NULL;
#endif
}

static void
gst_timecodestamper_release_pad (GstElement * element, GstPad * pad)
{
#if HAVE_LTC
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (element);

  GST_OBJECT_LOCK (timecodestamper);
  if (timecodestamper->ltcpad != pad) {
    GST_OBJECT_UNLOCK (timecodestamper);
    return;
  }

  timecodestamper->ltcpad = NULL;

  if (timecodestamper->ltc_internal_tc != NULL) {
    gst_video_time_code_free (timecodestamper->ltc_internal_tc);
    timecodestamper->ltc_internal_tc = NULL;
  }
  timecodestamper->ltc_internal_running_time = GST_CLOCK_TIME_NONE;

  {
    TimestampedTimecode *tc;
    while ((tc = g_queue_pop_tail (&timecodestamper->ltc_current_tcs))) {
      gst_video_time_code_clear (&tc->timecode);
      g_free (tc);
    }
  }
  GST_OBJECT_UNLOCK (timecodestamper);

  gst_pad_set_active (pad, FALSE);

  g_mutex_lock (&timecodestamper->mutex);
  timecodestamper->ltc_flushing = TRUE;
  timecodestamper->ltc_eos = TRUE;
  g_cond_signal (&timecodestamper->ltc_cond_video);
  g_cond_signal (&timecodestamper->ltc_cond_audio);

  gst_audio_info_init (&timecodestamper->ainfo);
  gst_segment_init (&timecodestamper->ltc_segment, GST_FORMAT_UNDEFINED);

  timecodestamper->ltc_first_running_time = GST_CLOCK_TIME_NONE;
  timecodestamper->ltc_current_running_time = GST_CLOCK_TIME_NONE;

  if (timecodestamper->ltc_dec) {
    ltc_decoder_free (timecodestamper->ltc_dec);
    timecodestamper->ltc_dec = NULL;
  }

  if (timecodestamper->stream_align) {
    gst_audio_stream_align_free (timecodestamper->stream_align);
    timecodestamper->stream_align = NULL;
  }

  timecodestamper->ltc_total = 0;

  timecodestamper->audio_live = FALSE;
  timecodestamper->audio_latency = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&timecodestamper->mutex);

  gst_element_post_message (GST_ELEMENT_CAST (timecodestamper),
      gst_message_new_latency (GST_OBJECT_CAST (timecodestamper)));

  gst_element_remove_pad (element, pad);
#endif
}

#if HAVE_LTC
static GstFlowReturn
gst_timecodestamper_ltcpad_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn fr = GST_FLOW_OK;
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (parent);
  GstMapInfo map;
  GstClockTime timestamp, running_time, duration;
  guint nsamples;
  gboolean discont;

  if (timecodestamper->audio_latency == -1 || gst_pad_check_reconfigure (pad)) {
    gst_timecodestamper_update_latency (timecodestamper, pad,
        &timecodestamper->audio_live, &timecodestamper->audio_latency);
  }

  g_mutex_lock (&timecodestamper->mutex);
  if (timecodestamper->ltc_flushing) {
    g_mutex_unlock (&timecodestamper->mutex);
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }

  nsamples = gst_buffer_get_size (buffer) /
      GST_AUDIO_INFO_BPF (&timecodestamper->ainfo);

  if (!timecodestamper->stream_align) {
    timecodestamper->stream_align =
        gst_audio_stream_align_new (timecodestamper->ainfo.rate,
        500 * GST_MSECOND, 20 * GST_MSECOND);
  }

  discont =
      gst_audio_stream_align_process (timecodestamper->stream_align,
      GST_BUFFER_IS_DISCONT (buffer), GST_BUFFER_PTS (buffer), nsamples,
      &timestamp, &duration, NULL);

  if (discont) {
    if (timecodestamper->ltc_dec) {
      GST_WARNING_OBJECT (timecodestamper, "Got discont at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (timestamp));
      ltc_decoder_queue_flush (timecodestamper->ltc_dec);
    }
    timecodestamper->ltc_total = 0;
  }

  if (!timecodestamper->ltc_dec) {
    gint samples_per_frame = 1920;

    GST_OBJECT_LOCK (timecodestamper);
    /* This is only for initialization and needs to be somewhat close to the
     * real value. It will be tracked automatically afterwards */
    if (timecodestamper->fps_n) {
      samples_per_frame = timecodestamper->ainfo.rate *
          timecodestamper->fps_d / timecodestamper->fps_n;
    }
    GST_OBJECT_UNLOCK (timecodestamper);

    timecodestamper->ltc_dec =
        ltc_decoder_create (samples_per_frame, DEFAULT_LTC_QUEUE);
    timecodestamper->ltc_total = 0;
  }

  running_time = gst_segment_to_running_time (&timecodestamper->ltc_segment,
      GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (timecodestamper,
      "Handling LTC audio buffer at %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT
      " (offset %" G_GUINT64_FORMAT ")",
      GST_TIME_ARGS (running_time),
      GST_TIME_ARGS (running_time + duration),
      (guint64) timecodestamper->ltc_total);

  if (timecodestamper->ltc_total == 0) {
    timecodestamper->ltc_first_running_time = running_time;
  }

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  ltc_decoder_write (timecodestamper->ltc_dec, map.data, map.size,
      timecodestamper->ltc_total);
  timecodestamper->ltc_total += map.size;
  gst_buffer_unmap (buffer, &map);

  /* Now read all the timecodes from the decoder that are currently available
   * and store them in our own queue, which gives us more control over how
   * things are working. */
  {
    LTCFrameExt ltc_frame;

    while (ltc_decoder_read (timecodestamper->ltc_dec, &ltc_frame) == 1) {
      SMPTETimecode stc;
      TimestampedTimecode *ltc_tc;
      GstClockTime ltc_running_time;

      if (ltc_frame.off_start < 0) {
        GstClockTime offset =
            gst_util_uint64_scale (GST_SECOND, -ltc_frame.off_start,
            timecodestamper->ainfo.rate);

        if (offset > timecodestamper->ltc_first_running_time)
          ltc_running_time = 0;
        else
          ltc_running_time = timecodestamper->ltc_first_running_time - offset;
      } else {
        ltc_running_time = timecodestamper->ltc_first_running_time +
            gst_util_uint64_scale (GST_SECOND, ltc_frame.off_start,
            timecodestamper->ainfo.rate);
      }

      ltc_frame_to_time (&stc, &ltc_frame.ltc, 0);
      GST_INFO_OBJECT (timecodestamper,
          "Got LTC timecode %02d:%02d:%02d:%02d at %" GST_TIME_FORMAT,
          stc.hours, stc.mins, stc.secs, stc.frame,
          GST_TIME_ARGS (ltc_running_time));

      ltc_tc = g_new0 (TimestampedTimecode, 1);
      ltc_tc->running_time = ltc_running_time;
      /* We fill in the framerate and other metadata later */
      gst_video_time_code_init (&ltc_tc->timecode,
          0, 0, timecodestamper->ltc_daily_jam, 0,
          stc.hours, stc.mins, stc.secs, stc.frame, 0);

      /* If we have a discontinuity it might happen that we're getting
       * timecodes that are in the past relative to timecodes we already have
       * in our queue. We have to get rid of all the timecodes that are in the
       * future now. */
      if (discont) {
        TimestampedTimecode *tmp;

        while ((tmp = g_queue_peek_tail (&timecodestamper->ltc_current_tcs)) &&
            tmp->running_time >= ltc_running_time) {
          gst_video_time_code_clear (&tmp->timecode);
          g_free (tmp);
          g_queue_pop_tail (&timecodestamper->ltc_current_tcs);
        }

        g_queue_push_tail (&timecodestamper->ltc_current_tcs,
            g_steal_pointer (&ltc_tc));
      } else {
        g_queue_push_tail (&timecodestamper->ltc_current_tcs,
            g_steal_pointer (&ltc_tc));
      }
    }
  }

  /* Notify the video streaming thread that new data is available */
  g_cond_signal (&timecodestamper->ltc_cond_video);

  /* Wait until video has caught up, if needed */
  if (timecodestamper->audio_live) {
    /* In live-mode, do no waiting as we're guaranteed to be more or less in
     * sync (~latency) with the video */
  } else {
    /* If we're ahead of the video, wait until the video has caught up.
     * Otherwise don't wait and drop any too old items from the ringbuffer */
    while ((timecodestamper->video_current_running_time == GST_CLOCK_TIME_NONE
            || running_time + duration >=
            timecodestamper->video_current_running_time)
        && timecodestamper->ltc_dec
        && g_queue_get_length (&timecodestamper->ltc_current_tcs) >
        DEFAULT_LTC_QUEUE / 2 && !timecodestamper->video_eos
        && !timecodestamper->ltc_flushing) {
      GST_TRACE_OBJECT (timecodestamper,
          "Waiting for video to advance, EOS or flushing");
      g_cond_wait (&timecodestamper->ltc_cond_audio, &timecodestamper->mutex);
    }
  }

  if (timecodestamper->ltc_flushing)
    fr = GST_FLOW_FLUSHING;
  else
    fr = GST_FLOW_OK;

  g_mutex_unlock (&timecodestamper->mutex);

  gst_buffer_unref (buffer);
  return fr;
}

static gboolean
gst_timecodestamper_ltcpad_event (GstPad * pad,
    GstObject * parent, GstEvent * event)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (parent);

  GstCaps *caps;
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);

      if (!gst_audio_info_from_caps (&timecodestamper->ainfo, caps)) {
        gst_event_unref (event);
        return FALSE;
      }

      if (timecodestamper->stream_align) {
        gst_audio_stream_align_set_rate (timecodestamper->stream_align,
            timecodestamper->ainfo.rate);
      }

      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &timecodestamper->ltc_segment);
      break;

    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->ltc_flushing = TRUE;
      g_cond_signal (&timecodestamper->ltc_cond_audio);
      g_mutex_unlock (&timecodestamper->mutex);
      break;
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->ltc_flushing = FALSE;
      timecodestamper->ltc_eos = FALSE;
      gst_segment_init (&timecodestamper->ltc_segment, GST_FORMAT_UNDEFINED);
      g_mutex_unlock (&timecodestamper->mutex);
      break;
    case GST_EVENT_EOS:
      g_mutex_lock (&timecodestamper->mutex);
      timecodestamper->ltc_eos = TRUE;
      g_cond_signal (&timecodestamper->ltc_cond_video);
      g_mutex_unlock (&timecodestamper->mutex);
      break;

    default:
      break;
  }

  gst_event_unref (event);
  return ret;
}

static gboolean
gst_timecodestamper_ltcpad_query (GstPad * pad,
    GstObject * parent, GstQuery * query)
{
  GstCaps *caps, *filter, *tcaps;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      gst_query_parse_caps (query, &filter);
      tcaps = gst_pad_get_pad_template_caps (pad);
      if (filter)
        caps = gst_caps_intersect_full (tcaps, filter,
            GST_CAPS_INTERSECT_FIRST);
      else
        caps = gst_caps_ref (tcaps);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (tcaps);
      gst_caps_unref (caps);
      return TRUE;
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static gboolean
gst_timecodestamper_ltcpad_activatemode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (parent);

  if (active) {
    g_mutex_lock (&timecodestamper->mutex);
    timecodestamper->ltc_flushing = FALSE;
    timecodestamper->ltc_eos = FALSE;
    timecodestamper->audio_live = FALSE;
    timecodestamper->audio_latency = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (&timecodestamper->mutex);
  } else {
    g_mutex_lock (&timecodestamper->mutex);
    timecodestamper->ltc_flushing = TRUE;
    timecodestamper->ltc_eos = TRUE;
    g_cond_signal (&timecodestamper->ltc_cond_audio);
    g_mutex_unlock (&timecodestamper->mutex);
  }

  return TRUE;
}

static gboolean
gst_timecodestamper_videopad_activatemode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (parent);

  if (active) {
    g_mutex_lock (&timecodestamper->mutex);
    timecodestamper->video_flushing = FALSE;
    timecodestamper->video_eos = FALSE;
    timecodestamper->video_live = FALSE;
    timecodestamper->video_latency = GST_CLOCK_TIME_NONE;
    timecodestamper->video_current_running_time = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (&timecodestamper->mutex);
  } else {
    g_mutex_lock (&timecodestamper->mutex);
    timecodestamper->video_flushing = TRUE;
    timecodestamper->video_current_running_time = GST_CLOCK_TIME_NONE;
    if (timecodestamper->video_clock_id)
      gst_clock_id_unschedule (timecodestamper->video_clock_id);
    g_cond_signal (&timecodestamper->ltc_cond_video);
    g_mutex_unlock (&timecodestamper->mutex);
  }

  return timecodestamper->video_activatemode_default (pad, parent, mode,
      active);
}

static GstIterator *
gst_timecodestamper_src_iterate_internal_link (GstPad * pad, GstObject * parent)
{
  GstTimeCodeStamper *timecodestamper = GST_TIME_CODE_STAMPER (parent);
  GValue value = G_VALUE_INIT;
  GstIterator *it;

  g_value_init (&value, GST_TYPE_PAD);
  g_value_set_object (&value, GST_BASE_TRANSFORM_SINK_PAD (timecodestamper));
  it = gst_iterator_new_single (GST_TYPE_PAD, &value);
  g_value_unset (&value);

  return it;
}
#endif
