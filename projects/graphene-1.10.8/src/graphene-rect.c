/* graphene-rect.c: Rectangular type
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * SECTION:graphene-rect
 * @title: Rectangle
 * @short_description: Rectangular shape type
 *
 * #graphene_rect_t is a type representing a rectangle through an origin
 * #graphene_point_t point and a #graphene_size_t size.
 *
 * ![](rectangle.png)
 *
 * Operations on a #graphene_rect_t will normalize the rectangle, to
 * ensure that the origin is always the top-left corner of the rectangle
 * and that the size is always positive.
 */

#include "graphene-private.h"

#include "graphene-rect.h"

#include <math.h>

/*< private >
 * graphene_rect_normalize_in_place:
 * @r: (inout): a #graphene_rect_t
 *
 * Normalizes the passed #graphene_rect_t.
 */
static inline void
graphene_rect_normalize_in_place (graphene_rect_t *r)
{
  if (r->size.width < 0.f)
    {
      float size = fabsf (r->size.width);

      r->origin.x -= size;
      r->size.width = size;
    }

  if (r->size.height < 0.f)
    {
      float size = fabsf (r->size.height);

      r->origin.y -= size;
      r->size.height = size;
    }
}

/**
 * graphene_rect_alloc:
 *
 * Allocates a new #graphene_rect_t.
 *
 * The contents of the returned rectangle are undefined.
 *
 * Returns: (transfer full): the newly allocated rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_alloc (void)
{
  return calloc (1, sizeof (graphene_rect_t));
}

/**
 * graphene_rect_free:
 * @r: a #graphene_rect_t
 *
 * Frees the resources allocated by graphene_rect_alloc().
 *
 * Since: 1.0
 */
void
graphene_rect_free (graphene_rect_t *r)
{
  free (r);
}

/**
 * graphene_rect_init:
 * @r: a #graphene_rect_t
 * @x: the X coordinate of the @graphene_rect_t.origin
 * @y: the Y coordinate of the @graphene_rect_t.origin
 * @width: the width of the @graphene_rect_t.size
 * @height: the height of the @graphene_rect_t.size
 *
 * Initializes the given #graphene_rect_t with the given values.
 *
 * This function will implicitly normalize the #graphene_rect_t
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_init (graphene_rect_t *r,
                    float            x,
                    float            y,
                    float            width,
                    float            height)
{
  graphene_point_init (&r->origin, x, y);
  graphene_size_init (&r->size, width, height);

  graphene_rect_normalize_in_place (r);

  return r;
}

/**
 * graphene_rect_init_from_rect:
 * @r: a #graphene_rect_t
 * @src: a #graphene_rect_t
 *
 * Initializes @r using the given @src rectangle.
 *
 * This function will implicitly normalize the #graphene_rect_t
 * before returning.
 *
 * Returns: (transfer none): the initialized rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_init_from_rect (graphene_rect_t       *r,
                              const graphene_rect_t *src)
{
  *r = *src;

  graphene_rect_normalize_in_place (r);

  return r;
}

static bool
rect_equal (const void *p1,
            const void *p2)
{
  const graphene_rect_t *a = p1;
  const graphene_rect_t *b = p2;

  graphene_rect_t r_a, r_b;

  graphene_rect_normalize_r (a, &r_a);
  graphene_rect_normalize_r (b, &r_b);

  return graphene_point_equal (&r_a.origin, &r_b.origin) &&
         graphene_size_equal (&r_a.size, &r_b.size);
}

/**
 * graphene_rect_equal:
 * @a: a #graphene_rect_t
 * @b: a #graphene_rect_t
 *
 * Checks whether the two given rectangle are equal.
 *
 * Returns: `true` if the rectangles are equal
 *
 * Since: 1.0
 */
bool
graphene_rect_equal (const graphene_rect_t *a,
                     const graphene_rect_t *b)
{
  return graphene_pointer_equal (a, b, rect_equal);
}

