/* GStreamer
 * Copyright (C) <2015> Wim Taymans <wim.taymans@gmail.com>
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

#ifdef HAVE_ORC
#include <orc/orc.h>
#endif

#include "audio-resampler.h"
#include "audio-resampler-private.h"
#include "audio-resampler-macros.h"

#define MEM_ALIGN(m,a) ((gint8 *)((guintptr)((gint8 *)(m) + ((a)-1)) & ~((a)-1)))
#define ALIGN 16
#define TAPS_OVERREAD 16

GST_DEBUG_CATEGORY_STATIC (audio_resampler_debug);
#define GST_CAT_DEFAULT audio_resampler_debug

/**
 * SECTION:gstaudioresampler
 * @title: GstAudioResampler
 * @short_description: Utility structure for resampler information
 *
 * #GstAudioResampler is a structure which holds the information
 * required to perform various kinds of resampling filtering.
 *
 */

static const gint oversample_qualities[] = {
  4, 4, 4, 8, 8, 16, 16, 16, 16, 32, 32
};

typedef struct
{
  gdouble cutoff;
  gdouble downsample_cutoff_factor;
  gdouble stopband_attenuation;
  gdouble transition_bandwidth;
} KaiserQualityMap;

static const KaiserQualityMap kaiser_qualities[] = {
  {0.860, 0.96511, 60, 0.7},    /* 8 taps */
  {0.880, 0.96591, 65, 0.29},   /* 16 taps */
  {0.910, 0.96923, 70, 0.145},  /* 32 taps */
  {0.920, 0.97600, 80, 0.105},  /* 48 taps */
  {0.940, 0.97979, 85, 0.087},  /* 64 taps default quality */
  {0.940, 0.98085, 95, 0.077},  /* 80 taps */
  {0.945, 0.99471, 100, 0.068}, /* 96 taps */
  {0.950, 1.0, 105, 0.055},     /* 128 taps */
  {0.960, 1.0, 110, 0.045},     /* 160 taps */
  {0.968, 1.0, 115, 0.039},     /* 192 taps */
  {0.975, 1.0, 120, 0.0305}     /* 256 taps */
};

typedef struct
{
  gint n_taps;
  gdouble cutoff;
} BlackmanQualityMap;

static const BlackmanQualityMap blackman_qualities[] = {
  {8, 0.5,},
  {16, 0.6,},
  {24, 0.72,},
  {32, 0.8,},
  {48, 0.85,},                  /* default */
  {64, 0.90,},
  {80, 0.92,},
  {96, 0.933,},
  {128, 0.950,},
  {148, 0.955,},
  {160, 0.960,}
};

#define DEFAULT_RESAMPLER_METHOD GST_AUDIO_RESAMPLER_METHOD_KAISER
#define DEFAULT_QUALITY GST_AUDIO_RESAMPLER_QUALITY_DEFAULT
#define DEFAULT_OPT_CUBIC_B 1.0
#define DEFAULT_OPT_CUBIC_C 0.0
#define DEFAULT_OPT_FILTER_MODE GST_AUDIO_RESAMPLER_FILTER_MODE_AUTO
#define DEFAULT_OPT_FILTER_MODE_THRESHOLD 1048576
#define DEFAULT_OPT_FILTER_INTERPOLATION GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_CUBIC
#define DEFAULT_OPT_FILTER_OVERSAMPLE 8
#define DEFAULT_OPT_MAX_PHASE_ERROR 0.1

static gdouble
get_opt_double (GstStructure * options, const gchar * name, gdouble def)
{
  gdouble res;
  if (!options || !gst_structure_get_double (options, name, &res))
    res = def;
  return res;
}

static gint
get_opt_int (GstStructure * options, const gchar * name, gint def)
{
  gint res;
  if (!options || !gst_structure_get_int (options, name, &res))
    res = def;
  return res;
}

static gint
get_opt_enum (GstStructure * options, const gchar * name, GType type, gint def)
{
  gint res;
  if (!options || !gst_structure_get_enum (options, name, type, &res))
    res = def;
  return res;
}


#define GET_OPT_CUTOFF(options,def) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_CUTOFF,def)
#define GET_OPT_DOWN_CUTOFF_FACTOR(options,def) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_DOWN_CUTOFF_FACTOR, def)
#define GET_OPT_STOP_ATTENUATION(options,def) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_STOP_ATTENUATION, def)
#define GET_OPT_TRANSITION_BANDWIDTH(options,def) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_TRANSITION_BANDWIDTH, def)
#define GET_OPT_CUBIC_B(options) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_CUBIC_B, DEFAULT_OPT_CUBIC_B)
#define GET_OPT_CUBIC_C(options) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_CUBIC_C, DEFAULT_OPT_CUBIC_C)
#define GET_OPT_N_TAPS(options,def) get_opt_int(options, \
    GST_AUDIO_RESAMPLER_OPT_N_TAPS, def)
#define GET_OPT_FILTER_MODE(options) get_opt_enum(options, \
    GST_AUDIO_RESAMPLER_OPT_FILTER_MODE, GST_TYPE_AUDIO_RESAMPLER_FILTER_MODE, \
    DEFAULT_OPT_FILTER_MODE)
#define GET_OPT_FILTER_MODE_THRESHOLD(options) get_opt_int(options, \
    GST_AUDIO_RESAMPLER_OPT_FILTER_MODE_THRESHOLD, DEFAULT_OPT_FILTER_MODE_THRESHOLD)
#define GET_OPT_FILTER_INTERPOLATION(options) get_opt_enum(options, \
    GST_AUDIO_RESAMPLER_OPT_FILTER_INTERPOLATION, GST_TYPE_AUDIO_RESAMPLER_FILTER_INTERPOLATION, \
    DEFAULT_OPT_FILTER_INTERPOLATION)
#define GET_OPT_FILTER_OVERSAMPLE(options) get_opt_int(options, \
    GST_AUDIO_RESAMPLER_OPT_FILTER_OVERSAMPLE, DEFAULT_OPT_FILTER_OVERSAMPLE)
#define GET_OPT_MAX_PHASE_ERROR(options) get_opt_double(options, \
    GST_AUDIO_RESAMPLER_OPT_MAX_PHASE_ERROR, DEFAULT_OPT_MAX_PHASE_ERROR)

#include "dbesi0.c"
#define bessel dbesi0

static inline gdouble
get_linear_tap (gdouble x, gint n_taps)
{
  gdouble res = GST_ROUND_UP_2 (n_taps) / 2 - fabs (x);
  return res;
}

static inline gdouble
get_cubic_tap (gdouble x, gint n_taps, gdouble b, gdouble c)
{
  gdouble res, a, a2, a3;

  a = fabs (x * 4.0) / n_taps;
  a2 = a * a;
  a3 = a2 * a;

  if (a <= 1.0)
    res = ((12.0 - 9.0 * b - 6.0 * c) * a3 +
        (-18.0 + 12.0 * b + 6.0 * c) * a2 + (6.0 - 2.0 * b)) / 6.0;
  else if (a <= 2.0)
    res = ((-b - 6.0 * c) * a3 +
        (6.0 * b + 30.0 * c) * a2 +
        (-12.0 * b - 48.0 * c) * a + (8.0 * b + 24.0 * c)) / 6.0;
  else
    res = 0.0;

  return res;
}

static inline gdouble
get_blackman_nuttall_tap (gdouble x, gint n_taps, gdouble Fc)
{
  gdouble s, y, w;

  y = G_PI * x;
  s = (y == 0.0 ? Fc : sin (y * Fc) / y);

  w = 2.0 * y / n_taps + G_PI;
  return s * (0.3635819 - 0.4891775 * cos (w) + 0.1365995 * cos (2 * w) -
      0.0106411 * cos (3 * w));
}

static inline gdouble
get_kaiser_tap (gdouble x, gint n_taps, gdouble Fc, gdouble beta)
{
  gdouble s, y, w;

  y = G_PI * x;
  s = (y == 0.0 ? Fc : sin (y * Fc) / y);

  w = 2.0 * x / n_taps;
  return s * bessel (beta * sqrt (MAX (1 - w * w, 0)));
}

