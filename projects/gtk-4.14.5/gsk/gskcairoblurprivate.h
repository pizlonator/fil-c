/*
 * Copyright (C) 2012 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License  along  with  this library;  if not,  write to  the Free
 * Software Foundation, Inc., 51  Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 * Original code from Mirco Mueller <mirco.mueller@canonical.com>
 *
 */

#pragma once

#include <gdk/gdk.h>
#include <cairo.h>

G_BEGIN_DECLS

typedef enum {
  GSK_BLUR_NONE = 0,
  GSK_BLUR_X = 1<<0,
  GSK_BLUR_Y = 1<<1,
  GSK_BLUR_REPEAT = 1<<2
} GskBlurFlags;

void            gsk_cairo_blur_surface          (cairo_surface_t *surface,
                                                 double           radius,
						 GskBlurFlags     flags);
int             gsk_cairo_blur_compute_pixels   (double           radius) G_GNUC_CONST;

cairo_t *       gsk_cairo_blur_start_drawing    (cairo_t         *cr,
                                                 float            radius,
                                                 GskBlurFlags     blur_flags);
cairo_t *       gsk_cairo_blur_finish_drawing   (cairo_t         *cr,
                                                 float            radius,
                                                 const GdkRGBA   *color,
                                                 GskBlurFlags     blur_flags);

G_END_DECLS

