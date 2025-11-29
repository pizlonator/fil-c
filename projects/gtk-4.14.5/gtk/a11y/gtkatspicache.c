/* gtkatspicache.c: AT-SPI object cache
 *
 * Copyright 2020  holder
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

#include "gtkatspicacheprivate.h"

#include "gtkatspicontextprivate.h"
#include "gtkatspirootprivate.h"
#include "gtkatspiutilsprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"

#include "a11y/atspi/atspi-accessible.h"
#include "a11y/atspi/atspi-application.h"
#include "a11y/atspi/atspi-cache.h"

/* Cached item:
 *
 *  (so): object ref
 *  (so): application ref
 *  (so): parent ref
 *    - parent.role == application ? desktop ref : null ref
 *  i: index in parent, or -1 for transient widgets/menu items
 *  i: child count, or -1 for defunct/menus
 *  as: interfaces
 *  s: name
 *  u: role
 *  s: description
 *  au: state set
 */
#define ITEM_SIGNATURE          "(so)(so)(so)iiassusau"
#define GET_ITEMS_SIGNATURE     "a(" ITEM_SIGNATURE ")"

struct _GtkAtSpiCache
{
  GObject parent_instance;

  char *cache_path;
  GDBusConnection *connection;

  /* HashTable<str, GtkAtSpiContext> */
  GHashTable *contexts_by_path;

  /* HashTable<GtkAtSpiContext, str> */
  GHashTable *contexts_to_path;

  /* Re-entrancy guard */
  gboolean in_get_items;

  GtkAtSpiRoot *root;
};

enum
{
  PROP_CACHE_PATH = 1,
  PROP_CONNECTION,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiCache, gtk_at_spi_cache, G_TYPE_OBJECT)

static void
gtk_at_spi_cache_finalize (GObject *gobject)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  g_clear_pointer (&self->contexts_to_path, g_hash_table_unref);
  g_clear_pointer (&self->contexts_by_path, g_hash_table_unref);
  g_clear_object (&self->connection);
  g_free (self->cache_path);

  G_OBJECT_CLASS (gtk_at_spi_cache_parent_class)->finalize (gobject);
}

static void
gtk_at_spi_cache_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  switch (prop_id)
    {
    case PROP_CACHE_PATH:
      g_free (self->cache_path);
      self->cache_path = g_value_dup_string (value);
      break;

    case PROP_CONNECTION:
      g_clear_object (&self->connection);
      self->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
collect_object (GtkAtSpiCache   *self,
                GVariantBuilder *builder,
                GtkAtSpiContext *context)
{
  g_variant_builder_add (builder, "@(so)", gtk_at_spi_context_to_ref (context));

  GtkAtSpiRoot *root = gtk_at_spi_context_get_root (context);
  g_variant_builder_add (builder, "@(so)", gtk_at_spi_root_to_ref (root));

  g_variant_builder_add (builder, "@(so)", gtk_at_spi_context_get_parent_ref (context));

  g_variant_builder_add (builder, "i", gtk_at_spi_context_get_index_in_parent (context));
  g_variant_builder_add (builder, "i", gtk_at_spi_context_get_child_count (context));

  g_variant_builder_add (builder, "@as", gtk_at_spi_context_get_interfaces (context));

  char *name = gtk_at_context_get_name (GTK_AT_CONTEXT (context));
  g_variant_builder_add (builder, "s", name ? name : "");
  g_free (name);

  guint atspi_role = gtk_atspi_role_for_context (GTK_AT_CONTEXT (context));
  g_variant_builder_add (builder, "u", atspi_role);

  char *description = gtk_at_context_get_description (GTK_AT_CONTEXT (context));
  g_variant_builder_add (builder, "s", description ? description : "");
  g_free (description);

  g_variant_builder_add (builder, "@au", gtk_at_spi_context_get_states (context));
}

static void
collect_root (GtkAtSpiCache   *self,
              GVariantBuilder *builder)
{
  g_variant_builder_add (builder, "@(so)", gtk_at_spi_root_to_ref (self->root));
  g_variant_builder_add (builder, "@(so)", gtk_at_spi_root_to_ref (self->root));

  g_variant_builder_add (builder, "@(so)", gtk_at_spi_null_ref ());

  g_variant_builder_add (builder, "i", -1);
  g_variant_builder_add (builder, "i", 0);

  GVariantBuilder interfaces = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("as"));

  g_variant_builder_add (&interfaces, "s", atspi_accessible_interface.name);
  g_variant_builder_add (&interfaces, "s", atspi_application_interface.name);
  g_variant_builder_add (builder, "@as", g_variant_builder_end (&interfaces));

  g_variant_builder_add (builder, "s", g_get_prgname () ? g_get_prgname () : "Unnamed");

  g_variant_builder_add (builder, "u", ATSPI_ROLE_APPLICATION);

  g_variant_builder_add (builder, "s", g_get_application_name () ? g_get_application_name () : "No description");

  GVariantBuilder states = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("au"));
  g_variant_builder_add (&states, "u", 0);
  g_variant_builder_add (&states, "u", 0);
  g_variant_builder_add (builder, "@au", g_variant_builder_end (&states));
}

