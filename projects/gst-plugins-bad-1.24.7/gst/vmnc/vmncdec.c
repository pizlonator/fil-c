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

/*
 * This is a decoder for the VMWare VMnc video codec, which VMWare uses for
 * recording * of virtual machine instances.
 * It's essentially a serialisation of RFB (the VNC protocol)
 * 'FramebufferUpdate' messages, with some special encoding-types for VMnc
 * extensions. There's some documentation (with fixes from VMWare employees) at:
 *   http://wiki.multimedia.cx/index.php?title=VMware_Video
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "vmncdec.h"

#include <string.h>

static gboolean gst_vmnc_dec_reset (GstVideoDecoder * decoder);
static gboolean gst_vmnc_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_vmnc_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_vmnc_dec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos);
static gboolean gst_vmnc_dec_sink_event (GstVideoDecoder * bdec,
    GstEvent * event);

#define GST_CAT_DEFAULT vmnc_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define RFB_GET_UINT32(ptr) GST_READ_UINT32_BE(ptr)
#define RFB_GET_UINT16(ptr) GST_READ_UINT16_BE(ptr)
#define RFB_GET_UINT8(ptr) GST_READ_UINT8(ptr)

enum
{
  PROP_0,
};

enum
{
  ERROR_INVALID = -1,           /* Invalid data in bitstream */
  ERROR_INSUFFICIENT_DATA = -2  /* Haven't received enough data yet */
};

static GstStaticPadTemplate vmnc_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGBx, BGRx, xRGB, xBGR,"
            " RGB15, BGR15, RGB16, BGR16, GRAY8 }")));

static GstStaticPadTemplate vmnc_dec_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vmnc, version=(int)1, "
        "framerate=(fraction)[0, max], "
        "width=(int)[0, max], " "height=(int)[0, max]")
    );

G_DEFINE_TYPE (GstVMncDec, gst_vmnc_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (vmncdec, "vmncdec", GST_RANK_PRIMARY,
    GST_TYPE_VMNC_DEC);

static void
gst_vmnc_dec_class_init (GstVMncDecClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->start = gst_vmnc_dec_reset;
  decoder_class->stop = gst_vmnc_dec_reset;
  decoder_class->parse = gst_vmnc_dec_parse;
  decoder_class->handle_frame = gst_vmnc_dec_handle_frame;
  decoder_class->set_format = gst_vmnc_dec_set_format;
  decoder_class->sink_event = gst_vmnc_dec_sink_event;

  gst_element_class_add_static_pad_template (gstelement_class,
      &vmnc_dec_src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &vmnc_dec_sink_factory);
  gst_element_class_set_static_metadata (gstelement_class, "VMnc video decoder",
      "Codec/Decoder/Video", "Decode VmWare video to raw (RGB) video",
      "Michael Smith <msmith@xiph.org>");

  GST_DEBUG_CATEGORY_INIT (vmnc_debug, "vmncdec", 0, "VMnc decoder");
}

static void
gst_vmnc_dec_init (GstVMncDec * dec)
{
  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (dec));
}

static gboolean
gst_vmnc_dec_reset (GstVideoDecoder * decoder)
{
  GstVMncDec *dec = GST_VMNC_DEC (decoder);

  g_free (dec->imagedata);
  dec->imagedata = NULL;

  g_free (dec->cursor.cursordata);
  dec->cursor.cursordata = NULL;

  g_free (dec->cursor.cursormask);
  dec->cursor.cursormask = NULL;

  dec->cursor.visible = 0;

  dec->have_format = FALSE;

  if (dec->input_state)
    gst_video_codec_state_unref (dec->input_state);
  dec->input_state = NULL;

  return TRUE;
}

struct RfbRectangle
{
  guint16 x;
  guint16 y;
  guint16 width;
  guint16 height;

  gint32 type;
};

/* Rectangle handling functions.
 * Return number of bytes consumed, or < 0 on error
 */
typedef int (*rectangle_handler) (GstVMncDec * dec, struct RfbRectangle * rect,
    const guint8 * data, int len, gboolean decode);

