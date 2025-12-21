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

#ifndef __GST_AUDIO_RESAMPLER_PRIVATE_H__
#define __GST_AUDIO_RESAMPLER_PRIVATE_H__

#include "audio-resampler.h"

/* Contains a collection of all things found in other resamplers:
 * speex (filter construction, optimizations), ffmpeg (fixed phase filter, blackman filter),
 * SRC (linear interpolation, fixed precomputed tables),...
 *
 *  Supports:
 *   - S16, S32, F32 and F64 formats
 *   - nearest, linear and cubic interpolation
 *   - sinc based interpolation with kaiser or blackman-nutall windows
 *   - fully configurable kaiser parameters
 *   - dynamic linear or cubic interpolation of filter table, this can
 *     use less memory but more CPU
 *   - full filter table, generated from optionally linear or cubic
 *     interpolation of filter table
 *   - fixed filter table size with nearest neighbour phase, optionally
 *     using a precomputed tables
 *   - dynamic samplerate changes
 *   - x86 and neon optimizations
 */
typedef void (*ConvertTapsFunc) (gdouble * tmp_taps, gpointer taps,
    gdouble weight, gint n_taps);
typedef void (*InterpolateFunc) (gpointer o, const gpointer a, gint len,
    const gpointer icoeff, gint astride);
typedef void (*ResampleFunc) (GstAudioResampler * resampler, gpointer in[],
    gsize in_len, gpointer out[], gsize out_len, gsize * consumed);
typedef void (*DeinterleaveFunc) (GstAudioResampler * resampler,
    gpointer * sbuf, gpointer in[], gsize in_frames);

struct _GstAudioResampler
{
  GstAudioResamplerMethod method;
  GstAudioResamplerFlags flags;
  GstAudioFormat format;
  GstStructure *options;
  gint format_index;
  gint channels;
  gint in_rate;
  gint out_rate;

  gint bps;
  gint ostride;

  GstAudioResamplerFilterMode filter_mode;
  guint filter_threshold;
  GstAudioResamplerFilterInterpolation filter_interpolation;

  gdouble cutoff;
  gdouble kaiser_beta;
  /* for cubic */
  gdouble b, c;

  /* temp taps */
  gpointer tmp_taps;

  /* oversampled main filter table */
  gint oversample;
  gint n_taps;
  gpointer taps;
  gpointer taps_mem;
  gsize taps_stride;
  gint n_phases;
  gint alloc_taps;
  gint alloc_phases;

  /* cached taps */
  gpointer *cached_phases;
  gpointer cached_taps;
  gpointer cached_taps_mem;
  gsize cached_taps_stride;

  ConvertTapsFunc convert_taps;
  InterpolateFunc interpolate;
  DeinterleaveFunc deinterleave;
  ResampleFunc resample;

  gint blocks;
  gint inc;
  gint samp_inc;
  gint samp_frac;
  gint samp_index;
  gint samp_phase;
  gint skip;

  gpointer samples;
  gsize samples_len;
  gsize samples_avail;
  gpointer *sbuf;
};

#endif /* __GST_AUDIO_RESAMPLER_PRIVATE_H__ */
