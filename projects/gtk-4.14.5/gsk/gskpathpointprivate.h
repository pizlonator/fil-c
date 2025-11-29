#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

#define GSK_PATH_POINT_INIT(c,i,tt) ((GskPathPoint){ .contour=(c), .idx=(i), .t=(tt) })

static inline gboolean
gsk_path_point_valid (const GskPathPoint *point,
                      GskPath            *path)
{
  const GskContour *contour;
  gsize n_ops;

  if (point == NULL)
    return FALSE;

  if (path == NULL)
    return TRUE;

  if (point->contour >= gsk_path_get_n_contours (path))
    return FALSE;

  contour = gsk_path_get_contour (path, point->contour);
  n_ops = gsk_contour_get_n_ops (contour);
  if ((n_ops > 1 && point->idx >= n_ops) ||
      (n_ops == 1 && point->idx > n_ops))
    return FALSE;

  if (point->t < 0 || point->t > 1)
    return FALSE;

  return TRUE;
}

G_END_DECLS