/**
 * graphene_rect_normalize:
 * @r: a #graphene_rect_t
 *
 * Normalizes the passed rectangle.
 *
 * This function ensures that the size of the rectangle is made of
 * positive values, and that the origin is the top-left corner of
 * the rectangle.
 *
 * Returns: (transfer none): the normalized rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_normalize (graphene_rect_t *r)
{
  graphene_rect_normalize_in_place (r);

  return r;
}

/**
 * graphene_rect_normalize_r:
 * @r: a #graphene_rect_t
 * @res: (out caller-allocates): the return location for the
 *   normalized rectangle
 *
 * Normalizes the passed rectangle.
 *
 * This function ensures that the size of the rectangle is made of
 * positive values, and that the origin is in the top-left corner
 * of the rectangle.
 *
 * Since: 1.4
 */
void
graphene_rect_normalize_r (const graphene_rect_t *r,
                           graphene_rect_t       *res)
{
  if (res != r)
    *res = *r;

  graphene_rect_normalize_in_place (res);
}

/**
 * graphene_rect_get_center:
 * @r: a #graphene_rect_t
 * @p: (out caller-allocates): return location for a #graphene_point_t
 *
 * Retrieves the coordinates of the center of the given rectangle.
 *
 * Since: 1.0
 */
void
graphene_rect_get_center (const graphene_rect_t *r,
                          graphene_point_t      *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  graphene_point_init (p,
                       rr.origin.x + (rr.size.width / 2.f),
                       rr.origin.y + (rr.size.height / 2.f));
}

/**
 * graphene_rect_get_top_left:
 * @r: a #graphene_rect_t
 * @p: (out caller-allocates): return location for a #graphene_point_t
 *
 * Retrieves the coordinates of the top-left corner of the given rectangle.
 *
 * Since: 1.0
 */
void
graphene_rect_get_top_left (const graphene_rect_t *r,
                            graphene_point_t      *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  graphene_point_init_from_point (p, &rr.origin);
}

/**
 * graphene_rect_get_top_right:
 * @r: a #graphene_rect_t
 * @p: (out caller-allocates): return location for a #graphene_point_t
 *
 * Retrieves the coordinates of the top-right corner of the given rectangle.
 *
 * Since: 1.0
 */
void
graphene_rect_get_top_right (const graphene_rect_t *r,
                             graphene_point_t      *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  graphene_point_init (p, rr.origin.x + rr.size.width, rr.origin.y);
}

/**
 * graphene_rect_get_bottom_left:
 * @r: a #graphene_rect_t
 * @p: (out caller-allocates): return location for a #graphene_point_t
 *
 * Retrieves the coordinates of the bottom-left corner of the given rectangle.
 *
 * Since: 1.0
 */
void
graphene_rect_get_bottom_left (const graphene_rect_t *r,
                               graphene_point_t      *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  graphene_point_init (p, rr.origin.x, rr.origin.y + rr.size.height);
}

/**
 * graphene_rect_get_bottom_right:
 * @r: a #graphene_rect_t
 * @p: (out caller-allocates): return location for a #graphene_point_t
 *
 * Retrieves the coordinates of the bottom-right corner of the given rectangle.
 *
 * Since: 1.0
 */
void
graphene_rect_get_bottom_right (const graphene_rect_t *r,
                                graphene_point_t      *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  graphene_point_init (p,
                       rr.origin.x + rr.size.width,
                       rr.origin.y + rr.size.height);
}

/**
 * graphene_rect_get_vertices:
 * @r: a #graphene_rect_t
 * @vertices: (out) (array fixed-size=4): return location for an array
 *  of 4 #graphene_vec2_t
 *
 * Computes the four vertices of a #graphene_rect_t.
 *
 * Since: 1.4
 */
void
graphene_rect_get_vertices (const graphene_rect_t *r,
                            graphene_vec2_t        vertices[])
{
  graphene_rect_t rr;

  graphene_rect_normalize_r (r, &rr);

  graphene_vec2_init (&vertices[0], rr.origin.x, rr.origin.y);
  graphene_vec2_init (&vertices[1], rr.origin.x + rr.size.width, rr.origin.y);
  graphene_vec2_init (&vertices[2], rr.origin.x + rr.size.width, rr.origin.y + rr.size.height);
  graphene_vec2_init (&vertices[3], rr.origin.x, rr.origin.y + rr.size.height);
}

