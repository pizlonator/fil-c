/* ASF Parser plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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


#ifndef __GST_ASF_PARSE_H__
#define __GST_ASF_PARSE_H__


#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbytereader.h>

#include "gstasfobjects.h"

G_BEGIN_DECLS

#define GST_TYPE_ASF_PARSE \
  (gst_asf_parse_get_type())
#define GST_ASF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ASF_PARSE,GstAsfParse))
#define GST_ASF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ASF_PARSE,GstAsfParseClass))
#define GST_IS_ASF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ASF_PARSE))
#define GST_IS_ASF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ASF_PARSE))

#define GST_ASF_PARSE_CAST(obj) ((GstAsfParse*)(obj))

enum GstAsfParsingState {
  ASF_PARSING_HEADERS,
  ASF_PARSING_DATA,
  ASF_PARSING_PACKETS,
  ASF_PARSING_INDEXES
};

typedef struct _GstAsfParse GstAsfParse;
typedef struct _GstAsfParseClass GstAsfParseClass;

struct _GstAsfParse {
  GstBaseParse baseparse;

  enum GstAsfParsingState parse_state;

  guint64 parsed_packets;

  /* parsed info */
  GstAsfFileInfo *asfinfo;
  GstAsfPacketInfo *packetinfo;
};

struct _GstAsfParseClass {
  GstBaseParseClass parent_class;
};

GType gst_asf_parse_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (asfparse);

G_END_DECLS


#endif /* __GST_ASF_PARSE_H__ */
