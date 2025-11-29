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

#include <gtk/gtk.h>

#include "path-utils.h"

typedef struct {
  GskPathOperation op;
  graphene_point_t pts[4];
} PathOperation;

static void
_g_string_append_double (GString *string,
                         double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, d);
  g_string_append (string, buf);
}

static void
_g_string_append_point (GString                *string,
                        const graphene_point_t *pt)
{
  _g_string_append_double (string, pt->x);
  g_string_append_c (string, ' ');
  _g_string_append_double (string, pt->y);
}

static void
path_operation_print (const PathOperation *p,
                      GString             *string)
{
  switch (p->op)
  {
    case GSK_PATH_MOVE:
      g_string_append (string, "M ");
      _g_string_append_point (string, &p->pts[0]);
      break;

    case GSK_PATH_CLOSE:
      g_string_append (string, " Z");
      break;

    case GSK_PATH_LINE:
      g_string_append (string, " L ");
      _g_string_append_point (string, &p->pts[1]);
      break;

    case GSK_PATH_QUAD:
      g_string_append (string, " Q ");
      _g_string_append_point (string, &p->pts[1]);
      g_string_append (string, ", ");
      _g_string_append_point (string, &p->pts[2]);
      break;

    case GSK_PATH_CUBIC:
      g_string_append (string, " C ");
      _g_string_append_point (string, &p->pts[1]);
      g_string_append (string, ", ");
      _g_string_append_point (string, &p->pts[2]);
      g_string_append (string, ", ");
      _g_string_append_point (string, &p->pts[3]);
      break;

    case GSK_PATH_CONIC:
      g_string_append (string, " O ");
      _g_string_append_point (string, &p->pts[1]);
      g_string_append (string, ", ");
      _g_string_append_point (string, &p->pts[3]);
      g_string_append (string, ", ");
      _g_string_append_double (string, p->pts[2].x);
      break;

    default:
      g_assert_not_reached();
      return;
  }
}

static gboolean
path_operation_equal (const PathOperation *p1,
                      const PathOperation *p2,
                      float                epsilon)
{
  if (p1->op != p2->op)
    return FALSE;

  /* No need to compare pts[0] for most ops, that's just
   * duplicate work. */
  switch (p1->op)
    {
      case GSK_PATH_MOVE:
        return graphene_point_near (&p1->pts[0], &p2->pts[0], epsilon);

      case GSK_PATH_LINE:
      case GSK_PATH_CLOSE:
        return graphene_point_near (&p1->pts[1], &p2->pts[1], epsilon);

      case GSK_PATH_QUAD:
        return graphene_point_near (&p1->pts[1], &p2->pts[1], epsilon)
            && graphene_point_near (&p1->pts[2], &p2->pts[2], epsilon);

      case GSK_PATH_CUBIC:
        return graphene_point_near (&p1->pts[1], &p2->pts[1], epsilon)
            && graphene_point_near (&p1->pts[2], &p2->pts[2], epsilon)
            && graphene_point_near (&p1->pts[3], &p2->pts[3], epsilon);

      case GSK_PATH_CONIC:
        return graphene_point_near (&p1->pts[1], &p2->pts[1], epsilon)
            && graphene_point_near (&p1->pts[3], &p2->pts[3], epsilon);

      default:
        g_return_val_if_reached (FALSE);
    }
}

static gboolean 
collect_path_operation_cb (GskPathOperation        op,
                           const graphene_point_t *pts,
                           gsize                   n_pts,
                           float                   weight,
                           gpointer                user_data)
{
  g_array_append_vals (user_data,
                       (PathOperation[1])  { {
                         op,
                         {
                           GRAPHENE_POINT_INIT(pts[0].x, pts[0].y),
                           GRAPHENE_POINT_INIT(n_pts > 1 ? pts[1].x : 0,
                                               n_pts > 1 ? pts[1].y : 0),
                           GRAPHENE_POINT_INIT(n_pts > 2 ? pts[2].x : 0,
                                               n_pts > 2 ? pts[2].y : 0),
                           GRAPHENE_POINT_INIT(n_pts > 3 ? pts[3].x : 0,
                                               n_pts > 3 ? pts[3].y : 0)
                         },
                       } },
                       1);
  return TRUE;
}

static GArray *
collect_path (GskPath *path)
{
  GArray *array = g_array_new (FALSE, FALSE, sizeof (PathOperation));

  /* Use -1 here because we want all the flags, even future additions */
  gsk_path_foreach (path, -1, collect_path_operation_cb, array);

  return array;
}

void
assert_path_equal_func (const char *domain,
                        const char *file,
                        int         line,
                        const char *func,
                        GskPath    *path1,
                        GskPath    *path2,
                        float       epsilon)
{
  GArray *ops1, *ops2;
  guint i;

  ops1 = collect_path (path1);
  ops2 = collect_path (path2);

  for (i = 0; i < MAX (ops1->len, ops2->len); i++)
    {
      PathOperation *op1 = i < ops1->len ? &g_array_index (ops1, PathOperation, i) : NULL;
      PathOperation *op2 = i < ops2->len ? &g_array_index (ops2, PathOperation, i) : NULL;

      if (op1 == NULL || op2 == NULL || !path_operation_equal (op1, op2, epsilon))
        {
          GString *string;
          guint j;

          /* Find the operation we start to print */
          for (j = i; j-- > 0; )
            {
              PathOperation *op = &g_array_index (ops1, PathOperation, j);
              if (op->op == GSK_PATH_MOVE)
                break;
              if (j + 3 == i)
                {
                  j = i - 1;
                  break;
                }
            }

          string = g_string_new (j == 0 ? "" : "... ");
          for (; j < i; j++)
            {
              PathOperation *op = &g_array_index (ops1, PathOperation, j);
              path_operation_print (op, string);
              g_string_append_c (string, ' ');
            }

          g_string_append (string, "\\\n    ");
          if (op1)
            {
              path_operation_print (op1, string);
              if (ops1->len > i + 1)
                g_string_append (string, " ...");
            }
          g_string_append (string, "\n    ");
          if (op1)
            {
              path_operation_print (op2, string);
              if (ops2->len > i + 1)
                g_string_append (string, " ...");
            }

          g_assertion_message (domain, file, line, func, string->str);

          g_string_free (string, TRUE);
        }
    }

  g_array_free (ops1, TRUE);
  g_array_free (ops2, TRUE);
}
