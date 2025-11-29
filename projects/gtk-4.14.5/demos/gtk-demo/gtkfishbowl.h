/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_FISHBOWL                  (gtk_fishbowl_get_type ())
#define GTK_FISHBOWL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FISHBOWL, GtkFishbowl))
#define GTK_FISHBOWL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FISHBOWL, GtkFishbowlClass))
#define GTK_IS_FISHBOWL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FISHBOWL))
#define GTK_IS_FISHBOWL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FISHBOWL))
#define GTK_FISHBOWL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FISHBOWL, GtkFishbowlClass))

typedef struct _GtkFishbowl              GtkFishbowl;
typedef struct _GtkFishbowlClass         GtkFishbowlClass;

typedef GtkWidget * (* GtkFishCreationFunc) (void);

struct _GtkFishbowl
{
  GtkWidget parent;
};

struct _GtkFishbowlClass
{
  GtkWidgetClass parent_class;
};

GType      gtk_fishbowl_get_type          (void) G_GNUC_CONST;

GtkWidget* gtk_fishbowl_new               (void);

guint      gtk_fishbowl_get_count         (GtkFishbowl       *fishbowl);
void       gtk_fishbowl_set_count         (GtkFishbowl       *fishbowl,
                                           guint              count);
gboolean   gtk_fishbowl_get_animating     (GtkFishbowl       *fishbowl);
void       gtk_fishbowl_set_animating     (GtkFishbowl       *fishbowl,
                                           gboolean           animating);
gboolean   gtk_fishbowl_get_benchmark     (GtkFishbowl       *fishbowl);
void       gtk_fishbowl_set_benchmark     (GtkFishbowl       *fishbowl,
                                           gboolean           animating);
double     gtk_fishbowl_get_framerate     (GtkFishbowl       *fishbowl);
gint64     gtk_fishbowl_get_update_delay  (GtkFishbowl       *fishbowl);
void       gtk_fishbowl_set_update_delay  (GtkFishbowl       *fishbowl,
                                           gint64             update_delay);
void       gtk_fishbowl_set_creation_func (GtkFishbowl       *fishbowl,
                                           GtkFishCreationFunc creation_func);

G_END_DECLS
