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
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

/**
 * SECTION:gstvideoscaler
 * @title: GstVideoScaler
 * @short_description: Utility object for rescaling video frames
 *
 * #GstVideoScaler is a utility object for rescaling and resampling
 * video frames using various interpolation / sampling methods.
 *
 */

#ifndef DISABLE_ORC
#include <orc/orcfunctions.h>
#else
#define orc_memcpy memcpy
#endif

#include "video-orc.h"
#include "video-scaler.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-scaler", 0,
        "video-scaler object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}

#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

#define SCALE_U8          12
#define SCALE_U8_ROUND    (1 << (SCALE_U8 -1))
#define SCALE_U8_LQ       6
#define SCALE_U8_LQ_ROUND (1 << (SCALE_U8_LQ -1))
#define SCALE_U16         12
#define SCALE_U16_ROUND   (1 << (SCALE_U16 -1))

#define LQ

typedef void (*GstVideoScalerHFunc) (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems);
typedef void (*GstVideoScalerVFunc) (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems);

struct _GstVideoScaler
{
  GstVideoResamplerMethod method;
  GstVideoScalerFlags flags;

  GstVideoResampler resampler;

  gboolean merged;
  gint in_y_offset;
  gint out_y_offset;

  /* cached integer coefficients */
  gint16 *taps_s16;
  gint16 *taps_s16_4;
  guint32 *offset_n;
  /* for ORC */
  gint inc;

  gint tmpwidth;
  gpointer tmpline1;
  gpointer tmpline2;
};

static void
resampler_zip (GstVideoResampler * resampler, const GstVideoResampler * r1,
    const GstVideoResampler * r2)
{
  guint i, out_size, max_taps, n_phases;
  gdouble *taps;
  guint32 *offset, *phase;

  g_return_if_fail (r1->max_taps == r2->max_taps);

  out_size = r1->out_size + r2->out_size;
  max_taps = r1->max_taps;
  n_phases = out_size;
  offset = g_malloc (sizeof (guint32) * out_size);
  phase = g_malloc (sizeof (guint32) * n_phases);
  taps = g_malloc (sizeof (gdouble) * max_taps * n_phases);

  resampler->in_size = r1->in_size + r2->in_size;
  resampler->out_size = out_size;
  resampler->max_taps = max_taps;
  resampler->n_phases = n_phases;
  resampler->offset = offset;
  resampler->phase = phase;
  resampler->n_taps = g_malloc (sizeof (guint32) * out_size);
  resampler->taps = taps;

  for (i = 0; i < out_size; i++) {
    guint idx = i / 2;
    const GstVideoResampler *r;

    r = (i & 1) ? r2 : r1;

    offset[i] = r->offset[idx] * 2 + (i & 1);
    phase[i] = i;

    memcpy (taps + i * max_taps, r->taps + r->phase[idx] * max_taps,
        max_taps * sizeof (gdouble));
  }
}

static void
realloc_tmplines (GstVideoScaler * scale, gint n_elems, gint width)
{
  gint n_taps = scale->resampler.max_taps;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    n_taps *= 2;

  scale->tmpline1 =
      g_realloc (scale->tmpline1, sizeof (gint32) * width * n_elems * n_taps);
  scale->tmpline2 =
      g_realloc (scale->tmpline2, sizeof (gint32) * width * n_elems);
  scale->tmpwidth = width;
}

static void
scaler_dump (GstVideoScaler * scale)
{
#if 0
  gint i, j, in_size, out_size, max_taps;
  guint32 *offset, *phase;
  gdouble *taps;
  GstVideoResampler *r = &scale->resampler;

  in_size = r->in_size;
  out_size = r->out_size;
  offset = r->offset;
  phase = r->phase;
  max_taps = r->max_taps;
  taps = r->taps;

  g_print ("in %d, out %d, max_taps %d, n_phases %d\n", in_size, out_size,
      max_taps, r->n_phases);

  for (i = 0; i < out_size; i++) {
    g_print ("%d: \t%d \t%d:", i, offset[i], phase[i]);

    for (j = 0; j < max_taps; j++) {
      g_print ("\t%f", taps[i * max_taps + j]);
    }
    g_print ("\n");
  }
#endif
}

#define INTERLACE_SHIFT 0.5

/**
 * gst_video_scaler_new: (skip)
 * @method: a #GstVideoResamplerMethod
 * @flags: #GstVideoScalerFlags
 * @n_taps: number of taps to use
 * @in_size: number of source elements
 * @out_size: number of destination elements
 * @options: (allow-none): extra options
 *
 * Make a new @method video scaler. @in_size source lines/pixels will
 * be scaled to @out_size destination lines/pixels.
 *
 * @n_taps specifies the amount of pixels to use from the source for one output
 * pixel. If n_taps is 0, this function chooses a good value automatically based
 * on the @method and @in_size/@out_size.
 *
 * Returns: a #GstVideoScaler
 */
GstVideoScaler *
gst_video_scaler_new (GstVideoResamplerMethod method, GstVideoScalerFlags flags,
    guint n_taps, guint in_size, guint out_size, GstStructure * options)
{
  GstVideoScaler *scale;

  g_return_val_if_fail (in_size != 0, NULL);
  g_return_val_if_fail (out_size != 0, NULL);

  scale = g_new0 (GstVideoScaler, 1);

  GST_DEBUG ("%d %u  %u->%u", method, n_taps, in_size, out_size);

  scale->method = method;
  scale->flags = flags;

  if (flags & GST_VIDEO_SCALER_FLAG_INTERLACED) {
    GstVideoResampler tresamp, bresamp;
    gdouble shift;

    shift = (INTERLACE_SHIFT * out_size) / in_size;

    gst_video_resampler_init (&tresamp, method,
        GST_VIDEO_RESAMPLER_FLAG_HALF_TAPS, (out_size + 1) / 2, n_taps, shift,
        (in_size + 1) / 2, (out_size + 1) / 2, options);

    n_taps = tresamp.max_taps;

    gst_video_resampler_init (&bresamp, method, 0, out_size - tresamp.out_size,
        n_taps, -shift, in_size - tresamp.in_size,
        out_size - tresamp.out_size, options);

    resampler_zip (&scale->resampler, &tresamp, &bresamp);
    gst_video_resampler_clear (&tresamp);
    gst_video_resampler_clear (&bresamp);
  } else {
    gst_video_resampler_init (&scale->resampler, method,
        GST_VIDEO_RESAMPLER_FLAG_NONE, out_size, n_taps, 0.0, in_size, out_size,
        options);
  }

  if (out_size == 1)
    scale->inc = 0;
  else
    scale->inc = ((in_size - 1) << 16) / (out_size - 1) - 1;

  scaler_dump (scale);
  GST_DEBUG ("max_taps %d", scale->resampler.max_taps);

  return scale;
}

