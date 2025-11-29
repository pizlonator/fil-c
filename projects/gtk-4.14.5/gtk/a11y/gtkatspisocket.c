/* gtkatspisocket.c: AT-SPI socket object
 *
 * Copyright 2024 GNOME Foundation Inc.
 *           2024 Igalia S.L.
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

/**
 * GtkAtApiSocket:
 *
 * `GtkAtApiSocket` is an AT-SPI specific `GtkAccessible` interface for
 * integrating remote accessible objects. It makes the accessible tree
 * of the remote accessible object appear as part of the accessible tree
 * that it belongs to itself.
 */

#include "config.h"

#include "gtkatspisocketprivate.h"

#include "gtkatspicontextprivate.h"
#include "gtktypebuiltins.h"

struct _GtkAtSpiSocket
{
  GObject parent_instance;

  char *bus_name;
  char *object_path;
  GtkATContext *at_context;
  GtkAccessibleRole accessible_role;

  GCancellable *cancellable;
  gboolean embedded;
};

static void g_initable_interface_init (GInitableIface *iface);
static void gtk_accessible_interface_init (GtkAccessibleInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkAtSpiSocket, gtk_at_spi_socket, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_initable_interface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                      gtk_accessible_interface_init))

enum {
  PROP_0,
  PROP_BUS_NAME,
  PROP_OBJECT_PATH,
  N_PROPS,

  /* GtkAccessible */
  PROP_ACCESSIBLE_ROLE,
};

static GParamSpec *properties [N_PROPS];

static void
set_accessible_role (GtkAtSpiSocket    *self,
                     GtkAccessibleRole  role)
{
  g_return_if_fail (!gtk_accessible_role_is_abstract (role));

  if (self->at_context == NULL || !gtk_at_context_is_realized (self->at_context))
    {
      self->accessible_role = role;

      if (self->at_context != NULL)
        gtk_at_context_set_accessible_role (self->at_context, role);

      g_object_notify (G_OBJECT (self), "accessible-role");
    }
  else
    {
      char *role_str = g_enum_to_string (GTK_TYPE_ACCESSIBLE_ROLE, self->accessible_role);

      g_critical ("%s already has an accessible role of type “%s”",
                  G_OBJECT_TYPE_NAME (self),
                  role_str);

      g_free (role_str);
    }
}

static gboolean
gtk_at_spi_socket_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  GtkAtSpiSocket *self = (GtkAtSpiSocket *) initable;

  g_assert (GTK_IS_AT_SPI_SOCKET (initable));

  if (self->bus_name == NULL || !g_dbus_is_name (self->bus_name))
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Invalid bus name");
      return FALSE;
    }

  if (self->object_path == NULL || !g_variant_is_object_path (self->object_path))
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Invalid object path");
      return FALSE;
    }

  self->at_context = gtk_at_context_create (self->accessible_role,
                                            GTK_ACCESSIBLE (self),
                                            gdk_display_get_default ());

  /* Sockets are strictly specific to AT-SPI */
  if (self->at_context != NULL && !GTK_IS_AT_SPI_CONTEXT (self->at_context))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "AT-SPI sockets can only be used with the AT-SPI backend");
      g_clear_object (&self->at_context);
      return FALSE;
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                               -1);

  return TRUE;
}

static void
g_initable_interface_init (GInitableIface *iface)
{
  iface->init = gtk_at_spi_socket_initable_init;
}

static GtkATContext *
gtk_at_spi_socket_get_at_context (GtkAccessible *accessible)
{
  GtkAtSpiSocket *self = (GtkAtSpiSocket *) accessible;

  g_assert (GTK_IS_AT_SPI_SOCKET (accessible));

  if (self->at_context != NULL)
    return g_object_ref (self->at_context);

  return NULL;
}

static gboolean
gtk_at_spi_socket_get_platform_state (GtkAccessible              *self,
                                      GtkAccessiblePlatformState  state)
{
  return FALSE;
}

static GtkAccessible *
gtk_at_spi_socket_get_first_accessible_child (GtkAccessible *accessible)
{
  return NULL;
}

static GtkAccessible *
gtk_at_spi_socket_get_next_accessible_sibling (GtkAccessible *accessible)
{
  return NULL;
}

static gboolean
gtk_at_spi_socket_get_bounds (GtkAccessible *accessible,
                              int           *x,
                              int           *y,
                              int           *width,
                              int           *height)
{
  GtkAccessible *accessible_parent;
  gboolean res = FALSE;

  g_assert (GTK_IS_AT_SPI_SOCKET (accessible));

  accessible_parent = gtk_accessible_get_accessible_parent (accessible);
  if (accessible_parent == NULL)
    return FALSE;

  res = gtk_accessible_get_bounds (accessible_parent, x, y, width, height);

  g_object_unref (accessible_parent);

  return res;
}

static void
gtk_accessible_interface_init (GtkAccessibleInterface *iface)
{
  iface->get_at_context = gtk_at_spi_socket_get_at_context;
  iface->get_platform_state = gtk_at_spi_socket_get_platform_state;
  iface->get_first_accessible_child = gtk_at_spi_socket_get_first_accessible_child;
  iface->get_next_accessible_sibling = gtk_at_spi_socket_get_next_accessible_sibling;
  iface->get_bounds = gtk_at_spi_socket_get_bounds;
}

