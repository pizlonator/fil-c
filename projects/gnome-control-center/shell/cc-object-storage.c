/* cc-object-storage.h
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "cc-object-storage"

#include "cc-object-storage.h"

struct _CcObjectStorage
{
  GObject     parent_instance;

  GHashTable *id_to_object;
};

G_DEFINE_TYPE (CcObjectStorage, cc_object_storage, G_TYPE_OBJECT)

/* Singleton instance */
static CcObjectStorage *_instance = NULL;

/* GTask API to create a new D-Bus proxy */
typedef struct
{
  GBusType         bus_type;
  GDBusProxyFlags  flags;
  gchar           *name;
  gchar           *path;
  gchar           *interface;
  gboolean         cached;
} TaskData;

static TaskData*
task_data_new (GBusType         bus_type,
               GDBusProxyFlags  flags,
               const gchar     *name,
               const gchar     *path,
               const gchar     *interface)
{
  TaskData *data = g_slice_new (TaskData);
  data->bus_type = bus_type;
  data->flags =flags;
  data->name = g_strdup (name);
  data->path = g_strdup (path);
  data->interface = g_strdup (interface);
  data->cached = FALSE;

  return data;
}

static void
task_data_free (TaskData *data)
{
  g_free (data->name);
  g_free (data->path);
  g_free (data->interface);
  g_slice_free (TaskData, data);
}

static void
create_dbus_proxy_in_thread_cb (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) local_error = NULL;
  TaskData *data = task_data;

  proxy = g_dbus_proxy_new_for_bus_sync (data->bus_type,
                                         data->flags,
                                         NULL,
                                         data->name,
                                         data->path,
                                         data->interface,
                                         cancellable,
                                         &local_error);

  if (local_error)
    {
      g_task_return_error (task, g_steal_pointer (&local_error));
      return;
    }

  g_task_return_pointer (task, g_object_ref (g_steal_pointer (&proxy)), g_object_unref);
}

static void
cc_object_storage_finalize (GObject *object)
{
  CcObjectStorage *self = (CcObjectStorage *)object;

  g_debug ("Destroying cached objects");

  g_clear_pointer (&self->id_to_object, g_hash_table_destroy);

  G_OBJECT_CLASS (cc_object_storage_parent_class)->finalize (object);
}

static void
cc_object_storage_class_init (CcObjectStorageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_object_storage_finalize;
}

