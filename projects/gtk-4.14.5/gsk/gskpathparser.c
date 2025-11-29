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

#include "gskpathprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"
#include "gskpathpoint.h"
#include "gskcontourprivate.h"

static void
skip_whitespace (const char **p)
{
  while (g_ascii_isspace (**p))
    (*p)++;
}

static void
skip_optional_comma (const char **p)
{
  skip_whitespace (p);
  if (**p == ',')
    (*p)++;
}

static gboolean
parse_number (const char **p,
              double      *c)
{
  char *e;
  *c = g_ascii_strtod (*p, &e);
  if (e == *p)
    return FALSE;
  *p = e;
  skip_optional_comma (p);
  return TRUE;
}

static gboolean
parse_coordinate (const char **p,
                  double      *c)
{
  return parse_number (p, c);
}

static gboolean
parse_coordinate_pair (const char **p,
                       double      *x,
                       double      *y)
{
  double xx, yy;
  const char *o = *p;

  if (!parse_coordinate (p, &xx))
    {
      *p = o;
      return FALSE;
    }
  if (!parse_coordinate (p, &yy))
    {
      *p = o;
      return FALSE;
    }

  *x = xx;
  *y = yy;

  return TRUE;
}

static gboolean
parse_nonnegative_number (const char **p,
                          double      *x)
{
  const char *o = *p;
  double n;

  if (!parse_number (p, &n))
    return FALSE;

  if (n < 0)
    {
      *p = o;
      return FALSE;
    }

  *x = n;

  return TRUE;
}

/* This fixes a flaw in our use of strchr() below:
 *
 * If p already points at the end of the string,
 * we misinterpret strchr ("xyz", *p) returning
 * non-NULL to mean that we can increment p.
 *
 * But strchr() will return a pointer to the
 * final NUL byte in this case, and we walk off
 * the end of the string. Oops
 */
static inline char *
_strchr (const char *str,
           int         c)
{
  if (c == 0)
    return NULL;
  else
    return strchr (str, c);
}

static gboolean
parse_flag (const char **p,
            gboolean    *f)
{
  skip_whitespace (p);
  if (_strchr ("01", **p))
    {
      *f = **p == '1';
      (*p)++;
      skip_optional_comma (p);
      return TRUE;
    }

  return FALSE;
}

static gboolean
parse_command (const char **p,
               char        *cmd)
{
  char *s;
  const char *allowed;

  if (*cmd == 'X')
    allowed = "mM";
  else
    allowed = "mMhHvVzZlLcCsStTqQaAoO";

  skip_whitespace (p);
  s = _strchr (allowed, **p);
  if (s)
    {
      *cmd = *s;
      (*p)++;
      return TRUE;
    }
  return FALSE;
}

static gboolean
parse_string (const char **p,
              const char  *s)
{ 
  int len = strlen (s);
  if (strncmp (*p, s, len) != 0)
    return FALSE;
  (*p) += len;
  return TRUE;
}

#define NEAR(x, y) (fabs ((x) - (y)) < 0.001)

static gboolean
is_rect (double x0, double y0,
         double x1, double y1,
         double x2, double y2,
         double x3, double y3)
{
  return NEAR (x0, x3) && NEAR (x1, x2) &&
         NEAR (y0, y1) && NEAR (y2, y3) &&
         x0 < x1 && y1 < y2;
}

static gboolean
is_line (double x0, double y0,
         double x1, double y1,
         double x2, double y2,
         double x3, double y3)
{
  if (NEAR (y0, y3))
    return x0 <= x1 && x1 <= x2 && x2 <= x3 &&
           NEAR (y0, y1) && NEAR (y0, y2) && NEAR (y0, y3);
  else
    return y0 <= y1 && y1 <= y2 && y2 <= y3 &&
           NEAR (x0, x1) && NEAR (x0, x2) && NEAR (x0, x3);
}

static gboolean
parse_rectangle (const char **p,
                 double      *x,
                 double      *y,
                 double      *w,
                 double      *h)
{
  const char *o = *p;
  double w2;

  if (parse_coordinate_pair (p, x, y) &&
      parse_string (p, "h") &&
      parse_coordinate (p, w) &&
      parse_string (p, "v") &&
      parse_coordinate (p, h) &&
      parse_string (p, "h") &&
      parse_coordinate (p, &w2) &&
      parse_string (p, "z") &&
      w2 == -*w && *w >= 0 && *h >= 0)
    {
      skip_whitespace (p);

      return TRUE;
    }

  *p = o;
  return FALSE;
}

