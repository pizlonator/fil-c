/* gtkcelleditable.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkCellEditable:
 *
 * Interface for widgets that can be used for editing cells
 *
 * The `GtkCellEditable` interface must be implemented for widgets to be usable
 * to edit the contents of a `GtkTreeView` cell. It provides a way to specify how
 * temporary widgets should be configured for editing, get the new value, etc.
 *
 * Deprecated: 4.10: List views use widgets for displaying their
 *   contents. See [iface@Gtk.Editable] for editable text widgets
 */

#include "config.h"
#include "gtkcelleditable.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef GtkCellEditableIface GtkCellEditableInterface;
G_DEFINE_INTERFACE(GtkCellEditable, gtk_cell_editable, GTK_TYPE_WIDGET)

static void
gtk_cell_editable_default_init (GtkCellEditableInterface *iface)
{
  /**
   * GtkCellEditable:editing-canceled:
   *
   * Indicates whether editing on the cell has been canceled.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("editing-canceled", NULL, NULL,
                                       FALSE,
                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellEditable::editing-done:
   * @cell_editable: the object on which the signal was emitted
   *
   * This signal is a sign for the cell renderer to update its
   * value from the @cell_editable.
   *
   * Implementations of `GtkCellEditable` are responsible for
   * emitting this signal when they are done editing, e.g.
   * `GtkEntry` emits this signal when the user presses Enter. Typical things to
   * do in a handler for ::editing-done are to capture the edited value,
   * disconnect the @cell_editable from signals on the `GtkCellRenderer`, etc.
   *
   * gtk_cell_editable_editing_done() is a convenience method
   * for emitting `GtkCellEditable::editing-done`.
   */
  g_signal_new (I_("editing-done"),
                GTK_TYPE_CELL_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkCellEditableIface, editing_done),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);

  /**
   * GtkCellEditable::remove-widget:
   * @cell_editable: the object on which the signal was emitted
   *
   * This signal is meant to indicate that the cell is finished
   * editing, and the @cell_editable widget is being removed and may
   * subsequently be destroyed.
   *
   * Implementations of `GtkCellEditable` are responsible for
   * emitting this signal when they are done editing. It must
   * be emitted after the `GtkCellEditable::editing-done` signal,
   * to give the cell renderer a chance to update the cell's value
   * before the widget is removed.
   *
   * gtk_cell_editable_remove_widget() is a convenience method
   * for emitting `GtkCellEditable::remove-widget`.
   */
  g_signal_new (I_("remove-widget"),
                GTK_TYPE_CELL_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkCellEditableIface, remove_widget),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);
}

/**
 * gtk_cell_editable_start_editing:
 * @cell_editable: A `GtkCellEditable`
 * @event: (nullable): The `GdkEvent` that began the editing process, or
 *   %NULL if editing was initiated programmatically
 *
 * Begins editing on a @cell_editable.
 *
 * The `GtkCellRenderer` for the cell creates and returns a `GtkCellEditable` from
 * gtk_cell_renderer_start_editing(), configured for the `GtkCellRenderer` type.
 *
 * gtk_cell_editable_start_editing() can then set up @cell_editable suitably for
 * editing a cell, e.g. making the Esc key emit `GtkCellEditable::editing-done`.
 *
 * Note that the @cell_editable is created on-demand for the current edit; its
 * lifetime is temporary and does not persist across other edits and/or cells.
 **/
void
gtk_cell_editable_start_editing (GtkCellEditable *cell_editable,
				 GdkEvent        *event)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  (* GTK_CELL_EDITABLE_GET_IFACE (cell_editable)->start_editing) (cell_editable, event);
}

/**
 * gtk_cell_editable_editing_done:
 * @cell_editable: A `GtkCellEditable`
 *
 * Emits the `GtkCellEditable::editing-done` signal.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_editable_editing_done (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "editing-done");
}

/**
 * gtk_cell_editable_remove_widget:
 * @cell_editable: A `GtkCellEditable`
 *
 * Emits the `GtkCellEditable::remove-widget` signal.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_editable_remove_widget (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "remove-widget");
}
