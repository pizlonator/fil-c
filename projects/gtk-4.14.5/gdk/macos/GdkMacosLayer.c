/* GdkMacosLayer.c
 *
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#import "GdkMacosLayer.h"
#import "GdkMacosTile.h"

@protocol CanSetContentsOpaque
- (void)setContentsOpaque:(BOOL)mask;
@end

@implementation GdkMacosLayer

#define TILE_MAX_SIZE 128
#define TILE_EDGE_MAX_SIZE 512

static CGAffineTransform flipTransform;
static gboolean hasFlipTransform;

typedef struct
{
  GdkMacosTile          *tile;
  cairo_rectangle_int_t  cr_area;
  CGRect                 area;
  guint                  opaque : 1;
} TileInfo;

typedef struct
{
  const cairo_region_t *region;
  guint n_rects;
  guint iter;
  cairo_rectangle_int_t rect;
  cairo_rectangle_int_t stash;
  guint finished : 1;
} Tiler;

static void
tiler_init (Tiler                *tiler,
            const cairo_region_t *region)
{
  memset (tiler, 0, sizeof *tiler);

  if (region == NULL)
    {
      tiler->finished = TRUE;
      return;
    }

  tiler->region = region;
  tiler->n_rects = cairo_region_num_rectangles (region);

  if (tiler->n_rects > 0)
    cairo_region_get_rectangle (region, 0, &tiler->rect);
  else
    tiler->finished = TRUE;
}

static gboolean
tiler_next (Tiler                 *tiler,
            cairo_rectangle_int_t *tile,
            int                    max_size)
{
  if (tiler->finished)
    return FALSE;

  if (tiler->rect.width == 0 || tiler->rect.height == 0)
    {
      tiler->iter++;

      if (tiler->iter >= tiler->n_rects)
        {
          tiler->finished = TRUE;
          return FALSE;
        }

      cairo_region_get_rectangle (tiler->region, tiler->iter, &tiler->rect);
    }

  /* If the next rectangle is too tall, slice the bottom off to
   * leave just the height we want into tiler->stash.
   */
  if (tiler->rect.height > max_size)
    {
      tiler->stash = tiler->rect;
      tiler->stash.y += max_size;
      tiler->stash.height -= max_size;
      tiler->rect.height = max_size;
    }

  /* Now we can take the next horizontal slice */
  tile->x = tiler->rect.x;
  tile->y = tiler->rect.y;
  tile->height = tiler->rect.height;
  tile->width = MIN (max_size, tiler->rect.width);

  tiler->rect.x += tile->width;
  tiler->rect.width -= tile->width;

  if (tiler->rect.width == 0)
    {
      tiler->rect = tiler->stash;
      tiler->stash.width = tiler->stash.height = 0;
    }

  return TRUE;
}

static inline CGRect
toCGRect (const cairo_rectangle_int_t *rect)
{
  return CGRectMake (rect->x, rect->y, rect->width, rect->height);
}

static inline cairo_rectangle_int_t
fromCGRect (const CGRect rect)
{
  return (cairo_rectangle_int_t) {
    rect.origin.x,
    rect.origin.y,
    rect.size.width,
    rect.size.height
  };
}

-(id)init
{
  if (!hasFlipTransform)
    {
      hasFlipTransform = TRUE;
      flipTransform = CGAffineTransformMakeScale (1, -1);
    }

  self = [super init];

  if (self == NULL)
    return NULL;

  self->_layoutInvalid = TRUE;

  [self setContentsGravity:kCAGravityCenter];
  [self setContentsScale:1.0f];
  [self setGeometryFlipped:YES];

  return self;
}

-(BOOL)isOpaque
{
  return NO;
}

-(void)_applyLayout:(GArray *)tiles
{
  CGAffineTransform transform;
  GArray *prev;
  gboolean exhausted;
  guint j = 0;

  if (self->_isFlipped)
    transform = flipTransform;
  else
    transform = CGAffineTransformIdentity;

  prev = g_steal_pointer (&self->_tiles);
  self->_tiles = tiles;
  exhausted = prev == NULL;

  /* Try to use existing CALayer to avoid creating new layers
   * as that can be rather expensive.
   */
  for (guint i = 0; i < tiles->len; i++)
    {
      TileInfo *info = &g_array_index (tiles, TileInfo, i);

      if (!exhausted)
        {
          TileInfo *other = NULL;

          for (; j < prev->len; j++)
            {
              other = &g_array_index (prev, TileInfo, j);

              if (other->opaque == info->opaque)
                {
                  j++;
                  break;
                }

              other = NULL;
            }

          if (other != NULL)
            {
              info->tile = g_steal_pointer (&other->tile);
              [info->tile setFrame:info->area];
              [info->tile setAffineTransform:transform];
              continue;
            }
        }

      info->tile = [GdkMacosTile layer];

      [info->tile setAffineTransform:transform];
      [info->tile setContentsScale:1.0f];
      [info->tile setOpaque:info->opaque];
      [(id<CanSetContentsOpaque>)info->tile setContentsOpaque:info->opaque];
      [info->tile setFrame:info->area];

      [self addSublayer:info->tile];
    }

  /* Release all of our old layers */
  if (prev != NULL)
    {
      for (guint i = 0; i < prev->len; i++)
        {
          TileInfo *info = &g_array_index (prev, TileInfo, i);

          if (info->tile != NULL)
            [info->tile removeFromSuperlayer];
        }

      g_array_unref (prev);
    }
}

