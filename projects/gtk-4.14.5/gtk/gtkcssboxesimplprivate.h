/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Benjamin Otte <otte@gnome.org>
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

#pragma once

#include "gtkcssboxesprivate.h"

#include "gtkcsscornervalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"
#include "gtkwidgetprivate.h"

/* This file is included from gtkcssboxesprivate.h */

static inline void
gtk_css_boxes_init (GtkCssBoxes *boxes,
                    GtkWidget   *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  gtk_css_boxes_init_content_box (boxes,
                                  gtk_css_node_get_style (priv->cssnode),
                                  0, 0,
                                  priv->width,
                                  priv->height);
}

static inline void
gtk_css_boxes_init_content_box (GtkCssBoxes *boxes,
                                GtkCssStyle *style,
                                double       x,
                                double       y,
                                double       width,
                                double       height)
{
  memset (boxes, 0, sizeof (GtkCssBoxes));

  boxes->style = style;
  boxes->box[GTK_CSS_AREA_CONTENT_BOX].bounds = GRAPHENE_RECT_INIT (x, y, width, height);
  boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_init_border_box (GtkCssBoxes *boxes,
                               GtkCssStyle *style,
                               double       x,
                               double       y,
                               double       width,
                               double       height)
{
  memset (boxes, 0, sizeof (GtkCssBoxes));

  boxes->style = style;
  boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds = GRAPHENE_RECT_INIT (x, y, width, height);
  boxes->has_rect[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_rect_grow (GskRoundedRect *dest,
                         GskRoundedRect *src,
                         GtkCssValue    *top,
                         GtkCssValue    *right,
                         GtkCssValue    *bottom,
                         GtkCssValue    *left)
{
  if (gtk_css_dimension_value_is_zero (left))
    {
      dest->bounds.origin.x = src->bounds.origin.x;
      if (gtk_css_dimension_value_is_zero (right))
        dest->bounds.size.width = src->bounds.size.width;
      else
        dest->bounds.size.width = src->bounds.size.width + _gtk_css_number_value_get (right, 100);
    }
  else
    {
      const double left_value = _gtk_css_number_value_get (left, 100);

      dest->bounds.origin.x = src->bounds.origin.x - left_value;
      if (gtk_css_dimension_value_is_zero (right))
        dest->bounds.size.width = src->bounds.size.width + left_value;
      else
        dest->bounds.size.width = src->bounds.size.width + left_value + _gtk_css_number_value_get (right, 100);

    }


  if (gtk_css_dimension_value_is_zero (top))
    {
      dest->bounds.origin.y = src->bounds.origin.y;
      if (gtk_css_dimension_value_is_zero (bottom))
        dest->bounds.size.height = src->bounds.size.height;
      else
        dest->bounds.size.height = src->bounds.size.height + _gtk_css_number_value_get (bottom, 100);
    }
  else
    {
      const double top_value = _gtk_css_number_value_get (top, 100);

      dest->bounds.origin.y = src->bounds.origin.y - top_value;
      if (gtk_css_dimension_value_is_zero (bottom))
        dest->bounds.size.height = src->bounds.size.height + top_value;
      else
        dest->bounds.size.height = src->bounds.size.height + top_value + _gtk_css_number_value_get (bottom, 100);
    }
}

static inline void
gtk_css_boxes_rect_shrink (GskRoundedRect *dest,
                           GskRoundedRect *src,
                           GtkCssValue    *top_value,
                           GtkCssValue    *right_value,
                           GtkCssValue    *bottom_value,
                           GtkCssValue    *left_value)
{
  double top = _gtk_css_number_value_get (top_value, 100);
  double right = _gtk_css_number_value_get (right_value, 100);
  double bottom = _gtk_css_number_value_get (bottom_value, 100);
  double left = _gtk_css_number_value_get (left_value, 100);

  /* FIXME: Do we need underflow checks here? */
  dest->bounds.origin.x = src->bounds.origin.x + left;
  dest->bounds.origin.y = src->bounds.origin.y + top;
  dest->bounds.size.width = src->bounds.size.width - left - right;
  dest->bounds.size.height = src->bounds.size.height - top - bottom;
}

static inline void gtk_css_boxes_compute_padding_rect (GtkCssBoxes *boxes);

static inline const graphene_rect_t *
gtk_css_boxes_get_rect (GtkCssBoxes *boxes,
                        GtkCssArea   area)
{
  switch (area)
    {
      case GTK_CSS_AREA_BORDER_BOX:
        return gtk_css_boxes_get_border_rect (boxes);
      case GTK_CSS_AREA_PADDING_BOX:
        return gtk_css_boxes_get_padding_rect (boxes);
      case GTK_CSS_AREA_CONTENT_BOX:
        return gtk_css_boxes_get_content_rect (boxes);
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static inline void
gtk_css_boxes_compute_border_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_BORDER_BOX])
    return;

  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_BORDER_BOX],
                           &boxes->box[GTK_CSS_AREA_PADDING_BOX],
                           boxes->style->border->border_top_width,
                           boxes->style->border->border_right_width,
                           boxes->style->border->border_bottom_width,
                           boxes->style->border->border_left_width);

  boxes->has_rect[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_padding_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_PADDING_BOX])
    return;

  if (boxes->has_rect[GTK_CSS_AREA_BORDER_BOX])
    {
      gtk_css_boxes_rect_shrink (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                                 &boxes->box[GTK_CSS_AREA_BORDER_BOX],
                                 boxes->style->border->border_top_width,
                                 boxes->style->border->border_right_width,
                                 boxes->style->border->border_bottom_width,
                                 boxes->style->border->border_left_width);
    }
  else
    {
      gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                               &boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                               boxes->style->size->padding_top,
                               boxes->style->size->padding_right,
                               boxes->style->size->padding_bottom,
                               boxes->style->size->padding_left);
    }

  boxes->has_rect[GTK_CSS_AREA_PADDING_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_content_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX])
    return;

  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_rect_shrink (&boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                             &boxes->box[GTK_CSS_AREA_PADDING_BOX],
                             boxes->style->size->padding_top,
                             boxes->style->size->padding_right,
                             boxes->style->size->padding_bottom,
                             boxes->style->size->padding_left);

  boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_margin_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_MARGIN_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_MARGIN_BOX],
                           &boxes->box[GTK_CSS_AREA_BORDER_BOX],
                           boxes->style->size->margin_top,
                           boxes->style->size->margin_right,
                           boxes->style->size->margin_bottom,
                           boxes->style->size->margin_left);

  boxes->has_rect[GTK_CSS_AREA_MARGIN_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_outline_rect (GtkCssBoxes *boxes)
{
  graphene_rect_t *dest, *src;
  double d;

  if (boxes->has_rect[GTK_CSS_AREA_OUTLINE_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  dest = &boxes->box[GTK_CSS_AREA_OUTLINE_BOX].bounds;
  src = &boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds;

  d = _gtk_css_number_value_get (boxes->style->outline->outline_offset, 100) +
      _gtk_css_number_value_get (boxes->style->outline->outline_width, 100);

  dest->origin.x = src->origin.x - d;
  dest->origin.y = src->origin.y - d;
  dest->size.width = src->size.width + d + d;
  dest->size.height = src->size.height + d + d;

  boxes->has_rect[GTK_CSS_AREA_OUTLINE_BOX] = TRUE;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_margin_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_margin_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_MARGIN_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_border_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_border_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_padding_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_padding_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_PADDING_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_content_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_content_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_CONTENT_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_outline_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_outline_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_OUTLINE_BOX].bounds;
}

/* clamp border radius, following CSS specs */
static inline void
gtk_css_boxes_clamp_border_radius (GskRoundedRect *box)
{
  double factor = 1.0;
  double corners;

  corners = box->corner[GSK_CORNER_TOP_LEFT].width + box->corner[GSK_CORNER_TOP_RIGHT].width;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.width / corners);

  corners = box->corner[GSK_CORNER_TOP_RIGHT].height + box->corner[GSK_CORNER_BOTTOM_RIGHT].height;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.height / corners);

  corners = box->corner[GSK_CORNER_BOTTOM_RIGHT].width + box->corner[GSK_CORNER_BOTTOM_LEFT].width;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.width / corners);

  corners = box->corner[GSK_CORNER_TOP_LEFT].height + box->corner[GSK_CORNER_BOTTOM_LEFT].height;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.height / corners);

  box->corner[GSK_CORNER_TOP_LEFT].width *= factor;
  box->corner[GSK_CORNER_TOP_LEFT].height *= factor;
  box->corner[GSK_CORNER_TOP_RIGHT].width *= factor;
  box->corner[GSK_CORNER_TOP_RIGHT].height *= factor;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].width *= factor;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].height *= factor;
  box->corner[GSK_CORNER_BOTTOM_LEFT].width *= factor;
  box->corner[GSK_CORNER_BOTTOM_LEFT].height *= factor;
}

