/* GStreamer
 * Copyright (C) <2014> Wim Taymans <wim.taymans@gmail.com>
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

#include <string.h>

#include "video-dither.h"
#include "video-orc.h"

/**
 * SECTION:gstvideodither
 * @title: GstVideoDither
 * @short_description: Utility object for dithering and quantizing lines of video
 *
 * GstVideoDither provides implementations of several dithering algorithms
 * that can be applied to lines of video pixels to quantize and dither them.
 *
 */
struct _GstVideoDither
{
  GstVideoDitherMethod method;
  GstVideoDitherFlags flags;
  GstVideoFormat format;
  guint width;

  guint depth;
  guint n_comp;

  void (*func) (GstVideoDither * dither, gpointer pixels, guint x, guint y,
      guint width);
  guint8 shift[4];
  guint16 mask[4];
  guint64 orc_mask64;
  guint32 orc_mask32;

  gpointer errors;
};

static void
dither_none_u8_mask (GstVideoDither * dither, gpointer pixels, guint x, guint y,
    guint width)
{
  guint8 *p = pixels;

  video_orc_dither_none_4u8_mask (p + (x * 4), dither->orc_mask32, width);
}

static void
dither_none_u16_mask (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint16 *p = pixels;

  video_orc_dither_none_4u16_mask (p + (x * 4), dither->orc_mask64, width);
}

static void
dither_verterr_u8 (GstVideoDither * dither, gpointer pixels, guint x, guint y,
    guint width)
{
  guint8 *p = pixels;
  guint16 *e = dither->errors;

  if (y == 0)
    memset (e + (x * 4), 0, width * 8);

  video_orc_dither_verterr_4u8_mask (p + (x * 4), e + (x * 4),
      dither->orc_mask64, width);
}

static void
dither_verterr_u16 (GstVideoDither * dither, gpointer pixels, guint x, guint y,
    guint width)
{
  guint16 *p = pixels;
  guint16 *e = dither->errors;

  if (y == 0)
    memset (e + (x * 4), 0, width * 8);

  {
    gint i, end;
    guint16 *m = dither->mask;
    guint32 v, mp;

    end = (width + x) * 4;
    for (i = x * 4; i < end; i++) {
      mp = m[i & 3];
      v = p[i] + e[i];
      /* take new error and store */
      e[i] = v & mp;
      /* quantize and store */
      v &= ~mp;
      p[i] = MIN (v, 65535);
    }
  }
}

static void
dither_floyd_steinberg_u8 (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint8 *p = pixels;
  guint16 *e = dither->errors;

  if (y == 0)
    memset (e + (x * 4), 0, (width + 1) * 8);

  /* add and multiply errors from previous line */
  video_orc_dither_fs_muladd_u8 (e + x * 4, width * 4);
#if 1
  {
    gint i, end;
    guint16 *m = dither->mask, mp;
    guint16 v;

    end = (width + x) * 4;

    for (i = x * 4; i < end; i++) {
      mp = m[i & 3];
      v = p[i] + ((7 * e[i] + e[i + 4]) >> 4);
      /* take new error and store */
      e[i + 4] = v & mp;
      /* quantize and store */
      v &= ~mp;
      p[i] = MIN (v, 255);
    }
  }
#else
  video_orc_dither_fs_add_4u8 (p, e + x * 4, e + (x + 1) * 4,
      dither->orc_mask64, width);
#endif
}

static void
dither_floyd_steinberg_u16 (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint16 *p = pixels;
  guint16 *e = dither->errors;

  if (y == 0)
    memset (e + (x * 4), 0, (width + 1) * 8);

  {
    gint i, end;
    guint16 *m = dither->mask, mp;
    guint32 v;

    end = (width + x) * 4;
    for (i = x * 4; i < end; i++) {
      mp = m[i & 3];
      /* apply previous errors to pixel */
      v = p[i] + ((7 * e[i] + e[i + 4] + 5 * e[i + 8] + 3 * e[i + 12]) >> 4);
      /* take new error and store */
      e[i + 4] = v & mp;
      /* quantize and store */
      v &= ~mp;
      p[i] = MIN (v, 65535);
    }
  }
}

static void
dither_sierra_lite_u8 (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint8 *p = pixels;
  guint16 *e = dither->errors;
  gint i, end;
  guint16 *m = dither->mask, mp;
  guint16 v;

  if (y == 0)
    memset (e + (x * 4), 0, (width + 4) * 8);

  end = (width + x) * 4;
  for (i = x; i < end; i++) {
    mp = m[i & 3];
    /* apply previous errors to pixel */
    v = p[i] + ((2 * e[i] + e[i + 8] + e[i + 12]) >> 2);
    /* store new error */
    e[i + 4] = v & mp;
    /* quantize and store */
    v &= ~mp;
    p[i] = MIN (v, 255);
  }
}

