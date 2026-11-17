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

#define PP_TYPE_CUPS (pp_cups_get_type ())
G_DECLARE_FINAL_TYPE (PpCups, pp_cups, PP, CUPS, GObject)

typedef struct{
  cups_dest_t *dests;
  gint         num_of_dests;
} PpCupsDests;

PpCups      *pp_cups_new              (void);

void         pp_cups_get_dests_async  (PpCups               *cups,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);

PpCupsDests *pp_cups_get_dests_finish (PpCups               *cups,
                                       GAsyncResult         *result,
                                       GError              **error);

void         pp_cups_connection_test_async (PpCups              *cups,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);

gboolean     pp_cups_connection_test_finish (PpCups         *cups,
                                             GAsyncResult   *result,
                                             GError        **error);

void         pp_cups_cancel_subscription_async    (PpCups              *cups,
                                                   gint                 subscription_id,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data);

gboolean     pp_cups_cancel_subscription_finish   (PpCups                *cups,
                                                   GAsyncResult          *result);

void         pp_cups_renew_subscription_async  (PpCups                *cups,
                                                gint                   subscription_id,
                                                gchar                **events,
                                                gint                   lease_duration,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data);

gint         pp_cups_renew_subscription_finish (PpCups                *cups,
                                                GAsyncResult          *result);

G_END_DECLS
