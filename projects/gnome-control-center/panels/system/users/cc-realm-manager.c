/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2012  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Stef Walter <stefw@gnome.org>
 */

#include "config.h"

#include "cc-realm-manager.h"

#include <krb5/krb5.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>


struct _CcRealmManager {
        CcRealmObjectManagerClient parent_instance;

        CcRealmProvider *provider;
        guint diagnostics_sig;
};

enum {
        REALM_ADDED,
        NUM_SIGNALS,
};

static gint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (CcRealmManager, cc_realm_manager, CC_REALM_TYPE_OBJECT_MANAGER_CLIENT);

GQuark
cc_realm_error_get_quark (void)
{
        static GQuark quark = 0;
        if (quark == 0)
                quark = g_quark_from_static_string ("cc-realm-error");
        return quark;
}

static gboolean
is_realm_with_kerberos_and_membership (gpointer object)
{
        g_autoptr(GDBusInterface) kerberos_interface = NULL;
        g_autoptr(GDBusInterface) kerberos_membership_interface = NULL;

        if (!G_IS_DBUS_OBJECT (object))
                return FALSE;

        kerberos_interface = g_dbus_object_get_interface (object, "org.freedesktop.realmd.Kerberos");
        if (kerberos_interface == NULL)
                return FALSE;

        kerberos_membership_interface = g_dbus_object_get_interface (object, "org.freedesktop.realmd.KerberosMembership");
        if (kerberos_membership_interface == NULL)
                return FALSE;

        return TRUE;
}

static void
on_interface_added (CcRealmManager *self,
                    GDBusObject *object,
                    GDBusInterface *interface)
{
        g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (interface), G_MAXINT);
}

static void
on_object_added (CcRealmManager *self,
                 GDBusObject *object)
{
        GList *interfaces, *l;

        interfaces = g_dbus_object_get_interfaces (object);
        for (l = interfaces; l != NULL; l = g_list_next (l))
                on_interface_added (self, object, l->data);
        g_list_free_full (interfaces, g_object_unref);

        if (is_realm_with_kerberos_and_membership (object)) {
                g_debug ("Saw realm: %s", g_dbus_object_get_object_path (object));
                g_signal_emit (self, signals[REALM_ADDED], 0, object);
        }
}

static void
cc_realm_manager_init (CcRealmManager *self)
{
        g_signal_connect (self, "object-added", G_CALLBACK (on_object_added), NULL);
        g_signal_connect (self, "interface-added", G_CALLBACK (on_interface_added), NULL);
}

static void
cc_realm_manager_dispose (GObject *obj)
{
        CcRealmManager *self = CC_REALM_MANAGER (obj);
        GDBusConnection *connection;

        g_clear_object (&self->provider);

        if (self->diagnostics_sig) {
                connection = g_dbus_object_manager_client_get_connection (G_DBUS_OBJECT_MANAGER_CLIENT (self));
                if (connection != NULL)
                        g_dbus_connection_signal_unsubscribe (connection, self->diagnostics_sig);
                self->diagnostics_sig = 0;
        }

        G_OBJECT_CLASS (cc_realm_manager_parent_class)->dispose (obj);
}

static void
cc_realm_manager_class_init (CcRealmManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = cc_realm_manager_dispose;

        signals[REALM_ADDED] = g_signal_new ("realm-added", CC_TYPE_REALM_MANAGER,
                                             G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                             g_cclosure_marshal_generic,
                                             G_TYPE_NONE, 1, CC_REALM_TYPE_OBJECT);
}

static void
on_realm_diagnostics (GDBusConnection *connection,
                      const gchar *sender_name,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *signal_name,
                      GVariant *parameters,
                      gpointer user_data)
{
        const gchar *message;
        const gchar *unused;

        if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ss)"))) {
                /* Data is already formatted appropriately for stderr */
                g_variant_get (parameters, "(&s&s)", &message, &unused);
                g_printerr ("%s", message);
        }
}

static void
on_provider_new (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        CcRealmManager *manager = g_task_get_task_data (task);
        GError *error = NULL;

        manager->provider = cc_realm_provider_proxy_new_finish (result, &error);
        if (error == NULL) {
                g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (manager->provider), -1);
                g_debug ("Created realm manager");
                g_task_return_pointer (task, g_object_ref (manager), g_object_unref);
        } else {
                g_task_return_error (task, error);
        }
}