#define GRAPHENE_RECT_GET(arg, part, field) \
  float \
  graphene_rect_get_ ## field (const graphene_rect_t * arg) \
  { \
    graphene_rect_t rr; \
\
    rr = *arg; \
    graphene_rect_normalize_in_place (&rr); \
\
    return rr.part.field; \
  }

/**
 * graphene_rect_get_x:
 * @r: a #graphene_rect_t
 *
 * Retrieves the normalized X coordinate of the origin of the given
 * rectangle.
 *
 * Returns: the normalized X coordinate of the rectangle
 *
 * Since: 1.0
 */
GRAPHENE_RECT_GET (r, origin, x)

/**
 * graphene_rect_get_y:
 * @r: a #graphene_rect_t
 *
 * Retrieves the normalized Y coordinate of the origin of the given
 * rectangle.
 *
 * Returns: the normalized Y coordinate of the rectangle
 *
 * Since: 1.0
 */
GRAPHENE_RECT_GET (r, origin, y)

/**
 * graphene_rect_get_width:
 * @r: a #graphene_rect_t
 *
 * Retrieves the normalized width of the given rectangle.
 *
 * Returns: the normalized width of the rectangle
 *
 * Since: 1.0
 */
GRAPHENE_RECT_GET (r, size, width)

/**
 * graphene_rect_get_height:
 * @r: a #graphene_rect_t
 *
 * Retrieves the normalized height of the given rectangle.
 *
 * Returns: the normalized height of the rectangle
 *
 * Since: 1.0
 */
GRAPHENE_RECT_GET (r, size, height)

#undef GRAPHENE_RECT_GET

/**
 * graphene_rect_get_area:
 * @r: a #graphene_rect_t
 *
 * Compute the area of given normalized rectangle.
 *
 * Returns: the area of the normalized rectangle
 *
 * Since: 1.10
 */
float
graphene_rect_get_area (const graphene_rect_t *r)
{
  graphene_rect_t rr;

  graphene_rect_normalize_r (r, &rr);

  return rr.size.width * rr.size.height;
}

/**
 * graphene_rect_union:
 * @a: a #graphene_rect_t
 * @b: a #graphene_rect_t
 * @res: (out caller-allocates): return location for a #graphene_rect_t
 *
 * Computes the union of the two given rectangles.
 *
 * ![](rectangle-union.png)
 *
 * The union in the image above is the blue outline.
 *
 * Since: 1.0
 */
void
graphene_rect_union (const graphene_rect_t *a,
                     const graphene_rect_t *b,
                     graphene_rect_t       *res)
{
  graphene_rect_t ra, rb;

  ra = *a;
  rb = *b;

  graphene_rect_normalize_in_place (&ra);
  graphene_rect_normalize_in_place (&rb);

  res->origin.x = MIN (ra.origin.x, rb.origin.x);
  res->origin.y = MIN (ra.origin.y, rb.origin.y);

  res->size.width = MAX (ra.origin.x + ra.size.width, rb.origin.x + rb.size.width) - res->origin.x;
  res->size.height = MAX (ra.origin.y + ra.size.height, rb.origin.y + rb.size.height) - res->origin.y;
}

/**
 * graphene_rect_intersection:
 * @a: a #graphene_rect_t
 * @b: a #graphene_rect_t
 * @res: (out caller-allocates) (optional): return location for
 *   a #graphene_rect_t
 *
 * Computes the intersection of the two given rectangles.
 *
 * ![](rectangle-intersection.png)
 *
 * The intersection in the image above is the blue outline.
 *
 * If the two rectangles do not intersect, @res will contain
 * a degenerate rectangle with origin in (0, 0) and a size of 0.
 *
 * Returns: `true` if the two rectangles intersect
 *
 * Since: 1.0
 */
