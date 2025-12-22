/* GStreamer AIFF parser
 * Copyright (C) <2008> Pioneers of the Inevitable <songbird@songbirdnest.com>
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


#ifndef __GST_AIFF_PARSE_H__
#define __GST_AIFF_PARSE_H__


#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_AIFF_PARSE \
  (gst_aiff_parse_get_type())
#define GST_AIFF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AIFF_PARSE,GstAiffParse))
#define GST_AIFF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AIFF_PARSE,GstAiffParseClass))
#define GST_IS_AIFF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AIFF_PARSE))
#define GST_IS_AIFF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AIFF_PARSE))

typedef enum {
  AIFF_PARSE_START,
  AIFF_PARSE_HEADER,
  AIFF_PARSE_DATA
} GstAiffParseState;

typedef struct _GstAiffParse GstAiffParse;
typedef struct _GstAiffParseClass GstAiffParseClass;

/**
 * GstAiffParse:
 *
 * Opaque data structure.
 */
struct _GstAiffParse {
  GstElement parent;

  /*< private >*/
  GstPad      *sinkpad;
  GstPad      *srcpad;

  GstEvent    *close_segment;
  GstEvent    *start_segment;

  /* AIFF decoding state */
  GstAiffParseState state;

  /* format of audio, see defines below */
  gint format;

  gboolean is_aifc;

  /* useful audio data */
  guint32 rate;
  guint16 channels;
  guint16 width;
  guint16 depth;
  guint32 endianness;
  gboolean floating_point;

  /* real bytes per second used or 0 when no bitrate is known */
  guint32 bps;

  guint bytes_per_sample;
  guint max_buf_size;

  guint32   total_frames;

  guint32 ssnd_offset;
  guint32 ssnd_blocksize;

  /* position in data part */
  guint64	offset;
  guint64	end_offset;
  guint64 	dataleft;
  /* offset/length of data part */
  guint64 	datastart;
  guint64 	datasize;
  /* duration in time */
  guint64 	duration;

  /* pending seek */
  GstEvent *seek_event;

  /* For streaming */
  GstAdapter *adapter;
  gboolean got_comm;
  gboolean streaming;

  /* configured segment, start/stop expressed in time */
  GstSegment segment;
  gboolean segment_running;

  /* discont after seek */
  gboolean discont;

  /* tags */
  GstTagList *tags;
};

struct _GstAiffParseClass {
  GstElementClass parent_class;
};

GType gst_aiff_parse_get_type(void);

G_END_DECLS

#endif /* __GST_AIFF_PARSE_H__ */
