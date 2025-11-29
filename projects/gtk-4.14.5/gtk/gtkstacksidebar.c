/*
 * Copyright (c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author:
 *      Ikey Doherty <michael.i.doherty@intel.com>
 */

#include "config.h"

#include "gtkstacksidebar.h"

#include "gtkbinlayout.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkscrolledwindow.h"
#include "gtkseparator.h"
#include "gtkselectionmodel.h"
#include "gtkstack.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <glib/gi18n-lib.h>

/**
 * GtkStackSidebar:
 *
 * A `GtkStackSidebar` uses a sidebar to switch between `GtkStack` pages.
 *
 * In order to use a `GtkStackSidebar`, you simply use a `GtkStack` to
 * organize your UI flow, and add the sidebar to your sidebar area. You
 * can use [method@Gtk.StackSidebar.set_stack] to connect the `GtkStackSidebar`
 * to the `GtkStack`.
 *
 * # CSS nodes
 *
 * `GtkStackSidebar` has a single CSS node with name stacksidebar and
 * style class .sidebar.
 *
 * When circumstances require it, `GtkStackSidebar` adds the
 * .needs-attention style class to the widgets representing the stack
 * pages.
 */

typedef struct _GtkStackSidebarClass   GtkStackSidebarClass;

struct _GtkStackSidebar
{
  GtkWidget parent_instance;

  GtkListBox *list;
  GtkStack *stack;
  GtkSelectionModel *pages;
  GHashTable *rows;
};

struct _GtkStackSidebarClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkStackSidebar, gtk_stack_sidebar, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_STACK,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gtk_stack_sidebar_set_property (GObject    *object,
                                guint       prop_id,
                                const       GValue *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_STACK:
      gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_sidebar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStackSidebar *self = GTK_STACK_SIDEBAR (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, self->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_sidebar_row_selected (GtkListBox    *box,
                                GtkListBoxRow *row,
                                gpointer       userdata)
{
  GtkStackSidebar *self = GTK_STACK_SIDEBAR (userdata);
  guint index;

  if (row == NULL)
    return;

  index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "child-index"));
  gtk_selection_model_select_item (self->pages, index, TRUE);
}

static void
gtk_stack_sidebar_init (GtkStackSidebar *self)
{
  GtkWidget *sw;

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  gtk_widget_set_parent (sw, GTK_WIDGET (self));

  self->list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->list), "navigation-sidebar");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->list),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  C_("accessibility", "Sidebar"),
                                  -1);


  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), GTK_WIDGET (self->list));

  g_signal_connect (self->list, "row-selected",
                    G_CALLBACK (gtk_stack_sidebar_row_selected), self);

  gtk_widget_add_css_class (GTK_WIDGET (self), "sidebar");

  self->rows = g_hash_table_new (NULL, NULL);
}

static void
update_row (GtkStackSidebar *self,
            GtkStackPage    *page,
            GtkWidget       *row)
{
  GtkWidget *item;
  char *title;
  gboolean needs_attention;
  gboolean visible;

  g_object_get (page,
                "title", &title,
                "needs-attention", &needs_attention,
                "visible", &visible,
                NULL);

  item = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));
  gtk_label_set_text (GTK_LABEL (item), title);

  gtk_widget_set_visible (row, visible && title != NULL);

  if (needs_attention)
    gtk_widget_add_css_class (row, "needs-attention");
  else
    gtk_widget_remove_css_class (row, "needs-attention");

  g_free (title);
}

static void
on_page_updated (GtkStackPage    *page,
                 GParamSpec      *pspec,
                 GtkStackSidebar *self)
{
  GtkWidget *row;

  row = g_hash_table_lookup (self->rows, page);
  update_row (self, page, row);
}

static void
add_child (guint            position,
           GtkStackSidebar *self)
{
  GtkWidget *item;
  GtkWidget *row;
  GtkStackPage *page;

  /* Make a pretty item when we add kids */
  item = gtk_label_new ("");
  gtk_widget_set_halign (item, GTK_ALIGN_START);
  gtk_widget_set_valign (item, GTK_ALIGN_CENTER);
  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), item);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY,
                                  item,
                                  NULL,
                                  -1);

  page = g_list_model_get_item (G_LIST_MODEL (self->pages), position);
  update_row (self, page, row);

  gtk_list_box_insert (GTK_LIST_BOX (self->list), row, -1);

  g_object_set_data (G_OBJECT (row), "child-index", GUINT_TO_POINTER (position));
  if (gtk_selection_model_is_selected (self->pages, position))
    gtk_list_box_select_row (self->list, GTK_LIST_BOX_ROW (row));
  else
    gtk_list_box_unselect_row (self->list, GTK_LIST_BOX_ROW (row));

  g_signal_connect (page, "notify", G_CALLBACK (on_page_updated), self);

  g_hash_table_insert (self->rows, page, row);

  g_object_unref (page);
}

static void
populate_sidebar (GtkStackSidebar *self)
{
  guint i, n;

  n = g_list_model_get_n_items (G_LIST_MODEL (self->pages));
  for (i = 0; i < n; i++)
    add_child (i, self);
}

