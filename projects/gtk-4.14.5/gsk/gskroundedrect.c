/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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
 */

/**
 * GskRoundedRect:
 * @bounds: the bounds of the rectangle
 * @corner: the size of the 4 rounded corners
 *
 * A rectangular region with rounded corners.
 *
 * Application code should normalize rectangles using
 * [method@Gsk.RoundedRect.normalize]; this function will ensure that
 * the bounds of the rectangle are normalized and ensure that the corner
 * values are positive and the corners do not overlap.
 *
 * All functions taking a `GskRoundedRect` as an argument will internally
 * operate on a normalized copy; all functions returning a `GskRoundedRect`
 * will always return a normalized one.
 *
 * The algorithm used for normalizing corner sizes is described in
 * [the CSS specification](https://drafts.csswg.org/css-backgrounds-3/#border-radius).
 */

#include "config.h"

#include "gskroundedrect.h"
#include "gskroundedrectprivate.h"

#include "gskdebugprivate.h"
#include "gskrectprivate.h"

#include <math.h>

static float
gsk_rounded_rect_get_corner_scale_factor (GskRoundedRect *self)
{
  float factor = 1.0;
  float corners;

  corners = self->corner[GSK_CORNER_TOP_LEFT].width + self->corner[GSK_CORNER_TOP_RIGHT].width;
  if (corners > self->bounds.size.width)
    factor = MIN (factor, self->bounds.size.width / corners);

  corners = self->corner[GSK_CORNER_TOP_RIGHT].height + self->corner[GSK_CORNER_BOTTOM_RIGHT].height;
  if (corners > self->bounds.size.height)
    factor = MIN (factor, self->bounds.size.height / corners);

  corners = self->corner[GSK_CORNER_BOTTOM_RIGHT].width + self->corner[GSK_CORNER_BOTTOM_LEFT].width;
  if (corners > self->bounds.size.width)
    factor = MIN (factor, self->bounds.size.width / corners);

  corners = self->corner[GSK_CORNER_TOP_LEFT].height + self->corner[GSK_CORNER_BOTTOM_LEFT].height;
  if (corners > self->bounds.size.height)
    factor = MIN (factor, self->bounds.size.height / corners);

  return factor;
}

static void
gsk_rounded_rect_normalize_in_place (GskRoundedRect *self)
{
  float factor;
  guint i;

  graphene_rect_normalize (&self->bounds);

  for (i = 0; i < 4; i++)
    {
      self->corner[i].width = MAX (self->corner[i].width, 0);
      self->corner[i].height = MAX (self->corner[i].height, 0);
    }

  /* clamp border radius, following CSS specs */
  factor = gsk_rounded_rect_get_corner_scale_factor (self);

  for (i = 0; i < 4; i++)
    graphene_size_scale (&self->corner[i], factor, &self->corner[i]);
}

/**
 * gsk_rounded_rect_init:
 * @self: The `GskRoundedRect` to initialize
 * @bounds: a `graphene_rect_t` describing the bounds
 * @top_left: the rounding radius of the top left corner
 * @top_right: the rounding radius of the top right corner
 * @bottom_right: the rounding radius of the bottom right corner
 * @bottom_left: the rounding radius of the bottom left corner
 *
 * Initializes the given `GskRoundedRect` with the given values.
 *
 * This function will implicitly normalize the `GskRoundedRect`
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
 */
GskRoundedRect *
gsk_rounded_rect_init (GskRoundedRect        *self,
                       const graphene_rect_t *bounds,
                       const graphene_size_t *top_left,
                       const graphene_size_t *top_right,
                       const graphene_size_t *bottom_right,
                       const graphene_size_t *bottom_left)
{
  graphene_rect_init_from_rect (&self->bounds, bounds);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_TOP_LEFT], top_left);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_TOP_RIGHT], top_right);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_BOTTOM_RIGHT], bottom_right);
  graphene_size_init_from_size (&self->corner[GSK_CORNER_BOTTOM_LEFT], bottom_left);

  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_init_copy:
 * @self: a `GskRoundedRect`
 * @src: a `GskRoundedRect`
 *
 * Initializes @self using the given @src rectangle.
 *
 * This function will not normalize the `GskRoundedRect`,
 * so make sure the source is normalized.
 *
 * Returns: (transfer none): the initialized rectangle
 */