#define MAKE_CONVERT_TAPS_INT_FUNC(type, precision)                     \
static void                                                             \
convert_taps_##type##_c (gdouble *tmp_taps, gpointer taps,              \
    gdouble weight, gint n_taps)                                        \
{                                                                       \
  gint64 one = (1LL << precision) - 1;                                  \
  type *t = taps;                                                       \
  gdouble multiplier = one;                                             \
  gint i, j;                                                            \
  gdouble offset, l_offset, h_offset;                                   \
  gboolean exact = FALSE;                                               \
  /* Round to integer, but with an adjustable bias that we use to */    \
  /* eliminate the DC error. */                                         \
  l_offset = 0.0;                                                       \
  h_offset = 1.0;                                                       \
  offset = 0.5;                                                         \
  for (i = 0; i < 32; i++) {                                            \
    gint64 sum = 0;                                                     \
    for (j = 0; j < n_taps; j++)                                        \
      sum += floor (offset + tmp_taps[j] * multiplier / weight);        \
    if (sum == one) {                                                   \
      exact = TRUE;                                                     \
      break;                                                            \
    }                                                                   \
    if (l_offset == h_offset)                                           \
      break;                                                            \
    if (sum < one) {                                                    \
      if (offset > l_offset)                                            \
        l_offset = offset;                                              \
      offset += (h_offset - l_offset) / 2;                              \
    } else {                                                            \
      if (offset < h_offset)                                            \
        h_offset = offset;                                              \
      offset -= (h_offset - l_offset) / 2;                              \
    }                                                                   \
  }                                                                     \
  for (j = 0; j < n_taps; j++)                                          \
    t[j] = floor (offset + tmp_taps[j] * multiplier / weight);          \
  if (!exact)                                                           \
    GST_DEBUG ("can't find exact taps");                                \
}

#define MAKE_CONVERT_TAPS_FLOAT_FUNC(type)                              \
static void                                                             \
convert_taps_##type##_c (gdouble *tmp_taps, gpointer taps,              \
    gdouble weight, gint n_taps)                                        \
{                                                                       \
  gint i;                                                               \
  type *t = taps;                                                       \
  for (i = 0; i < n_taps; i++)                                          \
    t[i] = tmp_taps[i] / weight;                                        \
}

MAKE_CONVERT_TAPS_INT_FUNC (gint16, PRECISION_S16);
MAKE_CONVERT_TAPS_INT_FUNC (gint32, PRECISION_S32);
MAKE_CONVERT_TAPS_FLOAT_FUNC (gfloat);
MAKE_CONVERT_TAPS_FLOAT_FUNC (gdouble);

static ConvertTapsFunc convert_taps_funcs[] = {
  convert_taps_gint16_c,
  convert_taps_gint32_c,
  convert_taps_gfloat_c,
  convert_taps_gdouble_c
};

#define convert_taps_gint16   convert_taps_funcs[0]
#define convert_taps_gint32   convert_taps_funcs[1]
#define convert_taps_gfloat   convert_taps_funcs[2]
#define convert_taps_gdouble  convert_taps_funcs[3]

static void
make_taps (GstAudioResampler * resampler, gdouble * res, gdouble x, gint n_taps)
{
  gdouble weight = 0.0, *tmp_taps = resampler->tmp_taps;
  gint i;

  switch (resampler->method) {
    case GST_AUDIO_RESAMPLER_METHOD_NEAREST:
      break;

    case GST_AUDIO_RESAMPLER_METHOD_LINEAR:
      for (i = 0; i < n_taps; i++)
        weight += tmp_taps[i] = get_linear_tap (x + i, resampler->n_taps);
      break;

    case GST_AUDIO_RESAMPLER_METHOD_CUBIC:
      for (i = 0; i < n_taps; i++)
        weight += tmp_taps[i] = get_cubic_tap (x + i, resampler->n_taps,
            resampler->b, resampler->c);
      break;

    case GST_AUDIO_RESAMPLER_METHOD_BLACKMAN_NUTTALL:
      for (i = 0; i < n_taps; i++)
        weight += tmp_taps[i] =
            get_blackman_nuttall_tap (x + i,
            resampler->n_taps, resampler->cutoff);
      break;

    case GST_AUDIO_RESAMPLER_METHOD_KAISER:
      for (i = 0; i < n_taps; i++)
        weight += tmp_taps[i] =
            get_kaiser_tap (x + i, resampler->n_taps,
            resampler->cutoff, resampler->kaiser_beta);
      break;
  }
  resampler->convert_taps (tmp_taps, res, weight, n_taps);
}

#define MAKE_COEFF_LINEAR_INT_FUNC(type,type2,prec)                     \
static inline void                                                      \
make_coeff_##type##_linear (gint num, gint denom, type *icoeff)         \
{                                                                       \
  type x = ((gint64)num << prec) / denom;                               \
  icoeff[0] = icoeff[2] = x;                                            \
  icoeff[1] = icoeff[3] = (type)(((type2)1 << prec)-1)  - x;            \
}
#define MAKE_COEFF_LINEAR_FLOAT_FUNC(type)                              \
static inline void                                                      \
make_coeff_##type##_linear (gint num, gint denom, type *icoeff)         \
{                                                                       \
  type x = (type)num / denom;                                           \
  icoeff[0] = icoeff[2] = x;                                            \
  icoeff[1] = icoeff[3] = (type)1.0 - x;                                \
}
MAKE_COEFF_LINEAR_INT_FUNC (gint16, gint32, PRECISION_S16);
MAKE_COEFF_LINEAR_INT_FUNC (gint32, gint64, PRECISION_S32);
MAKE_COEFF_LINEAR_FLOAT_FUNC (gfloat);
MAKE_COEFF_LINEAR_FLOAT_FUNC (gdouble);

#define MAKE_COEFF_CUBIC_INT_FUNC(type,type2,prec)                      \
static inline void                                                      \
make_coeff_##type##_cubic (gint num, gint denom, type *icoeff)          \
{                                                                       \
  type2 one = ((type2)1 << prec) - 1;                                   \
  type2 x = ((gint64) num << prec) / denom;                             \
  type2 x2 = (x * x) >> prec;                                           \
  type2 x3 = (x2 * x) >> prec;                                          \
  icoeff[0] = (((x3 - x) << prec) / 6) >> prec;                         \
  icoeff[1] = x + ((x2 - x3) >> 1);                                     \
  icoeff[3] = -(((x << prec) / 3) >> prec) +                            \
            (x2 >> 1) - (((x3 << prec) / 6) >> prec);                   \
  icoeff[2] = one - icoeff[0] - icoeff[1] - icoeff[3];                  \
}
#define MAKE_COEFF_CUBIC_FLOAT_FUNC(type)                               \
static inline void                                                      \
make_coeff_##type##_cubic (gint num, gint denom, type *icoeff)          \
{                                                                       \
  type x = (type) num / denom, x2 = x * x, x3 = x2 * x;                 \
  icoeff[0] = 0.16667f * (x3 - x);                                      \
  icoeff[1] = x + 0.5f * (x2 - x3);                                     \
  icoeff[3] = -0.33333f * x + 0.5f * x2 - 0.16667f * x3;                \
  icoeff[2] = (type)1.0 - icoeff[0] - icoeff[1] - icoeff[3];            \
}
MAKE_COEFF_CUBIC_INT_FUNC (gint16, gint32, PRECISION_S16);
MAKE_COEFF_CUBIC_INT_FUNC (gint32, gint64, PRECISION_S32);
MAKE_COEFF_CUBIC_FLOAT_FUNC (gfloat);
MAKE_COEFF_CUBIC_FLOAT_FUNC (gdouble);

