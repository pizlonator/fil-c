/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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

#include <glib-object.h>
#include <gio/gio.h>
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_HOST (pp_host_get_type ())
G_DECLARE_DERIVABLE_TYPE (PpHost, pp_host, PP, HOST, GObject)

struct _PpHostClass
{
  GObjectClass parent_class;
};

#define PP_HOST_UNSET_PORT               -1
#define PP_HOST_DEFAULT_IPP_PORT        631
#define PP_HOST_DEFAULT_JETDIRECT_PORT 9100
#define PP_HOST_DEFAULT_LPD_PORT        515

PpHost        *pp_host_new                            (const gchar          *hostname);

void           pp_host_get_snmp_devices_async         (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

GPtrArray     *pp_host_get_snmp_devices_finish        (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_remote_cups_devices_async  (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

GPtrArray     *pp_host_get_remote_cups_devices_finish (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_jetdirect_devices_async    (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

GPtrArray     *pp_host_get_jetdirect_devices_finish   (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_lpd_devices_async          (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

GPtrArray     *pp_host_get_lpd_devices_finish         (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS
