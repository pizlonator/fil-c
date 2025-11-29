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

/* Uninstalled header defining types and functions internal to GDK */

#pragma once

#include "gdkenumtypes.h"
#include "gdksurface.h"
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GdkSubsurface GdkSubsurface;
typedef struct _GdkSubsurfaceClass GdkSubsurfaceClass;

#define GDK_TYPE_SUBSURFACE              (gdk_subsurface_get_type ())
#define GDK_SUBSURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SUBSURFACE, GdkSubsurface))
#define GDK_SUBSURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SUBSURFACE, GdkSubsurfaceClass))
#define GDK_IS_SUBSURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SUBSURFACE))
#define GDK_SUBSURFACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SUBSURFACE, GdkSubsurfaceClass))

struct _GdkSubsurface
{
  GObject parent_instance;

  GdkSurface *parent;

  int ref_count;

  gboolean above_parent;
  GdkSubsurface *sibling_above;
  GdkSubsurface *sibling_below;
};


struct _GdkSubsurfaceClass
{
  GObjectClass parent_class;

  gboolean     (* attach)          (GdkSubsurface         *subsurface,
                                    GdkTexture            *texture,
                                    const graphene_rect_t *source,
                                    const graphene_rect_t *dest,
                                    gboolean               above,
                                    GdkSubsurface         *sibling);
  void         (* detach)          (GdkSubsurface         *subsurface);
  GdkTexture * (* get_texture)     (GdkSubsurface         *subsurface);
  void         (* get_source)      (GdkSubsurface         *subsurface,
                                    graphene_rect_t       *source);
  void         (* get_dest)        (GdkSubsurface         *subsurface,
                                    graphene_rect_t       *dest);
};

GType           gdk_subsurface_get_type        (void) G_GNUC_CONST;

GdkSurface *    gdk_subsurface_get_parent      (GdkSubsurface         *subsurface);
gboolean        gdk_subsurface_attach          (GdkSubsurface         *subsurface,
                                                GdkTexture            *texture,
                                                const graphene_rect_t *source,
                                                const graphene_rect_t *dest,
                                                gboolean               above,
                                                GdkSubsurface         *sibling);
void            gdk_subsurface_detach          (GdkSubsurface         *subsurface);
GdkTexture *    gdk_subsurface_get_texture     (GdkSubsurface         *subsurface);
void            gdk_subsurface_get_source      (GdkSubsurface         *subsurface,
                                                graphene_rect_t       *source);
void            gdk_subsurface_get_dest        (GdkSubsurface         *subsurface,
                                               graphene_rect_t       *dest);
gboolean        gdk_subsurface_is_above_parent (GdkSubsurface         *subsurface);


G_END_DECLS

