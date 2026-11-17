/* cc-background-preview.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "cc-background-preview.h"
#include "cc-background-paintable.h"

#define THUMBNAIL_WIDTH 256 /* No use asking for more, gnome_bg caps at 256 */
#define THUMBNAIL_HEIGHT (THUMBNAIL_WIDTH * 3 / 4)

struct _CcBackgroundPreview
{
  GtkWidget         parent;

  GtkWidget        *picture;
  GtkWidget        *light_dark_window;
  GtkWidget        *dark_window;

  GnomeDesktopThumbnailFactory *thumbnail_factory;

  gboolean          is_dark;
  CcBackgroundItem *item;
};

G_DEFINE_TYPE (CcBackgroundPreview, cc_background_preview, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_IS_DARK,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/* GObject overrides */

static void
cc_background_preview_dispose (GObject *object)
{
  CcBackgroundPreview *self = (CcBackgroundPreview *)object;

  g_clear_pointer (&self->picture, gtk_widget_unparent);
  g_clear_pointer (&self->light_dark_window, gtk_widget_unparent);
  g_clear_pointer (&self->dark_window, gtk_widget_unparent);

  G_OBJECT_CLASS (cc_background_preview_parent_class)->dispose (object);
}

static void
cc_background_preview_finalize (GObject *object)
{
  CcBackgroundPreview *self = (CcBackgroundPreview *)object;

  g_clear_object (&self->item);
  g_clear_object (&self->thumbnail_factory);

  G_OBJECT_CLASS (cc_background_preview_parent_class)->finalize (object);
}

static void
set_is_dark (CcBackgroundPreview *self,
             gboolean             is_dark)
{
  self->is_dark = is_dark;

  if (self->is_dark)
    {
      gtk_widget_add_css_class (self->light_dark_window, "dark");
      gtk_widget_remove_css_class (self->light_dark_window, "light");
    }
  else
    {
      gtk_widget_add_css_class (self->light_dark_window, "light");
      gtk_widget_remove_css_class (self->light_dark_window, "dark");
    }
}

static void
cc_background_preview_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IS_DARK:
      g_value_set_boolean (value, self->is_dark);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_preview_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IS_DARK:
      set_is_dark (self, g_value_get_boolean (value));
      break;

    case PROP_ITEM:
      cc_background_preview_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
cc_background_preview_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
cc_background_preview_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               gint            for_size,
                               gint           *minimum,
                               gint           *natural,
                               gint           *minimum_baseline,
                               gint           *natural_baseline)
{
  GtkWidget *child;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *natural = THUMBNAIL_WIDTH;
      *minimum = 0;
    }
  else
    {
      *natural = MAX (0, for_size * 3 / 4); /* 4:3 aspect ratio */
      *minimum = *natural;
    }

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min, child_nat;

      gtk_widget_measure (child, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
cc_background_preview_size_allocate (GtkWidget *widget,
                                     gint       width,
                                     gint       height,
                                     gint       baseline)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (widget);
  int window_width, window_height, margin_x, margin_y;
  int opposite_margin_x, opposite_margin_y;
  GskTransform *front_transform, *back_transform;
  gboolean is_rtl;

  window_width = ceil (width * 0.5);
  window_height = ceil (height * 0.5);
  margin_x = floor (width * 0.15);
  margin_y = floor (height * 0.15);
  opposite_margin_x = width - window_width - margin_x;
  opposite_margin_y = height - window_height - margin_y;
  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  front_transform =
    gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (is_rtl ? opposite_margin_x : margin_x,
                                                         opposite_margin_y));
  back_transform =
    gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (is_rtl ? margin_x : opposite_margin_x,
                                                         margin_y));

  gtk_widget_allocate (self->picture, width, height, baseline, NULL);
  gtk_widget_allocate (self->dark_window, window_width, window_height,
                       baseline, back_transform);
  gtk_widget_allocate (self->light_dark_window, window_width, window_height,
                       baseline, front_transform);
}

static void
cc_background_preview_class_init (CcBackgroundPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_background_preview_dispose;
  object_class->finalize = cc_background_preview_finalize;
  object_class->get_property = cc_background_preview_get_property;
  object_class->set_property = cc_background_preview_set_property;

  widget_class->get_request_mode = cc_background_preview_get_request_mode;
  widget_class->measure = cc_background_preview_measure;
  widget_class->size_allocate = cc_background_preview_size_allocate;

  properties[PROP_IS_DARK] = g_param_spec_boolean ("is-dark",
                                                   "Is dark",
                                                   "Whether the preview is dark",
                                                   FALSE,
                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ITEM] = g_param_spec_object ("item",
                                               "Item",
                                               "Background item",
                                               CC_TYPE_BACKGROUND_ITEM,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-preview.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, picture);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, light_dark_window);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, dark_window);

  gtk_widget_class_set_css_name (widget_class, "background-preview");
}

static void
cc_background_preview_init (CcBackgroundPreview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
}

CcBackgroundItem*
cc_background_preview_get_item (CcBackgroundPreview *self)
{
  g_return_val_if_fail (CC_IS_BACKGROUND_PREVIEW (self), NULL);

  return self->item;
}

void
cc_background_preview_set_item (CcBackgroundPreview *self,
                                CcBackgroundItem    *item)
{
  g_autoptr(CcBackgroundPaintable) paintable;
  CcBackgroundPaintFlags paint_flags;

  g_return_if_fail (CC_IS_BACKGROUND_PREVIEW (self));
  g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

  if (!g_set_object (&self->item, item))
    return;

  paint_flags = self->is_dark ? CC_BACKGROUND_PAINT_DARK : CC_BACKGROUND_PAINT_LIGHT;

  paintable = cc_background_paintable_new (self->thumbnail_factory,
                                           item,
                                           paint_flags,
                                           THUMBNAIL_WIDTH,
                                           THUMBNAIL_HEIGHT,
                                           gtk_widget_get_scale_factor (GTK_WIDGET (self)));

  g_object_bind_property (self->picture, "scale-factor",
                          paintable, "scale-factor", G_BINDING_SYNC_CREATE);

  gtk_picture_set_paintable (GTK_PICTURE (self->picture), GDK_PAINTABLE (paintable));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}
