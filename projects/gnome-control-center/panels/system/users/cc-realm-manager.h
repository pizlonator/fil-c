/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc.
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

#pragma once

#include "cc-realm-generated.h"

G_BEGIN_DECLS

typedef enum {
        CC_REALM_ERROR_BAD_LOGIN,
        CC_REALM_ERROR_BAD_PASSWORD,
        CC_REALM_ERROR_CANNOT_AUTH,
        CC_REALM_ERROR_BAD_HOSTNAME,
        CC_REALM_ERROR_GENERIC,
} CcRealmErrors;

#define CC_REALM_ERROR (cc_realm_error_get_quark ())

GQuark           cc_realm_error_get_quark         (void) G_GNUC_CONST;

#define CC_TYPE_REALM_MANAGER (cc_realm_manager_get_type ())
G_DECLARE_FINAL_TYPE (CcRealmManager, cc_realm_manager, CC, REALM_MANAGER, CcRealmObjectManagerClient)

void             cc_realm_manager_new             (GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

CcRealmManager * cc_realm_manager_new_finish      (GAsyncResult *result,
                                                   GError **error);

void             cc_realm_manager_discover        (CcRealmManager *self,
                                                   const gchar *input,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

GList *          cc_realm_manager_discover_finish (CcRealmManager *self,
                                                   GAsyncResult *result,
                                                   GError **error);

GList *          cc_realm_manager_get_realms      (CcRealmManager *self);

void             cc_realm_login                   (CcRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

GBytes *         cc_realm_login_finish            (GAsyncResult *result,
                                                   GError **error);

gboolean         cc_realm_join_as_user            (CcRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GBytes *credentials,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data)
                                                   G_GNUC_WARN_UNUSED_RESULT;

gboolean         cc_realm_join_as_admin           (CcRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GBytes *credentials,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data)
                                                   G_GNUC_WARN_UNUSED_RESULT;

gboolean         cc_realm_join_finish             (CcRealmObject *realm,
                                                   GAsyncResult *result,
                                                   GError **error);

gboolean         cc_realm_is_configured           (CcRealmObject *realm);

gchar *          cc_realm_calculate_login         (CcRealmCommon *realm,
                                                   const gchar *username);

G_END_DECLS
