/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "controllers.h"

#include "gtkbinlayout.h"
#include "gtkdropdown.h"
#include "gtkbox.h"
#include "gtkcustomsorter.h"
#include "gtkgesture.h"
#include "gtklabel.h"
#include "gtkscrolledwindow.h"
#include "gtksortlistmodel.h"
#include "gtkstack.h"
#include "gtkwidgetprivate.h"
#include "window.h"
#include "gtksignallistitemfactory.h"
#include "gtkcolumnview.h"
#include "gtkcolumnviewcolumn.h"
#include "gtklistitem.h"
#include "gtknoselection.h"

struct _GtkInspectorControllers
{
  GtkWidget parent_instance;

  GtkWidget *view;
};

struct _GtkInspectorControllersClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkInspectorControllers, gtk_inspector_controllers, GTK_TYPE_WIDGET)

static void
row_activated (GtkColumnView           *view,
               guint                    position,
               GtkInspectorControllers *self)
{
  GtkInspectorWindow *iw;
  GListModel *model;
  GObject *controller;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_INSPECTOR_WINDOW));

  model = G_LIST_MODEL (gtk_column_view_get_model (view));
  controller = g_list_model_get_item (model, position);
  gtk_inspector_window_push_object (iw, controller, CHILD_KIND_CONTROLLER, 0);
  g_object_unref (controller);
}

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
bind_type (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkWidget *label;
  GtkEventController *controller;

  label = gtk_list_item_get_child (list_item);
  controller = gtk_list_item_get_item (list_item);

  gtk_label_set_label (GTK_LABEL (label), G_OBJECT_TYPE_NAME (controller));
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkWidget *label;
  GtkEventController *controller;

  label = gtk_list_item_get_child (list_item);
  controller = gtk_list_item_get_item (list_item);

  gtk_label_set_label (GTK_LABEL (label), gtk_event_controller_get_name (controller));
}

static void
bind_phase (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;
  GtkEventController *controller;
  const char *name;

  label = gtk_list_item_get_child (list_item);
  controller = gtk_list_item_get_item (list_item);

  switch (gtk_event_controller_get_propagation_phase (controller))
    {
    case GTK_PHASE_NONE:
      name = C_("event phase", "None");
      break;
    case GTK_PHASE_CAPTURE:
      name = C_("event phase", "Capture");
      break;
    case GTK_PHASE_BUBBLE:
      name = C_("event phase", "Bubble");
      break;
    case GTK_PHASE_TARGET:
      name = C_("event phase", "Target");
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_label_set_label (GTK_LABEL (label), name);
}

static void
bind_limit (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;
  GtkEventController *controller;

  label = gtk_list_item_get_child (list_item);
  controller = gtk_list_item_get_item (list_item);

  if (gtk_event_controller_get_propagation_limit (controller) == GTK_LIMIT_SAME_NATIVE)
    gtk_label_set_label (GTK_LABEL (label), C_("propagation limit", "Native"));
  else
    gtk_label_set_label (GTK_LABEL (label), "");
}

static void
gtk_inspector_controllers_init (GtkInspectorControllers *self)
{
  GtkWidget *sw;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;

  sw = gtk_scrolled_window_new ();

  self->view = gtk_column_view_new (NULL);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_type), NULL);

  column = gtk_column_view_column_new ("Type", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);

  column = gtk_column_view_column_new ("Name", factory);
  gtk_column_view_column_set_expand (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_phase), NULL);

  column = gtk_column_view_column_new ("Phase", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_limit), NULL);

  column = gtk_column_view_column_new ("Limit", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (self->view), column);
  g_object_unref (column);

  g_signal_connect (self->view, "activate", G_CALLBACK (row_activated), self);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), self->view);

  gtk_widget_set_parent (sw, GTK_WIDGET (self));
}

static int
compare_phases (GtkPropagationPhase first,
                GtkPropagationPhase second)
{
  int priorities[] = {
    [GTK_PHASE_NONE] = 0,
    [GTK_PHASE_CAPTURE] = 1,
    [GTK_PHASE_BUBBLE] = 3,
    [GTK_PHASE_TARGET] = 2
  };

  return priorities[first] - priorities[second];
}

static int
compare_controllers (gconstpointer _first,
                     gconstpointer _second,
                     gpointer      unused)
{
  GtkEventController *first = GTK_EVENT_CONTROLLER (_first);
  GtkEventController *second = GTK_EVENT_CONTROLLER (_second);
  GtkPropagationPhase first_phase, second_phase;
  GtkWidget *first_widget, *second_widget;
  int result;

  
  first_phase = gtk_event_controller_get_propagation_phase (first);
  second_phase = gtk_event_controller_get_propagation_phase (second);
  result = compare_phases (first_phase, second_phase);
  if (result != 0)
    return result;

  first_widget = gtk_event_controller_get_widget (first);
  second_widget = gtk_event_controller_get_widget (second);
  if (first_widget == second_widget)
    return 0;

  if (gtk_widget_is_ancestor (first_widget, second_widget))
    result = -1;
  else
    result = 1;

  if (first_phase == GTK_PHASE_BUBBLE)
    result = -result;

  return result;
}

void
gtk_inspector_controllers_set_object (GtkInspectorControllers *self,
                                      GObject                 *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GListModel *model;
  GtkSortListModel *sort_model;
  GtkSorter *sorter;
  GtkNoSelection *no_selection;

  stack = gtk_widget_get_parent (GTK_WIDGET (self));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (self));

  if (!GTK_IS_WIDGET (object))
    {
      gtk_column_view_set_model (GTK_COLUMN_VIEW (self->view), NULL);
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  model = gtk_widget_observe_controllers (GTK_WIDGET (object));
  sorter = GTK_SORTER (gtk_custom_sorter_new (compare_controllers, NULL, NULL));
  sort_model = gtk_sort_list_model_new (model, sorter);
  no_selection = gtk_no_selection_new (G_LIST_MODEL (sort_model));

  gtk_column_view_set_model (GTK_COLUMN_VIEW (self->view), GTK_SELECTION_MODEL (no_selection));

  g_object_unref (no_selection);
}

static void
gtk_inspector_controllers_dispose (GObject *object)
{
  GtkInspectorControllers *self = GTK_INSPECTOR_CONTROLLERS (object);

  gtk_widget_unparent (gtk_widget_get_first_child (GTK_WIDGET (self)));

  G_OBJECT_CLASS (gtk_inspector_controllers_parent_class)->dispose (object);
}

static void
gtk_inspector_controllers_class_init (GtkInspectorControllersClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_inspector_controllers_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:
