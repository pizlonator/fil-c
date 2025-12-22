/*
 * GStreamer
 * Copyright (C) 2016 Vivia Nikolaidou <vivia@toolsonair.com>
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

#ifndef __GST_AVWAIT_H__
#define __GST_AVWAIT_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_AVWAIT                    (gst_avwait_get_type())
#define GST_AVWAIT(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVWAIT,GstAvWait))
#define GST_IS_AVWAIT(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVWAIT))
#define GST_AVWAIT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_AVWAIT,GstAvWaitClass))
#define GST_IS_AVWAIT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_AVWAIT))
#define GST_AVWAIT_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_AVWAIT,GstAvWaitClass))
#define GST_TYPE_AVWAIT_MODE (gst_avwait_mode_get_type ())
typedef struct _GstAvWait GstAvWait;
typedef struct _GstAvWaitClass GstAvWaitClass;

typedef enum
{
  MODE_TIMECODE,
  MODE_RUNNING_TIME,
  MODE_VIDEO_FIRST
} GstAvWaitMode;

struct _GstAvWait
{
  GstElement parent;

  GstVideoTimeCode *tc;
  GstClockTime target_running_time;
  GstAvWaitMode mode;

  GstVideoTimeCode *end_tc;
  GstClockTime end_running_time;
  GstClockTime running_time_to_end_at;

  GstPad *asrcpad, *asinkpad, *vsrcpad, *vsinkpad;

  GstAudioInfo ainfo;
  GstVideoInfo vinfo;

  GstSegment asegment, vsegment;

  GstClockTime running_time_to_wait_for;
  GstClockTime last_seen_video_running_time;
  GstClockTime first_audio_running_time;
  GstVideoTimeCode *last_seen_tc;

  /* If running_time_to_wait_for has been reached but we are
   * not recording, audio shouldn't start running. It should
   * instead start synchronised with the video when we start
   * recording. Similarly when stopping recording manually vs
   * when the target timecode has been reached. So we use
   * different variables for the audio */
  GstClockTime audio_running_time_to_wait_for;
  GstClockTime audio_running_time_to_end_at;

  gboolean video_eos_flag;
  gboolean audio_eos_flag;
  gboolean video_flush_flag;
  gboolean audio_flush_flag;
  gboolean shutdown_flag;

  gboolean dropping;
  gboolean recording;
  gboolean was_recording;
  gint must_send_end_message;

  GCond cond;
  GMutex mutex;
  GCond audio_cond;
};

struct _GstAvWaitClass
{
  GstElementClass parent_class;
};

GType gst_avwait_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (avwait);

G_END_DECLS
#endif /* __GST_AVWAIT_H__ */
