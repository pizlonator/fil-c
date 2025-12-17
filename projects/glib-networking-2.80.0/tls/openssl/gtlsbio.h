/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlsbio.h
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

BIO       *g_tls_bio_new_from_iostream     (GIOStream *io_stream);

BIO       *g_tls_bio_new_from_datagram_based (GDatagramBased *socket);

void       g_tls_bio_set_read_cancellable  (BIO          *bio,
                                            GCancellable *cancellable);

void       g_tls_bio_set_read_error        (BIO          *bio,
                                            GError      **error);

void       g_tls_bio_set_write_cancellable (BIO          *bio,
                                            GCancellable *cancellable);

void       g_tls_bio_set_write_error       (BIO          *bio,
                                            GError      **error);

gboolean   g_tls_bio_wait_available        (BIO          *bio,
                                            GIOCondition  condition,
                                            gint64        timeout,
                                            GCancellable *cancellable);

G_END_DECLS