static int
vmnc_handle_wmvi_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  GstVideoFormat format;
  gint bpp, tc;
  guint32 redmask, greenmask, bluemask;
  guint32 endianness, dataendianness;
  GstVideoCodecState *state;

  /* A WMVi rectangle has a 16byte payload */
  if (len < 16) {
    GST_DEBUG_OBJECT (dec, "Bad WMVi rect: too short");
    return ERROR_INSUFFICIENT_DATA;
  }

  /* We only compare 13 bytes; ignoring the 3 padding bytes at the end */
  if (dec->have_format && memcmp (data, dec->format.descriptor, 13) == 0) {
    /* Nothing changed, so just exit */
    return 16;
  }

  /* Store the whole block for simple comparison later */
  memcpy (dec->format.descriptor, data, 16);

  if (rect->x != 0 || rect->y != 0) {
    GST_WARNING_OBJECT (dec, "Bad WMVi rect: wrong coordinates");
    return ERROR_INVALID;
  }

  bpp = data[0];
  dec->format.depth = data[1];
  dec->format.big_endian = data[2];
  dataendianness = data[2] ? G_BIG_ENDIAN : G_LITTLE_ENDIAN;
  tc = data[3];

  if (bpp != 8 && bpp != 16 && bpp != 32) {
    GST_WARNING_OBJECT (dec, "Bad bpp value: %d", bpp);
    return ERROR_INVALID;
  }

  if (!tc) {
    GST_WARNING_OBJECT (dec, "Paletted video not supported");
    return ERROR_INVALID;
  }

  dec->format.bytes_per_pixel = bpp / 8;
  dec->format.width = rect->width;
  dec->format.height = rect->height;

  redmask = (guint32) (RFB_GET_UINT16 (data + 4)) << data[10];
  greenmask = (guint32) (RFB_GET_UINT16 (data + 6)) << data[11];
  bluemask = (guint32) (RFB_GET_UINT16 (data + 8)) << data[12];

  GST_DEBUG_OBJECT (dec, "Red: mask %d, shift %d",
      RFB_GET_UINT16 (data + 4), data[10]);
  GST_DEBUG_OBJECT (dec, "Green: mask %d, shift %d",
      RFB_GET_UINT16 (data + 6), data[11]);
  GST_DEBUG_OBJECT (dec, "Blue: mask %d, shift %d",
      RFB_GET_UINT16 (data + 8), data[12]);
  GST_DEBUG_OBJECT (dec, "BPP: %d. endianness: %s", bpp,
      data[2] ? "big" : "little");

  /* GStreamer's RGB caps are a bit weird. */
  if (bpp == 8) {
    endianness = G_BYTE_ORDER;  /* Doesn't matter */
  } else if (bpp == 16) {
    /* We require host-endian. */
    endianness = G_BYTE_ORDER;
  } else {                      /* bpp == 32 */
    /* We require big endian */
    endianness = G_BIG_ENDIAN;
    if (endianness != dataendianness) {
      redmask = GUINT32_SWAP_LE_BE (redmask);
      greenmask = GUINT32_SWAP_LE_BE (greenmask);
      bluemask = GUINT32_SWAP_LE_BE (bluemask);
    }
  }

  format = gst_video_format_from_masks (dec->format.depth, bpp, endianness,
      redmask, greenmask, bluemask, 0);

  GST_DEBUG_OBJECT (dec, "From depth: %d bpp: %u endianness: %s redmask: %X "
      "greenmask: %X bluemask: %X got format %s",
      dec->format.depth, bpp, endianness == G_BIG_ENDIAN ? "BE" : "LE",
      GUINT32_FROM_BE (redmask), GUINT32_FROM_BE (greenmask),
      GUINT32_FROM_BE (bluemask),
      format == GST_VIDEO_FORMAT_UNKNOWN ? "UNKNOWN" :
      gst_video_format_to_string (format));

  if (format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_WARNING_OBJECT (dec, "Video format unknown to GStreamer");
    return ERROR_INVALID;
  }

  dec->have_format = TRUE;
  if (!decode) {
    GST_LOG_OBJECT (dec, "Parsing, not setting caps");
    return 16;
  }


  state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (dec), format,
      rect->width, rect->height, dec->input_state);
  gst_video_codec_state_unref (state);

  g_free (dec->imagedata);
  dec->imagedata = g_malloc0 (dec->format.width * dec->format.height *
      dec->format.bytes_per_pixel);
  GST_DEBUG_OBJECT (dec, "Allocated image data at %p", dec->imagedata);

  dec->format.stride = dec->format.width * dec->format.bytes_per_pixel;

  return 16;
}