#define INTERPOLATE_INT_LINEAR_FUNC(type,type2,prec,limit)      \
static inline void                                              \
interpolate_##type##_linear_c (gpointer op, const gpointer ap,  \
    gint len, const gpointer icp, gint astride)                 \
{                                                               \
  gint i;                                                       \
  type *o = op, *a = ap, *ic = icp;                             \
  type2 tmp, c0 = ic[0];                                        \
  const type *c[2] = {(type*)((gint8*)a + 0*astride),           \
                      (type*)((gint8*)a + 1*astride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    tmp = ((type2)c[0][i] - (type2)c[1][i]) * c0 +              \
         (((type2)c[1][i]) << (prec));                          \
    o[i] = (tmp + ((type2)1 << ((prec) - 1))) >> (prec);        \
  }                                                             \
}
#define INTERPOLATE_FLOAT_LINEAR_FUNC(type)                     \
static inline void                                              \
interpolate_##type##_linear_c (gpointer op, const gpointer ap,  \
    gint len, const gpointer icp, gint astride)                 \
{                                                               \
  gint i;                                                       \
  type *o = op, *a = ap, *ic = icp;                             \
  type c0 = ic[0];                                              \
  const type *c[2] = {(type*)((gint8*)a + 0*astride),           \
                      (type*)((gint8*)a + 1*astride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    o[i] = (c[0][i] - c[1][i]) * c0 + c[1][i];                  \
  }                                                             \
}

INTERPOLATE_INT_LINEAR_FUNC (gint16, gint32, PRECISION_S16, (gint32) 1 << 15);
INTERPOLATE_INT_LINEAR_FUNC (gint32, gint64, PRECISION_S32, (gint64) 1 << 31);
INTERPOLATE_FLOAT_LINEAR_FUNC (gfloat);
INTERPOLATE_FLOAT_LINEAR_FUNC (gdouble);

#define INTERPOLATE_INT_CUBIC_FUNC(type,type2,prec,limit)       \
static inline void                                              \
interpolate_##type##_cubic_c (gpointer op, const gpointer ap,   \
    gint len, const gpointer icp, gint astride)                 \
{                                                               \
  gint i;                                                       \
  type *o = op, *a = ap, *ic = icp;                             \
  type2 tmp, c0 = ic[0], c1 = ic[1], c2 = ic[2], c3 = ic[3];    \
  const type *c[4] = {(type*)((gint8*)a + 0*astride),           \
                      (type*)((gint8*)a + 1*astride),           \
                      (type*)((gint8*)a + 2*astride),           \
                      (type*)((gint8*)a + 3*astride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    tmp = (type2)c[0][i] * c0 + (type2)c[1][i] * c1 +           \
          (type2)c[2][i] * c2 + (type2)c[3][i] * c3;            \
    tmp = (tmp + ((type2)1 << ((prec) - 1))) >> (prec);         \
    o[i] = CLAMP (tmp, -(limit), (limit) - 1);                  \
  }                                                             \
}
#define INTERPOLATE_FLOAT_CUBIC_FUNC(type)                      \
static inline void                                              \
interpolate_##type##_cubic_c (gpointer op, const gpointer ap,   \
    gint len, const gpointer icp, gint astride)                 \
{                                                               \
  gint i;                                                       \
  type *o = op, *a = ap, *ic = icp;                             \
  type c0 = ic[0], c1 = ic[1], c2 = ic[2], c3 = ic[3];          \
  const type *c[4] = {(type*)((gint8*)a + 0*astride),           \
                      (type*)((gint8*)a + 1*astride),           \
                      (type*)((gint8*)a + 2*astride),           \
                      (type*)((gint8*)a + 3*astride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    o[i] = c[0][i] * c0 + c[1][i] * c1 +                        \
           c[2][i] * c2 + c[3][i] * c3;                         \
  }                                                             \
}

INTERPOLATE_INT_CUBIC_FUNC (gint16, gint32, PRECISION_S16, (gint32) 1 << 15);
INTERPOLATE_INT_CUBIC_FUNC (gint32, gint64, PRECISION_S32, (gint64) 1 << 31);
INTERPOLATE_FLOAT_CUBIC_FUNC (gfloat);
INTERPOLATE_FLOAT_CUBIC_FUNC (gdouble);

static InterpolateFunc interpolate_funcs[] = {
  interpolate_gint16_linear_c,
  interpolate_gint32_linear_c,
  interpolate_gfloat_linear_c,
  interpolate_gdouble_linear_c,

  interpolate_gint16_cubic_c,
  interpolate_gint32_cubic_c,
  interpolate_gfloat_cubic_c,
  interpolate_gdouble_cubic_c,
};

#define interpolate_gint16_linear  interpolate_funcs[0]
#define interpolate_gint32_linear  interpolate_funcs[1]
#define interpolate_gfloat_linear  interpolate_funcs[2]
#define interpolate_gdouble_linear interpolate_funcs[3]

#define interpolate_gint16_cubic   interpolate_funcs[4]
#define interpolate_gint32_cubic   interpolate_funcs[5]
#define interpolate_gfloat_cubic   interpolate_funcs[6]
#define interpolate_gdouble_cubic  interpolate_funcs[7]

#define GET_TAPS_NEAREST_FUNC(type)                                             \
static inline gpointer                                                          \
get_taps_##type##_nearest (GstAudioResampler * resampler,                       \
    gint *samp_index, gint *samp_phase, type icoeff[4])                         \
{                                                                               \
  gint out_rate = resampler->out_rate;                                          \
  *samp_index += resampler->samp_inc;                                           \
  *samp_phase += resampler->samp_frac;                                          \
  if (*samp_phase >= out_rate) {                                                \
    *samp_phase -= out_rate;                                                    \
    *samp_index += 1;                                                           \
  }                                                                             \
  return NULL;                                                                  \
}
GET_TAPS_NEAREST_FUNC (gint16);
GET_TAPS_NEAREST_FUNC (gint32);
GET_TAPS_NEAREST_FUNC (gfloat);
GET_TAPS_NEAREST_FUNC (gdouble);

#define get_taps_gint16_nearest get_taps_gint16_nearest
#define get_taps_gint32_nearest get_taps_gint32_nearest
#define get_taps_gfloat_nearest get_taps_gfloat_nearest
#define get_taps_gdouble_nearest get_taps_gdouble_nearest

#define GET_TAPS_FULL_FUNC(type)                                                \
DECL_GET_TAPS_FULL_FUNC(type)                                                   \
{                                                                               \
  gpointer res;                                                                 \
  gint out_rate = resampler->out_rate;                                          \
  gint n_phases = resampler->n_phases;                                          \
  gint phase = (n_phases == out_rate ? *samp_phase :                            \
      ((gint64)*samp_phase * n_phases) / out_rate);                             \
                                                                                \
  res = resampler->cached_phases[phase];                                        \
  if (G_UNLIKELY (res == NULL)) {                                               \
    res = (gint8 *) resampler->cached_taps +                                    \
                        phase * resampler->cached_taps_stride;                  \
    switch (resampler->filter_interpolation) {                                  \
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE:                       \
      {                                                                         \
        gdouble x;                                                              \
        gint n_taps = resampler->n_taps;                                        \
                                                                                \
        x = 1.0 - n_taps / 2 - (gdouble) phase / n_phases;                      \
        make_taps (resampler, res, x, n_taps);                                  \
        break;                                                                  \
      }                                                                         \
      default:                                                                  \
      {                                                                         \
        gint offset, pos, frac;                                                 \
        gint oversample = resampler->oversample;                                \
        gint taps_stride = resampler->taps_stride;                              \
        gint n_taps = resampler->n_taps;                                        \
        type ic[4], *taps;                                                      \
                                                                                \
        pos = phase * oversample;                                               \
        offset = (oversample - 1) - pos / n_phases;                             \
        frac = pos % n_phases;                                                  \
                                                                                \
        taps = (type *) ((gint8 *) resampler->taps + offset * taps_stride);     \
                                                                                \
        switch (resampler->filter_interpolation) {                              \
          default:                                                              \
          case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_LINEAR:                 \
            make_coeff_##type##_linear (frac, n_phases, ic);                    \
            break;                                                              \
          case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_CUBIC:                  \
            make_coeff_##type##_cubic (frac, n_phases, ic);                     \
            break;                                                              \
        }                                                                       \
        resampler->interpolate (res, taps, n_taps, ic, taps_stride);            \
      }                                                                         \
    }                                                                           \
    resampler->cached_phases[phase] = res;                                      \
  }                                                                             \
  *samp_index += resampler->samp_inc;                                           \
  *samp_phase += resampler->samp_frac;                                          \
  if (*samp_phase >= out_rate) {                                                \
    *samp_phase -= out_rate;                                                    \
    *samp_index += 1;                                                           \
  }                                                                             \
  return res;                                                                   \
}
GET_TAPS_FULL_FUNC (gint16);
GET_TAPS_FULL_FUNC (gint32);
GET_TAPS_FULL_FUNC (gfloat);
GET_TAPS_FULL_FUNC (gdouble);

#define GET_TAPS_INTERPOLATE_FUNC(type,inter)                   \
DECL_GET_TAPS_INTERPOLATE_FUNC (type, inter)                    \
{                                                               \
  gpointer res;                                                 \
  gint out_rate = resampler->out_rate;                          \
  gint offset, frac, pos;                                       \
  gint oversample = resampler->oversample;                      \
  gint taps_stride = resampler->taps_stride;                    \
                                                                \
  pos = *samp_phase * oversample;                               \
  offset = (oversample - 1) - pos / out_rate;                   \
  frac = pos % out_rate;                                        \
                                                                \
  res = (gint8 *) resampler->taps + offset * taps_stride;       \
  make_coeff_##type##_##inter (frac, out_rate, icoeff);         \
                                                                \
  *samp_index += resampler->samp_inc;                           \
  *samp_phase += resampler->samp_frac;                          \
  if (*samp_phase >= out_rate) {                                \
    *samp_phase -= out_rate;                                    \
    *samp_index += 1;                                           \
  }                                                             \
  return res;                                                   \
}

GET_TAPS_INTERPOLATE_FUNC (gint16, linear);
GET_TAPS_INTERPOLATE_FUNC (gint32, linear);
GET_TAPS_INTERPOLATE_FUNC (gfloat, linear);
GET_TAPS_INTERPOLATE_FUNC (gdouble, linear);

GET_TAPS_INTERPOLATE_FUNC (gint16, cubic);
GET_TAPS_INTERPOLATE_FUNC (gint32, cubic);
GET_TAPS_INTERPOLATE_FUNC (gfloat, cubic);
GET_TAPS_INTERPOLATE_FUNC (gdouble, cubic);

#define INNER_PRODUCT_NEAREST_FUNC(type)                        \
static inline void                                              \
inner_product_##type##_nearest_1_c (type * o, const type * a,   \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  *o = *a;                                                      \
}
INNER_PRODUCT_NEAREST_FUNC (gint16);
INNER_PRODUCT_NEAREST_FUNC (gint32);
INNER_PRODUCT_NEAREST_FUNC (gfloat);
INNER_PRODUCT_NEAREST_FUNC (gdouble);

