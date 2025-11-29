/* gtkscrollable.c
 * Copyright (C) 2008 Tadej Borovšak <tadeboro@gmail.com>
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

/**
 * GtkScrollable:
 *
 * `GtkScrollable` is an interface for widgets with native scrolling ability.
 *
 * To implement this interface you should override the
 * [property@Gtk.Scrollable:hadjustment] and
 * [property@Gtk.Scrollable:vadjustment] properties.
 *
 * ## Creating a scrollable widget
 *
 * All scrollable widgets should do the following.
 *
 * - When a parent widget sets the scrollable child widget’s adjustments,
 *   the widget should connect to the [signal@Gtk.Adjustment::value-changed]
 *   signal. The child widget should then populate the adjustments’ properties
 *   as soon as possible, which usually means queueing an allocation right away
 *   and populating the properties in the [vfunc@Gtk.Widget.size_allocate]
 *   implementation.
 *
 * - Because its preferred size is the size for a fully expanded widget,
 *   the scrollable widget must be able to cope with underallocations.
 *   This means that it must accept any value passed to its
 *   [vfunc@Gtk.Widget.size_allocate] implementation.
 *
 * - When the parent allocates space to the scrollable child widget,
 *   the widget must ensure the adjustments’ property values are correct and up
 *   to date, for example using [method@Gtk.Adjustment.configure].
 *
 * - When any of the adjustments emits the [signal@Gtk.Adjustment::value-changed]
 *   signal, the scrollable widget should scroll its contents.
 */

#include "config.h"

#include "gtkscrollable.h"

#include "gtkadjustment.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

G_DEFINE_INTERFACE (GtkScrollable, gtk_scrollable, G_TYPE_OBJECT)

static void
gtk_scrollable_default_init (GtkScrollableInterface *iface)
{
  GParamSpec *pspec;

  /**
   * GtkScrollable:hadjustment: (attributes org.gtk.Property.get=gtk_scrollable_get_hadjustment org.gtk.Property.set=gtk_scrollable_set_hadjustment)
   *
   * Horizontal `GtkAdjustment` of the scrollable widget.
   *
   * This adjustment is shared between the scrollable widget and its parent.
   */
  pspec = g_param_spec_object ("hadjustment", NULL, NULL,
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:vadjustment: (attributes org.gtk.Property.get=gtk_scrollable_get_vadjustment org.gtk.Property.set=gtk_scrollable_set_vadjustment)
   *
   * Vertical `GtkAdjustment` of the scrollable widget.
   *
   * This adjustment is shared between the scrollable widget and its parent.
   */
  pspec = g_param_spec_object ("vadjustment", NULL, NULL,
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:hscroll-policy: (attributes org.gtk.Property.get=gtk_scrollable_get_hscroll_policy org.gtk.Property.set=gtk_scrollable_set_hscroll_policy)
   *
   * Determines when horizontal scrolling should start.
   */
  pspec = g_param_spec_enum ("hscroll-policy", NULL, NULL,
			     GTK_TYPE_SCROLLABLE_POLICY,
			     GTK_SCROLL_MINIMUM,
			     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:vscroll-policy: (attributes org.gtk.Property.get=gtk_scrollable_get_vscroll_policy org.gtk.Property.set=gtk_scrollable_set_vscroll_policy)
   *
   * Determines when vertical scrolling should start.
   */
  pspec = g_param_spec_enum ("vscroll-policy", NULL, NULL,
			     GTK_TYPE_SCROLLABLE_POLICY,
			     GTK_SCROLL_MINIMUM,
			     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  g_object_interface_install_property (iface, pspec);
}

/**
 * gtk_scrollable_get_hadjustment: (attributes org.gtk.Method.get_property=hadjustment)
 * @scrollable: a `GtkScrollable`
 *
 * Retrieves the `GtkAdjustment` used for horizontal scrolling.
 *
 * Returns: (transfer none) (nullable): horizontal `GtkAdjustment`.
 */
GtkAdjustment *
gtk_scrollable_get_hadjustment (GtkScrollable *scrollable)
{
  GtkAdjustment *adj = NULL;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), NULL);

  g_object_get (scrollable, "hadjustment", &adj, NULL);

  /* Horrid hack; g_object_get() returns a new reference but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (adj)
    g_object_unref (adj);

  return adj;
}

/**
 * gtk_scrollable_set_hadjustment: (attributes org.gtk.Method.set_property=hadjustment)
 * @scrollable: a `GtkScrollable`
 * @hadjustment: (nullable): a `GtkAdjustment`
 *
 * Sets the horizontal adjustment of the `GtkScrollable`.
 */
void
gtk_scrollable_set_hadjustment (GtkScrollable *scrollable,
                                GtkAdjustment *hadjustment)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));
  g_return_if_fail (hadjustment == NULL || GTK_IS_ADJUSTMENT (hadjustment));

  g_object_set (scrollable, "hadjustment", hadjustment, NULL);
}