bool
graphene_rect_intersection (const graphene_rect_t *a,
                            const graphene_rect_t *b,
                            graphene_rect_t       *res)
{
  graphene_rect_t ra, rb;
  float x_1, y_1, x_2, y_2;

  ra = *a;
  rb = *b;

  graphene_rect_normalize_in_place (&ra);
  graphene_rect_normalize_in_place (&rb);

  x_1 = MAX (ra.origin.x, rb.origin.x);
  y_1 = MAX (ra.origin.y, rb.origin.y);
  x_2 = MIN (ra.origin.x + ra.size.width, rb.origin.x + rb.size.width);
  y_2 = MIN (ra.origin.y + ra.size.height, rb.origin.y + rb.size.height);

  if (x_1 >= x_2 || y_1 >= y_2)
    {
      if (res != NULL)
        graphene_rect_init (res, 0.0f, 0.0f, 0.0f, 0.0f);

      return false;
    }

  if (res != NULL)
    graphene_rect_init (res, x_1, y_1, x_2 - x_1, y_2 - y_1);

  return true;
}

/**
 * graphene_rect_contains_point:
 * @r: a #graphene_rect_t
 * @p: a #graphene_point_t
 *
 * Checks whether a #graphene_rect_t contains the given coordinates.
 *
 * Returns: `true` if the rectangle contains the point
 *
 * Since: 1.0
 */
bool
graphene_rect_contains_point (const graphene_rect_t  *r,
                              const graphene_point_t *p)
{
  graphene_rect_t rr;

  rr = *r;
  graphene_rect_normalize_in_place (&rr);

  return p->x >= rr.origin.x &&
         p->y >= rr.origin.y &&
         p->x <= (rr.origin.x + rr.size.width) &&
         p->y <= (rr.origin.y + rr.size.height);
}

/**
 * graphene_rect_contains_rect:
 * @a: a #graphene_rect_t
 * @b: a #graphene_rect_t
 *
 * Checks whether a #graphene_rect_t fully contains the given
 * rectangle.
 *
 * Returns: `true` if the rectangle @a fully contains @b
 *
 * Since: 1.0
 */
bool
graphene_rect_contains_rect (const graphene_rect_t *a,
                             const graphene_rect_t *b)
{
  graphene_rect_t res;

  graphene_rect_union (a, b, &res);

  return graphene_rect_equal (a, &res);
}

/**
 * graphene_rect_offset:
 * @r: a #graphene_rect_t
 * @d_x: the horizontal offset
 * @d_y: the vertical offset
 *
 * Offsets the origin by @d_x and @d_y.
 *
 * The size of the rectangle is unchanged.
 *
 * Returns: (transfer none): the offset rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_offset (graphene_rect_t *r,
                      float            d_x,
                      float            d_y)
{
  graphene_rect_offset_r (r, d_x, d_y, r);

  return r;
}

/**
 * graphene_rect_offset_r:
 * @r: a #graphene_rect_t
 * @d_x: the horizontal offset
 * @d_y: the vertical offset
 * @res: (out caller-allocates): return location for the offset
 *   rectangle
 *
 * Offsets the origin of the given rectangle by @d_x and @d_y.
 *
 * The size of the rectangle is left unchanged.
 *
 * Since: 1.4
 */
void
graphene_rect_offset_r (const graphene_rect_t *r,
                        float                  d_x,
                        float                  d_y,
                        graphene_rect_t       *res)
{
  graphene_rect_normalize_r (r, res);

  res->origin.x += d_x;
  res->origin.y += d_y;
}

/**
 * graphene_rect_inset:
 * @r: a #graphene_rect_t
 * @d_x: the horizontal inset
 * @d_y: the vertical inset
 *
 * Changes the given rectangle to be smaller, or larger depending on the
 * given inset parameters.
 *
 * To create an inset rectangle, use positive @d_x or @d_y values; to
 * create a larger, encompassing rectangle, use negative @d_x or @d_y
 * values.
 *
 * The origin of the rectangle is offset by @d_x and @d_y, while the size
 * is adjusted by `(2 * @d_x, 2 * @d_y)`. If @d_x and @d_y are positive
 * values, the size of the rectangle is decreased; if @d_x and @d_y are
 * negative values, the size of the rectangle is increased.
 *
 * If the size of the resulting inset rectangle has a negative width or
 * height then the size will be set to zero.
 *
 * Returns: (transfer none): the inset rectangle
 *
 * Since: 1.0
 */