static void
on_manager_new (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        CcRealmManager *manager;
        GDBusConnection *connection;
        GError *error = NULL;
        GObject *object;
        guint sig;

        object = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, &error);
        if (error == NULL) {
                manager = CC_REALM_MANAGER (object);
                connection = g_dbus_object_manager_client_get_connection (G_DBUS_OBJECT_MANAGER_CLIENT (object));

                g_debug ("Connected to realmd");

                sig = g_dbus_connection_signal_subscribe (connection,
                                                          "org.freedesktop.realmd",
                                                          "org.freedesktop.realmd.Service",
                                                          "Diagnostics",
                                                          NULL,
                                                          NULL,
                                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                                          on_realm_diagnostics,
                                                          NULL,
                                                          NULL);
                manager->diagnostics_sig = sig;

                g_task_set_task_data (task, manager, g_object_unref);

                cc_realm_provider_proxy_new (connection,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                             "org.freedesktop.realmd",
                                             "/org/freedesktop/realmd",
                                             g_task_get_cancellable (task),
                                             on_provider_new, task);
                g_steal_pointer (&task);
        } else {
                g_task_return_error (task, error);
        }
}

void
cc_realm_manager_new (GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
        GTask *task;

        g_debug ("Connecting to realmd...");

        task = g_task_new (NULL, cancellable, callback, user_data);
        g_task_set_source_tag (task, cc_realm_manager_new);

        g_async_initable_new_async (CC_TYPE_REALM_MANAGER, G_PRIORITY_DEFAULT,
                                    cancellable, on_manager_new, task,
                                    "flags", G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                    "name", "org.freedesktop.realmd",
                                    "bus-type", G_BUS_TYPE_SYSTEM,
                                    "object-path", "/org/freedesktop/realmd",
                                    "get-proxy-type-func", cc_realm_object_manager_client_get_proxy_type,
                                    NULL);
}

CcRealmManager *
cc_realm_manager_new_finish (GAsyncResult *result,
                             GError **error)
{
        g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
        g_return_val_if_fail (g_async_result_is_tagged (result, cc_realm_manager_new), NULL);

        return g_task_propagate_pointer (G_TASK (result), error);
}

static void
realms_free (gpointer data)
{
        g_list_free_full (data, g_object_unref);
}

static void
on_provider_discover (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        CcRealmManager *manager = g_task_get_source_object (task);
        GError *error = NULL;
        gboolean no_membership = FALSE;
        gchar **realms;
        gint relevance;
        gint i;
        GList *kerberos_realms = NULL;

        cc_realm_provider_call_discover_finish (CC_REALM_PROVIDER (source), &relevance,
                                                &realms, result, &error);
        if (error == NULL) {
                for (i = 0; realms[i]; i++) {
                        g_autoptr(GDBusObject) object = NULL;

                        object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (manager), realms[i]);
                        if (object == NULL) {
                                g_warning ("Realm is not in object manager: %s", realms[i]);
                        } else {
                                if (is_realm_with_kerberos_and_membership (object)) {
                                        g_debug ("Discovered realm: %s", realms[i]);
                                        kerberos_realms = g_list_prepend (kerberos_realms, g_steal_pointer (&object));
                                } else {
                                        g_debug ("Realm does not support kerberos membership: %s", realms[i]);
                                        no_membership = TRUE;
                                }
                        }
                }
                g_strfreev (realms);

                if (!kerberos_realms && no_membership) {
                        g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_GENERIC,
                                                 _("Cannot automatically join this type of domain"));
                } else if (!kerberos_realms) {
                        g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_GENERIC,
                                                 _("No such domain or realm found"));
                } else {
                        kerberos_realms = g_list_reverse (kerberos_realms);
                        g_task_return_pointer (task, kerberos_realms, realms_free);
                }
        } else {
                g_task_return_error (task, error);
        }
}

void
cc_realm_manager_discover (CcRealmManager *self,
                           const gchar *input,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
        GTask *task;
        GVariant *options;

        g_return_if_fail (CC_IS_REALM_MANAGER (self));
        g_return_if_fail (input != NULL);
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

        g_debug ("Discovering realms for: %s", input);

        task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
        g_task_set_source_tag (task, cc_realm_manager_discover);

        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        cc_realm_provider_call_discover (self->provider, input, options, cancellable,
                                         on_provider_discover, task);
}