-(void)layoutSublayers
{
  Tiler tiler;
  GArray *ar;
  cairo_region_t *transparent;
  cairo_rectangle_int_t rect;
  int max_size;

  if (!self->_inSwapBuffer)
    return;

  self->_layoutInvalid = FALSE;

  ar = g_array_sized_new (FALSE, FALSE, sizeof (TileInfo), 32);

  rect = fromCGRect ([self bounds]);
  rect.x = rect.y = 0;

  /* Calculate the transparent region (edges usually) */
  transparent = cairo_region_create_rectangle (&rect);
  if (self->_opaqueRegion)
    cairo_region_subtract (transparent, self->_opaqueRegion);

  self->_opaque = cairo_region_is_empty (transparent);

  /* If we have transparent borders around the opaque region, then
   * we are okay with a bit larger tiles since they don't change
   * all that much and are generally small in width.
   */
  if (!self->_opaque &&
      self->_opaqueRegion &&
      !cairo_region_is_empty (self->_opaqueRegion))
    max_size = TILE_EDGE_MAX_SIZE;
  else
    max_size = TILE_MAX_SIZE;

  /* Track transparent children */
  tiler_init (&tiler, transparent);
  while (tiler_next (&tiler, &rect, max_size))
    {
      TileInfo *info;

      g_array_set_size (ar, ar->len+1);

      info = &g_array_index (ar, TileInfo, ar->len-1);
      info->tile = NULL;
      info->opaque = FALSE;
      info->cr_area = rect;
      info->area = toCGRect (&info->cr_area);
    }

  /* Track opaque children */
  tiler_init (&tiler, self->_opaqueRegion);
  while (tiler_next (&tiler, &rect, TILE_MAX_SIZE))
    {
      TileInfo *info;

      g_array_set_size (ar, ar->len+1);

      info = &g_array_index (ar, TileInfo, ar->len-1);
      info->tile = NULL;
      info->opaque = TRUE;
      info->cr_area = rect;
      info->area = toCGRect (&info->cr_area);
    }

  cairo_region_destroy (transparent);

  [self _applyLayout:g_steal_pointer (&ar)];
  [super layoutSublayers];
}

-(void)setFrame:(NSRect)frame
{
  if (frame.size.width != self.bounds.size.width ||
      frame.size.height != self.bounds.size.height)
    {
      self->_layoutInvalid = TRUE;
      [self setNeedsLayout];
    }

  [super setFrame:frame];
}

-(void)setOpaqueRegion:(const cairo_region_t *)opaqueRegion
{
  g_clear_pointer (&self->_opaqueRegion, cairo_region_destroy);
  self->_opaqueRegion = cairo_region_copy (opaqueRegion);
  self->_layoutInvalid = TRUE;

  [self setNeedsLayout];
}

-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage
{
  IOSurfaceRef ioSurface = _gdk_macos_buffer_get_native (buffer);
  gboolean flipped = _gdk_macos_buffer_get_flipped (buffer);
  double scale = _gdk_macos_buffer_get_device_scale (buffer);
  double width = _gdk_macos_buffer_get_width (buffer) / scale;
  double height = _gdk_macos_buffer_get_height (buffer) / scale;

  if (flipped != self->_isFlipped)
    {
      self->_isFlipped = flipped;
      self->_layoutInvalid = TRUE;
    }

  if (self->_layoutInvalid)
    {
      self->_inSwapBuffer = TRUE;
      [self layoutSublayers];
      self->_inSwapBuffer = FALSE;
    }

  if (self->_tiles == NULL)
    return;

  for (guint i = 0; i < self->_tiles->len; i++)
    {
      const TileInfo *info = &g_array_index (self->_tiles, TileInfo, i);
      cairo_region_overlap_t overlap;
      CGRect area;

      overlap = cairo_region_contains_rectangle (damage, &info->cr_area);
      if (overlap == CAIRO_REGION_OVERLAP_OUT)
        continue;

      area.origin.x = info->area.origin.x / width;
      area.size.width = info->area.size.width / width;
      area.size.height = info->area.size.height / height;

      if (flipped)
        area.origin.y = (height - info->area.origin.y - info->area.size.height) / height;
      else
        area.origin.y = info->area.origin.y / height;

      [info->tile swapBuffer:ioSurface withRect:area];
    }
}

@end
