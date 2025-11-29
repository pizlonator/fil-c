/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2023 Red Hat, Inc.
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

#include "gdksubsurfaceprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktexture.h"
#include "gsk/gskrectprivate.h"

G_DEFINE_TYPE (GdkSubsurface, gdk_subsurface, G_TYPE_OBJECT)

static void
gdk_subsurface_init (GdkSubsurface *self)
{
}

static void remove_subsurface (GdkSubsurface *subsurface);

static void
gdk_subsurface_finalize (GObject *object)
{
  GdkSubsurface *subsurface = GDK_SUBSURFACE (object);

  remove_subsurface (subsurface);
  g_ptr_array_remove (subsurface->parent->subsurfaces, subsurface);
  g_clear_object (&subsurface->parent);

  G_OBJECT_CLASS (gdk_subsurface_parent_class)->finalize (object);
}

static void
gdk_subsurface_class_init (GdkSubsurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_subsurface_finalize;
}

GdkSurface *
gdk_subsurface_get_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return subsurface->parent;
}

static void
remove_subsurface (GdkSubsurface *subsurface)
{
  GdkSurface *parent = subsurface->parent;

  if (parent->subsurfaces_above == subsurface)
    parent->subsurfaces_above = subsurface->sibling_above;
  if (parent->subsurfaces_below == subsurface)
    parent->subsurfaces_below = subsurface->sibling_below;

  if (subsurface->sibling_above)
    subsurface->sibling_above->sibling_below = subsurface->sibling_below;
  if (subsurface->sibling_below)
    subsurface->sibling_below->sibling_above = subsurface->sibling_above;

  subsurface->sibling_above = NULL;
  subsurface->sibling_below = NULL;
}

static void
insert_subsurface (GdkSubsurface *subsurface,
                   gboolean       above,
                   GdkSubsurface *sibling)
{
  GdkSurface *parent = subsurface->parent;

  subsurface->above_parent = sibling->above_parent;

  if (above)
    {
      subsurface->sibling_above = sibling->sibling_above;
      sibling->sibling_above = subsurface;
      subsurface->sibling_below = sibling;
      if (subsurface->sibling_above)
        subsurface->sibling_above->sibling_below = subsurface;

      if (parent->subsurfaces_below == sibling)
        parent->subsurfaces_below = subsurface;
    }
  else
    {
      subsurface->sibling_below = sibling->sibling_below;
      sibling->sibling_below = subsurface;
      subsurface->sibling_above = sibling;
      if (subsurface->sibling_below)
        subsurface->sibling_below->sibling_above = subsurface;

      if (parent->subsurfaces_above == sibling)
        parent->subsurfaces_above = subsurface;
    }
}

gboolean
gdk_subsurface_attach (GdkSubsurface         *subsurface,
                       GdkTexture            *texture,
                       const graphene_rect_t *source,
                       const graphene_rect_t *dest,
                       gboolean               above,
                       GdkSubsurface         *sibling)
{
  GdkSurface *parent = subsurface->parent;

  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (source != NULL &&
                        gsk_rect_contains_rect (&GRAPHENE_RECT_INIT (0, 0,
                                                                     gdk_texture_get_width (texture),
                                                                     gdk_texture_get_height (texture)),
                                                source), FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (sibling != subsurface, FALSE);
  g_return_val_if_fail (sibling == NULL || GDK_IS_SUBSURFACE (sibling), FALSE);
  g_return_val_if_fail (sibling == NULL || sibling->parent == subsurface->parent, FALSE);

  remove_subsurface (subsurface);

  if (sibling)
    {
      insert_subsurface (subsurface, above, sibling);
    }
  else
    {
      sibling = above ? parent->subsurfaces_above : parent->subsurfaces_below;

      if (sibling)
        {
          insert_subsurface (subsurface, !above, sibling);
        }
      else
        {
          subsurface->above_parent = above;

          if (above)
            parent->subsurfaces_above = subsurface;
          else
            parent->subsurfaces_below = subsurface;
        }
    }

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->attach (subsurface, texture, source, dest, above, sibling);
}

void
gdk_subsurface_detach (GdkSubsurface *subsurface)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));

  remove_subsurface (subsurface);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->detach (subsurface);
}

GdkTexture *
gdk_subsurface_get_texture (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_texture (subsurface);
}

void
gdk_subsurface_get_source (GdkSubsurface   *subsurface,
                           graphene_rect_t *source)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (source != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_source (subsurface, source);
}

void
gdk_subsurface_get_dest (GdkSubsurface   *subsurface,
                         graphene_rect_t *dest)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (dest != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_dest (subsurface, dest);
}

gboolean
gdk_subsurface_is_above_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), TRUE);

  return subsurface->above_parent;
}