#define INNER_PRODUCT_INT_FULL_FUNC(type,type2,prec,limit)      \
static inline void                                              \
inner_product_##type##_full_1_c (type * o, const type * a,      \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type2 res[4] = { 0, 0, 0, 0 };                                \
                                                                \
  for (i = 0; i < len; i += 4) {                                \
    res[0] += (type2) a[i + 0] * (type2) b[i + 0];              \
    res[1] += (type2) a[i + 1] * (type2) b[i + 1];              \
    res[2] += (type2) a[i + 2] * (type2) b[i + 2];              \
    res[3] += (type2) a[i + 3] * (type2) b[i + 3];              \
  }                                                             \
  res[0] = res[0] + res[1] + res[2] + res[3];                   \
  res[0] = (res[0] + ((type2)1 << ((prec) - 1))) >> (prec);     \
  *o = CLAMP (res[0], -(limit), (limit) - 1);                   \
}

INNER_PRODUCT_INT_FULL_FUNC (gint16, gint32, PRECISION_S16, (gint32) 1 << 15);
INNER_PRODUCT_INT_FULL_FUNC (gint32, gint64, PRECISION_S32, (gint64) 1 << 31);

#define INNER_PRODUCT_INT_LINEAR_FUNC(type,type2,prec,limit)    \
static inline void                                              \
inner_product_##type##_linear_1_c (type * o, const type * a,    \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type2 res[4] = { 0, 0, 0, 0 }, c0 = ic[0];                    \
  const type *c[2] = {(type*)((gint8*)b + 0*bstride),           \
                      (type*)((gint8*)b + 1*bstride)};          \
                                                                \
  for (i = 0; i < len; i += 2) {                                \
    res[0] += (type2) a[i + 0] * (type2) c[0][i + 0];           \
    res[1] += (type2) a[i + 0] * (type2) c[1][i + 0];           \
    res[2] += (type2) a[i + 1] * (type2) c[0][i + 1];           \
    res[3] += (type2) a[i + 1] * (type2) c[1][i + 1];           \
  }                                                             \
  res[0] = (res[0] + res[2]) >> (prec);                         \
  res[1] = (res[1] + res[3]) >> (prec);                         \
  res[0] = ((type2)(type)res[0] - (type2)(type)res[1]) * c0 +   \
           ((type2)(type)res[1] << (prec));                     \
  res[0] = (res[0] + ((type2)1 << ((prec) - 1))) >> (prec);     \
  *o = CLAMP (res[0], -(limit), (limit) - 1);                   \
}

INNER_PRODUCT_INT_LINEAR_FUNC (gint16, gint32, PRECISION_S16, (gint32) 1 << 15);
INNER_PRODUCT_INT_LINEAR_FUNC (gint32, gint64, PRECISION_S32, (gint64) 1 << 31);

#define INNER_PRODUCT_INT_CUBIC_FUNC(type,type2,prec,limit)     \
static inline void                                              \
inner_product_##type##_cubic_1_c (type * o, const type * a,     \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type2 res[4] = { 0, 0, 0, 0 };                                \
  const type *c[4] = {(type*)((gint8*)b + 0*bstride),           \
                      (type*)((gint8*)b + 1*bstride),           \
                      (type*)((gint8*)b + 2*bstride),           \
                      (type*)((gint8*)b + 3*bstride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    res[0] += (type2) a[i] * (type2) c[0][i];                   \
    res[1] += (type2) a[i] * (type2) c[1][i];                   \
    res[2] += (type2) a[i] * (type2) c[2][i];                   \
    res[3] += (type2) a[i] * (type2) c[3][i];                   \
  }                                                             \
  res[0] = (type2)(type)(res[0] >> (prec)) * (type2) ic[0] +    \
           (type2)(type)(res[1] >> (prec)) * (type2) ic[1] +    \
           (type2)(type)(res[2] >> (prec)) * (type2) ic[2] +    \
           (type2)(type)(res[3] >> (prec)) * (type2) ic[3];     \
  res[0] = (res[0] + ((type2)1 << ((prec) - 1))) >> (prec);     \
  *o = CLAMP (res[0], -(limit), (limit) - 1);                   \
}

INNER_PRODUCT_INT_CUBIC_FUNC (gint16, gint32, PRECISION_S16, (gint32) 1 << 15);
INNER_PRODUCT_INT_CUBIC_FUNC (gint32, gint64, PRECISION_S32, (gint64) 1 << 31);

#define INNER_PRODUCT_FLOAT_FULL_FUNC(type)                     \
static inline void                                              \
inner_product_##type##_full_1_c (type * o, const type * a,      \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type res[4] = { 0.0, 0.0, 0.0, 0.0 };                         \
                                                                \
  for (i = 0; i < len; i += 4) {                                \
    res[0] += a[i + 0] * b[i + 0];                              \
    res[1] += a[i + 1] * b[i + 1];                              \
    res[2] += a[i + 2] * b[i + 2];                              \
    res[3] += a[i + 3] * b[i + 3];                              \
  }                                                             \
  *o = res[0] + res[1] + res[2] + res[3];                       \
}

INNER_PRODUCT_FLOAT_FULL_FUNC (gfloat);
INNER_PRODUCT_FLOAT_FULL_FUNC (gdouble);

#define INNER_PRODUCT_FLOAT_LINEAR_FUNC(type)                   \
static inline void                                              \
inner_product_##type##_linear_1_c (type * o, const type * a,    \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type res[4] = { 0.0, 0.0, 0.0, 0.0 };                         \
  const type *c[2] = {(type*)((gint8*)b + 0*bstride),           \
                      (type*)((gint8*)b + 1*bstride)};          \
                                                                \
  for (i = 0; i < len; i += 2) {                                \
    res[0] += a[i + 0] * c[0][i + 0];                           \
    res[1] += a[i + 0] * c[1][i + 0];                           \
    res[2] += a[i + 1] * c[0][i + 1];                           \
    res[3] += a[i + 1] * c[1][i + 1];                           \
  }                                                             \
  res[0] += res[2];                                             \
  res[1] += res[3];                                             \
  *o = (res[0] - res[1]) * ic[0] + res[1];                      \
}
INNER_PRODUCT_FLOAT_LINEAR_FUNC (gfloat);
INNER_PRODUCT_FLOAT_LINEAR_FUNC (gdouble);

#define INNER_PRODUCT_FLOAT_CUBIC_FUNC(type)                    \
static inline void                                              \
inner_product_##type##_cubic_1_c (type * o, const type * a,     \
    const type * b, gint len, const type *ic, gint bstride)     \
{                                                               \
  gint i;                                                       \
  type res[4] = { 0.0, 0.0, 0.0, 0.0 };                         \
  const type *c[4] = {(type*)((gint8*)b + 0*bstride),           \
                      (type*)((gint8*)b + 1*bstride),           \
                      (type*)((gint8*)b + 2*bstride),           \
                      (type*)((gint8*)b + 3*bstride)};          \
                                                                \
  for (i = 0; i < len; i++) {                                   \
    res[0] += a[i] * c[0][i];                                   \
    res[1] += a[i] * c[1][i];                                   \
    res[2] += a[i] * c[2][i];                                   \
    res[3] += a[i] * c[3][i];                                   \
  }                                                             \
  *o = res[0] * ic[0] + res[1] * ic[1] +                        \
       res[2] * ic[2] + res[3] * ic[3];                         \
}
INNER_PRODUCT_FLOAT_CUBIC_FUNC (gfloat);
INNER_PRODUCT_FLOAT_CUBIC_FUNC (gdouble);

MAKE_RESAMPLE_FUNC_STATIC (gint16, nearest, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gint32, nearest, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, nearest, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gdouble, nearest, 1, c);

MAKE_RESAMPLE_FUNC_STATIC (gint16, full, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gint32, full, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, full, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gdouble, full, 1, c);

MAKE_RESAMPLE_FUNC_STATIC (gint16, linear, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gint32, linear, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, linear, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gdouble, linear, 1, c);

MAKE_RESAMPLE_FUNC_STATIC (gint16, cubic, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gint32, cubic, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, cubic, 1, c);
MAKE_RESAMPLE_FUNC_STATIC (gdouble, cubic, 1, c);

static ResampleFunc resample_funcs[] = {
  resample_gint16_nearest_1_c,
  resample_gint32_nearest_1_c,
  resample_gfloat_nearest_1_c,
  resample_gdouble_nearest_1_c,

  resample_gint16_full_1_c,
  resample_gint32_full_1_c,
  resample_gfloat_full_1_c,
  resample_gdouble_full_1_c,

  resample_gint16_linear_1_c,
  resample_gint32_linear_1_c,
  resample_gfloat_linear_1_c,
  resample_gdouble_linear_1_c,

  resample_gint16_cubic_1_c,
  resample_gint32_cubic_1_c,
  resample_gfloat_cubic_1_c,
  resample_gdouble_cubic_1_c,
};

