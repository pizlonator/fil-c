/* GDK - The GIMP Drawing Kit
 *
 * gdkcairocontext-x11.h: X11 specific Cairo wrappers
 *
 * Copyright © 2016  Benjamin Otte
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

#define GDK_TYPE_X11_CAIRO_CONTEXT		(gdk_x11_cairo_context_get_type ())
#define GDK_X11_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_X11_CAIRO_CONTEXT, GdkX11CairoContext))
#define GDK_IS_X11_CAIRO_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_X11_CAIRO_CONTEXT))
#define GDK_X11_CAIRO_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_X11_CAIRO_CONTEXT, GdkX11CairoContextClass))
#define GDK_IS_X11_CAIRO_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_X11_CAIRO_CONTEXT))
#define GDK_X11_CAIRO_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_X11_CAIRO_CONTEXT, GdkX11CairoContextClass))

typedef struct _GdkX11CairoContext GdkX11CairoContext;
typedef struct _GdkX11CairoContextClass GdkX11CairoContextClass;

struct _GdkX11CairoContext
{
  GdkCairoContext parent_instance;

  cairo_surface_t *window_surface;
  cairo_surface_t *paint_surface;
};

struct _GdkX11CairoContextClass
{
  GdkCairoContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_x11_cairo_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

