/* ninesliceprivate.h
 *
 * Copyright 2017 Timm Bäder <mail@baedert.org>
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gskgltextureprivate.h"

#if 0
# define DEBUG_NINE_SLICE
#endif

G_BEGIN_DECLS

enum {
  NINE_SLICE_TOP_LEFT      = 0,
  NINE_SLICE_TOP_CENTER    = 1,
  NINE_SLICE_TOP_RIGHT     = 2,
  NINE_SLICE_LEFT_CENTER   = 3,
  NINE_SLICE_CENTER        = 4,
  NINE_SLICE_RIGHT_CENTER  = 5,
  NINE_SLICE_BOTTOM_LEFT   = 6,
  NINE_SLICE_BOTTOM_CENTER = 7,
  NINE_SLICE_BOTTOM_RIGHT  = 8,
};

static inline bool G_GNUC_PURE
nine_slice_is_visible (const GskGLTextureNineSlice *slice)
{
  return slice->rect.width > 0 && slice->rect.height > 0;
}

static inline void
nine_slice_rounded_rect (GskGLTextureNineSlice *slices,
                         const GskRoundedRect  *rect)
{
  const graphene_point_t *origin = &rect->bounds.origin;
  const graphene_size_t *size = &rect->bounds.size;
  int top_height = ceilf (MAX (rect->corner[GSK_CORNER_TOP_LEFT].height,
                               rect->corner[GSK_CORNER_TOP_RIGHT].height));
  int bottom_height = ceilf (MAX (rect->corner[GSK_CORNER_BOTTOM_LEFT].height,
                                  rect->corner[GSK_CORNER_BOTTOM_RIGHT].height));
  int right_width = ceilf (MAX (rect->corner[GSK_CORNER_TOP_RIGHT].width,
                                rect->corner[GSK_CORNER_BOTTOM_RIGHT].width));
  int left_width = ceilf (MAX (rect->corner[GSK_CORNER_TOP_LEFT].width,
                               rect->corner[GSK_CORNER_BOTTOM_LEFT].width));

  /* Top left */
  slices[0].rect.x = origin->x;
  slices[0].rect.y = origin->y;
  slices[0].rect.width = left_width;
  slices[0].rect.height = top_height;

  /* Top center */
  slices[1].rect.x = origin->x + size->width / 2.0 - 0.5;
  slices[1].rect.y = origin->y;
  slices[1].rect.width = 1;
  slices[1].rect.height = top_height;

  /* Top right */
  slices[2].rect.x = origin->x + size->width - right_width;
  slices[2].rect.y = origin->y;
  slices[2].rect.width = right_width;
  slices[2].rect.height = top_height;

  /* Left center */
  slices[3].rect.x = origin->x;
  slices[3].rect.y = origin->y + size->height / 2;
  slices[3].rect.width = left_width;
  slices[3].rect.height = 1;

  /* center */
  slices[4].rect.x = origin->x + size->width / 2.0 - 0.5;
  slices[4].rect.y = origin->y + size->height / 2.0 - 0.5;
  slices[4].rect.width = 1;
  slices[4].rect.height = 1;

  /* Right center */
  slices[5].rect.x = origin->x + size->width - right_width;
  slices[5].rect.y = origin->y + (size->height / 2.0) - 0.5;
  slices[5].rect.width = right_width;
  slices[5].rect.height = 1;

  /* Bottom Left */
  slices[6].rect.x = origin->x;
  slices[6].rect.y = origin->y + size->height - bottom_height;
  slices[6].rect.width = left_width;
  slices[6].rect.height = bottom_height;

  /* Bottom center */
  slices[7].rect.x = origin->x + (size->width / 2.0) - 0.5;
  slices[7].rect.y = origin->y + size->height - bottom_height;
  slices[7].rect.width = 1;
  slices[7].rect.height = bottom_height;

  /* Bottom right */
  slices[8].rect.x = origin->x + size->width - right_width;
  slices[8].rect.y = origin->y + size->height - bottom_height;
  slices[8].rect.width = right_width;
  slices[8].rect.height = bottom_height;

#ifdef DEBUG_NINE_SLICE
  /* These only hold true when the values from ceilf() above
   * are greater than one. Otherwise they fail, like will happen
   * with the node editor viewing the textures zoomed out.
   */
  if (size->width > 1)
    g_assert_cmpfloat (size->width, >=, left_width + right_width);
  if (size->height > 1)
  g_assert_cmpfloat (size->height, >=, top_height + bottom_height);
#endif
}

