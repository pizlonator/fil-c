/* GStreamer
 * Copyright (C) 2007 Michael Smith <msmith@xiph.org>
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


#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#ifndef __VMNCDEC_H__
#define __VMNCDEC_H__

G_BEGIN_DECLS

#define GST_TYPE_VMNC_DEC \
  (gst_vmnc_dec_get_type())
#define GST_VMNC_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VMNC_DEC,GstVMncDec))


#define MAKE_TYPE(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|d)
enum
{
  TYPE_RAW = 0,
  TYPE_COPY = 1,
  TYPE_RRE = 2,
  TYPE_CoRRE = 4,
  TYPE_HEXTILE = 5,

  TYPE_WMVd = MAKE_TYPE ('W', 'M', 'V', 'd'),
  TYPE_WMVe = MAKE_TYPE ('W', 'M', 'V', 'e'),
  TYPE_WMVf = MAKE_TYPE ('W', 'M', 'V', 'f'),
  TYPE_WMVg = MAKE_TYPE ('W', 'M', 'V', 'g'),
  TYPE_WMVh = MAKE_TYPE ('W', 'M', 'V', 'h'),
  TYPE_WMVi = MAKE_TYPE ('W', 'M', 'V', 'i'),
  TYPE_WMVj = MAKE_TYPE ('W', 'M', 'V', 'j')
};

struct RFBFormat
{
  int width;
  int height;
  int stride;
  int bytes_per_pixel;
  int depth;
  int big_endian;

  guint8 descriptor[16];        /* The raw format descriptor block */
};

enum CursorType
{
  CURSOR_COLOUR = 0,
  CURSOR_ALPHA = 1
};

struct Cursor
{
  enum CursorType type;
  int visible;
  int x;
  int y;
  int width;
  int height;
  int hot_x;
  int hot_y;
  guint8 *cursordata;
  guint8 *cursormask;
};

typedef struct
{
  GstVideoDecoder parent;

  gboolean have_format;

  GstVideoCodecState *input_state;

  int framerate_num;
  int framerate_denom;

  struct Cursor cursor;
  struct RFBFormat format;
  guint8 *imagedata;
} GstVMncDec;

typedef struct
{
  GstVideoDecoderClass parent_class;
} GstVMncDecClass;

GType gst_vmnc_dec_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (vmncdec);

G_END_DECLS

#endif /* __VMNCDEC_H__ */
