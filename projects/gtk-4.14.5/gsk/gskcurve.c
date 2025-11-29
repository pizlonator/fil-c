/*
 * Copyright © 2020 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskcurveprivate.h"
#include "gskboundingboxprivate.h"

/* GskCurve collects all the functionality we need for Bézier segments */

#define MIN_PROGRESS (1/1024.f)

typedef struct _GskCurveClass GskCurveClass;

struct _GskCurveClass
{
  void                          (* init)                (GskCurve               *curve,
                                                         gskpathop               op);
  void                          (* init_foreach)        (GskCurve               *curve,
                                                         GskPathOperation        op,
                                                         const graphene_point_t *pts,
                                                         gsize                   n_pts,
                                                         float                   weight);
  void                          (* print)               (const GskCurve         *curve,
                                                         GString                *string);
  gskpathop                     (* pathop)              (const GskCurve         *curve);
  const graphene_point_t *      (* get_start_point)     (const GskCurve         *curve);
  const graphene_point_t *      (* get_end_point)       (const GskCurve         *curve);
  void                          (* get_start_tangent)   (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_end_tangent)     (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_point)           (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_point_t       *pos);
  void                          (* get_tangent)         (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_vec2_t        *tangent);
  void                          (* reverse)             (const GskCurve         *curve,
                                                         GskCurve               *reverse);
  float                         (* get_curvature)       (const GskCurve         *curve,
                                                         float                   t);
  void                          (* split)               (const GskCurve         *curve,
                                                         float                   progress,
                                                         GskCurve               *result1,
                                                         GskCurve               *result2);
  void                          (* segment)             (const GskCurve         *curve,
                                                         float                   start,
                                                         float                   end,
                                                         GskCurve               *segment);
  gboolean                      (* decompose)           (const GskCurve         *curve,
                                                         float                   tolerance,
                                                         GskCurveAddLineFunc     add_line_func,
                                                         gpointer                user_data);
  gboolean                      (* decompose_curve)     (const GskCurve         *curve,
                                                         GskPathForeachFlags     flags,
                                                         float                   tolerance,
                                                         GskCurveAddCurveFunc    add_curve_func,
                                                         gpointer                user_data);
  void                          (* get_bounds)          (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
  void                          (* get_tight_bounds)    (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
  void                          (* get_derivative_at)   (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_point_t       *value);
  int                           (* get_crossing)        (const GskCurve         *curve,
                                                         const graphene_point_t *point);
  float                         (* get_length_to)       (const GskCurve         *curve,
                                                         float                   t);
  float                         (* get_at_length)       (const GskCurve         *curve,
                                                         float                   distance,
                                                         float                   epsilon);
};

/* {{{ Utilities */

#define RAD_TO_DEG(r) ((r)*180.f/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.f)

static void
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static int
line_get_crossing (const graphene_point_t *p,
                   const graphene_point_t *p1,
                   const graphene_point_t *p2)
{
  if (p1->y <= p->y)
    {
      if (p2->y > p->y)
        {
          if ((p2->x - p1->x) * (p->y - p1->y) - (p->x - p1->x) * (p2->y - p1->y) > 0)
            return 1;
        }
    }
  else if (p2->y <= p->y)
    {
      if ((p2->x - p1->x) * (p->y - p1->y) - (p->x - p1->x) * (p2->y - p1->y) < 0)
        return -1;
    }

  return 0;
}

static int
get_crossing_by_bisection (const GskCurve         *curve,
                           const graphene_point_t *point)
{
  GskBoundingBox bounds;
  GskCurve c1, c2;

  gsk_curve_get_bounds (curve, &bounds);

  if (bounds.max.y < point->y || bounds.min.y > point->y || bounds.max.x < point->x)
    return 0;

  if (bounds.min.x > point->x)
    return line_get_crossing (point, gsk_curve_get_start_point (curve), gsk_curve_get_end_point (curve));

  if (graphene_point_distance (&bounds.min, &bounds.max, NULL, NULL) < 0.001)
    return line_get_crossing (point, gsk_curve_get_start_point (curve), gsk_curve_get_end_point (curve));

  gsk_curve_split (curve, 0.5, &c1, &c2);

  return gsk_curve_get_crossing (&c1, point) + gsk_curve_get_crossing (&c2, point);
}

/* Replace a line by an equivalent quad,
 * and a quad by an equivalent cubic.
 */
static void
gsk_curve_elevate (const GskCurve *curve,
                   GskCurve       *elevated)
{
  if (curve->op == GSK_PATH_LINE)
    {
      graphene_point_t p[3];

      p[0] = curve->line.points[0];
      graphene_point_interpolate (&curve->line.points[0],
                                  &curve->line.points[1],
                                  0.5,
                                  &p[1]);
      p[2] = curve->line.points[1];
      gsk_curve_init (elevated, gsk_pathop_encode (GSK_PATH_QUAD, p));
    }
  else if (curve->op == GSK_PATH_QUAD)
    {
      graphene_point_t p[4];

      p[0] = curve->quad.points[0];
      graphene_point_interpolate (&curve->quad.points[0],
                                  &curve->quad.points[1],
                                  2/3.,
                                  &p[1]);
      graphene_point_interpolate (&curve->quad.points[2],
                                  &curve->quad.points[1],
                                  2/3.,
                                  &p[2]);
      p[3] = curve->quad.points[2];
      gsk_curve_init (elevated, gsk_pathop_encode (GSK_PATH_CUBIC, p));
    }
  else
    g_assert_not_reached ();
}

/* Compute arclength by using Gauss quadrature on
 *
 * \int_0^z \sqrt{ (dx/dt)^2 + (dy/dt)^2 } dt
 */

#include "gskcurve-ct-values.c"

static float
get_length_by_approximation (const GskCurve *curve,
                             float           t)
{
  double z = t / 2;
  double sum = 0;
  graphene_point_t d;

  for (unsigned int i = 0; i < G_N_ELEMENTS (T); i++)
    {
      gsk_curve_get_derivative_at (curve, z * T[i] + z, &d);
      sum += C[i] * sqrt (d.x * d.x + d.y * d.y);
    }

  return z * sum;
}

/* Compute the inverse of the arclength using bisection,
 * to a given precision
 */
static float
get_t_by_bisection (const GskCurve *curve,
                    float           length,
                    float           epsilon)
{
  float t1, t2, t, l;
  GskCurve c1;

  g_assert (epsilon >= FLT_EPSILON);

  t1 = 0;
  t2 = 1;

  do
    {
      t = (t1 + t2) / 2;
      if (t == t1 || t == t2)
        break;

      gsk_curve_split (curve, t, &c1, NULL);

      l = gsk_curve_get_length (&c1);
      if (fabsf (length - l) < epsilon)
        break;
      else if (l < length)
        t1 = t;
      else
        t2 = t;
    }
  while (t1 < t2);

  return t;
}
/* }}} */
/* {{{ Line */

static void
gsk_line_curve_init_from_points (GskLineCurve           *self,
                                 GskPathOperation        op,
                                 const graphene_point_t *start,
                                 const graphene_point_t *end)
{
  self->op = op;
  self->points[0] = *start;
  self->points[1] = *end;
}

static void
gsk_line_curve_init (GskCurve  *curve,
                     gskpathop  op)
{
  GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = gsk_pathop_points (op);

  gsk_line_curve_init_from_points (self, gsk_pathop_op (op), &pts[0], &pts[1]);
}

static void
gsk_line_curve_init_foreach (GskCurve               *curve,
                             GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts,
                             float                   weight)
{
  GskLineCurve *self = &curve->line;

  g_assert (n_pts == 2);

  gsk_line_curve_init_from_points (self, op, &pts[0], &pts[1]);
}

static void
gsk_line_curve_print (const GskCurve *curve,
                      GString        *string)
{
  g_string_append_printf (string,
                          "M %g %g L %g %g",
                          curve->line.points[0].x, curve->line.points[0].y,
                          curve->line.points[1].x, curve->line.points[1].y);
}

static gskpathop
gsk_line_curve_pathop (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_line_curve_get_start_point (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return &self->points[0];
}

static const graphene_point_t *
gsk_line_curve_get_end_point (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return &self->points[1];
}

static void
gsk_line_curve_get_start_end_tangent (const GskCurve  *curve,
                                      graphene_vec2_t *tangent)
{
  const GskLineCurve *self = &curve->line;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_line_curve_get_point (const GskCurve   *curve,
                          float             t,
                          graphene_point_t *pos)
{
  const GskLineCurve *self = &curve->line;

  graphene_point_interpolate (&self->points[0], &self->points[1], t, pos);
}

static void
gsk_line_curve_get_tangent (const GskCurve  *curve,
                            float            t,
                            graphene_vec2_t *tangent)
{
  const GskLineCurve *self = &curve->line;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static float
gsk_line_curve_get_curvature (const GskCurve *curve,
                              float           t)
{
  return 0;
}

static void
gsk_line_curve_reverse (const GskCurve *curve,
                        GskCurve       *reverse)
{
  const GskLineCurve *self = &curve->line;

  reverse->op = GSK_PATH_LINE;
  reverse->line.points[0] = self->points[1];
  reverse->line.points[1] = self->points[0];
}

static void
gsk_line_curve_split (const GskCurve   *curve,
                      float             progress,
                      GskCurve         *start,
                      GskCurve         *end)
{
  const GskLineCurve *self = &curve->line;
  graphene_point_t point;

  graphene_point_interpolate (&self->points[0], &self->points[1], progress, &point);

  if (start)
    gsk_line_curve_init_from_points (&start->line, GSK_PATH_LINE, &self->points[0], &point);
  if (end)
    gsk_line_curve_init_from_points (&end->line, GSK_PATH_LINE, &point, &self->points[1]);
}

static void
gsk_line_curve_segment (const GskCurve *curve,
                        float           start,
                        float           end,
                        GskCurve       *segment)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;
  graphene_point_t p0, p1;

  graphene_point_interpolate (&pts[0], &pts[1], start, &p0);
  graphene_point_interpolate (&pts[0], &pts[1], end, &p1);

  gsk_line_curve_init_from_points (&segment->line, GSK_PATH_LINE, &p0, &p1);
}

static gboolean
gsk_line_curve_decompose (const GskCurve      *curve,
                          float                tolerance,
                          GskCurveAddLineFunc  add_line_func,
                          gpointer             user_data)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  return add_line_func (&pts[0], &pts[1], 0.f, 1.f, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
}

static gboolean
gsk_line_curve_decompose_curve (const GskCurve       *curve,
                                GskPathForeachFlags   flags,
                                float                 tolerance,
                                GskCurveAddCurveFunc  add_curve_func,
                                gpointer              user_data)
{
  const GskLineCurve *self = &curve->line;

  return add_curve_func (GSK_PATH_LINE, self->points, 2, 0.f, user_data);
}

static void
gsk_line_curve_get_bounds (const GskCurve  *curve,
                           GskBoundingBox  *bounds)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[1]);
}

static void
gsk_line_curve_get_derivative_at (const GskCurve   *curve,
                                  float             t,
                                  graphene_point_t *value)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  value->x = pts[1].x - pts[0].x;
  value->y = pts[1].y - pts[0].y;
}

static int
gsk_line_curve_get_crossing (const GskCurve         *curve,
                             const graphene_point_t *point)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  return line_get_crossing (point, &pts[0], &pts[1]);
}

