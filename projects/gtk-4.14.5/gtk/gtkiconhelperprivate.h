/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
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

#pragma once

#include "gtk/gtkimage.h"
#include "gtk/gtktypes.h"

#include "gtkcssnodeprivate.h"
#include "gtkimagedefinitionprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_ICON_HELPER (gtk_icon_helper_get_type())

G_DECLARE_FINAL_TYPE(GtkIconHelper, gtk_icon_helper, GTK, ICON_HELPER, GObject)

GtkIconHelper *gtk_icon_helper_new (GtkCssNode    *css_node,
                                    GtkWidget     *owner);

void _gtk_icon_helper_clear (GtkIconHelper *self);

gboolean _gtk_icon_helper_get_is_empty (GtkIconHelper *self);

void _gtk_icon_helper_set_definition (GtkIconHelper *self,
                                      GtkImageDefinition *def);
void _gtk_icon_helper_set_gicon (GtkIconHelper *self,
                                 GIcon *gicon);

void _gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                     const char *icon_name);
void _gtk_icon_helper_set_paintable (GtkIconHelper *self,
				     GdkPaintable  *paintable);

gboolean _gtk_icon_helper_set_pixel_size   (GtkIconHelper *self,
                                            int            pixel_size);
gboolean _gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                            gboolean       use_fallback);

GtkImageType _gtk_icon_helper_get_storage_type (GtkIconHelper *self);
int _gtk_icon_helper_get_pixel_size (GtkIconHelper *self);
gboolean _gtk_icon_helper_get_use_fallback (GtkIconHelper *self);

GIcon *_gtk_icon_helper_peek_gicon (GtkIconHelper *self);
GdkPaintable *_gtk_icon_helper_peek_paintable (GtkIconHelper *self);

GtkImageDefinition *gtk_icon_helper_get_definition (GtkIconHelper *self);
const char *_gtk_icon_helper_get_icon_name (GtkIconHelper *self);

int gtk_icon_helper_get_size (GtkIconHelper *self);

void      gtk_icon_helper_invalidate (GtkIconHelper *self);
void      gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                                 GtkCssStyleChange *change);

void      gtk_icon_size_set_style_classes (GtkCssNode  *cssnode,
                                           GtkIconSize  icon_size);

G_END_DECLS