static void
collect_cached_objects (GtkAtSpiCache   *self,
                        GVariantBuilder *builder)
{
  GHashTable *collection = g_hash_table_new (NULL, NULL);
  GHashTableIter iter;
  gpointer key_p, value_p;

  /* Serializing the contexts might re-enter, and end up modifying the hash
   * table, so we take a snapshot here and return the items we have at the
   * moment of the GetItems() call
   */
  g_hash_table_iter_init (&iter, self->contexts_by_path);
  while (g_hash_table_iter_next (&iter, &key_p, &value_p))
    g_hash_table_add (collection, value_p);

  g_variant_builder_open (builder, G_VARIANT_TYPE ("(" ITEM_SIGNATURE ")"));
  collect_root (self, builder);
  g_variant_builder_close (builder);

  g_hash_table_iter_init (&iter, collection);
  while (g_hash_table_iter_next (&iter, &key_p, &value_p))
    {
      g_variant_builder_open (builder, G_VARIANT_TYPE ("(" ITEM_SIGNATURE ")"));

      GtkAtSpiContext *context = value_p;

      collect_object (self, builder, context);

      g_variant_builder_close (builder);
    }

  g_hash_table_unref (collection);
}

static void
emit_add_accessible (GtkAtSpiCache   *self,
                     GtkAtSpiContext *context)
{
  GtkATContext *at_context = GTK_AT_CONTEXT (context);

  /* If the context is hidden, we don't need to update the cache */
  if (gtk_at_context_has_accessible_state (at_context, GTK_ACCESSIBLE_STATE_HIDDEN))
    {
      GtkAccessibleValue *is_hidden =
        gtk_at_context_get_accessible_state (at_context, GTK_ACCESSIBLE_STATE_HIDDEN);

      if (gtk_boolean_accessible_value_get (is_hidden))
        return;
    }

  GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(" ITEM_SIGNATURE ")"));

  collect_object (self, &builder, context);

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->cache_path,
                                 "org.a11y.atspi.Cache",
                                 "AddAccessible",
                                 g_variant_new ("(@(" ITEM_SIGNATURE "))",
                                   g_variant_builder_end (&builder)),
                                 NULL);
}

static void
emit_remove_accessible (GtkAtSpiCache   *self,
                        GtkAtSpiContext *context)
{
  GtkATContext *at_context = GTK_AT_CONTEXT (context);

  /* If the context is hidden, we don't need to update the cache */
  if (gtk_at_context_has_accessible_state (at_context, GTK_ACCESSIBLE_STATE_HIDDEN))
    {
      GtkAccessibleValue *is_hidden =
        gtk_at_context_get_accessible_state (at_context, GTK_ACCESSIBLE_STATE_HIDDEN);

      if (gtk_boolean_accessible_value_get (is_hidden))
        return;
    }

  GVariant *ref = gtk_at_spi_context_to_ref (context);

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->cache_path,
                                 "org.a11y.atspi.Cache",
                                 "RemoveAccessible",
                                 g_variant_new ("(@(so))", ref),
                                 NULL);
}

