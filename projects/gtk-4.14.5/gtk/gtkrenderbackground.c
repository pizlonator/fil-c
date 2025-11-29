/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 * 
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#include "config.h"

#include "gtkrenderbackgroundprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
#include "gtkcssboxesprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsstypesprivate.h"

#include <math.h>

#include <gdk/gdk.h>

#include "gsk/gskroundedrectprivate.h"

static void
gtk_theming_background_snapshot_color (GtkCssBoxes       *boxes,
                                       GtkSnapshot       *snapshot,
                                       const GdkRGBA     *bg_color,
                                       guint              n_bg_values)
{
  const GskRoundedRect *box;
  GtkCssArea clip;

  clip = _gtk_css_area_value_get (_gtk_css_array_value_get_nth (boxes->style->background->background_clip, n_bg_values - 1));
  box = gtk_css_boxes_get_box (boxes, clip);

  if (gsk_rounded_rect_is_rectilinear (box))
    {
      gtk_snapshot_append_color (snapshot,
                                 bg_color,
                                 &box->bounds);
    }
  else
    {
      gtk_snapshot_push_rounded_clip (snapshot, box);
      gtk_snapshot_append_color (snapshot,
                                 bg_color,
                                 &box->bounds);
      gtk_snapshot_pop (snapshot);
    }
}

static void
gtk_theming_background_snapshot_layer (GtkCssBoxes *bg,
                                       guint        idx,
                                       GtkSnapshot *snapshot)
{
  GtkCssBackgroundValues *background = bg->style->background;
  GtkCssRepeatStyle hrepeat, vrepeat;
  const GtkCssValue *pos, *repeat;
  GtkCssImage *image;
  const GskRoundedRect *origin, *clip;
  double image_width, image_height;
  double width, height;
  double x, y;

  image = _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (background->background_image, idx));

  if (image == NULL)
    return;

  pos = _gtk_css_array_value_get_nth (background->background_position, idx);
  repeat = _gtk_css_array_value_get_nth (background->background_repeat, idx);

  origin = gtk_css_boxes_get_box (bg,
                                  _gtk_css_area_value_get (
                                      _gtk_css_array_value_get_nth (background->background_origin, idx)));

  width = origin->bounds.size.width;
  height = origin->bounds.size.height;

  if (width <= 0 || height <= 0)
    return;

  clip = gtk_css_boxes_get_box (bg,
                                _gtk_css_area_value_get (
                                    _gtk_css_array_value_get_nth (background->background_clip, idx)));

  _gtk_css_bg_size_value_compute_size (_gtk_css_array_value_get_nth (background->background_size, idx),
                                       image,
                                       width,
                                       height,
                                       &image_width,
                                       &image_height);

  if (image_width <= 0 || image_height <= 0)
    return;

  /* optimization */
  if (image_width == width)
    hrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;
  else
    hrepeat = _gtk_css_background_repeat_value_get_x (repeat);

  if (image_height == height)
    vrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;
  else
    vrepeat = _gtk_css_background_repeat_value_get_y (repeat);

  gtk_snapshot_push_debug (snapshot, "Layer %u", idx);
  gtk_snapshot_push_rounded_clip (snapshot, clip);

  x = _gtk_css_position_value_get_x (pos, width - image_width) + origin->bounds.origin.x;
  y = _gtk_css_position_value_get_y (pos, height - image_height) + origin->bounds.origin.y;
  if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT && vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
    {
      /* shortcut for normal case */
      if (x != 0 || y != 0)
        {
          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
          gtk_css_image_snapshot (image, snapshot, image_width, image_height);
          gtk_snapshot_restore (snapshot);
        }
      else
        {
          gtk_css_image_snapshot (image, snapshot, image_width, image_height);
        }
    }
  else
    {
      float repeat_width, repeat_height;
      graphene_rect_t fill_rect;

      /* If ‘background-repeat’ is ‘round’ for one (or both) dimensions,
       * there is a second step. The UA must scale the image in that
       * dimension (or both dimensions) so that it fits a whole number of
       * times in the background positioning area. In the case of the width
       * (height is analogous):
       *
       * If X ≠ 0 is the width of the image after step one and W is the width
       * of the background positioning area, then the rounded width
       * X' = W / round(W / X) where round() is a function that returns the
       * nearest natural number (integer greater than zero). 
       *
       * If ‘background-repeat’ is ‘round’ for one dimension only and if
       * ‘background-size’ is ‘auto’ for the other dimension, then there is
       * a third step: that other dimension is scaled so that the original
       * aspect ratio is restored. 
       */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (width / image_width);

          n = MAX (1, n);

          if (vrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && vsize == auto (it is by default) */)
            image_height *= width / (image_width * n);
          image_width = width / n;
        }
      if (vrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (height / image_height);

          n = MAX (1, n);

          if (hrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && hsize == auto (it is by default) */)
            image_width *= height / (image_height * n);
          image_height = height / n;
        }

      /* if hrepeat or vrepeat is 'space', we create a somewhat larger surface
       * to store the extra space. */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (width / image_width);
          repeat_width = n ? round (width / n) : 0;
        }
      else
        repeat_width = round (image_width);

      if (vrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (height / image_height);
          repeat_height = n ? round (height / n) : 0;
        }
      else
        repeat_height = round (image_height);

      fill_rect = clip->bounds;
      if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.origin.x = _gtk_css_position_value_get_x (pos, width - image_width);
          fill_rect.size.width = image_width;
        }
      if (vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.origin.y = _gtk_css_position_value_get_y (pos, height - image_height);
          fill_rect.size.height = image_height;
        }

      gtk_snapshot_push_repeat (snapshot,
                                &fill_rect,
                                &GRAPHENE_RECT_INIT (
                                    x, y,
                                    repeat_width, repeat_height
                                ));

      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                              x + 0.5 * (repeat_width - image_width),
                              y + 0.5 * (repeat_height - image_height)));
      gtk_css_image_snapshot (image, snapshot, image_width, image_height);

      gtk_snapshot_pop (snapshot);
    }

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

