/* GStreamer
 * Copyright (C) <2020> Jan Schmidt <jan@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

//#define HACK_2BIT /* Force 2-bit output by discarding colours */
//#define HACK_4BIT /* Force 4-bit output by discarding colours */

#include "gstdvbsubenc.h"
#include <gst/base/gstbytewriter.h>
#include <gst/base/gstbitwriter.h>

#include "libimagequant/libimagequant.h"

#define DVB_SEGMENT_SYNC_BYTE 0xF

enum DVBSegmentType
{
  DVB_SEGMENT_TYPE_PAGE_COMPOSITION = 0x10,
  DVB_SEGMENT_TYPE_REGION_COMPOSITION = 0x11,
  DVB_SEGMENT_TYPE_CLUT_DEFINITION = 0x12,
  DVB_SEGMENT_TYPE_OBJECT_DATA = 0x13,
  DVB_SEGMENT_TYPE_DISPLAY_DEFINITION = 0x14,

  DVB_SEGMENT_TYPE_END_OF_DISPLAY = 0x80
};

enum DVBPixelDataType
{
  DVB_PIXEL_DATA_TYPE_2BIT = 0x10,
  DVB_PIXEL_DATA_TYPE_4BIT = 0x11,
  DVB_PIXEL_DATA_TYPE_8BIT = 0x12,
  DVB_PIXEL_DATA_TYPE_END_OF_LINE = 0xF0
};

struct HistogramEntry
{
  guint32 colour;
  guint32 count;
  guint32 substitution;
};

struct ColourEntry
{
  guint32 colour;
  guint32 pix_index;
};

typedef struct HistogramEntry HistogramEntry;
typedef struct ColourEntry ColourEntry;

static gint
compare_uint32 (gconstpointer a, gconstpointer b)
{
  guint32 v1 = *(guint32 *) (a);
  guint32 v2 = *(guint32 *) (b);

  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return 1;
  return 0;
}

static gint
compare_colour_entry_colour (gconstpointer a, gconstpointer b)
{
  const ColourEntry *c1 = (ColourEntry *) (a);
  const ColourEntry *c2 = (ColourEntry *) (b);

  /* Reverse order, so highest alpha comes first: */
  return compare_uint32 (&c2->colour, &c1->colour);
}

static void
image_get_rgba_row_callback (liq_color row_out[], int row_index, int width,
    void *user_info)
{
  int column_index;
  GstVideoFrame *src = (GstVideoFrame *) (user_info);
  guint8 *src_pixels = (guint8 *) (src->data[0]);
  const guint32 src_stride = GST_VIDEO_INFO_PLANE_STRIDE (&src->info, 0);
  guint8 *src_row = src_pixels + (row_index * src_stride);
  gint offset = 0;

  for (column_index = 0; column_index < width; column_index++) {
    liq_color *col = row_out + column_index;
    guint8 *p = src_row + offset;

    /* FIXME: We pass AYUV into the ARGB colour values,
     * which works but probably makes suboptimal choices about
     * which colours to preserve. It would be better to convert to RGBA
     * and back again, or to modify libimagequant to handle ayuv */
    col->a = p[0];
    col->r = p[1];
    col->g = p[2];
    col->b = p[3];

    offset += 4;
  }
}

/*
 * Utility function to unintelligently extract a
 * (max) 256 colour image from an AYUV input
 * Dumb for now, but could be improved if needed. If there's
 * more than 256 colours in the input, it will reduce it 256
 * by taking the most common 255 colours + transparent and mapping all
 * remaining colours to the nearest neighbour.
 *
 * FIXME: Integrate a better palette selection algorithm.
 */