/**
 * gst_video_scaler_free:
 * @scale: a #GstVideoScaler
 *
 * Free a previously allocated #GstVideoScaler @scale.
 */
void
gst_video_scaler_free (GstVideoScaler * scale)
{
  g_return_if_fail (scale != NULL);

  gst_video_resampler_clear (&scale->resampler);
  g_free (scale->taps_s16);
  g_free (scale->taps_s16_4);
  g_free (scale->offset_n);
  g_free (scale->tmpline1);
  g_free (scale->tmpline2);
  g_free (scale);
}

/**
 * gst_video_scaler_get_max_taps:
 * @scale: a #GstVideoScaler
 *
 * Get the maximum number of taps for @scale.
 *
 * Returns: the maximum number of taps
 */
guint
gst_video_scaler_get_max_taps (GstVideoScaler * scale)
{
  g_return_val_if_fail (scale != NULL, 0);

  return scale->resampler.max_taps;
}

/**
 * gst_video_scaler_get_coeff:
 * @scale: a #GstVideoScaler
 * @out_offset: an output offset
 * @in_offset: (out) (optional): result input offset
 * @n_taps: (out) (optional): result n_taps
 *
 * For a given pixel at @out_offset, get the first required input pixel at
 * @in_offset and the @n_taps filter coefficients.
 *
 * Note that for interlaced content, @in_offset needs to be incremented with
 * 2 to get the next input line.
 *
 * Returns: an array of @n_tap gdouble values with filter coefficients.
 */
const gdouble *
gst_video_scaler_get_coeff (GstVideoScaler * scale,
    guint out_offset, guint * in_offset, guint * n_taps)
{
  guint offset, phase;

  g_return_val_if_fail (scale != NULL, NULL);
  g_return_val_if_fail (out_offset < scale->resampler.out_size, NULL);

  offset = scale->resampler.offset[out_offset];
  phase = scale->resampler.phase[out_offset];

  if (in_offset)
    *in_offset = offset;
  if (n_taps) {
    *n_taps = scale->resampler.max_taps;
    if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
      *n_taps *= 2;
  }
  return scale->resampler.taps + phase * scale->resampler.max_taps;
}

static gboolean
resampler_convert_coeff (const gdouble * src,
    gpointer dest, guint n, guint bits, guint precision)
{
  gdouble multiplier;
  gint i, j;
  gdouble offset, l_offset, h_offset;
  gboolean exact = FALSE;

  multiplier = (1 << precision);

  /* Round to integer, but with an adjustable bias that we use to
   * eliminate the DC error. */
  l_offset = 0.0;
  h_offset = 1.0;
  offset = 0.5;

  for (i = 0; i < 64; i++) {
    gint sum = 0;

    for (j = 0; j < n; j++) {
      gint16 tap = floor (offset + src[j] * multiplier);

      ((gint16 *) dest)[j] = tap;

      sum += tap;
    }
    if (sum == (1 << precision)) {
      exact = TRUE;
      break;
    }

    if (l_offset == h_offset)
      break;

    if (sum < (1 << precision)) {
      if (offset > l_offset)
        l_offset = offset;
      offset += (h_offset - l_offset) / 2;
    } else {
      if (offset < h_offset)
        h_offset = offset;
      offset -= (h_offset - l_offset) / 2;
    }
  }

  if (!exact)
    GST_DEBUG ("can't find exact taps");

  return exact;
}

static void
make_s16_taps (GstVideoScaler * scale, gint n_elems, gint precision)
{
  gint i, j, max_taps, n_phases, out_size, src_inc;
  gint16 *taps_s16, *taps_s16_4;
  gdouble *taps;
  guint32 *phase, *offset, *offset_n;

  n_phases = scale->resampler.n_phases;
  max_taps = scale->resampler.max_taps;

  taps = scale->resampler.taps;
  taps_s16 = scale->taps_s16 = g_malloc (sizeof (gint16) * n_phases * max_taps);

  for (i = 0; i < n_phases; i++) {
    resampler_convert_coeff (taps, taps_s16, max_taps, 16, precision);

    taps += max_taps;
    taps_s16 += max_taps;
  }

  out_size = scale->resampler.out_size;

  taps_s16 = scale->taps_s16;
  phase = scale->resampler.phase;
  offset = scale->resampler.offset;

  taps_s16_4 = scale->taps_s16_4 =
      g_malloc (sizeof (gint16) * out_size * max_taps * 4);
  offset_n = scale->offset_n =
      g_malloc (sizeof (guint32) * out_size * max_taps);

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  for (j = 0; j < max_taps; j++) {
    for (i = 0; i < out_size; i++) {
      gint16 tap;

      if (scale->merged) {
        if ((i & 1) == scale->out_y_offset)
          offset_n[j * out_size + i] = offset[i] + (2 * j);
        else
          offset_n[j * out_size + i] = offset[i] + (4 * j);
      } else {
        offset_n[j * out_size + i] = offset[i] + j * src_inc;
      }
      tap = taps_s16[phase[i] * max_taps + j];
      taps_s16_4[(j * out_size + i) * n_elems + 0] = tap;
      if (n_elems > 1)
        taps_s16_4[(j * out_size + i) * n_elems + 1] = tap;
      if (n_elems > 2)
        taps_s16_4[(j * out_size + i) * n_elems + 2] = tap;
      if (n_elems > 3)
        taps_s16_4[(j * out_size + i) * n_elems + 3] = tap;
    }
  }
}

