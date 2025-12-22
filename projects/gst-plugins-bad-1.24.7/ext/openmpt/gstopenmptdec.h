/* GStreamer
 * Copyright (C) <2017> Carlos Rafael Giani <dv at pseudoterminal dot org>
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


#ifndef __GST_OPENMPT_DEC_H__
#define __GST_OPENMPT_DEC_H__


#include <gst/gst.h>
#include "gst/audio/gstnonstreamaudiodecoder.h"
#include <libopenmpt/libopenmpt.h>


G_BEGIN_DECLS


typedef struct _GstOpenMptDec GstOpenMptDec;
typedef struct _GstOpenMptDecClass GstOpenMptDecClass;


#define GST_TYPE_OPENMPT_DEC             (gst_openmpt_dec_get_type())
#define GST_OPENMPT_DEC(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OPENMPT_DEC, GstOpenMptDec))
#define GST_OPENMPT_DEC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OPENMPT_DEC, GstOpenMptDecClass))
#define GST_IS_OPENMPT_DEC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OPENMPT_DEC))
#define GST_IS_OPENMPT_DEC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OPENMPT_DEC))


struct _GstOpenMptDec
{
  GstNonstreamAudioDecoder parent;
  openmpt_module *mod;

  guint cur_subsong, num_subsongs;
  double *subsong_durations;
  /* NOTE: this is of type int, not guint, because the value
   * is defined by OpenMPT, and can be -1 (= "all subsongs") */
  int default_openmpt_subsong;
  GstNonstreamAudioSubsongMode cur_subsong_mode;

  gint num_loops;

  gint master_gain, stereo_separation, filter_length, volume_ramping;

  GstAudioFormat sample_format;
  gint sample_rate, num_channels;

  guint output_buffer_size;

  GstTagList *main_tags;
};


struct _GstOpenMptDecClass
{
  GstNonstreamAudioDecoderClass parent_class;
};


GType gst_openmpt_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (openmptdec);

G_END_DECLS


#endif /* __GST_OPENMPT_DEC_H__ */
