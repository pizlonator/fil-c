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


#ifndef __GST_WILDMIDI_DEC_H__
#define __GST_WILDMIDI_DEC_H__


#include <gst/gst.h>
#include "gst/audio/gstnonstreamaudiodecoder.h"
#include <wildmidi_lib.h>


G_BEGIN_DECLS


typedef struct _GstWildmidiDec GstWildmidiDec;
typedef struct _GstWildmidiDecClass GstWildmidiDecClass;


#define GST_TYPE_WILDMIDI_DEC             (gst_wildmidi_dec_get_type())
#define GST_WILDMIDI_DEC(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WILDMIDI_DEC, GstWildmidiDec))
#define GST_WILDMIDI_DEC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WILDMIDI_DEC, GstWildmidiDecClass))
#define GST_IS_WILDMIDI_DEC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WILDMIDI_DEC))
#define GST_IS_WILDMIDI_DEC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WILDMIDI_DEC))


struct _GstWildmidiDec
{
  GstNonstreamAudioDecoder parent;

  midi *song;

  gboolean log_volume_scale;
  gboolean enhanced_resampling;
  gboolean reverb;
  guint output_buffer_size;
};


struct _GstWildmidiDecClass
{
  GstNonstreamAudioDecoderClass parent_class;
};


GType gst_wildmidi_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (wildmididec);

G_END_DECLS


#endif /* __GST_WILDMIDI_DEC_H__ */
