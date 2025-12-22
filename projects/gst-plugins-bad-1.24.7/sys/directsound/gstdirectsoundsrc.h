/*
 * GStreamer
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright 2005 Sébastien Moutte <sebastien@moutte.net>
 * Copyright 2006 Joni Valtanen <joni.valtanen@movial.fi>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

#ifndef __GST_DIRECTSOUNDSRC_H__
#define __GST_DIRECTSOUNDSRC_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiosrc.h>
#include <windows.h>
#include <dsound.h>
#include <mmsystem.h>

/* add here some headers if needed */


G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DIRECTSOUND_SRC (gst_directsound_src_get_type())
#define GST_DIRECTSOUND_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DIRECTSOUND_SRC,GstDirectSoundSrc))
#define GST_DIRECTSOUND_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DIRECTSOUND_SRC,GstDirectSoundSrcClass))
#define GST_IS_DIRECTSOUND_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DIRECTSOUND_SRC))
#define GST_IS_DIRECTSOUND_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DIRECTSOUND_SRC))

typedef struct _GstDirectSoundSrc      GstDirectSoundSrc;
typedef struct _GstDirectSoundSrcClass GstDirectSoundSrcClass;

#define GST_DSOUND_LOCK(obj)	(g_mutex_lock (&obj->dsound_lock))
#define GST_DSOUND_UNLOCK(obj)	(g_mutex_unlock (&obj->dsound_lock))

struct _GstDirectSoundSrc
{

  GstAudioSrc src;

  LPDIRECTSOUNDCAPTURE pDSC; /* DirectSoundCapture*/
  LPDIRECTSOUNDCAPTUREBUFFER pDSBSecondary;  /*Secondaty capturebuffer*/
  DWORD current_circular_offset;

  guint buffer_size;
  guint bytes_per_sample;

  guint latency_time;

  HMIXER mixer;
  DWORD mixerline_cchannels;
  gint control_id_volume;
  gint control_id_mute;
  glong dw_vol_max;
  glong dw_vol_min;

  glong volume;
  gboolean mute;

  GUID *device_guid;

  char *device_name;
  char *device_id;

  GMutex dsound_lock;

  GstClock *system_clock;
  GstClockID *read_wait_clock_id;
  gboolean reset_while_sleeping;
};

struct _GstDirectSoundSrcClass 
{
  GstAudioSrcClass parent_class;
};

GType gst_directsound_src_get_type (void);

#define GST_DIRECTSOUND_SRC_CAPS "audio/x-raw, " \
        "format = (string) { S16LE, S8 }, " \
        "layout = (string) interleaved, " \
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]"

G_END_DECLS

#endif /* __GST_DIRECTSOUNDSRC_H__ */
