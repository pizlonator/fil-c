/*
 * Copyright (C) 2018 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#pragma once

#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema * cc_grd_rdp_credentials_get_schema (void);

void cc_grd_store_rdp_credentials (const gchar  *username,
                                   const gchar  *password,
                                   GCancellable *cancellable);

gchar * cc_grd_lookup_rdp_username (GCancellable *cancellable);
gchar * cc_grd_lookup_rdp_password (GCancellable *cancellable);

G_END_DECLS
