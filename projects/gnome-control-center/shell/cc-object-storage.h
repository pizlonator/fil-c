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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* Default storage keys */
#define CC_OBJECT_NMCLIENT     "CcObjectStorage::nm-client"
#define CC_OBJECT_HOSTNAME     "CcObjectStorage::hostname"
#define CC_OBJECT_PWQ_SETTINGS "CcObjectStorage::pw-quality-settings"

#define CC_TYPE_OBJECT_STORAGE (cc_object_storage_get_type())

G_DECLARE_FINAL_TYPE (CcObjectStorage, cc_object_storage, CC, OBJECT_STORAGE, GObject)

gboolean cc_object_storage_has_object             (const gchar         *key);

void     cc_object_storage_add_object             (const gchar         *key,
                                                   gpointer             object);

gpointer cc_object_storage_get_object             (const gchar         *key);

gpointer cc_object_storage_create_dbus_proxy_sync (GBusType             bus_type,
                                                   GDBusProxyFlags     flags,
                                                   const gchar         *name,
                                                   const gchar         *path,
                                                   const gchar         *interface,
                                                   GCancellable        *cancellable,
                                                   GError             **error);

void     cc_object_storage_create_dbus_proxy      (GBusType              bus_type,
                                                   GDBusProxyFlags       flags,
                                                   const gchar          *name,
                                                   const gchar          *path,
                                                   const gchar          *interface,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);

gpointer cc_object_storage_create_dbus_proxy_finish (GAsyncResult       *result,
                                                     GError            **error);

void     cc_object_storage_initialize               (void);

void     cc_object_storage_destroy                  (void);

G_END_DECLS
