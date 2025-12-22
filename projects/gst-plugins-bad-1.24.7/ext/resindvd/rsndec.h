/* GStreamer
 * Copyright (C) <2009> Jan Schmidt <thaytan@noraisin.net>
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

#ifndef __RSNDEC_H__
#define __RSNDEC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define RSN_TYPE_DEC               (rsn_dec_get_type())
#define RSN_DEC(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj),RSN_TYPE_DEC,RsnDec))
#define RSN_DEC_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass),RSN_TYPE_DEC,RsnDecClass))
#define RSN_IS_DEC(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),RSN_TYPE_DEC))
#define RSN_IS_DEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),RSN_TYPE_DEC))
#define RSN_DEC_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), RSN_TYPE_DEC, RsnDecClass))

#define RSN_TYPE_AUDIODEC               (rsn_audiodec_get_type())
#define RSN_AUDIODEC(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj),RSN_TYPE_AUDIODEC,RsnAudioDec))
#define RSN_AUDIODEC_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass),RSN_TYPE_AUDIODEC,RsnAudioDecClass))
#define RSN_IS_AUDIODEC(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),RSN_TYPE_AUDIODEC))
#define RSN_IS_AUDIODEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),RSN_TYPE_AUDIODEC))

#define RSN_TYPE_VIDEODEC               (rsn_videodec_get_type())
#define RSN_VIDEODEC(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj),RSN_TYPE_VIDEODEC,RsnVideoDec))
#define RSN_VIDEODEC_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass),RSN_TYPE_VIDEODEC,RsnVideoDecClass))
#define RSN_IS_VIDEODEC(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),RSN_TYPE_VIDEODEC))
#define RSN_IS_VIDEODEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),RSN_TYPE_VIDEODEC))

GType           rsn_dec_get_type           (void) G_GNUC_CONST;
GType           rsn_audiodec_get_type      (void) G_GNUC_CONST;
GType           rsn_videodec_get_type      (void) G_GNUC_CONST;

typedef struct _RsnDec             RsnDec;
typedef struct _RsnDecClass        RsnDecClass;

typedef struct _RsnDec             RsnAudioDec;
typedef struct _RsnDecClass        RsnAudioDecClass;

typedef struct _RsnDec             RsnVideoDec;
typedef struct _RsnDecClass        RsnVideoDecClass;

struct _RsnDec {
  GstBin element;

  /* Our sink and source pads */
  GstGhostPad *sinkpad;
  GstGhostPad *srcpad;

  GstPadEventFunction sink_event_func;

  GstElement *current_decoder;
};

struct _RsnDecClass {
  GstBinClass parent_class;

  const GList * (*get_decoder_factories) (RsnDecClass *klass);
};

G_END_DECLS

#endif /* __RSNDEC_H__ */