static gboolean
parse_circle (const char **p,
              double      *cx,
              double      *cy,
              double      *r)
{
  const char *o = *p;
  double x0, y0, x1, y1, x2, y2, x3, y3;
  double x4, y4, x5, y5, x6, y6, x7, y7;
  double x8, y8, w0, w1, w2, w3;
  double rr;

  if (parse_coordinate_pair (p, &x0, &y0) &&
      parse_string (p, "o") &&
      parse_coordinate_pair (p, &x1, &y1) &&
      parse_coordinate_pair (p, &x2, &y2) &&
      parse_nonnegative_number (p, &w0) &&
      parse_string (p, "o") &&
      parse_coordinate_pair (p, &x3, &y3) &&
      parse_coordinate_pair (p, &x4, &y4) &&
      parse_nonnegative_number (p, &w1) &&
      parse_string (p, "o") &&
      parse_coordinate_pair (p, &x5, &y5) &&
      parse_coordinate_pair (p, &x6, &y6) &&
      parse_nonnegative_number (p, &w2) &&
      parse_string (p, "o") &&
      parse_coordinate_pair (p, &x7, &y7) &&
      parse_coordinate_pair (p, &x8, &y8) &&
      parse_nonnegative_number (p, &w3) &&
      parse_string (p, "z"))
    {
      rr = y1;

      if (x1 == 0   && y1 == rr  &&
          x2 == -rr && y2 == rr  &&
          x3 == -rr && y3 == 0   &&
          x4 == -rr && y4 == -rr &&
          x5 == 0   && y5 == -rr &&
          x6 == rr  && y6 == -rr &&
          x7 == rr  && y7 == 0   &&
          x8 == rr  && y8 == rr &&
          NEAR (w0, M_SQRT1_2) && NEAR (w1, M_SQRT1_2) &&
          NEAR (w2, M_SQRT1_2) && NEAR (w3, M_SQRT1_2))
        {
          *cx = x0 - rr;
          *cy = y0;
          *r = rr;

          skip_whitespace (p);

          return TRUE;
        }
    }

  *p = o;
  return FALSE;
}

static gboolean
parse_rounded_rect (const char     **p,
                    GskRoundedRect  *rr)
{
  const char *o = *p;
  double x0, y0, x1, y1, x2, y2, x3, y3;
  double x4, y4, x5, y5, x6, y6, x7, y7;
  double x8, y8, x9, y9, x10, y10, x11, y11;
  double x12, y12, w0, w1, w2, w3;

  if (parse_coordinate_pair (p, &x0, &y0) &&
      parse_string (p, "L") &&
      parse_coordinate_pair (p, &x1, &y1) &&
      parse_string (p, "O") &&
      parse_coordinate_pair (p, &x2, &y2) &&
      parse_coordinate_pair (p, &x3, &y3) &&
      parse_nonnegative_number (p, &w0) &&
      parse_string (p, "L") &&
      parse_coordinate_pair (p, &x4, &y4) &&
      parse_string (p, "O") &&
      parse_coordinate_pair (p, &x5, &y5) &&
      parse_coordinate_pair (p, &x6, &y6) &&
      parse_nonnegative_number (p, &w1) &&
      parse_string (p, "L") &&
      parse_coordinate_pair (p, &x7, &y7) &&
      parse_string (p, "O") &&
      parse_coordinate_pair (p, &x8, &y8) &&
      parse_coordinate_pair (p, &x9, &y9) &&
      parse_nonnegative_number (p, &w2) &&
      parse_string (p, "L") &&
      parse_coordinate_pair (p, &x10, &y10) &&
      parse_string (p, "O") &&
      parse_coordinate_pair (p, &x11, &y11) &&
      parse_coordinate_pair (p, &x12, &y12) &&
      parse_nonnegative_number (p, &w3) &&
      parse_string (p, "Z"))
    {
      if (NEAR (x0, x12) && NEAR (y0, y12) &&
          is_rect (x11, y11, x2, y2, x5, y5, x8, y8) &&
          is_line (x11, y11, x0, y0, x1, y1, x2, y2) &&
          is_line (x2, y2, x3, y3, x4, y4, x5, y5) &&
          is_line (x8, y8, x7, y7, x6, y6, x5, y5) &&
          is_line (x11, y11, x10, y10, x9, y9, x8, y8) &&
          NEAR (w0, M_SQRT1_2) && NEAR (w1, M_SQRT1_2) &&
          NEAR (w2, M_SQRT1_2) && NEAR (w3, M_SQRT1_2))
        {
          rr->bounds = GRAPHENE_RECT_INIT (x11, y11, x5 - x11, y5 - y11);
          rr->corner[GSK_CORNER_TOP_LEFT] = GRAPHENE_SIZE_INIT (x12 - x11, y10 - y11);
          rr->corner[GSK_CORNER_TOP_RIGHT] = GRAPHENE_SIZE_INIT (x2 - x1, y3 - y2);
          rr->corner[GSK_CORNER_BOTTOM_RIGHT] = GRAPHENE_SIZE_INIT (x5 - x6, y5 - y4);
          rr->corner[GSK_CORNER_BOTTOM_LEFT] = GRAPHENE_SIZE_INIT (x7 - x8, y8 - y9);

          skip_whitespace (p);

          return TRUE;
        }
    }

  *p = o;
  return FALSE;
}

