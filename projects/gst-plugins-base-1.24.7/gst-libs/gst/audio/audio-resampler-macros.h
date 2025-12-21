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

#ifndef __GST_AUDIO_RESAMPLER_MACROS_H__
#define __GST_AUDIO_RESAMPLER_MACROS_H__

#include <string.h>

#include "audio-resampler-private.h"

#define PRECISION_S16 15
#define PRECISION_S32 31

#define DECL_GET_TAPS_FULL_FUNC(type)                           \
gpointer                                                        \
get_taps_##type##_full (GstAudioResampler * resampler,          \
    gint *samp_index, gint *samp_phase, type icoeff[4])

DECL_GET_TAPS_FULL_FUNC (gint16);
DECL_GET_TAPS_FULL_FUNC (gint32);
DECL_GET_TAPS_FULL_FUNC (gfloat);
DECL_GET_TAPS_FULL_FUNC (gdouble);


#define DECL_GET_TAPS_INTERPOLATE_FUNC(type, inter)             \
gpointer                                                        \
get_taps_##type##_##inter (GstAudioResampler * resampler,       \
    gint *samp_index, gint *samp_phase, type icoeff[4])         \

DECL_GET_TAPS_INTERPOLATE_FUNC (gint16, linear);
DECL_GET_TAPS_INTERPOLATE_FUNC (gint32, linear);
DECL_GET_TAPS_INTERPOLATE_FUNC (gfloat, linear);
DECL_GET_TAPS_INTERPOLATE_FUNC (gdouble, linear);

DECL_GET_TAPS_INTERPOLATE_FUNC (gint16, cubic);
DECL_GET_TAPS_INTERPOLATE_FUNC (gint32, cubic);
DECL_GET_TAPS_INTERPOLATE_FUNC (gfloat, cubic);
DECL_GET_TAPS_INTERPOLATE_FUNC (gdouble, cubic);


#define DECL_RESAMPLE_FUNC(type,inter,channels,arch)                    \
void                                                                    \
resample_ ##type## _ ##inter## _ ##channels## _ ##arch (GstAudioResampler * resampler,      \
    gpointer in[], gsize in_len,  gpointer out[], gsize out_len,        \
    gsize * consumed)

#define MAKE_RESAMPLE_FUNC(type,inter,channels,arch)            \
DECL_RESAMPLE_FUNC (type, inter, channels, arch)                \
{                                                               \
  gint c, di = 0;                                               \
  gint n_taps = resampler->n_taps;                              \
  gint blocks = resampler->blocks;                              \
  gint ostride = resampler->ostride;                            \
  gint taps_stride = resampler->taps_stride;                    \
  gint samp_index = 0;                                          \
  gint samp_phase = 0;                                          \
                                                                \
  for (c = 0; c < blocks; c++) {                                \
    type *ip = in[c];                                           \
    type *op = ostride == 1 ? out[c] : (type *)out[0] + c;      \
                                                                \
    samp_index = resampler->samp_index;                         \
    samp_phase = resampler->samp_phase;                         \
                                                                \
    for (di = 0; di < out_len; di++) {                          \
      type *ipp, icoeff[4], *taps;                              \
                                                                \
      ipp = &ip[samp_index * channels];                         \
                                                                \
      taps = get_taps_ ##type##_##inter                         \
              (resampler, &samp_index, &samp_phase, icoeff);    \
      inner_product_ ##type##_##inter##_##channels##_##arch     \
              (op, ipp, taps, n_taps, icoeff, taps_stride);     \
      op += ostride;                                            \
    }                                                           \
    if (in_len > samp_index)                                    \
      memmove (ip, &ip[samp_index * channels],                  \
          (in_len - samp_index) * sizeof(type) * channels);     \
  }                                                             \
  *consumed = samp_index - resampler->samp_index;               \
                                                                \
  resampler->samp_index = 0;                                    \
  resampler->samp_phase = samp_phase;                           \
}

#define DECL_RESAMPLE_FUNC_STATIC(type,inter,channels,arch)     \
static DECL_RESAMPLE_FUNC (type, inter, channels, arch)

#define MAKE_RESAMPLE_FUNC_STATIC(type,inter,channels,arch)     \
static MAKE_RESAMPLE_FUNC (type, inter, channels, arch)

#endif /* __GST_AUDIO_RESAMPLER_MACROS_H__ */