static void
render_colour_cursor (GstVMncDec * dec, guint8 * data, int x, int y,
    int off_x, int off_y, int width, int height)
{
  int i, j;
  guint8 *dstraw = data + dec->format.stride * y +
      dec->format.bytes_per_pixel * x;
  guint8 *srcraw = dec->cursor.cursordata +
      dec->cursor.width * dec->format.bytes_per_pixel * off_y;
  guint8 *maskraw = dec->cursor.cursormask +
      dec->cursor.width * dec->format.bytes_per_pixel * off_y;

  /* Boundchecking done by caller; this is just the renderer inner loop */
  if (dec->format.bytes_per_pixel == 1) {
    guint8 *dst = dstraw;
    guint8 *src = srcraw;
    guint8 *mask = maskraw;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        dst[j] = (dst[j] & src[j]) ^ mask[j];
      }
      dst += dec->format.width;
      src += dec->cursor.width;
      mask += dec->cursor.width;
    }
  } else if (dec->format.bytes_per_pixel == 2) {
    guint16 *dst = (guint16 *) dstraw;
    guint16 *src = (guint16 *) srcraw;
    guint16 *mask = (guint16 *) maskraw;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        dst[j] = (dst[j] & src[j]) ^ mask[j];
      }
      dst += dec->format.width;
      src += dec->cursor.width;
      mask += dec->cursor.width;
    }
  } else {
    guint32 *dst = (guint32 *) dstraw;
    guint32 *src = (guint32 *) srcraw;
    guint32 *mask = (guint32 *) maskraw;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        dst[j] = (dst[j] & src[j]) ^ mask[j];
      }
      dst += dec->format.width;
      src += dec->cursor.width;
      mask += dec->cursor.width;
    }
  }
}

static void
render_cursor (GstVMncDec * dec, guint8 * data)
{
  /* First, figure out the portion of the cursor that's on-screen */
  /* X,Y of top-left of cursor */
  int x = dec->cursor.x - dec->cursor.hot_x;
  int y = dec->cursor.y - dec->cursor.hot_y;

  /* Width, height of rendered portion of cursor */
  int width = dec->cursor.width;
  int height = dec->cursor.height;

  /* X,Y offset of rendered portion of cursor */
  int off_x = 0;
  int off_y = 0;

  if (x < 0) {
    off_x = -x;
    width += x;
    x = 0;
  }
  if (x + width > dec->format.width)
    width = dec->format.width - x;
  if (y < 0) {
    off_y = -y;
    height += y;
    y = 0;
  }
  if (y + height > dec->format.height)
    height = dec->format.height - y;

  if (dec->cursor.type == CURSOR_COLOUR) {
    render_colour_cursor (dec, data, x, y, off_x, off_y, width, height);
  } else {
    /* Alpha cursor. */
    /* TODO: Implement me! */
    GST_WARNING_OBJECT (dec, "Alpha composited cursors not yet implemented");
  }
}

static GstFlowReturn
vmnc_fill_buffer (GstVMncDec * dec, GstVideoCodecFrame * frame)
{
  GstFlowReturn ret;
  GstMapInfo map;

  ret =
      gst_video_decoder_allocate_output_frame (GST_VIDEO_DECODER (dec), frame);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_buffer_map (frame->output_buffer, &map, GST_MAP_READWRITE);

  memcpy (map.data, dec->imagedata, map.size);

  if (dec->cursor.visible)
    render_cursor (dec, map.data);

  gst_buffer_unmap (frame->output_buffer, &map);

  return GST_FLOW_OK;
}