/**
 * gtk_scrollable_get_vadjustment: (attributes org.gtk.Method.get_property=vadjustment)
 * @scrollable: a `GtkScrollable`
 *
 * Retrieves the `GtkAdjustment` used for vertical scrolling.
 *
 * Returns: (transfer none) (nullable): vertical `GtkAdjustment`.
 */
GtkAdjustment *
gtk_scrollable_get_vadjustment (GtkScrollable *scrollable)
{
  GtkAdjustment *adj = NULL;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), NULL);

  g_object_get (scrollable, "vadjustment", &adj, NULL);

  /* Horrid hack; g_object_get() returns a new reference but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (adj)
    g_object_unref (adj);

  return adj;
}

/**
 * gtk_scrollable_set_vadjustment: (attributes org.gtk.Method.set_property=vadjustment)
 * @scrollable: a `GtkScrollable`
 * @vadjustment: (nullable): a `GtkAdjustment`
 *
 * Sets the vertical adjustment of the `GtkScrollable`.
 */
void
gtk_scrollable_set_vadjustment (GtkScrollable *scrollable,
                                GtkAdjustment *vadjustment)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));
  g_return_if_fail (vadjustment == NULL || GTK_IS_ADJUSTMENT (vadjustment));

  g_object_set (scrollable, "vadjustment", vadjustment, NULL);
}


/**
 * gtk_scrollable_get_hscroll_policy: (attributes org.gtk.Method.get_property=hscroll-policy)
 * @scrollable: a `GtkScrollable`
 *
 * Gets the horizontal `GtkScrollablePolicy`.
 *
 * Returns: The horizontal `GtkScrollablePolicy`.
 */
GtkScrollablePolicy
gtk_scrollable_get_hscroll_policy (GtkScrollable *scrollable)
{
  GtkScrollablePolicy policy;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), GTK_SCROLL_MINIMUM);

  g_object_get (scrollable, "hscroll-policy", &policy, NULL);

  return policy;
}

/**
 * gtk_scrollable_set_hscroll_policy: (attributes org.gtk.Method.set_property=hscroll-policy)
 * @scrollable: a `GtkScrollable`
 * @policy: the horizontal `GtkScrollablePolicy`
 *
 * Sets the `GtkScrollablePolicy`.
 *
 * The policy determines whether horizontal scrolling should start
 * below the minimum width or below the natural width.
 */
void
gtk_scrollable_set_hscroll_policy (GtkScrollable       *scrollable,
				   GtkScrollablePolicy  policy)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));

  g_object_set (scrollable, "hscroll-policy", policy, NULL);
}

/**
 * gtk_scrollable_get_vscroll_policy: (attributes org.gtk.Method.get_property=vscroll-policy)
 * @scrollable: a `GtkScrollable`
 *
 * Gets the vertical `GtkScrollablePolicy`.
 *
 * Returns: The vertical `GtkScrollablePolicy`.
 */
GtkScrollablePolicy
gtk_scrollable_get_vscroll_policy (GtkScrollable *scrollable)
{
  GtkScrollablePolicy policy;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), GTK_SCROLL_MINIMUM);

  g_object_get (scrollable, "vscroll-policy", &policy, NULL);

  return policy;
}

/**
 * gtk_scrollable_set_vscroll_policy: (attributes org.gtk.Method.set_property=vscroll-policy)
 * @scrollable: a `GtkScrollable`
 * @policy: the vertical `GtkScrollablePolicy`
 *
 * Sets the `GtkScrollablePolicy`.
 *
 * The policy determines whether vertical scrolling should start
 * below the minimum height or below the natural height.
 */
void
gtk_scrollable_set_vscroll_policy (GtkScrollable       *scrollable,
				   GtkScrollablePolicy  policy)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));

  g_object_set (scrollable, "vscroll-policy", policy, NULL);
}

/**
 * gtk_scrollable_get_border:
 * @scrollable: a `GtkScrollable`
 * @border: (out caller-allocates): return location for the results
 *
 * Returns the size of a non-scrolling border around the
 * outside of the scrollable.
 *
 * An example for this would be treeview headers. GTK can use
 * this information to display overlaid graphics, like the
 * overshoot indication, at the right position.
 *
 * Returns: %TRUE if @border has been set
 */
gboolean
gtk_scrollable_get_border (GtkScrollable *scrollable,
                           GtkBorder     *border)
{
  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), FALSE);
  g_return_val_if_fail (border != NULL, FALSE);

  if (GTK_SCROLLABLE_GET_IFACE (scrollable)->get_border)
    return GTK_SCROLLABLE_GET_IFACE (scrollable)->get_border (scrollable, border);

  return FALSE;
}