#define resample_gint16_nearest_1 resample_funcs[0]
#define resample_gint32_nearest_1 resample_funcs[1]
#define resample_gfloat_nearest_1 resample_funcs[2]
#define resample_gdouble_nearest_1 resample_funcs[3]

#define resample_gint16_full_1 resample_funcs[4]
#define resample_gint32_full_1 resample_funcs[5]
#define resample_gfloat_full_1 resample_funcs[6]
#define resample_gdouble_full_1 resample_funcs[7]

#define resample_gint16_linear_1 resample_funcs[8]
#define resample_gint32_linear_1 resample_funcs[9]
#define resample_gfloat_linear_1 resample_funcs[10]
#define resample_gdouble_linear_1 resample_funcs[11]

#define resample_gint16_cubic_1 resample_funcs[12]
#define resample_gint32_cubic_1 resample_funcs[13]
#define resample_gfloat_cubic_1 resample_funcs[14]
#define resample_gdouble_cubic_1 resample_funcs[15]

#if defined HAVE_ORC && !defined DISABLE_ORC
# if defined (HAVE_ARM_NEON)
#  define CHECK_NEON
#  include "audio-resampler-neon.h"
# endif
# if defined (__i386__) || defined (__x86_64__)
#  define CHECK_X86
#  include "audio-resampler-x86.h"
# endif
#endif

static void
audio_resampler_init (void)
{
  static gsize init_gonce = 0;

  if (g_once_init_enter (&init_gonce)) {

    GST_DEBUG_CATEGORY_INIT (audio_resampler_debug, "audio-resampler", 0,
        "audio-resampler object");

#if defined HAVE_ORC && !defined DISABLE_ORC
    orc_init ();
    {
      OrcTarget *target = orc_target_get_default ();
      gint i;

      if (target) {
        const gchar *name;
        unsigned int flags = orc_target_get_default_flags (target);

        for (i = -1; i < 32; ++i) {
          if (i == -1) {
            name = orc_target_get_name (target);
            GST_DEBUG ("target %s, default flags %08x", name, flags);
          } else if (flags & (1U << i)) {
            name = orc_target_get_flag_name (target, i);
            GST_DEBUG ("target flag %s", name);
          } else
            name = NULL;

          if (name) {
#ifdef CHECK_X86
            audio_resampler_check_x86 (name);
#endif
#ifdef CHECK_NEON
            audio_resampler_check_neon (name);
#endif
          }
        }
      }
    }
#endif
    g_once_init_leave (&init_gonce, 1);
  }
}

#define MAKE_DEINTERLEAVE_FUNC(type)                                    \
static void                                                             \
deinterleave_ ##type (GstAudioResampler * resampler, gpointer sbuf[],   \
    gpointer in[], gsize in_frames)                                     \
{                                                                       \
  gint i, c, channels = resampler->channels;                            \
  gsize samples_avail = resampler->samples_avail;                       \
  for (c = 0; c < channels; c++) {                                      \
    type *s = (type *) sbuf[c] + samples_avail;                         \
    if (G_UNLIKELY (in == NULL)) {                                      \
      for (i = 0; i < in_frames; i++)                                   \
        s[i] = 0;                                                       \
    } else {                                                            \
      type *ip = (type *) in[0] + c;                                    \
      for (i = 0; i < in_frames; i++, ip += channels)                   \
        s[i] = *ip;                                                     \
    }                                                                   \
  }                                                                     \
}

MAKE_DEINTERLEAVE_FUNC (gint16);
MAKE_DEINTERLEAVE_FUNC (gint32);
MAKE_DEINTERLEAVE_FUNC (gfloat);
MAKE_DEINTERLEAVE_FUNC (gdouble);

static DeinterleaveFunc deinterleave_funcs[] = {
  deinterleave_gint16,
  deinterleave_gint32,
  deinterleave_gfloat,
  deinterleave_gdouble
};

static void
copy_func (GstAudioResampler * resampler, gpointer sbuf[],
    gpointer in[], gsize in_frames)
{
  gint c, channels = resampler->channels;
  gsize samples_avail = resampler->samples_avail;
  for (c = 0; c < channels; c++) {
    guint8 *s = ((guint8 *) sbuf[c]) + (samples_avail * resampler->bps);
    if (G_UNLIKELY (in == NULL)) {
      memset (s, 0, in_frames * resampler->bps);
    } else {
      memcpy (s, in[c], in_frames * resampler->bps);
    }
  }
}

static void
calculate_kaiser_params (GstAudioResampler * resampler)
{
  gdouble A, B, dw, tr_bw, Fc;
  gint n;
  const KaiserQualityMap *q = &kaiser_qualities[DEFAULT_QUALITY];

  /* default cutoff */
  Fc = q->cutoff;
  if (resampler->out_rate < resampler->in_rate)
    Fc *= q->downsample_cutoff_factor;

  Fc = GET_OPT_CUTOFF (resampler->options, Fc);
  A = GET_OPT_STOP_ATTENUATION (resampler->options, q->stopband_attenuation);
  tr_bw =
      GET_OPT_TRANSITION_BANDWIDTH (resampler->options,
      q->transition_bandwidth);

  GST_LOG ("Fc %f, A %f, tr_bw %f", Fc, A, tr_bw);

  /* calculate Beta */
  if (A > 50)
    B = 0.1102 * (A - 8.7);
  else if (A >= 21)
    B = 0.5842 * pow (A - 21, 0.4) + 0.07886 * (A - 21);
  else
    B = 0.0;
  /* calculate transition width in radians */
  dw = 2 * G_PI * (tr_bw);
  /* order of the filter */
  n = (A - 8.0) / (2.285 * dw);

  resampler->kaiser_beta = B;
  resampler->n_taps = n + 1;
  resampler->cutoff = Fc;

  GST_LOG ("using Beta %f n_taps %d cutoff %f", resampler->kaiser_beta,
      resampler->n_taps, resampler->cutoff);
}

static void
alloc_taps_mem (GstAudioResampler * resampler, gint bps, gint n_taps,
    gint n_phases)
{
  if (resampler->alloc_taps >= n_taps && resampler->alloc_phases >= n_phases)
    return;

  GST_DEBUG ("allocate bps %d n_taps %d n_phases %d", bps, n_taps, n_phases);

  resampler->tmp_taps =
      g_realloc_n (resampler->tmp_taps, n_taps, sizeof (gdouble));

  resampler->taps_stride = GST_ROUND_UP_32 (bps * (n_taps + TAPS_OVERREAD));

  g_free (resampler->taps_mem);
  resampler->taps_mem =
      g_malloc0 (n_phases * resampler->taps_stride + ALIGN - 1);
  resampler->taps = MEM_ALIGN ((gint8 *) resampler->taps_mem, ALIGN);
  resampler->alloc_taps = n_taps;
  resampler->alloc_phases = n_phases;
}

static void
alloc_cache_mem (GstAudioResampler * resampler, gint bps, gint n_taps,
    gint n_phases)
{
  gsize phases_size;

  resampler->tmp_taps =
      g_realloc_n (resampler->tmp_taps, n_taps, sizeof (gdouble));

  resampler->cached_taps_stride =
      GST_ROUND_UP_32 (bps * (n_taps + TAPS_OVERREAD));

  phases_size = sizeof (gpointer) * n_phases;

  g_free (resampler->cached_taps_mem);
  resampler->cached_taps_mem =
      g_malloc0 (phases_size + n_phases * resampler->cached_taps_stride +
      ALIGN - 1);
  resampler->cached_taps =
      MEM_ALIGN ((gint8 *) resampler->cached_taps_mem + phases_size, ALIGN);
  resampler->cached_phases = resampler->cached_taps_mem;
}

static void
setup_functions (GstAudioResampler * resampler)
{
  gint index, fidx;

  index = resampler->format_index;

  if (resampler->in_rate == resampler->out_rate)
    resampler->resample = resample_funcs[index];
  else {
    switch (resampler->filter_interpolation) {
      default:
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE:
        fidx = 0;
        break;
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_LINEAR:
        GST_DEBUG ("using linear interpolation for filter coefficients");
        fidx = 0;
        break;
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_CUBIC:
        GST_DEBUG ("using cubic interpolation for filter coefficients");
        fidx = 4;
        break;
    }
    GST_DEBUG ("using filter interpolate function %d", index + fidx);
    resampler->interpolate = interpolate_funcs[index + fidx];

    switch (resampler->method) {
      case GST_AUDIO_RESAMPLER_METHOD_NEAREST:
        GST_DEBUG ("using nearest filter function");
        break;
      default:
        index += 4;
        switch (resampler->filter_mode) {
          default:
          case GST_AUDIO_RESAMPLER_FILTER_MODE_FULL:
            GST_DEBUG ("using full filter function");
            break;
          case GST_AUDIO_RESAMPLER_FILTER_MODE_INTERPOLATED:
            index += 4 + fidx;
            GST_DEBUG ("using interpolated filter function");
            break;
        }
        break;
    }
    GST_DEBUG ("using resample function %d", index);
    resampler->resample = resample_funcs[index];
  }
}

