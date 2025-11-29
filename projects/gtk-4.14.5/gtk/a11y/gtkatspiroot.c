/* gtkatspiroot.c: AT-SPI root object
 *
 * Copyright 2020  GNOME Foundation
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

#include "gtkatspirootprivate.h"

#include "gtkatspicacheprivate.h"
#include "gtkatspicontextprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkatspiprivate.h"
#include "gtkatspiutilsprivate.h"

#include "gtkdebug.h"
#include "gtkwindow.h"
#include "gtkprivate.h"

#include "a11y/atspi/atspi-accessible.h"
#include "a11y/atspi/atspi-application.h"

#include <locale.h>

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#define ATSPI_VERSION           "2.1"

#define ATSPI_PATH_PREFIX       "/org/a11y/atspi"
#define ATSPI_ROOT_PATH         ATSPI_PATH_PREFIX "/accessible/root"
#define ATSPI_CACHE_PATH        ATSPI_PATH_PREFIX "/cache"

struct _GtkAtSpiRoot
{
  GObject parent_instance;

  char *bus_address;
  GDBusConnection *connection;

  char *base_path;

  const char *root_path;

  const char *toolkit_name;
  const char *version;
  const char *atspi_version;

  char *desktop_name;
  char *desktop_path;

  gint32 application_id;
  guint register_id;

  GList *queued_contexts;
  GtkAtSpiCache *cache;

  GListModel *toplevels;
};

enum
{
  PROP_BUS_ADDRESS = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiRoot, gtk_at_spi_root, G_TYPE_OBJECT)

static void
gtk_at_spi_root_finalize (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  g_clear_handle_id (&self->register_id, g_source_remove);

  g_free (self->bus_address);
  g_free (self->base_path);
  g_free (self->desktop_name);
  g_free (self->desktop_path);

  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->finalize (gobject);
}

static void
gtk_at_spi_root_dispose (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  g_clear_object (&self->cache);
  g_clear_object (&self->connection);
  g_clear_pointer (&self->queued_contexts, g_list_free);

  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->dispose (gobject);
}

static void
gtk_at_spi_root_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      self->bus_address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_root_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      g_value_set_string (value, self->bus_address);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
handle_application_method (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           const gchar           *interface_name,
                           const gchar           *method_name,
                           GVariant              *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
  if (g_strcmp0 (method_name, "GetLocale") == 0)
    {
      guint lctype;
      const char *locale;

      int types[] = {
        LC_MESSAGES, LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC, LC_TIME
      };

      g_variant_get (parameters, "(u)", &lctype);
      if (lctype >= G_N_ELEMENTS (types))
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Not a known locale facet: %u", lctype);
          return;
        }

      locale = setlocale (types[lctype], NULL);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", locale));
    }
}

static GVariant *
handle_application_get_property (GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *property_name,
                                 GError               **error,
                                 gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;
  GVariant *res = NULL;

  if (g_strcmp0 (property_name, "Id") == 0)
    res = g_variant_new_int32 (self->application_id);
  else if (g_strcmp0 (property_name, "ToolkitName") == 0)
    res = g_variant_new_string (self->toolkit_name);
  else if (g_strcmp0 (property_name, "Version") == 0)
    res = g_variant_new_string (self->version);
  else if (g_strcmp0 (property_name, "AtspiVersion") == 0)
    res = g_variant_new_string (self->atspi_version);
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static gboolean
handle_application_set_property (GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *property_name,
                                 GVariant              *value,
                                 GError               **error,
                                 gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;

  if (g_strcmp0 (property_name, "Id") == 0)
    {
      g_variant_get (value, "i", &(self->application_id));
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Invalid property '%s'", property_name);
      return FALSE;
    }

  return TRUE;
}

static void
handle_accessible_method (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;

  if (g_strcmp0 (method_name, "GetRole") == 0)
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", ATSPI_ROLE_APPLICATION));
  else if (g_strcmp0 (method_name, "GetRoleName") == 0)
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", "application"));
  else if (g_strcmp0 (method_name, "GetLocalizedRoleName") == 0)
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", C_("accessibility", "application")));
  else if (g_strcmp0 (method_name, "GetState") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(au)"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("au"));
      g_variant_builder_add (&builder, "u", 0);
      g_variant_builder_add (&builder, "u", 0);
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
     }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(a{ss})"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
      g_variant_builder_add (&builder, "{ss}", "toolkit", "GTK");
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetApplication") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("((so))",
                                                            self->desktop_name,
                                                            self->desktop_path));
    }
  else if (g_strcmp0 (method_name, "GetChildAtIndex") == 0)
    {
      int idx, real_idx = 0;

      g_variant_get (parameters, "(i)", &idx);

      GtkWidget *window = NULL;
      guint n_toplevels = g_list_model_get_n_items (self->toplevels);
      for (guint i = 0; i < n_toplevels; i++)
        {
          window = g_list_model_get_item (self->toplevels, i);

          g_object_unref (window);

          if (!gtk_widget_get_visible (window))
            continue;

          if (real_idx == idx)
            break;

          real_idx += 1;
        }

      if (window == NULL)
        return;

      GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (window));

      const char *name = g_dbus_connection_get_unique_name (self->connection);
      const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("((so))", name, path));

      g_object_unref (context);
    }
  else if (g_strcmp0 (method_name, "GetChildren") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      guint n_toplevels = g_list_model_get_n_items (self->toplevels);
      for (guint i = 0; i < n_toplevels; i++)
        {
          GtkWidget *window = g_list_model_get_item (self->toplevels, i);

          g_object_unref (window);

          if (!gtk_widget_get_visible (window))
            continue;

          GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (window));
          const char *name = g_dbus_connection_get_unique_name (self->connection);
          const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

          g_variant_builder_add (&builder, "(so)", name, path);

          g_object_unref (context);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(so))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetIndexInParent") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", -1));
    }
  else if (g_strcmp0 (method_name, "GetRelationSet") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(ua(so))"));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(ua(so)))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetInterfaces") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("as"));

      g_variant_builder_add (&builder, "s", atspi_accessible_interface.name);
      g_variant_builder_add (&builder, "s", atspi_application_interface.name);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", &builder));
    }
}

static GVariant *
handle_accessible_get_property (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *property_name,
                                GError               **error,
                                gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;
  GVariant *res = NULL;

  if (g_strcmp0 (property_name, "Name") == 0)
    res = g_variant_new_string (g_get_prgname () ? g_get_prgname () : "Unnamed");
  else if (g_strcmp0 (property_name, "Description") == 0)
    res = g_variant_new_string (g_get_application_name () ? g_get_application_name () : "No description");
  else if (g_strcmp0 (property_name, "Locale") == 0)
    res = g_variant_new_string (setlocale (LC_MESSAGES, NULL));
  else if (g_strcmp0 (property_name, "AccessibleId") == 0)
    res = g_variant_new_string ("");
  else if (g_strcmp0 (property_name, "Parent") == 0)
    res = g_variant_new ("(so)", self->desktop_name, self->desktop_path);
  else if (g_strcmp0 (property_name, "ChildCount") == 0)
    {
      guint n_toplevels = g_list_model_get_n_items (self->toplevels);
      int n_children = 0;

      for (guint i = 0; i < n_toplevels; i++)
        {
          GtkWidget *window = g_list_model_get_item (self->toplevels, i);

          if (gtk_widget_get_visible (window))
            n_children += 1;

          g_object_unref (window);
        }

      res = g_variant_new_int32 (n_children);
    }
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable root_application_vtable = {
  handle_application_method,
  handle_application_get_property,
  handle_application_set_property,
};

static const GDBusInterfaceVTable root_accessible_vtable = {
  handle_accessible_method,
  handle_accessible_get_property,
  NULL,
};

void
gtk_at_spi_root_child_changed (GtkAtSpiRoot             *self,
                               GtkAccessibleChildChange  change,
                               GtkAccessible            *child)
{
  guint n, i;
  int idx = 0;
  GVariant *window_ref;
  GtkAccessibleChildState state;

  if (!self->toplevels)
    return;

  for (i = 0, n = g_list_model_get_n_items (self->toplevels); i < n; i++)
    {
      GtkAccessible *item = g_list_model_get_item (self->toplevels, i);

      g_object_unref (item);

      if (item == child)
        break;

      if (!gtk_accessible_should_present (item))
        continue;

      idx++;
    }

  if (child == NULL)
    {
      window_ref = gtk_at_spi_null_ref ();
    }
  else
    {
      GtkATContext *context = gtk_accessible_get_at_context (child);

      window_ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (context));

      g_object_unref (context);
    }

  switch (change)
    {
    case GTK_ACCESSIBLE_CHILD_CHANGE_ADDED:
      state = GTK_ACCESSIBLE_CHILD_STATE_ADDED;
      break;
    case GTK_ACCESSIBLE_CHILD_CHANGE_REMOVED:
      state = GTK_ACCESSIBLE_CHILD_STATE_REMOVED;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_at_spi_emit_children_changed (self->connection,
                                    self->root_path,
                                    state,
                                    idx,
                                    gtk_at_spi_root_to_ref (self),
                                    window_ref);
}

typedef struct {
  GtkAtSpiRoot *root;
  GtkAtSpiRootRegisterFunc register_func;
} RegistrationData;

static void
on_registration_reply (GObject      *gobject,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  RegistrationData *data = user_data;
  GtkAtSpiRoot *self = data->root;

  GError *error = NULL;
  GVariant *reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (gobject), result, &error);

  self->register_id = 0;

  if (error != NULL)
    {
      g_critical ("Unable to register the application: %s", error->message);
      g_error_free (error);
      return;
    }

  if (reply != NULL)
    {
      g_variant_get (reply, "((so))",
                     &self->desktop_name,
                     &self->desktop_path);
      g_variant_unref (reply);

      GTK_DEBUG (A11Y, "Connected to the a11y registry at (%s, %s)",
                       self->desktop_name,
                       self->desktop_path);
    }

  /* Register the cache object */
  self->cache = gtk_at_spi_cache_new (self->connection, ATSPI_CACHE_PATH, self);

  /* Drain the list of queued GtkAtSpiContexts, and add them to the cache */
  if (self->queued_contexts != NULL)
    {
      self->queued_contexts = g_list_reverse (self->queued_contexts);
      for (GList *l = self->queued_contexts; l != NULL; l = l->next)
        {
          if (data->register_func != NULL)
            data->register_func (self, l->data);

          gtk_at_spi_cache_add_context (self->cache, l->data);
        }

      g_clear_pointer (&self->queued_contexts, g_list_free);
    }

  self->toplevels = gtk_window_get_toplevels ();

  g_free (data);
}