gboolean
gst_dvbsubenc_ayuv_to_ayuv8p (GstVideoFrame * src, GstVideoFrame * dest,
    int max_colours, guint32 * out_num_colours)
{
  /* Allocate a temporary array the size of the input frame, copy in
   * the source pixels, sort them by value and then count the first
   * up to 256 colours. */
  gboolean ret = FALSE;

  GArray *colours, *histogram;
  gint i, num_pixels, dest_y_index, out_index;
  guint num_colours, cur_count;
  guint32 last;
  guint8 *s;
  HistogramEntry *h;
  ColourEntry *c;
  const guint32 src_stride = GST_VIDEO_INFO_PLANE_STRIDE (&src->info, 0);
  const guint32 dest_stride = GST_VIDEO_INFO_PLANE_STRIDE (&dest->info, 0);

  if (GST_VIDEO_INFO_FORMAT (&src->info) != GST_VIDEO_FORMAT_AYUV)
    return FALSE;

  if (GST_VIDEO_INFO_WIDTH (&src->info) != GST_VIDEO_INFO_WIDTH (&dest->info) ||
      GST_VIDEO_INFO_HEIGHT (&src->info) != GST_VIDEO_INFO_HEIGHT (&dest->info))
    return FALSE;

  num_pixels =
      GST_VIDEO_INFO_WIDTH (&src->info) * GST_VIDEO_INFO_HEIGHT (&src->info);
  s = (guint8 *) (src->data[0]);

  colours = g_array_sized_new (FALSE, FALSE, sizeof (ColourEntry), num_pixels);
  colours = g_array_set_size (colours, num_pixels);

  histogram =
      g_array_sized_new (FALSE, TRUE, sizeof (HistogramEntry), num_pixels);
  histogram = g_array_set_size (histogram, num_pixels);

  /* Copy the pixels to an array we can sort, dropping any stride padding,
   * and recording the output index into the destination bitmap in the
   * pix_index field */
  dest_y_index = 0;
  out_index = 0;
  for (i = 0; i < GST_VIDEO_INFO_HEIGHT (&src->info); i++) {
    guint32 x_index;
    gint x;

    for (x = 0, x_index = 0; x < GST_VIDEO_INFO_WIDTH (&src->info);
        x++, x_index += 4) {
      guint8 *pix = s + x_index;

      c = &g_array_index (colours, ColourEntry, out_index);
      c->colour = GST_READ_UINT32_BE (pix);
      c->pix_index = dest_y_index + x;

      out_index++;
    }

    s += src_stride;
    dest_y_index += dest_stride;
  }

  /* Build a histogram of the highest colour counts: */
  g_array_sort (colours, compare_colour_entry_colour);
  c = &g_array_index (colours, ColourEntry, 0);
  last = c->colour;
  num_colours = 0;
  cur_count = 1;
  for (i = 1; i < num_pixels; i++) {
    ColourEntry *c = &g_array_index (colours, ColourEntry, i);
    guint32 cur = c->colour;

    if (cur == last) {
      cur_count++;
      continue;
    }

    /* Colour changed - add an entry to the histogram */
    h = &g_array_index (histogram, HistogramEntry, num_colours);
    h->colour = last;
    h->count = cur_count;

    num_colours++;
    cur_count = 1;
    last = cur;
  }
  h = &g_array_index (histogram, HistogramEntry, num_colours);
  h->colour = last;
  h->count = cur_count;
  num_colours++;

  GST_LOG ("image has %u colours", num_colours);
  histogram = g_array_set_size (histogram, num_colours);

  if (num_colours > max_colours) {
    liq_image *image;
    liq_result *res;
    const liq_palette *pal;
    int i;
    int height = GST_VIDEO_INFO_HEIGHT (&src->info);
    unsigned char **dest_rows = malloc (height * sizeof (void *));
    guint8 *dest_palette = (guint8 *) (dest->data[1]);
    liq_attr *attr = liq_attr_create ();
    gint out_index = 0;

    for (i = 0; i < height; i++) {
      dest_rows[i] = (guint8 *) (dest->data[0]) + i * dest_stride;
    }

    liq_set_max_colors (attr, max_colours);

    image = liq_image_create_custom (attr, image_get_rgba_row_callback, src,
        GST_VIDEO_INFO_WIDTH (&src->info), GST_VIDEO_INFO_HEIGHT (&src->info),
        0);

    res = liq_quantize_image (attr, image);

    liq_write_remapped_image_rows (res, image, dest_rows);

    pal = liq_get_palette (res);
    num_colours = pal->count;

    /* Write out the palette */
    for (i = 0; i < num_colours; i++) {
      guint8 *c = dest_palette + out_index;
      const liq_color *col = pal->entries + i;

      c[0] = col->a;
      c[1] = col->r;
      c[2] = col->g;
      c[3] = col->b;

      out_index += 4;
    }

    free (dest_rows);

    liq_attr_destroy (attr);
    liq_image_destroy (image);
    liq_result_destroy (res);
  } else {
    guint8 *d = (guint8 *) (dest->data[0]);
    guint8 *palette = (guint8 *) (dest->data[1]);
    gint out_index = 0;

    /* Write out the palette */
    for (i = 0; i < num_colours; i++) {
      h = &g_array_index (histogram, HistogramEntry, i);
      GST_WRITE_UINT32_BE (palette + out_index, h->colour);
      out_index += 4;
    }

    /* Write out the palette image. At this point, both the
     * colours and histogram arrays are sorted in descending AYUV value,
     * so walk them both and write out the current palette index */
    out_index = 0;
    for (i = 0; i < num_pixels; i++) {
      c = &g_array_index (colours, ColourEntry, i);
      h = &g_array_index (histogram, HistogramEntry, out_index);

      if (c->colour != h->colour) {
        out_index++;
        h = &g_array_index (histogram, HistogramEntry, out_index);
        g_assert (h->colour == c->colour);      /* We must be walking colours in the same order in both arrays */
      }
      d[c->pix_index] = out_index;
    }
  }

  ret = TRUE;
  if (out_num_colours)
    *out_num_colours = num_colours;

  g_array_free (colours, TRUE);
  g_array_free (histogram, TRUE);

  return ret;
}

