/* gtkaccessiblerange.h: Accessible range interface
 *
 * SPDX-FileCopyrightText: 2022 Red Hat Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkaccessible.h>

G_BEGIN_DECLS
#define GTK_TYPE_ACCESSIBLE_RANGE (gtk_accessible_range_get_type())

GDK_AVAILABLE_IN_4_10
G_DECLARE_INTERFACE (GtkAccessibleRange, gtk_accessible_range, GTK, ACCESSIBLE_RANGE, GtkAccessible)

struct _GtkAccessibleRangeInterface
{
  GTypeInterface g_iface;

  /**
   * GtkAccessibleRangeInterface::set_current_value:
   * @self: a `GtkAccessibleRange`
   * @value: the value to set
   *
   * Sets the current value of the accessible range.
   *
   * This operation should behave similarly as if the user performed the
   * action.
   *
   * Returns: true if the operation was performed, false otherwise
   *
   * Since: 4.10
   */
  gboolean (* set_current_value) (GtkAccessibleRange *self,
                                  double              value);
};

G_END_DECLS