#undef ACC_SCALE

static void
video_scale_h_near_u8 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint8 *s, *d;
  gint i;

  d = (guint8 *) dest + dest_offset;
  s = (guint8 *) src;

  {
#ifndef ACC_SCALE
    guint32 *offset = scale->resampler.offset + dest_offset;

    for (i = 0; i < width; i++)
      d[i] = s[offset[i]];
#else
    gint acc = 0;

    for (i = 0; i < width; i++) {
      gint j = (acc + 0x8000) >> 16;
      d[i] = s[j];
      acc += scale->inc;
    }
#endif
  }
}

static void
video_scale_h_near_3u8 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint8 *s, *d;
  gint i;

  d = (guint8 *) dest + dest_offset;
  s = (guint8 *) src;

  {
#ifndef ACC_SCALE
    guint32 *offset = scale->resampler.offset + dest_offset;

    for (i = 0; i < width; i++) {
      gint j = offset[i] * 3;

      d[i * 3 + 0] = s[j + 0];
      d[i * 3 + 1] = s[j + 1];
      d[i * 3 + 2] = s[j + 2];
    }
#else
    gint acc = 0;

    for (i = 0; i < width; i++) {
      gint j = ((acc + 0x8000) >> 16) * 3;

      d[i * 3 + 0] = s[j + 0];
      d[i * 3 + 1] = s[j + 1];
      d[i * 3 + 2] = s[j + 2];
      acc += scale->inc;
    }
#endif
  }
}

static void
video_scale_h_near_u16 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint16 *s, *d;
  gint i;

  d = (guint16 *) dest + dest_offset;
  s = (guint16 *) src;

  {
#ifndef ACC_SCALE
    guint32 *offset = scale->resampler.offset + dest_offset;

    for (i = 0; i < width; i++)
      d[i] = s[offset[i]];
#else
    gint acc = 0;

    for (i = 0; i < width; i++) {
      gint j = (acc + 0x8000) >> 16;
      d[i] = s[j];
      acc += scale->inc;
    }
#endif
  }
}

static void
video_scale_h_near_u32 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint32 *s, *d;

  d = (guint32 *) dest + dest_offset;
  s = (guint32 *) src;

#if 0
  /* ORC is slower on this */
  video_orc_resample_h_near_u32_lq (d, s, 0, scale->inc, width);
#elif 0
  video_orc_resample_h_near_u32 (d, s, offset, width);
#else
  {
    gint i;
#ifndef ACC_SCALE
    guint32 *offset = scale->resampler.offset + dest_offset;

    for (i = 0; i < width; i++)
      d[i] = s[offset[i]];
#else
    gint acc = 0;

    for (i = 0; i < width; i++) {
      gint j = (acc + 0x8000) >> 16;
      d[i] = s[j];
      acc += scale->inc;
    }
#endif
  }
#endif
}

static void
video_scale_h_near_u64 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint64 *s, *d;
  gint i;
  guint32 *offset;

  d = (guint64 *) dest + dest_offset;
  s = (guint64 *) src;

  offset = scale->resampler.offset + dest_offset;
  for (i = 0; i < width; i++)
    d[i] = s[offset[i]];
}

static void
video_scale_h_2tap_1u8 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint8 *s, *d;

  d = (guint8 *) dest + dest_offset;
  s = (guint8 *) src;

  video_orc_resample_h_2tap_1u8_lq (d, s, 0, scale->inc, width);
}

static void
video_scale_h_2tap_4u8 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  guint32 *s, *d;

  d = (guint32 *) dest + dest_offset;
  s = (guint32 *) src;

  video_orc_resample_h_2tap_4u8_lq (d, s, 0, scale->inc, width);
}

static void
video_scale_h_ntap_u8 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  gint16 *taps;
  gint i, max_taps, count;
  gpointer d;
  guint32 *offset_n;
  guint8 *pixels;
  gint16 *temp;

  if (scale->taps_s16 == NULL)
#ifdef LQ
    make_s16_taps (scale, n_elems, SCALE_U8_LQ);
#else
    make_s16_taps (scale, n_elems, SCALE_U8);
#endif

  max_taps = scale->resampler.max_taps;
  offset_n = scale->offset_n;

  pixels = (guint8 *) scale->tmpline1;

  /* prepare the arrays */
  count = width * max_taps;
  switch (n_elems) {
    case 1:
    {
      guint8 *s = (guint8 *) src;

      for (i = 0; i < count; i++)
        pixels[i] = s[offset_n[i]];

      d = (guint8 *) dest + dest_offset;
      break;
    }
    case 2:
    {
      guint16 *p16 = (guint16 *) pixels;
      guint16 *s = (guint16 *) src;

      for (i = 0; i < count; i++)
        p16[i] = s[offset_n[i]];

      d = (guint16 *) dest + dest_offset;
      break;
    }
    case 3:
    {
      guint8 *s = (guint8 *) src;

      for (i = 0; i < count; i++) {
        gint j = offset_n[i] * 3;
        pixels[i * 3 + 0] = s[j + 0];
        pixels[i * 3 + 1] = s[j + 1];
        pixels[i * 3 + 2] = s[j + 2];
      }
      d = (guint8 *) dest + dest_offset * 3;
      break;
    }
    case 4:
    {
      guint32 *p32 = (guint32 *) pixels;
      guint32 *s = (guint32 *) src;
#if 0
      video_orc_resample_h_near_u32 (p32, s, offset_n, count);
#else
      for (i = 0; i < count; i++)
        p32[i] = s[offset_n[i]];
#endif
      d = (guint32 *) dest + dest_offset;
      break;
    }
    default:
      return;
  }
  temp = (gint16 *) scale->tmpline2;
  taps = scale->taps_s16_4;
  count = width * n_elems;