typedef void (*EncodeRLEFunc) (GstByteWriter * b, const guint8 * pixels,
    const gint stride, const gint w, const gint h);

static void
encode_rle2 (GstByteWriter * b, const guint8 * pixels,
    const gint stride, const gint w, const gint h)
{
  GstBitWriter bits;

  int y;

  gst_bit_writer_init (&bits);

  for (y = 0; y < h; y++) {
    int x = 0;
    guint size;

    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_2BIT);

    while (x < w) {
      int x_end = x;
      int run_length;
      guint8 pix;

      pix = pixels[x_end++];
      while (x_end < w && pixels[x_end] == pix)
        x_end++;

#ifdef HACK_2BIT
      pix >>= 6;                /* HACK to convert 8 bit to 2 bit palette */
#endif

      /* 284 is the largest run length we can encode */
      run_length = MIN (x_end - x, 284);

      if (run_length >= 29) {
        /* 000011LLLL = run 29 to 284 pixels */
        if (run_length > 284)
          run_length = 284;

        gst_bit_writer_put_bits_uint8 (&bits, 0x03, 6);
        gst_bit_writer_put_bits_uint8 (&bits, run_length - 29, 8);
        gst_bit_writer_put_bits_uint8 (&bits, pix, 2);
      } else if (run_length >= 12 && run_length <= 27) {
        /* 000010LLLL = run 12 to 27 pixels */
        gst_bit_writer_put_bits_uint8 (&bits, 0x02, 6);
        gst_bit_writer_put_bits_uint8 (&bits, run_length - 12, 4);
        gst_bit_writer_put_bits_uint8 (&bits, pix, 2);
      } else if (run_length >= 3 && run_length <= 10) {
        /* 001LL = run 3 to 10 pixels */
        gst_bit_writer_put_bits_uint8 (&bits, 0, 2);
        gst_bit_writer_put_bits_uint8 (&bits, 0x8 + run_length - 3, 4);
        gst_bit_writer_put_bits_uint8 (&bits, pix, 2);
      }
      /* Missed cases - 11 pixels, 28 pixels or a short length 1 or 2 pixels
       * - write out a single pixel if != 0, or 1 or 2 pixels of black */
      else if (pix != 0) {
        gst_bit_writer_put_bits_uint8 (&bits, pix, 2);
        run_length = 1;
      } else if (run_length == 2) {
        /* 0000 01 - 2 pixels colour 0 */
        gst_bit_writer_put_bits_uint8 (&bits, 0x1, 6);
        run_length = 2;
      } else {
        /* 0001 - single pixel colour 0 */
        gst_bit_writer_put_bits_uint8 (&bits, 0x1, 4);
        run_length = 1;
      }

      x += run_length;
      GST_LOG ("%u pixels = colour %u", run_length, pix);
    }

    /* End of line 0x00 */
    gst_bit_writer_put_bits_uint8 (&bits, 0x00, 8);

    /* pad by 4 bits if needed to byte align, then
     * write bit string to output */
    gst_bit_writer_align_bytes (&bits, 0);
    size = gst_bit_writer_get_size (&bits);

    gst_byte_writer_put_data (b, gst_bit_writer_get_data (&bits), size / 8);

    gst_bit_writer_reset (&bits);
    gst_bit_writer_init (&bits);

    GST_LOG ("y %u 2-bit RLE string = %u bits", y, size);
    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_END_OF_LINE);
    pixels += stride;
  }
}