static int
vmnc_handle_wmvd_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  /* Cursor data. */
  int datalen = 2;
  int type, size;

  if (len < datalen) {
    GST_LOG_OBJECT (dec, "Cursor data too short");
    return ERROR_INSUFFICIENT_DATA;
  }

  type = RFB_GET_UINT8 (data);

  if (type == CURSOR_COLOUR) {
    datalen += rect->width * rect->height * dec->format.bytes_per_pixel * 2;
  } else if (type == CURSOR_ALPHA) {
    datalen += rect->width * rect->height * 4;
  } else {
    GST_WARNING_OBJECT (dec, "Unknown cursor type: %d", type);
    return ERROR_INVALID;
  }

  if (len < datalen) {
    GST_LOG_OBJECT (dec, "Cursor data too short");
    return ERROR_INSUFFICIENT_DATA;
  } else if (!decode)
    return datalen;

  dec->cursor.type = type;
  dec->cursor.width = rect->width;
  dec->cursor.height = rect->height;
  dec->cursor.type = type;
  dec->cursor.hot_x = rect->x;
  dec->cursor.hot_y = rect->y;

  g_free (dec->cursor.cursordata);
  g_free (dec->cursor.cursormask);

  if (type == 0) {
    size = rect->width * rect->height * dec->format.bytes_per_pixel;
    dec->cursor.cursordata = g_malloc (size);
    dec->cursor.cursormask = g_malloc (size);
    memcpy (dec->cursor.cursordata, data + 2, size);
    memcpy (dec->cursor.cursormask, data + 2 + size, size);
  } else {
    dec->cursor.cursordata = g_malloc (rect->width * rect->height * 4);
    memcpy (dec->cursor.cursordata, data + 2, rect->width * rect->height * 4);
  }

  return datalen;
}

static int
vmnc_handle_wmve_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  guint16 flags;

  /* Cursor state. */
  if (len < 2) {
    GST_LOG_OBJECT (dec, "Cursor data too short");
    return ERROR_INSUFFICIENT_DATA;
  } else if (!decode)
    return 2;

  flags = RFB_GET_UINT16 (data);
  dec->cursor.visible = flags & 0x01;

  return 2;
}

static int
vmnc_handle_wmvf_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  /* Cursor position. */
  dec->cursor.x = rect->x;
  dec->cursor.y = rect->y;
  return 0;
}

static int
vmnc_handle_wmvg_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  /* Keyboard stuff; not interesting for playback */
  if (len < 10) {
    GST_LOG_OBJECT (dec, "Keyboard data too short");
    return ERROR_INSUFFICIENT_DATA;
  }
  return 10;
}

static int
vmnc_handle_wmvh_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  /* More keyboard stuff; not interesting for playback */
  if (len < 4) {
    GST_LOG_OBJECT (dec, "Keyboard data too short");
    return ERROR_INSUFFICIENT_DATA;
  }
  return 4;
}

static int
vmnc_handle_wmvj_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  /* VM state info, not interesting for playback */
  if (len < 2) {
    GST_LOG_OBJECT (dec, "VM state data too short");
    return ERROR_INSUFFICIENT_DATA;
  }
  return 2;
}

static void
render_raw_tile (GstVMncDec * dec, const guint8 * data, int x, int y,
    int width, int height)
{
  int i;
  guint8 *dst;
  const guint8 *src;
  int line;

  src = data;
  dst = dec->imagedata + dec->format.stride * y +
      dec->format.bytes_per_pixel * x;
  line = width * dec->format.bytes_per_pixel;

  for (i = 0; i < height; i++) {
    /* This is wrong-endian currently */
    memcpy (dst, src, line);

    dst += dec->format.stride;
    src += line;
  }
}

static void
render_subrect (GstVMncDec * dec, int x, int y, int width,
    int height, guint32 colour)
{
  /* Crazy inefficient! */
  int i, j;
  guint8 *dst;

  for (i = 0; i < height; i++) {
    dst = dec->imagedata + dec->format.stride * (y + i) +
        dec->format.bytes_per_pixel * x;
    for (j = 0; j < width; j++) {
      memcpy (dst, &colour, dec->format.bytes_per_pixel);
      dst += dec->format.bytes_per_pixel;
    }
  }
}

static int
vmnc_handle_raw_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  int datalen = rect->width * rect->height * dec->format.bytes_per_pixel;

  if (len < datalen) {
    GST_LOG_OBJECT (dec, "Raw data too short");
    return ERROR_INSUFFICIENT_DATA;
  }

  if (decode)
    render_raw_tile (dec, data, rect->x, rect->y, rect->width, rect->height);

  return datalen;
}

