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

#ifndef AUDIO_RESAMPLER_X86_SSE_H
#define AUDIO_RESAMPLER_X86_SSE_H

#include "audio-resampler-macros.h"

DECL_RESAMPLE_FUNC (gfloat, full, 1, sse);
DECL_RESAMPLE_FUNC (gfloat, linear, 1, sse);
DECL_RESAMPLE_FUNC (gfloat, cubic, 1, sse);

void interpolate_gfloat_linear_sse (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

void interpolate_gfloat_cubic_sse (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride);

#endif /* AUDIO_RESAMPLER_X86_SSE_H */