static void
encode_rle4 (GstByteWriter * b, const guint8 * pixels,
    const gint stride, const gint w, const gint h)
{
  GstBitWriter bits;

  int y;

  gst_bit_writer_init (&bits);

  for (y = 0; y < h; y++) {
    int x = 0;
    guint size;

    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_4BIT);

    while (x < w) {
      int x_end = x;
      int run_length;
      guint8 pix;

      pix = pixels[x_end++];
      while (x_end < w && pixels[x_end] == pix)
        x_end++;

      /* 280 is the largest run length we can encode */
      run_length = MIN (x_end - x, 280);

      GST_LOG ("Encoding run %u pixels = colour %u", run_length, pix);

#ifdef HACK_4BIT
      pix >>= 4;                /* HACK to convert 8 bit to 4 palette */
#endif

      if (pix == 0 && run_length >= 3 && run_length <= 9) {
        gst_bit_writer_put_bits_uint8 (&bits, 0, 4);
        gst_bit_writer_put_bits_uint8 (&bits, run_length - 2, 4);
      } else if (run_length >= 4 && run_length < 25) {
        /* 4 to 7 pixels encoding */
        if (run_length > 7)
          run_length = 7;

        gst_bit_writer_put_bits_uint8 (&bits, 0, 4);
        gst_bit_writer_put_bits_uint8 (&bits, 0x8 + run_length - 4, 4);
        gst_bit_writer_put_bits_uint8 (&bits, pix, 4);
      } else if (run_length >= 25) {
        /* Run length 25 to 280 pixels */
        if (run_length > 280)
          run_length = 280;

        gst_bit_writer_put_bits_uint8 (&bits, 0x0f, 8);
        gst_bit_writer_put_bits_uint8 (&bits, run_length - 25, 8);
        gst_bit_writer_put_bits_uint8 (&bits, pix, 4);
      }
      /* Short length, 1, 2 or 3 pixels - write out a single pixel if != 0,
       * or 1 or 2 pixels of black */
      else if (pix != 0) {
        gst_bit_writer_put_bits_uint8 (&bits, pix, 4);
        run_length = 1;
      } else if (run_length > 1) {
        /* 0000 1101 */
        gst_bit_writer_put_bits_uint8 (&bits, 0xd, 8);
        run_length = 2;
      } else {
        /* 0000 1100 */
        gst_bit_writer_put_bits_uint8 (&bits, 0xc, 8);
        run_length = 1;
      }
      x += run_length;

      GST_LOG ("Put %u pixels = colour %u", run_length, pix);
    }

    /* End of line 0x00 */
    gst_bit_writer_put_bits_uint8 (&bits, 0x00, 8);

    /* pad by 4 bits if needed to byte align, then
     * write bit string to output */
    gst_bit_writer_align_bytes (&bits, 0);
    size = gst_bit_writer_get_size (&bits);

    gst_byte_writer_put_data (b, gst_bit_writer_get_data (&bits), size / 8);

    gst_bit_writer_reset (&bits);
    gst_bit_writer_init (&bits);

    GST_LOG ("y %u 4-bit RLE string = %u bits", y, size);
    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_END_OF_LINE);
    pixels += stride;
  }
}

