/* GStreamer
 * Copyright (C) <2006> Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>
 * Copyright (C) <2004> David A. Schleef <ds@schleef.org>
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

#ifndef __GST_RFB_SRC_H__
#define __GST_RFB_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/gstvideopool.h>

#include "rfbdecoder.h"

G_BEGIN_DECLS
#define GST_TYPE_RFB_SRC \
  (gst_rfb_src_get_type())
#define GST_RFB_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RFB_SRC,GstRfbSrc))
#define GST_RFB_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RFB_SRC,GstRfbSrc))
#define GST_IS_RFB_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RFB_SRC))
#define GST_IS_RFB_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RFB_SRC))
typedef struct _GstRfbSrc GstRfbSrc;
typedef struct _GstRfbSrcClass GstRfbSrcClass;

struct _GstRfbSrcClass
{
  GstPushSrcClass parent_class;
};

struct _GstRfbSrc
{
  GstPushSrc element;

  GstUri *uri;
  gchar *host;
  gint port;

  RfbDecoder *decoder;
  gboolean go;
  gboolean incremental_update;
  gboolean view_only;

  guint button_mask;

  /* protocol version */
  guint version_major;
  guint version_minor;
};

GType gst_rfb_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (rfbsrc);

G_END_DECLS
#endif
