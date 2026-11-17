/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2021 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include "cc-background-enum-types.h"
#include "cc-background-paintable.h"

struct _CcBackgroundPaintable
{
  GObject           parent_instance;

  GnomeDesktopThumbnailFactory *thumbnail_factory;
  CcBackgroundItem *item;
  int               width;
  int               height;
  int               scale_factor;
  GtkTextDirection  text_direction;

  GdkPaintable     *texture;
  GdkPaintable     *dark_texture;

  CcBackgroundPaintFlags  paint_flags;
};

enum
{
  PROP_0,
  PROP_THUMBNAIL_FACTORY,
  PROP_ITEM,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SCALE_FACTOR,
  PROP_TEXT_DIRECTION,
  PROP_PAINT_FLAGS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void cc_background_paintable_paintable_init (GdkPaintableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CcBackgroundPaintable, cc_background_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                cc_background_paintable_paintable_init))

static void
update_cache (CcBackgroundPaintable *self)
{
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  g_autoptr(GdkPixbuf) dark_pixbuf = NULL;
  gboolean has_dark;

  g_clear_object (&self->texture);
  g_clear_object (&self->dark_texture);

  has_dark = cc_background_item_has_dark_version (self->item);

  if ((self->paint_flags & CC_BACKGROUND_PAINT_LIGHT) || !has_dark)
    {
      pixbuf = cc_background_item_get_thumbnail (self->item,
                                                 self->thumbnail_factory,
                                                 self->width,
                                                 self->height,
                                                 self->scale_factor,
                                                 FALSE);
      self->texture = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));
    }

  if ((self->paint_flags & CC_BACKGROUND_PAINT_DARK) && has_dark)
    {
      dark_pixbuf = cc_background_item_get_thumbnail (self->item,
                                                      self->thumbnail_factory,
                                                      self->width,
                                                      self->height,
                                                      self->scale_factor,
                                                      TRUE);
      self->dark_texture = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (dark_pixbuf));
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

static void
cc_background_paintable_dispose (GObject *object)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  g_clear_object (&self->item);
  g_clear_object (&self->thumbnail_factory);
  g_clear_object (&self->texture);
  g_clear_object (&self->dark_texture);

  G_OBJECT_CLASS (cc_background_paintable_parent_class)->dispose (object);
}

static void
cc_background_paintable_constructed (GObject *object)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  G_OBJECT_CLASS (cc_background_paintable_parent_class)->constructed (object);

  update_cache (self);
}