static inline void
nine_slice_to_texture_coords (GskGLTextureNineSlice *slices,
                              int                    texture_width,
                              int                    texture_height)
{
  float fw = texture_width;
  float fh = texture_height;

  for (guint i = 0; i < 9; i++)
    {
      GskGLTextureNineSlice *slice = &slices[i];

      slice->area.x = slice->rect.x / fw;
      slice->area.y = 1.0 - ((slice->rect.y + slice->rect.height) / fh);
      slice->area.x2 = ((slice->rect.x + slice->rect.width) / fw);
      slice->area.y2 = (1.0 - (slice->rect.y / fh));

#ifdef DEBUG_NINE_SLICE
      g_assert_cmpfloat (slice->area.x, >=, 0);
      g_assert_cmpfloat (slice->area.x, <=, 1);
      g_assert_cmpfloat (slice->area.y, >=, 0);
      g_assert_cmpfloat (slice->area.y, <=, 1);
      g_assert_cmpfloat (slice->area.x2, >, slice->area.x);
      g_assert_cmpfloat (slice->area.y2, >, slice->area.y);
#endif
    }
}

static inline void
nine_slice_grow (GskGLTextureNineSlice *slices,
                 int                    amount_x,
                 int                    amount_y)
{
  if (amount_x == 0 && amount_y == 0)
    return;

  /* top left */
  slices[0].rect.x -= amount_x;
  slices[0].rect.y -= amount_y;
  if (amount_x > slices[0].rect.width)
    slices[0].rect.width += amount_x * 2;
  else
    slices[0].rect.width += amount_x;

  if (amount_y > slices[0].rect.height)
    slices[0].rect.height += amount_y * 2;
  else
    slices[0].rect.height += amount_y;


  /* Top center */
  slices[1].rect.y -= amount_y;
  if (amount_y > slices[1].rect.height)
    slices[1].rect.height += amount_y * 2;
  else
    slices[1].rect.height += amount_y;

  /* top right */
  slices[2].rect.y -= amount_y;
  if (amount_x > slices[2].rect.width)
    {
      slices[2].rect.x -= amount_x;
      slices[2].rect.width += amount_x * 2;
    }
  else
    {
     slices[2].rect.width += amount_x;
    }

  if (amount_y > slices[2].rect.height)
    slices[2].rect.height += amount_y * 2;
  else
    slices[2].rect.height += amount_y;



  slices[3].rect.x -= amount_x;
  if (amount_x > slices[3].rect.width)
    slices[3].rect.width += amount_x * 2;
  else
    slices[3].rect.width += amount_x;

  /* Leave center alone */

  if (amount_x > slices[5].rect.width)
    {
      slices[5].rect.x -= amount_x;
      slices[5].rect.width += amount_x * 2;
    }
  else
    {
      slices[5].rect.width += amount_x;
    }


  /* Bottom left */
  slices[6].rect.x -= amount_x;
  if (amount_x > slices[6].rect.width)
    {
      slices[6].rect.width += amount_x * 2;
    }
  else
    {
      slices[6].rect.width += amount_x;
    }

  if (amount_y > slices[6].rect.height)
    {
      slices[6].rect.y -= amount_y;
      slices[6].rect.height += amount_y * 2;
    }
  else
    {
      slices[6].rect.height += amount_y;
    }


  /* Bottom center */
  if (amount_y > slices[7].rect.height)
    {
      slices[7].rect.y -= amount_y;
      slices[7].rect.height += amount_y * 2;
    }
  else
    {
      slices[7].rect.height += amount_y;
    }

  if (amount_x > slices[8].rect.width)
    {
      slices[8].rect.x -= amount_x;
      slices[8].rect.width += amount_x * 2;
    }
  else
    {
      slices[8].rect.width += amount_x;
    }

  if (amount_y > slices[8].rect.height)
    {
      slices[8].rect.y -= amount_y;
      slices[8].rect.height += amount_y * 2;
    }
  else
    {
      slices[8].rect.height += amount_y;
    }

#ifdef DEBUG_NINE_SLICE
  /* These cannot be relied on in all cases right now, specifically
   * when viewing data zoomed out.
   */
  for (guint i = 0; i < 9; i ++)
    {
      g_assert_cmpint (slices[i].rect.x, >=, 0);
      g_assert_cmpint (slices[i].rect.y, >=, 0);
      g_assert_cmpint (slices[i].rect.width, >=, 0);
      g_assert_cmpint (slices[i].rect.height, >=, 0);
    }

  /* Rows don't overlap */
  for (guint i = 0; i < 3; i++)
    {
      int lhs = slices[i * 3 + 0].rect.x + slices[i * 3 + 0].rect.width;
      int rhs = slices[i * 3 + 1].rect.x;

      /* Ignore the case where we are scaled out and the
       * positioning is degenerate, such as from node-editor.
       */
      if (rhs > 1)
        g_assert_cmpint (lhs, <, rhs);
    }
#endif

}

G_END_DECLS

