/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009-2011 Red Hat, Inc
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

#define G_TYPE_TLS_CONNECTION_BASE            (g_tls_connection_base_get_type ())

G_DECLARE_DERIVABLE_TYPE (GTlsConnectionBase, g_tls_connection_base, G, TLS_CONNECTION_BASE, GTlsConnection)

typedef enum {
  G_TLS_CONNECTION_BASE_OK,
  G_TLS_CONNECTION_BASE_WOULD_BLOCK,
  G_TLS_CONNECTION_BASE_TIMED_OUT,
  G_TLS_CONNECTION_BASE_REHANDSHAKE,
  G_TLS_CONNECTION_BASE_TRY_AGAIN,
  G_TLS_CONNECTION_BASE_ERROR,
} GTlsConnectionBaseStatus;

typedef enum {
  G_TLS_DIRECTION_NONE = 0,
  G_TLS_DIRECTION_READ = 1 << 0,
  G_TLS_DIRECTION_WRITE = 1 << 1,
} GTlsDirection;

typedef enum {
  G_TLS_SAFE_RENEGOTIATION_SUPPORTED_BY_PEER,
  G_TLS_SAFE_RENEGOTIATION_UNSUPPORTED
} GTlsSafeRenegotiationStatus;

#define G_TLS_DIRECTION_BOTH (G_TLS_DIRECTION_READ | G_TLS_DIRECTION_WRITE)

struct _GTlsConnectionBaseClass
{
  GTlsConnectionClass parent_class;

