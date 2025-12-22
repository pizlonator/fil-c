/* GStreamer
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2015> British Broadcasting Corporation
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

#ifndef __GST_TTML_PARSE_H__
#define __GST_TTML_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_TTML_PARSE \
  (gst_ttml_parse_get_type ())
#define GST_TTML_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TTML_PARSE, GstTtmlParse))
#define GST_TTML_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TTML_PARSE, GstTtmlParseClass))
#define GST_IS_TTML_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TTML_PARSE))
#define GST_IS_TTML_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TTML_PARSE))

typedef struct _GstTtmlParse GstTtmlParse;
typedef struct _GstTtmlParseClass GstTtmlParseClass;

struct _GstTtmlParse {
  GstElement element;

  GstPad *sinkpad, *srcpad;

  /* contains the input in the input encoding */
  GstAdapter *adapter;
  /* contains the UTF-8 decoded input */
  GString *textbuf;

  /* seek */
  guint64 offset;

  /* Segment */
  GstSegment    segment;
  gboolean      need_segment;

  gboolean valid_utf8;
  gchar   *detected_encoding;
  gchar   *encoding;

  gboolean first_buffer;
};

struct _GstTtmlParseClass {
  GstElementClass parent_class;
};

GType gst_ttml_parse_get_type (void);

G_END_DECLS

#endif /* __GST_TTML_PARSE_H__ */
