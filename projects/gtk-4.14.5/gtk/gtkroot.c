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

#include "gtkrootprivate.h"
#include "gtknative.h"
#include "gtknativeprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdkprivate.h"
#include "gtkprivate.h"

#include "gtkshortcutmanager.h"

/**
 * GtkRoot:
 *
 * `GtkRoot` is the interface implemented by all widgets that can act as a toplevel
 * widget.
 *
 * The root widget takes care of providing the connection to the windowing system
 * and manages layout, drawing and event delivery for its widget hierarchy.
 *
 * The obvious example of a `GtkRoot` is `GtkWindow`.
 *
 * To get the display to which a `GtkRoot` belongs, use
 * [method@Gtk.Root.get_display].
 *
 * `GtkRoot` also maintains the location of keyboard focus inside its widget
 * hierarchy, with [method@Gtk.Root.set_focus] and [method@Gtk.Root.get_focus].
 */

G_DEFINE_INTERFACE_WITH_CODE (GtkRoot, gtk_root, GTK_TYPE_WIDGET,
                              g_type_interface_add_prerequisite (g_define_type_id, GTK_TYPE_NATIVE))

static GdkDisplay *
gtk_root_default_get_display (GtkRoot *self)
{
  return gdk_display_get_default ();
}


static GtkConstraintSolver *
gtk_root_default_get_constraint_solver (GtkRoot *self)
{
  return NULL;
}

static GtkWidget *
gtk_root_default_get_focus (GtkRoot *self)
{
  return NULL;
}

static void
gtk_root_default_set_focus (GtkRoot   *self,
                            GtkWidget *focus)
{
}

static void
gtk_root_default_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_root_default_get_display;
  iface->get_constraint_solver = gtk_root_default_get_constraint_solver;
  iface->get_focus = gtk_root_default_get_focus;
  iface->set_focus = gtk_root_default_set_focus;
}

/**
 * gtk_root_get_display:
 * @self: a `GtkRoot`
 *
 * Returns the display that this `GtkRoot` is on.
 *
 * Returns: (transfer none): the display of @root
 */
GdkDisplay *
gtk_root_get_display (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_display (self);
}

GtkConstraintSolver *
gtk_root_get_constraint_solver (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_constraint_solver (self);
}

/**
 * gtk_root_set_focus:
 * @self: a `GtkRoot`
 * @focus: (nullable): widget to be the new focus widget, or %NULL
 *    to unset the focus widget
 *
 * If @focus is not the current focus widget, and is focusable, sets
 * it as the focus widget for the root.
 *
 * If @focus is %NULL, unsets the focus widget for the root.
 *
 * To set the focus to a particular widget in the root, it is usually
 * more convenient to use [method@Gtk.Widget.grab_focus] instead of
 * this function.
 */
void
gtk_root_set_focus (GtkRoot   *self,
                    GtkWidget *focus)
{
  g_return_if_fail (GTK_IS_ROOT (self));
  g_return_if_fail (focus == NULL || GTK_IS_WIDGET (focus));

  GTK_ROOT_GET_IFACE (self)->set_focus (self, focus);
}

/**
 * gtk_root_get_focus:
 * @self: a `GtkRoot`
 *
 * Retrieves the current focused widget within the root.
 *
 * Note that this is the widget that would have the focus
 * if the root is active; if the root is not focused then
 * `gtk_widget_has_focus (widget)` will be %FALSE for the
 * widget.
 *
 * Returns: (nullable) (transfer none): the currently focused widget
 */
GtkWidget *
gtk_root_get_focus (GtkRoot *self)
{
  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  return GTK_ROOT_GET_IFACE (self)->get_focus (self);
}

void
gtk_root_start_layout (GtkRoot *self)
{
  gtk_native_queue_relayout (GTK_NATIVE (self));
}

void
gtk_root_stop_layout (GtkRoot *self)
{
}

void
gtk_root_queue_restyle (GtkRoot *self)
{
  gtk_root_start_layout (self);
}