static void
dither_sierra_lite_u16 (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint16 *p = pixels;
  guint16 *e = dither->errors;
  gint i, end;
  guint16 *m = dither->mask, mp;
  guint32 v;

  if (y == 0)
    memset (e + (x * 4), 0, (width + 4) * 8);

  end = (width + x) * 4;
  for (i = x; i < end; i++) {
    mp = m[i & 3];
    /* apply previous errors to pixel */
    v = p[i] + ((2 * e[i] + e[i + 8] + e[i + 12]) >> 2);
    /* store new error */
    e[i + 4] = v & mp;
    /* quantize and store */
    v &= ~mp;
    p[i] = MIN (v & ~mp, 65535);
  }
}

static const guint16 bayer_map[16][16] = {
  {0, 128, 32, 160, 8, 136, 40, 168, 2, 130, 34, 162, 10, 138, 42, 170},
  {192, 64, 224, 96, 200, 72, 232, 104, 194, 66, 226, 98, 202, 74, 234, 106},
  {48, 176, 16, 144, 56, 184, 24, 152, 50, 178, 18, 146, 58, 186, 26, 154},
  {240, 112, 208, 80, 248, 120, 216, 88, 242, 114, 210, 82, 250, 122, 218, 90},
  {12, 240, 44, 172, 4, 132, 36, 164, 14, 242, 46, 174, 6, 134, 38, 166},
  {204, 76, 236, 108, 196, 68, 228, 100, 206, 78, 238, 110, 198, 70, 230, 102},
  {60, 188, 28, 156, 52, 180, 20, 148, 62, 190, 30, 158, 54, 182, 22, 150},
  {252, 142, 220, 92, 244, 116, 212, 84, 254, 144, 222, 94, 246, 118, 214, 86},
  {3, 131, 35, 163, 11, 139, 43, 171, 1, 129, 33, 161, 9, 137, 41, 169},
  {195, 67, 227, 99, 203, 75, 235, 107, 193, 65, 225, 97, 201, 73, 233, 105},
  {51, 179, 19, 147, 59, 187, 27, 155, 49, 177, 17, 145, 57, 185, 25, 153},
  {243, 115, 211, 83, 251, 123, 219, 91, 241, 113, 209, 81, 249, 121, 217, 89},
  {15, 243, 47, 175, 7, 135, 39, 167, 13, 241, 45, 173, 5, 133, 37, 165},
  {207, 79, 239, 111, 199, 71, 231, 103, 205, 77, 237, 109, 197, 69, 229, 101},
  {63, 191, 31, 159, 55, 183, 23, 151, 61, 189, 29, 157, 53, 181, 21, 149},
  {255, 145, 223, 95, 247, 119, 215, 87, 253, 143, 221, 93, 245, 117, 213, 85}
};

static void
dither_ordered_u8 (GstVideoDither * dither, gpointer pixels, guint x, guint y,
    guint width)
{
  guint8 *p = pixels;
  guint8 *c = (guint8 *) dither->errors + ((y & 15) * width + (x & 15)) * 4;

  video_orc_dither_ordered_u8 (p, c, width * 4);
}

static void
dither_ordered_u8_mask (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint8 *p = pixels;
  guint16 *c = (guint16 *) dither->errors + ((y & 15) * width + (x & 15)) * 4;

  video_orc_dither_ordered_4u8_mask (p, c, dither->orc_mask64, width);
}

static void
dither_ordered_u16_mask (GstVideoDither * dither, gpointer pixels, guint x,
    guint y, guint width)
{
  guint16 *p = pixels;
  guint16 *c = (guint16 *) dither->errors + ((y & 15) * width + (x & 15)) * 4;

  video_orc_dither_ordered_4u16_mask (p, c, dither->orc_mask64, width);
}

static void
alloc_errors (GstVideoDither * dither, guint lines)
{
  guint width, n_comp;

  width = dither->width;
  n_comp = dither->n_comp;

  dither->errors = g_malloc0 (sizeof (guint16) * (width + 8) * n_comp * lines);
}

static void
setup_bayer (GstVideoDither * dither)
{
  guint i, j, k, width, n_comp, errdepth;
  guint8 *shift;

  width = dither->width;
  shift = dither->shift;
  n_comp = dither->n_comp;

  if (dither->depth == 8) {
    if (dither->flags & GST_VIDEO_DITHER_FLAG_QUANTIZE) {
      dither->func = dither_ordered_u8_mask;
      errdepth = 16;
    } else {
      dither->func = dither_ordered_u8;
      errdepth = 8;
    }
  } else {
    dither->func = dither_ordered_u16_mask;
    errdepth = 16;
  }

  alloc_errors (dither, 16);

  if (errdepth == 8) {
    for (i = 0; i < 16; i++) {
      guint8 *p = (guint8 *) dither->errors + (n_comp * width * i), v;
      for (j = 0; j < width; j++) {
        for (k = 0; k < n_comp; k++) {
          v = bayer_map[i & 15][j & 15];
          if (shift[k] < 8)
            v = v >> (8 - shift[k]);
          p[n_comp * j + k] = v;
        }
      }
    }
  } else {
    for (i = 0; i < 16; i++) {
      guint16 *p = (guint16 *) dither->errors + (n_comp * width * i), v;
      for (j = 0; j < width; j++) {
        for (k = 0; k < n_comp; k++) {
          v = bayer_map[i & 15][j & 15];
          if (shift[k] < 8)
            v = v >> (8 - shift[k]);
          p[n_comp * j + k] = v;
        }
      }
    }
  }
}

