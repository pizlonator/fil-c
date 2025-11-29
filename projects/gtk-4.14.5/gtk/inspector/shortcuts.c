/*
 * Copyright (c) 2020 Red Hat, Inc.
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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "shortcuts.h"
#include "gtklabel.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtkshortcut.h"
#include "gtkshortcuttrigger.h"
#include "gtkshortcutcontroller.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtkcolumnview.h"
#include "gtkcolumnviewcolumn.h"
#include "gtkscrolledwindow.h"
#include "gtknoselection.h"
#include "gtkbinlayout.h"


struct _GtkInspectorShortcuts
{
  GtkWidget parent;

  GtkWidget *view;
};

G_DEFINE_TYPE (GtkInspectorShortcuts, gtk_inspector_shortcuts, GTK_TYPE_WIDGET)

static void
setup_row (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_trigger (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item,
              gpointer                  data)
{
  GtkWidget *label;
  GtkShortcut *shortcut;
  GtkShortcutTrigger *trigger;
  char *str;

  label = gtk_list_item_get_child (list_item);
  shortcut = gtk_list_item_get_item (list_item);
  trigger = gtk_shortcut_get_trigger (shortcut);
  str = gtk_shortcut_trigger_to_label (trigger, gtk_widget_get_display (label));
  gtk_label_set_label (GTK_LABEL (label), str);
  g_free (str);
}

static void
bind_action (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item,
             gpointer                  data)
{
  GtkWidget *label;
  GtkShortcut *shortcut;
  GtkShortcutAction *action;
  char *str;

  label = gtk_list_item_get_child (list_item);
  shortcut = gtk_list_item_get_item (list_item);
  action = gtk_shortcut_get_action (shortcut);
  str = gtk_shortcut_action_to_string (action);
  gtk_label_set_label (GTK_LABEL (label), str);
  g_free (str);
}

static void
gtk_inspector_shortcuts_init (GtkInspectorShortcuts *self)
{
  GtkWidget *sw;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;

  sw = gtk_scrolled_window_new ();

  self->view = gtk_column_view_new (NULL);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_trigger), NULL);

  column = gtk_column_view_column_new ("Trigger", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_action), NULL);

  column = gtk_column_view_column_new ("Action", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), self->view);

  gtk_widget_set_parent (sw, GTK_WIDGET (self));
}

void
gtk_inspector_shortcuts_set_object (GtkInspectorShortcuts *self,
                                    GObject               *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkNoSelection *no_selection;

  stack = gtk_widget_get_parent (GTK_WIDGET (self));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (self));

  if (!GTK_IS_SHORTCUT_CONTROLLER (object))
    {
      gtk_column_view_set_model (GTK_COLUMN_VIEW (self->view), NULL);
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  no_selection = gtk_no_selection_new (g_object_ref (G_LIST_MODEL (object)));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (self->view), GTK_SELECTION_MODEL (no_selection));
  g_object_unref (no_selection);
}

static void
dispose (GObject *object)
{
  GtkInspectorShortcuts *self = GTK_INSPECTOR_SHORTCUTS (object);

  gtk_widget_unparent (gtk_widget_get_first_child (GTK_WIDGET (self)));

  G_OBJECT_CLASS (gtk_inspector_shortcuts_parent_class)->dispose (object);
}

static void
gtk_inspector_shortcuts_class_init (GtkInspectorShortcutsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}