static void
cc_object_storage_init (CcObjectStorage *self)
{
  self->id_to_object = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

/**
 * cc_object_storage_has_object:
 * @key: the unique string identifier of the object
 *
 * Checks whether there is an object associated with @key.
 *
 * Returns: %TRUE if the object is stored, %FALSE otherwise.
 */
gboolean
cc_object_storage_has_object (const gchar *key)
{
  g_assert (CC_IS_OBJECT_STORAGE (_instance));
  g_assert (key != NULL);

  return g_hash_table_contains (_instance->id_to_object, key);
}

/**
 * cc_object_storage_add_object:
 * @key: the unique string identifier of the object
 * @object: (type GObject): the object to be stored
 *
 * Adds @object to the object storage. It is a programming error to try to
 * add an object that was already added.
 *
 * @object must be a GObject.
 *
 * Always check if the object is stored with cc_object_storage_has_object()
 * before calling this function.
 */
void
cc_object_storage_add_object (const gchar *key,
                              gpointer     object)
{
  /* Trying to add an object that was already added is a hard error. Each
   * object must be added once, and only once, over the entire lifetime
   * of the application.
   */
  g_assert (CC_IS_OBJECT_STORAGE (_instance));
  g_assert (key != NULL);
  g_assert (G_IS_OBJECT (object));
  g_assert (!g_hash_table_contains (_instance->id_to_object, key));

  g_debug ("Adding object %s (%s → %p) to the storage",
           g_type_name (G_OBJECT_TYPE (object)),
           key,
           object);

  g_hash_table_insert (_instance->id_to_object, g_strdup (key), g_object_ref (object));
}

/**
 * cc_object_storage_get_object:
 * @key: the unique string identifier of the object
 *
 * Retrieves the object associated with @key. It is a programming error to
 * try to retrieve an object before adding it.
 *
 * Always check if the object is stored with cc_object_storage_has_object()
 * before calling this function.
 *
 * Returns: (transfer full): the GObject associated with @key.
 */
gpointer
cc_object_storage_get_object (const gchar *key)
{
  /* Trying to peek an object that was not yet added is a hard error. Users
   * of this API need to first check if the object is available with
   * cc_object_storage_has_object().
   */
  g_assert (CC_IS_OBJECT_STORAGE (_instance));
  g_assert (key != NULL);
  g_assert (g_hash_table_contains (_instance->id_to_object, key));

  return g_object_ref (g_hash_table_lookup (_instance->id_to_object, key));
}

/**
 * cc_object_storage_create_dbus_proxy_sync:
 * @name: the D-Bus name
 * @flags: the D-Bus proxy flags
 * @path: the D-Bus object path
 * @interface: the D-Bus interface name
 * @cancellable: (nullable): #GCancellable to cancel the operation
 * @error: (nullable): return location for a #GError
 *
 * Synchronously create a #GDBusProxy with @name, @path and @interface,
 * stores it in the cache, and returns the newly created proxy.
 *
 * If a proxy with that signature is already created, it will be used
 * instead of creating a new one.
 *
 * Returns: (transfer full)(nullable): the new #GDBusProxy.
 */
gpointer
cc_object_storage_create_dbus_proxy_sync (GBusType          bus_type,
                                          GDBusProxyFlags   flags,
                                          const gchar      *name,
                                          const gchar      *path,
                                          const gchar      *interface,
                                          GCancellable     *cancellable,
                                          GError          **error)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *key = NULL;

  g_assert (CC_IS_OBJECT_STORAGE (_instance));
  g_assert (name && *name);
  g_assert (path && *path);
  g_assert (interface && *interface);
  g_assert (!error || !*error);

  key = g_strdup_printf ("CcObjectStorage::dbus-proxy(%s,%s,%s)", name, path, interface);

  g_debug ("Creating D-Bus proxy for %s", key);

  /* Check if a DBus proxy with that signature is already available; if it is,
   * return that instead of a new one.
   */
  if (g_hash_table_contains (_instance->id_to_object, key))
    return cc_object_storage_get_object (key);

  proxy = g_dbus_proxy_new_for_bus_sync (bus_type,
                                         flags,
                                         NULL,
                                         name,
                                         path,
                                         interface,
                                         cancellable,
                                         &local_error);

  if (local_error)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return NULL;
    }

  /* Store the newly created D-Bus proxy */
  cc_object_storage_add_object (key, proxy);

  return g_steal_pointer (&proxy);
}


/**
 * cc_object_storage_create_dbus_proxy:
 * @name: the D-Bus name
 * @flags: the D-Bus proxy flags
 * @path: the D-Bus object path
 * @interface: the D-Bus interface name
 * @cancellable: (nullable): #GCancellable to cancel the operation
 * @callback: callback for when the async operation is finished
 * @user_data: user data for @callback
 *
 * Asynchronously create a #GDBusProxy with @name, @path and @interface.
 *
 * If a proxy with that signature is already created, it will be used instead of
 * creating a new one.
 *
 * It is a programming error to create the an identical proxy while asynchronously
 * creating one. Not cancelling this operation will result in an assertion failure
 * when calling cc_object_storage_create_dbus_proxy_finish().
 */