static void
clear_sidebar (GtkStackSidebar *self)
{
  GHashTableIter iter;
  GtkStackPage *page;
  GtkWidget *row;

  g_hash_table_iter_init (&iter, self->rows);
  while (g_hash_table_iter_next (&iter, (gpointer *)&page, (gpointer *)&row))
    {
      gtk_list_box_remove (GTK_LIST_BOX (self->list), row);
      g_hash_table_iter_remove (&iter);
      g_signal_handlers_disconnect_by_func (page, on_page_updated, self);
    }
}

static void
items_changed_cb (GListModel       *model,
                  guint             position,
                  guint             removed,
                  guint             added,
                  GtkStackSidebar  *self)
{
  /* FIXME: we can do better */
  clear_sidebar (self);
  populate_sidebar (self);
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      guint              position,
                      guint              n_items,
                      GtkStackSidebar   *self)
{
  guint i;

  for (i = position; i < position + n_items; i++)
    {
      GtkStackPage *page;
      GtkWidget *row;

      page = g_list_model_get_item (G_LIST_MODEL (self->pages), i);
      row = g_hash_table_lookup (self->rows, page);
      if (gtk_selection_model_is_selected (self->pages, i))
        gtk_list_box_select_row (self->list, GTK_LIST_BOX_ROW (row));
      else
        gtk_list_box_unselect_row (self->list, GTK_LIST_BOX_ROW (row));
      g_object_unref (page);
    }
}

static void
set_stack (GtkStackSidebar *self,
           GtkStack        *stack)
{
  if (stack)
    {
      self->stack = g_object_ref (stack);
      self->pages = gtk_stack_get_pages (stack);
      populate_sidebar (self);
      g_signal_connect (self->pages, "items-changed", G_CALLBACK (items_changed_cb), self);
      g_signal_connect (self->pages, "selection-changed", G_CALLBACK (selection_changed_cb), self);
    }
}

static void
unset_stack (GtkStackSidebar *self)
{
  if (self->stack)
    {
      g_signal_handlers_disconnect_by_func (self->pages, items_changed_cb, self);
      g_signal_handlers_disconnect_by_func (self->pages, selection_changed_cb, self);
      clear_sidebar (self);
      g_clear_object (&self->stack);
      g_clear_object (&self->pages);
    }
}

static void
gtk_stack_sidebar_dispose (GObject *object)
{
  GtkStackSidebar *self = GTK_STACK_SIDEBAR (object);
  GtkWidget *child;

  unset_stack (self);

  /* The scrolled window */
  child = gtk_widget_get_first_child (GTK_WIDGET (self));
  if (child)
    {
      gtk_widget_unparent (child);
      self->list = NULL;
    }

  G_OBJECT_CLASS (gtk_stack_sidebar_parent_class)->dispose (object);
}

static void
gtk_stack_sidebar_finalize (GObject *object)
{
  GtkStackSidebar *self = GTK_STACK_SIDEBAR (object);

  g_hash_table_destroy (self->rows);

  G_OBJECT_CLASS (gtk_stack_sidebar_parent_class)->finalize (object);
}

static void
gtk_stack_sidebar_class_init (GtkStackSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_stack_sidebar_dispose;
  object_class->finalize = gtk_stack_sidebar_finalize;
  object_class->set_property = gtk_stack_sidebar_set_property;
  object_class->get_property = gtk_stack_sidebar_get_property;

   /**
   * GtkStackSidebar:stack: (attributes org.gtk.Property.get=gtk_stack_sidebar_get_stack org.gtk.Property.set=gtk_stack_sidebar_set_stack)
   *
   * The stack.
   */
  obj_properties[PROP_STACK] =
      g_param_spec_object (I_("stack"), NULL, NULL,
                           GTK_TYPE_STACK,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("stacksidebar"));
}

/**
 * gtk_stack_sidebar_new:
 *
 * Creates a new `GtkStackSidebar`.
 *
 * Returns: the new `GtkStackSidebar`
 */
GtkWidget *
gtk_stack_sidebar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_STACK_SIDEBAR, NULL));
}

/**
 * gtk_stack_sidebar_set_stack: (attributes org.gtk.Method.set_property=stack)
 * @self: a `GtkStackSidebar`
 * @stack: a `GtkStack`
 *
 * Set the `GtkStack` associated with this `GtkStackSidebar`.
 *
 * The sidebar widget will automatically update according to
 * the order and items within the given `GtkStack`.
 */
void
gtk_stack_sidebar_set_stack (GtkStackSidebar *self,
                             GtkStack        *stack)
{
  g_return_if_fail (GTK_IS_STACK_SIDEBAR (self));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);


  if (self->stack == stack)
    return;

  unset_stack (self);
  set_stack (self, stack);

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify (G_OBJECT (self), "stack");
}

/**
 * gtk_stack_sidebar_get_stack: (attributes org.gtk.Method.get_property=stack)
 * @self: a `GtkStackSidebar`
 *
 * Retrieves the stack.
 *
 * Returns: (nullable) (transfer none): the associated `GtkStack` or
 *   %NULL if none has been set explicitly
 */
GtkStack *
gtk_stack_sidebar_get_stack (GtkStackSidebar *self)
{
  g_return_val_if_fail (GTK_IS_STACK_SIDEBAR (self), NULL);

  return GTK_STACK (self->stack);
}