static void
resampler_calculate_taps (GstAudioResampler * resampler)
{
  gint bps;
  gint n_taps, oversample;
  gint in_rate, out_rate;
  gboolean scale = TRUE, sinc_table = FALSE;
  GstAudioResamplerFilterInterpolation filter_interpolation;

  switch (resampler->method) {
    case GST_AUDIO_RESAMPLER_METHOD_NEAREST:
      resampler->n_taps = 2;
      scale = FALSE;
      break;
    case GST_AUDIO_RESAMPLER_METHOD_LINEAR:
      resampler->n_taps = GET_OPT_N_TAPS (resampler->options, 2);
      break;
    case GST_AUDIO_RESAMPLER_METHOD_CUBIC:
      resampler->n_taps = GET_OPT_N_TAPS (resampler->options, 4);
      resampler->b = GET_OPT_CUBIC_B (resampler->options);
      resampler->c = GET_OPT_CUBIC_C (resampler->options);;
      break;
    case GST_AUDIO_RESAMPLER_METHOD_BLACKMAN_NUTTALL:
    {
      const BlackmanQualityMap *q = &blackman_qualities[DEFAULT_QUALITY];
      resampler->n_taps = GET_OPT_N_TAPS (resampler->options, q->n_taps);
      resampler->cutoff = GET_OPT_CUTOFF (resampler->options, q->cutoff);
      sinc_table = TRUE;
      break;
    }
    case GST_AUDIO_RESAMPLER_METHOD_KAISER:
      calculate_kaiser_params (resampler);
      sinc_table = TRUE;
      break;
  }

  in_rate = resampler->in_rate;
  out_rate = resampler->out_rate;

  if (out_rate < in_rate && scale) {
    resampler->cutoff = resampler->cutoff * out_rate / in_rate;
    resampler->n_taps =
        gst_util_uint64_scale_int (resampler->n_taps, in_rate, out_rate);
  }

  if (sinc_table) {
    resampler->n_taps = GST_ROUND_UP_8 (resampler->n_taps);
    resampler->filter_mode = GET_OPT_FILTER_MODE (resampler->options);
    resampler->filter_threshold =
        GET_OPT_FILTER_MODE_THRESHOLD (resampler->options);
    filter_interpolation = GET_OPT_FILTER_INTERPOLATION (resampler->options);

  } else {
    resampler->filter_mode = GST_AUDIO_RESAMPLER_FILTER_MODE_FULL;
    filter_interpolation = GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE;
  }

  /* calculate oversampling for interpolated filter */
  if (filter_interpolation != GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE) {
    gint mult = 2;

    oversample = GET_OPT_FILTER_OVERSAMPLE (resampler->options);
    while (oversample > 1) {
      if (mult * out_rate >= in_rate)
        break;

      mult *= 2;
      oversample >>= 1;
    }

    switch (filter_interpolation) {
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_LINEAR:
        oversample *= 11;
        break;
      default:
        break;
    }
  } else {
    oversample = 1;
  }
  resampler->oversample = oversample;

  n_taps = resampler->n_taps;
  bps = resampler->bps;

  GST_LOG ("using n_taps %d cutoff %f oversample %d", n_taps, resampler->cutoff,
      oversample);

  if (resampler->filter_mode == GST_AUDIO_RESAMPLER_FILTER_MODE_AUTO) {
    if (out_rate <= oversample
        && !(resampler->flags & GST_AUDIO_RESAMPLER_FLAG_VARIABLE_RATE)) {
      /* don't interpolate if we need to calculate at least the same amount
       * of filter coefficients than the full table case */
      resampler->filter_mode = GST_AUDIO_RESAMPLER_FILTER_MODE_FULL;
      GST_DEBUG ("automatically selected full filter, %d <= %d", out_rate,
          oversample);
    } else if (bps * n_taps * out_rate < resampler->filter_threshold) {
      /* switch to full filter when memory is below threshold */
      resampler->filter_mode = GST_AUDIO_RESAMPLER_FILTER_MODE_FULL;
      GST_DEBUG ("automatically selected full filter, memory %d <= %d",
          bps * n_taps * out_rate, resampler->filter_threshold);
    } else {
      GST_DEBUG ("automatically selected interpolated filter");
      resampler->filter_mode = GST_AUDIO_RESAMPLER_FILTER_MODE_INTERPOLATED;
    }
  }
  /* interpolated table but no interpolation given, assume default */
  if (resampler->filter_mode != GST_AUDIO_RESAMPLER_FILTER_MODE_FULL &&
      filter_interpolation == GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE)
    filter_interpolation = DEFAULT_OPT_FILTER_INTERPOLATION;

  resampler->filter_interpolation = filter_interpolation;

  if (resampler->filter_mode == GST_AUDIO_RESAMPLER_FILTER_MODE_FULL &&
      resampler->method != GST_AUDIO_RESAMPLER_METHOD_NEAREST) {
    GST_DEBUG ("setting up filter cache");
    resampler->n_phases = out_rate;
    alloc_cache_mem (resampler, bps, n_taps, out_rate);
  }

  if (resampler->filter_interpolation !=
      GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_NONE) {
    gint i, isize;
    gdouble x;
    gpointer taps;

    switch (resampler->filter_interpolation) {
      default:
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_LINEAR:
        GST_DEBUG ("using linear interpolation to build filter");
        isize = 2;
        break;
      case GST_AUDIO_RESAMPLER_FILTER_INTERPOLATION_CUBIC:
        GST_DEBUG ("using cubic interpolation to build filter");
        isize = 4;
        break;
    }

    alloc_taps_mem (resampler, bps, n_taps, oversample + isize);

    for (i = 0; i < oversample + isize; i++) {
      x = -(n_taps / 2) + i / (gdouble) oversample;
      taps = (gint8 *) resampler->taps + i * resampler->taps_stride;
      make_taps (resampler, taps, x, n_taps);
    }
  }
}

#define PRINT_TAPS(type,print)                          \
G_STMT_START {                                          \
  type sum = 0.0, *taps;                                \
  type icoeff[4];                                       \
  gint samp_index = 0, samp_phase = i;                  \
                                                        \
  taps = get_taps_##type##_full (resampler, &samp_index,\
      &samp_phase, icoeff);                             \
                                                        \
  for (j = 0; j < n_taps; j++) {                        \
    type tap = taps[j];                                 \
    fprintf (stderr, "\t%" print " ", tap);             \
    sum += tap;                                         \
  }                                                     \
  fprintf (stderr, "\t: sum %" print "\n", sum);        \
} G_STMT_END

static void
resampler_dump (GstAudioResampler * resampler)
{
#if 0
  gint i, n_taps, out_rate;
  gint64 a;

  out_rate = resampler->out_rate;
  n_taps = resampler->n_taps;

  fprintf (stderr, "out size %d, max taps %d\n", out_rate, n_taps);

  a = g_get_monotonic_time ();

  for (i = 0; i < out_rate; i++) {
    gint j;

    //fprintf (stderr, "%u: %d %d\t ", i, t->sample_inc, t->next_phase);
    switch (resampler->format) {
      case GST_AUDIO_FORMAT_F64:
        PRINT_TAPS (gdouble, "f");
        break;
      case GST_AUDIO_FORMAT_F32:
        PRINT_TAPS (gfloat, "f");
        break;
      case GST_AUDIO_FORMAT_S32:
        PRINT_TAPS (gint32, "d");
        break;
      case GST_AUDIO_FORMAT_S16:
        PRINT_TAPS (gint16, "d");
        break;
      default:
        break;
    }
  }
  fprintf (stderr, "time %" G_GUINT64_FORMAT "\n", g_get_monotonic_time () - a);
#endif
}

/**
 * gst_audio_resampler_options_set_quality:
 * @method: a #GstAudioResamplerMethod
 * @quality: the quality
 * @in_rate: the input rate
 * @out_rate: the output rate
 * @options: a #GstStructure
 *
 * Set the parameters for resampling from @in_rate to @out_rate using @method
 * for @quality in @options.
 */
