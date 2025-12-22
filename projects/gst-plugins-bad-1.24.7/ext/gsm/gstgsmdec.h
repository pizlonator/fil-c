/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#ifndef __GST_GSMDEC_H__
#define __GST_GSMDEC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

#ifdef GSM_HEADER_IN_SUBDIR
#include <gsm/gsm.h>
#else
#include <gsm.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_GSMDEC \
  (gst_gsmdec_get_type())
#define GST_GSMDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GSMDEC,GstGSMDec))
#define GST_GSMDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GSMDEC,GstGSMDecClass))
#define GST_IS_GSMDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GSMDEC))
#define GST_IS_GSMDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GSMDEC))

typedef struct _GstGSMDec GstGSMDec;
typedef struct _GstGSMDecClass GstGSMDecClass;

struct _GstGSMDec
{
  GstAudioDecoder element;

  gsm state;
  gint use_wav49;
  gint needed;
};

struct _GstGSMDecClass
{
  GstAudioDecoderClass parent_class;
};

GType gst_gsmdec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (gsmdec);

G_END_DECLS

#endif /* __GST_GSMDEC_H__ */