graphene_rect_t *
graphene_rect_inset (graphene_rect_t *r,
                     float            d_x,
                     float            d_y)
{
  graphene_rect_inset_r (r, d_x, d_y, r);

  return r;
}

/**
 * graphene_rect_inset_r:
 * @r: a #graphene_rect_t
 * @d_x: the horizontal inset
 * @d_y: the vertical inset
 * @res: (out caller-allocates): return location for the inset rectangle
 *
 * Changes the given rectangle to be smaller, or larger depending on the
 * given inset parameters.
 *
 * To create an inset rectangle, use positive @d_x or @d_y values; to
 * create a larger, encompassing rectangle, use negative @d_x or @d_y
 * values.
 *
 * The origin of the rectangle is offset by @d_x and @d_y, while the size
 * is adjusted by `(2 * @d_x, 2 * @d_y)`. If @d_x and @d_y are positive
 * values, the size of the rectangle is decreased; if @d_x and @d_y are
 * negative values, the size of the rectangle is increased.
 *
 * If the size of the resulting inset rectangle has a negative width or
 * height then the size will be set to zero.
 *
 * Since: 1.4
 */
void
graphene_rect_inset_r (const graphene_rect_t *r,
                       float                  d_x,
                       float                  d_y,
                       graphene_rect_t       *res)
{
  graphene_rect_normalize_r (r, res);

  res->origin.x += d_x;
  res->origin.y += d_y;

  if (d_x >= 0.f)
    res->size.width -= (d_x * 2.f);
  else
    res->size.width += (d_x * -2.f);

  if (d_y >= 0.f)
    res->size.height -= (d_y * 2.f);
  else
    res->size.height += (d_y * -2.f);

  if (res->size.width < 0.f)
    res->size.width = 0.f;

  if (res->size.height < 0.f)
    res->size.height = 0.f;
}

/**
 * graphene_rect_round_to_pixel:
 * @r: a #graphene_rect_t
 *
 * Rounds the origin and the size of the given rectangle to
 * their nearest integer values; the rounding is guaranteed
 * to be large enough to contain the original rectangle.
 *
 * Returns: (transfer none): the pixel-aligned rectangle.
 *
 * Since: 1.0
 *
 * Deprecated: 1.4: Use graphene_rect_round() instead
 */
graphene_rect_t *
graphene_rect_round_to_pixel (graphene_rect_t *r)
{
  graphene_rect_round (r, r);

  return r;
}

/**
 * graphene_rect_round:
 * @r: a #graphene_rect_t
 * @res: (out caller-allocates): return location for the
 *   rounded rectangle
 *
 * Rounds the origin and size of the given rectangle to
 * their nearest integer values; the rounding is guaranteed
 * to be large enough to have an area bigger or equal to the
 * original rectangle, but might not fully contain its extents.
 * Use graphene_rect_round_extents() in case you need to round
 * to a rectangle that covers fully the original one.
 *
 * This function is the equivalent of calling `floor` on
 * the coordinates of the origin, and `ceil` on the size.
 *
 * Since: 1.4
 *
 * Deprecated: 1.10: Use graphene_rect_round_extents() instead
 */
void
graphene_rect_round (const graphene_rect_t *r,
                     graphene_rect_t       *res)
{
  graphene_rect_normalize_r (r, res);

  res->origin.x = floorf (res->origin.x);
  res->origin.y = floorf (res->origin.y);

  res->size.width = ceilf (res->size.width);
  res->size.height = ceilf (res->size.height);
}

