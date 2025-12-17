/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlscertificate-openssl.h
 *
 * Copyright (C) 2015 NICE s.r.l.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Authors: Ignacio Casal Quinteiro
 */

#pragma once

#include <gio/gio.h>
#include "openssl-include.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_CERTIFICATE_OPENSSL (g_tls_certificate_openssl_get_type ())
G_DECLARE_FINAL_TYPE (GTlsCertificateOpenssl, g_tls_certificate_openssl,
                      G, TLS_CERTIFICATE_OPENSSL, GTlsCertificate)

GTlsCertificate             *g_tls_certificate_openssl_new             (GBytes                 *bytes,
                                                                        GTlsCertificate        *issuer);

GTlsCertificate             *g_tls_certificate_openssl_new_from_x509   (X509                   *x,
                                                                        GTlsCertificate        *issuer);

void                         g_tls_certificate_openssl_set_data        (GTlsCertificateOpenssl *openssl,
                                                                        GBytes                 *bytes);

GBytes *                     g_tls_certificate_openssl_get_bytes       (GTlsCertificateOpenssl *openssl);

X509                        *g_tls_certificate_openssl_get_cert        (GTlsCertificateOpenssl *openssl);
EVP_PKEY                    *g_tls_certificate_openssl_get_key         (GTlsCertificateOpenssl *openssl);

void                         g_tls_certificate_openssl_set_issuer      (GTlsCertificateOpenssl *openssl,
                                                                        GTlsCertificateOpenssl *issuer);

GTlsCertificateFlags         g_tls_certificate_openssl_verify_identity (GTlsCertificateOpenssl *openssl,
                                                                        GSocketConnectable     *identity);

GTlsCertificateFlags         g_tls_certificate_openssl_convert_error   (guint                   openssl_error);

GTlsCertificateOpenssl      *g_tls_certificate_openssl_build_chain     (X509                   *x,
                                                                        STACK_OF (X509)        *chain);

G_END_DECLS
