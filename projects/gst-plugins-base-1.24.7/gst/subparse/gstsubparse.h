/* GStreamer
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
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

#ifndef __GST_SUBPARSE_H__
#define __GST_SUBPARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "gstsubparseelements.h"

G_BEGIN_DECLS

#define GST_TYPE_SUBPARSE (gst_sub_parse_get_type ())
G_DECLARE_FINAL_TYPE (GstSubParse, gst_sub_parse, GST, SUBPARSE, GstElement)


typedef struct {
  int      state;
  GString *buf;
  guint64  start_time;
  guint64  duration;
  guint64  max_duration; /* to clamp duration, 0 = no limit (used by tmplayer parser) */
  GstSegment *segment;
  gpointer user_data;
  gboolean have_internal_fps; /* If TRUE don't overwrite fps by property */
  gint fps_n, fps_d;     /* used by frame based parsers */
  guint8 line_position;          /* percent value */
  gint line_number;              /* line number, can be positive or negative */
  guint8 text_position;          /* percent value */
  guint8 text_size;          /* percent value */
  gchar *vertical;        /* "", "vertical", "vertical-lr" */
  gchar *alignment;       /* "", "start", "middle", "end" */
  gconstpointer allowed_tags; /* list of markup tags allowed in the cue text. */
  gboolean allows_tag_attributes;
} ParserState;

typedef gchar* (*Parser) (ParserState *state, const gchar *line);

struct _GstSubParse {
  GstElement element;

  GstPad *sinkpad,*srcpad;

  /* contains the input in the input encoding */
  GstAdapter *adapter;
  /* contains the UTF-8 decoded input */
  GString *textbuf;

  GstSubParseFormat parser_type;
  gboolean parser_detected;
  const gchar *subtitle_codec;

  Parser parse_line;
  ParserState state;

  /* seek */
  guint64 offset;
  
  /* Segment */
  guint32       segment_seqnum;
  GstSegment    segment;
  gboolean      need_segment;
  
  gboolean flushing;
  gboolean valid_utf8;
  gchar   *detected_encoding;
  gchar   *encoding;
  gboolean strip_pango_markup;

  gboolean first_buffer;

  /* used by frame based parsers */
  gint fps_n, fps_d;          
};

G_END_DECLS

#endif /* __GST_SUBPARSE_H__ */