static int
vmnc_handle_copy_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  int src_x, src_y;
  guint8 *src, *dst;
  int i;

  if (len < 4) {
    GST_LOG_OBJECT (dec, "Copy data too short");
    return ERROR_INSUFFICIENT_DATA;
  } else if (!decode)
    return 4;

  src_x = RFB_GET_UINT16 (data);
  src_y = RFB_GET_UINT16 (data + 2);

  /* Our destination rectangle is guaranteed in-frame; check this for the source
   * rectangle. */
  if (src_x + rect->width > dec->format.width ||
      src_y + rect->height > dec->format.height) {
    GST_WARNING_OBJECT (dec, "Source rectangle out of range");
    return ERROR_INVALID;
  }

  if (src_y > rect->y || src_x > rect->x) {
    /* Moving forward */
    src = dec->imagedata + dec->format.stride * src_y +
        dec->format.bytes_per_pixel * src_x;
    dst = dec->imagedata + dec->format.stride * rect->y +
        dec->format.bytes_per_pixel * rect->x;
    for (i = 0; i < rect->height; i++) {
      memmove (dst, src, rect->width * dec->format.bytes_per_pixel);
      dst += dec->format.stride;
      src += dec->format.stride;
    }
  } else {
    /* Backwards */
    src = dec->imagedata + dec->format.stride * (src_y + rect->height - 1) +
        dec->format.bytes_per_pixel * src_x;
    dst = dec->imagedata + dec->format.stride * (rect->y + rect->height - 1) +
        dec->format.bytes_per_pixel * rect->x;
    for (i = rect->height; i > 0; i--) {
      memmove (dst, src, rect->width * dec->format.bytes_per_pixel);
      dst -= dec->format.stride;
      src -= dec->format.stride;
    }
  }

  return 4;
}

/* FIXME: data+off might not be properly aligned */
#define READ_PIXEL(pixel, data, off, len)         \
  if (dec->format.bytes_per_pixel == 1) {         \
     if (off >= len)                              \
       return ERROR_INSUFFICIENT_DATA;            \
     pixel = data[off++];                         \
  } else if (dec->format.bytes_per_pixel == 2) {  \
     if (off+2 > len)                             \
       return ERROR_INSUFFICIENT_DATA;            \
     pixel = (*(guint16 *)(data + off));          \
     off += 2;                                    \
  } else {                                        \
     if (off+4 > len)                             \
       return ERROR_INSUFFICIENT_DATA;            \
     pixel = (*(guint32 *)(data + off));          \
     off += 4;                                    \
  }

static int
vmnc_handle_hextile_rectangle (GstVMncDec * dec, struct RfbRectangle *rect,
    const guint8 * data, int len, gboolean decode)
{
  int tilesx = GST_ROUND_UP_16 (rect->width) / 16;
  int tilesy = GST_ROUND_UP_16 (rect->height) / 16;
  int x, y, z;
  int off = 0;
  int subrects;
  int coloured;
  int width, height;
  guint32 fg = 0, bg = 0, colour;
  guint8 flags;

  for (y = 0; y < tilesy; y++) {
    if (y == tilesy - 1)
      height = rect->height - (tilesy - 1) * 16;
    else
      height = 16;

    for (x = 0; x < tilesx; x++) {
      if (x == tilesx - 1)
        width = rect->width - (tilesx - 1) * 16;
      else
        width = 16;

      if (off >= len) {
        return ERROR_INSUFFICIENT_DATA;
      }
      flags = data[off++];

      if (flags & 0x1) {
        if (off + width * height * dec->format.bytes_per_pixel > len) {
          return ERROR_INSUFFICIENT_DATA;
        }
        if (decode)
          render_raw_tile (dec, data + off, rect->x + x * 16, rect->y + y * 16,
              width, height);
        off += width * height * dec->format.bytes_per_pixel;
      } else {
        if (flags & 0x2) {
          READ_PIXEL (bg, data, off, len)
        }
        if (flags & 0x4) {
          READ_PIXEL (fg, data, off, len)
        }

        subrects = 0;
        if (flags & 0x8) {
          if (off >= len) {
            return ERROR_INSUFFICIENT_DATA;
          }
          subrects = data[off++];
        }

        /* Paint background colour on entire tile */
        if (decode)
          render_subrect (dec, rect->x + x * 16, rect->y + y * 16,
              width, height, bg);

        coloured = flags & 0x10;
        for (z = 0; z < subrects; z++) {
          if (coloured) {
            READ_PIXEL (colour, data, off, len);
          } else
            colour = fg;
          if (off + 2 > len)
            return ERROR_INSUFFICIENT_DATA;

          {
            int off_x = (data[off] & 0xf0) >> 4;
            int off_y = (data[off] & 0x0f);
            int w = ((data[off + 1] & 0xf0) >> 4) + 1;
            int h = (data[off + 1] & 0x0f) + 1;

            off += 2;

            /* Ensure we don't have out of bounds coordinates */
            if (off_x + w > width || off_y + h > height) {
              GST_WARNING_OBJECT (dec, "Subrect out of bounds: %d-%d x %d-%d "
                  "extends outside %dx%d", off_x, w, off_y, h, width, height);
              return ERROR_INVALID;
            }

            if (decode)
              render_subrect (dec, rect->x + x * 16 + off_x,
                  rect->y + y * 16 + off_y, w, h, colour);
          }
        }
      }
    }
  }

  return off;
}