GskRoundedRect *
gsk_rounded_rect_init_copy (GskRoundedRect       *self,
                            const GskRoundedRect *src)
{
  *self = *src;

  return self;
}

/**
 * gsk_rounded_rect_init_from_rect:
 * @self: a `GskRoundedRect`
 * @bounds: a `graphene_rect_t`
 * @radius: the border radius
 *
 * Initializes @self to the given @bounds and sets the radius
 * of all four corners to @radius.
 *
 * Returns: (transfer none): the initialized rectangle
 **/
GskRoundedRect *
gsk_rounded_rect_init_from_rect (GskRoundedRect        *self,
                                 const graphene_rect_t *bounds,
                                 float                  radius)
{
  graphene_size_t corner = GRAPHENE_SIZE_INIT(radius, radius);

  return gsk_rounded_rect_init (self, bounds, &corner, &corner, &corner, &corner);
}

/**
 * gsk_rounded_rect_normalize:
 * @self: a `GskRoundedRect`
 *
 * Normalizes the passed rectangle.
 *
 * This function will ensure that the bounds of the rectangle
 * are normalized and ensure that the corner values are positive
 * and the corners do not overlap.
 *
 * Returns: (transfer none): the normalized rectangle
 */
GskRoundedRect *
gsk_rounded_rect_normalize (GskRoundedRect *self)
{
  gsk_rounded_rect_normalize_in_place (self);

  return self;
}

/**
 * gsk_rounded_rect_offset:
 * @self: a `GskRoundedRect`
 * @dx: the horizontal offset
 * @dy: the vertical offset
 *
 * Offsets the bound's origin by @dx and @dy.
 *
 * The size and corners of the rectangle are unchanged.
 *
 * Returns: (transfer none): the offset rectangle
 */
GskRoundedRect *
gsk_rounded_rect_offset (GskRoundedRect *self,
                         float           dx,
                         float           dy)
{
  gsk_rounded_rect_normalize (self);

  self->bounds.origin.x += dx;
  self->bounds.origin.y += dy;

  return self;
}

static inline void
border_radius_shrink (graphene_size_t       *corner,
                      double                 width,
                      double                 height,
                      const graphene_size_t *max)
{
  if (corner->width > 0)
    corner->width -= width;
  if (corner->height > 0)
    corner->height -= height;

  if (corner->width <= 0 || corner->height <= 0)
    {
      corner->width = 0;
      corner->height = 0;
    }
  else
    {
      corner->width = MIN (corner->width, max->width);
      corner->height = MIN (corner->height, max->height);
    }
}

/**
 * gsk_rounded_rect_shrink:
 * @self: The `GskRoundedRect` to shrink or grow
 * @top: How far to move the top side downwards
 * @right: How far to move the right side to the left
 * @bottom: How far to move the bottom side upwards
 * @left: How far to move the left side to the right
 *
 * Shrinks (or grows) the given rectangle by moving the 4 sides
 * according to the offsets given.
 *
 * The corner radii will be changed in a way that tries to keep
 * the center of the corner circle intact. This emulates CSS behavior.
 *
 * This function also works for growing rectangles if you pass
 * negative values for the @top, @right, @bottom or @left.
 *
 * Returns: (transfer none): the resized `GskRoundedRect`
 **/
GskRoundedRect *
gsk_rounded_rect_shrink (GskRoundedRect *self,
                         float           top,
                         float           right,
                         float           bottom,
                         float           left)
{
  float width = left + right;
  float height = top + bottom;

  if (self->bounds.size.width - width < 0)
    {
      self->bounds.origin.x += left * self->bounds.size.width / width;
      self->bounds.size.width = 0;
    }
  else
    {
      self->bounds.origin.x += left;
      self->bounds.size.width -= width;
    }

  if (self->bounds.size.height - height < 0)
    {
      self->bounds.origin.y += top * self->bounds.size.height / height;
      self->bounds.size.height = 0;
    }
  else
    {
      self->bounds.origin.y += top;
      self->bounds.size.height -= height;
    }

  border_radius_shrink (&self->corner[GSK_CORNER_TOP_LEFT], left, top, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_TOP_RIGHT], right, top, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_RIGHT], right, bottom, &self->bounds.size);
  border_radius_shrink (&self->corner[GSK_CORNER_BOTTOM_LEFT], left, bottom, &self->bounds.size);

  return self;
}

