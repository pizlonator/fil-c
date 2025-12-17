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
#include <gnutls/x509.h>

#include "gtlscertificate-gnutls.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_DATABASE_GNUTLS            (g_tls_database_gnutls_get_type ())

G_DECLARE_DERIVABLE_TYPE (GTlsDatabaseGnutls, g_tls_database_gnutls, G, TLS_DATABASE_GNUTLS, GTlsDatabase)

struct _GTlsDatabaseGnutlsClass
{
  GTlsDatabaseClass parent_class;

  gchar    *(*create_handle_for_certificate)  (GTlsDatabaseGnutls                *self,
                                               GBytes                            *der);
  gboolean  (*populate_trust_list)            (GTlsDatabaseGnutls                *self,
                                               gnutls_x509_trust_list_t           trust_list,
                                               GError                           **error);
};

typedef struct
{
  gnutls_certificate_credentials_t credentials;
  gatomicrefcount ref_count;
} GGnutlsCertificateCredentials;

GTlsDatabaseGnutls *g_tls_database_gnutls_new (GError **error);

GGnutlsCertificateCredentials *g_tls_database_gnutls_get_credentials (GTlsDatabaseGnutls  *self,
                                                                      GError             **error);

GGnutlsCertificateCredentials *g_gnutls_certificate_credentials_new   (GError **error);
GGnutlsCertificateCredentials *g_gnutls_certificate_credentials_ref   (GGnutlsCertificateCredentials *credentials);
void                           g_gnutls_certificate_credentials_unref (GGnutlsCertificateCredentials *credentials);

G_END_DECLS
