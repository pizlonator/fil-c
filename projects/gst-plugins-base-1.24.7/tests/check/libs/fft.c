/* GStreamer
 *
 * unit test for FFT library
 *
 * Copyright (C) 2007 Sebastian Dröge <slomo@circular-chaos.org>
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

#include <gst/check/gstcheck.h>

#include <gst/fft/gstfft.h>
#include <gst/fft/gstffts16.h>
#include <gst/fft/gstffts32.h>
#include <gst/fft/gstfftf32.h>
#include <gst/fft/gstfftf64.h>

GST_START_TEST (test_next_fast_length)
{
  fail_unless_equals_int (gst_fft_next_fast_length (13), 16);
  fail_unless_equals_int (gst_fft_next_fast_length (30), 30);
  fail_unless_equals_int (gst_fft_next_fast_length (31), 32);
  fail_unless_equals_int (gst_fft_next_fast_length (1), 2);
}

GST_END_TEST;

GST_START_TEST (test_s16_0hz)
{
  gint i;
  gint16 *in;
  GstFFTS16Complex *out;
  GstFFTS16 *ctx;

  in = g_new (gint16, 2048);
  out = g_new (GstFFTS16Complex, 1025);
  ctx = gst_fft_s16_new (2048, FALSE);

  for (i = 0; i < 2048; i++)
    in[i] = G_MAXINT16;

  gst_fft_s16_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 32767.0 * 32767.0;
    mag = 10.0 * log10 (mag);
    if (i < 2)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -55.0);
  }

  gst_fft_s16_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_s16_11025hz)
{
  gint i;
  gint16 *in;
  GstFFTS16Complex *out;
  GstFFTS16 *ctx;

  in = g_new (gint16, 2048);
  out = g_new (GstFFTS16Complex, 1025);
  ctx = gst_fft_s16_new (2048, FALSE);

  for (i = 0; i < 2048; i += 4) {
    in[i] = 0;
    in[i + 1] = G_MAXINT16;
    in[i + 2] = 0;
    in[i + 3] = G_MININT16;
  }

  gst_fft_s16_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 32767.0 * 32767.0;
    mag = 10.0 * log10 (mag);

    if (abs (512 - i) < 2)
      fail_unless (mag > -20.0);
    else
      fail_unless (mag < -55.0);
  }

  gst_fft_s16_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_s16_22050hz)
{
  gint i;
  gint16 *in;
  GstFFTS16Complex *out;
  GstFFTS16 *ctx;

  in = g_new (gint16, 2048);
  out = g_new (GstFFTS16Complex, 1025);
  ctx = gst_fft_s16_new (2048, FALSE);

  for (i = 0; i < 2048; i += 2) {
    in[i] = G_MAXINT16;
    in[i + 1] = G_MININT16;
  }

  gst_fft_s16_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 32767.0 * 32767.0;
    mag = 10.0 * log10 (mag);

    if (i > 1022)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -55.0);
  }

  gst_fft_s16_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_s32_0hz)
{
  gint i;
  gint32 *in;
  GstFFTS32Complex *out;
  GstFFTS32 *ctx;

  in = g_new (gint32, 2048);
  out = g_new (GstFFTS32Complex, 1025);
  ctx = gst_fft_s32_new (2048, FALSE);

  for (i = 0; i < 2048; i++)
    in[i] = 2147483647;

  gst_fft_s32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2147483647.0 * 2147483647.0;
    mag = 10.0 * log10 (mag);

    if (i < 2)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_s32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_s32_11025hz)
{
  gint i;
  gint32 *in;
  GstFFTS32Complex *out;
  GstFFTS32 *ctx;

  in = g_new (gint32, 2048);
  out = g_new (GstFFTS32Complex, 1025);
  ctx = gst_fft_s32_new (2048, FALSE);

  for (i = 0; i < 2048; i += 4) {
    in[i] = 0;
    in[i + 1] = G_MAXINT32;
    in[i + 2] = 0;
    in[i + 3] = G_MININT32;
  }

  gst_fft_s32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2147483647.0 * 2147483647.0;
    mag = 10.0 * log10 (mag);

    if (abs (512 - i) < 2)
      fail_unless (mag > -20.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_s32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_s32_22050hz)
{
  gint i;
  gint32 *in;
  GstFFTS32Complex *out;
  GstFFTS32 *ctx;

  in = g_new (gint32, 2048);
  out = g_new (GstFFTS32Complex, 1025);
  ctx = gst_fft_s32_new (2048, FALSE);

  for (i = 0; i < 2048; i += 2) {
    in[i] = G_MAXINT32;
    in[i + 1] = G_MININT32;
  }

  gst_fft_s32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_s32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2147483647.0 * 2147483647.0;
    mag = 10.0 * log10 (mag);

    if (i > 1022)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_s32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f32_0hz)
{
  gint i;
  gfloat *in;
  GstFFTF32Complex *out;
  GstFFTF32 *ctx;

  in = g_new (gfloat, 2048);
  out = g_new (GstFFTF32Complex, 1025);
  ctx = gst_fft_f32_new (2048, FALSE);

  for (i = 0; i < 2048; i++)
    in[i] = 1.0;

  gst_fft_f32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (i < 2)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f32_11025hz)
{
  gint i;
  gfloat *in;
  GstFFTF32Complex *out;
  GstFFTF32 *ctx;

  in = g_new (gfloat, 2048);
  out = g_new (GstFFTF32Complex, 1025);
  ctx = gst_fft_f32_new (2048, FALSE);

  for (i = 0; i < 2048; i += 4) {
    in[i] = 0.0;
    in[i + 1] = 1.0;
    in[i + 2] = 0.0;
    in[i + 3] = -1.0;
  }

  gst_fft_f32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (abs (512 - i) < 2)
      fail_unless (mag > -20.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f32_22050hz)
{
  gint i;
  gfloat *in;
  GstFFTF32Complex *out;
  GstFFTF32 *ctx;

  in = g_new (gfloat, 2048);
  out = g_new (GstFFTF32Complex, 1025);
  ctx = gst_fft_f32_new (2048, FALSE);

  for (i = 0; i < 2048; i += 2) {
    in[i] = 1.0;
    in[i + 1] = -1.0;
  }

  gst_fft_f32_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f32_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (i > 1022)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f32_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f64_0hz)
{
  gint i;
  gdouble *in;
  GstFFTF64Complex *out;
  GstFFTF64 *ctx;

  in = g_new (gdouble, 2048);
  out = g_new (GstFFTF64Complex, 1025);
  ctx = gst_fft_f64_new (2048, FALSE);

  for (i = 0; i < 2048; i++)
    in[i] = 1.0;

  gst_fft_f64_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f64_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (i < 2)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f64_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f64_11025hz)
{
  gint i;
  gdouble *in;
  GstFFTF64Complex *out;
  GstFFTF64 *ctx;

  in = g_new (gdouble, 2048);
  out = g_new (GstFFTF64Complex, 1025);
  ctx = gst_fft_f64_new (2048, FALSE);

  for (i = 0; i < 2048; i += 4) {
    in[i] = 0.0;
    in[i + 1] = 1.0;
    in[i + 2] = 0.0;
    in[i + 3] = -1.0;
  }

  gst_fft_f64_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f64_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (abs (512 - i) < 2)
      fail_unless (mag > -20.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f64_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

GST_START_TEST (test_f64_22050hz)
{
  gint i;
  gdouble *in;
  GstFFTF64Complex *out;
  GstFFTF64 *ctx;

  in = g_new (gdouble, 2048);
  out = g_new (GstFFTF64Complex, 1025);
  ctx = gst_fft_f64_new (2048, FALSE);

  for (i = 0; i < 2048; i += 2) {
    in[i] = 1.0;
    in[i + 1] = -1.0;
  }

  gst_fft_f64_window (ctx, in, GST_FFT_WINDOW_HAMMING);
  gst_fft_f64_fft (ctx, in, out);

  for (i = 0; i < 1025; i++) {
    gdouble mag;

    mag = (gdouble) out[i].r * (gdouble) out[i].r;
    mag += (gdouble) out[i].i * (gdouble) out[i].i;
    mag /= 2048.0 * 2048.0;
    mag = 10.0 * log10 (mag);

    if (i > 1022)
      fail_unless (mag > -15.0);
    else
      fail_unless (mag < -60.0);
  }

  gst_fft_f64_free (ctx);
  g_free (in);
  g_free (out);
}

GST_END_TEST;

static Suite *
fft_suite (void)
{
  Suite *s = suite_create ("fft library");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_next_fast_length);
  tcase_add_test (tc_chain, test_s16_0hz);
  tcase_add_test (tc_chain, test_s16_11025hz);
  tcase_add_test (tc_chain, test_s16_22050hz);
  tcase_add_test (tc_chain, test_s32_0hz);
  tcase_add_test (tc_chain, test_s32_11025hz);
  tcase_add_test (tc_chain, test_s32_22050hz);
  tcase_add_test (tc_chain, test_f32_0hz);
  tcase_add_test (tc_chain, test_f32_11025hz);
  tcase_add_test (tc_chain, test_f32_22050hz);
  tcase_add_test (tc_chain, test_f64_0hz);
  tcase_add_test (tc_chain, test_f64_11025hz);
  tcase_add_test (tc_chain, test_f64_22050hz);

  return s;
}

GST_CHECK_MAIN (fft);
