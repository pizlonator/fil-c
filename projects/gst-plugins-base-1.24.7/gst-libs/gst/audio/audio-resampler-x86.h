/* GStreamer
 * Copyright (C) <2016> Wim Taymans <wim.taymans@gmail.com>
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

#include "audio-resampler-macros.h"
#include "audio-resampler-x86-sse.h"
#include "audio-resampler-x86-sse2.h"
#include "audio-resampler-x86-sse41.h"

static void
audio_resampler_check_x86 (const gchar *option)
{
  if (!strcmp (option, "sse")) {
#if defined (HAVE_XMMINTRIN_H) && HAVE_SSE
    GST_DEBUG ("enable SSE optimisations");
    resample_gfloat_full_1 = resample_gfloat_full_1_sse;
    resample_gfloat_linear_1 = resample_gfloat_linear_1_sse;
    resample_gfloat_cubic_1 = resample_gfloat_cubic_1_sse;

    interpolate_gfloat_linear = interpolate_gfloat_linear_sse;
    interpolate_gfloat_cubic = interpolate_gfloat_cubic_sse;
#else
    GST_DEBUG ("SSE optimisations not enabled");
#endif
  } else if (!strcmp (option, "sse2")) {
#if defined (HAVE_EMMINTRIN_H) && HAVE_SSE2
    GST_DEBUG ("enable SSE2 optimisations");
    resample_gint16_full_1 = resample_gint16_full_1_sse2;
    resample_gint16_linear_1 = resample_gint16_linear_1_sse2;
    resample_gint16_cubic_1 = resample_gint16_cubic_1_sse2;

    interpolate_gint16_linear = interpolate_gint16_linear_sse2;
    interpolate_gint16_cubic = interpolate_gint16_cubic_sse2;

    resample_gdouble_full_1 = resample_gdouble_full_1_sse2;
    resample_gdouble_linear_1 = resample_gdouble_linear_1_sse2;
    resample_gdouble_cubic_1 = resample_gdouble_cubic_1_sse2;

    interpolate_gdouble_linear = interpolate_gdouble_linear_sse2;
    interpolate_gdouble_cubic = interpolate_gdouble_cubic_sse2;
#else
    GST_DEBUG ("SSE2 optimisations not enabled");
#endif
  } else if (!strcmp (option, "sse41")) {
#if defined (__x86_64__) && \
    defined (HAVE_SMMINTRIN_H) && defined (HAVE_EMMINTRIN_H) && \
    HAVE_SSE41
    GST_DEBUG ("enable SSE41 optimisations");
    resample_gint32_full_1 = resample_gint32_full_1_sse41;
    resample_gint32_linear_1 = resample_gint32_linear_1_sse41;
    resample_gint32_cubic_1 = resample_gint32_cubic_1_sse41;
#else
    GST_DEBUG ("SSE41 optimisations not enabled");
#endif
  }
}
