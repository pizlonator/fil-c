/* GDK - The GIMP Drawing Kit
 *
 * gdkcairocontextprivate.h: specific Cairo wrappers
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

#include "gdkcairocontext.h"

#include "gdkdrawcontextprivate.h"

#include <cairo.h>

G_BEGIN_DECLS

#define GDK_CAIRO_CONTEXT_CLASS(klass) 	        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CAIRO_CONTEXT, GdkCairoContextClass))
#define GDK_IS_CAIRO_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CAIRO_CONTEXT))
#define GDK_CAIRO_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CAIRO_CONTEXT, GdkCairoContextClass))

typedef struct _GdkCairoContextClass GdkCairoContextClass;

struct _GdkCairoContext
{
  GdkDrawContext parent_instance;
};

struct _GdkCairoContextClass
{
  GdkDrawContextClass parent_class;

  cairo_t *     (* cairo_create)                (GdkCairoContext        *self);
};

G_END_DECLS