#ifdef LQ
  if (max_taps == 2) {
    video_orc_resample_h_2tap_u8_lq (d, pixels, pixels + count, taps,
        taps + count, count);
  } else {
    /* first pixels with first tap to temp */
    if (max_taps >= 3) {
      video_orc_resample_h_multaps3_u8_lq (temp, pixels, pixels + count,
          pixels + count * 2, taps, taps + count, taps + count * 2, count);
      max_taps -= 3;
      pixels += count * 3;
      taps += count * 3;
    } else {
      gint first = max_taps % 3;

      video_orc_resample_h_multaps_u8_lq (temp, pixels, taps, count);
      video_orc_resample_h_muladdtaps_u8_lq (temp, 0, pixels + count, count,
          taps + count, count * 2, count, first - 1);
      max_taps -= first;
      pixels += count * first;
      taps += count * first;
    }
    while (max_taps > 3) {
      if (max_taps >= 6) {
        video_orc_resample_h_muladdtaps3_u8_lq (temp, pixels, pixels + count,
            pixels + count * 2, taps, taps + count, taps + count * 2, count);
        max_taps -= 3;
        pixels += count * 3;
        taps += count * 3;
      } else {
        video_orc_resample_h_muladdtaps_u8_lq (temp, 0, pixels, count,
            taps, count * 2, count, max_taps - 3);
        pixels += count * (max_taps - 3);
        taps += count * (max_taps - 3);
        max_taps = 3;
      }
    }
    if (max_taps == 3) {
      video_orc_resample_h_muladdscaletaps3_u8_lq (d, pixels, pixels + count,
          pixels + count * 2, taps, taps + count, taps + count * 2, temp,
          count);
    } else {
      if (max_taps) {
        /* add other pixels with other taps to t4 */
        video_orc_resample_h_muladdtaps_u8_lq (temp, 0, pixels, count,
            taps, count * 2, count, max_taps);
      }
      /* scale and write final result */
      video_orc_resample_scaletaps_u8_lq (d, temp, count);
    }
  }
#else
  /* first pixels with first tap to t4 */
  video_orc_resample_h_multaps_u8 (temp, pixels, taps, count);
  /* add other pixels with other taps to t4 */
  video_orc_resample_h_muladdtaps_u8 (temp, 0, pixels + count, count,
      taps + count, count * 2, count, max_taps - 1);
  /* scale and write final result */
  video_orc_resample_scaletaps_u8 (d, temp, count);
#endif
}

static void
video_scale_h_ntap_u16 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width, guint n_elems)
{
  gint16 *taps;
  gint i, max_taps, count;
  gpointer d;
  guint32 *offset_n;
  guint16 *pixels;
  gint32 *temp;

  if (scale->taps_s16 == NULL)
    make_s16_taps (scale, n_elems, SCALE_U16);

  max_taps = scale->resampler.max_taps;
  offset_n = scale->offset_n;

  pixels = (guint16 *) scale->tmpline1;
  /* prepare the arrays FIXME, we can add this into ORC */
  count = width * max_taps;
  switch (n_elems) {
    case 1:
    {
      guint16 *s = (guint16 *) src;

      for (i = 0; i < count; i++)
        pixels[i] = s[offset_n[i]];

      d = (guint16 *) dest + dest_offset;
      break;
    }
    case 4:
    {
      guint64 *p64 = (guint64 *) pixels;
      guint64 *s = (guint64 *) src;
#if 0
      video_orc_resample_h_near_u32 (p32, s, offset_n, count);
#else
      for (i = 0; i < count; i++)
        p64[i] = s[offset_n[i]];
#endif
      d = (guint64 *) dest + dest_offset;
      break;
    }
    default:
      return;
  }

  temp = (gint32 *) scale->tmpline2;
  taps = scale->taps_s16_4;
  count = width * n_elems;

  if (max_taps == 2) {
    video_orc_resample_h_2tap_u16 (d, pixels, pixels + count, taps,
        taps + count, count);
  } else {
    /* first pixels with first tap to t4 */
    video_orc_resample_h_multaps_u16 (temp, pixels, taps, count);
    /* add other pixels with other taps to t4 */
    video_orc_resample_h_muladdtaps_u16 (temp, 0, pixels + count, count * 2,
        taps + count, count * 2, count, max_taps - 1);
    /* scale and write final result */
    video_orc_resample_scaletaps_u16 (d, temp, count);
  }
}

static void
video_scale_v_near_u8 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  if (dest != srcs[0])
    memcpy (dest, srcs[0], n_elems * width);
}

static void
video_scale_v_near_u16 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  if (dest != srcs[0])
    memcpy (dest, srcs[0], n_elems * 2 * width);
}

static void
video_scale_v_2tap_u8 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  gint max_taps, src_inc;
  guint8 *s1, *s2, *d;
  gint16 p1;

  if (scale->taps_s16 == NULL)
#ifdef LQ
    make_s16_taps (scale, n_elems, SCALE_U8_LQ + 2);
#else
    make_s16_taps (scale, n_elems, SCALE_U8);
#endif

  max_taps = scale->resampler.max_taps;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  d = (guint8 *) dest;
  s1 = (guint8 *) srcs[0 * src_inc];
  s2 = (guint8 *) srcs[1 * src_inc];
  p1 = scale->taps_s16[dest_offset * max_taps + 1];

#ifdef LQ
  video_orc_resample_v_2tap_u8_lq (d, s1, s2, p1, width * n_elems);
#else
  video_orc_resample_v_2tap_u8 (d, s1, s2, p1, width * n_elems);
#endif
}

