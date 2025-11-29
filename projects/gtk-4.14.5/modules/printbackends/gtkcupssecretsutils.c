/* gtkcupssecretsutils.h: Helper to use a secrets service for printer passwords
 * Copyright (C) 2014, Intevation GmbH
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

#include <glib.h>
#include <gio/gio.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gtkprivate.h"

#include "gtkcupssecretsutils.h"

#define SECRETS_BUS              "org.freedesktop.secrets"
#define SECRETS_IFACE(interface) "org.freedesktop.Secret."interface
#define SECRETS_PATH             "/org/freedesktop/secrets"
#define SECRETS_TIMEOUT          5000

typedef enum
{
  SECRETS_SERVICE_ACTION_QUERY,
  SECRETS_SERVICE_ACTION_STORE
} SecretsServiceAction;

typedef struct
{
  GDBusConnection       *dbus_connection;
  SecretsServiceAction   action;
  char                 **auth_info,
                       **auth_info_labels,
                       **auth_info_required,
                        *printer_uri,
                        *session_path,
                        *collection_path;
  GDBusProxy            *item_proxy;
  guint                  prompt_subscription;
} SecretsServiceData;

/**
 * create_attributes:
 * @printer_uri: URI for the printer
 * @additional_labels: Optional labels for additional attributes
 * @additional_attrs: Optional additional attributes
 *
 * Creates a GVariant dictionary with key / value pairs that
 * can be used to identify a secret item.
 *
 * Returns: A GVariant dictionary of string pairs or NULL on error.
 */
static GVariant *
create_attributes (const char   *printer_uri,
                   const char  **additional_attrs,
                   const char  **additional_labels)
{
  GVariantBuilder *attr_builder = NULL;
  GVariant        *ret = NULL;

  if (printer_uri == NULL)
    {
      GTK_DEBUG (PRINTING, "create_attributes called with invalid parameters.");
      return NULL;
    }

  attr_builder = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);
  /* The printer uri is the main identifying part */
  g_variant_builder_add (attr_builder, "{ss}", "uri", printer_uri);

  if (additional_labels != NULL)
    {
      int i;
      for (i = 0; additional_labels[i] != NULL; i++)
        {
          g_variant_builder_add (attr_builder, "{ss}",
                                 additional_labels[i],
                                 additional_attrs[i]);
        }
    }

  ret = g_variant_builder_end (attr_builder);
  g_variant_builder_unref (attr_builder);

  return ret;
}

