/* Copyright (C) 2004-2005, 2009 Michael Pyne <michael dot pyne at kdemail net>
 * Copyright (C) 2004-2006 Chris Lee <clee at kde org>
 * Copyright (C) 2007 Brian Koropoff <bkoropoff at gmail com>
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

#ifndef __GST_GME_DEC_H__
#define __GST_GME_DEC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include <gme/gme.h>

G_BEGIN_DECLS

#define GST_TYPE_GME_DEC \
  (gst_gme_dec_get_type())
#define GST_GME_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GME_DEC,GstGmeDec))
#define GST_GME_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GME_DEC,GstGmeDecClass))
#define GST_IS_GME_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GME_DEC))
#define GST_IS_GME_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GME_DEC))

typedef struct _GstGmeDec GstGmeDec;
typedef struct _GstGmeDecClass GstGmeDecClass;

struct _GstGmeDec
{
  GstElement  element;

  GstPad     *sinkpad;
  GstPad     *srcpad;

  GstAdapter *adapter;
  Music_Emu  *player;
  gboolean    initialized;
  gboolean    seeking;
  int         seekpoint;

  GstClockTime total_duration;
};

struct _GstGmeDecClass
{
  GstElementClass parent_class;
};

GType gst_gme_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (gmedec);

G_END_DECLS

#endif /* __GST_GME_DEC_H__ */