#undef NEAR

/**
 * gsk_path_parse:
 * @string: a string
 *
 * This is a convenience function that constructs a `GskPath`
 * from a serialized form.
 *
 * The string is expected to be in (a superset of)
 * [SVG path syntax](https://www.w3.org/TR/SVG11/paths.html#PathData),
 * as e.g. produced by [method@Gsk.Path.to_string].
 *
 * A high-level summary of the syntax:
 *
 * - `M x y` Move to `(x, y)`
 * - `L x y` Add a line from the current point to `(x, y)`
 * - `Q x1 y1 x2 y2` Add a quadratic Bézier from the current point to `(x2, y2)`, with control point `(x1, y1)`
 * - `C x1 y1 x2 y2 x3 y3` Add a cubic Bézier from the current point to `(x3, y3)`, with control points `(x1, y1)` and `(x2, y2)`
 * - `Z` Close the contour by drawing a line back to the start point
 * - `H x` Add a horizontal line from the current point to the given x value
 * - `V y` Add a vertical line from the current point to the given y value
 * - `T x2 y2` Add a quadratic Bézier, using the reflection of the previous segments' control point as control point
 * - `S x2 y2 x3 y3` Add a cubic Bézier, using the reflection of the previous segments' second control point as first control point
 * - `A rx ry r l s x y` Add an elliptical arc from the current point to `(x, y)` with radii rx and ry. See the SVG documentation for how the other parameters influence the arc.
 * - `O x1 y1 x2 y2 w` Add a rational quadratic Bézier from the current point to `(x2, y2)` with control point `(x1, y1)` and weight `w`.
 *
 * All the commands have lowercase variants that interpret coordinates
 * relative to the current point.
 *
 * The `O` command is an extension that is not supported in SVG.
 *
 * Returns: (nullable): a new `GskPath`, or `NULL` if @string could not be parsed
 *
 * Since: 4.14
 */
