/* gskgltexture.c
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

#include "config.h"

#include <gdk/gdktextureprivate.h>

#include "gskgltextureprivate.h"
#include "ninesliceprivate.h"

void
gsk_gl_texture_free (GskGLTexture *texture)
{
  if (texture != NULL)
    {
      g_assert (texture->link.prev == NULL);
      g_assert (texture->link.next == NULL);

      if (texture->user)
        g_clear_pointer (&texture->user, gdk_texture_clear_render_data);

      if (texture->texture_id != 0)
        {
          glDeleteTextures (1, &texture->texture_id);
          texture->texture_id = 0;
        }

      for (guint i = 0; i < texture->n_slices; i++)
        {
          glDeleteTextures (1, &texture->slices[i].texture_id);
          texture->slices[i].texture_id = 0;
        }

      g_clear_pointer (&texture->slices, g_free);
      g_clear_pointer (&texture->nine_slice, g_free);

      g_free (texture);
    }
}

GskGLTexture *
gsk_gl_texture_new (guint  texture_id,
                    int    width,
                    int    height,
                    gint64 frame_id)
{
  GskGLTexture *texture;

  texture = g_new0 (GskGLTexture, 1);
  texture->texture_id = texture_id;
  texture->link.data = texture;
  texture->width = width;
  texture->height = height;
  texture->last_used_in_frame = frame_id;

  return texture;
}

const GskGLTextureNineSlice *
gsk_gl_texture_get_nine_slice (GskGLTexture         *texture,
                               const GskRoundedRect *outline,
                               float                 extra_pixels_x,
                               float                 extra_pixels_y)
{
  g_assert (texture != NULL);
  g_assert (outline != NULL);

  if G_UNLIKELY (texture->nine_slice == NULL)
    {
      texture->nine_slice = g_new0 (GskGLTextureNineSlice, 9);

      nine_slice_rounded_rect (texture->nine_slice, outline);
      nine_slice_grow (texture->nine_slice, extra_pixels_x, extra_pixels_y);
      nine_slice_to_texture_coords (texture->nine_slice, texture->width, texture->height);
    }

  return texture->nine_slice;
}