static gint
count_power (guint v)
{
  gint res = 0;
  while (v > 1) {
    res++;
    v >>= 1;
  }
  return res;
}

/**
 * gst_video_dither_new: (skip)
 * @method: a #GstVideoDitherMethod
 * @flags: a #GstVideoDitherFlags
 * @format: a #GstVideoFormat
 * @quantizer: quantizer
 * @width: the width of the lines
 *
 * Make a new dither object for dithering lines of @format using the
 * algorithm described by @method.
 *
 * Each component will be quantized to a multiple of @quantizer. Better
 * performance is achieved when @quantizer is a power of 2.
 *
 * @width is the width of the lines that this ditherer will handle.
 *
 * Returns: a new #GstVideoDither
 */
GstVideoDither *
gst_video_dither_new (GstVideoDitherMethod method, GstVideoDitherFlags flags,
    GstVideoFormat format, guint quantizer[GST_VIDEO_MAX_COMPONENTS],
    guint width)
{
  GstVideoDither *dither;
  gint i;

  dither = g_new0 (GstVideoDither, 1);
  dither->method = method;
  dither->flags = flags;
  dither->format = format;
  dither->width = width;

  dither->n_comp = 4;

  switch (format) {
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_ARGB:
      dither->depth = 8;
      break;
    case GST_VIDEO_FORMAT_AYUV64:
    case GST_VIDEO_FORMAT_ARGB64:
      dither->depth = 16;
      break;
    default:
      g_free (dither);
      g_return_val_if_reached (NULL);
      break;
  }

  for (i = 0; i < 4; i++) {
    /* FIXME, only power of 2 quantizers */
    guint q = quantizer[(i + 3) & 3];

    dither->shift[i] = count_power (q);
    dither->mask[i] = (1 << dither->shift[i]) - 1;
    GST_DEBUG ("%d: quant %d shift %d mask %08x", i, q, dither->shift[i],
        dither->mask[i]);
    dither->orc_mask64 =
        (dither->orc_mask64 << 16) | GUINT16_FROM_BE (dither->mask[i]);
    dither->orc_mask32 = (dither->orc_mask32 << 8) | (guint8) dither->mask[i];
  }
  dither->orc_mask64 = GUINT64_FROM_BE (dither->orc_mask64);
  dither->orc_mask32 = GUINT32_FROM_BE (dither->orc_mask32);
  GST_DEBUG ("mask64 %08" G_GINT64_MODIFIER "x", (guint64) dither->orc_mask64);
  GST_DEBUG ("mask32 %08x", dither->orc_mask32);

  switch (method) {
    case GST_VIDEO_DITHER_NONE:
      if (dither->flags & GST_VIDEO_DITHER_FLAG_QUANTIZE)
        if (dither->depth == 8)
          dither->func = dither_none_u8_mask;
        else
          dither->func = dither_none_u16_mask;
      else
        dither->func = NULL;
      break;
    case GST_VIDEO_DITHER_VERTERR:
      alloc_errors (dither, 1);
      if (dither->depth == 8) {
        dither->func = dither_verterr_u8;
      } else
        dither->func = dither_verterr_u16;
      break;
    case GST_VIDEO_DITHER_FLOYD_STEINBERG:
      alloc_errors (dither, 1);
      if (dither->depth == 8) {
        dither->func = dither_floyd_steinberg_u8;
      } else
        dither->func = dither_floyd_steinberg_u16;
      break;
    case GST_VIDEO_DITHER_SIERRA_LITE:
      alloc_errors (dither, 1);
      if (dither->depth == 8) {
        dither->func = dither_sierra_lite_u8;
      } else
        dither->func = dither_sierra_lite_u16;
      break;
    case GST_VIDEO_DITHER_BAYER:
      setup_bayer (dither);
      break;
  }
  return dither;
}

/**
 * gst_video_dither_free:
 * @dither: a #GstVideoDither
 *
 * Free @dither
 */
void
gst_video_dither_free (GstVideoDither * dither)
{
  g_return_if_fail (dither != NULL);

  g_free (dither->errors);
  g_free (dither);
}

/**
 * gst_video_dither_line:
 * @dither: a #GstVideoDither
 * @line: pointer to the pixels of the line
 * @x: x coordinate
 * @y: y coordinate
 * @width: the width
 *
 * Dither @width pixels starting from offset @x in @line using @dither.
 *
 * @y is the line number of @line in the output image.
 */
void
gst_video_dither_line (GstVideoDither * dither, gpointer line, guint x, guint y,
    guint width)
{
  g_return_if_fail (dither != NULL);
  g_return_if_fail (x + width <= dither->width);

  if (dither->func)
    dither->func (dither, line, x, y, width);
}
