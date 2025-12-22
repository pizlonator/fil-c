/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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

#ifndef _GST_INTER_SURFACE_H_
#define _GST_INTER_SURFACE_H_

#include <gst/base/gstadapter.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct _GstInterSurface GstInterSurface;

struct _GstInterSurface
{
  GMutex mutex;
  gint ref_count;

  char *name;

  /* video */
  GstVideoInfo video_info;
  int video_buffer_count;

  /* audio */
  GstAudioInfo audio_info;
  guint64 audio_buffer_time;
  guint64 audio_latency_time;
  guint64 audio_period_time;

  GstBuffer *video_buffer;
  GstBuffer *sub_buffer;
  GstAdapter *audio_adapter;
};

#define DEFAULT_AUDIO_BUFFER_TIME  (GST_SECOND)
#define DEFAULT_AUDIO_LATENCY_TIME (100 * GST_MSECOND)
#define DEFAULT_AUDIO_PERIOD_TIME  (25 * GST_MSECOND)


GstInterSurface * gst_inter_surface_get (const char *name);
void gst_inter_surface_unref (GstInterSurface *surface);


G_END_DECLS

#endif