static void
encode_rle8 (GstByteWriter * b, const guint8 * pixels,
    const gint stride, const gint w, const gint h)
{
  int y;

  for (y = 0; y < h; y++) {
    int x = 0;

    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_8BIT);

    while (x < w) {
      int x_end = x;
      int run_length;
      guint8 pix;

      pix = pixels[x_end++];
      while (x_end < w && pixels[x_end] == pix)
        x_end++;

      /* 127 is the largest run length we can encode */
      run_length = MIN (x_end - x, 127);

      if (run_length == 1 && pix != 0) {
        /* a single non-zero pixel - encode directly */
        gst_byte_writer_put_uint8 (b, pix);
      } else if (pix == 0) {
        /* Encode up to 1-127 pixels of colour 0 */
        gst_byte_writer_put_uint8 (b, 0);
        gst_byte_writer_put_uint8 (b, run_length);
      } else if (run_length > 2) {
        /* Encode 3-127 pixels of colour 'pix' directly */
        gst_byte_writer_put_uint8 (b, 0);
        gst_byte_writer_put_uint8 (b, 0x80 | run_length);
        gst_byte_writer_put_uint8 (b, pix);
      } else {
        /* Short 1-2 pixel run, encode it directly */
        if (run_length == 2)
          gst_byte_writer_put_uint8 (b, pix);
        gst_byte_writer_put_uint8 (b, pix);
        g_assert (run_length == 1 || run_length == 2);
      }
      x += run_length;
    }

    /* End of line bytes */
    gst_byte_writer_put_uint8 (b, 0x00);
    // This 2nd 0x00 byte is correct from the spec, but ffmpeg
    // as of 2020-04-24 does not like it
    gst_byte_writer_put_uint8 (b, 0x00);
    gst_byte_writer_put_uint8 (b, DVB_PIXEL_DATA_TYPE_END_OF_LINE);
    pixels += stride;
  }
}

static gboolean
dvbenc_write_object_data (GstByteWriter * b, int object_version, int page_id,
    int object_id, SubpictureRect * s)
{
  guint seg_size_pos, end_pos, bottom_end_pos;
  guint pixel_fields_size_pos, top_start_pos, bottom_start_pos;
  EncodeRLEFunc encode_rle_func;
  const gint stride = GST_VIDEO_INFO_PLANE_STRIDE (&s->frame->info, 0);
  const gint w = GST_VIDEO_INFO_WIDTH (&s->frame->info);
  const gint h = GST_VIDEO_INFO_HEIGHT (&s->frame->info);
  const guint8 *pixels = (guint8 *) (s->frame->data[0]);

  if (s->nb_colours <= 4)
    encode_rle_func = encode_rle2;
  else if (s->nb_colours <= 16)
    encode_rle_func = encode_rle4;
  else
    encode_rle_func = encode_rle8;

  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_TYPE_OBJECT_DATA);
  gst_byte_writer_put_uint16_be (b, page_id);
  seg_size_pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_put_uint16_be (b, 0);
  gst_byte_writer_put_uint16_be (b, object_id);
  /* version number, coding_method (0), non-modifying-flag (0), reserved bit */
  gst_byte_writer_put_uint8 (b, (object_version << 4) | 0x01);

  pixel_fields_size_pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_put_uint16_be (b, 0);
  gst_byte_writer_put_uint16_be (b, 0);

  /* Write the top field (even) lines (round up lines / 2) */
  top_start_pos = gst_byte_writer_get_pos (b);
  encode_rle_func (b, pixels, stride * 2, w, (h + 1) / 2);

  /* Write the bottom field (odd) lines (round down lines / 2) */
  bottom_start_pos = gst_byte_writer_get_pos (b);
  if (h > 1)
    encode_rle_func (b, pixels + stride, stride * 2, w, h >> 1);

  bottom_end_pos = gst_byte_writer_get_pos (b);

  /* If the encoded size of the top+bottom field data blocks is even,
   * add a stuffing byte */
  if (((bottom_end_pos - top_start_pos) & 1) == 0) {
    gst_byte_writer_put_uint8 (b, 0);
    end_pos = gst_byte_writer_get_pos (b);
  } else {
    end_pos = bottom_end_pos;
  }

  /* Re-write the size fields */
  gst_byte_writer_set_pos (b, seg_size_pos);
  if (end_pos - (seg_size_pos + 2) > G_MAXUINT16)
    return FALSE;               /* Data too big */
  gst_byte_writer_put_uint16_be (b, end_pos - (seg_size_pos + 2));

  if (bottom_start_pos - top_start_pos > G_MAXUINT16)
    return FALSE;               /* Data too big */
  if (bottom_end_pos - bottom_start_pos > G_MAXUINT16)
    return FALSE;               /* Data too big */

  gst_byte_writer_set_pos (b, pixel_fields_size_pos);
  gst_byte_writer_put_uint16_be (b, bottom_start_pos - top_start_pos);
  gst_byte_writer_put_uint16_be (b, bottom_end_pos - bottom_start_pos);
  gst_byte_writer_set_pos (b, end_pos);

  GST_LOG ("Object seg size %u top_size %u bottom_size %u",
      end_pos - (seg_size_pos + 2), bottom_start_pos - top_start_pos,
      end_pos - bottom_start_pos);

  return TRUE;
}

