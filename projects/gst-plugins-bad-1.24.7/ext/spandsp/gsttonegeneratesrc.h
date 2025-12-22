/* GStreamer
 * Copyright (C) 2016 Iskratel d.o.o.
 *   Author: Okrslar Ales <okrslar@iskratel.si>
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_TONE_GENERATE_SRC_H__
#define __GST_TONE_GENERATE_SRC_H__


#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>

#include <gst/audio/audio.h>
#include <spandsp.h>

G_BEGIN_DECLS

#define GST_TYPE_TONE_GENERATE_SRC \
  (gst_tone_generate_src_get_type())
#define GST_TONE_GENERATE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TONE_GENERATE_SRC,GstToneGenerateSrc))
#define GST_TONE_GENERATE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TONE_GENERATE_SRC,GstToneGenerateSrcClass))
#define GST_IS_TONE_GENERATE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TONE_GENERATE_SRC))
#define GST_IS_TONE_GENERATE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TONE_GENERATE_SRC))

typedef struct _GstToneGenerateSrc GstToneGenerateSrc;
typedef struct _GstToneGenerateSrcClass GstToneGenerateSrcClass;

/**
 * GstToneGenerateSrc:
 *
 * tonegeneratesrc object structure.
 */
struct _GstToneGenerateSrc {
  GstPushSrc parent;

  /* parameters */
  gint volume;      /* The level of the first frequency, in dBm0 */
  gint volume2;     /* The level of the second frequency, in dBm0, or the percentage modulation depth for an AM modulated tone. */
  gint freq;        /* The first frequency, in Hz */
  gint freq2;       /* 0 for no second frequency, a positive number for the second frequency, in Hz, or a negative number for an AM modulation frequency, in Hz */
  gint on_time;         /* On time for the first presence of tone signal. */
  gint off_time;        /* Off time between first and second presence of tone signal. */
  gint on_time2;        /* On time for the second presence of tone signal. */
  gint off_time2;       /* Off time after the second presence of tone signal. */
  gboolean repeat;         /* 0/1 if the tone repeates itself or not. */

  /* audio parameters */
  gint samples_per_buffer;

  /*< private >*/
  GstClockTime next_time;               /* next timestamp */
  gint64 next_sample;                   /* next sample to send */

  /* SpanDSP */
  tone_gen_state_t *tone_state;
  tone_gen_descriptor_t *tone_desc;
  gboolean properties_changed;
};

struct _GstToneGenerateSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_tone_generate_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (tonegeneratesrc);

G_END_DECLS

#endif /* __GST_TONE_GENERATE_SRC_H__ */
