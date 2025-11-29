/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkimagedefinitionprivate.h"

typedef struct _GtkImageDefinitionEmpty GtkImageDefinitionEmpty;
typedef struct _GtkImageDefinitionIconName GtkImageDefinitionIconName;
typedef struct _GtkImageDefinitionGIcon GtkImageDefinitionGIcon;
typedef struct _GtkImageDefinitionPaintable GtkImageDefinitionPaintable;

struct _GtkImageDefinitionEmpty {
  GtkImageType type;
  int ref_count;
};

struct _GtkImageDefinitionIconName {
  GtkImageType type;
  int ref_count;

  char *icon_name;
};

struct _GtkImageDefinitionGIcon {
  GtkImageType type;
  int ref_count;

  GIcon *gicon;
};

struct _GtkImageDefinitionPaintable {
  GtkImageType type;
  int ref_count;

  GdkPaintable *paintable;
};

union _GtkImageDefinition
{
  GtkImageType type;
  GtkImageDefinitionEmpty empty;
  GtkImageDefinitionIconName icon_name;
  GtkImageDefinitionGIcon gicon;
  GtkImageDefinitionPaintable paintable;
};

GtkImageDefinition *
gtk_image_definition_new_empty (void)
{
  static GtkImageDefinitionEmpty empty = { GTK_IMAGE_EMPTY, 1 };

  return gtk_image_definition_ref ((GtkImageDefinition *) &empty);
}

static inline GtkImageDefinition *
gtk_image_definition_alloc (GtkImageType type)
{
  static gsize sizes[] = {
    sizeof (GtkImageDefinitionEmpty),
    sizeof (GtkImageDefinitionIconName),
    sizeof (GtkImageDefinitionGIcon),
    sizeof (GtkImageDefinitionPaintable)
  };
  GtkImageDefinition *def;

  g_assert (type < G_N_ELEMENTS (sizes));

  def = g_malloc0 (sizes[type]);
  def->type = type;
  def->empty.ref_count = 1;

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_icon_name (const char *icon_name)
{
  GtkImageDefinition *def;

  if (icon_name == NULL || icon_name[0] == '\0')
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_ICON_NAME);
  def->icon_name.icon_name = g_strdup (icon_name);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_gicon (GIcon *gicon)
{
  GtkImageDefinition *def;

  if (gicon == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_GICON);
  def->gicon.gicon = g_object_ref (gicon);

  return def;
}

GtkImageDefinition *
gtk_image_definition_new_paintable (GdkPaintable *paintable)
{
  GtkImageDefinition *def;

  if (paintable == NULL)
    return NULL;

  def = gtk_image_definition_alloc (GTK_IMAGE_PAINTABLE);
  def->paintable.paintable = g_object_ref (paintable);

  return def;
}

GtkImageDefinition *
gtk_image_definition_ref (GtkImageDefinition *def)
{
  GtkImageDefinitionEmpty *empty = (GtkImageDefinitionEmpty *) def;

  empty->ref_count++;

  return def;
}

void
gtk_image_definition_unref (GtkImageDefinition *def)
{
  GtkImageDefinitionEmpty *empty = (GtkImageDefinitionEmpty *) def;

  empty->ref_count--;

  if (empty->ref_count > 0)
    return;

  switch (def->type)
    {
    default:
    case GTK_IMAGE_EMPTY:
      g_assert_not_reached ();
      break;
    case GTK_IMAGE_PAINTABLE:
      {
        GtkImageDefinitionPaintable *paintable = (GtkImageDefinitionPaintable *) def;
        g_object_unref (paintable->paintable);
      }
      break;
    case GTK_IMAGE_ICON_NAME:
      {
        GtkImageDefinitionIconName *icon_name = (GtkImageDefinitionIconName *) def;
        g_free (icon_name->icon_name);
      }
      break;
    case GTK_IMAGE_GICON:
      {
        GtkImageDefinitionGIcon *gicon = (GtkImageDefinitionGIcon *) def;
        g_object_unref (gicon->gicon);
      }
      break;
    }

  g_free (def);
}

GtkImageType
gtk_image_definition_get_storage_type (const GtkImageDefinition *def)
{
  return def->type;
}

int
gtk_image_definition_get_scale (const GtkImageDefinition *def)
{
  switch (def->type)
    {
    default:
      g_assert_not_reached ();
    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_PAINTABLE:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      return 1;
    }
}

const char *
gtk_image_definition_get_icon_name (const GtkImageDefinition *def)
{
  const GtkImageDefinitionIconName *icon_name = (const GtkImageDefinitionIconName *) def;

  if (def->type != GTK_IMAGE_ICON_NAME)
    return NULL;

  return icon_name->icon_name;
}

GIcon *
gtk_image_definition_get_gicon (const GtkImageDefinition *def)
{
  const GtkImageDefinitionGIcon *gicon = (const GtkImageDefinitionGIcon *) def;

  if (def->type != GTK_IMAGE_GICON)
    return NULL;

  return gicon->gicon;
}

GdkPaintable *
gtk_image_definition_get_paintable (const GtkImageDefinition *def)
{
  const GtkImageDefinitionPaintable *paintable = (const GtkImageDefinitionPaintable *) def;

  if (def->type != GTK_IMAGE_PAINTABLE)
    return NULL;

  return paintable->paintable;
}
