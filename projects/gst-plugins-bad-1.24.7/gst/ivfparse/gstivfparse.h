/* -*- Mode: c; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * GStreamer IVF parser
 * (c) 2010 Opera Software ASA, Philip Jägenstedt <philipj@opera.com>
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

#ifndef __GST_IVF_PARSE_H__
#define __GST_IVF_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>

G_BEGIN_DECLS

#define GST_TYPE_IVF_PARSE \
  (gst_ivf_parse_get_type())
#define GST_IVF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IVF_PARSE,GstIvfParse))
#define GST_IVF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IVF_PARSE,GstIvfParseClass))
#define GST_IS_IVF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IVF_PARSE))
#define GST_IS_IVF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IVF_PARSE))

typedef enum {
  GST_IVF_PARSE_START,
  GST_IVF_PARSE_DATA
} GstIvfParseState;

typedef struct _GstIvfParse GstIvfParse;
typedef struct _GstIvfParseClass GstIvfParseClass;

struct _GstIvfParse
{
  GstBaseParse baseparse;

  GstIvfParseState state;

  guint width;
  guint height;
  guint fps_n;
  guint fps_d;
  guint32 fourcc;
  gboolean update_caps;
};

struct _GstIvfParseClass
{
  GstBaseParseClass parent_class;
};

GType gst_ivf_parse_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ivfparse);

G_END_DECLS

#endif /* __GST_IVF_PARSE_H__ */