static inline void
gtk_css_boxes_apply_border_radius (GskRoundedRect    *box,
                                   const GtkCssValue *top_left,
                                   const GtkCssValue *top_right,
                                   const GtkCssValue *bottom_right,
                                   const GtkCssValue *bottom_left)
{
  gboolean has_border_radius = FALSE;

  if (!gtk_css_corner_value_is_zero (top_left))
    {
      box->corner[GSK_CORNER_TOP_LEFT].width = _gtk_css_corner_value_get_x (top_left, box->bounds.size.width);
      box->corner[GSK_CORNER_TOP_LEFT].height = _gtk_css_corner_value_get_y (top_left, box->bounds.size.height);
      has_border_radius = TRUE;
    }

  if (!gtk_css_corner_value_is_zero (top_right))
    {
      box->corner[GSK_CORNER_TOP_RIGHT].width = _gtk_css_corner_value_get_x (top_right, box->bounds.size.width);
      box->corner[GSK_CORNER_TOP_RIGHT].height = _gtk_css_corner_value_get_y (top_right, box->bounds.size.height);
      has_border_radius = TRUE;
    }

  if (!gtk_css_corner_value_is_zero (bottom_right))
    {
      box->corner[GSK_CORNER_BOTTOM_RIGHT].width = _gtk_css_corner_value_get_x (bottom_right, box->bounds.size.width);
      box->corner[GSK_CORNER_BOTTOM_RIGHT].height = _gtk_css_corner_value_get_y (bottom_right, box->bounds.size.height);
      has_border_radius = TRUE;
    }

  if (!gtk_css_corner_value_is_zero (bottom_left))
    {
      box->corner[GSK_CORNER_BOTTOM_LEFT].width = _gtk_css_corner_value_get_x (bottom_left, box->bounds.size.width);
      box->corner[GSK_CORNER_BOTTOM_LEFT].height = _gtk_css_corner_value_get_y (bottom_left, box->bounds.size.height);
      has_border_radius = TRUE;
    }

  if (has_border_radius)
    gtk_css_boxes_clamp_border_radius (box);
}