static float
gsk_line_curve_get_length_to (const GskCurve *curve,
                              float           t)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  return t * graphene_point_distance (&pts[0], &pts[1], NULL, NULL);
}

static float
gsk_line_curve_get_at_length (const GskCurve *curve,
                              float           distance,
                              float           epsilon)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;
  float length;

  length = graphene_point_distance (&pts[0], &pts[1], NULL, NULL);

  if (length == 0)
    return 0;

  return CLAMP (distance / length, 0, 1);
}

static const GskCurveClass GSK_LINE_CURVE_CLASS = {
  gsk_line_curve_init,
  gsk_line_curve_init_foreach,
  gsk_line_curve_print,
  gsk_line_curve_pathop,
  gsk_line_curve_get_start_point,
  gsk_line_curve_get_end_point,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_point,
  gsk_line_curve_get_tangent,
  gsk_line_curve_reverse,
  gsk_line_curve_get_curvature,
  gsk_line_curve_split,
  gsk_line_curve_segment,
  gsk_line_curve_decompose,
  gsk_line_curve_decompose_curve,
  gsk_line_curve_get_bounds,
  gsk_line_curve_get_bounds,
  gsk_line_curve_get_derivative_at,
  gsk_line_curve_get_crossing,
  gsk_line_curve_get_length_to,
  gsk_line_curve_get_at_length,
};

/* }}} */
/* {{{ Quadratic */

static void
gsk_quad_curve_ensure_coefficients (const GskQuadCurve *curve)
{
  GskQuadCurve *self = (GskQuadCurve *) curve;
  const graphene_point_t *pts = self->points;

  if (self->has_coefficients)
    return;

  self->coeffs[2] = pts[0];
  self->coeffs[1] = GRAPHENE_POINT_INIT (2 * (pts[1].x - pts[0].x),
                                         2 * (pts[1].y - pts[0].y));
  self->coeffs[0] = GRAPHENE_POINT_INIT (pts[2].x - 2 * pts[1].x + pts[0].x,
                                         pts[2].y - 2 * pts[1].y + pts[0].y);

  self->has_coefficients = TRUE;
}

static void
gsk_quad_curve_init_from_points (GskQuadCurve           *self,
                                 const graphene_point_t  pts[3])
{
  self->op = GSK_PATH_QUAD;
  self->has_coefficients = FALSE;
  memcpy (self->points, pts, sizeof (graphene_point_t) * 3);
}

static void
gsk_quad_curve_init (GskCurve  *curve,
                     gskpathop  op)
{
  GskQuadCurve *self = &curve->quad;

  gsk_quad_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_quad_curve_init_foreach (GskCurve               *curve,
                             GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts,
                             float                   weight)
{
  GskQuadCurve *self = &curve->quad;

  g_assert (n_pts == 3);

  gsk_quad_curve_init_from_points (self, pts);
}

static void
gsk_quad_curve_print (const GskCurve *curve,
                      GString        *string)
{
  g_string_append_printf (string,
                          "M %g %g Q %g %g %g %g",
                          curve->quad.points[0].x, curve->quad.points[0].y,
                          curve->quad.points[1].x, curve->cubic.points[1].y,
                          curve->quad.points[2].x, curve->cubic.points[2].y);
}

static gskpathop
gsk_quad_curve_pathop (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_quad_curve_get_start_point (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return &self->points[0];
}

static const graphene_point_t *
gsk_quad_curve_get_end_point (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return &self->points[2];
}

static void
gsk_quad_curve_get_start_tangent (const GskCurve  *curve,
                                  graphene_vec2_t *tangent)
{
  const GskQuadCurve *self = &curve->quad;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_quad_curve_get_end_tangent (const GskCurve  *curve,
                                graphene_vec2_t *tangent)
{
  const GskQuadCurve *self = &curve->quad;

  get_tangent (&self->points[1], &self->points[2], tangent);
}

static void
gsk_quad_curve_get_point (const GskCurve   *curve,
                          float             t,
                          graphene_point_t *pos)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *c = self->coeffs;

  gsk_quad_curve_ensure_coefficients (self);

  *pos = GRAPHENE_POINT_INIT ((c[0].x * t + c[1].x) * t + c[2].x,
                              (c[0].y * t + c[1].y) * t + c[2].y);
}

static void
gsk_quad_curve_get_tangent (const GskCurve   *curve,
                            float             t,
                            graphene_vec2_t  *tangent)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *c = self->coeffs;

  gsk_quad_curve_ensure_coefficients (self);

  graphene_vec2_init (tangent,
                      2.0f * c[0].x * t + c[1].x,
                      2.0f * c[0].y * t + c[1].y);
  graphene_vec2_normalize (tangent, tangent);
}


static float gsk_cubic_curve_get_curvature (const GskCurve *curve,
                                            float           t);

static float
gsk_quad_curve_get_curvature (const GskCurve *curve,
                              float           t)
{
  return gsk_cubic_curve_get_curvature (curve, t);
}

static void
gsk_quad_curve_reverse (const GskCurve *curve,
                        GskCurve       *reverse)
{
  const GskCubicCurve *self = &curve->cubic;

  reverse->op = GSK_PATH_QUAD;
  reverse->cubic.points[0] = self->points[2];
  reverse->cubic.points[1] = self->points[1];
  reverse->cubic.points[2] = self->points[0];
  reverse->cubic.has_coefficients = FALSE;
}

static void
gsk_quad_curve_split (const GskCurve   *curve,
                      float             progress,
                      GskCurve         *start,
                      GskCurve         *end)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *pts = self->points;
  graphene_point_t ab, bc;
  graphene_point_t final;

  graphene_point_interpolate (&pts[0], &pts[1], progress, &ab);
  graphene_point_interpolate (&pts[1], &pts[2], progress, &bc);
  graphene_point_interpolate (&ab, &bc, progress, &final);

  if (start)
    gsk_quad_curve_init_from_points (&start->quad, (graphene_point_t[3]) { pts[0], ab, final });
  if (end)
    gsk_quad_curve_init_from_points (&end->quad, (graphene_point_t[3]) { final, bc, pts[2] });
}

static void
gsk_quad_curve_segment (const GskCurve *curve,
                        float           start,
                        float           end,
                        GskCurve       *segment)
{
  GskCurve tmp;

  gsk_quad_curve_split (curve, start, NULL, &tmp);
  gsk_quad_curve_split (&tmp, (end - start) / (1.0f - start), segment, NULL);
}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_quad_curve_too_curvy (const GskQuadCurve *self,
                               float          tolerance)
{
  const graphene_point_t *pts = self->points;
  float dx, dy;

  dx = fabs (pts[1].x / 2 - (pts[0].x + pts[2].x) / 4);
  dy = fabs (pts[1].y / 2 - (pts[0].y + pts[2].y) / 4);

  return MAX (dx, dy) > tolerance;
}

