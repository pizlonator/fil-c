/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Red Hat, Inc
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

#include "config.h"
#include "glib.h"

#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "gtlsserverconnection-gnutls.h"
#include "gtlsbackend-gnutls.h"
#include "gtlscertificate-gnutls.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_0,
  PROP_AUTHENTICATION_MODE
};

struct _GTlsServerConnectionGnutls
{
  GTlsConnectionGnutls parent_instance;

  GTlsAuthenticationMode authentication_mode;

  gnutls_pcert_st *pcert;
  unsigned int pcert_length;
  gnutls_privkey_t pkey;
};

static void     g_tls_server_connection_gnutls_initable_interface_init (GInitableIface  *iface);

static void g_tls_server_connection_gnutls_server_connection_interface_init (GTlsServerConnectionInterface *iface);

static int g_tls_server_connection_gnutls_handshake_thread_retrieve_function (GTlsConnectionGnutls         *self,
                                                                              gnutls_session_t              session,
                                                                              const gnutls_datum_t         *req_ca_rdn,
                                                                              int                           nreqs,
                                                                              const gnutls_pk_algorithm_t  *pk_algos,
                                                                              int                           pk_algos_length,
                                                                              gnutls_pcert_st             **pcert,
                                                                              unsigned int                 *pcert_length,
                                                                              gnutls_privkey_t             *pkey);

static GInitableIface *g_tls_server_connection_gnutls_parent_initable_iface;

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsServerConnectionGnutls, g_tls_server_connection_gnutls, G_TYPE_TLS_CONNECTION_GNUTLS,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_server_connection_gnutls_initable_interface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_SERVER_CONNECTION,
                                                      g_tls_server_connection_gnutls_server_connection_interface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_SERVER_CONNECTION,
                                                      NULL)
)

static void
clear_gnutls_certificate_copy (GTlsServerConnectionGnutls *gnutls)
{
  g_tls_certificate_gnutls_copy_free (gnutls->pcert, gnutls->pcert_length, gnutls->pkey);

  gnutls->pcert = NULL;
  gnutls->pcert_length = 0;
  gnutls->pkey = NULL;
}

static void
g_tls_server_connection_gnutls_init (GTlsServerConnectionGnutls *gnutls)
{
}

static void
g_tls_server_connection_gnutls_finalize (GObject *object)
{
  GTlsServerConnectionGnutls *gnutls = G_TLS_SERVER_CONNECTION_GNUTLS (object);

  clear_gnutls_certificate_copy (gnutls);

  G_OBJECT_CLASS (g_tls_server_connection_gnutls_parent_class)->finalize (object);
}

static gboolean
g_tls_server_connection_gnutls_initable_init (GInitable       *initable,
                                              GCancellable    *cancellable,
                                              GError         **error)
{
  GTlsCertificate *cert;

  if (!g_tls_server_connection_gnutls_parent_initable_iface->init (initable, cancellable, error))
    return FALSE;

  /* Currently we don't know ahead of time if a PKCS #11 backed certificate has a private key. */
  cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (initable));
  if (cert && !g_tls_certificate_gnutls_has_key (G_TLS_CERTIFICATE_GNUTLS (cert)) &&
      !g_tls_certificate_gnutls_is_pkcs11_backed (G_TLS_CERTIFICATE_GNUTLS (cert)))
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Certificate has no private key"));
      return FALSE;
    }

  return TRUE;
}

static void
g_tls_server_connection_gnutls_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  GTlsServerConnectionGnutls *gnutls = G_TLS_SERVER_CONNECTION_GNUTLS (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      g_value_set_enum (value, gnutls->authentication_mode);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_server_connection_gnutls_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  GTlsServerConnectionGnutls *gnutls = G_TLS_SERVER_CONNECTION_GNUTLS (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      gnutls->authentication_mode = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static int
g_tls_server_connection_gnutls_handshake_thread_retrieve_function (GTlsConnectionGnutls         *self,
                                                                   gnutls_session_t              session,
                                                                   const gnutls_datum_t         *req_ca_rdn,
                                                                   int                           nreqs,
                                                                   const gnutls_pk_algorithm_t  *pk_algos,
                                                                   int                           pk_algos_length,
                                                                   gnutls_pcert_st             **pcert,
                                                                   unsigned int                 *pcert_length,
                                                                   gnutls_privkey_t             *pkey)
{
  GTlsServerConnectionGnutls *gnutls = G_TLS_SERVER_CONNECTION_GNUTLS (self);

  clear_gnutls_certificate_copy (gnutls);

  g_tls_connection_gnutls_handshake_thread_get_certificate (G_TLS_CONNECTION_GNUTLS (gnutls),
                                                            pcert, pcert_length, pkey);

  gnutls->pcert = *pcert;
  gnutls->pcert_length = *pcert_length;
  gnutls->pkey = *pkey;

  return 0;
}

static void
g_tls_server_connection_gnutls_prepare_handshake (GTlsConnectionBase  *tls,
                                                  gchar              **advertised_protocols)
{
  GTlsServerConnectionGnutls *gnutls = G_TLS_SERVER_CONNECTION_GNUTLS (tls);
  gnutls_session_t session;
  gnutls_certificate_request_t req_mode;

  switch (gnutls->authentication_mode)
    {
    case G_TLS_AUTHENTICATION_REQUESTED:
      req_mode = GNUTLS_CERT_REQUEST;
      break;
    case G_TLS_AUTHENTICATION_REQUIRED:
      req_mode = GNUTLS_CERT_REQUIRE;
      break;
    case G_TLS_AUTHENTICATION_NONE:
    default:
      req_mode = GNUTLS_CERT_IGNORE;
      break;
    }

  session = g_tls_connection_gnutls_get_session (G_TLS_CONNECTION_GNUTLS (tls));
  gnutls_certificate_server_set_request (session, req_mode);

  G_TLS_CONNECTION_BASE_CLASS (g_tls_server_connection_gnutls_parent_class)->prepare_handshake (tls, advertised_protocols);
}

static void
g_tls_server_connection_gnutls_class_init (GTlsServerConnectionGnutlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);
  GTlsConnectionGnutlsClass *gnutls_class = G_TLS_CONNECTION_GNUTLS_CLASS (klass);

  gobject_class->finalize     = g_tls_server_connection_gnutls_finalize;
  gobject_class->get_property = g_tls_server_connection_gnutls_get_property;
  gobject_class->set_property = g_tls_server_connection_gnutls_set_property;

  base_class->prepare_handshake  = g_tls_server_connection_gnutls_prepare_handshake;

  gnutls_class->handshake_thread_retrieve_function = g_tls_server_connection_gnutls_handshake_thread_retrieve_function;

  g_object_class_override_property (gobject_class, PROP_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_tls_server_connection_gnutls_server_connection_interface_init (GTlsServerConnectionInterface *iface)
{
}

static void
g_tls_server_connection_gnutls_initable_interface_init (GInitableIface  *iface)
{
  g_tls_server_connection_gnutls_parent_initable_iface = g_type_interface_peek_parent (iface);

  iface->init = g_tls_server_connection_gnutls_initable_init;
}