/* NB: width and height must be >= 0 */
static inline void
gtk_css_boxes_shrink_border_radius (graphene_size_t       *dest,
                                    const graphene_size_t *src,
                                    double                 width,
                                    double                 height)
{
  dest->width = src->width - width;
  dest->height = src->height - height;

  if (dest->width <= 0 || dest->height <= 0)
    {
      dest->width = 0;
      dest->height = 0;
    }
}

static inline void
gtk_css_boxes_shrink_corners (GskRoundedRect       *dest,
                              const GskRoundedRect *src)
{
  double top = dest->bounds.origin.y - src->bounds.origin.y;
  double right = src->bounds.origin.x + src->bounds.size.width - dest->bounds.origin.x - dest->bounds.size.width;
  double bottom = src->bounds.origin.y + src->bounds.size.height - dest->bounds.origin.y - dest->bounds.size.height;
  double left = dest->bounds.origin.x - src->bounds.origin.x;

  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_TOP_LEFT],
                                      &src->corner[GSK_CORNER_TOP_LEFT],
                                      top, left);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_TOP_RIGHT],
                                      &src->corner[GSK_CORNER_TOP_RIGHT],
                                      top, right);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_BOTTOM_RIGHT],
                                      &src->corner[GSK_CORNER_BOTTOM_RIGHT],
                                      bottom, right);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_BOTTOM_LEFT],
                                      &src->corner[GSK_CORNER_BOTTOM_LEFT],
                                      bottom, left);
}

