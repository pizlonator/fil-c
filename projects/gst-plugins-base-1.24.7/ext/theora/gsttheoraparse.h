/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) 2006 Andy Wingo <wingo@pobox.com>
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


#ifndef __GST_THEORA_PARSE_H__
#define __GST_THEORA_PARSE_H__


#include <gst/gst.h>
#include <theora/theoradec.h>

G_BEGIN_DECLS

#define GST_TYPE_THEORA_PARSE \
  (gst_theora_parse_get_type())
#define GST_THEORA_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_THEORA_PARSE,GstTheoraParse))
#define GST_THEORA_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_THEORA_PARSE,GstTheoraParseClass))
#define GST_IS_THEORA_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_THEORA_PARSE))
#define GST_IS_THEORA_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_THEORA_PARSE))

typedef struct _GstTheoraParse GstTheoraParse;
typedef struct _GstTheoraParseClass GstTheoraParseClass;

/**
 * GstTheoraParse:
 *
 * Opaque data structure.
 */
struct _GstTheoraParse {
  GstElement            element;

  GstPad *              sinkpad;
  GstPad *              srcpad;

  gboolean              send_streamheader;
  gboolean              streamheader_received;
  gboolean		is_old_bitstream;
  GstBuffer *		streamheader[3];

  GQueue *		event_queue;
  GQueue *		buffer_queue;

  th_info		info;
  th_comment            comment;

  gint64		prev_frame;
  gint64		prev_keyframe;
  guint32		fps_n;
  guint32		fps_d;
  gint			shift;
  gint64		granule_offset;

  GstClockTime		*times;
  gint			npairs;
};

struct _GstTheoraParseClass {
  GstElementClass parent_class;
};

GType gst_theora_parse_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (theoraparse);

G_END_DECLS

#endif /* __GST_THEORA_PARSE_H__ */