static void
dvbenc_write_clut (GstByteWriter * b, int object_version, int page_id,
    int clut_id, SubpictureRect * s)
{
  guint8 *palette;
  int clut_entry_flag;
  guint seg_size_pos, pos;
  int i;

  if (s->nb_colours <= 4)
    clut_entry_flag = 4;
  else if (s->nb_colours <= 16)
    clut_entry_flag = 2;
  else
    clut_entry_flag = 1;
  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_TYPE_CLUT_DEFINITION);
  gst_byte_writer_put_uint16_be (b, page_id);
  seg_size_pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_put_uint16_be (b, 0);
  gst_byte_writer_put_uint8 (b, clut_id);
  /* version number, reserved bits */
  gst_byte_writer_put_uint8 (b, (object_version << 4) | 0x0F);

  palette = (guint8 *) (s->frame->data[1]);
  for (i = 0; i < s->nb_colours; i++) {

    gst_byte_writer_put_uint8 (b, i);
    /* clut_entry_flag | 4-bits reserved | full_range_flag = 1 */
    gst_byte_writer_put_uint8 (b, clut_entry_flag << 5 | 0x1F);
    /* Write YVUT value, where T (transparency) = 255 - A, Palette is AYUV */
    gst_byte_writer_put_uint8 (b, palette[1]);  /* Y */
    gst_byte_writer_put_uint8 (b, palette[3]);  /* V */
    gst_byte_writer_put_uint8 (b, palette[2]);  /* U */
    gst_byte_writer_put_uint8 (b, 255 - palette[0]);    /* A */

#if defined (HACK_2BIT)
    palette += 4 * 64;          /* HACK to generate 4-colour palette */
#elif defined (HACK_4BIT)
    palette += 4 * 16;          /* HACK to generate 16-colour palette */
#else
    palette += 4;
#endif
  }

  /* Re-write the size field */
  pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_set_pos (b, seg_size_pos);
  gst_byte_writer_put_uint16_be (b, pos - (seg_size_pos + 2));
  gst_byte_writer_set_pos (b, pos);
}

static void
dvbenc_write_region_segment (GstByteWriter * b, int object_version, int page_id,
    int region_id, SubpictureRect * s)
{
  int region_depth;
  guint seg_size_pos, pos;
  gint w = GST_VIDEO_INFO_WIDTH (&s->frame->info);
  gint h = GST_VIDEO_INFO_HEIGHT (&s->frame->info);

  if (s->nb_colours <= 4)
    region_depth = 1;
  else if (s->nb_colours <= 16)
    region_depth = 2;
  else
    region_depth = 3;

  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_TYPE_REGION_COMPOSITION);
  gst_byte_writer_put_uint16_be (b, page_id);

  /* Size placeholder */
  seg_size_pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_put_uint16_be (b, 0);

  gst_byte_writer_put_uint8 (b, region_id);
  /* version number, fill flag, reserved bits */
  gst_byte_writer_put_uint8 (b, (object_version << 4) | (0 << 3) | 0x07);
  gst_byte_writer_put_uint16_be (b, w);
  gst_byte_writer_put_uint16_be (b, h);
  /* level_of_compatibility and depth */
  gst_byte_writer_put_uint8 (b, region_depth << 5 | region_depth << 2 | 0x03);
  /* CLUT id */
  gst_byte_writer_put_uint8 (b, region_id);
  /* Dummy flags for the fill colours */
  gst_byte_writer_put_uint16_be (b, 0x0003);

  /* Object ID = region_id = CLUT id */
  gst_byte_writer_put_uint16_be (b, region_id);
  /* object type = 0, x,y corner = 0 */
  gst_byte_writer_put_uint16_be (b, 0x0000);
  gst_byte_writer_put_uint16_be (b, 0xf000);

  /* Re-write the size field */
  pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_set_pos (b, seg_size_pos);
  gst_byte_writer_put_uint16_be (b, pos - (seg_size_pos + 2));
  gst_byte_writer_set_pos (b, pos);
}