void
cc_object_storage_create_dbus_proxy (GBusType             bus_type,
                                     GDBusProxyFlags      flags,
                                     const gchar         *name,
                                     const gchar         *path,
                                     const gchar         *interface,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *key = NULL;
  TaskData *data = NULL;

  g_assert (CC_IS_OBJECT_STORAGE (_instance));
  g_assert (name && *name);
  g_assert (path && *path);
  g_assert (interface && *interface);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  data = task_data_new (bus_type, flags, name, path, interface);

  task = g_task_new (_instance, cancellable, callback, user_data);
  g_task_set_source_tag (task, cc_object_storage_create_dbus_proxy);
  g_task_set_task_data (task, data, (GDestroyNotify) task_data_free);

  /* Check if the D-Bus proxy is already created */
  key = g_strdup_printf ("CcObjectStorage::dbus-proxy(%s,%s,%s)", name, path, interface);

  g_debug ("Asynchronously creating D-Bus proxy for %s", key);

  if (g_hash_table_contains (_instance->id_to_object, key))
    {
      /* Mark this GTask as already cached, so we can call the right assertions
       * on the callback
       * */
      data->cached = TRUE;

      g_debug ("Found in cache the D-Bus proxy %s", key);

      g_task_return_pointer (task, cc_object_storage_get_object (key), g_object_unref);
      return;
    }

  g_task_run_in_thread (task, create_dbus_proxy_in_thread_cb);
}

/**
 * cc_object_storage_create_dbus_proxy_finish:
 * @result:
 * @error: (nullable): return location for a #GError
 *
 * Finishes a D-Bus proxy creation started by cc_object_storage_create_dbus_proxy().
 *
 * Synchronously create a #GDBusProxy with @name, @path and @interface,
 * stores it in the cache, and returns the newly created proxy.
 *
 * If a proxy with that signature is already created, it will be used
 * instead of creating a new one.
 *
 * Returns: (transfer full)(nullable): the new #GDBusProxy.
 */
gpointer
cc_object_storage_create_dbus_proxy_finish (GAsyncResult  *result,
                                            GError       **error)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *key = NULL;
  TaskData *task_data;
  GTask *task;

  task = G_TASK (result);

  g_assert (task && G_TASK (result));
  g_assert (!error || !*error);

  task_data = g_task_get_task_data (task);
  g_assert (task_data != NULL);

  key = g_strdup_printf ("CcObjectStorage::dbus-proxy(%s,%s,%s)",
                         task_data->name,
                         task_data->path,
                         task_data->interface);

  g_debug ("Finished creating D-Bus proxy for %s", key);

  /* Retrieve the newly created proxy */
  proxy = g_task_propagate_pointer (task, &local_error);

  /* If the proxy is not cached, do the normal caching routine */
  if (local_error)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return NULL;
    }

  /* Either we have the object stored right when trying to create it - in which case,
   * task_data->cached == TRUE and cc_object_storage_has_object (key) == TRUE - or we
   * didn't have a cached proxy before, and we shouldn't have it now.
   *
   * This is to force consumers of this code to *never* try to create the same D-Bus
   * proxy asynchronously multiple times. Trying to do so is considered a programming
   * error.
   */
  g_assert (task_data->cached == cc_object_storage_has_object (key));

  /* If the proxy is already cached, destroy the newly created and used the cached proxy
   * instead.
   */
  if (cc_object_storage_has_object (key))
    return cc_object_storage_get_object (key);

  /* Store the newly created D-Bus proxy */
  cc_object_storage_add_object (key, proxy);

  return g_steal_pointer (&proxy);
}

/**
 * cc_object_storage_initialize:
 *
 * Initializes the single CcObjectStorage. This must be called only once,
 * and before every other method of this object.
 */
void
cc_object_storage_initialize (void)
{
  g_assert (_instance == NULL);

  /* Fil-C: _instance is a pointer-typed static (GType is a pointer in the
   * pizlonated glib), so the pointer once-init variants must be used; the
   * plain g_once_init_enter/leave functions access the location through a
   * gsize view, which fil-C rejects as a type confusion. */
  if (g_once_init_enter_pointer (&_instance))
    {
      CcObjectStorage *instance = g_object_new (CC_TYPE_OBJECT_STORAGE, NULL);

      g_debug ("Initializing object storage");

      g_once_init_leave_pointer (&_instance, instance);
    }
}

/**
 * cc_object_storage_destroy:
 *
 * Destroys the instance of #CcObjectStorage. This must be called only
 * once during the application lifetime. It is a programming error to
 * call this function multiple times
 */
void
cc_object_storage_destroy (void)
{
  g_assert (_instance != NULL);

  g_clear_object (&_instance);
}
