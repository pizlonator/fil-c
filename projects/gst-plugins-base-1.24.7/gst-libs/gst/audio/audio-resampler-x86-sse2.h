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

#ifndef AUDIO_RESAMPLER_X86_SSE2_H
#define AUDIO_RESAMPLER_X86_SSE2_H

#include "audio-resampler-macros.h"

DECL_RESAMPLE_FUNC (gint16, full, 1, sse2);
DECL_RESAMPLE_FUNC (gint16, linear, 1, sse2);
DECL_RESAMPLE_FUNC (gint16, cubic, 1, sse2);

DECL_RESAMPLE_FUNC (gdouble, full, 1, sse2);
DECL_RESAMPLE_FUNC (gdouble, linear, 1, sse2);
DECL_RESAMPLE_FUNC (gdouble, cubic, 1, sse2);

void
interpolate_gint16_linear_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

void
interpolate_gint16_cubic_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

void
interpolate_gdouble_linear_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

void
interpolate_gdouble_cubic_sse2 (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

#endif /* AUDIO_RESAMPLER_X86_SSE2_H */