void
gsk_rounded_rect_scale_affine (GskRoundedRect       *dest,
                               const GskRoundedRect *src,
                               float                 scale_x,
                               float                 scale_y,
                               float                 dx,
                               float                 dy)
{
  guint flip = ((scale_x < 0) ? 1 : 0) + (scale_y < 0 ? 2 : 0);

  g_assert (dest != src);

  gsk_rect_scale (&src->bounds, scale_x, scale_y, &dest->bounds);
  graphene_rect_offset (&dest->bounds, dx, dy);

  scale_x = fabsf (scale_x);
  scale_y = fabsf (scale_y);

  for (guint i = 0; i < 4; i++)
    {
      dest->corner[i].width = src->corner[i ^ flip].width * scale_x;
      dest->corner[i].height = src->corner[i ^ flip].height * scale_y;
    }
}

/*<private>
 * gsk_rounded_rect_is_circular:
 * @self: the `GskRoundedRect` to check
 *
 * Checks if all corners of @self are quarter-circles (as
 * opposed to quarter-ellipses).
 *
 * Note that different corners can still have different radii.
 *
 * Returns: %TRUE if the rectangle is circular.
 */
gboolean
gsk_rounded_rect_is_circular (const GskRoundedRect *self)
{
  for (guint i = 0; i < 4; i++)
    {
      if (self->corner[i].width != self->corner[i].height)
        return FALSE;
    }

  return TRUE;
}

/**
 * gsk_rounded_rect_is_rectilinear:
 * @self: the `GskRoundedRect` to check
 *
 * Checks if all corners of @self are right angles and the
 * rectangle covers all of its bounds.
 *
 * This information can be used to decide if [ctor@Gsk.ClipNode.new]
 * or [ctor@Gsk.RoundedClipNode.new] should be called.
 *
 * Returns: %TRUE if the rectangle is rectilinear
 **/
gboolean
gsk_rounded_rect_is_rectilinear (const GskRoundedRect *self)
{
  for (guint i = 0; i < 4; i++)
    {
      if (self->corner[i].width > 0 ||
          self->corner[i].height > 0)
        return FALSE;
    }

  return TRUE;
}

static inline gboolean
ellipsis_contains_point (const graphene_size_t  *ellipsis,
                         const graphene_point_t *point)
{
  return (point->x * point->x) / (ellipsis->width * ellipsis->width)
       + (point->y * point->y) / (ellipsis->height * ellipsis->height) <= 1;
}

typedef enum
{
  INSIDE,
  OUTSIDE_TOP_LEFT,
  OUTSIDE_TOP_RIGHT,
  OUTSIDE_BOTTOM_LEFT,
  OUTSIDE_BOTTOM_RIGHT,
  OUTSIDE
} Location;

static Location
gsk_rounded_rect_locate_point (const GskRoundedRect   *self,
                               const graphene_point_t *point)
{
  float px, py;
  float ox, oy;

  ox = self->bounds.origin.x + self->bounds.size.width;
  oy = self->bounds.origin.y + self->bounds.size.height;

  if (point->x < self->bounds.origin.x ||
      point->y < self->bounds.origin.y ||
      point->x > ox ||
      point->y > oy)
    return OUTSIDE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width - point->x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height - point->y;
  if (px > 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_LEFT], &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_TOP_LEFT;

  px = ox - self->corner[GSK_CORNER_TOP_RIGHT].width - point->x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height - point->y;
  if (px < 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_RIGHT], &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_TOP_RIGHT;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width - point->x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_LEFT].height - point->y;
  if (px > 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_LEFT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_BOTTOM_LEFT;

  px = ox - self->corner[GSK_CORNER_BOTTOM_RIGHT].width - point->x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_RIGHT].height - point->y;
  if (px < 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_RIGHT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return OUTSIDE_BOTTOM_RIGHT;

  return INSIDE;
}

