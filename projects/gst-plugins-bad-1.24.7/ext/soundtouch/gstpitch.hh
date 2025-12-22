/* GStreamer pitch controller element
 * Copyright (C) 2006 Wouter Paesen <wouter@blue-gate.be>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GST_PITCH_H__
#define __GST_PITCH_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_PITCH \
  (gst_pitch_get_type())
#define GST_PITCH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PITCH,GstPitch))
#define GST_PITCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PITCH,GstPitchClass))
#define GST_IS_PITCH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PITCH))
#define GST_IS_PITCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PITCH))

typedef struct _GstPitch            GstPitch;
typedef struct _GstPitchClass       GstPitchClass;
typedef struct _GstPitchPrivate     GstPitchPrivate;


struct _GstPitch
{
  GstElement element;

  GstPad  *srcpad;
  GstPad  *sinkpad;

  /* parameter values */
  gfloat   tempo;               /* time stretch 
                                 * change the duration, without affecting the pitch
                                 * > 1 makes the stream shorter
                                 */

  gfloat   rate;                /* change playback rate 
                                 * resample
                                 * > 1 makes the stream shorter
                                 */

  gfloat   out_seg_rate;        /* change output segment rate 
                                 * Affects playback when input
                                 * segments have rate != out_rate
                                 */

  gfloat   pitch;               /* change pitch 
                                 * change the pitch without affecting the
                                 * duration, stream length doesn't change
                                 */

  gfloat  seg_arate;            /* Rate to apply from input segment */

  /* values extracted from caps */
  GstAudioInfo  info;

  /* stream tracking */
  GstClockTime  next_buffer_time;
  gint64        next_buffer_offset;

  GstClockTimeDiff  min_latency, max_latency;

  GstPitchPrivate *priv;
};

struct _GstPitchClass 
{
  GstElementClass parent_class;
};

GType gst_pitch_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (pitch);

G_END_DECLS

#endif /* __GST_PITCH_H__ */
