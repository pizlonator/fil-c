/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_TYPE_GST_PAINTABLE (gtk_gst_paintable_get_type ())

G_DECLARE_FINAL_TYPE (GtkGstPaintable, gtk_gst_paintable, GTK, GST_PAINTABLE, GObject)

GdkPaintable *  gtk_gst_paintable_new                   (void);

void            gtk_gst_paintable_realize               (GtkGstPaintable        *self,
                                                         GdkSurface             *surface);
void            gtk_gst_paintable_unrealize             (GtkGstPaintable        *self,
                                                         GdkSurface             *surface);
void            gtk_gst_paintable_queue_set_texture     (GtkGstPaintable        *self,
                                                         GdkTexture             *texture,
                                                         double                  pixel_aspect_ratio,
                                                         const graphene_rect_t  *viewport);

G_END_DECLS