static void
dvbenc_write_display_definition_segment (GstByteWriter * b, int object_version,
    int page_id, guint16 width, guint16 height)
{
  guint seg_size_pos, pos;

  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (b, DVB_SEGMENT_TYPE_DISPLAY_DEFINITION);
  gst_byte_writer_put_uint16_be (b, page_id);

  /* Size placeholder */
  seg_size_pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_put_uint16_be (b, 0);

  /* version number, display window flag, reserved bits */
  gst_byte_writer_put_uint8 (b, (object_version << 4) | (0 << 3) | 0x07);
  gst_byte_writer_put_uint16_be (b, width);
  gst_byte_writer_put_uint16_be (b, height);

  /* Re-write the size field */
  pos = gst_byte_writer_get_pos (b);
  gst_byte_writer_set_pos (b, seg_size_pos);
  gst_byte_writer_put_uint16_be (b, pos - (seg_size_pos + 2));
  gst_byte_writer_set_pos (b, pos);
}

GstBuffer *
gst_dvbenc_encode (int object_version, int page_id, int display_version,
    guint16 width, guint16 height, SubpictureRect * s, guint num_subpictures)
{
  GstByteWriter b;
  guint seg_size_pos, pos;
  guint i;

#ifdef HACK_2BIT
  /* HACK: Only output 4 colours (results may be garbage, but tests
   * the encoding */
  s->nb_colours = 4;
#elif defined (HACK_4BIT)
  /* HACK: Only output 16 colours */
  s->nb_colours = 16;
#endif

  gst_byte_writer_init (&b);

  /* GStreamer passes DVB subpictures as private PES packets with
   * 0x20 0x00 prefixed */
  gst_byte_writer_put_uint16_be (&b, 0x2000);

  /* If non-default width/height are used, write a display definiton segment */
  if (width != 720 || height != 576)
    dvbenc_write_display_definition_segment (&b, display_version, page_id,
        width, height);

  /* Page Composition Segment */
  gst_byte_writer_put_uint8 (&b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (&b, DVB_SEGMENT_TYPE_PAGE_COMPOSITION);
  gst_byte_writer_put_uint16_be (&b, page_id);
  seg_size_pos = gst_byte_writer_get_pos (&b);
  gst_byte_writer_put_uint16_be (&b, 0);
  gst_byte_writer_put_uint8 (&b, 30);

  /* We always write complete overlay subregions, so use page_state = 2 (mode change) */
  gst_byte_writer_put_uint8 (&b, (object_version << 4) | (2 << 2) | 0x3);

  for (i = 0; i < num_subpictures; i++) {
    gst_byte_writer_put_uint8 (&b, i);
    gst_byte_writer_put_uint8 (&b, 0xFF);
    gst_byte_writer_put_uint16_be (&b, s[i].x);
    gst_byte_writer_put_uint16_be (&b, s[i].y);
  }

  /* Rewrite the size field */
  pos = gst_byte_writer_get_pos (&b);
  gst_byte_writer_set_pos (&b, seg_size_pos);
  gst_byte_writer_put_uint16_be (&b, pos - (seg_size_pos + 2));
  gst_byte_writer_set_pos (&b, pos);

  /* Region Composition */
  for (i = 0; i < num_subpictures; i++) {
    dvbenc_write_region_segment (&b, object_version, page_id, i, s + i);
  }
  /* CLUT definitions */
  for (i = 0; i < num_subpictures; i++) {
    dvbenc_write_clut (&b, object_version, page_id, i, s + i);
  }
  /* object data */
  for (i = 0; i < num_subpictures; i++) {
    /* FIXME: Any object data could potentially overflow the 64K field
     * size, in which case we should split it */
    if (!dvbenc_write_object_data (&b, object_version, page_id, i, s + i)) {
      GST_WARNING ("Object data was too big to encode");
      goto fail;
    }
  }
  /* End of Display Set segment */
  gst_byte_writer_put_uint8 (&b, DVB_SEGMENT_SYNC_BYTE);
  gst_byte_writer_put_uint8 (&b, DVB_SEGMENT_TYPE_END_OF_DISPLAY);
  gst_byte_writer_put_uint16_be (&b, page_id);
  gst_byte_writer_put_uint16_be (&b, 0);

  /* End of PES data marker */
  gst_byte_writer_put_uint8 (&b, 0xFF);

  return gst_byte_writer_reset_and_get_buffer (&b);

fail:
  gst_byte_writer_reset (&b);
  return NULL;
}
