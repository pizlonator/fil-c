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

#include "list-data.h"

#include "gtkcolumnview.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"
#include "gtknoselection.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "window.h"


struct _GtkInspectorListData
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *swin;
  GListModel *object;
  GtkColumnView *view;
  GtkWidget *items_label;
};

struct _GtkInspectorListDataClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkInspectorListData, gtk_inspector_list_data, GTK_TYPE_WIDGET)

static void
gtk_inspector_list_data_init (GtkInspectorListData *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (GTK_WIDGET (sl))),
                                  GTK_ORIENTATION_VERTICAL);
}

void
gtk_inspector_list_data_set_object (GtkInspectorListData *sl,
                                    GObject              *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  char *text;
  GtkNoSelection *selection;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  gtk_column_view_set_model (sl->view, NULL);
  sl->object = NULL;

  if (!G_IS_LIST_MODEL (object))
    {
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  text = g_strdup_printf ("%u items", g_list_model_get_n_items (G_LIST_MODEL (object)));
  gtk_label_set_label (GTK_LABEL (sl->items_label), text);
  g_free (text);

  g_object_set (page, "visible", TRUE, NULL);

  sl->object = G_LIST_MODEL (object);
  selection = gtk_no_selection_new (g_object_ref (sl->object));
  gtk_column_view_set_model (sl->view, GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);
}

static void
setup_object (GtkSignalListItemFactory *factory,
              GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (item, label);
}

static void
bind_object (GtkSignalListItemFactory *factory,
             GtkListItem              *item)
{
  GtkWidget *label;
  gpointer obj;
  char *text;

  label = gtk_list_item_get_child (item);
  obj = gtk_list_item_get_item (item);

  text = g_strdup_printf ("%p", obj);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
}

static void
setup_type (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (item, label);
}

static void
bind_type (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  GtkWidget *label;
  gpointer obj;

  label = gtk_list_item_get_child (item);
  obj = gtk_list_item_get_item (item);

  gtk_label_set_label (GTK_LABEL (label), G_OBJECT_TYPE_NAME (obj));
}

static void
setup_props (GtkSignalListItemFactory *factory,
             GtkListItem              *item)
{
  GtkWidget *button;

  button = gtk_button_new_with_label ("Properties");
  gtk_widget_add_css_class (button, "cell");
  gtk_widget_set_halign (button, GTK_ALIGN_START);
  gtk_list_item_set_child (item, button);
}

static void
object_properties (GtkWidget   *button,
                   GtkListItem *item)
{
  GtkInspectorWindow *iw;
  GObject *obj;
  guint pos;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (button, GTK_TYPE_INSPECTOR_WINDOW));

  obj = gtk_list_item_get_item (item);
  pos = gtk_list_item_get_position (item);
  gtk_inspector_window_push_object (iw, obj, CHILD_KIND_LISTITEM, pos);
}

static void
bind_props (GtkSignalListItemFactory *factory,
            GtkListItem              *item,
            GtkInspectorListData     *sl)
{
  g_signal_connect (gtk_list_item_get_child (item), "clicked",
                    G_CALLBACK (object_properties), item);
}

static void
unbind_props (GtkSignalListItemFactory *factory,
              GtkListItem              *item)
{
  g_signal_handlers_disconnect_by_func (gtk_list_item_get_child (item), object_properties, item);
}

static void
dispose (GObject *object)
{
  GtkInspectorListData *sl = GTK_INSPECTOR_LIST_DATA (object);

  gtk_widget_dispose_template (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_LIST_DATA);

  G_OBJECT_CLASS (gtk_inspector_list_data_parent_class)->dispose (object);
}

static void
gtk_inspector_list_data_class_init (GtkInspectorListDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/list-data.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorListData, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorListData, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorListData, view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorListData, items_label);

  gtk_widget_class_bind_template_callback (widget_class, setup_object);
  gtk_widget_class_bind_template_callback (widget_class, bind_object);
  gtk_widget_class_bind_template_callback (widget_class, setup_type);
  gtk_widget_class_bind_template_callback (widget_class, bind_type);
  gtk_widget_class_bind_template_callback (widget_class, setup_props);
  gtk_widget_class_bind_template_callback (widget_class, bind_props);
  gtk_widget_class_bind_template_callback (widget_class, unbind_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

// vim: set et sw=2 ts=2:
