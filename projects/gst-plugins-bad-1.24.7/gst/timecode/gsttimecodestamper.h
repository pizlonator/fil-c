/*
 * GStreamer
 * Copyright (C) 2016 Vivia Nikolaidou <vivia@toolsonair.com>
 *
 * gsttimecodestamper.h
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

#ifndef __GST_TIME_CODE_STAMPER_H__
#define __GST_TIME_CODE_STAMPER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

#if HAVE_LTC
#include <ltc.h>
#endif

#define GST_TYPE_TIME_CODE_STAMPER            (gst_timecodestamper_get_type())
#define GST_TIME_CODE_STAMPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIME_CODE_STAMPER,GstTimeCodeStamper))
#define GST_TIME_CODE_STAMPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TIME_CODE_STAMPER,GstTimeCodeStamperClass))
#define GST_TIME_CODE_STAMPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_TIME_CODE_STAMPER,GstTimeCodeStamperClass))
#define GST_IS_TIME_CODE_STAMPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIME_CODE_STAMPER))
#define GST_IS_TIME_CODE_STAMPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TIME_CODE_STAMPER))

#define GST_TYPE_TIME_CODE_STAMPER_SOURCE (gst_timecodestamper_source_get_type())
#define GST_TYPE_TIME_CODE_STAMPER_SET (gst_timecodestamper_set_get_type())

typedef struct _GstTimeCodeStamper GstTimeCodeStamper;
typedef struct _GstTimeCodeStamperClass GstTimeCodeStamperClass;

typedef enum GstTimeCodeStamperSource
{
  GST_TIME_CODE_STAMPER_SOURCE_INTERNAL,
  GST_TIME_CODE_STAMPER_SOURCE_ZERO,
  GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN,
  GST_TIME_CODE_STAMPER_SOURCE_LAST_KNOWN_OR_ZERO,
  GST_TIME_CODE_STAMPER_SOURCE_LTC,
  GST_TIME_CODE_STAMPER_SOURCE_RTC,
} GstTimeCodeStamperSource;

typedef enum GstTimeCodeStamperSet {
  GST_TIME_CODE_STAMPER_SET_NEVER,
  GST_TIME_CODE_STAMPER_SET_KEEP,
  GST_TIME_CODE_STAMPER_SET_ALWAYS,
} GstTimeCodeStamperSet;

/**
 * GstTimeCodeStamper:
 *
 * Opaque data structure.
 */
struct _GstTimeCodeStamper
{
  GstBaseTransform videofilter;

  /* protected by object lock */
  GstPad *ltcpad;

  /* < private > */

  /* Properties, protected by object lock */
  GstTimeCodeStamperSource tc_source;
  GstTimeCodeStamperSet tc_set;
  gboolean tc_auto_resync;
  GstClockTime tc_timeout;
  gboolean drop_frame;
  gboolean post_messages;
  GstVideoTimeCode *set_internal_tc;
  GDateTime *ltc_daily_jam;
  gboolean ltc_auto_resync;
  GstClockTime ltc_timeout;
  GstClockTime ltc_extra_latency;
  GstClockTime rtc_max_drift;
  gboolean rtc_auto_resync;
  gint timecode_offset;

  /* Timecode tracking, protected by object lock */
  GstVideoTimeCode *internal_tc;
  GstVideoTimeCode *last_tc;
  GstClockTime last_tc_running_time;
  GstVideoTimeCode *rtc_tc;

  /* Internal state, protected by object lock, changed only from video streaming thread */
  gint fps_n;
  gint fps_d;
  GstVideoInterlaceMode interlace_mode;

  /* Seek handling, protected by the object lock */
  guint32 prev_seek_seqnum;
  gboolean reset_internal_tc_from_seek;
  gint64 seeked_frames;

  /* LTC specific fields */
#if HAVE_LTC
  GMutex mutex;
  GCond ltc_cond_video;
  GCond ltc_cond_audio;

  /* Only accessed from audio streaming thread */
  GstAudioInfo ainfo;
  GstAudioStreamAlign *stream_align;
  GstSegment ltc_segment;
  /* Running time of the first audio buffer passed to the LTC decoder */
  GstClockTime ltc_first_running_time;
  /* Running time of the last sample we passed to the LTC decoder so far */
  GstClockTime ltc_current_running_time;

  /* Protected by object lock */
  /* Queue of LTC timecodes we took out of the LTC decoder already
   * together with their corresponding running times */
  GQueue ltc_current_tcs;

  /* LTC timecode we last synced to and potentially incremented manually since
   * then */
  GstVideoTimeCode *ltc_internal_tc;
  GstClockTime ltc_internal_running_time;

  /* Running time of last video frame we received */
  GstClockTime video_current_running_time;

  /* Protected by mutex above */
  LTCDecoder *ltc_dec;
  ltc_off_t ltc_total;

  /* Protected by mutex above */
  gboolean video_flushing;
  gboolean video_eos;

  /* Protected by mutex above */
  gboolean ltc_flushing;
  gboolean ltc_eos;

  /* Latency information for LTC audio and video stream */
  GstClockTime audio_latency, video_latency;
  gboolean audio_live, video_live;
  /* Latency we report to downstream */
  GstClockTime latency;
  GstClockID video_clock_id;

  GstPadActivateModeFunction video_activatemode_default;
#endif
};

struct _GstTimeCodeStamperClass
{
  GstBaseTransformClass parent_class;
};

GType gst_timecodestamper_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (timecodestamper);

GType gst_timecodestamper_source_get_type (void);
GType gst_timecodestamper_set_get_type (void);

G_END_DECLS
#endif /* __GST_TIME_CODE_STAMPER_H__ */