static void
gtk_at_spi_socket_dispose (GObject *object)
{
  GtkAtSpiSocket *self = (GtkAtSpiSocket *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_pointer (&self->object_path, g_free);
  g_clear_pointer (&self->bus_name, g_free);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->at_context);

  G_OBJECT_CLASS (gtk_at_spi_socket_parent_class)->dispose (object);
}

static void
gtk_at_spi_socket_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkAtSpiSocket *self = GTK_AT_SPI_SOCKET (object);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, self->accessible_role);
      break;

    case PROP_BUS_NAME:
      g_value_set_string (value, self->bus_name);
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_at_spi_socket_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkAtSpiSocket *self = GTK_AT_SPI_SOCKET (object);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      set_accessible_role (self, g_value_get_enum (value));
      break;

    case PROP_BUS_NAME:
      self->bus_name = g_value_dup_string (value);
      break;

    case PROP_OBJECT_PATH:
      self->object_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_at_spi_socket_class_init (GtkAtSpiSocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_at_spi_socket_dispose;
  object_class->get_property = gtk_at_spi_socket_get_property;
  object_class->set_property = gtk_at_spi_socket_set_property;

  /**
   * GtkAtSpiSocket:bus-name: (attributes org.gtk.Property.get=gtk_at_spi_socket_get_bus_name)
   *
   * The bus name of the remote accessible client.
   *
   * Since: 4.14
   */
  properties[PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAtSpiSocket:height: (attributes org.gtk.Property.get=gtk_at_spi_socket_get_object_path)
   *
   * The object path of the remote accessible object that
   * the socket connects to.
   *
   * Since: 4.14
   */
  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_object_class_override_property (object_class, PROP_ACCESSIBLE_ROLE, "accessible-role");
}

static void
gtk_at_spi_socket_init (GtkAtSpiSocket *self)
{
  self->accessible_role = GTK_ACCESSIBLE_ROLE_GENERIC;
}

GVariant *
gtk_at_spi_socket_to_ref (GtkAtSpiSocket *self)
{
  return g_variant_new ("(so)", self->bus_name, self->object_path);
}

static void
update_embedded_state (GtkAtSpiSocket *self,
                       gboolean        embedded)
{
  if (self->embedded == embedded)
    return;

  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_HIDDEN, !embedded,
                               -1);

  self->embedded = embedded;

  g_clear_object (&self->cancellable);
}

static void
socket_embedded_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GtkAtSpiSocket *self = (GtkAtSpiSocket *)user_data;
  GVariant *res = NULL;
  GError *error = NULL;

  res = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                       result,
                                       &error);

  g_clear_pointer (&res, g_variant_unref);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error embedding AT-SPI socket: %s", error->message);
          update_embedded_state (self, FALSE);
        }
      g_clear_error (&error);
      return;
    }

  update_embedded_state (self, TRUE);
}

void
gtk_at_spi_socket_embed (GtkAtSpiSocket  *self,
                         GDBusConnection *connection)
{
  const char *context_path = NULL;

  if (self->at_context == NULL)
    return;

  if (self->embedded || self->cancellable != NULL)
    return;

  context_path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (self->at_context));

  self->cancellable = g_cancellable_new ();

  g_dbus_connection_call (connection,
                          self->bus_name,
                          self->object_path,
                          "org.a11y.atspi.Socket",
                          "Embedded",
                          g_variant_new ("(s)", context_path),
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          self->cancellable,
                          socket_embedded_cb,
                          self);
}

/**
 * gtk_at_spi_socket_new:
 * @bus_name: the bus name of the remote accessible object
 * @object_path: the object path of the remote accessible object
 * @error: (nullable): return location for a #GError
 *
 * Creates an AT-SPI socket object that makes the accessible tree
 * at the given @bus_name and @object_path appear as part of the
 * accessible tree that it belongs to itself.
 *
 * It is up to the app to acquire @bus_name and @object_path. That's
 * usually done through a side channel with the remote side, for
 * example using sockets, or reading the output of a subprocess.
 *
 * The remote accessible object at @object_path must support
 * the `org.a11y.atspi.Socket` interface with the `Embedded()`
 * method.
 *
 * This constructor can fail, most notably if the accessibility
 * stack is not AT-SPI.
 *
 * Since: 4.14
 */
GtkAccessible *
gtk_at_spi_socket_new (const char  *bus_name,
                       const char  *object_path,
                       GError     **error)
{
  return g_initable_new (GTK_TYPE_AT_SPI_SOCKET,
                         NULL,
                         error,
                         "bus-name", bus_name,
                         "object-path", object_path,
                         NULL);
}

/**
 * gtk_at_spi_socket_get_bus_name:
 * @self: a #GtkAtSpiSocket
 *
 * Retrieves the bus name of the remote accessible object that the
 * socket is connected to.
 *
 * Returns: (transfer none): the bus name of the remote accessible object
 *
 * Since: 4.14
 */
const char *
gtk_at_spi_socket_get_bus_name (GtkAtSpiSocket *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_SOCKET (self), NULL);

  return self->bus_name;
}

/**
 * gtk_at_spi_socket_get_object_path:
 * @self: a #GtkAtSpiSocket
 *
 * Retrieves the object path of the remote accessible object that
 * the socket is connected to.
 *
 * Returns: (transfer none): the object path of the socket remote
 * accessible object
 *
 * Since: 4.14
 */
const char *
gtk_at_spi_socket_get_object_path (GtkAtSpiSocket *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_SOCKET (self), NULL);

  return self->object_path;
}
