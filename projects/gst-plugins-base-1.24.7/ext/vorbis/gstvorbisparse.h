/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
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


#ifndef __GST_VORBIS_PARSE_H__
#define __GST_VORBIS_PARSE_H__


#include <gst/gst.h>
#include <vorbis/codec.h>

G_BEGIN_DECLS

#define GST_TYPE_VORBIS_PARSE \
  (gst_vorbis_parse_get_type())
#define GST_VORBIS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VORBIS_PARSE,GstVorbisParse))
#define GST_VORBIS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VORBIS_PARSE,GstVorbisParseClass))
#define GST_IS_VORBIS_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VORBIS_PARSE))
#define GST_IS_VORBIS_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VORBIS_PARSE))

typedef struct _GstVorbisParse GstVorbisParse;
typedef struct _GstVorbisParseClass GstVorbisParseClass;

/**
 * GstVorbisParse:
 *
 * Opaque data structure.
 */
struct _GstVorbisParse {
  GstElement            element;

  GstPad *              sinkpad;
  GstPad *              srcpad;

  guint                 packetno;
  gboolean              streamheader_sent;
  GList *               streamheader;

  GQueue *		event_queue;
  GQueue *		buffer_queue;

  vorbis_info		vi;
  vorbis_comment	vc;

  gint64		prev_granulepos;
  gint32		prev_blocksize;
  guint32		sample_rate;
  guint32               channels;
};

struct _GstVorbisParseClass {
  GstElementClass parent_class;

  /* virtual functions */
  GstFlowReturn  (*parse_packet) (GstVorbisParse * parse, GstBuffer * buf);
};

GType gst_vorbis_parse_get_type(void);

G_END_DECLS

#endif /* __GST_VORBIS_PARSE_H__ */
