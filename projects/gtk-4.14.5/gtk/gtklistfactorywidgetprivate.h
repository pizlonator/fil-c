/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtklistitembaseprivate.h"

#include "gtklistitemfactory.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_FACTORY_WIDGET         (gtk_list_factory_widget_get_type ())
#define GTK_LIST_FACTORY_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_FACTORY_WIDGET, GtkListFactoryWidget))
#define GTK_LIST_FACTORY_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_FACTORY_WIDGET, GtkListFactoryWidgetClass))
#define GTK_IS_LIST_FACTORY_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_FACTORY_WIDGET))
#define GTK_IS_LIST_FACTORY_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_FACTORY_WIDGET))
#define GTK_LIST_FACTORY_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_FACTORY_WIDGET, GtkListFactoryWidgetClass))

typedef struct _GtkListFactoryWidget GtkListFactoryWidget;
typedef struct _GtkListFactoryWidgetClass GtkListFactoryWidgetClass;

struct _GtkListFactoryWidget
{
  GtkListItemBase parent_instance;
};

struct _GtkListFactoryWidgetClass
{
  GtkListItemBaseClass parent_class;

  void          (* activate_signal)                             (GtkListFactoryWidget         *self);

  gpointer      (* create_object)                               (GtkListFactoryWidget         *self);
  void          (* setup_object)                                (GtkListFactoryWidget         *self,
                                                                 gpointer                      object);
  void          (* update_object)                               (GtkListFactoryWidget         *self,
                                                                 gpointer                      object,
                                                                 guint                         position,
                                                                 gpointer                      item,
                                                                 gboolean                      selected);
  void          (* teardown_object)                             (GtkListFactoryWidget         *self,
                                                                 gpointer                      object);
};

GType                   gtk_list_factory_widget_get_type        (void) G_GNUC_CONST;

gpointer                gtk_list_factory_widget_get_object      (GtkListFactoryWidget   *self);

void                    gtk_list_factory_widget_set_factory     (GtkListFactoryWidget   *self,
                                                                 GtkListItemFactory     *factory);
GtkListItemFactory *    gtk_list_factory_widget_get_factory     (GtkListFactoryWidget   *self);

void                    gtk_list_factory_widget_set_single_click_activate
                                                                (GtkListFactoryWidget   *self,
                                                                 gboolean                single_click_activate);
gboolean                gtk_list_factory_widget_get_single_click_activate
                                                                (GtkListFactoryWidget   *self);

void                    gtk_list_factory_widget_set_activatable (GtkListFactoryWidget   *self,
                                                                 gboolean                activatable);
gboolean                gtk_list_factory_widget_get_activatable (GtkListFactoryWidget   *self);

void                    gtk_list_factory_widget_set_selectable  (GtkListFactoryWidget   *self,
                                                                 gboolean                activatable);
gboolean                gtk_list_factory_widget_get_selectable  (GtkListFactoryWidget   *self);

G_END_DECLS