/**
 * gsk_rounded_rect_contains_point:
 * @self: a `GskRoundedRect`
 * @point: the point to check
 *
 * Checks if the given @point is inside the rounded rectangle.
 *
 * Returns: %TRUE if the @point is inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_point (const GskRoundedRect   *self,
                                 const graphene_point_t *point)
{
  return gsk_rounded_rect_locate_point (self, point) == INSIDE;
}

/**
 * gsk_rounded_rect_contains_rect:
 * @self: a `GskRoundedRect`
 * @rect: the rectangle to check
 *
 * Checks if the given @rect is contained inside the rounded rectangle.
 *
 * Returns: %TRUE if the @rect is fully contained inside the rounded rectangle
 **/
gboolean
gsk_rounded_rect_contains_rect (const GskRoundedRect  *self,
                                const graphene_rect_t *rect)
{
  float tx, ty;
  float px, py;
  float ox, oy;

  tx = rect->origin.x + rect->size.width;
  ty = rect->origin.y + rect->size.height;
  ox = self->bounds.origin.x + self->bounds.size.width;
  oy = self->bounds.origin.y + self->bounds.size.height;

  if (rect->origin.x < self->bounds.origin.x ||
      rect->origin.y < self->bounds.origin.y ||
      tx > ox ||
      ty > oy)
    return FALSE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width - rect->origin.x;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height - rect->origin.y;
  if (px > 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_LEFT], &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = ox - self->corner[GSK_CORNER_TOP_RIGHT].width - tx;
  py = self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height - rect->origin.y;
  if (px < 0 && py > 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_TOP_RIGHT], &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width - rect->origin.x;
  py = oy - self->corner[GSK_CORNER_BOTTOM_LEFT].height - ty;
  if (px > 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_LEFT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  px = ox - self->corner[GSK_CORNER_BOTTOM_RIGHT].width - tx;
  py = oy - self->corner[GSK_CORNER_BOTTOM_RIGHT].height - ty;
  if (px < 0 && py < 0 &&
      !ellipsis_contains_point (&self->corner[GSK_CORNER_BOTTOM_RIGHT],
                                &GRAPHENE_POINT_INIT (px, py)))
    return FALSE;

  return TRUE;
}

/**
 * gsk_rounded_rect_intersects_rect:
 * @self: a `GskRoundedRect`
 * @rect: the rectangle to check
 *
 * Checks if part of the given @rect is contained inside the rounded rectangle.
 *
 * Returns: %TRUE if the @rect intersects with the rounded rectangle
 */
gboolean
gsk_rounded_rect_intersects_rect (const GskRoundedRect  *self,
                                  const graphene_rect_t *rect)
{
  if (!gsk_rect_intersects (&self->bounds, rect))
    return FALSE;

  /* If the bounding boxes intersect but the rectangles don't,
   * one of the rect's corners must be in the opposite corner's
   * outside region
   */
  if (gsk_rounded_rect_locate_point (self, &rect->origin) == OUTSIDE_BOTTOM_RIGHT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y)) == OUTSIDE_BOTTOM_LEFT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x, rect->origin.y + rect->size.height)) == OUTSIDE_TOP_RIGHT ||
      gsk_rounded_rect_locate_point (self, &GRAPHENE_POINT_INIT (rect->origin.x + rect->size.width, rect->origin.y + rect->size.height)) == OUTSIDE_TOP_LEFT)
    return FALSE;
return TRUE;
}

#define rect_point0(r) ((r)->origin)
#define rect_point1(r) (GRAPHENE_POINT_INIT ((r)->origin.x + (r)->size.width, (r)->origin.y))
#define rect_point2(r) (GRAPHENE_POINT_INIT ((r)->origin.x + (r)->size.width, (r)->origin.y + (r)->size.height))
#define rect_point3(r) (GRAPHENE_POINT_INIT ((r)->origin.x, (r)->origin.y + (r)->size.height))

#define rounded_rect_corner0(r)                                                        \
  (GRAPHENE_RECT_INIT((r)->bounds.origin.x,                                               \
                      (r)->bounds.origin.y,                                               \
                      (r)->corner[0].width, (r)->corner[0].height))
#define rounded_rect_corner1(r) \
  (GRAPHENE_RECT_INIT((r)->bounds.origin.x + (r)->bounds.size.width - (r)->corner[1].width,   \
                      (r)->bounds.origin.y, \
                      (r)->corner[1].width, (r)->corner[1].height))
#define rounded_rect_corner2(r) \
  (GRAPHENE_RECT_INIT((r)->bounds.origin.x + (r)->bounds.size.width - (r)->corner[2].width,   \
                      (r)->bounds.origin.y + (r)->bounds.size.height - (r)->corner[2].height, \
                      (r)->corner[2].width, (r)->corner[2].height))
