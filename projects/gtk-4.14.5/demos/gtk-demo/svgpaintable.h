#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SVG_TYPE_PAINTABLE (svg_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SvgPaintable, svg_paintable, SVG, PAINTABLE, GObject)

GdkPaintable * svg_paintable_new (GFile *file);

G_END_DECLS
