#pragma once

#include <graphene.h>
#include <math.h>

static inline void
gsk_rect_init (graphene_rect_t *r,
               float            x,
               float            y,
               float            width,
               float            height)
{
  r->origin.x = x;
  r->origin.y = y;
  r->size.width = width;
  r->size.height = height;
}

static inline void
gsk_rect_init_from_rect (graphene_rect_t       *r,
                         const graphene_rect_t *r1)
{
  gsk_rect_init (r, r1->origin.x, r1->origin.y, r1->size.width, r1->size.height);
}

static inline void
gsk_rect_init_offset (graphene_rect_t       *r,
                      const graphene_rect_t *src,
                      float                  dx,
                      float                  dy)
{
  gsk_rect_init (r, src->origin.x + dx, src->origin.y + dy, src->size.width, src->size.height);
}

static inline gboolean G_GNUC_PURE
gsk_rect_contains_rect (const graphene_rect_t *r1,
                        const graphene_rect_t *r2)
{
  return r2->origin.x >= r1->origin.x &&
         (r2->origin.x + r2->size.width) <= (r1->origin.x + r1->size.width) &&
         r2->origin.y >= r1->origin.y &&
         (r2->origin.y + r2->size.height) <= (r1->origin.y + r1->size.height);
}

static inline gboolean G_GNUC_PURE
gsk_rect_intersects (const graphene_rect_t *r1,
                     const graphene_rect_t *r2)
{
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    return FALSE;
  else
    return TRUE;
}

static inline gboolean
gsk_rect_intersection (const graphene_rect_t *r1,
                       const graphene_rect_t *r2,
                       graphene_rect_t       *res)
{
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    {
      gsk_rect_init (res, 0.f, 0.f, 0.f, 0.f);
      return FALSE;
    }
  else
    {
      gsk_rect_init (res, x1, y1, x2 - x1, y2 - y1);
      return TRUE;
    }
}

static inline gboolean G_GNUC_PURE
gsk_rect_is_empty (const graphene_rect_t *rect)
{
  return rect->size.width == 0 || rect->size.height == 0;
}

static inline void
gsk_rect_to_float (const graphene_rect_t *rect,
                   float                  values[4])
{
  values[0] = rect->origin.x;
  values[1] = rect->origin.y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline void
gsk_rect_to_cairo_grow (const graphene_rect_t *graphene,
                        cairo_rectangle_int_t *cairo)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

static inline gboolean
gsk_rect_equal (const graphene_rect_t *r1,
                const graphene_rect_t *r2)
{
  return r1->origin.x == r2->origin.x &&
         r1->origin.y == r2->origin.y &&
         r1->size.width == r2->size.width &&
         r1->size.height == r2->size.height;
}

static inline void
gsk_gpu_rect_to_float (const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       float                   values[4])
{
  values[0] = rect->origin.x + offset->x;
  values[1] = rect->origin.y + offset->y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline void
gsk_rect_round_larger (graphene_rect_t *rect)
{
  float x = floor (rect->origin.x);
  float y = floor (rect->origin.y);
  *rect = GRAPHENE_RECT_INIT (x, y,
                              ceil (rect->origin.x + rect->size.width) - x,
                              ceil (rect->origin.y + rect->size.height) - y);
}

static inline void
gsk_rect_scale (const graphene_rect_t *r,
                float                  sx,
                float                  sy,
                graphene_rect_t       *res)
{
  if (G_UNLIKELY (sx < 0 || sy < 0))
    {
      graphene_rect_scale (r, sx, sy, res);
      return;
    }

  res->origin.x = r->origin.x * sx;
  res->origin.y = r->origin.y * sy;
  res->size.width = r->size.width * sx;
  res->size.height = r->size.height * sy;
}

static inline void
gsk_rect_normalize (graphene_rect_t *r)
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