GList *
cc_realm_manager_discover_finish (CcRealmManager *self,
                                  GAsyncResult *result,
                                  GError **error)
{
        g_return_val_if_fail (CC_IS_REALM_MANAGER (self), NULL);
        g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (self)), NULL);
        g_return_val_if_fail (g_async_result_is_tagged (result, cc_realm_manager_discover), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        return g_task_propagate_pointer (G_TASK (result), error);
}

GList *
cc_realm_manager_get_realms (CcRealmManager *self)
{
        GList *objects;
        GList *realms = NULL;
        GList *l;

        g_return_val_if_fail (CC_IS_REALM_MANAGER (self), NULL);

        objects = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (self));
        for (l = objects; l != NULL; l = g_list_next (l)) {
                if (is_realm_with_kerberos_and_membership (l->data))
                        realms = g_list_prepend (realms, g_object_ref (l->data));
        }

        g_list_free_full (objects, g_object_unref);
        return realms;
}

static void
string_replace (GString *string,
                const gchar *find,
                const gchar *replace)
{
        const gchar *at;
        gssize pos;

        at = strstr (string->str, find);
        if (at != NULL) {
                pos = at - string->str;
                g_string_erase (string, pos, strlen (find));
                g_string_insert (string, pos, replace);
        }
}

gchar *
cc_realm_calculate_login (CcRealmCommon *realm,
                          const gchar *username)
{
        const gchar *const *formats;

        formats = cc_realm_common_get_login_formats (realm);
        if (formats[0] != NULL) {
                GString *string = g_string_new (formats[0]);
                string_replace (string, "%U", username);
                string_replace (string, "%D", cc_realm_common_get_name (realm));
                return g_string_free (string, FALSE);
        }

        return NULL;
}

gboolean
cc_realm_is_configured (CcRealmObject *realm)
{
        g_autoptr(CcRealmCommon) common = NULL;
        const gchar *configured;
        gboolean is = FALSE;

        common = cc_realm_object_get_common (realm);
        if (common != NULL) {
                configured = cc_realm_common_get_configured (common);
                is = configured != NULL && !g_str_equal (configured, "");
        }

        return is;
}

static const gchar *
find_supported_credentials (CcRealmKerberosMembership *membership,
                            const gchar *owner)
{
        const gchar *cred_owner;
        const gchar *cred_type;
        GVariant *supported;
        GVariantIter iter;

        supported = cc_realm_kerberos_membership_get_supported_join_credentials (membership);
        g_return_val_if_fail (supported != NULL, NULL);

        g_variant_iter_init (&iter, supported);
        while (g_variant_iter_loop (&iter, "(&s&s)", &cred_type, &cred_owner)) {
                if (g_str_equal (owner, cred_owner)) {
                        if (g_str_equal (cred_type, "ccache") ||
                            g_str_equal (cred_type, "password")) {
                                return g_intern_string (cred_type);
                        }
                }
        }

        return NULL;
}

static void
join_cb (GObject      *source,
         GAsyncResult *result,
         gpointer      user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        g_autoptr(GError) call_error = NULL;
        g_autofree gchar *dbus_error = NULL;

        if (cc_realm_kerberos_membership_call_join_finish (CC_REALM_KERBEROS_MEMBERSHIP (source), result, &call_error)) {
                g_debug ("Completed Join() method call");

                g_task_return_boolean (task, TRUE);
                return;
        }

        dbus_error = g_dbus_error_get_remote_error (call_error);
        if (dbus_error == NULL) {
                g_debug ("Join() failed because of %s", call_error->message);
                g_task_return_error (task, g_steal_pointer (&call_error));
                return;
        }

        g_dbus_error_strip_remote_error (call_error);

        if (g_str_equal (dbus_error, "org.freedesktop.realmd.Error.AuthenticationFailed")) {
                g_debug ("Join() failed because of invalid/insufficient credentials");
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN, "%s", call_error->message);
        } else if (g_str_equal (dbus_error, "org.freedesktop.realmd.Error.BadHostname")) {
                g_debug ("Join() failed because of invalid/conflicting host name");
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_BAD_HOSTNAME, "%s", call_error->message);
        } else {
                g_debug ("Join() failed because of %s", call_error->message);
                g_task_return_error (task, g_steal_pointer (&call_error));
        }
}