static gboolean
gsk_quad_curve_decompose_step (const GskCurve      *curve,
                               float                start_progress,
                               float                end_progress,
                               float                tolerance,
                               GskCurveAddLineFunc  add_line_func,
                               gpointer             user_data)
{
  const GskQuadCurve *self = &curve->quad;
  GskCurve left, right;
  float mid_progress;

  if (!gsk_quad_curve_too_curvy (self, tolerance))
    return add_line_func (&self->points[0], &self->points[2], start_progress, end_progress, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
  if (end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (&self->points[0], &self->points[2], start_progress, end_progress, GSK_CURVE_LINE_REASON_SHORT, user_data);

  gsk_quad_curve_split ((const GskCurve *) self, 0.5, &left, &right);
  mid_progress = (start_progress + end_progress) / 2;

  return gsk_quad_curve_decompose_step (&left, start_progress, mid_progress, tolerance, add_line_func, user_data)
      && gsk_quad_curve_decompose_step (&right, mid_progress, end_progress, tolerance, add_line_func, user_data);
}

static gboolean
gsk_quad_curve_decompose (const GskCurve      *curve,
                          float                tolerance,
                          GskCurveAddLineFunc  add_line_func,
                          gpointer             user_data)
{
  return gsk_quad_curve_decompose_step (curve, 0.0, 1.0, tolerance, add_line_func, user_data);
}

typedef struct
{
  GskCurveAddCurveFunc add_curve;
  gpointer user_data;
} AddLineData;

static gboolean
gsk_curve_add_line_cb (const graphene_point_t *from,
                       const graphene_point_t *to,
                       float                   from_progress,
                       float                   to_progress,
                       GskCurveLineReason      reason,
                       gpointer                user_data)
{
  AddLineData *data = user_data;
  graphene_point_t p[2] = { *from, *to };

  return data->add_curve (GSK_PATH_LINE, p, 2, 0.f, data->user_data);
}

static gboolean
gsk_quad_curve_decompose_curve (const GskCurve       *curve,
                                GskPathForeachFlags   flags,
                                float                 tolerance,
                                GskCurveAddCurveFunc  add_curve_func,
                                gpointer              user_data)
{
  const GskQuadCurve *self = &curve->quad;

  if (flags & GSK_PATH_FOREACH_ALLOW_QUAD)
    return add_curve_func (GSK_PATH_QUAD, self->points, 3, 0.f, user_data);
  else if (graphene_point_equal (&curve->conic.points[0], &curve->conic.points[1]) ||
           graphene_point_equal (&curve->conic.points[1], &curve->conic.points[2]))
    {
      if (!graphene_point_equal (&curve->conic.points[0], &curve->conic.points[2]))
        return add_curve_func (GSK_PATH_LINE,
                               (graphene_point_t[2]) {
                                 curve->conic.points[0],
                                 curve->conic.points[2],
                               },
                               2, 0.f, user_data);
      else
        return TRUE;
    }
  else if (flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
    {
      GskCurve c;

      gsk_curve_elevate (curve, &c);
      return add_curve_func (GSK_PATH_CUBIC, c.cubic.points, 4, 0.f, user_data);
    }
  else
    {
      return gsk_quad_curve_decompose (curve,
                                       tolerance,
                                       gsk_curve_add_line_cb,
                                       &(AddLineData) { add_curve_func, user_data });
    }
}

static void
gsk_quad_curve_get_bounds (const GskCurve  *curve,
                           GskBoundingBox  *bounds)
{
  const GskQuadCurve *self = &curve->quad;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[2]);
  gsk_bounding_box_expand (bounds, &pts[1]);
}

/* Solve P' = 0 where P is
 * P = (1-t)^2*pa + 2*t*(1-t)*pb + t^2*pc
 */
static int
get_quadratic_extrema (float pa, float pb, float pc, float t[1])
{
  float d = pa - 2 * pb + pc;

  if (fabs (d) > 0.0001)
    {
      t[0] = (pa - pb) / d;
      return 1;
    }

  return 0;
}

static void
gsk_quad_curve_get_tight_bounds (const GskCurve  *curve,
                                 GskBoundingBox  *bounds)
{
  const GskQuadCurve *self = &curve->quad;
  const graphene_point_t *pts = self->points;
  float t[4];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[2]);

  n = 0;
  n += get_quadratic_extrema (pts[0].x, pts[1].x, pts[2].x, &t[n]);
  n += get_quadratic_extrema (pts[0].y, pts[1].y, pts[2].y, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_quad_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static void
gsk_quad_curve_get_derivative (const GskCurve *curve,
                               GskCurve       *deriv)
{
  const GskQuadCurve *self = &curve->quad;
  graphene_point_t p[2];

  p[0].x = 2.f * (self->points[1].x - self->points[0].x);
  p[0].y = 2.f * (self->points[1].y - self->points[0].y);
  p[1].x = 2.f * (self->points[2].x - self->points[1].x);
  p[1].y = 2.f * (self->points[2].y - self->points[1].y);

  gsk_line_curve_init_from_points (&deriv->line, GSK_PATH_LINE, &p[0], &p[1]);
}

static void
gsk_quad_curve_get_derivative_at (const GskCurve   *curve,
                                  float             t,
                                  graphene_point_t *value)
{
  GskCurve d;
  gsk_quad_curve_get_derivative (curve, &d);
  gsk_curve_get_point (&d, t, value);
}

static int
gsk_quad_curve_get_crossing (const GskCurve         *curve,
                             const graphene_point_t *point)
{
  return get_crossing_by_bisection (curve, point);
}

static float
gsk_quad_curve_get_length_to (const GskCurve *curve,
                              float           t)
{
  return get_length_by_approximation (curve, t);
}

static float
gsk_quad_curve_get_at_length (const GskCurve *curve,
                              float           t,
                              float           epsilon)
{
  return get_t_by_bisection (curve, t, epsilon);
}

static const GskCurveClass GSK_QUAD_CURVE_CLASS = {
  gsk_quad_curve_init,
  gsk_quad_curve_init_foreach,
  gsk_quad_curve_print,
  gsk_quad_curve_pathop,
  gsk_quad_curve_get_start_point,
  gsk_quad_curve_get_end_point,
  gsk_quad_curve_get_start_tangent,
  gsk_quad_curve_get_end_tangent,
  gsk_quad_curve_get_point,
  gsk_quad_curve_get_tangent,
  gsk_quad_curve_reverse,
  gsk_quad_curve_get_curvature,
  gsk_quad_curve_split,
  gsk_quad_curve_segment,
  gsk_quad_curve_decompose,
  gsk_quad_curve_decompose_curve,
  gsk_quad_curve_get_bounds,
  gsk_quad_curve_get_tight_bounds,
  gsk_quad_curve_get_derivative_at,
  gsk_quad_curve_get_crossing,
  gsk_quad_curve_get_length_to,
  gsk_quad_curve_get_at_length,
};

/* }}} */
/* {{{ Cubic */

static void
gsk_cubic_curve_ensure_coefficients (const GskCubicCurve *curve)
{
  GskCubicCurve *self = (GskCubicCurve *) curve;
  const graphene_point_t *pts = &self->points[0];

  if (self->has_coefficients)
    return;

  self->coeffs[0] = GRAPHENE_POINT_INIT (pts[3].x - 3.0f * pts[2].x + 3.0f * pts[1].x - pts[0].x,
                                         pts[3].y - 3.0f * pts[2].y + 3.0f * pts[1].y - pts[0].y);
  self->coeffs[1] = GRAPHENE_POINT_INIT (3.0f * pts[2].x - 6.0f * pts[1].x + 3.0f * pts[0].x,
                                         3.0f * pts[2].y - 6.0f * pts[1].y + 3.0f * pts[0].y);
  self->coeffs[2] = GRAPHENE_POINT_INIT (3.0f * pts[1].x - 3.0f * pts[0].x,
                                         3.0f * pts[1].y - 3.0f * pts[0].y);
  self->coeffs[3] = pts[0];

  self->has_coefficients = TRUE;
}

static void
gsk_cubic_curve_init_from_points (GskCubicCurve          *self,
                                  const graphene_point_t  pts[4])
{
  self->op = GSK_PATH_CUBIC;
  self->has_coefficients = FALSE;
  memcpy (self->points, pts, sizeof (graphene_point_t) * 4);
}

static void
gsk_cubic_curve_init (GskCurve  *curve,
                      gskpathop  op)
{
  GskCubicCurve *self = &curve->cubic;

  gsk_cubic_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_cubic_curve_init_foreach (GskCurve               *curve,
                              GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts,
                              float                   weight)
{
  GskCubicCurve *self = &curve->cubic;

  g_assert (n_pts == 4);

  gsk_cubic_curve_init_from_points (self, pts);
}

static void
gsk_cubic_curve_print (const GskCurve *curve,
                       GString        *string)
{
  g_string_append_printf (string,
                          "M %f %f C %f %f %f %f %f %f",
                          curve->cubic.points[0].x, curve->cubic.points[0].y,
                          curve->cubic.points[1].x, curve->cubic.points[1].y,
                          curve->cubic.points[2].x, curve->cubic.points[2].y,
                          curve->cubic.points[3].x, curve->cubic.points[3].y);
}

static gskpathop
gsk_cubic_curve_pathop (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_cubic_curve_get_start_point (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return &self->points[0];
}

static const graphene_point_t *
gsk_cubic_curve_get_end_point (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return &self->points[3];
}

static void
gsk_cubic_curve_get_start_tangent (const GskCurve  *curve,
                                   graphene_vec2_t *tangent)
{
  const GskCubicCurve *self = &curve->cubic;

  if (graphene_point_near (&self->points[0], &self->points[1], 0.0001))
    {
      if (graphene_point_near (&self->points[0], &self->points[2], 0.0001))
        get_tangent (&self->points[0], &self->points[3], tangent);
      else
        get_tangent (&self->points[0], &self->points[2], tangent);
    }
  else
    get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_cubic_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskCubicCurve *self = &curve->cubic;

  if (graphene_point_near (&self->points[2], &self->points[3], 0.0001))
    {
      if (graphene_point_near (&self->points[1], &self->points[3], 0.0001))
        get_tangent (&self->points[0], &self->points[3], tangent);
      else
        get_tangent (&self->points[1], &self->points[3], tangent);
    }
  else
    get_tangent (&self->points[2], &self->points[3], tangent);
}

static void
gsk_cubic_curve_get_point (const GskCurve   *curve,
                           float             t,
                           graphene_point_t *pos)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *c = self->coeffs;

  gsk_cubic_curve_ensure_coefficients (self);

  *pos = GRAPHENE_POINT_INIT (((c[0].x * t + c[1].x) * t +c[2].x) * t + c[3].x,
                              ((c[0].y * t + c[1].y) * t +c[2].y) * t + c[3].y);
}

static void
gsk_cubic_curve_get_tangent (const GskCurve   *curve,
                             float             t,
                             graphene_vec2_t  *tangent)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *c = self->coeffs;

  gsk_cubic_curve_ensure_coefficients (self);

  graphene_vec2_init (tangent,
                      (3.0f * c[0].x * t + 2.0f * c[1].x) * t + c[2].x,
                      (3.0f * c[0].y * t + 2.0f * c[1].y) * t + c[2].y);
  graphene_vec2_normalize (tangent, tangent);
}

static void
gsk_cubic_curve_reverse (const GskCurve *curve,
                         GskCurve       *reverse)
{
  const GskCubicCurve *self = &curve->cubic;

  reverse->op = GSK_PATH_CUBIC;
  reverse->cubic.points[0] = self->points[3];
  reverse->cubic.points[1] = self->points[2];
  reverse->cubic.points[2] = self->points[1];
  reverse->cubic.points[3] = self->points[0];
  reverse->cubic.has_coefficients = FALSE;
}

static inline float
cross (const graphene_vec2_t *v1,
       const graphene_vec2_t *v2)
{
  return graphene_vec2_get_x (v1) * graphene_vec2_get_y (v2)
         - graphene_vec2_get_y (v1) * graphene_vec2_get_x (v2);
}

static inline float
pow3 (float w)
{
  return w * w * w;
}

static void
gsk_cubic_curve_get_derivative (const GskCurve *curve,
                                GskCurve       *deriv)
{
  const GskCubicCurve *self = &curve->cubic;
  graphene_point_t p[3];

  p[0].x = 3.f * (self->points[1].x - self->points[0].x);
  p[0].y = 3.f * (self->points[1].y - self->points[0].y);
  p[1].x = 3.f * (self->points[2].x - self->points[1].x);
  p[1].y = 3.f * (self->points[2].y - self->points[1].y);
  p[2].x = 3.f * (self->points[3].x - self->points[2].x);
  p[2].y = 3.f * (self->points[3].y - self->points[2].y);

  gsk_quad_curve_init_from_points (&deriv->quad, p);
}

static float
gsk_cubic_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  GskCurve c1, c2;
  graphene_point_t p, pp;
  graphene_vec2_t d, dd;
  float num, denom;

  gsk_cubic_curve_get_derivative (curve, &c1);
  gsk_quad_curve_get_derivative (&c1, &c2);

  gsk_curve_get_point (&c1, t, &p);
  gsk_curve_get_point (&c2, t, &pp);

  graphene_vec2_init (&d, p.x, p.y);
  graphene_vec2_init (&dd, pp.x, pp.y);

  num = cross (&d, &dd);
  if (num == 0)
    return 0;

  denom = pow3 (graphene_vec2_length (&d));
  if (denom == 0)
    return 0;

  return num / denom;
}

static void
gsk_cubic_curve_split (const GskCurve   *curve,
                       float             progress,
                       GskCurve         *start,
                       GskCurve         *end)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *pts = self->points;
  graphene_point_t ab, bc, cd;
  graphene_point_t abbc, bccd;
  graphene_point_t final;

  graphene_point_interpolate (&pts[0], &pts[1], progress, &ab);
  graphene_point_interpolate (&pts[1], &pts[2], progress, &bc);
  graphene_point_interpolate (&pts[2], &pts[3], progress, &cd);
  graphene_point_interpolate (&ab, &bc, progress, &abbc);
  graphene_point_interpolate (&bc, &cd, progress, &bccd);
  graphene_point_interpolate (&abbc, &bccd, progress, &final);

  if (start)
    gsk_cubic_curve_init_from_points (&start->cubic, (graphene_point_t[4]) { pts[0], ab, abbc, final });
  if (end)
    gsk_cubic_curve_init_from_points (&end->cubic, (graphene_point_t[4]) { final, bccd, cd, pts[3] });
}

static void
gsk_cubic_curve_segment (const GskCurve *curve,
                         float           start,
                         float           end,
                         GskCurve       *segment)
{
  GskCurve tmp;

  gsk_cubic_curve_split (curve, start, NULL, &tmp);
  gsk_cubic_curve_split (&tmp, (end - start) / (1.0f - start), segment, NULL);
}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_cubic_curve_too_curvy (const GskCubicCurve *self,
                           float                tolerance)
{
  const graphene_point_t *pts = self->points;
  graphene_point_t p;

  graphene_point_interpolate (&pts[0], &pts[3], 1.0f / 3, &p);
  if (MAX (ABS (p.x - pts[1].x), ABS (p.y - pts[1].y)) > tolerance)
    return TRUE;

  graphene_point_interpolate (&pts[0], &pts[3], 2.0f / 3, &p);
  if (MAX (ABS (p.x - pts[2].x), ABS (p.y - pts[2].y)) > tolerance)
    return TRUE;

  return FALSE;
}

static gboolean
gsk_cubic_curve_decompose_step (const GskCurve      *curve,
                                float                start_progress,
                                float                end_progress,
                                float                tolerance,
                                GskCurveAddLineFunc  add_line_func,
                                gpointer             user_data)
{
  const GskCubicCurve *self = &curve->cubic;
  GskCurve left, right;
  float mid_progress;

  if (!gsk_cubic_curve_too_curvy (self, tolerance))
    return add_line_func (&self->points[0], &self->points[3], start_progress, end_progress, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
  if (end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (&self->points[0], &self->points[3], start_progress, end_progress, GSK_CURVE_LINE_REASON_SHORT, user_data);

  gsk_cubic_curve_split ((const GskCurve *) self, 0.5, &left, &right);
  mid_progress = (start_progress + end_progress) / 2;

  return gsk_cubic_curve_decompose_step (&left, start_progress, mid_progress, tolerance, add_line_func, user_data)
      && gsk_cubic_curve_decompose_step (&right, mid_progress, end_progress, tolerance, add_line_func, user_data);
}

static gboolean
gsk_cubic_curve_decompose (const GskCurve      *curve,
                           float                tolerance,
                           GskCurveAddLineFunc  add_line_func,
                           gpointer             user_data)
{
  return gsk_cubic_curve_decompose_step (curve, 0.0, 1.0, tolerance, add_line_func, user_data);
}

static gboolean
gsk_cubic_curve_decompose_curve (const GskCurve       *curve,
                                 GskPathForeachFlags   flags,
                                 float                 tolerance,
                                 GskCurveAddCurveFunc  add_curve_func,
                                 gpointer              user_data)
{
  const GskCubicCurve *self = &curve->cubic;

  if (flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
    return add_curve_func (GSK_PATH_CUBIC, self->points, 4, 0.f, user_data);

  /* FIXME: Quadratic or arc approximation */
  return gsk_cubic_curve_decompose (curve,
                                    tolerance,
                                    gsk_curve_add_line_cb,
                                    &(AddLineData) { add_curve_func, user_data });
}

static void
gsk_cubic_curve_get_bounds (const GskCurve  *curve,
                            GskBoundingBox  *bounds)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);
  gsk_bounding_box_expand (bounds, &pts[1]);
  gsk_bounding_box_expand (bounds, &pts[2]);
}

static inline gboolean
acceptable (float t)
{
  return 0 <= t && t <= 1;
}

/* Solve P' = 0 where P is
 * P = (1-t)^3*pa + 3*t*(1-t)^2*pb + 3*t^2*(1-t)*pc + t^3*pd
 */
static int
get_cubic_extrema (float pa, float pb, float pc, float pd, float t[2])
{
  float a, b, c;
  float d, tt;
  int n = 0;

  a = 3 * (pd - 3*pc + 3*pb - pa);
  b = 6 * (pc - 2*pb + pa);
  c = 3 * (pb - pa);

  if (fabs (a) > 0.0001)
    {
      if (b*b > 4*a*c)
        {
          d = sqrt (b*b - 4*a*c);
          tt = (-b + d)/(2*a);
          if (acceptable (tt))
            t[n++] = tt;
          tt = (-b - d)/(2*a);
          if (acceptable (tt))
            t[n++] = tt;
        }
      else
        {
          tt = -b / (2*a);
          if (acceptable (tt))
            t[n++] = tt;
        }
    }
  else if (fabs (b) > 0.0001)
    {
      tt = -c / b;
      if (acceptable (tt))
        t[n++] = tt;
    }

  return n;
}

static void
gsk_cubic_curve_get_tight_bounds (const GskCurve  *curve,
                                  GskBoundingBox  *bounds)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *pts = self->points;
  float t[4];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);

  n = 0;
  n += get_cubic_extrema (pts[0].x, pts[1].x, pts[2].x, pts[3].x, &t[n]);
  n += get_cubic_extrema (pts[0].y, pts[1].y, pts[2].y, pts[3].y, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_cubic_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static void
gsk_cubic_curve_get_derivative_at (const GskCurve   *curve,
                                   float             t,
                                   graphene_point_t *value)
{
  GskCurve d;
  gsk_cubic_curve_get_derivative (curve, &d);
  gsk_curve_get_point (&d, t, value);
}

static int
gsk_cubic_curve_get_crossing (const GskCurve         *curve,
                              const graphene_point_t *point)
{
  return get_crossing_by_bisection (curve, point);
}

static float
gsk_cubic_curve_get_length_to (const GskCurve *curve,
                               float           t)
{
  return get_length_by_approximation (curve, t);
}

static float
gsk_cubic_curve_get_at_length (const GskCurve *curve,
                               float           t,
                               float           epsilon)
{
  return get_t_by_bisection (curve, t, epsilon);
}

static const GskCurveClass GSK_CUBIC_CURVE_CLASS = {
  gsk_cubic_curve_init,
  gsk_cubic_curve_init_foreach,
  gsk_cubic_curve_print,
  gsk_cubic_curve_pathop,
  gsk_cubic_curve_get_start_point,
  gsk_cubic_curve_get_end_point,
  gsk_cubic_curve_get_start_tangent,
  gsk_cubic_curve_get_end_tangent,
  gsk_cubic_curve_get_point,
  gsk_cubic_curve_get_tangent,
  gsk_cubic_curve_reverse,
  gsk_cubic_curve_get_curvature,
  gsk_cubic_curve_split,
  gsk_cubic_curve_segment,
  gsk_cubic_curve_decompose,
  gsk_cubic_curve_decompose_curve,
  gsk_cubic_curve_get_bounds,
  gsk_cubic_curve_get_tight_bounds,
  gsk_cubic_curve_get_derivative_at,
  gsk_cubic_curve_get_crossing,
  gsk_cubic_curve_get_length_to,
  gsk_cubic_curve_get_at_length,
};

 /*  }}} */
/* {{{ Conic */

static inline float
gsk_conic_curve_get_weight (const GskConicCurve *self)
{
  return self->points[2].x;
}

static void
gsk_conic_curve_ensure_coefficents (const GskConicCurve *curve)
{
  GskConicCurve *self = (GskConicCurve *) curve;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  graphene_point_t pw = GRAPHENE_POINT_INIT (w * pts[1].x, w * pts[1].y);

  if (self->has_coefficients)
    return;

  self->num[2] = pts[0];
  self->num[1] = GRAPHENE_POINT_INIT (2 * (pw.x - pts[0].x),
                                      2 * (pw.y - pts[0].y));
  self->num[0] = GRAPHENE_POINT_INIT (pts[3].x - 2 * pw.x + pts[0].x,
                                      pts[3].y - 2 * pw.y + pts[0].y);

  self->denom[2] = GRAPHENE_POINT_INIT (1, 1);
  self->denom[1] = GRAPHENE_POINT_INIT (2 * (w - 1), 2 * (w - 1));
  self->denom[0] = GRAPHENE_POINT_INIT (-self->denom[1].x, -self->denom[1].y);

  self->has_coefficients = TRUE;
}

static void
gsk_conic_curve_init_from_points (GskConicCurve          *self,
                                  const graphene_point_t  pts[4])
{
  self->op = GSK_PATH_CONIC;
  self->has_coefficients = FALSE;

  memcpy (self->points, pts, sizeof (graphene_point_t) * 4);
}

static void
gsk_conic_curve_init (GskCurve  *curve,
                      gskpathop  op)
{
  GskConicCurve *self = &curve->conic;

  gsk_conic_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_conic_curve_init_foreach (GskCurve               *curve,
                              GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts,
                              float                   weight)
{
  GskConicCurve *self = &curve->conic;

  g_assert (n_pts == 3);

  gsk_conic_curve_init_from_points (self,
                                    (graphene_point_t[4]) {
                                      pts[0],
                                      pts[1],
                                      GRAPHENE_POINT_INIT (weight, 0),
                                      pts[2]
                                    });
}

static void
gsk_conic_curve_print (const GskCurve *curve,
                       GString        *string)
{
  g_string_append_printf (string,
                          "M %g %g O %g %g %g %g %g",
                          curve->conic.points[0].x, curve->conic.points[0].y,
                          curve->conic.points[1].x, curve->conic.points[1].y,
                          curve->conic.points[3].x, curve->conic.points[3].y,
                          curve->conic.points[2].x);
}

static gskpathop
gsk_conic_curve_pathop (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_conic_curve_get_start_point (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return &self->points[0];
}

static const graphene_point_t *
gsk_conic_curve_get_end_point (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return &self->points[3];
}

static void
gsk_conic_curve_get_start_tangent (const GskCurve  *curve,
                                   graphene_vec2_t *tangent)
{
  const GskConicCurve *self = &curve->conic;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_conic_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskConicCurve *self = &curve->conic;

  get_tangent (&self->points[1], &self->points[3], tangent);
}

static inline void
gsk_curve_eval_quad (const graphene_point_t quad[3],
                     float                  progress,
                     graphene_point_t      *result)
{
  *result = GRAPHENE_POINT_INIT ((quad[0].x * progress + quad[1].x) * progress + quad[2].x,
                                 (quad[0].y * progress + quad[1].y) * progress + quad[2].y);
}

static inline void
gsk_conic_curve_eval_point (const GskConicCurve *self,
                            float                progress,
                            graphene_point_t    *point)
{
  graphene_point_t num, denom;

  gsk_curve_eval_quad (self->num, progress, &num);
  gsk_curve_eval_quad (self->denom, progress, &denom);

  *point = GRAPHENE_POINT_INIT (num.x / denom.x, num.y / denom.y);
}

static void
gsk_conic_curve_get_point (const GskCurve   *curve,
                           float             t,
                           graphene_point_t *pos)
{
  const GskConicCurve *self = &curve->conic;

  gsk_conic_curve_ensure_coefficents (self);

  gsk_conic_curve_eval_point (self, t, pos);
}

/* See M. Floater, Derivatives of rational Bezier curves */
static void
gsk_conic_curve_get_derivative_at (const GskCurve   *curve,
                                   float             t,
                                   graphene_point_t *value)
{
  const GskConicCurve *self = &curve->conic;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  graphene_point_t p[3], p1[2];
  float w1[2], w2;

  /* The tangent will be 0 in these corner cases, just
   * treat it like a line here.
   */
  if ((t <= 0.f && graphene_point_equal (&pts[0], &pts[1])) ||
      (t >= 1.f && graphene_point_equal (&pts[1], &pts[3])))
    {
      graphene_point_init (value, pts[3].x - pts[0].x, pts[3].y - pts[0].y);
      return;
    }

  p[0] = pts[0];
  p[1] = pts[1];
  p[2] = pts[3];

  w1[0] = (1 - t) + t*w;
  w1[1] = (1 - t)*w + t;

  w2 = (1 - t) * w1[0] + t * w1[1];

  p1[0].x = ((1 - t)*p[0].x + t*w*p[1].x)/w1[0];
  p1[0].y = ((1 - t)*p[0].y + t*w*p[1].y)/w1[0];
  p1[1].x = ((1 - t)*w*p[1].x + t*p[2].x)/w1[1];
  p1[1].y = ((1 - t)*w*p[1].y + t*p[2].y)/w1[1];

  value->x = 2 * (w1[0] * w1[1])/(w2*w2) * (p1[1].x - p1[0].x);
  value->y = 2 * (w1[0] * w1[1])/(w2*w2) * (p1[1].y - p1[0].y);
}

static void
gsk_conic_curve_get_tangent (const GskCurve   *curve,
                             float             t,
                             graphene_vec2_t  *tangent)
{
  graphene_point_t tmp;
  gsk_conic_curve_get_derivative_at (curve, t, &tmp);
  graphene_vec2_init (tangent, tmp.x, tmp.y);
  graphene_vec2_normalize (tangent, tangent);
}

/* See M. Floater, Derivatives of rational Bezier curves */
static float
gsk_conic_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  graphene_point_t p[3], p1[2];
  float w, w1[2], w2;
  graphene_vec2_t t1, t2, t3;

  w = curve->conic.points[2].x;

  p[0] = curve->conic.points[0];
  p[1] = curve->conic.points[1];
  p[2] = curve->conic.points[3];

  w1[0] = (1 - t) + t*w;
  w1[1] = (1 - t)*w + t;

  w2 = (1 - t)*w1[0] + t*w1[1];

  p1[0].x = ((1 - t)*p[0].x + t*w*p[1].x)/w1[0];
  p1[0].y = ((1 - t)*p[0].y + t*w*p[1].y)/w1[0];
  p1[1].x = ((1 - t)*w*p[1].x + t*p[2].x)/w1[1];
  p1[1].y = ((1 - t)*w*p[1].y + t*p[2].y)/w1[1];

  graphene_vec2_init (&t1, p[1].x - p[0].x, p[1].y - p[0].y);
  graphene_vec2_init (&t2, p[2].x - p[1].x, p[2].y - p[1].y);
  graphene_vec2_init (&t3, p1[1].x - p1[0].x, p1[1].y - p1[0].y);

  return 0.5 * ((w*pow3 (w2))/(pow3 (w1[0])*pow3 (w1[1]))) * (cross (&t1, &t2) / pow3 (graphene_vec2_length (&t3)));
}

static void
gsk_conic_curve_reverse (const GskCurve *curve,
                         GskCurve       *reverse)
{
  const GskConicCurve *self = &curve->conic;

  reverse->op = GSK_PATH_CONIC;
  reverse->conic.points[0] = self->points[3];
  reverse->conic.points[1] = self->points[1];
  reverse->conic.points[2] = self->points[2];
  reverse->conic.points[3] = self->points[0];
  reverse->conic.has_coefficients = FALSE;
}

static void
split_bezier3d_recurse (const graphene_point3d_t *p,
                        int                       l,
                        float                     t,
                        graphene_point3d_t       *left,
                        graphene_point3d_t       *right,
                        int                      *lpos,
                        int                      *rpos)
{
  if (l == 1)
    {
      left[*lpos] = p[0];
      right[*rpos] = p[0];
    }
  else
    {
      graphene_point3d_t *np;
      int i;

      np = g_alloca (sizeof (graphene_point3d_t) * (l - 1));
      for (i = 0; i < l - 1; i++)
        {
          if (i == 0)
            {
              left[*lpos] = p[i];
              (*lpos)++;
            }
          if (i + 1 == l - 1)
            {
              right[*rpos] = p[i + 1];
              (*rpos)--;
            }
          graphene_point3d_interpolate (&p[i], &p[i + 1], t, &np[i]);
        }
      split_bezier3d_recurse (np, l - 1, t, left, right, lpos, rpos);
    }
}

static void
split_bezier3d (const graphene_point3d_t *p,
                int                       l,
                float                     t,
                graphene_point3d_t       *left,
                graphene_point3d_t       *right)
{
  int lpos = 0;
  int rpos = l - 1;
  split_bezier3d_recurse (p, l, t, left, right, &lpos, &rpos);
}

static void
gsk_conic_curve_split (const GskCurve   *curve,
                       float             progress,
                       GskCurve         *start,
                       GskCurve         *end)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point3d_t p[3];
  graphene_point3d_t l[3], r[3];
  graphene_point_t left[4], right[4];
  float w;

  /* do de Casteljau in homogeneous coordinates... */
  w = self->points[2].x;
  p[0] = GRAPHENE_POINT3D_INIT (self->points[0].x, self->points[0].y, 1);
  p[1] = GRAPHENE_POINT3D_INIT (self->points[1].x * w, self->points[1].y * w, w);
  p[2] = GRAPHENE_POINT3D_INIT (self->points[3].x, self->points[3].y, 1);

  split_bezier3d (p, 3, progress, l, r);

  /* then project the control points down */
  left[0] = GRAPHENE_POINT_INIT (l[0].x / l[0].z, l[0].y / l[0].z);
  left[1] = GRAPHENE_POINT_INIT (l[1].x / l[1].z, l[1].y / l[1].z);
  left[3] = GRAPHENE_POINT_INIT (l[2].x / l[2].z, l[2].y / l[2].z);

  right[0] = GRAPHENE_POINT_INIT (r[0].x / r[0].z, r[0].y / r[0].z);
  right[1] = GRAPHENE_POINT_INIT (r[1].x / r[1].z, r[1].y / r[1].z);
  right[3] = GRAPHENE_POINT_INIT (r[2].x / r[2].z, r[2].y / r[2].z);

  /* normalize the outer weights to be 1 by using
   * the fact that weights w_i and c*w_i are equivalent
   * for any nonzero constant c
   */
  for (int i = 0; i < 3; i++)
    {
      l[i].z /= l[0].z;
      r[i].z /= r[2].z;
    }

  /* normalize the inner weight to be 1 by using
   * the fact that w_0*w_2/w_1^2 is a constant for
   * all equivalent weights.
   */
  left[2] = GRAPHENE_POINT_INIT (l[1].z / sqrt (l[2].z), 0);
  right[2] = GRAPHENE_POINT_INIT (r[1].z / sqrt (r[0].z), 0);

  if (start)
    gsk_curve_init (start, gsk_pathop_encode (GSK_PATH_CONIC, left));
  if (end)
    gsk_curve_init (end, gsk_pathop_encode (GSK_PATH_CONIC, right));
}

static void
gsk_conic_curve_segment (const GskCurve *curve,
                         float           start,
                         float           end,
                         GskCurve       *segment)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t start_num, start_denom;
  graphene_point_t mid_num, mid_denom;
  graphene_point_t end_num, end_denom;
  graphene_point_t ctrl_num, ctrl_denom;
  float mid;

  if (start <= 0.0f || end >= 1.0f)
    {
      if (start <= 0.0f)
        gsk_conic_curve_split (curve, end, segment, NULL);
      else if (end >= 1.0f)
        gsk_conic_curve_split (curve, start, NULL, segment);

      return;
    }

  gsk_conic_curve_ensure_coefficents (self);

  gsk_curve_eval_quad (self->num, start, &start_num);
  gsk_curve_eval_quad (self->denom, start, &start_denom);
  mid = (start + end) / 2;
  gsk_curve_eval_quad (self->num, mid, &mid_num);
  gsk_curve_eval_quad (self->denom, mid, &mid_denom);
  gsk_curve_eval_quad (self->num, end, &end_num);
  gsk_curve_eval_quad (self->denom, end, &end_denom);
  ctrl_num = GRAPHENE_POINT_INIT (2 * mid_num.x - (start_num.x + end_num.x) / 2,
                                  2 * mid_num.y - (start_num.y + end_num.y) / 2);
  ctrl_denom = GRAPHENE_POINT_INIT (2 * mid_denom.x - (start_denom.x + end_denom.x) / 2,
                                    2 * mid_denom.y - (start_denom.y + end_denom.y) / 2);

  gsk_conic_curve_init_from_points (&segment->conic,
                                    (graphene_point_t[4]) {
                                      GRAPHENE_POINT_INIT (start_num.x / start_denom.x,
                                                           start_num.y / start_denom.y),
                                      GRAPHENE_POINT_INIT (ctrl_num.x / ctrl_denom.x,
                                                           ctrl_num.y / ctrl_denom.y),
                                      GRAPHENE_POINT_INIT (ctrl_denom.x / sqrtf (start_denom.x * end_denom.x),
                                                           0),
                                      GRAPHENE_POINT_INIT (end_num.x / end_denom.x,
                                                           end_num.y / end_denom.y)
                                    });

}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_conic_curve_too_curvy (const graphene_point_t *start,
                           const graphene_point_t *mid,
                           const graphene_point_t *end,
                           float                  tolerance)
{
  return fabs ((start->x + end->x) * 0.5 - mid->x) > tolerance
      || fabs ((start->y + end->y) * 0.5 - mid->y) > tolerance;
}

static gboolean
gsk_conic_curve_decompose_subdivide (const GskConicCurve    *self,
                                     float                   tolerance,
                                     const graphene_point_t *start,
                                     float                   start_progress,
                                     const graphene_point_t *end,
                                     float                   end_progress,
                                     GskCurveAddLineFunc     add_line_func,
                                     gpointer                user_data)
{
  graphene_point_t mid;
  float mid_progress;

  mid_progress = (start_progress + end_progress) / 2;
  gsk_conic_curve_eval_point (self, mid_progress, &mid);

  if (!gsk_conic_curve_too_curvy (start, &mid, end, tolerance))
    return add_line_func (start, end, start_progress, end_progress, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
  if (end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (start, end, start_progress, end_progress, GSK_CURVE_LINE_REASON_SHORT, user_data);

  return gsk_conic_curve_decompose_subdivide (self, tolerance,
                                              start, start_progress, &mid, mid_progress,
                                              add_line_func, user_data)
      && gsk_conic_curve_decompose_subdivide (self, tolerance,
                                              &mid, mid_progress, end, end_progress,
                                              add_line_func, user_data);
}

static gboolean
gsk_conic_curve_decompose (const GskCurve      *curve,
                           float                tolerance,
                           GskCurveAddLineFunc  add_line_func,
                           gpointer             user_data)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t mid;

  gsk_conic_curve_ensure_coefficents (self);

  gsk_conic_curve_eval_point (self, 0.5, &mid);

  return gsk_conic_curve_decompose_subdivide (self,
                                              tolerance,
                                              &self->points[0],
                                              0.0f,
                                              &mid,
                                              0.5f,
                                              add_line_func,
                                              user_data)
      && gsk_conic_curve_decompose_subdivide (self,
                                              tolerance,
                                              &mid,
                                              0.5f,
                                              &self->points[3],
                                              1.0f,
                                              add_line_func,
                                              user_data);
}

/* See Floater, M: An analysis of cubic approximation schemes
 * for conic sections
 */
static void
cubic_approximation (const GskCurve *curve,
                     GskCurve       *cubic)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t p[4];
  float w = self->points[2].x;
  float w2 = w*w;
  float lambda;

  lambda = 2 * (6*w2 + 1 - sqrt (3*w2 + 1)) / (12*w2 + 3);

  p[0] = self->points[0];
  p[3] = self->points[3];
  graphene_point_interpolate (&self->points[0], &self->points[1], lambda, &p[1]);
  graphene_point_interpolate (&self->points[3], &self->points[1], lambda, &p[2]);

  gsk_curve_init (cubic, gsk_pathop_encode (GSK_PATH_CUBIC, p));
}

static gboolean
gsk_conic_is_close_to_cubic (const GskCurve *conic,
                             const GskCurve *cubic,
                             float           tolerance)
{
  float t[] = { 0.1, 0.5, 0.9 };
  graphene_point_t p0, p1;

  for (int i = 0; i < G_N_ELEMENTS (t); i++)
    {
      gsk_curve_get_point (conic, t[i], &p0);
      gsk_curve_get_point (cubic, t[i], &p1);
      if (graphene_point_distance (&p0, &p1, NULL, NULL) > tolerance)
        return FALSE;
    }

  return TRUE;
}

static gboolean gsk_conic_curve_decompose_curve (const GskCurve       *curve,
                                                 GskPathForeachFlags   flags,
                                                 float                 tolerance,
                                                 GskCurveAddCurveFunc  add_curve_func,
                                                 gpointer              user_data);

static gboolean
gsk_conic_curve_decompose_or_add (const GskCurve       *curve,
                                  const GskCurve       *cubic,
                                  float                 tolerance,
                                  GskCurveAddCurveFunc  add_curve_func,
                                  gpointer              user_data)
{
  if (graphene_point_equal (&curve->conic.points[0], &curve->conic.points[1]) ||
      graphene_point_equal (&curve->conic.points[1], &curve->conic.points[3]))
    {
      if (!graphene_point_equal (&curve->conic.points[0], &curve->conic.points[3]))
        return add_curve_func (GSK_PATH_LINE,
                               (graphene_point_t[2]) {
                                 curve->conic.points[0],
                                 curve->conic.points[3],
                               },
                               2, 0.f, user_data);
      else
        return TRUE;
    }
  else if (gsk_conic_is_close_to_cubic (curve, cubic, tolerance))
    return add_curve_func (GSK_PATH_CUBIC, cubic->cubic.points, 4, 0.f, user_data);
  else
    {
      GskCurve c1, c2;
      GskCurve cc1, cc2;

      gsk_conic_curve_split (curve, 0.5, &c1, &c2);

      cubic_approximation (&c1, &cc1);
      cubic_approximation (&c2, &cc2);

      return gsk_conic_curve_decompose_or_add (&c1, &cc1, tolerance, add_curve_func, user_data) &&
             gsk_conic_curve_decompose_or_add (&c2, &cc2, tolerance, add_curve_func, user_data);
    }
}

static gboolean
gsk_conic_curve_decompose_curve (const GskCurve       *curve,
                                 GskPathForeachFlags   flags,
                                 float                 tolerance,
                                 GskCurveAddCurveFunc  add_curve_func,
                                 gpointer              user_data)
{
  const GskConicCurve *self = &curve->conic;
  GskCurve c;

  if (flags & GSK_PATH_FOREACH_ALLOW_CONIC)
    return add_curve_func (GSK_PATH_CONIC,
                           (const graphene_point_t[3]) { self->points[0],
                                                         self->points[1],
                                                         self->points[3] },
                            3,
                            self->points[2].x,
                            user_data);

  if (flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
    {
      cubic_approximation (curve, &c);
      return gsk_conic_curve_decompose_or_add (curve, &c, tolerance, add_curve_func, user_data);
    }

  /* FIXME: Quadratic approximation */
  return gsk_conic_curve_decompose (curve,
                                    tolerance,
                                    gsk_curve_add_line_cb,
                                    &(AddLineData) { add_curve_func, user_data });
}

static void
gsk_conic_curve_get_bounds (const GskCurve *curve,
                            GskBoundingBox *bounds)
{
  const GskConicCurve *self = &curve->conic;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);
  gsk_bounding_box_expand (bounds, &pts[1]);
}

/* Solve N = 0 where N is the numerator of (P/Q)', with
 * P = (1-t)^2*a + 2*t*(1-t)*w*b + t^2*c
 * Q = (1-t)^2 + 2*t*(1-t)*w + t^2
 */
static int
get_conic_extrema (float a, float b, float c, float w, float t[4])
{
  float q, tt;
  int n = 0;
  float w2 = w*w;
  float wac = (w - 1)*(a - c);

  if (wac != 0)
    {
      q = - sqrt (a*a - 4*a*b*w2 + 4*a*c*w2 - 2*a*c + 4*b*b*w2 - 4*b*c*w2 + c*c);

      tt = (- q + 2*a*w - a - 2*b*w + c)/(2*wac);

      if (acceptable (tt))
        t[n++] = tt;

      tt = (q + 2*a*w - a - 2*b*w + c)/(2*wac);

      if (acceptable (tt))
        t[n++] = tt;
    }

  if (w * (b - c) != 0 && a == c)
    t[n++] = 0.5;

  if (w == 1 && a - 2*b + c != 0)
    {
      tt = (a - b) / (a - 2*b + c);
      if (acceptable (tt))
        t[n++] = tt;
    }

  return n;
}

static void
gsk_conic_curve_get_tight_bounds (const GskCurve *curve,
                                  GskBoundingBox *bounds)
{
  const GskConicCurve *self = &curve->conic;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  float t[8];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);

  n = 0;
  n += get_conic_extrema (pts[0].x, pts[1].x, pts[3].x, w, &t[n]);
  n += get_conic_extrema (pts[0].y, pts[1].y, pts[3].y, w, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_conic_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static int
gsk_conic_curve_get_crossing (const GskCurve         *curve,
                              const graphene_point_t *point)
{
  return get_crossing_by_bisection (curve, point);
}

static float
gsk_conic_curve_get_length_to (const GskCurve *curve,
                               float           t)
{
  return get_length_by_approximation (curve, t);
}

static float
gsk_conic_curve_get_at_length (const GskCurve *curve,
                               float           t,
                               float           epsilon)
{
  return get_t_by_bisection (curve, t, epsilon);
}

static const GskCurveClass GSK_CONIC_CURVE_CLASS = {
  gsk_conic_curve_init,
  gsk_conic_curve_init_foreach,
  gsk_conic_curve_print,
  gsk_conic_curve_pathop,
  gsk_conic_curve_get_start_point,
  gsk_conic_curve_get_end_point,
  gsk_conic_curve_get_start_tangent,
  gsk_conic_curve_get_end_tangent,
  gsk_conic_curve_get_point,
  gsk_conic_curve_get_tangent,
  gsk_conic_curve_reverse,
  gsk_conic_curve_get_curvature,
  gsk_conic_curve_split,
  gsk_conic_curve_segment,
  gsk_conic_curve_decompose,
  gsk_conic_curve_decompose_curve,
  gsk_conic_curve_get_bounds,
  gsk_conic_curve_get_tight_bounds,
  gsk_conic_curve_get_derivative_at,
  gsk_conic_curve_get_crossing,
  gsk_conic_curve_get_length_to,
  gsk_conic_curve_get_at_length,
};

/*  }}} */
/* {{{ API */

static const GskCurveClass *
get_class (GskPathOperation op)
{
  const GskCurveClass *klasses[] = {
    [GSK_PATH_CLOSE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_LINE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_QUAD] = &GSK_QUAD_CURVE_CLASS,
    [GSK_PATH_CUBIC] = &GSK_CUBIC_CURVE_CLASS,
    [GSK_PATH_CONIC] = &GSK_CONIC_CURVE_CLASS,
  };

  g_assert (op < G_N_ELEMENTS (klasses) && klasses[op] != NULL);

  return klasses[op];
}

void
gsk_curve_init (GskCurve  *curve,
                gskpathop  op)
{
  memset (curve, 0, sizeof (GskCurve));
  get_class (gsk_pathop_op (op))->init (curve, op);
}

void
gsk_curve_init_foreach (GskCurve               *curve,
                        GskPathOperation        op,
                        const graphene_point_t *pts,
                        gsize                   n_pts,
                        float                   weight)
{
  memset (curve, 0, sizeof (GskCurve));
  get_class (op)->init_foreach (curve, op, pts, n_pts, weight);
}

void
gsk_curve_print (const GskCurve *curve,
                 GString        *string)
{
  get_class (curve->op)->print (curve, string);
}

char *
gsk_curve_to_string (const GskCurve *curve)
{
  GString *s = g_string_new ("");
  gsk_curve_print (curve, s);
  return g_string_free (s, FALSE);
}

gskpathop
gsk_curve_pathop (const GskCurve *curve)
{
  return get_class (curve->op)->pathop (curve);
}

const graphene_point_t *
gsk_curve_get_start_point (const GskCurve *curve)
{
  return get_class (curve->op)->get_start_point (curve);
}

const graphene_point_t *
gsk_curve_get_end_point (const GskCurve *curve)
{
  return get_class (curve->op)->get_end_point (curve);
}

void
gsk_curve_get_start_tangent (const GskCurve  *curve,
                             graphene_vec2_t *tangent)
{
  get_class (curve->op)->get_start_tangent (curve, tangent);
}

void
gsk_curve_get_end_tangent (const GskCurve  *curve,
                           graphene_vec2_t *tangent)
{
  get_class (curve->op)->get_end_tangent (curve, tangent);
}

void
gsk_curve_get_point (const GskCurve   *curve,
                     float             progress,
                     graphene_point_t *pos)
{
  get_class (curve->op)->get_point (curve, progress, pos);
}

void
gsk_curve_get_tangent (const GskCurve   *curve,
                       float             progress,
                       graphene_vec2_t  *tangent)
{
  get_class (curve->op)->get_tangent (curve, progress, tangent);
}

float
gsk_curve_get_curvature (const GskCurve   *curve,
                         float             t,
                         graphene_point_t *center)
{
  float k;

  k = get_class (curve->op)->get_curvature (curve, t);

  if (center != NULL && k != 0)
    {
      graphene_point_t p;
      graphene_vec2_t tangent;
      float r;

      r = 1/k;
      gsk_curve_get_point (curve, t, &p);
      gsk_curve_get_tangent (curve, t, &tangent);
      center->x = p.x - r * graphene_vec2_get_y (&tangent);
      center->y = p.y + r * graphene_vec2_get_x (&tangent);
    }

  return k;
}

void
gsk_curve_reverse (const GskCurve *curve,
                   GskCurve       *reverse)
{
  get_class (curve->op)->reverse (curve, reverse);
}

void
gsk_curve_split (const GskCurve *curve,
                 float           progress,
                 GskCurve       *start,
                 GskCurve       *end)
{
  get_class (curve->op)->split (curve, progress, start, end);
}

void
gsk_curve_segment (const GskCurve *curve,
                   float           start,
                   float           end,
                   GskCurve       *segment)
{
  if (start <= 0 && end >= 1)
    {
      *segment = *curve;
      return;
    }

  get_class (curve->op)->segment (curve, start, end, segment);
}

gboolean
gsk_curve_decompose (const GskCurve      *curve,
                     float                tolerance,
                     GskCurveAddLineFunc  add_line_func,
                     gpointer             user_data)
{
  return get_class (curve->op)->decompose (curve, tolerance, add_line_func, user_data);
}

gboolean
gsk_curve_decompose_curve (const GskCurve       *curve,
                           GskPathForeachFlags   flags,
                           float                 tolerance,
                           GskCurveAddCurveFunc  add_curve_func,
                           gpointer              user_data)
{
  return get_class (curve->op)->decompose_curve (curve, flags, tolerance, add_curve_func, user_data);
}

void
gsk_curve_get_bounds (const GskCurve  *curve,
                      GskBoundingBox  *bounds)
{
  get_class (curve->op)->get_bounds (curve, bounds);
}

void
gsk_curve_get_tight_bounds (const GskCurve  *curve,
                            GskBoundingBox  *bounds)
{
  get_class (curve->op)->get_tight_bounds (curve, bounds);
}

void
gsk_curve_get_derivative_at (const GskCurve  *curve,
                             float            t,
                             graphene_point_t *value)
{
  get_class (curve->op)->get_derivative_at (curve, t, value);
}

int
gsk_curve_get_crossing (const GskCurve         *curve,
                        const graphene_point_t *point)
{
  return get_class (curve->op)->get_crossing (curve, point);
}

static gboolean
project_point_onto_line (const GskCurve         *curve,
                         const graphene_point_t *point,
                         float                   threshold,
                         float                  *out_distance,
                         float                  *out_t)
{
  const graphene_point_t *a = gsk_curve_get_start_point (curve);
  const graphene_point_t *b = gsk_curve_get_end_point (curve);
  graphene_vec2_t n, ap;
  graphene_point_t p;

  if (graphene_point_equal (a, b))
    {
      *out_t = 0;
      *out_distance = graphene_point_distance (point, a, NULL, NULL);
    }
  else
    {
      graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
      graphene_vec2_init (&ap, point->x - a->x, point->y - a->y);

      *out_t = graphene_vec2_dot (&n, &ap) / graphene_vec2_dot (&n, &n);
      *out_t = CLAMP (*out_t, 0, 1);

      graphene_point_interpolate (a, b, *out_t, &p);
      *out_distance = graphene_point_distance (point, &p, NULL, NULL);
    }

  return *out_distance <= threshold;
}

static float
get_segment_bounding_sphere (const GskCurve  *curve,
                             float            t1,
                             float            t2,
                             graphene_point_t *center)
{
  GskCurve c;
  GskBoundingBox bounds;

  gsk_curve_segment (curve, t1, t2, &c);
  gsk_curve_get_tight_bounds (&c, &bounds);
  graphene_point_interpolate (&bounds.min, &bounds.max, 0.5, center);
  return graphene_point_distance (center, &bounds.min, NULL, NULL);
}

static gboolean
find_closest_point (const GskCurve         *curve,
                    const graphene_point_t *point,
                    float                   threshold,
                    float                   t1,
                    float                   t2,
                    float                  *out_distance,
                    float                  *out_t)
{
  graphene_point_t center;
  float radius;
  float t, d, nt;

  radius = get_segment_bounding_sphere (curve, t1, t2, &center);
  if (graphene_point_distance (&center, point, NULL, NULL) > threshold + radius)
    return FALSE;

  d = INFINITY;
  t = (t1 + t2) / 2;

  if (fabs (t1 - t2) < 0.001)
    {
      graphene_point_t p;
      gsk_curve_get_point (curve, t, &p);
      d = graphene_point_distance (point, &p, NULL, NULL);
      nt = t;
    }
  else
    {
      float dd, tt;

      dd = INFINITY;
      nt = 0;

      if (find_closest_point (curve, point, threshold, t1, t, &dd, &tt))
        {
          d = dd;
          nt = tt;
        }

      if (find_closest_point (curve, point, MIN (dd, threshold), t, t2, &dd, &tt))
        {
          d = dd;
          nt = tt;
        }
    }

  if (d < threshold)
    {
      *out_distance = d;
      *out_t = nt;
      return TRUE;
    }
  else
    {
      *out_distance = INFINITY;
      *out_t = 0;
      return FALSE;
    }
}

gboolean
gsk_curve_get_closest_point (const GskCurve         *curve,
                             const graphene_point_t *point,
                             float                   threshold,
                             float                  *out_dist,
                             float                  *out_t)
{
  if (curve->op == GSK_PATH_LINE || curve->op == GSK_PATH_CLOSE)
    return project_point_onto_line (curve, point, threshold, out_dist, out_t);
  else
    return find_closest_point (curve, point, threshold, 0, 1, out_dist, out_t);
}

float
gsk_curve_get_length_to (const GskCurve *curve,
                         float           t)
{
  return get_class (curve->op)->get_length_to (curve, t);
}

float
gsk_curve_get_length (const GskCurve *curve)
{
  return gsk_curve_get_length_to (curve, 1);
}

/* Compute the inverse of the arclength using bisection,
 * to a given precision
 */
float
gsk_curve_at_length (const GskCurve *curve,
                     float           length,
                     float           epsilon)
{
  return get_class (curve->op)->get_at_length (curve, length, epsilon);
}

static inline void
_sincosf (float  angle,
          float *out_s,
          float *out_c)
{
#ifdef HAVE_SINCOSF
      sincosf (angle, out_s, out_c);
#else
      *out_s = sinf (angle);
      *out_c = cosf (angle);
#endif
}

static void
align_points (const graphene_point_t *p,
              const graphene_point_t *a,
              const graphene_point_t *b,
              graphene_point_t       *q,
              int                     n)
{
  graphene_vec2_t n1;
  float angle;
  float s, c;

  get_tangent (a, b, &n1);
  angle = - atan2f (graphene_vec2_get_y (&n1), graphene_vec2_get_x (&n1));
  _sincosf (angle, &s, &c);

  for (int i = 0; i < n; i++)
    {
      q[i].x = (p[i].x - a->x) * c - (p[i].y - a->y) * s;
      q[i].y = (p[i].x - a->x) * s + (p[i].y - a->y) * c;
    }
}

static int
filter_allowable (float t[3],
                  int   n)
{
  float g[3];
  int j = 0;

  for (int i = 0; i < n; i++)
    if (0 < t[i] && t[i] < 1)
      g[j++] = t[i];
  for (int i = 0; i < j; i++)
    t[i] = g[i];
  return j;
}

/* find solutions for at^2 + bt + c = 0 */
static int
solve_quadratic (float a, float b, float c, float t[2])
{
  float d;
  int n = 0;

  if (fabsf (a) > 0.0001)
    {
      if (b*b > 4*a*c)
        {
          d = sqrtf (b*b - 4*a*c);
          t[n++] = (-b + d)/(2*a);
          t[n++] = (-b - d)/(2*a);
        }
      else
        {
          t[n++] = -b / (2*a);
        }
    }
  else if (fabsf (b) > 0.0001)
    {
      t[n++] = -c / b;
    }

  return n;
}

int
gsk_curve_get_curvature_points (const GskCurve *curve,
                                float           t[3])
{
  const graphene_point_t *pts = curve->cubic.points;
  graphene_point_t p[4];
  float a, b, c, d;
  float x, y, z;
  int n;

  if (curve->op != GSK_PATH_CUBIC)
    return 0; /* FIXME */

  align_points (pts, &pts[0], &pts[3], p, 4);

  a = p[2].x * p[1].y;
  b = p[3].x * p[1].y;
  c = p[1].x * p[2].y;
  d = p[3].x * p[2].y;

  x = - 3*a + 2*b + 3*c - d;
  y = 3*a - b - 3*c;
  z = c - a;

  n = solve_quadratic (x, y, z, t);
  return filter_allowable (t, n);
}

/* Find cusps inside the open interval from 0 to 1.
 *
 * According to Stone & deRose, A Geometric Characterization
 * of Parametric Cubic curves, a necessary and sufficient
 * condition is that the first derivative vanishes.
 */
int
gsk_curve_get_cusps (const GskCurve *curve,
                     float           t[2])
{
  const graphene_point_t *pts = curve->cubic.points;
  graphene_point_t p[3];
  float ax, bx, cx;
  float ay, by, cy;
  float tx[3];
  int nx;
  int n = 0;

  if (curve->op != GSK_PATH_CUBIC)
    return 0;

  p[0].x = 3 * (pts[1].x - pts[0].x);
  p[0].y = 3 * (pts[1].y - pts[0].y);
  p[1].x = 3 * (pts[2].x - pts[1].x);
  p[1].y = 3 * (pts[2].y - pts[1].y);
  p[2].x = 3 * (pts[3].x - pts[2].x);
  p[2].y = 3 * (pts[3].y - pts[2].y);

  ax = p[0].x - 2 * p[1].x + p[2].x;
  bx = - 2 * p[0].x + 2 * p[1].x;
  cx = p[0].x;

  nx = solve_quadratic (ax, bx, cx, tx);
  nx = filter_allowable (tx, nx);

  ay = p[0].y - 2 * p[1].y + p[2].y;
  by = - 2 * p[0].y + 2 * p[1].y;
  cy = p[0].y;

  for (int i = 0; i < nx; i++)
    {
      float ti = tx[i];

      if (0 < ti && ti < 1 &&
          fabsf (ay * ti * ti + by * ti + cy) < 0.001)
        t[n++] = ti;
    }

  return n;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
