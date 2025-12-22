/* GStreamer PNM decoder
 * Copyright (C) 2009 Lutz Mueller <lutz@users.sourceforge.net>
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

#ifndef __GST_PNMDEC_H__
#define __GST_PNMDEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstpnmutils.h"

G_BEGIN_DECLS

#define GST_TYPE_PNMDEC (gst_pnmdec_get_type())
#define GST_PNMDEC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PNMDEC,GstPnmdec))
#define GST_PNMDEC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PNMDEC,GstPnmdec))
#define GST_IS_PNMDEC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PNMDEC))
#define GST_IS_PNMDEC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PNMDEC))

typedef struct _GstPnmdec GstPnmdec;
typedef struct _GstPnmdecClass GstPnmdecClass;

struct _GstPnmdec
{
  GstElement element;
  GstVideoDecoder decoder;
  GstPnmInfoMngr mngr;
  GstVideoCodecState *input_state;
  guint size, last_val, current_size ;
  gboolean have_last_val;
  GstBuffer *buf;
  GstVideoFormat out_format;
};

struct _GstPnmdecClass
{
  GstVideoDecoderClass parent_class ;
};

GType gst_pnmdec_get_type (void) G_GNUC_CONST;
GST_ELEMENT_REGISTER_DECLARE (pnmdec);
G_END_DECLS

#endif /* __GST_PNMDEC_H__ */