static gboolean
realm_join_as_owner (CcRealmObject *realm,
                     const gchar *owner,
                     const gchar *login,
                     const gchar *password,
                     GBytes *credentials,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
        g_autoptr(CcRealmKerberosMembership) membership = NULL;
        GVariant *contents;
        GVariant *options;
        GVariant *option;
        GVariant *creds;
        const gchar *type;
        GTask *task;

        membership = cc_realm_object_get_kerberos_membership (realm);
        g_return_val_if_fail (membership != NULL, FALSE);

        type = find_supported_credentials (membership, owner);
        if (type == NULL) {
                g_debug ("Couldn't find supported credential type for owner: %s", owner);
                return FALSE;
        }

        if (g_str_equal (type, "ccache")) {
                g_debug ("Using a kerberos credential cache to join the realm");
                contents = g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                    g_bytes_get_data (credentials, NULL),
                                                    g_bytes_get_size (credentials),
                                                    TRUE, (GDestroyNotify)g_bytes_unref, credentials);

        } else if (g_str_equal (type, "password")) {
                g_debug ("Using a user/password to join the realm");
                contents = g_variant_new ("(ss)", login, password);

        } else {
                g_assert_not_reached ();
        }

        creds = g_variant_new ("(ssv)", type, owner, contents);
        option = g_variant_new ("{sv}", "manage-system", g_variant_new_boolean (FALSE));
        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), &option, 1);

        g_debug ("Calling the Join() method with %s credentials", owner);

        task = g_task_new (realm, cancellable, callback, user_data);
        cc_realm_kerberos_membership_call_join (membership, creds, options,
                                                cancellable, join_cb, task);

        return TRUE;
}

gboolean
cc_realm_join_as_user (CcRealmObject *realm,
                       const gchar *login,
                       const gchar *password,
                       GBytes *credentials,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
        g_return_val_if_fail (CC_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "user", login, password,
                                    credentials, cancellable, callback, user_data);
}

gboolean
cc_realm_join_as_admin (CcRealmObject *realm,
                        const gchar *login,
                        const gchar *password,
                        GBytes *credentials,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
        g_return_val_if_fail (CC_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "administrator", login, password, credentials,
                                    cancellable, callback, user_data);
}