static void
video_scale_v_2tap_u16 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  gint max_taps, src_inc;
  guint16 *s1, *s2, *d;
  gint16 p1;

  if (scale->taps_s16 == NULL)
    make_s16_taps (scale, n_elems, SCALE_U16);

  max_taps = scale->resampler.max_taps;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  d = (guint16 *) dest;
  s1 = (guint16 *) srcs[0 * src_inc];
  s2 = (guint16 *) srcs[1 * src_inc];
  p1 = scale->taps_s16[dest_offset * max_taps + 1];

  video_orc_resample_v_2tap_u16 (d, s1, s2, p1, width * n_elems);
}

#if 0
static void
video_scale_h_4tap_8888 (GstVideoScaler * scale,
    gpointer src, gpointer dest, guint dest_offset, guint width)
{
  gint16 *taps;
  gint i, max_taps, count;
  guint8 *d;
  guint32 *offset_n;
  guint32 *pixels;

  if (scale->taps_s16 == NULL)
    make_s16_taps (scale, n_elems, S16_SCALE);

  max_taps = scale->resampler.max_taps;
  offset_n = scale->offset_n;

  d = (guint8 *) dest + 4 * dest_offset;

  /* prepare the arrays FIXME, we can add this into ORC */
  count = width * max_taps;
  pixels = (guint32 *) scale->tmpline1;
  for (i = 0; i < count; i++)
    pixels[i] = ((guint32 *) src)[offset_n[i]];

  taps = scale->taps_s16_4;
  count = width * 4;

  video_orc_resample_h_4tap_8 (d, pixels, pixels + width, pixels + 2 * width,
      pixels + 3 * width, taps, taps + count, taps + 2 * count,
      taps + 3 * count, count);
}
#endif

static void
video_scale_v_4tap_u8 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  gint max_taps;
  guint8 *s1, *s2, *s3, *s4, *d;
  gint p1, p2, p3, p4, src_inc;
  gint16 *taps;

  if (scale->taps_s16 == NULL)
#ifdef LQ
    make_s16_taps (scale, n_elems, SCALE_U8_LQ);
#else
    make_s16_taps (scale, n_elems, SCALE_U8);
#endif

  max_taps = scale->resampler.max_taps;
  taps = scale->taps_s16 + dest_offset * max_taps;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  d = (guint8 *) dest;
  s1 = (guint8 *) srcs[0 * src_inc];
  s2 = (guint8 *) srcs[1 * src_inc];
  s3 = (guint8 *) srcs[2 * src_inc];
  s4 = (guint8 *) srcs[3 * src_inc];
  p1 = taps[0];
  p2 = taps[1];
  p3 = taps[2];
  p4 = taps[3];

#ifdef LQ
  video_orc_resample_v_4tap_u8_lq (d, s1, s2, s3, s4, p1, p2, p3, p4,
      width * n_elems);
#else
  video_orc_resample_v_4tap_u8 (d, s1, s2, s3, s4, p1, p2, p3, p4,
      width * n_elems);
#endif
}

static void
video_scale_v_ntap_u8 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  gint16 *taps;
  gint i, max_taps, count, src_inc;
  gpointer d;
  gint16 *temp;

  if (scale->taps_s16 == NULL)
#ifdef LQ
    make_s16_taps (scale, n_elems, SCALE_U8_LQ);
#else
    make_s16_taps (scale, n_elems, SCALE_U8);
#endif

  max_taps = scale->resampler.max_taps;
  taps = scale->taps_s16 + (scale->resampler.phase[dest_offset] * max_taps);

  d = (guint32 *) dest;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  temp = (gint16 *) scale->tmpline2;
  count = width * n_elems;

#ifdef LQ
  if (max_taps >= 4) {
    video_orc_resample_v_multaps4_u8_lq (temp, srcs[0], srcs[1 * src_inc],
        srcs[2 * src_inc], srcs[3 * src_inc], taps[0], taps[1], taps[2],
        taps[3], count);
    max_taps -= 4;
    srcs += 4 * src_inc;
    taps += 4;
  } else {
    gint first = (max_taps % 4);

    video_orc_resample_v_multaps_u8_lq (temp, srcs[0], taps[0], count);
    for (i = 1; i < first; i++) {
      video_orc_resample_v_muladdtaps_u8_lq (temp, srcs[i * src_inc], taps[i],
          count);
    }
    max_taps -= first;
    srcs += first * src_inc;
    taps += first;
  }
  while (max_taps > 4) {
    if (max_taps >= 8) {
      video_orc_resample_v_muladdtaps4_u8_lq (temp, srcs[0], srcs[1 * src_inc],
          srcs[2 * src_inc], srcs[3 * src_inc], taps[0], taps[1], taps[2],
          taps[3], count);
      max_taps -= 4;
      srcs += 4 * src_inc;
      taps += 4;
    } else {
      for (i = 0; i < max_taps - 4; i++)
        video_orc_resample_v_muladdtaps_u8_lq (temp, srcs[i * src_inc], taps[i],
            count);
      srcs += (max_taps - 4) * src_inc;
      taps += (max_taps - 4);
      max_taps = 4;
    }
  }
  if (max_taps == 4) {
    video_orc_resample_v_muladdscaletaps4_u8_lq (d, srcs[0], srcs[1 * src_inc],
        srcs[2 * src_inc], srcs[3 * src_inc], temp, taps[0], taps[1], taps[2],
        taps[3], count);
  } else {
    for (i = 0; i < max_taps; i++)
      video_orc_resample_v_muladdtaps_u8_lq (temp, srcs[i * src_inc], taps[i],
          count);
    video_orc_resample_scaletaps_u8_lq (d, temp, count);
  }

#else
  video_orc_resample_v_multaps_u8 (temp, srcs[0], taps[0], count);
  for (i = 1; i < max_taps; i++) {
    video_orc_resample_v_muladdtaps_u8 (temp, srcs[i * src_inc], taps[i],
        count);
  }
  video_orc_resample_scaletaps_u8 (d, temp, count);
