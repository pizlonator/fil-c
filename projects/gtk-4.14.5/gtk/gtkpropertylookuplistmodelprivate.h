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

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL         (gtk_property_lookup_list_model_get_type ())
#define GTK_PROPERTY_LOOKUP_LIST_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL, GtkPropertyLookupListModel))
#define GTK_PROPERTY_LOOKUP_LIST_MODEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL, GtkPropertyLookupListModelClass))
#define GTK_IS_PROPERTY_LOOKUP_LIST_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL))
#define GTK_IS_PROPERTY_LOOKUP_LIST_MODEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL))
#define GTK_PROPERTY_LOOKUP_LIST_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL, GtkPropertyLookupListModelClass))

typedef struct _GtkPropertyLookupListModel GtkPropertyLookupListModel;
typedef struct _GtkPropertyLookupListModelClass GtkPropertyLookupListModelClass;

GType                           gtk_property_lookup_list_model_get_type         (void) G_GNUC_CONST;

GtkPropertyLookupListModel *    gtk_property_lookup_list_model_new              (GType                           item_type,
                                                                                 const char                     *property_name);

void                            gtk_property_lookup_list_model_set_object       (GtkPropertyLookupListModel     *self,
                                                                                 gpointer                        object);
gpointer                        gtk_property_lookup_list_model_get_object       (GtkPropertyLookupListModel     *self);


G_END_DECLS

