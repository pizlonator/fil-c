/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009 Red Hat, Inc.
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
#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>

G_BEGIN_DECLS

#define G_TYPE_TLS_CERTIFICATE_GNUTLS            (g_tls_certificate_gnutls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsCertificateGnutls, g_tls_certificate_gnutls, G, TLS_CERTIFICATE_GNUTLS, GTlsCertificate)

GTlsCertificate *            g_tls_certificate_gnutls_new             (const gnutls_datum_t  *datum,
                                                                       GTlsCertificate       *issuer);

GBytes *                     g_tls_certificate_gnutls_get_bytes       (GTlsCertificateGnutls *gnutls);

void                         g_tls_certificate_gnutls_set_data        (GTlsCertificateGnutls *gnutls,
                                                                       const gnutls_datum_t  *datum);

const gnutls_x509_crt_t      g_tls_certificate_gnutls_get_cert        (GTlsCertificateGnutls *gnutls);
gboolean                     g_tls_certificate_gnutls_has_key         (GTlsCertificateGnutls *gnutls);
gboolean                     g_tls_certificate_gnutls_is_pkcs11_backed (GTlsCertificateGnutls *gnutls);

void                         g_tls_certificate_gnutls_copy            (GTlsCertificateGnutls  *gnutls,
                                                                       const gchar            *interaction_id,
                                                                       gnutls_pcert_st       **pcert,
                                                                       unsigned int           *pcert_length,
                                                                       gnutls_privkey_t       *pkey);

void                         g_tls_certificate_gnutls_copy_free       (gnutls_pcert_st        *pcert,
                                                                       unsigned int            pcert_length,
                                                                       gnutls_privkey_t        pkey);

GTlsCertificateFlags         g_tls_certificate_gnutls_verify_identity (GTlsCertificateGnutls  *gnutls,
                                                                       GSocketConnectable     *identity,
                                                                       GError                **error);

GTlsCertificateFlags         g_tls_certificate_gnutls_convert_flags   (guint                  gnutls_flags);

void                         g_tls_certificate_gnutls_set_issuer      (GTlsCertificateGnutls *gnutls,
                                                                       GTlsCertificateGnutls *issuer);

GTlsCertificateGnutls*       g_tls_certificate_gnutls_steal_issuer    (GTlsCertificateGnutls *gnutls);

GTlsCertificateGnutls*       g_tls_certificate_gnutls_build_chain     (const gnutls_datum_t  *certs,
                                                                       guint                  num_certs,
                                                                       gnutls_x509_crt_fmt_t  format);

G_END_DECLS
