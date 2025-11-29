/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkconfig.h"

#include "gdkcairocontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_BROADWAY_CAIRO_CONTEXT		(gdk_broadway_cairo_context_get_type ())
#define GDK_BROADWAY_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_BROADWAY_CAIRO_CONTEXT, GdkBroadwayCairoContext))
#define GDK_IS_BROADWAY_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_BROADWAY_CAIRO_CONTEXT))
#define GDK_BROADWAY_CAIRO_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_CAIRO_CONTEXT, GdkBroadwayCairoContextClass))
#define GDK_IS_BROADWAY_CAIRO_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_CAIRO_CONTEXT))
#define GDK_BROADWAY_CAIRO_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_CAIRO_CONTEXT, GdkBroadwayCairoContextClass))

typedef struct _GdkBroadwayCairoContext GdkBroadwayCairoContext;
typedef struct _GdkBroadwayCairoContextClass GdkBroadwayCairoContextClass;

struct _GdkBroadwayCairoContext
{
  GdkCairoContext parent_instance;

  cairo_surface_t *paint_surface;
};

struct _GdkBroadwayCairoContextClass
{
  GdkCairoContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_broadway_cairo_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