#define rounded_rect_corner3(r)                                                     \
  (GRAPHENE_RECT_INIT((r)->bounds.origin.x,                                               \
                      (r)->bounds.origin.y + (r)->bounds.size.height - (r)->corner[3].height, \
                      (r)->corner[3].width, (r)->corner[3].height))

enum {
  BELOW,
  INNER,
  ABOVE
};

static inline void
classify_point (const graphene_point_t *p, const graphene_rect_t *rect, int *px, int *py)
{
  if (p->x <= rect->origin.x)
    *px = BELOW;
  else if (p->x >= rect->origin.x + rect->size.width)
    *px = ABOVE;
  else
    *px = INNER;

  if (p->y <= rect->origin.y)
    *py = BELOW;
  else if (p->y >= rect->origin.y + rect->size.height)
    *py = ABOVE;
  else
    *py = INNER;
}

GskRoundedRectIntersection
gsk_rounded_rect_intersect_with_rect (const GskRoundedRect  *self,
                                      const graphene_rect_t *rect,
                                      GskRoundedRect        *result)
{
  int px, py, qx, qy, rx, ry;

  if (!gsk_rect_intersection (&self->bounds, rect, &result->bounds))
    return GSK_INTERSECTION_EMPTY;

  classify_point (&rect_point0 (rect), &rounded_rect_corner0 (self), &px, &py);

  if (px == BELOW && py == BELOW)
    {
      classify_point (&rect_point2 (rect), &rounded_rect_corner0 (self), &qx, &qy);

      if (qx == BELOW || qy == BELOW)
        return GSK_INTERSECTION_EMPTY;
      else if (qx == INNER && qy == INNER &&
               gsk_rounded_rect_locate_point (self, &rect_point2 (rect)) != INSIDE)
        {
          classify_point (&rect_point2 (rect), &rounded_rect_corner2 (self), &rx, &ry);
          if (rx == BELOW || ry == BELOW)
            return GSK_INTERSECTION_EMPTY;
        }
      else if (qx == ABOVE && qy == ABOVE)
        result->corner[0] = self->corner[0];
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else if ((px == INNER || py == INNER) &&
           gsk_rounded_rect_locate_point (self, &rect_point0 (rect)) != INSIDE)
    {
      if (gsk_rounded_rect_locate_point (self, &rect_point2 (rect)) == OUTSIDE_TOP_LEFT)
        return GSK_INTERSECTION_EMPTY;
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else
    result->corner[0].width = result->corner[0].height = 0;

  classify_point (&rect_point1 (rect), &rounded_rect_corner1 (self), &px, &py);

  if (px == ABOVE && py == BELOW)
    {
      classify_point (&rect_point3 (rect), &rounded_rect_corner1 (self), &qx, &qy);

      if (qx == ABOVE || qy == BELOW)
        return GSK_INTERSECTION_EMPTY;
      else if (qx == INNER && qy == INNER &&
               gsk_rounded_rect_locate_point (self, &rect_point3 (rect)) != INSIDE)
        {
          classify_point (&rect_point3 (rect), &rounded_rect_corner3 (self), &rx, &ry);
          if (rx == ABOVE || ry == BELOW)
            return GSK_INTERSECTION_EMPTY;
        }
      else if (qx == BELOW && qy == ABOVE)
        result->corner[1] = self->corner[1];
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else if ((px == INNER || py == INNER) &&
           gsk_rounded_rect_locate_point (self, &rect_point1 (rect)) != INSIDE)
    {
      if (gsk_rounded_rect_locate_point (self, &rect_point3 (rect)) == OUTSIDE_TOP_RIGHT)
        return GSK_INTERSECTION_EMPTY;
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else
    result->corner[1].width = result->corner[1].height = 0;

  classify_point (&rect_point2 (rect), &rounded_rect_corner2 (self), &px, &py);

  if (px == ABOVE && py == ABOVE)
    {
      classify_point (&rect_point0 (rect), &rounded_rect_corner2 (self), &qx, &qy);

      if (qx == ABOVE || qy == ABOVE)
        return GSK_INTERSECTION_EMPTY;
      else if (qx == INNER && qy == INNER &&
               gsk_rounded_rect_locate_point (self, &rect_point0 (rect)) != INSIDE)
        {

          classify_point (&rect_point2 (rect), &rounded_rect_corner0 (self), &rx, &ry);
          if (rx == ABOVE || ry == ABOVE)
            return GSK_INTERSECTION_EMPTY;
        }
      else if (qx == BELOW && qy == BELOW)
        result->corner[2] = self->corner[2];
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else if ((px == INNER || py == INNER) &&
           gsk_rounded_rect_locate_point (self, &rect_point2 (rect)) != INSIDE)
    {
      if (gsk_rounded_rect_locate_point (self, &rect_point0 (rect)) == OUTSIDE_BOTTOM_RIGHT)
        return GSK_INTERSECTION_EMPTY;
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else
    result->corner[2].width = result->corner[2].height = 0;

  classify_point (&rect_point3 (rect), &rounded_rect_corner3 (self), &px, &py);

  if (px == BELOW && py == ABOVE)
    {
      classify_point (&rect_point1 (rect), &rounded_rect_corner3 (self), &qx, &qy);

      if (qx == BELOW || qy == ABOVE)
        return GSK_INTERSECTION_EMPTY;
      else if (qx == INNER && qy == INNER &&
               gsk_rounded_rect_locate_point (self, &rect_point1 (rect)) != INSIDE)
        {
          classify_point (&rect_point1 (rect), &rounded_rect_corner1 (self), &rx, &ry);
          if (rx == BELOW || ry == ABOVE)
            return GSK_INTERSECTION_EMPTY;
        }
      else if (qx == ABOVE && qy == BELOW)
        result->corner[3] = self->corner[3];
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else if ((px == INNER || py == INNER) &&
           gsk_rounded_rect_locate_point (self, &rect_point3 (rect)) != INSIDE)
    {
      if (gsk_rounded_rect_locate_point (self, &rect_point1 (rect)) == OUTSIDE_BOTTOM_LEFT)
        return GSK_INTERSECTION_EMPTY;
      else
        return GSK_INTERSECTION_NOT_REPRESENTABLE;
    }
  else
    result->corner[3].width = result->corner[3].height = 0;

  return GSK_INTERSECTION_NONEMPTY;
}

static gboolean
check_nonintersecting_corner (const GskRoundedRect *out,
                              const GskRoundedRect *in,
                              GskCorner             corner,
                              float                 diff_x,
                              float                 diff_y,
                              GskRoundedRect       *result)
{
  g_assert (diff_x >= 0);
  g_assert (diff_y >= 0);

  if (out->corner[corner].width < diff_x ||
      out->corner[corner].height < diff_y ||
      (out->corner[corner].width <= in->corner[corner].width + diff_x &&
       out->corner[corner].height <= in->corner[corner].height + diff_y))
    {
      result->corner[corner] = in->corner[corner];
      return TRUE;
    }

  if (diff_x > 0 || diff_y > 0)
    return FALSE;

  if (out->corner[corner].width > in->corner[corner].width &&
      out->corner[corner].height > in->corner[corner].height)
    {
      result->corner[corner] = out->corner[corner];
      return TRUE;
    }

  return FALSE;
}

/* a is outside in x direction, b is outside in y direction */
static gboolean
check_intersecting_corner (const GskRoundedRect *a,
                           const GskRoundedRect *b,
                           GskCorner             corner,
                           float                 diff_x,
                           float                 diff_y,
                           GskRoundedRect       *result)
{
  g_assert (diff_x > 0);
  g_assert (diff_y > 0);

  if (diff_x < a->corner[corner].width ||
      diff_x > a->bounds.size.width - a->corner[corner].width - a->corner[OPPOSITE_CORNER_X (corner)].width ||
      diff_y < b->corner[corner].height ||
      diff_y > b->bounds.size.height - b->corner[corner].height - b->corner[OPPOSITE_CORNER_Y (corner)].height)
    return FALSE;

  result->corner[corner] = GRAPHENE_SIZE_INIT (0, 0);
  return TRUE;
}

static gboolean
check_corner (const GskRoundedRect *a,
              const GskRoundedRect *b,
              GskCorner             corner,
              float                 diff_x,
              float                 diff_y,
              GskRoundedRect       *result)
{
  if (diff_x >= 0)
    {
      if (diff_y >= 0)
        {
          return check_nonintersecting_corner (a, b, corner, diff_x, diff_y, result);
        }
      else if (diff_x == 0)
        {
          return check_nonintersecting_corner (b, a, corner, 0, - diff_y, result);
        }
      else
        {
          return check_intersecting_corner (a, b, corner, diff_x, - diff_y, result);
        }
    }
  else
    {
      if (diff_y <= 0)
        {
          return check_nonintersecting_corner (b, a, corner, - diff_x, - diff_y, result);
        }
      else
        {
          return check_intersecting_corner (b, a, corner, - diff_x, diff_y, result);
        }
    }
                    
}

GskRoundedRectIntersection
gsk_rounded_rect_intersection (const GskRoundedRect *a,
                               const GskRoundedRect *b,
                               GskRoundedRect       *result)
{
  float top, left, bottom, right;

  if (!gsk_rect_intersection (&a->bounds, &b->bounds, &result->bounds))
    return GSK_INTERSECTION_EMPTY;

  left = b->bounds.origin.x - a->bounds.origin.x;
  top = b->bounds.origin.y - a->bounds.origin.y;
  right = a->bounds.origin.x + a->bounds.size.width - b->bounds.origin.x - b->bounds.size.width;
  bottom = a->bounds.origin.y + a->bounds.size.height - b->bounds.origin.y - b->bounds.size.height;

  if (check_corner (a, b,
                    GSK_CORNER_TOP_LEFT,
                    left, top,
                    result) &&
      check_corner (a, b,
                    GSK_CORNER_TOP_RIGHT,
                    right, top,
                    result) &&
      check_corner (a, b,
                    GSK_CORNER_BOTTOM_LEFT,
                    left, bottom,
                    result) &&
      check_corner (a, b,
                    GSK_CORNER_BOTTOM_RIGHT,
                    right, bottom,
                    result) &&
      gsk_rounded_rect_get_corner_scale_factor (result) >= 1.0)
    return GSK_INTERSECTION_NONEMPTY;

  return GSK_INTERSECTION_NOT_REPRESENTABLE;
}

static void
append_arc (cairo_t *cr, double angle1, double angle2, gboolean negative)
{
  if (negative)
    cairo_arc_negative (cr, 0.0, 0.0, 1.0, angle1, angle2);
  else
    cairo_arc (cr, 0.0, 0.0, 1.0, angle1, angle2);
}

static void
_cairo_ellipsis (cairo_t *cr,
	         double xc, double yc,
	         double xradius, double yradius,
	         double angle1, double angle2)
{
  cairo_matrix_t save;

  if (xradius <= 0.0 || yradius <= 0.0)
    {
      cairo_line_to (cr, xc, yc);
      return;
    }

  cairo_get_matrix (cr, &save);
  cairo_translate (cr, xc, yc);
  cairo_scale (cr, xradius, yradius);
  append_arc (cr, angle1, angle2, FALSE);
  cairo_set_matrix (cr, &save);
}

void
gsk_rounded_rect_path (const GskRoundedRect *self,
                       cairo_t              *cr)
{
  cairo_new_sub_path (cr);

  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->corner[GSK_CORNER_TOP_LEFT].width,
                   self->bounds.origin.y + self->corner[GSK_CORNER_TOP_LEFT].height,
                   self->corner[GSK_CORNER_TOP_LEFT].width,
                   self->corner[GSK_CORNER_TOP_LEFT].height,
                   G_PI, 3 * G_PI_2);
  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_TOP_RIGHT].width,
                   self->bounds.origin.y + self->corner[GSK_CORNER_TOP_RIGHT].height,
                   self->corner[GSK_CORNER_TOP_RIGHT].width,
                   self->corner[GSK_CORNER_TOP_RIGHT].height,
                   - G_PI_2, 0);
  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->bounds.size.width - self->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   self->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                   self->corner[GSK_CORNER_BOTTOM_RIGHT].height,
                   0, G_PI_2);
  _cairo_ellipsis (cr,
                   self->bounds.origin.x + self->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   self->bounds.origin.y + self->bounds.size.height - self->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   self->corner[GSK_CORNER_BOTTOM_LEFT].width,
                   self->corner[GSK_CORNER_BOTTOM_LEFT].height,
                   G_PI_2, G_PI);

  cairo_close_path (cr);
}

/*< private >
 * Converts to the format we use in our shaders:
 * vec4 rect;
 * vec4 corner_widths;
 * vec4 corner_heights;
 * rect is (x, y, width, height), the corners are the same
 * order as in the rounded rect.
 *
 * This is so that shaders can use just the first vec4 for
 * rectilinear rects, the 2nd vec4 for circular rects and
 * only look at the last vec4 if they have to.
 */
void
gsk_rounded_rect_to_float (const GskRoundedRect   *self,
                           const graphene_point_t *offset,
                           float                   rect[12])
{
  guint i;

  rect[0] = self->bounds.origin.x + offset->x;
  rect[1] = self->bounds.origin.y + offset->y;
  rect[2] = self->bounds.size.width;
  rect[3] = self->bounds.size.height;

  for (i = 0; i < 4; i++)
    {
      rect[4 + i] = self->corner[i].width;
      rect[8 + i] = self->corner[i].height;
    }
}

static inline gboolean
gsk_size_equal (const graphene_size_t *s1,
                const graphene_size_t *s2)
{
  return s1->width == s2->width && s1->height == s2->height;
}

gboolean
gsk_rounded_rect_equal (gconstpointer rect1,
                        gconstpointer rect2)
{
  const GskRoundedRect *self1 = rect1;
  const GskRoundedRect *self2 = rect2;

  return gsk_rect_equal (&self1->bounds, &self2->bounds)
      && gsk_size_equal (&self1->corner[0], &self2->corner[0])
      && gsk_size_equal (&self1->corner[1], &self2->corner[1])
      && gsk_size_equal (&self1->corner[2], &self2->corner[2])
      && gsk_size_equal (&self1->corner[3], &self2->corner[3]);
}

char *
gsk_rounded_rect_to_string (const GskRoundedRect *self)
{
  return g_strdup_printf ("GskRoundedRect %p: Bounds: (%f, %f, %f, %f)"
                          " Corners: (%f, %f) (%f, %f) (%f, %f) (%f, %f)",
                          self,
                          self->bounds.origin.x,
                          self->bounds.origin.y,
                          self->bounds.size.width,
                          self->bounds.size.height,
                          self->corner[0].width,
                          self->corner[0].height,
                          self->corner[1].width,
                          self->corner[1].height,
                          self->corner[2].width,
                          self->corner[2].height,
                          self->corner[3].width,
                          self->corner[3].height);
}

/*
 * gsk_rounded_rect_get_largest_cover:
 * @self: the rounded rect to intersect with
 * @rect: the rectangle to intersect
 * @result: (out caller-allocates): The resulting rectangle
 *
 * Computes the largest rectangle that is fully covered by both
 * the given rect and the rounded rect.  
 * In particular, this function respects corners, so
 *   gsk_rounded_rect_get_largest_cover(self, &self->bounds, &rect)
 * can be used to compute a decomposition for a rounded rect itself.
 **/
void
gsk_rounded_rect_get_largest_cover (const GskRoundedRect  *self,
                                    const graphene_rect_t *rect,
                                    graphene_rect_t       *result)
{
  graphene_rect_t wide, high;
  double start, end;

  wide = self->bounds;
  start = MAX(self->corner[GSK_CORNER_TOP_LEFT].height, self->corner[GSK_CORNER_TOP_RIGHT].height);
  end = MAX(self->corner[GSK_CORNER_BOTTOM_LEFT].height, self->corner[GSK_CORNER_BOTTOM_RIGHT].height);
  wide.size.height -= MIN (wide.size.height, start + end);
  wide.origin.y += start;
  gsk_rect_intersection (&wide, rect, &wide);

  high = self->bounds;
  start = MAX(self->corner[GSK_CORNER_TOP_LEFT].width, self->corner[GSK_CORNER_BOTTOM_LEFT].width);
  end = MAX(self->corner[GSK_CORNER_TOP_RIGHT].width, self->corner[GSK_CORNER_BOTTOM_RIGHT].width);
  high.size.width -= MIN (high.size.width, start + end);
  high.origin.x += start;
  gsk_rect_intersection (&high, rect, &high);

  if (wide.size.width * wide.size.height > high.size.width * high.size.height)
    *result = wide;
  else
    *result = high;
}