gboolean
cc_realm_join_finish (CcRealmObject *realm,
                      GAsyncResult *result,
                      GError **error)
{
        g_return_val_if_fail (CC_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
        gchar *domain;
        gchar *realm;
        gchar *user;
        gchar *password;
} LoginClosure;

static void
login_closure_free (gpointer data)
{
        LoginClosure *login = data;
        g_clear_pointer (&login->domain, g_free);
        g_clear_pointer (&login->realm, g_free);
        g_clear_pointer (&login->user, g_free);
        g_clear_pointer (&login->password, g_free);
        g_slice_free (LoginClosure, login);
}

static krb5_error_code
login_perform_kinit (krb5_context k5,
                     const gchar *realm,
                     const gchar *login,
                     const gchar *password,
                     const gchar *filename)
{
        krb5_get_init_creds_opt *opts;
        krb5_error_code code;
        krb5_principal principal;
        krb5_ccache ccache;
        krb5_creds creds;
        g_autofree gchar *name = NULL;

        name = g_strdup_printf ("%s@%s", login, realm);
        code = krb5_parse_name (k5, name, &principal);

        if (code != 0) {
                g_debug ("Couldn't parse principal name: %s: %s",
                         name, krb5_get_error_message (k5, code));
                return code;
        }

        g_debug ("Using principal name to kinit: %s", name);

        if (filename == NULL)
                code = krb5_cc_default (k5, &ccache);
        else
                code = krb5_cc_resolve (k5, filename, &ccache);

        if (code != 0) {
                krb5_free_principal (k5, principal);
                g_debug ("Couldn't open credential cache: %s: %s",
                         filename ? filename : "<default>",
                         krb5_get_error_message (k5, code));
                return code;
        }

        code = krb5_get_init_creds_opt_alloc (k5, &opts);
        g_return_val_if_fail (code == 0, code);

        code = krb5_get_init_creds_opt_set_out_ccache (k5, opts, ccache);
        g_return_val_if_fail (code == 0, code);

        code = krb5_get_init_creds_password (k5, &creds, principal,
                                             (char *)password,
                                             NULL, 0, 0, NULL, opts);

        krb5_get_init_creds_opt_free (k5, opts);
        krb5_cc_close (k5, ccache);
        krb5_free_principal (k5, principal);

        if (code == 0) {
                g_debug ("kinit succeeded");
                krb5_free_cred_contents (k5, &creds);
        } else {
                g_debug ("kinit failed: %s", krb5_get_error_message (k5, code));
        }

        return code;
}

static void
kinit_thread_func (GTask *t,
                   gpointer object,
                   gpointer task_data,
                   GCancellable *cancellable)
{
        g_autoptr(GTask) task = t;
        LoginClosure *login = task_data;
        krb5_context k5 = NULL;
        krb5_error_code code;
        g_autofree gchar *filename = NULL;
        gchar *contents;
        gsize length;
        gint temp_fd;

        filename = g_build_filename (g_get_user_runtime_dir (),
                                     "um-krb5-creds.XXXXXX", NULL);
        temp_fd = g_mkstemp_full (filename, O_RDWR, S_IRUSR | S_IWUSR);
        if (temp_fd == -1) {
                g_warning ("Couldn't create credential cache file: %s: %s",
                           filename, g_strerror (errno));
                g_clear_pointer (&filename, g_free);
        } else {
                close (temp_fd);
        }

        code = krb5_init_context (&k5);
        if (code == 0) {
                code = login_perform_kinit (k5, login->realm, login->user,
                                            login->password, filename);
        }

        switch (code) {
        case 0:
                if (filename != NULL) {
                        g_autoptr(GError) error = NULL;

                        if (g_file_get_contents (filename, &contents, &length, &error)) {
                                g_debug ("Read in credential cache: %s", filename);
                        } else {
                                g_warning ("Couldn't read credential cache: %s: %s",
                                           filename, error->message);
                        }

                        g_task_return_pointer (task, g_bytes_new_take (contents, length), (GDestroyNotify) g_bytes_unref);
                }
                break;

        case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
        case KRB5KDC_ERR_POLICY:
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN,
                                         _("Cannot log in as %s at the %s domain"),
                                         login->user, login->domain);
                break;
        case KRB5KDC_ERR_PREAUTH_FAILED:
        case KRB5KRB_AP_ERR_BAD_INTEGRITY:
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_BAD_PASSWORD,
                                         _("Invalid password, please try again"));
                break;
        case KRB5_PREAUTH_FAILED:
        case KRB5KDC_ERR_KEY_EXP:
        case KRB5KDC_ERR_CLIENT_REVOKED:
        case KRB5KDC_ERR_ETYPE_NOSUPP:
        case KRB5_PROG_ETYPE_NOSUPP:
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_CANNOT_AUTH,
                                         _("Cannot log in as %s at the %s domain"),
                                         login->user, login->domain);
                break;
        default:
                g_task_return_new_error (task, CC_REALM_ERROR, CC_REALM_ERROR_GENERIC,
                                         _("Couldn’t connect to the %s domain: %s"),
                                         login->domain, krb5_get_error_message (k5, code));
                break;
        }

        if (filename) {
                g_unlink (filename);
                g_debug ("Deleted credential cache: %s", filename);
        }

        if (k5)
                krb5_free_context (k5);
}

void
cc_realm_login (CcRealmObject *realm,
                const gchar *user,
                const gchar *password,
                GCancellable *cancellable,
                GAsyncReadyCallback callback,
                gpointer user_data)
{
        GTask *task;
        LoginClosure *login;
        g_autoptr(CcRealmKerberos) kerberos = NULL;

        g_return_if_fail (CC_REALM_IS_OBJECT (realm));
        g_return_if_fail (user != NULL);
        g_return_if_fail (password != NULL);
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

        kerberos = cc_realm_object_get_kerberos (realm);
        g_return_if_fail (kerberos != NULL);

        task = g_task_new (NULL, cancellable, callback, user_data);
        g_task_set_source_tag (task, cc_realm_login);

        login = g_slice_new0 (LoginClosure);
        login->domain = g_strdup (cc_realm_kerberos_get_domain_name (kerberos));
        login->realm = g_strdup (cc_realm_kerberos_get_realm_name (kerberos));
        login->user = g_strdup (user);
        login->password = g_strdup (password);
        g_task_set_task_data (task, login, login_closure_free);

        g_task_set_return_on_cancel (task, TRUE);
        g_task_run_in_thread (task, kinit_thread_func);
}

GBytes *
cc_realm_login_finish (GAsyncResult *result,
                       GError **error)
{
        g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
        g_return_val_if_fail (g_async_result_is_tagged (result, cc_realm_login), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        return g_task_propagate_pointer (G_TASK (result), error);
}
