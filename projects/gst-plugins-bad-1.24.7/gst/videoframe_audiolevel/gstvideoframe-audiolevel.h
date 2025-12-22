/* 
 * GStreamer
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

#ifndef __GST_VIDEOFRAME_AUDIOLEVEL_H__
#define __GST_VIDEOFRAME_AUDIOLEVEL_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS
#define GST_TYPE_VIDEOFRAME_AUDIOLEVEL                    (gst_videoframe_audiolevel_get_type())
#define GST_VIDEOFRAME_AUDIOLEVEL(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEOFRAME_AUDIOLEVEL,GstVideoFrameAudioLevel))
#define GST_IS_VIDEOFRAME_AUDIOLEVEL(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEOFRAME_AUDIOLEVEL))
#define GST_VIDEOFRAME_AUDIOLEVEL_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_VIDEOFRAME_AUDIOLEVEL,GstVideoFrameAudioLevelClass))
#define GST_IS_VIDEOFRAME_AUDIOLEVEL_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_VIDEOFRAME_AUDIOLEVEL))
#define GST_VIDEOFRAME_AUDIOLEVEL_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_VIDEOFRAME_AUDIOLEVEL,GstVideoFrameAudioLevelClass))
typedef struct _GstVideoFrameAudioLevel GstVideoFrameAudioLevel;
typedef struct _GstVideoFrameAudioLevelClass GstVideoFrameAudioLevelClass;

struct _GstVideoFrameAudioLevel
{
  GstElement parent;

  GstPad *asrcpad, *asinkpad, *vsrcpad, *vsinkpad;

  GstAudioInfo ainfo;

  gdouble *CS;                  /* normalized Cumulative Square */

  GstSegment asegment, vsegment;

  void (*process) (gpointer, guint, guint, gdouble *);

  GQueue vtimeq;
  GstAdapter *adapter;
  GstClockTime first_time;
  guint total_frames;
  guint64 next_offset, alignment_threshold, discont_time, discont_wait;

  gboolean video_eos_flag;
  gboolean audio_flush_flag;
  gboolean shutdown_flag;

  GCond cond;
  GMutex mutex;
};

struct _GstVideoFrameAudioLevelClass
{
  GstElementClass parent_class;
};

GType gst_videoframe_audiolevel_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (videoframe_audiolevel);

G_END_DECLS
#endif /* __GST_VIDEOFRAME_AUDIOLEVEL_H__ */
