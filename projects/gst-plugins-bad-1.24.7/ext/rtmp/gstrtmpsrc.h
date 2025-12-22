/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2002 Kristian Rietveld <kris@gtk.org>
 *                    2002,2003 Colin Walters <walters@gnu.org>
 *                    2001,2010 Bastien Nocera <hadess@hadess.net>
 *                    2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_RTMP_SRC_H__
#define __GST_RTMP_SRC_H__

#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>

#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <librtmp/amf.h>

G_BEGIN_DECLS

#define GST_TYPE_RTMP_SRC \
  (gst_rtmp_src_get_type())
#define GST_RTMP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_SRC,GstRTMPSrc))
#define GST_RTMP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_SRC,GstRTMPSrcClass))
#define GST_IS_RTMP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_SRC))
#define GST_IS_RTMP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_SRC))

typedef struct _GstRTMPSrc      GstRTMPSrc;
typedef struct _GstRTMPSrcClass GstRTMPSrcClass;

/**
 * GstRTMPSrc:
 *
 * Opaque data structure.
 */
struct _GstRTMPSrc
{
  GstPushSrc parent;
  
  /* < private > */
  gchar *uri;
  gchar *swf_url;
  gchar *page_url;

  RTMP *rtmp;
  int timeout;
  gint64 cur_offset;
  GstClockTime last_timestamp;
  gboolean seekable;
  gboolean discont;
};

struct _GstRTMPSrcClass
{
  GstPushSrcClass  parent;
};

GType gst_rtmp_src_get_type (void);

G_END_DECLS

#endif /* __GST_RTMP_SRC_H__ */

