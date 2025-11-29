/* gtkatspicomponent.c: AT-SPI Component implementation
 *
 * Copyright 2020 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkatspicomponentprivate.h"

#include "gtkatspicontextprivate.h"
#include "gtkatspiprivate.h"
#include "gtkatspiutilsprivate.h"
#include "gtkatspisocket.h"
#include "gtkpopover.h"

#include "a11y/atspi/atspi-component.h"

static GtkAccessible *
find_first_accessible_non_socket (GtkAccessible *accessible)
{
  GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);

  while (parent != NULL)
    {
      g_object_unref (parent);

      if (!GTK_IS_AT_SPI_SOCKET (parent))
        return parent;

      parent = gtk_accessible_get_accessible_parent (parent);
    }

  return NULL;
}

static void
translate_coordinates_to_accessible (GtkAccessible  *accessible,
                                     AtspiCoordType  coordtype,
                                     int             xi,
                                     int             yi,
                                     int            *xo,
                                     int            *yo)
{
  GtkAccessible *parent;
  int x, y, width, height;

  if (coordtype == ATSPI_COORD_TYPE_SCREEN)
    {
      *xo = 0;
      *yo = 0;
      return;
    }

  if (!gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      *xo = xi;
      *yo = yi;
      return;
    }

  // Transform coords to our parent, we will need that in any case
  *xo = xi - x;
  *yo = yi - y;

  // If that's what the caller requested, we're done
  if (coordtype == ATSPI_COORD_TYPE_PARENT)
    return;

  if (coordtype == ATSPI_COORD_TYPE_WINDOW)
    {
      parent = gtk_accessible_get_accessible_parent (accessible);
      while (parent != NULL)
        {
          g_object_unref (parent);

          if (gtk_accessible_get_bounds (parent, &x, &y, &width, &height))
            {
              *xo = *xo - x;
              *yo = *yo - y;
              parent = gtk_accessible_get_accessible_parent (parent);
            }
          else
            break;
        }
    }
  else
    g_assert_not_reached ();
}

static void
translate_coordinates_from_accessible (GtkAccessible *accessible,
                                 AtspiCoordType     coordtype,
                                 int                xi,
                                 int                yi,
                                 int               *xo,
                                 int               *yo)
{
  GtkAccessible *parent;
  int x, y, width, height;

  if (coordtype == ATSPI_COORD_TYPE_SCREEN)
    {
      *xo = 0;
      *yo = 0;
      return;
    }

  if (!gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      *xo = xi;
      *yo = yi;
      return;
    }

  // Transform coords to our parent, we will need that in any case
  *xo = xi + x;
  *yo = yi + y;

  // If that's what the caller requested, we're done
  if (coordtype == ATSPI_COORD_TYPE_PARENT)
    return;

  if (coordtype == ATSPI_COORD_TYPE_WINDOW)
    {
      parent = gtk_accessible_get_accessible_parent (accessible);
      while (parent != NULL)
        {
          g_object_unref (parent);

          if (gtk_accessible_get_bounds (parent, &x, &y, &width, &height))
            {
              *xo = *xo + x;
              *yo = *yo + y;
              parent = gtk_accessible_get_accessible_parent (parent);
            }
          else
            break;
        }
    }
  else
    g_assert_not_reached ();
}

static GtkAccessible *
accessible_at_point (GtkAccessible *parent,
                     int            x,
                     int            y,
                     bool           children_only)
{
  GtkAccessible *result = NULL;
  int px, py, width, height;

  if (!gtk_accessible_get_bounds (parent, &px, &py, &width, &height))
    return NULL;

  if (!children_only && x >= px && x <= px + width && y >= py && y <= py + height)
    result = parent;

  for (GtkAccessible *child = gtk_accessible_get_first_accessible_child (parent);
       child != NULL;
       child = gtk_accessible_get_next_accessible_sibling (child))
    {
      GtkAccessible *found = accessible_at_point (child, x - px, y - py, FALSE);
      g_object_unref (child);
      if (found)
        result = found;
    }

  return result;
}

static void
component_handle_method (GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);

  if (GTK_IS_AT_SPI_SOCKET (accessible))
     accessible = find_first_accessible_non_socket (accessible);

  if (g_strcmp0 (method_name, "Contains") == 0)
    {
      int x, y;
      int bounds_x, bounds_y, width, height;
      AtspiCoordType coordtype;
      gboolean ret;

      g_variant_get (parameters, "(iiu)", &x, &y, &coordtype);

      translate_coordinates_to_accessible (accessible, coordtype, x, y, &x, &y);

      if (gtk_accessible_get_bounds (accessible, &bounds_x, &bounds_y, &width, &height))
        ret = x >= 0 && x <= bounds_x && y >= 0 && y <= bounds_y;
      else
        ret = FALSE;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "GetAccessibleAtPoint") == 0)
    {
      int x, y;
      AtspiCoordType coordtype;
      GtkAccessible *child;

      g_variant_get (parameters, "(iiu)", &x, &y, &coordtype);

      translate_coordinates_to_accessible (accessible, coordtype, x, y, &x, &y);

      child = accessible_at_point (accessible, x, y, TRUE);
      if (!child)
        {
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", gtk_at_spi_null_ref ()));
        }
      else
        {
          GtkATContext *context = gtk_accessible_get_at_context (child);
          GtkAtSpiContext *ctx = GTK_AT_SPI_CONTEXT (context);

          /* Realize the ATContext in order to get its ref */
          gtk_at_context_realize (context);

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", gtk_at_spi_context_to_ref (ctx)));

          g_object_unref (context);
        }
    }
  else if (g_strcmp0 (method_name, "GetExtents") == 0)
    {
      AtspiCoordType coordtype;
      int x, y, width, height;

      gtk_accessible_get_bounds (accessible, &x, &y, &width, &height);

      g_variant_get (parameters, "(u)", &coordtype);

      translate_coordinates_from_accessible (accessible, coordtype, 0, 0, &x, &y);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("((iiii))", x, y, width, height));
    }
  else if (g_strcmp0 (method_name, "GetPosition") == 0)
    {
      AtspiCoordType coordtype;
      int x, y;

      g_variant_get (parameters, "(u)", &coordtype);

      translate_coordinates_from_accessible (accessible, coordtype, 0, 0, &x, &y);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", x, y));
    }
  else if (g_strcmp0 (method_name, "GetSize") == 0)
    {
      int x, y, width, height;

      gtk_accessible_get_bounds (accessible, &x, &y, &width, &height);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", width, height));
    }
  else if (g_strcmp0 (method_name, "GetLayer") == 0)
    {
      AtspiComponentLayer layer;

      if (self->accessible_role == GTK_ACCESSIBLE_ROLE_WINDOW)
        layer = ATSPI_COMPONENT_LAYER_WINDOW;
      else if (GTK_IS_POPOVER (accessible))
        layer = ATSPI_COMPONENT_LAYER_POPUP;
      else
        layer = ATSPI_COMPONENT_LAYER_WIDGET;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", layer));
    }
  else if (g_strcmp0 (method_name, "GetMDIZOrder") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(n)", 0));
    }
  else if (g_strcmp0 (method_name, "GrabFocus") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "GetAlpha") == 0)
    {
      double opacity = 1.0;

      if (GTK_IS_WIDGET (accessible))
        opacity = gtk_widget_get_opacity (GTK_WIDGET (accessible));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(d)", opacity));
    }
  else if (g_strcmp0 (method_name, "SetExtents") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "SetPosition") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "SetSize") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "ScrollTo") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "ScrollToPoint") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
}

static const GDBusInterfaceVTable component_vtable = {
  component_handle_method,
};

const GDBusInterfaceVTable *
gtk_atspi_get_component_vtable (GtkAccessible *accessible)
{
  return &component_vtable;
}