/**
 * graphene_rect_round_extents:
 * @r: a #graphene_rect_t
 * @res: (out caller-allocates): return location for the
 *   rectangle with rounded extents
 *
 * Rounds the origin of the given rectangle to its nearest
 * integer value and and recompute the size so that the
 * rectangle is large enough to contain all the conrners
 * of the original rectangle.
 *
 * This function is the equivalent of calling `floor` on
 * the coordinates of the origin, and recomputing the size
 * calling `ceil` on the bottom-right coordinates.
 *
 * If you want to be sure that the rounded rectangle
 * completely covers the area that was covered by the
 * original rectangle — i.e. you want to cover the area
 * including all its corners — this function will make sure
 * that the size is recomputed taking into account the ceiling
 * of the coordinates of the bottom-right corner.
 * If the difference between the original coordinates and the
 * coordinates of the rounded rectangle is greater than the
 * difference between the original size and and the rounded
 * size, then the move of the origin would not be compensated
 * by a move in the anti-origin, leaving the corners of the
 * original rectangle outside the rounded one.
 *
 * Since: 1.10
 */
void
graphene_rect_round_extents (const graphene_rect_t *r,
                             graphene_rect_t       *res)
{
  float x2, y2;

  graphene_rect_normalize_r (r, res);

  x2 = res->origin.x + res->size.width;
  y2 = res->origin.y + res->size.height;

  res->origin.x = floorf (res->origin.x);
  res->origin.y = floorf (res->origin.y);

  res->size.width = ceilf (x2) - res->origin.x;
  res->size.height = ceilf (y2) - res->origin.y;
}

/**
 * graphene_rect_expand:
 * @r: a #graphene_rect_t
 * @p: a #graphene_point_t
 * @res: (out caller-allocates): return location for the expanded rectangle
 *
 * Expands a #graphene_rect_t to contain the given #graphene_point_t.
 *
 * Since: 1.4
 */
void
graphene_rect_expand (const graphene_rect_t  *r,
                      const graphene_point_t *p,
                      graphene_rect_t        *res)
{
  graphene_rect_t tmp;

  graphene_rect_init (&tmp, p->x, p->y, 0.f, 0.f);
  graphene_rect_union (r, &tmp, res);

  graphene_rect_normalize_in_place (res);
}

/**
 * graphene_rect_interpolate:
 * @a: a #graphene_rect_t
 * @b: a #graphene_rect_t
 * @factor: the linear interpolation factor
 * @res: (out caller-allocates): return location for the
 *   interpolated rectangle
 *
 * Linearly interpolates the origin and size of the two given
 * rectangles.
 *
 * Since: 1.0
 */
void
graphene_rect_interpolate (const graphene_rect_t *a,
                           const graphene_rect_t *b,
                           double                 factor,
                           graphene_rect_t       *res)
{
  graphene_rect_t ra, rb;

  ra = *a;
  graphene_rect_normalize_in_place (&ra);

  rb = *b;
  graphene_rect_normalize_in_place (&rb);

  res->origin.x = graphene_lerp (ra.origin.x, rb.origin.x, factor);
  res->origin.y = graphene_lerp (ra.origin.y, rb.origin.y, factor);
  res->size.width = graphene_lerp (ra.size.width, rb.size.width, factor);
  res->size.height = graphene_lerp (ra.size.height, rb.size.height, factor);
}

static const graphene_rect_t _graphene_rect_zero;

/**
 * graphene_rect_zero:
 *
 * Returns a degenerate rectangle with origin fixed at (0, 0) and
 * a size of 0, 0.
 *
 * Returns: (transfer none): a fixed rectangle
 *
 * Since: 1.4
 */
const graphene_rect_t *
graphene_rect_zero (void)
{
  return &_graphene_rect_zero;
}

/**
 * graphene_rect_scale:
 * @r: a #graphene_rect_t
 * @s_h: horizontal scale factor
 * @s_v: vertical scale factor
 * @res: (out caller-allocates): return location for the
 *   scaled rectangle
 *
 * Scales the size and origin of a rectangle horizontaly by @s_h,
 * and vertically by @s_v. The result @res is normalized.
 *
 * Since: 1.10
 */
void
graphene_rect_scale (const graphene_rect_t *r,
                     float                  s_h,
                     float                  s_v,
                     graphene_rect_t       *res)
{
  graphene_rect_normalize_r (r, res);

  res->origin.x *= s_h;
  res->origin.y *= s_v;
  res->size.width *= s_h;
  res->size.height *= s_v;

  graphene_rect_normalize_r (res, res);
}
