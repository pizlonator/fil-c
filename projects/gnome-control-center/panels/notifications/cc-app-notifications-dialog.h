/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_APP_NOTIFICATIONS_DIALOG (cc_app_notifications_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcAppNotificationsDialog, cc_app_notifications_dialog, CC, APP_NOTIFICATIONS_DIALOG, AdwWindow)

CcAppNotificationsDialog *cc_app_notifications_dialog_new (const gchar          *app_id,
                                                           const gchar          *title,
                                                           GSettings            *settings,
                                                           GSettings            *master_settings,
                                                           GDBusProxy           *perm_store);

G_END_DECLS
