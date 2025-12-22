/* GStreamer Interleaved RTSP parser
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
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

#ifndef __GST_IRTSP_PARSE_H__
#define __GST_IRTSP_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>

G_BEGIN_DECLS
#define GST_TYPE_IRTSP_PARSE \
  (gst_irtsp_parse_get_type())
#define GST_IRTSP_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_IRTSP_PARSE, GstIRTSPParse))
#define GST_IRTSP_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_IRTSP_PARSE, GstIRTSPParseClass))
#define GST_IS_IRTSP_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_IRTSP_PARSE))
#define GST_IS_IRTSP_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_IRTSP_PARSE))
typedef struct _GstIRTSPParse GstIRTSPParse;
typedef struct _GstIRTSPParseClass GstIRTSPParseClass;

typedef enum
{
  IRTSP_SEARCH_FRAME,
  IRTSP_PARSE_FRAME,
  IRTSP_FLUSH_FRAME,
  IRTSP_SKIP_FRAME
} RtspParserState;

/**
 * GstIRTSPParse:
 *
 * The opaque GstIRTSPParse object
 */
struct _GstIRTSPParse
{
  GstBaseParse baseparse;

  guint8 target_channel_id;
  /*< private > */
  RtspParserState state;
  guint16 frame_size;
  guint current_offset;
  gboolean discont;
};

/**
 * GstIRTSPParseClass:
 * @parent_class: Element parent class.
 *
 * The opaque GstIRTSPParseClass data structure.
 */
struct _GstIRTSPParseClass
{
  GstBaseParseClass baseparse_class;
};

GType gst_irtsp_parse_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (irtspparse);

G_END_DECLS
#endif /* __GST_IRTSP_PARSE_H__ */