static inline void
gtk_css_boxes_compute_border_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_BORDER_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  gtk_css_boxes_apply_border_radius (&boxes->box[GTK_CSS_AREA_BORDER_BOX],
                                     boxes->style->border->border_top_left_radius,
                                     boxes->style->border->border_top_right_radius,
                                     boxes->style->border->border_bottom_right_radius,
                                     boxes->style->border->border_bottom_left_radius);

  boxes->has_box[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_padding_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_PADDING_BOX])
    return;

  gtk_css_boxes_compute_border_box (boxes);
  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_shrink_corners (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                                &boxes->box[GTK_CSS_AREA_BORDER_BOX]);

  boxes->has_box[GTK_CSS_AREA_PADDING_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_content_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_CONTENT_BOX])
    return;

  gtk_css_boxes_compute_padding_box (boxes);
  gtk_css_boxes_compute_content_rect (boxes);

  gtk_css_boxes_shrink_corners (&boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                                &boxes->box[GTK_CSS_AREA_PADDING_BOX]);

  boxes->has_box[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_outline_box (GtkCssBoxes *boxes)
{
  const GskRoundedRect *src;
  GskRoundedRect *dest;
  double d;
  int i;

  if (boxes->has_box[GTK_CSS_AREA_OUTLINE_BOX])
    return;

  gtk_css_boxes_compute_border_box (boxes);

  src = &boxes->box[GTK_CSS_AREA_BORDER_BOX];
  dest = &boxes->box[GTK_CSS_AREA_OUTLINE_BOX];

  d = _gtk_css_number_value_get (boxes->style->outline->outline_offset, 100) +
      _gtk_css_number_value_get (boxes->style->outline->outline_width, 100);

  /* Grow border rect into outline rect */
  dest->bounds.origin.x = src->bounds.origin.x - d;
  dest->bounds.origin.y = src->bounds.origin.y - d;
  dest->bounds.size.width = src->bounds.size.width + d + d;
  dest->bounds.size.height = src->bounds.size.height + d + d;

  /* Grow corner radii of border rect */
  for (i = 0; i < 4; i ++)
    {
      if (src->corner[i].width > 0) dest->corner[i].width = src->corner[i].width + d;
      if (src->corner[i].height > 0) dest->corner[i].height = src->corner[i].height + d;

      if (dest->corner[i].width <= 0 || dest->corner[i].height <= 0)
        {
          dest->corner[i].width = 0;
          dest->corner[i].height = 0;
        }
      else
        {
          dest->corner[i].width = MIN (dest->corner[i].width, dest->bounds.size.width);
          dest->corner[i].height = MIN (dest->corner[i].height, dest->bounds.size.height);
        }
    }

  boxes->has_box[GTK_CSS_AREA_OUTLINE_BOX] = TRUE;
}

static inline const GskRoundedRect *
gtk_css_boxes_get_box (GtkCssBoxes *boxes,
                       GtkCssArea  area)
{
  switch (area)
    {
      case GTK_CSS_AREA_BORDER_BOX:
        return gtk_css_boxes_get_border_box (boxes);
      case GTK_CSS_AREA_PADDING_BOX:
        return gtk_css_boxes_get_padding_box (boxes);
      case GTK_CSS_AREA_CONTENT_BOX:
        return gtk_css_boxes_get_content_box (boxes);
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static inline const GskRoundedRect *
gtk_css_boxes_get_border_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_border_box (boxes);

  return &boxes->box[GTK_CSS_AREA_BORDER_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_padding_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_padding_box (boxes);

  return &boxes->box[GTK_CSS_AREA_PADDING_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_content_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_content_box (boxes);

  return &boxes->box[GTK_CSS_AREA_CONTENT_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_outline_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_outline_box (boxes);

  return &boxes->box[GTK_CSS_AREA_OUTLINE_BOX];
}