/* Handle a packet in one of two modes: decode or parse.
 * In parse mode, we don't execute any of the decoding, we just do enough to
 * figure out how many bytes it contains
 *
 * Returns: >= 0, the number of bytes consumed
 *           < 0, packet too short to decode, or error
 */
static int
vmnc_handle_packet (GstVMncDec * dec, const guint8 * data, int len,
    gboolean decode)
{
  int type;
  int offset = 0;

  if (len < 4) {
    GST_LOG_OBJECT (dec, "Packet too short");
    return ERROR_INSUFFICIENT_DATA;
  }

  type = data[0];

  switch (type) {
    case 0:
    {
      int numrect = RFB_GET_UINT16 (data + 2);
      int i;
      int read;

      offset = 4;

      for (i = 0; i < numrect; i++) {
        struct RfbRectangle r;
        rectangle_handler handler;

        if (len < offset + 12) {
          GST_LOG_OBJECT (dec,
              "Packet too short for rectangle header: %d < %d",
              len, offset + 12);
          return ERROR_INSUFFICIENT_DATA;
        }
        GST_LOG_OBJECT (dec, "Reading rectangle %d", i);
        r.x = RFB_GET_UINT16 (data + offset);
        r.y = RFB_GET_UINT16 (data + offset + 2);
        r.width = RFB_GET_UINT16 (data + offset + 4);
        r.height = RFB_GET_UINT16 (data + offset + 6);
        r.type = RFB_GET_UINT32 (data + offset + 8);

        if (r.type != TYPE_WMVi) {
          /* We must have a WMVi packet to initialise things before we can 
           * continue */
          if (!dec->have_format) {
            GST_WARNING_OBJECT (dec, "Received packet without WMVi: %d",
                r.type);
            return ERROR_INVALID;
          }
          if (r.x > dec->format.width || r.y > dec->format.height ||
              r.x + r.width > dec->format.width ||
              r.y + r.height > dec->format.height) {
            GST_WARNING_OBJECT (dec, "Rectangle out of range, type %d", r.type);
            return ERROR_INVALID;
          }
        } else if (r.width > 16384 || r.height > 16384) {
          GST_WARNING_OBJECT (dec, "Width or height too high: %ux%u", r.width,
              r.height);
          return ERROR_INVALID;
        }

        switch (r.type) {
          case TYPE_WMVd:
            handler = vmnc_handle_wmvd_rectangle;
            break;
          case TYPE_WMVe:
            handler = vmnc_handle_wmve_rectangle;
            break;
          case TYPE_WMVf:
            handler = vmnc_handle_wmvf_rectangle;
            break;
          case TYPE_WMVg:
            handler = vmnc_handle_wmvg_rectangle;
            break;
          case TYPE_WMVh:
            handler = vmnc_handle_wmvh_rectangle;
            break;
          case TYPE_WMVi:
            handler = vmnc_handle_wmvi_rectangle;
            break;
          case TYPE_WMVj:
            handler = vmnc_handle_wmvj_rectangle;
            break;
          case TYPE_RAW:
            handler = vmnc_handle_raw_rectangle;
            break;
          case TYPE_COPY:
            handler = vmnc_handle_copy_rectangle;
            break;
          case TYPE_HEXTILE:
            handler = vmnc_handle_hextile_rectangle;
            break;
          default:
            GST_WARNING_OBJECT (dec, "Unknown rectangle type");
            return ERROR_INVALID;
        }

        read = handler (dec, &r, data + offset + 12, len - offset - 12, decode);
        if (read < 0) {
          GST_DEBUG_OBJECT (dec, "Error calling rectangle handler");
          return read;
        }
        offset += 12 + read;
      }
      break;
    }
    default:
      GST_WARNING_OBJECT (dec, "Packet type unknown: %d", type);
      return ERROR_INVALID;
  }

  return offset;
}

