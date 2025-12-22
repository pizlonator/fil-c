/* GStreamer
 *
 * jpegparse: a parser for JPEG streams
 *
 * Copyright (C) <2009> Arnout Vandecappelle (Essensium/Mind) <arnout@mind.be>
 *               <2022> Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_JPEG_PARSE_H__
#define __GST_JPEG_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_JPEG_PARSE \
  (gst_jpeg_parse_get_type())
#define GST_JPEG_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JPEG_PARSE,GstJpegParse))
#define GST_JPEG_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JPEG_PARSE,GstJpegParseClass))
#define GST_IS_JPEG_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JPEG_PARSE))
#define GST_IS_JPEG_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JPEG_PARSE))
#define GST_JPEG_PARSE_CAST(obj) ((GstJpegParse *)obj)

typedef struct _GstJpegParse           GstJpegParse;
typedef struct _GstJpegParseClass      GstJpegParseClass;

struct _GstJpegParse {
  GstBaseParse parse;

  guint last_offset;
  gint state;

  gboolean first_picture;
  gboolean multiscope;
  gboolean avid;
  gboolean renegotiate;

  gint8 sof;
  gint8 adobe_transform;

  /* the parsed frame size */
  guint16 width, height;
  gint orig_width, orig_height;

  GstBuffer *codec_data;
  char *colorimetry;
  GstVideoInterlaceMode interlace_mode;
  GstVideoFieldOrder field_order;
  guint field;

  /* format color space */
  guint colorspace;
  guint sampling;
  gint par_num;
  gint par_den;
  GstCaps *prev_caps;

  /* fps */
  gint framerate_numerator;
  gint framerate_denominator;

  /* tags */
  GstTagList *tags;
};

struct _GstJpegParseClass {
  GstBaseParseClass  parent_class;
};

GType gst_jpeg_parse_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (jpegparse);

G_END_DECLS

#endif /* __GST_JPEG_PARSE_H__ */