#endif
}

static void
video_scale_v_ntap_u16 (GstVideoScaler * scale,
    gpointer srcs[], gpointer dest, guint dest_offset, guint width,
    guint n_elems)
{
  gint16 *taps;
  gint i, max_taps, count, src_inc;
  gpointer d;
  gint32 *temp;

  if (scale->taps_s16 == NULL)
    make_s16_taps (scale, n_elems, SCALE_U16);

  max_taps = scale->resampler.max_taps;
  taps = scale->taps_s16 + (scale->resampler.phase[dest_offset] * max_taps);

  d = (guint16 *) dest;

  if (scale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED)
    src_inc = 2;
  else
    src_inc = 1;

  temp = (gint32 *) scale->tmpline2;
  count = width * n_elems;

  video_orc_resample_v_multaps_u16 (temp, srcs[0], taps[0], count);
  for (i = 1; i < max_taps; i++) {
    video_orc_resample_v_muladdtaps_u16 (temp, srcs[i * src_inc], taps[i],
        count);
  }
  video_orc_resample_scaletaps_u16 (d, temp, count);
}

static gint
get_y_offset (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
      return 0;
    default:
    case GST_VIDEO_FORMAT_UYVY:
      return 1;
  }
}

/**
 * gst_video_scaler_combine_packed_YUV: (skip)
 * @y_scale: a scaler for the Y component
 * @uv_scale: a scaler for the U and V components
 * @in_format: the input video format
 * @out_format: the output video format
 *
 * Combine a scaler for Y and UV into one scaler for the packed @format.
 *
 * Returns: a new horizontal videoscaler for @format.
 *
 * Since: 1.6
 */
GstVideoScaler *
gst_video_scaler_combine_packed_YUV (GstVideoScaler * y_scale,
    GstVideoScaler * uv_scale, GstVideoFormat in_format,
    GstVideoFormat out_format)
{
  GstVideoScaler *scale;
  GstVideoResampler *resampler;
  guint i, out_size, max_taps, n_phases;
  gdouble *taps;
  guint32 *offset, *phase;

  g_return_val_if_fail (y_scale != NULL, NULL);
  g_return_val_if_fail (uv_scale != NULL, NULL);
  g_return_val_if_fail (uv_scale->resampler.max_taps ==
      y_scale->resampler.max_taps, NULL);

  scale = g_new0 (GstVideoScaler, 1);

  scale->method = y_scale->method;
  scale->flags = y_scale->flags;
  scale->merged = TRUE;

  resampler = &scale->resampler;

  out_size = GST_ROUND_UP_4 (y_scale->resampler.out_size * 2);
  max_taps = y_scale->resampler.max_taps;
  n_phases = out_size;
  offset = g_malloc (sizeof (guint32) * out_size);
  phase = g_malloc (sizeof (guint32) * n_phases);
  taps = g_malloc (sizeof (gdouble) * max_taps * n_phases);

  resampler->in_size = y_scale->resampler.in_size * 2;
  resampler->out_size = out_size;
  resampler->max_taps = max_taps;
  resampler->n_phases = n_phases;
  resampler->offset = offset;
  resampler->phase = phase;
  resampler->n_taps = g_malloc (sizeof (guint32) * out_size);
  resampler->taps = taps;

  scale->in_y_offset = get_y_offset (in_format);
  scale->out_y_offset = get_y_offset (out_format);
  scale->inc = y_scale->inc;

  for (i = 0; i < out_size; i++) {
    gint ic;

    if ((i & 1) == scale->out_y_offset) {
      ic = MIN (i / 2, y_scale->resampler.out_size - 1);
      offset[i] = y_scale->resampler.offset[ic] * 2 + scale->in_y_offset;
      memcpy (taps + i * max_taps, y_scale->resampler.taps +
          y_scale->resampler.phase[ic] * max_taps, max_taps * sizeof (gdouble));
    } else {
      ic = MIN (i / 4, uv_scale->resampler.out_size - 1);
      offset[i] = uv_scale->resampler.offset[ic] * 4 + (i & 3);
      memcpy (taps + i * max_taps, uv_scale->resampler.taps +
          uv_scale->resampler.phase[ic] * max_taps,
          max_taps * sizeof (gdouble));
    }
    phase[i] = i;
  }

  scaler_dump (scale);

  return scale;
}

