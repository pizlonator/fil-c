/* 
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef __GST_GL_EFFECTS_TEXTURES_H__
#define __GST_GL_EFFECTS_TEXTURES_H__

#include <glib.h>

struct _GstGLEffectsCurve {
  guint 	 width;
  guint 	 height;
  guint 	 bytes_per_pixel; /* 3:RGB */ 
  guint8	 pixel_data[256 * 1 * 3 + 1];
};

typedef struct _GstGLEffectsCurve GstGLEffectsCurve;

/* CURVE for the heat signature effect */
extern const GstGLEffectsCurve xpro_curve;

extern const GstGLEffectsCurve luma_xpro_curve;

/* CURVE for the heat signature effect */
extern const GstGLEffectsCurve heat_curve;

extern const GstGLEffectsCurve sepia_curve;

extern const GstGLEffectsCurve xray_curve;

#endif
