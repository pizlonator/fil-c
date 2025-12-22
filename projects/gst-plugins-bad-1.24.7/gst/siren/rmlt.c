/*
 * Siren Encoder/Decoder library
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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


#include "siren7.h"


static int rmlt_initialized = 0;
static float rmlt_window_640[640];
static float rmlt_window_320[320];

#define PI_2     1.57079632679489661923

void
siren_rmlt_init (void)
{
  int i = 0;
  float angle;

  for (i = 0; i < 640; i++) {
    angle = (float) (((i + 0.5) * PI_2) / 640);
    rmlt_window_640[i] = (float) sin (angle);
  }
  for (i = 0; i < 320; i++) {
    angle = (float) (((i + 0.5) * PI_2) / 320);
    rmlt_window_320[i] = (float) sin (angle);
  }

  rmlt_initialized = 1;
}

int
siren_rmlt_encode_samples (float *samples, float *old_samples, int dct_length,
    float *rmlt_coefs)
{
  int half_dct_length = dct_length / 2;
  float *old_ptr = old_samples + half_dct_length;
  float *coef_high = rmlt_coefs + half_dct_length;
  float *coef_low = rmlt_coefs + half_dct_length;
  float *samples_low = samples;
  float *samples_high = samples + dct_length;
  float *window_low = NULL;
  float *window_high = NULL;
  int i = 0;

  if (rmlt_initialized == 0)
    siren_rmlt_init ();

  if (dct_length == 320)
    window_low = rmlt_window_320;
  else if (dct_length == 640)
    window_low = rmlt_window_640;
  else
    return 4;

  window_high = window_low + dct_length;


  for (i = 0; i < half_dct_length; i++) {
    *--coef_low = *--old_ptr;
    *coef_high++ =
        (*samples_low * *--window_high) - (*--samples_high * *window_low);
    *old_ptr =
        (*samples_high * *window_high) + (*samples_low++ * *window_low++);
  }
  siren_dct4 (rmlt_coefs, rmlt_coefs, dct_length);

  return 0;
}



int
siren_rmlt_decode_samples (float *coefs, float *old_coefs, int dct_length,
    float *samples)
{
  int half_dct_length = dct_length / 2;
  float *old_low = old_coefs;
  float *old_high = old_coefs + half_dct_length;
  float *samples_low = samples;
  float *samples_high = samples + dct_length;
  float *samples_middle_low = samples + half_dct_length;
  float *samples_middle_high = samples + half_dct_length;
  float *window_low = NULL;
  float *window_high = NULL;
  float *window_middle_low = NULL;
  float *window_middle_high = NULL;
  float sample_low_val;
  float sample_high_val;
  float sample_middle_low_val;
  float sample_middle_high_val;
  int i = 0;

  if (rmlt_initialized == 0)
    siren_rmlt_init ();

  if (dct_length == 320)
    window_low = rmlt_window_320;
  else if (dct_length == 640)
    window_low = rmlt_window_640;
  else
    return 4;


  window_high = window_low + dct_length;
  window_middle_low = window_low + half_dct_length;
  window_middle_high = window_low + half_dct_length;

  siren_dct4 (coefs, samples, dct_length);

  for (i = 0; i < half_dct_length; i += 2) {
    sample_low_val = *samples_low;
    sample_high_val = *--samples_high;
    sample_middle_low_val = *--samples_middle_low;
    sample_middle_high_val = *samples_middle_high;
    *samples_low++ =
        (*old_low * *--window_high) + (sample_middle_low_val * *window_low);
    *samples_high =
        (sample_middle_low_val * *window_high) - (*old_low * *window_low++);
    *samples_middle_high++ =
        (sample_low_val * *window_middle_high) -
        (*--old_high * *--window_middle_low);
    *samples_middle_low =
        (*old_high * *window_middle_high++) +
        (sample_low_val * *window_middle_low);
    *old_low++ = sample_middle_high_val;
    *old_high = sample_high_val;
  }

  return 0;
}
