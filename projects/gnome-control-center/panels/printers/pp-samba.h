/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#pragma once

#include "pp-host.h"
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_SAMBA (pp_samba_get_type ())
G_DECLARE_FINAL_TYPE (PpSamba, pp_samba, PP, SAMBA, PpHost)

PpSamba       *pp_samba_new                (const gchar         *hostname);

void           pp_samba_get_devices_async  (PpSamba             *samba,
                                            gboolean             auth_if_needed,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);

GPtrArray     *pp_samba_get_devices_finish (PpSamba             *samba,
                                            GAsyncResult        *result,
                                            GError             **error);

void           pp_samba_set_auth_info      (PpSamba             *samba,
                                            const gchar         *username,
                                            const gchar         *password);

G_END_DECLS