static void
get_secret_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;
  GVariant           *output,
                     *attributes;
  char              **auth_info = NULL,
                     *key = NULL,
                     *value = NULL;
  GVariantIter       *iter = NULL;
  guint               i, required_len;
  int                 pw_field = -1;

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                     res,
                                     &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  attributes = g_dbus_proxy_get_cached_property (task_data->item_proxy,
                                                 "Attributes");
  if (attributes == NULL)
    {
      GTK_DEBUG (PRINTING, "Failed to lookup attributes.");
      g_variant_unref (output);
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  /* Iterate over the attributes to fill the auth info */
  g_variant_get (attributes, "a{ss}", &iter);

  auth_info = g_new0 (char *,
                      g_strv_length (task_data->auth_info_required) + 1);

  while (g_variant_iter_loop (iter, "{ss}", &key, &value))
    {
      /* Match attributes with required auth info */
      for (i = 0; task_data->auth_info_required[i] != NULL; i++)
        {
          if ((strcmp (key, "user") == 0 ||
               strcmp (key, "username") == 0) &&
              strcmp (task_data->auth_info_required[i],
                      "username") == 0)
            {
              auth_info[i] = g_strdup (value);
            }
          else if (strcmp (key, "domain") == 0 &&
                   strcmp (task_data->auth_info_required[i], "domain") == 0)
            {
              auth_info[i] = g_strdup (value);
            }
          else if ((strcmp (key, "hostname") == 0 ||
                    strcmp (key, "server") == 0 ) &&
                   strcmp (task_data->auth_info_required[i], "hostname") == 0)
            {
              auth_info[i] = g_strdup (value);
            }
          else if (strcmp (task_data->auth_info_required[i], "password") == 0)
            {
              pw_field = i;
            }
        }
    }

  if (pw_field == -1)
    {
      /* should not happen... */
      GTK_DEBUG (PRINTING, "No password required?.");
      g_variant_unref (output);
      goto fail;
    }
  else
    {
      GVariant      *secret,
                    *s_value;
      gconstpointer  ba_passwd = NULL;
      gsize          len = 0;

      secret = g_variant_get_child_value (output, 0);
      g_variant_unref (output);
      if (secret == NULL || g_variant_n_children (secret) != 4)
        {
          GTK_DEBUG (PRINTING, "Get secret response invalid.");
          if (secret != NULL)
            g_variant_unref (secret);
          goto fail;
        }
      s_value = g_variant_get_child_value (secret, 2);
      ba_passwd = g_variant_get_fixed_array (s_value,
                                             &len,
                                             sizeof (guchar));

      g_variant_unref (secret);

      if (ba_passwd == NULL)
        {
          GTK_DEBUG (PRINTING, "Invalid / no secret found.");
          g_variant_unref (s_value);
          goto fail;
        }

      auth_info[pw_field] = g_strndup (ba_passwd, len);
      g_variant_unref (s_value);
    }

  for (i = 0; task_data->auth_info_required[i] != NULL; i++)
    {
      if (auth_info[i] == NULL)
        {
          /* Error out if we did not find everything */
          GTK_DEBUG (PRINTING, "Failed to lookup required attribute: %s.",
                               task_data->auth_info_required[i]);
          goto fail;
        }
    }

  g_task_return_pointer (task, auth_info, NULL);
  return;

fail:
  /* Error out */
  GTK_DEBUG (PRINTING, "Failed to lookup secret.");
  required_len = g_strv_length (task_data->auth_info_required);
  for (i = 0; i < required_len; i++)
    {
      /* Not all fields of auth_info are necessarily written so we can not
         use strfreev here */
      g_free (auth_info[i]);
    }
  g_free (auth_info);
  g_task_return_pointer (task, NULL, NULL);
}

