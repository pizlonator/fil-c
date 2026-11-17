/* cc-tls-certificate.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean         bonsai_is_tls_hash                                       (const gchar          *hash);
GTlsCertificate *bonsai_tls_certificate_new_generate                      (const gchar          *public_key_path,
                                                                           const gchar          *private_key_path,
                                                                           const gchar          *c,
                                                                           const gchar          *cn,
                                                                           GCancellable         *cancellable,
                                                                           GError              **error);
void             bonsai_tls_certificate_new_generate_async                (const gchar          *public_key_path,
                                                                           const gchar          *private_key_path,
                                                                           const gchar          *c,
                                                                           const gchar          *cn,
                                                                           GCancellable         *cancellable,
                                                                           GAsyncReadyCallback   callback,
                                                                           gpointer              user_data);
GTlsCertificate *bonsai_tls_certificate_new_generate_finish               (GAsyncResult         *result,
                                                                           GError              **error);
gchar           *bonsai_tls_certificate_get_hash                          (GTlsCertificate      *cert);
GTlsCertificate *bonsai_tls_certificate_new_from_files_or_generate        (const gchar          *public_key_path,
                                                                           const gchar          *private_key_path,
                                                                           const gchar          *c,
                                                                           const gchar          *cn,
                                                                           GCancellable         *cancellable,
                                                                           GError              **error);
void             bonsai_tls_certificate_new_from_files_or_generate_async  (const gchar          *public_key_path,
                                                                           const gchar          *private_key_path,
                                                                           const gchar          *c,
                                                                           const gchar          *cn,
                                                                           GCancellable         *cancellable,
                                                                           GAsyncReadyCallback   callback,
                                                                           gpointer              user_data);
GTlsCertificate *bonsai_tls_certificate_new_from_files_or_generate_finish (GAsyncResult         *result,
                                                                           GError              **error);
GTlsCertificate *bonsai_tls_certificate_new_for_user                      (GCancellable         *cancellable,
                                                                           GError              **error);

G_END_DECLS

