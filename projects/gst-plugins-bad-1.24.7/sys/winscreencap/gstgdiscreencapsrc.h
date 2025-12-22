/* GStreamer
 * Copyright (C) 2007 Haakon Sporsheim <hakon.sporsheim@tandberg.com>
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

#ifndef __GST_GDISCREENCAPSRC_H__
#define __GST_GDISCREENCAPSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include "gstwinscreencap.h"

G_BEGIN_DECLS

#define GST_TYPE_GDISCREENCAPSRC  (gst_gdiscreencapsrc_get_type())
#define GST_GDISCREENCAPSRC(obj)                                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                     \
  GST_TYPE_GDISCREENCAPSRC,GstGDIScreenCapSrc))
#define GST_GDISCREENCAPSRC_CLASS(klass)                                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                      \
  GST_TYPE_GDISCREENCAPSRC,GstGDIScreenCapSrcClass))
#define GST_IS_GDISCREENCAPSRC(obj)                                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),GST_TYPE_GDISCREENCAPSRC))
#define GST_IS_GDISCREENCAPSRC_CLASS(klass)                               \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),GST_TYPE_GDISCREENCAPSRC))

typedef struct _GstGDIScreenCapSrc GstGDIScreenCapSrc;
typedef struct _GstGDIScreenCapSrcClass GstGDIScreenCapSrcClass;

struct _GstGDIScreenCapSrc
{
  /* Parent */
  GstPushSrc src;

  /* Properties */
  gint capture_x;
  gint capture_y;
  gint capture_w;
  gint capture_h;
  gint monitor;
  gboolean show_cursor;

  /* Source pad frame rate */
  gint rate_numerator;
  gint rate_denominator;

  /* Runtime variables */
  RECT screen_rect;
  RECT src_rect;
  guint64 frame_number;
  GstClockID clock_id;

  BITMAPINFO info;
  BYTE *dibMem;
  HBITMAP hBitmap;
  HDC memDC;
};

struct _GstGDIScreenCapSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_gdiscreencapsrc_get_type (void);

G_END_DECLS

#endif /* __GST_GDISCREENCAPSRC_H__ */
