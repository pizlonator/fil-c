/* GStreamer
 * (c) 2011 Stefan Kost <ensonic@users.sf.net>
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

#include <stdio.h>
#include <gst/fft/gstffts16.h>
#include <gst/fft/gstffts32.h>
#include <gst/fft/gstfftf32.h>
#include <gst/fft/gstfftf64.h>

/* effectively max range seems to be 1/4 of what it should be */

#define MAKE_I_TEST(_g_,_G_,_t_,_T_,_f_)                        \
static void                                                     \
test_##_t_ (const gchar *test_name, gint num_freq, gint window) \
{                                                               \
  GstFFT ##_T_ *ctx;                                            \
  GstFFT ##_T_ ##Complex *fdata;                                \
  _g_ *adata;                                                   \
  _g_ maxfr = 0, maxfi = 0;                                     \
  gint num_samples = num_freq * 2 - 2;                          \
  gint s, f;                                                    \
                                                                \
  ctx = gst_fft_ ##_t_ ##_new (num_samples, FALSE);             \
  fdata = g_new (GstFFT ##_T_ ##Complex, num_freq);             \
  adata = g_new (_g_, num_samples);                             \
                                                                \
  for (s = 0; s < num_samples;) {                               \
    adata[s++]=G_MIN##_G_;                                      \
    adata[s++]=G_MAX##_G_;                                      \
  }                                                             \
                                                                \
  gst_fft_ ##_t_ ##_window (ctx, adata, window);                \
  gst_fft_ ##_t_ ##_fft (ctx, adata, fdata);                    \
                                                                \
  for (f = 0; f < num_freq; f++) {                              \
    if (fdata[1+f].r > maxfr)                                   \
      maxfr = fdata[1+f].r;                                     \
    if (fdata[1+f].i > maxfi)                                   \
      maxfi = fdata[1+f].i;                                     \
  }                                                             \
                                                                \
  printf (#_t_" %-15s: maxfr: %"_f_" %10.5f maxfi: %"_f_" %10.5f\n",\
    test_name,                                                  \
    maxfr, (gfloat)G_MAX##_G_/maxfr,                            \
    maxfi, (gfloat)G_MAX##_G_/maxfi);                           \
                                                                \
  gst_fft_ ##_t_ ##_free (ctx);                                 \
  g_free (fdata);                                               \
  g_free (adata);                                               \
}

MAKE_I_TEST (gint16, INT16, s16, S16, "6d");
MAKE_I_TEST (gint32, INT32, s32, S32, "9d");

#define MAKE_F_TEST(_g_,_G_,_t_,_T_,_f_)                        \
static void                                                     \
test_##_t_ (const gchar *test_name, gint num_freq, gint window) \
{                                                               \
  GstFFT ##_T_ *ctx;                                            \
  GstFFT ##_T_ ##Complex *fdata;                                \
  _g_ *adata;                                                   \
  _g_ maxfr = 0, maxfi = 0;                                     \
  gint num_samples = num_freq * 2 - 2;                          \
  gint s, f;                                                    \
                                                                \
  ctx = gst_fft_ ##_t_ ##_new (num_samples, FALSE);             \
  fdata = g_new (GstFFT ##_T_ ##Complex, num_freq);             \
  adata = g_new (_g_, num_samples);                             \
                                                                \
  for (s = 0; s < num_samples;) {                               \
    adata[s++]=-1.0;                                            \
    adata[s++]=+1.0;                                            \
  }                                                             \
                                                                \
  gst_fft_ ##_t_ ##_window (ctx, adata, window);                \
  gst_fft_ ##_t_ ##_fft (ctx, adata, fdata);                    \
                                                                \
  for (f = 0; f < num_freq; f++) {                              \
    if (fdata[1+f].r > maxfr)                                   \
      maxfr = fdata[1+f].r;                                     \
    if (fdata[1+f].i > maxfi)                                   \
      maxfi = fdata[1+f].i;                                     \
  }                                                             \
                                                                \
  printf (#_t_" %-15s: maxfr: %"_f_" %10.5f maxfi: %"_f_" %10.5f\n",\
    test_name,                                                  \
    maxfr, (gfloat)1.0/maxfr,                                   \
    maxfi, (gfloat)1.0/maxfi);                                  \
                                                                \
  gst_fft_ ##_t_ ##_free (ctx);                                 \
  g_free (fdata);                                               \
  g_free (adata);                                               \
}

MAKE_F_TEST (gfloat, FLOAT, f32, F32, "10.5f");
MAKE_F_TEST (gdouble, DOUBLE, f64, F64, "10.5f");

gint
main (gint argc, gchar * argv[])
{
  gint num_bands;

  gst_init (&argc, &argv);

  num_bands = 200;
  test_s16 ("200, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_s16 ("200, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_s16 ("200, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_s16 ("200, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_s16 ("200, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("");

  num_bands = 300;
  test_s16 ("300, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_s16 ("300, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_s16 ("300, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_s16 ("300, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_s16 ("300, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("\n");

  num_bands = 200;
  test_s32 ("200, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_s32 ("200, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_s32 ("200, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_s32 ("200, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_s32 ("200, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("");

  num_bands = 300;
  test_s32 ("300, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_s32 ("300, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_s32 ("300, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_s32 ("300, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_s32 ("300, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("\n");

  num_bands = 200;
  test_f32 ("200, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_f32 ("200, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_f32 ("200, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_f32 ("200, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_f32 ("200, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("");

  num_bands = 300;
  test_f32 ("300, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_f32 ("300, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_f32 ("300, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_f32 ("300, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_f32 ("300, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("\n");

  num_bands = 200;
  test_f64 ("200, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_f64 ("200, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_f64 ("200, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_f64 ("200, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_f64 ("200, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("");

  num_bands = 300;
  test_f64 ("300, none", num_bands, GST_FFT_WINDOW_RECTANGULAR);
  test_f64 ("300, hamming", num_bands, GST_FFT_WINDOW_HAMMING);
  test_f64 ("300, hann", num_bands, GST_FFT_WINDOW_HANN);
  test_f64 ("300, bartlett", num_bands, GST_FFT_WINDOW_BARTLETT);
  test_f64 ("300, blackman", num_bands, GST_FFT_WINDOW_BLACKMAN);
  puts ("\n");

  return 0;
}