static gboolean
root_register (gpointer user_data)
{
  RegistrationData *data = user_data;
  GtkAtSpiRoot *self = data->root;
  const char *unique_name;

  /* Register the root element; every application has a single root, so we only
   * need to do this once.
   *
   * The root element is used to advertise our existence on the accessibility
   * bus, and it's the entry point to the accessible objects tree.
   *
   * The announcement is split into two phases:
   *
   *  1. we register the org.a11y.atspi.Application and org.a11y.atspi.Accessible
   *     interfaces at the well-known object path
   *  2. we invoke the org.a11y.atspi.Socket.Embed method with the connection's
   *     unique name and the object path
   *  3. the ATSPI registry daemon will set the org.a11y.atspi.Application.Id
   *     property on the given object path
   *  4. the registration concludes when the Embed method returns us the desktop
   *     name and object path
   */
  self->toolkit_name = "GTK";
  self->version = PACKAGE_VERSION;
  self->atspi_version = ATSPI_VERSION;
  self->root_path = ATSPI_ROOT_PATH;

  unique_name = g_dbus_connection_get_unique_name (self->connection);

  g_dbus_connection_register_object (self->connection,
                                     self->root_path,
                                     (GDBusInterfaceInfo *) &atspi_application_interface,
                                     &root_application_vtable,
                                     self,
                                     NULL,
                                     NULL);
  g_dbus_connection_register_object (self->connection,
                                     self->root_path,
                                     (GDBusInterfaceInfo *) &atspi_accessible_interface,
                                     &root_accessible_vtable,
                                     self,
                                     NULL,
                                     NULL);

  GTK_DEBUG (A11Y, "Registering (%s, %s) on the a11y bus",
                   unique_name,
                   self->root_path);

  g_dbus_connection_call (self->connection,
                          "org.a11y.atspi.Registry",
                          ATSPI_ROOT_PATH,
                          "org.a11y.atspi.Socket",
                          "Embed",
                          g_variant_new ("((so))", unique_name, self->root_path),
                          G_VARIANT_TYPE ("((so))"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL,
                          on_registration_reply,
                          data);

  return G_SOURCE_REMOVE;
}

/*< private >
 * gtk_at_spi_root_queue_register:
 * @self: a `GtkAtSpiRoot`
 * @context: the AtSpi context to register
 * @func: the function to call when the root has been registered
 *
 * Queues the registration of the root object on the AT-SPI bus.
 */
void
gtk_at_spi_root_queue_register (GtkAtSpiRoot             *self,
                                GtkAtSpiContext          *context,
                                GtkAtSpiRootRegisterFunc  func)
{
  /* The cache is available if the root has finished registering itself; if we
   * are still waiting for the registration to finish, add the context to a queue
   */
  if (self->cache != NULL)
    {
      if (func != NULL)
        func (self, context);

      gtk_at_spi_cache_add_context (self->cache, context);
      return;
    }
  else
    {
      if (g_list_find (self->queued_contexts, context) == NULL)
        self->queued_contexts = g_list_prepend (self->queued_contexts, context);
    }

  /* Ignore multiple registration requests while one is already in flight */
  if (self->register_id != 0)
    return;

  RegistrationData *data = g_new (RegistrationData, 1);
  data->root = self;
  data->register_func = func;

  self->register_id = g_idle_add (root_register, data);
  gdk_source_set_static_name_by_id (self->register_id, "[gtk] ATSPI root registration");
}

void
gtk_at_spi_root_unregister (GtkAtSpiRoot    *self,
                            GtkAtSpiContext *context)
{
  if (self->queued_contexts != NULL)
    self->queued_contexts = g_list_remove (self->queued_contexts, context);

  if (self->cache != NULL)
    gtk_at_spi_cache_remove_context (self->cache, context);
}

static void
gtk_at_spi_root_constructed (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  GError *error = NULL;

  /* The accessibility bus is a fully managed bus */
  self->connection =
    g_dbus_connection_new_for_address_sync (self->bus_address,
                                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                            NULL, NULL,
                                            &error);

  if (error != NULL)
    {
      g_critical ("Unable to connect to the accessibility bus at '%s': %s",
                  self->bus_address,
                  error->message);
      g_error_free (error);
      goto out;
    }

  /* We use the application's object path to build the path of each
   * accessible object exposed on the accessibility bus; the path is
   * also used to access the object cache
   */
  GApplication *application = g_application_get_default ();

  if (application != NULL && g_application_get_is_registered (application))
    {
      const char *app_path = g_application_get_dbus_object_path (application);

      /* No need to validate the path */
      self->base_path = g_strconcat (app_path, "/a11y", NULL);
    }

  if (self->base_path == NULL)
    {
      const char *program_name = g_get_prgname ();

      char *base_name = NULL;
      if (program_name == NULL || *program_name == 0)
        base_name = g_strdup ("unknown");
      else if (*program_name == '/')
        base_name = g_path_get_basename (program_name);
      else
        base_name = g_strdup (program_name);

      self->base_path = g_strconcat ("/org/gtk/application/",
                                     base_name,
                                     "/a11y",
                                     NULL);

      g_free (base_name);

      /* Turn potentially invalid program names into something that can be
       * used as a DBus path
       */
      size_t len = strlen (self->base_path);
      for (size_t i = 0; i < len; i++)
        {
          char c = self->base_path[i];

          if (c == '/')
            continue;

          if ((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c == '_'))
            continue;

          self->base_path[i] = '_';
        }
    }

out:
  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_root_class_init (GtkAtSpiRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_root_constructed;
  gobject_class->set_property = gtk_at_spi_root_set_property;
  gobject_class->get_property = gtk_at_spi_root_get_property;
  gobject_class->dispose = gtk_at_spi_root_dispose;
  gobject_class->finalize = gtk_at_spi_root_finalize;

  obj_props[PROP_BUS_ADDRESS] =
    g_param_spec_string ("bus-address", NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_root_init (GtkAtSpiRoot *self)
{
}

GtkAtSpiRoot *
gtk_at_spi_root_new (const char *bus_address)
{
  g_return_val_if_fail (bus_address != NULL, NULL);

  return g_object_new (GTK_TYPE_AT_SPI_ROOT,
                       "bus-address", bus_address,
                       NULL);
}

GDBusConnection *
gtk_at_spi_root_get_connection (GtkAtSpiRoot *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_ROOT (self), NULL);

  return self->connection;
}

GtkAtSpiCache *
gtk_at_spi_root_get_cache (GtkAtSpiRoot *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_ROOT (self), NULL);

  return self->cache;
}

/*< private >
 * gtk_at_spi_root_to_ref:
 * @self: a `GtkAtSpiRoot`
 *
 * Returns an ATSPI object reference for the `GtkAtSpiRoot` node.
 *
 * Returns: (transfer floating): a `GVariant` with the root reference
 */
GVariant *
gtk_at_spi_root_to_ref (GtkAtSpiRoot *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_ROOT (self), NULL);

  return g_variant_new ("(so)",
                        g_dbus_connection_get_unique_name (self->connection),
                        self->root_path);
}

const char *
gtk_at_spi_root_get_base_path (GtkAtSpiRoot *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_ROOT (self), NULL);

  return self->base_path;
}