void
gtk_css_style_snapshot_background (GtkCssBoxes *boxes,
                                   GtkSnapshot *snapshot)
{
  const GtkCssBackgroundValues *background = boxes->style->background;
  GtkCssValue *background_image;
  const GdkRGBA *bg_color;
  const GtkCssValue *box_shadow;
  gboolean has_bg_color;
  gboolean has_bg_image;
  gboolean has_shadow;
  int idx;
  guint number_of_layers;

  if (background->base.type == GTK_CSS_BACKGROUND_INITIAL_VALUES)
    return;

  background_image = background->background_image;
  bg_color = gtk_css_color_value_get_rgba (background->background_color);
  box_shadow = background->box_shadow;

  has_bg_color = !gdk_rgba_is_clear (bg_color);
  has_bg_image = _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (background_image, 0)) != NULL;
  has_shadow = !gtk_css_shadow_value_is_none (box_shadow);

  /* This is the common default case of no background */
  if (!has_bg_color && !has_bg_image && !has_shadow)
    return;

  gtk_snapshot_push_debug (snapshot, "CSS background");

  if (has_shadow)
    gtk_css_shadow_value_snapshot_outset (box_shadow,
                                          snapshot,
                                          gtk_css_boxes_get_border_box (boxes));

  number_of_layers = _gtk_css_array_value_get_n_values (background_image);

  if (has_bg_image)
    {
      GtkCssValue *blend_modes = background->background_blend_mode;
      GskBlendMode *blend_mode_values = g_alloca (sizeof (GskBlendMode) * number_of_layers);

      for (idx = number_of_layers - 1; idx >= 0; idx--)
        {
          blend_mode_values[idx] = _gtk_css_blend_mode_value_get (_gtk_css_array_value_get_nth (blend_modes, idx));

          if (blend_mode_values[idx] != GSK_BLEND_MODE_DEFAULT)
            gtk_snapshot_push_blend (snapshot, blend_mode_values[idx]);
        }

      if (has_bg_color)
        gtk_theming_background_snapshot_color (boxes, snapshot, bg_color, number_of_layers);

      for (idx = number_of_layers - 1; idx >= 0; idx--)
        {
          if (blend_mode_values[idx] == GSK_BLEND_MODE_DEFAULT)
            {
              gtk_theming_background_snapshot_layer (boxes, idx, snapshot);
            }
          else
            {
              gtk_snapshot_pop (snapshot);
              gtk_theming_background_snapshot_layer (boxes, idx, snapshot);
              gtk_snapshot_pop (snapshot);
            }
        }
    }
  else if (has_bg_color)
    {
      gtk_theming_background_snapshot_color (boxes, snapshot, bg_color, number_of_layers);
    }

  if (has_shadow)
    gtk_css_shadow_value_snapshot_inset (box_shadow,
                                         snapshot,
                                         gtk_css_boxes_get_padding_box (boxes));

  gtk_snapshot_pop (snapshot);
}

