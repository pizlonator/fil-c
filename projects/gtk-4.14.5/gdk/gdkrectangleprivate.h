#pragma once

#include "gdkrectangle.h"

#include <math.h>

G_BEGIN_DECLS

/*
 * gdk_rectangle_transform_affine:
 * @src: the rectangle to transform
 * @scale_x: scale factor in the X direction. The scale factor
 *   may be negative or 0.
 * @scale_y: scale factor in the Y direction. The scale factor
 *   may be negative or 0.
 * @offset_x: offset of result in X direction.
 * @offset_y: offset of result in Y direction.
 * @dest: (out caller-allocates): destination rectangle, may be
 *   identical to @src
 *
 * Does an affine transform of the source rectangle and stores the
 * result in the dest rectangle. If the destination rectangle does
 * not fit on integer bounds, the result will be enlarged to make it
 * fit.
 * (Fun fact: This means with a scale of 0 and an offset of 0.5,
 * the resulting rect will have a width of 1.)
 * 
 * The width and height of the result will be positive, even if the
 * src rectangle or the scale were negative.
 *
 * This function can be used with the output of
 * gsk_transform_to_affine().
 **/
static inline void
gdk_rectangle_transform_affine (const GdkRectangle *src,
                                float               scale_x,
                                float               scale_y,
                                float               offset_x,
                                float               offset_y,
                                GdkRectangle       *dest)
{
  float x1, x2, y1, y2;

  x1 = offset_x + src->x * scale_x;
  x2 = offset_x + (src->x + src->width) * scale_x;
  y1 = offset_y + src->y * scale_y;
  y2 = offset_y + (src->y + src->height) * scale_y;

  dest->x = floorf (MIN (x1, x2));
  dest->y = floorf (MIN (y1, y2));
  dest->width = ceilf (MAX (x1, x2)) - dest->x;
  dest->height = ceilf (MAX (y1, y2)) - dest->y;
}


G_END_DECLS