static void
create_item_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GTask    *task;
  GError   *error = NULL;
  GVariant *output;
  char     *item = NULL;

  task = user_data;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_variant_get (output, "(&o&o)", &item, NULL);
  if (item != NULL && strlen (item) > 1)
    {
      GTK_DEBUG (PRINTING, "Successfully stored auth info.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }
  g_variant_unref (output);
}

static void
do_store_auth_info (GTask *task)
{
  GVariant            *attributes = NULL,
                      *properties = NULL,
                      *secret = NULL;
  const char         **additional_attrs = NULL,
                     **additional_labels = NULL,
                      *password = NULL;
  SecretsServiceData  *task_data = g_task_get_task_data (task);
  guint                i,
                       length,
                       additional_count = 0;
  GVariantBuilder     *prop_builder = NULL;

  length = g_strv_length (task_data->auth_info_labels);

  additional_attrs = g_new0 (const char *, length + 1);
  additional_labels = g_new0 (const char *, length + 1);
  /* The labels user and server are chosen to be compatible with
     the attributes used by system-config-printer */
  for (i = 0; task_data->auth_info_labels[i] != NULL; i++)
    {
      if (g_strcmp0 (task_data->auth_info_labels[i], "username") == 0)
        {
          additional_attrs[additional_count] = task_data->auth_info[i];
          additional_labels[additional_count++] = "user";
        }
      else if (g_strcmp0 (task_data->auth_info_labels[i], "hostname") == 0)
        {
          additional_attrs[additional_count] = task_data->auth_info[i];
          additional_labels[additional_count++] = "server";
        }
      else if (g_strcmp0 (task_data->auth_info_labels[i], "password") == 0)
        {
          password = task_data->auth_info[i];
        }
    }

  attributes = create_attributes (task_data->printer_uri,
                                  additional_attrs,
                                  additional_labels);
  g_free (additional_labels);
  g_free (additional_attrs);
  if (attributes == NULL)
    {
      GTK_DEBUG (PRINTING, "Failed to create attributes.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  if (password == NULL)
    {
      GTK_DEBUG (PRINTING, "No secret to store.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  prop_builder = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);

  g_variant_builder_add (prop_builder, "{sv}", SECRETS_IFACE ("Item.Label"),
                         g_variant_new_string (task_data->printer_uri));
  g_variant_builder_add (prop_builder, "{sv}", SECRETS_IFACE ("Item.Attributes"),
                         attributes);

  properties = g_variant_builder_end (prop_builder);

  g_variant_builder_unref (prop_builder);

  secret = g_variant_new ("(oay@ays)",
                          task_data->session_path,
                          NULL,
                          g_variant_new_bytestring (password),
                          "text/plain");

  g_dbus_connection_call (task_data->dbus_connection,
                          SECRETS_BUS,
                          task_data->collection_path,
                          SECRETS_IFACE ("Collection"),
                          "CreateItem",
                          g_variant_new ("(@a{sv}@(oayays)b)",
                                         properties,
                                         secret,
                                         TRUE),
                          G_VARIANT_TYPE ("(oo)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          SECRETS_TIMEOUT,
                          g_task_get_cancellable (task),
                          create_item_cb,
                          task);
}

static void
prompt_completed_cb (GDBusConnection *connection,
                     const char      *sender_name,
                     const char      *object_path,
                     const char      *interface_name,
                     const char      *signal_name,
                     GVariant        *parameters,
                     gpointer         user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GVariant           *dismissed;
  gboolean            is_dismissed = TRUE;

  task = user_data;
  task_data = g_task_get_task_data (task);

  g_dbus_connection_signal_unsubscribe (task_data->dbus_connection,
                                        task_data->prompt_subscription);
  task_data->prompt_subscription = 0;

  dismissed = g_variant_get_child_value (parameters, 0);

  if (dismissed == NULL)
    {
      GTK_DEBUG (PRINTING, "Invalid prompt signal.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  g_variant_get (dismissed, "b", &is_dismissed);
  g_variant_unref (dismissed);

  if (is_dismissed)
    {
      GTK_DEBUG (PRINTING, "Collection unlock dismissed.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  /* Prompt successful, proceed to get or store secret */
  switch (task_data->action)
    {
      case SECRETS_SERVICE_ACTION_STORE:
        do_store_auth_info (task);
        break;

      case SECRETS_SERVICE_ACTION_QUERY:
        g_dbus_proxy_call (task_data->item_proxy,
                           "GetSecret",
                           g_variant_new ("(o)",
                                          task_data->session_path),
                           G_DBUS_CALL_FLAGS_NONE,
                           SECRETS_TIMEOUT,
                           g_task_get_cancellable (task),
                           get_secret_cb,
                           task);
        break;
      default:
        ;
    }
}

static void
prompt_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;
  GVariant           *output;

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_variant_unref (output);

  /* Connect to the prompt's completed signal */
  task_data->prompt_subscription =
    g_dbus_connection_signal_subscribe (task_data->dbus_connection,
                                        NULL,
                                        SECRETS_IFACE ("Prompt"),
                                        "Completed",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        prompt_completed_cb,
                                        task,
                                        NULL);
}

static void
unlock_collection_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;
  GVariant           *output;
  const char         *prompt_path;

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_variant_get (output, "(@ao&o)", NULL, &prompt_path);

  if (prompt_path != NULL && strlen (prompt_path) > 1)
    {
      g_dbus_connection_call (task_data->dbus_connection,
                              SECRETS_BUS,
                              prompt_path,
                              SECRETS_IFACE ("Prompt"),
                              "Prompt",
                              g_variant_new ("(s)", "0"),
                              G_VARIANT_TYPE ("()"),
                              G_DBUS_CALL_FLAGS_NONE,
                              SECRETS_TIMEOUT,
                              g_task_get_cancellable (task),
                              prompt_cb,
                              task);
    }
  else
    {
      switch (task_data->action)
        {
          case SECRETS_SERVICE_ACTION_STORE:
            do_store_auth_info (task);
            break;

          case SECRETS_SERVICE_ACTION_QUERY:
            /* Prompt successful proceed to get secret */
            g_dbus_proxy_call (task_data->item_proxy,
                               "GetSecret",
                               g_variant_new ("(o)",
                                              task_data->session_path),
                               G_DBUS_CALL_FLAGS_NONE,
                               SECRETS_TIMEOUT,
                               g_task_get_cancellable (task),
                               get_secret_cb,
                               task);
            break;
          default:
            ;
        }
    }
  g_variant_unref (output);
}

static void
unlock_read_alias_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GTask *task;
  SecretsServiceData *task_data;
  GError *error = NULL;
  GVariant *output, *subresult;
  gsize path_len = 0;
  const char *collection_path;
  const char *to_unlock[2];

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  subresult = g_variant_get_child_value (output, 0);
  g_variant_unref (output);

  if (subresult == NULL)
    {
      GTK_DEBUG (PRINTING, "Invalid ReadAlias response.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  collection_path = g_variant_get_string (subresult, &path_len);
  to_unlock[0] = collection_path;
  to_unlock[1] = NULL;

  task_data->collection_path = g_strdup (collection_path);

  g_dbus_connection_call (task_data->dbus_connection,
                          SECRETS_BUS,
                          SECRETS_PATH,
                          SECRETS_IFACE ("Service"),
                          "Unlock",
                          g_variant_new ("(^ao)", to_unlock),
                          G_VARIANT_TYPE ("(aoo)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          SECRETS_TIMEOUT,
                          g_task_get_cancellable (task),
                          unlock_collection_cb,
                          task);

  g_variant_unref (subresult);
}

static void
item_proxy_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;
  GDBusProxy         *item_proxy;
  GVariant           *locked;
  gboolean            is_locked;

  task = user_data;
  task_data = g_task_get_task_data (task);

  item_proxy = g_dbus_proxy_new_finish (res,
                                        &error);
  if (item_proxy == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  task_data->item_proxy = item_proxy;

  locked = g_dbus_proxy_get_cached_property (item_proxy, "Locked");

  if (locked == NULL)
    {
      GTK_DEBUG (PRINTING, "Failed to look up \"Locked\" property on item.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  g_variant_get (locked, "b", &is_locked);
  g_variant_unref (locked);

  if (is_locked)
    {
      /* Go down the unlock -> lookup path */
      g_dbus_connection_call (task_data->dbus_connection,
                              SECRETS_BUS,
                              SECRETS_PATH,
                              SECRETS_IFACE ("Service"),
                              "ReadAlias",
                              g_variant_new ("(s)", "default"),
                              G_VARIANT_TYPE ("(o)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              SECRETS_TIMEOUT,
                              g_task_get_cancellable (task),
                              unlock_read_alias_cb,
                              task);
      return;
    }

  /* Unlocked proceed to get or store secret */
  switch (task_data->action)
    {
      case SECRETS_SERVICE_ACTION_STORE:
        do_store_auth_info (task);
        break;

      case SECRETS_SERVICE_ACTION_QUERY:
        g_dbus_proxy_call (item_proxy,
                           "GetSecret",
                           g_variant_new ("(o)",
                                          task_data->session_path),
                           G_DBUS_CALL_FLAGS_NONE,
                           SECRETS_TIMEOUT,
                           g_task_get_cancellable (task),
                           get_secret_cb,
                           task);
        break;
      default:
        ;
    }
}

static void
search_items_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;
  GVariant           *output;
  gsize               array_cnt,
                      i;
  gboolean            found_item = FALSE;

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  array_cnt = g_variant_n_children (output);

  for (i = 0; i < array_cnt; i++)
    {
      GVariant * const   item_paths = g_variant_get_child_value (output, i);
      const char       **items = NULL;

      if (item_paths == NULL)
        {
          GTK_DEBUG (PRINTING, "SearchItems returned invalid result.");
          continue;
        }

      items = g_variant_get_objv (item_paths, NULL);

      if (*items == NULL)
        {
          g_variant_unref (item_paths);
          g_free ((gpointer) items);
          continue;
        }

      /* Access the first found item. */
      found_item = TRUE;
      g_dbus_proxy_new (task_data->dbus_connection,
                        G_DBUS_PROXY_FLAGS_NONE,
                        NULL,
                        SECRETS_BUS,
                        *items,
                        SECRETS_IFACE ("Item"),
                        g_task_get_cancellable (task),
                        item_proxy_cb,
                        task);
      g_free ((gpointer) items);
      g_variant_unref (item_paths);
      break;
    }
  g_variant_unref (output);

  if (!found_item)
    {
      GTK_DEBUG (PRINTING, "No match found in secrets service.");
      g_task_return_pointer (task, NULL, NULL);
      return;
    }
}

static void
open_session_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GTask              *task;
  GVariant           *output,
                     *session_variant;
  SecretsServiceData *task_data;
  GError             *error = NULL;

  task = user_data;
  task_data = g_task_get_task_data (task);

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  session_variant = g_variant_get_child_value (output, 1);

  if (session_variant == NULL)
    {
      GTK_DEBUG (PRINTING, "Invalid session path response.");
      g_variant_unref (output);
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  task_data->session_path = g_variant_dup_string (session_variant, NULL);

  if (task_data->session_path == NULL)
    {
      GTK_DEBUG (PRINTING, "Invalid session path string value.");
      g_variant_unref (session_variant);
      g_variant_unref (output);
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  g_variant_unref (session_variant);
  g_variant_unref (output);

  switch (task_data->action)
    {
      case SECRETS_SERVICE_ACTION_QUERY:
        {
          /* Search for the secret item */
          GVariant *secrets_attrs;

          secrets_attrs = create_attributes (task_data->printer_uri, NULL, NULL);
          if (secrets_attrs == NULL)
            {
              GTK_DEBUG (PRINTING, "Failed to create attributes.");
              g_task_return_pointer (task, NULL, NULL);
              return;
            }

          g_dbus_connection_call (task_data->dbus_connection,
                                  SECRETS_BUS,
                                  SECRETS_PATH,
                                  SECRETS_IFACE ("Service"),
                                  "SearchItems",
                                  g_variant_new ("(@a{ss})", secrets_attrs),
                                  G_VARIANT_TYPE ("(aoao)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  SECRETS_TIMEOUT,
                                  g_task_get_cancellable (task),
                                  search_items_cb,
                                  task);
          break;
        }

      case SECRETS_SERVICE_ACTION_STORE:
        {
          /* Look up / unlock the default collection for storing */
          g_dbus_connection_call (task_data->dbus_connection,
                                  SECRETS_BUS,
                                  SECRETS_PATH,
                                  SECRETS_IFACE ("Service"),
                                  "ReadAlias",
                                  g_variant_new ("(s)", "default"),
                                  G_VARIANT_TYPE ("(o)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  SECRETS_TIMEOUT,
                                  g_task_get_cancellable (task),
                                  unlock_read_alias_cb,
                                  task);
          break;
        }
      default:
        ;
    }
}

static void
get_connection_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GTask              *task;
  SecretsServiceData *task_data;
  GError             *error = NULL;

  task = user_data;
  task_data = g_task_get_task_data (task);

  task_data->dbus_connection = g_bus_get_finish (res, &error);
  if (task_data->dbus_connection == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  /* Now open a session */
  g_dbus_connection_call (task_data->dbus_connection,
                          SECRETS_BUS,
                          SECRETS_PATH,
                          SECRETS_IFACE ("Service"),
                          "OpenSession",
                          g_variant_new ("(sv)", "plain",
                                         g_variant_new_string ("")),
                          G_VARIANT_TYPE ("(vo)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          SECRETS_TIMEOUT,
                          g_task_get_cancellable (task),
                          open_session_cb,
                          task);
}

/**
 * gtk_cups_secrets_service_watch:
 * @appeared: The callback to call when the service interface appears
 * @vanished: The callback to call when the service interface vanishes
 * @user_data: A reference to the watching printbackend
 *
 * Registers a watch for the secrets service interface.
 *
 * Returns: The watcher id
 */
guint
gtk_cups_secrets_service_watch (GBusNameAppearedCallback appeared,
                                GBusNameVanishedCallback vanished,
                                gpointer                 user_data)
{
  return g_bus_watch_name (G_BUS_TYPE_SESSION,
                           SECRETS_BUS,
                           G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                           appeared,
                           vanished,
                           user_data,
                           NULL);
}

static void
cleanup_task_data (gpointer data)
{
  int                 i;
  SecretsServiceData *task_data = data;

  g_free (task_data->collection_path);
  g_strfreev (task_data->auth_info_labels);
  g_strfreev (task_data->auth_info_required);
  g_free (task_data->printer_uri);

  if (task_data->auth_info != NULL)
    {
      for (i = 0; task_data->auth_info[i] != NULL; i++)
        {
          memset (task_data->auth_info[i], 0, strlen (task_data->auth_info[i]));
          g_clear_pointer (&task_data->auth_info[i], g_free);
        }
      g_clear_pointer (&task_data->auth_info, g_free);
    }

  if (task_data->prompt_subscription != 0)
    {
      g_dbus_connection_signal_unsubscribe (task_data->dbus_connection,
                                            task_data->prompt_subscription);
      task_data->prompt_subscription = 0;
    }

  if (task_data->session_path != NULL)
    {
      g_dbus_connection_call (task_data->dbus_connection,
                              SECRETS_BUS,
                              task_data->session_path,
                              SECRETS_IFACE ("Session"),
                              "Close",
                              NULL,
                              G_VARIANT_TYPE ("()"),
                              G_DBUS_CALL_FLAGS_NONE,
                              SECRETS_TIMEOUT,
                              NULL,
                              NULL,
                              NULL);
    }

  g_clear_object (&task_data->dbus_connection);
  g_clear_pointer (&task_data->session_path, g_free);
  g_clear_object (&task_data->item_proxy);
}

/**
 * gtk_cups_secrets_service_query_task:
 * @source_object: Source object for this task
 * @cancellable: Cancellable to cancel this task
 * @callback: Callback to call once the query is finished
 * @user_data: (closure): The user_data passed to the callback
 * @printer_uri: URI of the printer
 * @auth_info_required: Info required for authentication
 *
 * Checks if a secrets service as described by the secrets-service standard
 * is available and if so it tries to find the authentication info in the
 * default collection of the service.
 *
 * This is the entry point to a chain of async calls to open a session,
 * search the secret, unlock the collection (if necessary) and finally
 * to lookup the secret.
 *
 * See: http://standards.freedesktop.org/secret-service/ for documentation
 * of the used API.
 */
void
gtk_cups_secrets_service_query_task (gpointer              source_object,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data,
                                     const char           *printer_uri,
                                     char                **auth_info_required)
{
  GTask              *task;
  SecretsServiceData *task_data;

  task_data = g_new0 (SecretsServiceData, 1);
  task_data->action = SECRETS_SERVICE_ACTION_QUERY;
  task_data->printer_uri = g_strdup (printer_uri);
  task_data->auth_info_required = g_strdupv (auth_info_required);

  task = g_task_new (source_object, cancellable, callback, user_data);

  g_task_set_task_data (task, task_data, cleanup_task_data);

  g_bus_get (G_BUS_TYPE_SESSION, cancellable,
             get_connection_cb, task);
}

static void
store_done_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GTask  *task = (GTask *) res;
  GError *error = NULL;

  g_task_propagate_pointer (task, &error);

  if (error != NULL)
    {
      GTK_DEBUG (PRINTING, "Failed to store auth info: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (task);
  GTK_DEBUG (PRINTING, "gtk_cups_secrets_service_store finished.");
}

/**
 * gtk_cups_secrets_service_store:
 * @auth_info: Auth info that should be stored
 * @auth_info_labels: The keys to use for the auth info
 * @printer_uri: URI of the printer
 *
 * Tries to store the auth_info in a secrets service.
 */
void
gtk_cups_secrets_service_store (char        **auth_info,
                                char        **auth_info_labels,
                                const char   *printer_uri)
{
  GTask              *task;
  SecretsServiceData *task_data;

  if (auth_info == NULL || auth_info_labels == NULL || printer_uri == NULL)
    {
      GTK_DEBUG (PRINTING, "Invalid call to gtk_cups_secrets_service_store.");
      return;
    }

  task_data = g_new0 (SecretsServiceData, 1);
  task_data->action = SECRETS_SERVICE_ACTION_STORE;
  task_data->printer_uri = g_strdup (printer_uri);
  task_data->auth_info = g_strdupv (auth_info);
  task_data->auth_info_labels = g_strdupv (auth_info_labels);

  task = g_task_new (NULL, NULL, store_done_cb, NULL);

  g_task_set_task_data (task, task_data, cleanup_task_data);

  g_bus_get (G_BUS_TYPE_SESSION, NULL,
             get_connection_cb, task);
}