static void
handle_cache_method (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  GtkAtSpiCache *self = user_data;

  GTK_DEBUG (A11Y, "[Cache] Method '%s' on interface '%s' for object '%s' from '%s'",
                   method_name, interface_name, object_path, sender);


  if (g_strcmp0 (method_name, "GetItems") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(" GET_ITEMS_SIGNATURE ")"));
      GVariant *items;

      /* Prevent the emission os signals while collecting accessible
       * objects as the result of walking the accessible tree
       */
      self->in_get_items = TRUE;

      g_variant_builder_open (&builder, G_VARIANT_TYPE (GET_ITEMS_SIGNATURE));
      collect_cached_objects (self, &builder);
      g_variant_builder_close (&builder);
      items = g_variant_builder_end (&builder);

      self->in_get_items = FALSE;

      GTK_DEBUG (A11Y, "Returning %lu items", g_variant_n_children (items));

      g_dbus_method_invocation_return_value (invocation, items);
    }
}

static GVariant *
handle_cache_get_property (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           const gchar           *interface_name,
                           const gchar           *property_name,
                           GError               **error,
                           gpointer               user_data)
{
  GVariant *res = NULL;

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Unknown property '%s'", property_name);

  return res;
}


static const GDBusInterfaceVTable cache_vtable = {
  handle_cache_method,
  handle_cache_get_property,
  NULL,
};

static void
gtk_at_spi_cache_constructed (GObject *gobject)
{
  GtkAtSpiCache *self = GTK_AT_SPI_CACHE (gobject);

  g_assert (self->connection);
  g_assert (self->cache_path);

  g_dbus_connection_register_object (self->connection,
                                     self->cache_path,
                                     (GDBusInterfaceInfo *) &atspi_cache_interface,
                                     &cache_vtable,
                                     self,
                                     NULL,
                                     NULL);

  GTK_DEBUG (A11Y, "Cache registered at %s", self->cache_path);

  G_OBJECT_CLASS (gtk_at_spi_cache_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_cache_class_init (GtkAtSpiCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_cache_constructed;
  gobject_class->set_property = gtk_at_spi_cache_set_property;
  gobject_class->finalize = gtk_at_spi_cache_finalize;

  obj_props[PROP_CACHE_PATH] =
    g_param_spec_string ("cache-path", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  obj_props[PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         G_TYPE_DBUS_CONNECTION,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_cache_init (GtkAtSpiCache *self)
{
  self->contexts_by_path = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free,
                                                  NULL);
  self->contexts_to_path = g_hash_table_new (NULL, NULL);
}

GtkAtSpiCache *
gtk_at_spi_cache_new (GDBusConnection *connection,
                      const char      *cache_path,
                      GtkAtSpiRoot    *root)
{
  GtkAtSpiCache *cache;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (cache_path != NULL, NULL);

  cache = g_object_new (GTK_TYPE_AT_SPI_CACHE,
                        "connection", connection,
                        "cache-path", cache_path,
                        NULL);
  cache->root = root;

  return cache;
}

void
gtk_at_spi_cache_add_context (GtkAtSpiCache   *self,
                              GtkAtSpiContext *context)
{
  g_return_if_fail (GTK_IS_AT_SPI_CACHE (self));
  g_return_if_fail (GTK_IS_AT_SPI_CONTEXT (context));

  const char *path = gtk_at_spi_context_get_context_path (context);
  if (path == NULL)
    return;

  if (g_hash_table_contains (self->contexts_by_path, path))
    return;

  char *path_key = g_strdup (path);
  g_hash_table_insert (self->contexts_by_path, path_key, context);
  g_hash_table_insert (self->contexts_to_path, context, path_key);

  GTK_DEBUG (A11Y, "Adding context '%s' to cache", path_key);

  /* GetItems is safe from re-entrancy, but we still don't want to
   * emit an unnecessary signal while we're collecting ATContexts
   */
  if (!self->in_get_items)
    emit_add_accessible (self, context);
}

void
gtk_at_spi_cache_remove_context (GtkAtSpiCache   *self,
                                 GtkAtSpiContext *context)
{
  g_return_if_fail (GTK_IS_AT_SPI_CACHE (self));
  g_return_if_fail (GTK_IS_AT_SPI_CONTEXT (context));

  const char *path = gtk_at_spi_context_get_context_path (context);
  if (path == NULL)
    return;

  if (!g_hash_table_contains (self->contexts_by_path, path))
    return;

  emit_remove_accessible (self, context);

  /* The order is important: the value in contexts_by_path is the
   * key in contexts_to_path
   */
  g_hash_table_remove (self->contexts_to_path, context);
  g_hash_table_remove (self->contexts_by_path, path);

  GTK_DEBUG (A11Y, "Removing context '%s' from cache", path);
}
