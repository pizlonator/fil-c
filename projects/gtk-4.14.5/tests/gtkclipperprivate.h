#pragma once

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_TYPE_CLIPPER (gtk_clipper_get_type ())

G_DECLARE_FINAL_TYPE (GtkClipper, gtk_clipper, GTK, CLIPPER, GObject)

GdkPaintable * gtk_clipper_new (GdkPaintable          *paintable,
                                const graphene_rect_t *clip);

G_END_DECLS

