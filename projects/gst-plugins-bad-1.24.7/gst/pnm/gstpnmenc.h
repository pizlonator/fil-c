/* GStreamer PNM encoder
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

#ifndef __GST_PNMENC_H__
#define __GST_PNMENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstpnmutils.h"

G_BEGIN_DECLS

#define GST_TYPE_PNMENC  (gst_pnmenc_get_type())
#define GST_PNMENC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PNMENC,GstPnmenc))
#define GST_PNMENC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PNMENC,GstPnmenc))
#define GST_IS_PNMENC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PNMENC))
#define GST_IS_PNMENC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PNMENC))

typedef struct _GstPnmenc GstPnmenc;
typedef struct _GstPnmencClass GstPnmencClass;

struct _GstPnmenc
{
  GstVideoEncoder parent;
  GstVideoCodecState *input_state;
  GstPnmInfo info;

  GstPad *src;
};

struct _GstPnmencClass
{
  GstVideoEncoderClass parent;
};

GType gst_pnmenc_get_type (void) G_GNUC_CONST;
GST_ELEMENT_REGISTER_DECLARE (pnmenc);

G_END_DECLS

#endif /* __GST_PNMENC_H__ */