void
gst_audio_resampler_options_set_quality (GstAudioResamplerMethod method,
    guint quality, gint in_rate, gint out_rate, GstStructure * options)
{
  g_return_if_fail (options != NULL);
  g_return_if_fail (quality <= GST_AUDIO_RESAMPLER_QUALITY_MAX);
  g_return_if_fail (in_rate > 0 && out_rate > 0);

  switch (method) {
    case GST_AUDIO_RESAMPLER_METHOD_NEAREST:
      break;
    case GST_AUDIO_RESAMPLER_METHOD_LINEAR:
      gst_structure_set (options,
          GST_AUDIO_RESAMPLER_OPT_N_TAPS, G_TYPE_INT, 2, NULL);
      break;
    case GST_AUDIO_RESAMPLER_METHOD_CUBIC:
      gst_structure_set (options,
          GST_AUDIO_RESAMPLER_OPT_N_TAPS, G_TYPE_INT, 4,
          GST_AUDIO_RESAMPLER_OPT_CUBIC_B, G_TYPE_DOUBLE, DEFAULT_OPT_CUBIC_B,
          GST_AUDIO_RESAMPLER_OPT_CUBIC_C, G_TYPE_DOUBLE, DEFAULT_OPT_CUBIC_C,
          NULL);
      break;
    case GST_AUDIO_RESAMPLER_METHOD_BLACKMAN_NUTTALL:
    {
      const BlackmanQualityMap *map = &blackman_qualities[quality];
      gst_structure_set (options,
          GST_AUDIO_RESAMPLER_OPT_N_TAPS, G_TYPE_INT, map->n_taps,
          GST_AUDIO_RESAMPLER_OPT_CUTOFF, G_TYPE_DOUBLE, map->cutoff, NULL);
      break;
    }
    case GST_AUDIO_RESAMPLER_METHOD_KAISER:
    {
      const KaiserQualityMap *map = &kaiser_qualities[quality];
      gdouble cutoff;

      cutoff = map->cutoff;
      if (out_rate < in_rate)
        cutoff *= map->downsample_cutoff_factor;

      gst_structure_set (options,
          GST_AUDIO_RESAMPLER_OPT_CUTOFF, G_TYPE_DOUBLE, cutoff,
          GST_AUDIO_RESAMPLER_OPT_STOP_ATTENUATION, G_TYPE_DOUBLE,
          map->stopband_attenuation,
          GST_AUDIO_RESAMPLER_OPT_TRANSITION_BANDWIDTH, G_TYPE_DOUBLE,
          map->transition_bandwidth, NULL);
      break;
    }
  }
  gst_structure_set (options,
      GST_AUDIO_RESAMPLER_OPT_FILTER_OVERSAMPLE, G_TYPE_INT,
      oversample_qualities[quality], NULL);
}

/**
 * gst_audio_resampler_new:
 * @method: a #GstAudioResamplerMethod
 * @flags: #GstAudioResamplerFlags
 * @format: the #GstAudioFormat
 * @channels: the number of channels
 * @in_rate: input rate
 * @out_rate: output rate
 * @options: extra options
 *
 * Make a new resampler.
 *
 * Returns: (skip) (transfer full): The new #GstAudioResampler.
 */