static gboolean
get_functions (GstVideoScaler * hscale, GstVideoScaler * vscale,
    GstVideoFormat format,
    GstVideoScalerHFunc * hfunc, GstVideoScalerVFunc * vfunc,
    gint * n_elems, guint * width, gint * bits)
{
  gboolean mono = FALSE;

  switch (format) {
    case GST_VIDEO_FORMAT_GRAY8:
      *bits = 8;
      *n_elems = 1;
      mono = TRUE;
      break;
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
      *bits = 8;
      *n_elems = 1;
      *width = GST_ROUND_UP_4 (*width * 2);
      break;
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_v308:
    case GST_VIDEO_FORMAT_IYU2:
      *bits = 8;
      *n_elems = 3;
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
      *bits = 8;
      *n_elems = 4;
      break;
    case GST_VIDEO_FORMAT_ARGB64:
    case GST_VIDEO_FORMAT_ARGB64_LE:
    case GST_VIDEO_FORMAT_ARGB64_BE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_BGRA64_BE:
    case GST_VIDEO_FORMAT_BGRA64_LE:
    case GST_VIDEO_FORMAT_ABGR64_BE:
    case GST_VIDEO_FORMAT_ABGR64_LE:
    case GST_VIDEO_FORMAT_AYUV64:
      *bits = 16;
      *n_elems = 4;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
      *bits = 16;
      *n_elems = 1;
      mono = TRUE;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_NV24:
    case GST_VIDEO_FORMAT_NV61:
      *bits = 8;
      *n_elems = 2;
      break;
    default:
      return FALSE;
  }
  if (*bits == 8) {
    switch (hscale ? hscale->resampler.max_taps : 0) {
      case 0:
        break;
      case 1:
        if (*n_elems == 1)
          *hfunc = video_scale_h_near_u8;
        else if (*n_elems == 2)
          *hfunc = video_scale_h_near_u16;
        else if (*n_elems == 3)
          *hfunc = video_scale_h_near_3u8;
        else if (*n_elems == 4)
          *hfunc = video_scale_h_near_u32;
        break;
      case 2:
        if (*n_elems == 1 && mono)
          *hfunc = video_scale_h_2tap_1u8;
        else if (*n_elems == 4)
          *hfunc = video_scale_h_2tap_4u8;
        else
          *hfunc = video_scale_h_ntap_u8;
        break;
      default:
        *hfunc = video_scale_h_ntap_u8;
        break;
    }
    switch (vscale ? vscale->resampler.max_taps : 0) {
      case 0:
        break;
      case 1:
        *vfunc = video_scale_v_near_u8;
        break;
      case 2:
        *vfunc = video_scale_v_2tap_u8;
        break;
      case 4:
        *vfunc = video_scale_v_4tap_u8;
        break;
      default:
        *vfunc = video_scale_v_ntap_u8;
        break;
    }
  } else if (*bits == 16) {
    switch (hscale ? hscale->resampler.max_taps : 0) {
      case 0:
        break;
      case 1:
        if (*n_elems == 1)
          *hfunc = video_scale_h_near_u16;
        else
          *hfunc = video_scale_h_near_u64;
        break;
      default:
        *hfunc = video_scale_h_ntap_u16;
        break;
    }
    switch (vscale ? vscale->resampler.max_taps : 0) {
      case 0:
        break;
      case 1:
        *vfunc = video_scale_v_near_u16;
        break;
      case 2:
        *vfunc = video_scale_v_2tap_u16;
        break;
      default:
        *vfunc = video_scale_v_ntap_u16;
        break;
    }
  }
  return TRUE;
}

/**
 * gst_video_scaler_horizontal:
 * @scale: a #GstVideoScaler
 * @format: a #GstVideoFormat for @src and @dest
 * @src: source pixels
 * @dest: destination pixels
 * @dest_offset: the horizontal destination offset
 * @width: the number of pixels to scale
 *
 * Horizontally scale the pixels in @src to @dest, starting from @dest_offset
 * for @width samples.
 */
void
gst_video_scaler_horizontal (GstVideoScaler * scale, GstVideoFormat format,
    gpointer src, gpointer dest, guint dest_offset, guint width)
{
  gint n_elems, bits;
  GstVideoScalerHFunc func = NULL;

  g_return_if_fail (scale != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest_offset + width <= scale->resampler.out_size);

  if (!get_functions (scale, NULL, format, &func, NULL, &n_elems, &width, &bits)
      || func == NULL)
    goto no_func;

  if (scale->tmpwidth < width)
    realloc_tmplines (scale, n_elems, width);

  func (scale, src, dest, dest_offset, width, n_elems);
  return;

no_func:
  {
    GST_WARNING ("no scaler function for format");
  }
}

/**
 * gst_video_scaler_vertical:
 * @scale: a #GstVideoScaler
 * @format: a #GstVideoFormat for @srcs and @dest
 * @src_lines: source pixels lines
 * @dest: destination pixels
 * @dest_offset: the vertical destination offset
 * @width: the number of pixels to scale
 *
 * Vertically combine @width pixels in the lines in @src_lines to @dest.
 * @dest is the location of the target line at @dest_offset and
 * @srcs are the input lines for @dest_offset.
 */
void
gst_video_scaler_vertical (GstVideoScaler * scale, GstVideoFormat format,
    gpointer src_lines[], gpointer dest, guint dest_offset, guint width)
{
  gint n_elems, bits;
  GstVideoScalerVFunc func = NULL;

  g_return_if_fail (scale != NULL);
  g_return_if_fail (src_lines != NULL);
  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest_offset < scale->resampler.out_size);

  if (!get_functions (NULL, scale, format, NULL, &func, &n_elems, &width, &bits)
      || func == NULL)
    goto no_func;

  if (scale->tmpwidth < width)
    realloc_tmplines (scale, n_elems, width);

  func (scale, src_lines, dest, dest_offset, width, n_elems);

  return;

no_func:
  {
    GST_WARNING ("no scaler function for format");
  }
}


/**
 * gst_video_scaler_2d:
 * @hscale: a horizontal #GstVideoScaler
 * @vscale: a vertical #GstVideoScaler
 * @format: a #GstVideoFormat for @srcs and @dest
 * @src: source pixels
 * @src_stride: source pixels stride
 * @dest: destination pixels
 * @dest_stride: destination pixels stride
 * @x: the horizontal destination offset
 * @y: the vertical destination offset
 * @width: the number of output pixels to scale
 * @height: the number of output lines to scale
 *
 * Scale a rectangle of pixels in @src with @src_stride to @dest with
 * @dest_stride using the horizontal scaler @hscaler and the vertical
 * scaler @vscale.
 *
 * One or both of @hscale and @vscale can be NULL to only perform scaling in
 * one dimension or do a copy without scaling.
 *
 * @x and @y are the coordinates in the destination image to process.
 */
