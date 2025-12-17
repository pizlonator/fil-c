/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Collabora, Ltd.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#pragma once

#include <gio/gio.h>

#include "gtlsdatabase-gnutls.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_FILE_DATABASE_GNUTLS            (g_tls_file_database_gnutls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsFileDatabaseGnutls, g_tls_file_database_gnutls, G, TLS_FILE_DATABASE_GNUTLS, GTlsDatabaseGnutls)

GTlsDatabase*                g_tls_file_database_gnutls_new                   (const gchar *anchor_file);

G_END_DECLS
