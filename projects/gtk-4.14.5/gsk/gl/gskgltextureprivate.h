/* gskgltextureprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gskgltypesprivate.h"

G_BEGIN_DECLS

struct _GskGLTextureSlice
{
  cairo_rectangle_int_t rect;
  struct {
    float x;
    float y;
    float x2;
    float y2;
  } area;
  guint texture_id;
};

struct _GskGLTextureNineSlice
{
  cairo_rectangle_int_t rect;
  struct {
    float x;
    float y;
    float x2;
    float y2;
  } area;
};

struct _GskGLTexture
{
  /* Used to insert into queue */
  GList link;

  /* Identifier of the frame that created it */
  gint64 last_used_in_frame;

  /* Backpointer to texture (can be cleared asynchronously) */
  GdkTexture *user;

  /* Only used by nine-slice textures */
  GskGLTextureNineSlice *nine_slice;

  /* Only used by sliced textures */
  GskGLTextureSlice *slices;
  guint n_slices;

  /* The actual GL texture identifier in some shared context */
  guint texture_id;

  int width;
  int height;

  /* Set when used by an atlas so we don't drop the texture */
  guint              permanent : 1;
  /* we are allowed to call glGenerateMipmap() for this texture */
  guint              can_mipmap : 1;
  /* we called glGenerateMipmap() for this texture */
  guint              has_mipmap : 1;
};

GskGLTexture                * gsk_gl_texture_new            (guint                 texture_id,
                                                             int                   width,
                                                             int                   height,
                                                             gint64                frame_id);
const GskGLTextureNineSlice * gsk_gl_texture_get_nine_slice (GskGLTexture         *texture,
                                                             const GskRoundedRect *outline,
                                                             float                 extra_pixels_x,
                                                             float                 extra_pixels_y);
void                          gsk_gl_texture_free           (GskGLTexture         *texture);

G_END_DECLS

