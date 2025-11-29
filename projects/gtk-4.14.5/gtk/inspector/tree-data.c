/*
 * Copyright (c) 2014, 2020 Red Hat, Inc.
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

#include "tree-data.h"

#include "object-tree.h"

#include "deprecated/gtktreeview.h"
#include "deprecated/gtkcellrenderertext.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

struct _GtkInspectorTreeData
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *swin;
  GtkTreeModel *object;
  GtkTreeModel *types;
  GtkTreeView *view;
  GtkWidget *object_title;
  gboolean show_data;
};

typedef struct _GtkInspectorTreeDataClass GtkInspectorTreeDataClass;
struct _GtkInspectorTreeDataClass
{
  GtkWidgetClass parent_class;
};


G_DEFINE_TYPE (GtkInspectorTreeData, gtk_inspector_tree_data, GTK_TYPE_WIDGET)

static void
gtk_inspector_tree_data_init (GtkInspectorTreeData *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (GTK_WIDGET (sl))),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
cell_data_func (GtkTreeViewColumn *col,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  int num;
  GValue gvalue = { 0, };
  char *value;

  num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (col), "num"));
  gtk_tree_model_get_value (model, iter, num, &gvalue);
  value = g_strdup_value_contents (&gvalue);
  g_object_set (cell, "text", value ? value : "", NULL);
  g_free (value);
  g_value_unset (&gvalue);
}

static void
add_columns (GtkInspectorTreeData *sl)
{
  int n_columns;
  GtkCellRenderer *cell;
  GType type;
  char *title;
  GtkTreeViewColumn *col;
  int i;

  n_columns = gtk_tree_model_get_n_columns (sl->object);
  for (i = 0; i < n_columns; i++)
    {
      cell = gtk_cell_renderer_text_new ();
      type = gtk_tree_model_get_column_type (sl->object, i);
      title = g_strdup_printf ("%d: %s", i, g_type_name (type));
      col = gtk_tree_view_column_new_with_attributes (title, cell, NULL);
      g_object_set_data (G_OBJECT (col), "num", GINT_TO_POINTER (i));
      gtk_tree_view_column_set_cell_data_func (col, cell, cell_data_func, sl, NULL);
      gtk_tree_view_append_column (sl->view, col);
      g_free (title);
    }
}

static void
show_types (GtkInspectorTreeData *sl)
{
  gtk_tree_view_set_model (sl->view, NULL);
  sl->show_data = FALSE;
}

static void
show_data (GtkInspectorTreeData *sl)
{
  gtk_tree_view_set_model (sl->view, sl->object);
  sl->show_data = TRUE;
}

static void
clear_view (GtkInspectorTreeData *sl)
{
  gtk_tree_view_set_model (sl->view, NULL);
  while (gtk_tree_view_get_n_columns (sl->view) > 0)
    gtk_tree_view_remove_column (sl->view,
                                 gtk_tree_view_get_column (sl->view, 0));
}

void
gtk_inspector_tree_data_set_object (GtkInspectorTreeData *sl,
                                    GObject              *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  char *title;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  clear_view (sl);
  sl->object = NULL;
  sl->show_data = FALSE;

  if (!GTK_IS_TREE_MODEL (object))
    {
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  title = gtk_inspector_get_object_title (object);
  gtk_label_set_label (GTK_LABEL (sl->object_title), title);
  g_free (title);

  g_object_set (page, "visible", TRUE, NULL);

  sl->object = GTK_TREE_MODEL (object);
  add_columns (sl);
  show_types (sl);
}

static void
toggle_show (GtkToggleButton      *button,
             GtkInspectorTreeData *sl)
{
  if (gtk_toggle_button_get_active (button) == sl->show_data)
    return;

  if (gtk_toggle_button_get_active (button))
    show_data (sl);
  else
    show_types (sl);
}

static void
dispose (GObject *object)
{
  GtkInspectorTreeData *sl = GTK_INSPECTOR_TREE_DATA (object);

  gtk_widget_dispose_template (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_TREE_DATA);

  G_OBJECT_CLASS (gtk_inspector_tree_data_parent_class)->dispose (object);
}

static void
gtk_inspector_tree_data_class_init (GtkInspectorTreeDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/tree-data.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorTreeData, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorTreeData, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorTreeData, view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorTreeData, object_title);
  gtk_widget_class_bind_template_callback (widget_class, toggle_show);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

// vim: set et sw=2 ts=2:
