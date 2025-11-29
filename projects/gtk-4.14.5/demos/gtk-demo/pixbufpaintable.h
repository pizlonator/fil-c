#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIXBUF_TYPE_PAINTABLE (pixbuf_paintable_get_type ())

G_DECLARE_FINAL_TYPE(PixbufPaintable, pixbuf_paintable, PIXBUF, PAINTABLE, GObject)

GdkPaintable * pixbuf_paintable_new_from_resource (const char *path);

G_END_DECLS