  void                        (*prepare_handshake)          (GTlsConnectionBase       *tls,
                                                             gchar                   **advertised_protocols);
  GTlsSafeRenegotiationStatus (*handshake_thread_safe_renegotiation_status)
                                                            (GTlsConnectionBase        *tls);
  GTlsConnectionBaseStatus    (*handshake_thread_request_rehandshake)
                                                            (GTlsConnectionBase   *tls,
                                                             gint64                timeout,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
  GTlsConnectionBaseStatus    (*handshake_thread_handshake) (GTlsConnectionBase   *tls,
                                                             gint64                timeout,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
  GTlsCertificate            *(*retrieve_peer_certificate)  (GTlsConnectionBase   *tls);
  GTlsCertificateFlags        (*verify_chain)               (GTlsConnectionBase       *tls,
                                                             GTlsCertificate          *chain,
                                                             const gchar              *purpose,
                                                             GSocketConnectable       *identity,
                                                             GTlsInteraction          *interaction,
                                                             GTlsDatabaseVerifyFlags   flags,
                                                             GCancellable             *cancellable,
                                                             GError                  **error);
  GTlsCertificateFlags        (*verify_peer_certificate)    (GTlsConnectionBase   *tls,
                                                             GTlsCertificate      *certificate,
                                                             GTlsCertificateFlags  flags);
  void                        (*complete_handshake)         (GTlsConnectionBase   *tls,
                                                             gboolean              handshake_succeeded,
                                                             gchar               **negotiated_protocol,
                                                             GTlsProtocolVersion  *protocol_version,
                                                             gchar               **ciphersuite_name,
                                                             GError              **error);

  gboolean                    (*is_session_resumed)         (GTlsConnectionBase   *tls);

  gboolean                    (*get_channel_binding_data)   (GTlsConnectionBase      *tls,
                                                             GTlsChannelBindingType   type,
                                                             GByteArray              *data,
                                                             GError                 **error);

  void                        (*push_io)                    (GTlsConnectionBase   *tls,
                                                             GIOCondition          direction,
                                                             gint64                timeout,
                                                             GCancellable         *cancellable);
  GTlsConnectionBaseStatus    (*pop_io)                     (GTlsConnectionBase   *tls,
                                                             GIOCondition          direction,
                                                             gboolean              success,
                                                             GError              **error);

  GTlsConnectionBaseStatus    (*read_fn)                    (GTlsConnectionBase   *tls,
                                                             void                 *buffer,
                                                             gsize                 count,
                                                             gint64                timeout,
                                                             gssize               *nread,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
  GTlsConnectionBaseStatus    (*read_message_fn)            (GTlsConnectionBase   *tls,
                                                             GInputVector         *vectors,
                                                             guint                 num_vectors,
                                                             gint64                timeout,
                                                             gssize               *nread,
                                                             GCancellable         *cancellable,
                                                             GError              **error);

  GTlsConnectionBaseStatus    (*write_fn)                   (GTlsConnectionBase   *tls,
                                                             const void           *buffer,
                                                             gsize                 count,
                                                             gint64                timeout,
                                                             gssize               *nwrote,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
  GTlsConnectionBaseStatus    (*write_message_fn)           (GTlsConnectionBase   *tls,
                                                             GOutputVector        *vectors,
                                                             guint                 num_vectors,
                                                             gint64                timeout,
                                                             gssize               *nwrote,
                                                             GCancellable         *cancellable,
                                                             GError              **error);

  GTlsConnectionBaseStatus    (*close_fn)                   (GTlsConnectionBase   *tls,
                                                             gint64                timeout,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
};

gboolean                  g_tls_connection_base_handshake_thread_verify_certificate
                                                                        (GTlsConnectionBase *tls);

void                      g_tls_connection_base_push_io                 (GTlsConnectionBase *tls,
                                                                         GIOCondition        direction,
                                                                         gint64              timeout,
                                                                         GCancellable       *cancellable);
GTlsConnectionBaseStatus  g_tls_connection_base_pop_io                  (GTlsConnectionBase  *tls,
                                                                         GIOCondition         direction,
                                                                         gboolean             success,
                                                                         GError             **error);

gssize                    g_tls_connection_base_read                    (GTlsConnectionBase  *tls,
                                                                         void                *buffer,
                                                                         gsize                size,
                                                                         gint64               timeout,
                                                                         GCancellable        *cancellable,
                                                                         GError             **error);
gssize                    g_tls_connection_base_write                   (GTlsConnectionBase  *tls,
                                                                         const void          *buffer,
                                                                         gsize                size,
                                                                         gint64               timeout,
                                                                         GCancellable        *cancellable,
                                                                         GError             **error);

gboolean                  g_tls_connection_base_check                   (GTlsConnectionBase  *tls,
                                                                         GIOCondition         condition);
gboolean                  g_tls_connection_base_base_check              (GTlsConnectionBase  *tls,
                                                                         GIOCondition         condition);
GSource                  *g_tls_connection_base_create_source           (GTlsConnectionBase  *tls,
                                                                         GIOCondition         condition,
                                                                         GCancellable        *cancellable);

gboolean                  g_tls_connection_base_close_internal          (GIOStream      *stream,
                                                                         GTlsDirection   direction,
                                                                         gint64          timeout,
                                                                         GCancellable   *cancellable,
                                                                         GError        **error);

gboolean                  g_tls_connection_base_is_dtls                 (GTlsConnectionBase *tls);

GDatagramBased           *g_tls_connection_base_get_base_socket         (GTlsConnectionBase *tls);

GIOStream                *g_tls_connection_base_get_base_iostream       (GTlsConnectionBase *tls);
GPollableInputStream     *g_tls_connection_base_get_base_istream        (GTlsConnectionBase *tls);
GPollableOutputStream    *g_tls_connection_base_get_base_ostream        (GTlsConnectionBase *tls);

void                      g_tls_connection_base_handshake_thread_set_missing_requested_client_certificate
                                                                        (GTlsConnectionBase *tls);

GError                  **g_tls_connection_base_get_read_error          (GTlsConnectionBase *tls);
GError                  **g_tls_connection_base_get_write_error         (GTlsConnectionBase *tls);

gint64                    g_tls_connection_base_get_read_timeout        (GTlsConnectionBase *tls);
gint64                    g_tls_connection_base_get_write_timeout       (GTlsConnectionBase *tls);

GCancellable             *g_tls_connection_base_get_read_cancellable    (GTlsConnectionBase *tls);
GCancellable             *g_tls_connection_base_get_write_cancellable   (GTlsConnectionBase *tls);

gboolean                  g_tls_connection_base_is_handshaking          (GTlsConnectionBase *tls);

gboolean                  g_tls_connection_base_ever_handshaked         (GTlsConnectionBase *tls);

gboolean                  g_tls_connection_base_handshake_thread_request_certificate
                                                                        (GTlsConnectionBase  *tls);
gboolean                  g_tls_connection_base_handshake_thread_ask_password
                                                                        (GTlsConnectionBase *tls,
                                                                         GTlsPassword       *password);

void                      g_tls_connection_base_handshake_thread_buffer_application_data
                                                                        (GTlsConnectionBase *tls,
                                                                         guint8             *data,
                                                                         gsize               length);

gchar                    *g_tls_connection_base_get_session_id          (GTlsConnectionBase  *tls);

gboolean                  g_tls_connection_base_get_session_resumption  (GTlsConnectionBase  *tls);

void                      g_tls_connection_base_set_session_resumption  (GTlsConnectionBase *tls,
                                                                         gboolean session_resumption_enabled);

G_END_DECLS