void
gst_video_scaler_2d (GstVideoScaler * hscale, GstVideoScaler * vscale,
    GstVideoFormat format, gpointer src, gint src_stride,
    gpointer dest, gint dest_stride, guint x, guint y,
    guint width, guint height)
{
  gint n_elems, bits;
  GstVideoScalerHFunc hfunc = NULL;
  GstVideoScalerVFunc vfunc = NULL;
  gint i;
  gboolean interlaced;

  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  if (!get_functions (hscale, vscale, format, &hfunc, &vfunc, &n_elems, &width,
          &bits))
    goto no_func;

  interlaced = vscale && !!(vscale->flags & GST_VIDEO_SCALER_FLAG_INTERLACED);

#define LINE(s,ss,i)  ((guint8 *)(s) + ((i) * (ss)))
#define TMP_LINE(s,i) ((guint8 *)((s)->tmpline1) + (i) * (sizeof (gint32) * width * n_elems))

  if (vscale == NULL) {
    if (hscale == NULL) {
      guint xo, xw;
      guint8 *s, *d;

      xo = x * n_elems;
      xw = width * n_elems * (bits / 8);

      s = LINE (src, src_stride, y) + xo;
      d = LINE (dest, dest_stride, y) + xo;

      /* no scaling, do memcpy */
      for (i = y; i < height; i++) {
        memcpy (d, s, xw);
        d += dest_stride;
        s += src_stride;
      }
    } else {
      if (hscale->tmpwidth < width)
        realloc_tmplines (hscale, n_elems, width);

      /* only horizontal scaling */
      for (i = y; i < height; i++) {
        hfunc (hscale, LINE (src, src_stride, i), LINE (dest, dest_stride, i),
            x, width, n_elems);
      }
    }
  } else {
    guint v_taps;
    gpointer *lines;

    if (vscale->tmpwidth < width)
      realloc_tmplines (vscale, n_elems, width);

    v_taps = vscale->resampler.max_taps;

    lines = g_alloca ((interlaced ? 2 : 1) * v_taps * sizeof (gpointer));
    memset (lines, 0, (interlaced ? 2 : 1) * v_taps * sizeof (gpointer));

    if (hscale == NULL) {
      guint src_inc = interlaced ? 2 : 1;

      /* only vertical scaling */
      for (i = y; i < height; i++) {
        guint in, j;

        in = vscale->resampler.offset[i];
        for (j = 0; j < v_taps; j++) {
          guint l = in + j * src_inc;

          g_assert (l < vscale->resampler.in_size);
          lines[j * src_inc] = LINE (src, src_stride, l);
        }

        vfunc (vscale, lines, LINE (dest, dest_stride, i), i, width, n_elems);
      }
    } else {
      gint s1, s2;
      guint *tmpline_lines;

      tmpline_lines = g_newa (guint, (interlaced ? 2 : 1) * v_taps);
      /* initialize with -1 */
      memset (tmpline_lines, 0xff,
          (interlaced ? 2 : 1) * v_taps * sizeof (guint));

      if (hscale->tmpwidth < width)
        realloc_tmplines (hscale, n_elems, width);

      s1 = width * vscale->resampler.offset[height - 1];
      s2 = width * height;

      if (s1 <= s2) {
        for (i = y; i < height; i++) {
          guint in, j;
          guint src_inc = interlaced ? 2 : 1;
          guint f2_offset = (interlaced && (i % 2 == 1)) * v_taps;

          in = vscale->resampler.offset[i];
          for (j = 0; j < v_taps; j++) {
            guint k;
            guint l = in + j * src_inc;

            g_assert (l < vscale->resampler.in_size);

            /* First check if we already have this line in tmplines */
            for (k = f2_offset; k < v_taps + f2_offset; k++) {
              if (tmpline_lines[k] == l) {
                lines[j * src_inc] = TMP_LINE (vscale, k);
                break;
              }
            }
            /* Found */
            if (k < v_taps + f2_offset)
              continue;

            /* Otherwise find an empty line we can clear */
            for (k = f2_offset; k < v_taps + f2_offset; k++) {
              if (tmpline_lines[k] < in || tmpline_lines[k] == -1)
                break;
            }

            /* Must not happen, that would mean we don't have enough space to
             * begin with */
            g_assert (k < v_taps + f2_offset);

            hfunc (hscale, LINE (src, src_stride, l), TMP_LINE (vscale, k), x,
                width, n_elems);
            tmpline_lines[k] = l;
            lines[j * src_inc] = TMP_LINE (vscale, k);
          }

          vfunc (vscale, lines, LINE (dest, dest_stride, i), i, width, n_elems);
        }
      } else {
        guint vx, vw, w1, ws;
        guint h_taps;

        h_taps = hscale->resampler.max_taps;
        w1 = x + width - 1;
        ws = hscale->resampler.offset[w1];

        /* we need to estimate the area that we first need to scale in the
         * vertical direction. Scale x and width to find the lower bound and
         * overshoot the width to find the upper bound */
        vx = (hscale->inc * x) >> 16;
        vx = MIN (vx, hscale->resampler.offset[x]);
        vw = (hscale->inc * (x + width)) >> 16;
        if (hscale->merged) {
          if ((w1 & 1) == hscale->out_y_offset)
            vw = MAX (vw, ws + (2 * h_taps));
          else
            vw = MAX (vw, ws + (4 * h_taps));
        } else {
          vw = MAX (vw, ws + h_taps);
        }
        vw += 1;
        /* but clamp to max size */
        vw = MIN (vw, hscale->resampler.in_size);

        if (vscale->tmpwidth < vw)
          realloc_tmplines (vscale, n_elems, vw);

        for (i = y; i < height; i++) {
          guint in, j;
          guint src_inc = interlaced ? 2 : 1;

          in = vscale->resampler.offset[i];
          for (j = 0; j < v_taps; j++) {
            guint l = in + j * src_inc;

            g_assert (l < vscale->resampler.in_size);
            lines[j * src_inc] =
                LINE (src, src_stride, in + j * src_inc) + vx * n_elems;
          }

          vfunc (vscale, lines, TMP_LINE (vscale, 0) + vx * n_elems, i,
              vw - vx, n_elems);

          hfunc (hscale, TMP_LINE (vscale, 0), LINE (dest, dest_stride,
                  i), x, width, n_elems);
        }
      }
    }
  }
  return;

no_func:
  {
    GST_WARNING ("no scaler function for format");
  }
}

#undef LINE
#undef TMP_LINE
