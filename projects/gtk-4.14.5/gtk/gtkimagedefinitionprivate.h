/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

G_BEGIN_DECLS

typedef union _GtkImageDefinition GtkImageDefinition;

GtkImageDefinition *    gtk_image_definition_new_empty          (void);
GtkImageDefinition *    gtk_image_definition_new_icon_name      (const char                     *icon_name);
GtkImageDefinition *    gtk_image_definition_new_gicon          (GIcon                          *gicon);
GtkImageDefinition *    gtk_image_definition_new_paintable      (GdkPaintable                   *paintable);

GtkImageDefinition *    gtk_image_definition_ref                (GtkImageDefinition             *def);
void                    gtk_image_definition_unref              (GtkImageDefinition             *def);

GtkImageType            gtk_image_definition_get_storage_type   (const GtkImageDefinition       *def);
int                     gtk_image_definition_get_scale          (const GtkImageDefinition       *def);
const char *           gtk_image_definition_get_icon_name      (const GtkImageDefinition       *def);
GIcon *                 gtk_image_definition_get_gicon          (const GtkImageDefinition       *def);
GdkPaintable *          gtk_image_definition_get_paintable      (const GtkImageDefinition       *def);


G_END_DECLS