static void
cc_background_paintable_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_THUMBNAIL_FACTORY:
      g_value_set_object (value, self->thumbnail_factory);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    case PROP_SCALE_FACTOR:
      g_value_set_int (value, self->scale_factor);
      break;

    case PROP_TEXT_DIRECTION:
      g_value_set_enum (value, self->text_direction);
      break;

    case PROP_PAINT_FLAGS:
      g_value_set_flags (value, self->paint_flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_paintable_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);
  int scale_factor;

  switch (prop_id)
    {
    case PROP_THUMBNAIL_FACTORY:
      g_set_object (&self->thumbnail_factory, g_value_get_object (value));
      break;

    case PROP_ITEM:
      g_set_object (&self->item, g_value_get_object (value));
      break;

    case PROP_WIDTH:
      self->width = g_value_get_int (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_int (value);
      break;

    case PROP_SCALE_FACTOR:
      scale_factor = g_value_get_int (value);
      if (self->scale_factor != scale_factor)
        {
          self->scale_factor = scale_factor;
          /* Update cache, but only if it's already constructed */
          if (self->texture || self->dark_texture)
            update_cache (self);
        }
      break;

    case PROP_TEXT_DIRECTION:
      self->text_direction = g_value_get_enum (value);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
      break;

    case PROP_PAINT_FLAGS:
      self->paint_flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_paintable_class_init (CcBackgroundPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_background_paintable_dispose;
  object_class->constructed = cc_background_paintable_constructed;
  object_class->get_property = cc_background_paintable_get_property;
  object_class->set_property = cc_background_paintable_set_property;

  properties[PROP_THUMBNAIL_FACTORY] =
    g_param_spec_object ("thumbnail-factory",
                         "Thumbnail factory",
                         "Thumbnail factory",
                         GNOME_DESKTOP_TYPE_THUMBNAIL_FACTORY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "Item",
                         CC_TYPE_BACKGROUND_ITEM,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDTH] =
    g_param_spec_int ("width",
                      "Width",
                      "Width",
                      1, G_MAXINT, 144,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  properties[PROP_HEIGHT] =
    g_param_spec_int ("height",
                      "Height",
                      "Height",
                      1, G_MAXINT, 144 * 3 / 4,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  properties[PROP_SCALE_FACTOR] =
    g_param_spec_int ("scale-factor",
                      "Scale Factor",
                      "Scale Factor",
                      1, G_MAXINT, 1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT |
                      G_PARAM_STATIC_STRINGS);

  properties[PROP_TEXT_DIRECTION] =
    g_param_spec_enum ("text-direction",
                       "Text Direction",
                       "Text Direction",
                       GTK_TYPE_TEXT_DIRECTION,
                       GTK_TEXT_DIR_LTR,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT |
                       G_PARAM_STATIC_STRINGS);

  properties[PROP_PAINT_FLAGS] =
    g_param_spec_flags ("paint-flags",
                        "Paint Flags",
                        "Paint Flags",
                        CC_TYPE_BACKGROUND_PAINT_FLAGS,
                        CC_BACKGROUND_PAINT_LIGHT,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_background_paintable_init (CcBackgroundPaintable *self)
{
}

static void
cc_background_paintable_snapshot (GdkPaintable *paintable,
                                  GdkSnapshot  *snapshot,
                                  double        width,
                                  double        height)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);
  gboolean is_rtl;

  if (!self->dark_texture)
    {
      gdk_paintable_snapshot (self->texture, snapshot, width, height);
      return;
    }

  if (!self->texture)
    {
      gdk_paintable_snapshot (self->dark_texture, snapshot, width, height);
      return;
    }

  is_rtl = self->text_direction == GTK_TEXT_DIR_RTL;

  gtk_snapshot_push_clip (GTK_SNAPSHOT (snapshot),
                          &GRAPHENE_RECT_INIT (is_rtl ? width / 2.0f : 0.0f,
                                               0.0f,
                                               width / 2.0f,
                                               height));
  gdk_paintable_snapshot (self->texture, snapshot, width, height);
  gtk_snapshot_pop (GTK_SNAPSHOT (snapshot));

  gtk_snapshot_push_clip (GTK_SNAPSHOT (snapshot),
                          &GRAPHENE_RECT_INIT (is_rtl ? 0.0f : width / 2.0f,
                                               0.0f,
                                               width / 2.0f,
                                               height));
  gdk_paintable_snapshot (self->dark_texture, snapshot, width, height);
  gtk_snapshot_pop (GTK_SNAPSHOT (snapshot));
}

static int
cc_background_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);
  GdkPaintable *valid_texture = self->texture ? self->texture : self->dark_texture;

  return gdk_paintable_get_intrinsic_width (valid_texture) / self->scale_factor;
}

static int
cc_background_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);
  GdkPaintable *valid_texture = self->texture ? self->texture : self->dark_texture;

  return gdk_paintable_get_intrinsic_height (valid_texture) / self->scale_factor;
}

static double
cc_background_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);
  GdkPaintable *valid_texture = self->texture ? self->texture : self->dark_texture;

  return gdk_paintable_get_intrinsic_aspect_ratio (valid_texture);
}

static void
cc_background_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = cc_background_paintable_snapshot;
  iface->get_intrinsic_width = cc_background_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = cc_background_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = cc_background_paintable_get_intrinsic_aspect_ratio;
}

/* Workaround for a typo in libgnome-desktop, see gnome-desktop!160 */
#define G_TYPE_INSTANCE_CHECK_TYPE G_TYPE_CHECK_INSTANCE_TYPE

CcBackgroundPaintable *
cc_background_paintable_new (GnomeDesktopThumbnailFactory *thumbnail_factory,
                             CcBackgroundItem             *item,
                             CcBackgroundPaintFlags        paint_flags,
                             int                           width,
                             int                           height,
                             int                           scale_factor)
{
  g_return_val_if_fail (GNOME_DESKTOP_IS_THUMBNAIL_FACTORY (thumbnail_factory), NULL);
  g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

  return g_object_new (CC_TYPE_BACKGROUND_PAINTABLE,
                       "thumbnail-factory", thumbnail_factory,
                       "item", item,
                       "paint-flags", paint_flags,
                       "width", width,
                       "height", height,
                       "scale-factor", scale_factor,
                       NULL);
}