GstAudioResampler *
gst_audio_resampler_new (GstAudioResamplerMethod method,
    GstAudioResamplerFlags flags,
    GstAudioFormat format, gint channels,
    gint in_rate, gint out_rate, GstStructure * options)
{
  gboolean non_interleaved_in, non_interleaved_out;
  GstAudioResampler *resampler;
  const GstAudioFormatInfo *info;
  GstStructure *def_options = NULL;

  g_return_val_if_fail (method >= GST_AUDIO_RESAMPLER_METHOD_NEAREST
      && method <= GST_AUDIO_RESAMPLER_METHOD_KAISER, NULL);
  g_return_val_if_fail (format == GST_AUDIO_FORMAT_S16 ||
      format == GST_AUDIO_FORMAT_S32 || format == GST_AUDIO_FORMAT_F32 ||
      format == GST_AUDIO_FORMAT_F64, NULL);
  g_return_val_if_fail (channels > 0, NULL);
  g_return_val_if_fail (in_rate > 0, NULL);
  g_return_val_if_fail (out_rate > 0, NULL);

  audio_resampler_init ();

  resampler = g_new0 (GstAudioResampler, 1);
  resampler->method = method;
  resampler->flags = flags;
  resampler->format = format;
  resampler->channels = channels;

  switch (format) {
    case GST_AUDIO_FORMAT_S16:
      resampler->format_index = 0;
      break;
    case GST_AUDIO_FORMAT_S32:
      resampler->format_index = 1;
      break;
    case GST_AUDIO_FORMAT_F32:
      resampler->format_index = 2;
      break;
    case GST_AUDIO_FORMAT_F64:
      resampler->format_index = 3;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  info = gst_audio_format_get_info (format);
  resampler->bps = GST_AUDIO_FORMAT_INFO_WIDTH (info) / 8;
  resampler->sbuf = g_malloc0 (sizeof (gpointer) * channels);

  non_interleaved_in =
      (resampler->flags & GST_AUDIO_RESAMPLER_FLAG_NON_INTERLEAVED_IN);
  non_interleaved_out =
      (resampler->flags & GST_AUDIO_RESAMPLER_FLAG_NON_INTERLEAVED_OUT);

  /* we resample each channel separately */
  resampler->blocks = resampler->channels;
  resampler->inc = 1;
  resampler->ostride = non_interleaved_out ? 1 : resampler->channels;
  resampler->deinterleave = non_interleaved_in ?
      copy_func : deinterleave_funcs[resampler->format_index];
  resampler->convert_taps = convert_taps_funcs[resampler->format_index];

  GST_DEBUG ("method %d, bps %d, channels %d", method, resampler->bps,
      resampler->channels);

  if (options == NULL) {
    options = def_options =
        gst_structure_new_empty ("GstAudioResampler.options");
    gst_audio_resampler_options_set_quality (DEFAULT_RESAMPLER_METHOD,
        GST_AUDIO_RESAMPLER_QUALITY_DEFAULT, in_rate, out_rate, options);
  }

  gst_audio_resampler_update (resampler, in_rate, out_rate, options);
  gst_audio_resampler_reset (resampler);

  if (def_options)
    gst_structure_free (def_options);

  return resampler;
}

/* make the buffers to hold the (deinterleaved) samples */
static inline gpointer *
get_sample_bufs (GstAudioResampler * resampler, gsize need)
{
  if (G_LIKELY (resampler->samples_len < need)) {
    gint c, blocks = resampler->blocks;
    gsize bytes, to_move = 0;
    gint8 *ptr, *samples;

    GST_LOG ("realloc %d -> %d", (gint) resampler->samples_len, (gint) need);

    bytes = GST_ROUND_UP_N (need * resampler->bps * resampler->inc, ALIGN);

    samples = g_malloc0 (blocks * bytes + ALIGN - 1);
    ptr = MEM_ALIGN (samples, ALIGN);

    /* if we had some data, move history */
    if (resampler->samples_len > 0)
      to_move = resampler->samples_avail * resampler->bps * resampler->inc;

    /* set up new pointers */
    for (c = 0; c < blocks; c++) {
      memcpy (ptr + (c * bytes), resampler->sbuf[c], to_move);
      resampler->sbuf[c] = ptr + (c * bytes);
    }
    g_free (resampler->samples);
    resampler->samples = samples;
    resampler->samples_len = need;
  }
  return resampler->sbuf;
}

/**
 * gst_audio_resampler_reset:
 * @resampler: a #GstAudioResampler
 *
 * Reset @resampler to the state it was when it was first created, discarding
 * all sample history.
 */
void
gst_audio_resampler_reset (GstAudioResampler * resampler)
{
  g_return_if_fail (resampler != NULL);

  if (resampler->samples) {
    gsize bytes;
    gint c, blocks, bpf;

    bpf = resampler->bps * resampler->inc;
    bytes = (resampler->n_taps / 2) * bpf;
    blocks = resampler->blocks;

    for (c = 0; c < blocks; c++)
      memset (resampler->sbuf[c], 0, bytes);
  }
  /* half of the filter is filled with 0 */
  resampler->samp_index = 0;
  resampler->samples_avail = resampler->n_taps / 2 - 1;
}

/**
 * gst_audio_resampler_update:
 * @resampler: a #GstAudioResampler
 * @in_rate: new input rate
 * @out_rate: new output rate
 * @options: new options or %NULL
 *
 * Update the resampler parameters for @resampler. This function should
 * not be called concurrently with any other function on @resampler.
 *
 * When @in_rate or @out_rate is 0, its value is unchanged.
 *
 * When @options is %NULL, the previously configured options are reused.
 *
 * Returns: %TRUE if the new parameters could be set
 */
gboolean
gst_audio_resampler_update (GstAudioResampler * resampler,
    gint in_rate, gint out_rate, GstStructure * options)
{
  gint gcd, samp_phase, old_n_taps;
  gdouble max_error;

  g_return_val_if_fail (resampler != NULL, FALSE);

  if (in_rate <= 0)
    in_rate = resampler->in_rate;
  if (out_rate <= 0)
    out_rate = resampler->out_rate;

  if (resampler->out_rate > 0) {
    GST_INFO ("old phase %d/%d", resampler->samp_phase, resampler->out_rate);
    samp_phase =
        gst_util_uint64_scale_int (resampler->samp_phase, out_rate,
        resampler->out_rate);
  } else
    samp_phase = 0;

  gcd = gst_util_greatest_common_divisor (in_rate, out_rate);

  max_error = GET_OPT_MAX_PHASE_ERROR (resampler->options);

  if (max_error < 1.0e-8) {
    GST_INFO ("using exact phase divider");
    gcd = gst_util_greatest_common_divisor (gcd, samp_phase);
  } else {
    while (gcd > 1) {
      gdouble ph1 = (gdouble) samp_phase / out_rate;
      gint factor = 2;

      /* reduce the factor until we have a phase error of less than 10% */
      gdouble ph2 = (gdouble) (samp_phase / gcd) / (out_rate / gcd);

      if (fabs (ph1 - ph2) < max_error)
        break;

      while (gcd % factor != 0)
        factor++;
      gcd /= factor;

      GST_INFO ("divide by factor %d, gcd %d", factor, gcd);
    }
  }

  GST_INFO ("phase %d out_rate %d, in_rate %d, gcd %d", samp_phase, out_rate,
      in_rate, gcd);

  resampler->samp_phase = samp_phase /= gcd;
  resampler->in_rate = in_rate /= gcd;
  resampler->out_rate = out_rate /= gcd;

  GST_INFO ("new phase %d/%d", resampler->samp_phase, resampler->out_rate);

  resampler->samp_inc = in_rate / out_rate;
  resampler->samp_frac = in_rate % out_rate;

  if (options) {
    GST_INFO ("have new options, reconfigure filter");

    if (resampler->options)
      gst_structure_free (resampler->options);
    resampler->options = gst_structure_copy (options);

    old_n_taps = resampler->n_taps;

    resampler_calculate_taps (resampler);
    resampler_dump (resampler);

    if (old_n_taps > 0 && old_n_taps != resampler->n_taps) {
      gpointer *sbuf;
      gint i, bpf, bytes, soff, doff, diff;

      sbuf = get_sample_bufs (resampler, resampler->n_taps);

      bpf = resampler->bps * resampler->inc;
      bytes = resampler->samples_avail * bpf;
      soff = doff = resampler->samp_index * bpf;

      diff = ((gint) resampler->n_taps - old_n_taps) / 2;

      GST_DEBUG ("taps %d->%d, %d", old_n_taps, resampler->n_taps, diff);

      if (diff < 0) {
        /* diff < 0, decrease taps, adjust source */
        soff += -diff * bpf;
        bytes -= -diff * bpf;
      } else {
        /* diff > 0, increase taps, adjust dest */
        doff += diff * bpf;
      }

      /* now shrink or enlarge the history buffer, when we enlarge we
       * just leave the old samples in there. FIXME, probably do something better
       * like mirror or fill with zeroes. */
      for (i = 0; i < resampler->blocks; i++)
        memmove ((gint8 *) sbuf[i] + doff, (gint8 *) sbuf[i] + soff, bytes);

      resampler->samples_avail += diff;
    }
  } else if (resampler->filter_mode == GST_AUDIO_RESAMPLER_FILTER_MODE_FULL) {
    GST_DEBUG ("setting up filter cache");
    resampler->n_phases = resampler->out_rate;
    alloc_cache_mem (resampler, resampler->bps, resampler->n_taps,
        resampler->n_phases);
  }
  setup_functions (resampler);

  return TRUE;
}

/**
 * gst_audio_resampler_free:
 * @resampler: a #GstAudioResampler
 *
 * Free a previously allocated #GstAudioResampler @resampler.
 */
void
gst_audio_resampler_free (GstAudioResampler * resampler)
{
  g_return_if_fail (resampler != NULL);

  g_free (resampler->cached_taps_mem);
  g_free (resampler->taps_mem);
  g_free (resampler->tmp_taps);
  g_free (resampler->samples);
  g_free (resampler->sbuf);
  if (resampler->options)
    gst_structure_free (resampler->options);
  g_free (resampler);
}

/**
 * gst_audio_resampler_get_out_frames:
 * @resampler: a #GstAudioResampler
 * @in_frames: number of input frames
 *
 * Get the number of output frames that would be currently available when
 * @in_frames are given to @resampler.
 *
 * Returns: The number of frames that would be available after giving
 * @in_frames as input to @resampler.
 */
gsize
gst_audio_resampler_get_out_frames (GstAudioResampler * resampler,
    gsize in_frames)
{
  gsize need, avail, out;

  g_return_val_if_fail (resampler != NULL, 0);

  need = resampler->n_taps + resampler->samp_index + resampler->skip;
  avail = resampler->samples_avail + in_frames;
  GST_LOG ("need %d = %d + %d + %d, avail %d = %d + %d", (gint) need,
      resampler->n_taps, resampler->samp_index, resampler->skip,
      (gint) avail, (gint) resampler->samples_avail, (gint) in_frames);
  if (avail < need) {
    GST_LOG ("avail %d < need %d", (int) avail, (int) need);
    return 0;
  }

  out = (avail - need) * resampler->out_rate;
  if (out < resampler->samp_phase) {
    GST_LOG ("out %d < samp_phase %d", (int) out, (int) resampler->samp_phase);
    return 0;
  }

  out = ((out - resampler->samp_phase) / resampler->in_rate) + 1;
  GST_LOG ("out %d = ((%d * %d - %d) / %d) + 1", (gint) out,
      (gint) (avail - need), resampler->out_rate, resampler->samp_phase,
      resampler->in_rate);

  return out;
}

/**
 * gst_audio_resampler_get_in_frames:
 * @resampler: a #GstAudioResampler
 * @out_frames: number of input frames
 *
 * Get the number of input frames that would currently be needed
 * to produce @out_frames from @resampler.
 *
 * Returns: The number of input frames needed for producing
 * @out_frames of data from @resampler.
 */
gsize
gst_audio_resampler_get_in_frames (GstAudioResampler * resampler,
    gsize out_frames)
{
  gsize in_frames;

  g_return_val_if_fail (resampler != NULL, 0);

  in_frames =
      (resampler->samp_phase +
      out_frames * resampler->samp_frac) / resampler->out_rate;
  in_frames += out_frames * resampler->samp_inc;

  return in_frames;
}

/**
 * gst_audio_resampler_get_max_latency:
 * @resampler: a #GstAudioResampler
 *
 * Get the maximum number of input samples that the resampler would
 * need before producing output.
 *
 * Returns: the latency of @resampler as expressed in the number of
 * frames.
 */
gsize
gst_audio_resampler_get_max_latency (GstAudioResampler * resampler)
{
  g_return_val_if_fail (resampler != NULL, 0);

  return resampler->n_taps / 2;
}

/**
 * gst_audio_resampler_resample:
 * @resampler: a #GstAudioResampler
 * @in: input samples
 * @in_frames: number of input frames
 * @out: output samples
 * @out_frames: number of output frames
 *
 * Perform resampling on @in_frames frames in @in and write @out_frames to @out.
 *
 * In case the samples are interleaved, @in and @out must point to an
 * array with a single element pointing to a block of interleaved samples.
 *
 * If non-interleaved samples are used, @in and @out must point to an
 * array with pointers to memory blocks, one for each channel.
 *
 * @in may be %NULL, in which case @in_frames of silence samples are pushed
 * into the resampler.
 *
 * This function always produces @out_frames of output and consumes @in_frames of
 * input. Use gst_audio_resampler_get_out_frames() and
 * gst_audio_resampler_get_in_frames() to make sure @in_frames and @out_frames
 * are matching and @in and @out point to enough memory.
 */
void
gst_audio_resampler_resample (GstAudioResampler * resampler,
    gpointer in[], gsize in_frames, gpointer out[], gsize out_frames)
{
  gsize samples_avail;
  gsize need, consumed;
  gpointer *sbuf;

  /* do sample skipping */
  if (G_UNLIKELY (resampler->skip >= in_frames)) {
    /* we need tp skip all input */
    resampler->skip -= in_frames;
    return;
  }
  /* skip the last samples by advancing the sample index */
  resampler->samp_index += resampler->skip;

  samples_avail = resampler->samples_avail;

  /* make sure we have enough space to copy our samples */
  sbuf = get_sample_bufs (resampler, in_frames + samples_avail);

  /* copy/deinterleave the samples */
  resampler->deinterleave (resampler, sbuf, in, in_frames);

  /* update new amount of samples in our buffer */
  resampler->samples_avail = samples_avail += in_frames;

  need = resampler->n_taps + resampler->samp_index;
  if (G_UNLIKELY (samples_avail < need || out_frames == 0)) {
    GST_LOG ("not enough samples to start: need %" G_GSIZE_FORMAT ", avail %"
        G_GSIZE_FORMAT ", out %" G_GSIZE_FORMAT, need, samples_avail,
        out_frames);
    /* not enough samples to start */
    return;
  }

  /* resample all channels */
  resampler->resample (resampler, sbuf, samples_avail, out, out_frames,
      &consumed);

  GST_LOG ("in %" G_GSIZE_FORMAT ", avail %" G_GSIZE_FORMAT ", consumed %"
      G_GSIZE_FORMAT, in_frames, samples_avail, consumed);

  /* update pointers */
  if (G_LIKELY (consumed > 0)) {
    gssize left = samples_avail - consumed;
    if (left > 0) {
      /* we consumed part of our samples */
      resampler->samples_avail = left;
    } else {
      /* we consumed all our samples, empty our buffers */
      resampler->samples_avail = 0;
      resampler->skip = -left;
    }
  }
}