static gboolean
gst_vmnc_dec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstVMncDec *dec = GST_VMNC_DEC (decoder);

  /* We require a format descriptor in-stream, so we ignore the info from the
   * container here. We just use the framerate */

  if (dec->input_state)
    gst_video_codec_state_unref (dec->input_state);
  dec->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static gboolean
gst_vmnc_dec_sink_event (GstVideoDecoder * bdec, GstEvent * event)
{
  const GstSegment *segment;

  if (GST_EVENT_TYPE (event) != GST_EVENT_SEGMENT)
    goto done;

  gst_event_parse_segment (event, &segment);

  if (segment->format == GST_FORMAT_TIME)
    gst_video_decoder_set_packetized (bdec, TRUE);
  else
    gst_video_decoder_set_packetized (bdec, FALSE);

done:
  return GST_VIDEO_DECODER_CLASS (gst_vmnc_dec_parent_class)->sink_event (bdec,
      event);
}

static GstFlowReturn
gst_vmnc_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVMncDec *dec = GST_VMNC_DEC (decoder);
  int res;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;

  if (!gst_buffer_map (frame->input_buffer, &map, GST_MAP_READ))
    return GST_FLOW_ERROR;

  res = vmnc_handle_packet (dec, map.data, map.size, TRUE);

  gst_buffer_unmap (frame->input_buffer, &map);

  if (!dec->have_format) {
    GST_VIDEO_DECODER_ERROR (decoder, 2, STREAM, DECODE, (NULL),
        ("Data found before header"), ret);
    gst_video_decoder_drop_frame (decoder, frame);
  } else if (res < 0) {
    ret = GST_FLOW_ERROR;
    gst_video_decoder_drop_frame (decoder, frame);
    GST_VIDEO_DECODER_ERROR (decoder, 1, STREAM, DECODE, (NULL),
        ("Couldn't decode packet"), ret);
  } else {
    GST_LOG_OBJECT (dec, "read %d bytes of %" G_GSIZE_FORMAT, res,
        gst_buffer_get_size (frame->input_buffer));
    /* inbuf may be NULL; that's ok */
    ret = vmnc_fill_buffer (dec, frame);
    if (ret == GST_FLOW_OK)
      gst_video_decoder_finish_frame (decoder, frame);
    else
      gst_video_decoder_drop_frame (decoder, frame);
  }

  return ret;
}


static GstFlowReturn
gst_vmnc_dec_parse (GstVideoDecoder * decoder, GstVideoCodecFrame * frame,
    GstAdapter * adapter, gboolean at_eos)
{
  GstVMncDec *dec = GST_VMNC_DEC (decoder);
  const guint8 *data;
  int avail;
  int len;

  avail = gst_adapter_available (adapter);
  data = gst_adapter_map (adapter, avail);

  GST_LOG_OBJECT (dec, "Parsing %d bytes", avail);

  len = vmnc_handle_packet (dec, data, avail, FALSE);

  if (len == ERROR_INSUFFICIENT_DATA) {
    GST_LOG_OBJECT (dec, "Not enough data yet");
    return GST_VIDEO_DECODER_FLOW_NEED_DATA;
  } else if (len < 0) {
    GST_ERROR_OBJECT (dec, "Fatal error in bitstream");
    return GST_FLOW_ERROR;
  } else {
    GST_LOG_OBJECT (dec, "Parsed packet: %d bytes", len);
    gst_video_decoder_add_to_frame (decoder, len);
    return gst_video_decoder_have_frame (decoder);
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (vmncdec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vmnc,
    "VmWare Video Codec plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
