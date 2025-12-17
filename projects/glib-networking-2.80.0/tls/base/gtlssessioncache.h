/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2022 YouView TV Ltd.
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
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef gpointer (*SessionDup)      (gpointer);
typedef gint     (*SessionAcquire)  (gpointer);
typedef void     (*SessionRelease)  (gpointer);

void    g_tls_store_session_data (gchar                    *session_id,
                                  gpointer                  session_data,
                                  SessionDup                session_dup,
                                  SessionAcquire            inc_ref,
                                  SessionRelease            dec_ref,
                                  GTlsProtocolVersion       protocol_version
                                  );

gpointer g_tls_lookup_session_data (gchar *session_id);

G_END_DECLS