GskPath *
gsk_path_parse (const char *string)
{
  GskPathBuilder *builder;
  double x, y;
  double prev_x1, prev_y1;
  double path_x, path_y;
  const char *p;
  char cmd;
  char prev_cmd;
  gboolean after_comma;
  gboolean repeat;

  builder = gsk_path_builder_new ();

  cmd = 'X';
  path_x = path_y = 0;
  x = y = 0;
  prev_x1 = prev_y1 = 0;
  after_comma = FALSE;

  p = string;
  while (*p)
    {
      prev_cmd = cmd;
      repeat = !parse_command (&p, &cmd);

      if (after_comma && !repeat)
        goto error;

      switch (cmd)
        {
        case 'X':
          goto error;

        case 'Z':
        case 'z':
          if (repeat)
            goto error;
          else
            {
              gsk_path_builder_close (builder);
              x = path_x;
              y = path_y;
            }
          break;

        case 'M':
        case 'm':
          {
            double x1, y1, w, h, r;
            GskRoundedRect rr;

            /* Look for special contours */
            if (parse_rectangle (&p, &x1, &y1, &w, &h))
              {
                gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x1, y1, w, h));
                if (_strchr ("zZX", prev_cmd))
                  {
                    path_x = x1;
                    path_y = y1;
                  }

                x = x1;
                y = y1;
              }
            else if (parse_circle (&p, &x1, &y1, &r))
              {
                gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (x1, y1), r);

                if (_strchr ("zZX", prev_cmd))
                  {
                    path_x = x1 + r;
                    path_y = y1;
                  }

                x = x1 + r;
                y = y1;
              }
            else if (parse_rounded_rect (&p, &rr))
              {
                gsk_path_builder_add_rounded_rect (builder, &rr);

                if (_strchr ("zZX", prev_cmd))
                  {
                    path_x = rr.bounds.origin.x + rr.corner[GSK_CORNER_TOP_LEFT].width;
                    path_y = rr.bounds.origin.y;
                  }

                x = rr.bounds.origin.x + rr.corner[GSK_CORNER_TOP_LEFT].width;
                y = rr.bounds.origin.y;
              }
            else if (parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'm')
                  {
                    x1 += x;
                    y1 += y;
                  }

                if (repeat)
                  gsk_path_builder_line_to (builder, x1, y1);
                else
                  {
                    gsk_path_builder_move_to (builder, x1, y1);
                    if (_strchr ("zZX", prev_cmd))
                      {
                        path_x = x1;
                        path_y = y1;
                      }
                  }

                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'L':
        case 'l':
          {
            double x1, y1;

            if (parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'l')
                  {
                    x1 += x;
                    y1 += y;
                  }

                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x1, y1);
                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'H':
        case 'h':
          {
            double x1;

            if (parse_coordinate (&p, &x1))
              {
                if (cmd == 'h')
                  x1 += x;
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x1, y);
                x = x1;
              }
            else
              goto error;
          }
          break;

        case 'V':
        case 'v':
          {
            double y1;

            if (parse_coordinate (&p, &y1))
              {
                if (cmd == 'v')
                  y1 += y;
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x, y1);
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'C':
        case 'c':
          {
            double x0, y0, x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x0, &y0) &&
                parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 'c')
                  {
                    x0 += x;
                    y0 += y;
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_cubic_to (builder, x0, y0, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'S':
        case 's':
          {
            double x0, y0, x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 's')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (_strchr ("CcSs", prev_cmd))
                  {
                    x0 = 2 * x - prev_x1;
                    y0 = 2 * y - prev_y1;
                  }
                else
                  {
                    x0 = x;
                    y0 = y;
                  }
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_cubic_to (builder, x0, y0, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'Q':
        case 'q':
          {
            double x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 'q')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_quad_to (builder, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'T':
        case 't':
          {
            double x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 't')
                  {
                    x2 += x;
                    y2 += y;
                  }
                if (_strchr ("QqTt", prev_cmd))
                  {
                    x1 = 2 * x - prev_x1;
                    y1 = 2 * y - prev_y1;
                  }
                else
                  {
                    x1 = x;
                    y1 = y;
                  }
                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_quad_to (builder, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'O':
        case 'o':
          {
            double x1, y1, x2, y2, weight;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2) &&
                parse_nonnegative_number (&p, &weight))
              {
                if (cmd == 'o')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (_strchr ("zZ", prev_cmd))
                  { 
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_conic_to (builder, x1, y1, x2, y2, weight);
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'A':
        case 'a':
          {
            double rx, ry;
            double x_axis_rotation;
            int large_arc, sweep;
            double x1, y1;

            if (parse_nonnegative_number (&p, &rx) &&
                parse_nonnegative_number (&p, &ry) &&
                parse_number (&p, &x_axis_rotation) &&
                parse_flag (&p, &large_arc) &&
                parse_flag (&p, &sweep) &&
                parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'a')
                  {
                    x1 += x;
                    y1 += y;
                  }

                if (_strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_svg_arc_to (builder,
                                             rx, ry, x_axis_rotation,
                                             large_arc, sweep,
                                             x1, y1);
                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        default:
          goto error;
        }

      after_comma = (p > string) && p[-1] == ',';
    }

  if (after_comma)
    goto error;

  return gsk_path_builder_free_to_path (builder);

error:
  //g_warning ("Can't parse string '%s' as GskPath, error at %ld", string, p - string);
  gsk_path_builder_unref (builder);

  return NULL;
}

/* vim:set foldmethod=marker expandtab: */
