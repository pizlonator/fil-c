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

#include "config.h"

#include "gtklistitemfactoryprivate.h"

#include "gtklistitemprivate.h"

/**
 * GtkListItemFactory:
 *
 * A `GtkListItemFactory` creates widgets for the items taken from a `GListModel`.
 *
 * This is one of the core concepts of handling list widgets such
 * as [class@Gtk.ListView] or [class@Gtk.GridView].
 *
 * The `GtkListItemFactory` is tasked with creating widgets for items
 * taken from the model when the views need them and updating them as
 * the items displayed by the view change.
 *
 * A view is usually only able to display anything after both a factory
 * and a model have been set on the view. So it is important that you do
 * not skip this step when setting up your first view.
 *
 * Because views do not display the whole list at once but only a few
 * items, they only need to maintain a few widgets at a time. They will
 * instruct the `GtkListItemFactory` to create these widgets and bind them
 * to the items that are currently displayed.
 *
 * As the list model changes or the user scrolls to the list, the items will
 * change and the view will instruct the factory to bind the widgets to those
 * new items.
 *
 * The actual widgets used for displaying those widgets is provided by you.
 *
 * When the factory needs widgets created, it will create a `GtkListItem`
 * and hand it to your code to set up a widget for. This list item will provide
 * various properties with information about what item to display and provide
 * you with some opportunities to configure its behavior. See the
 * [class@Gtk.ListItem] documentation for further details.
 *
 * Various implementations of `GtkListItemFactory` exist to allow you different
 * ways to provide those widgets. The most common implementations are
 * [class@Gtk.BuilderListItemFactory] which takes a `GtkBuilder` .ui file
 * and then creates widgets and manages everything automatically from the
 * information in that file and [class@Gtk.SignalListItemFactory] which allows
 * you to connect to signals with your own code and retain full control over
 * how the widgets are setup and managed.
 *
 * A `GtkListItemFactory` is supposed to be final - that means its behavior should
 * not change and the first widget created from it should behave the same way as
 * the last widget created from it.
 * If you intend to do changes to the behavior, it is recommended that you create
 * a new `GtkListItemFactory` which will allow the views to recreate its widgets.
 *
 * Once you have chosen your factory and created it, you need to set it
 * on the view widget you want to use it with, such as via
 * [method@Gtk.ListView.set_factory]. Reusing factories across different
 * views is allowed, but very uncommon.
 */

G_DEFINE_TYPE (GtkListItemFactory, gtk_list_item_factory, G_TYPE_OBJECT)

static void
gtk_list_item_factory_default_setup (GtkListItemFactory *self,
                                     GObject            *item,
                                     gboolean            bind,
                                     GFunc               func,
                                     gpointer            data)
{
  if (func)
    func (item, data);
}

static void
gtk_list_item_factory_default_teardown (GtkListItemFactory *self,
                                        GObject            *item,
                                        gboolean            unbind,
                                        GFunc               func,
                                        gpointer            data)
{
  if (func)
    func (item, data);
}

static void                  
gtk_list_item_factory_default_update (GtkListItemFactory *self,
                                      GObject            *item,
                                      gboolean            unbind,
                                      gboolean            bind,
                                      GFunc               func,
                                      gpointer            data)
{
  if (func)
    func (item, data);
}

static void
gtk_list_item_factory_class_init (GtkListItemFactoryClass *klass)
{
  klass->setup = gtk_list_item_factory_default_setup;
  klass->teardown = gtk_list_item_factory_default_teardown;
  klass->update = gtk_list_item_factory_default_update;
}

static void
gtk_list_item_factory_init (GtkListItemFactory *self)
{
}

void
gtk_list_item_factory_setup (GtkListItemFactory *self,
                             GObject            *item,
                             gboolean            bind,
                             GFunc               func,
                             gpointer            data)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->setup (self, item, bind, func, data);
}

void
gtk_list_item_factory_teardown (GtkListItemFactory *self,
                                GObject            *item,
                                gboolean            unbind,
                                GFunc               func,
                                gpointer            data)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->teardown (self, item, unbind, func, data);
}

void
gtk_list_item_factory_update (GtkListItemFactory *self,
                              GObject            *item,
                              gboolean            unbind,
                              gboolean            bind,
                              GFunc               func,
                              gpointer            data)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (G_IS_OBJECT (item));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->update (self, item, unbind, bind, func, data);
}
