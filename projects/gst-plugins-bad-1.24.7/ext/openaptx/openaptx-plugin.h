/* GStreamer openaptx audio plugin
 *
 * Copyright (C) 2020 Igor V. Kovalenko <igor.v.kovalenko@gmail.com>
 * Copyright (C) 2020 Thomas Weißschuh <thomas@t-8ch.de>
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
#ifndef __GST_OPENAPTX_PLUGIN_H__
#define __GST_OPENAPTX_PLUGIN_H__

#include <glib.h>

#define APTX_HD_DEFAULT 1
#define APTX_AUTOSYNC_DEFAULT TRUE

#define APTX_LATENCY_SAMPLES 90

/* always stereo */
#define APTX_NUM_CHANNELS 2

/* always S24LE */
#define APTX_SAMPLE_SIZE 3

/* always 4 samples per channel*/
#define APTX_SAMPLES_PER_CHANNEL 4

/* always 4 stereo samples */
#define APTX_SAMPLES_PER_FRAME (APTX_SAMPLES_PER_CHANNEL * APTX_NUM_CHANNELS)

/* fixed encoded frame size hd=0: LLRR, hd=1: LLLRRR */
#define APTX_FRAME_SIZE    (2 * APTX_NUM_CHANNELS)
#define APTX_HD_FRAME_SIZE (3 * APTX_NUM_CHANNELS)

/* while finishing encoding, up to 92 frames will be produced */
#define APTX_FINISH_FRAMES 92

static inline const char* aptx_name(gboolean hd)
{
    return hd ? "aptX-HD" : "aptX";
}

/* fixed encoded frame size hd=FALSE: LLRR, hd=TRUE: LLLRRR */
static inline gsize aptx_frame_size(gboolean hd)
{
  return hd ? APTX_HD_FRAME_SIZE : APTX_FRAME_SIZE;
}

#endif /* __GST_OPENAPTX_PLUGIN_H__ */
