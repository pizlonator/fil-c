/* GTK - The GIMP Toolkit
 * Copyright (C) 2014,2015 Benjamin Otte
 * 
 * Authors: Benjamin Otte <otte@gnome.org>
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

#include "gtkrendericonprivate.h"

#include "gtkcssfiltervalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkiconthemeprivate.h"
#include "gtksnapshot.h"
#include "gtksymbolicpaintable.h"
#include "gsktransform.h"

#include <math.h>

void
gtk_css_style_snapshot_icon (GtkCssStyle            *style,
                             GtkSnapshot            *snapshot,
                             double                  width,
                             double                  height)
{
  GskTransform *transform;
  GtkCssImage *image;
  gboolean has_shadow;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (snapshot != NULL);

  if (width == 0.0 || height == 0.0)
    return;

  image = _gtk_css_image_value_get_image (style->other->icon_source);
  if (image == NULL)
    return;

  transform = gtk_css_transform_value_get_transform (style->other->icon_transform);

  gtk_snapshot_push_debug (snapshot, "CSS Icon @ %gx%g", width, height);

  gtk_css_filter_value_push_snapshot (style->other->icon_filter, snapshot);

  has_shadow = gtk_css_shadow_value_push_snapshot (style->icon->icon_shadow, snapshot);

  if (transform == NULL)
    {
      gtk_css_image_snapshot (image, snapshot, width, height);
    }
  else
    {
      gtk_snapshot_save (snapshot);

      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2.0, height / 2.0));
      gtk_snapshot_transform (snapshot, transform);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- width / 2.0, - height / 2.0));

      gtk_css_image_snapshot (image, snapshot, width, height);

      gtk_snapshot_restore (snapshot);
    }

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (style->other->icon_filter, snapshot);

  gtk_snapshot_pop (snapshot);

  gsk_transform_unref (transform);
}

void
gtk_css_style_snapshot_icon_paintable (GtkCssStyle  *style,
                                       GtkSnapshot  *snapshot,
                                       GdkPaintable *paintable,
                                       double        width,
                                       double        height)
{
  GskTransform *transform;
  gboolean has_shadow;
  gboolean is_symbolic_paintable;
  GdkRGBA colors[4];

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  transform = gtk_css_transform_value_get_transform (style->other->icon_transform);

  gtk_css_filter_value_push_snapshot (style->other->icon_filter, snapshot);

  has_shadow = gtk_css_shadow_value_push_snapshot (style->icon->icon_shadow, snapshot);

  is_symbolic_paintable = GTK_IS_SYMBOLIC_PAINTABLE (paintable);
  if (is_symbolic_paintable)
    {
      gtk_icon_theme_lookup_symbolic_colors (style, colors);

      if (gdk_rgba_is_clear (&colors[0]))
        goto transparent;
    }

  if (transform == NULL)
    {
      if (is_symbolic_paintable)
        gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, colors, G_N_ELEMENTS (colors));
      else
        gdk_paintable_snapshot (paintable, snapshot, width, height);
    }
  else
    {
      gtk_snapshot_save (snapshot);

      /* XXX: Implement -gtk-icon-transform-origin instead of hardcoding "50% 50%" here */
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2.0, height / 2.0));
      gtk_snapshot_transform (snapshot, transform);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- width / 2.0, - height / 2.0));

      if (is_symbolic_paintable)
        gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, colors, G_N_ELEMENTS (colors));
      else
        gdk_paintable_snapshot (paintable, snapshot, width, height);

      gtk_snapshot_restore (snapshot);
    }

transparent:
  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (style->other->icon_filter, snapshot);

  gsk_transform_unref (transform);
}
