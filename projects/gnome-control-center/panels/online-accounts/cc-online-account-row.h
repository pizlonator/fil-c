/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2020 Canonical Ltd.
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
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

G_BEGIN_DECLS

#define CC_TYPE_ONLINE_ACCOUNT_ROW (cc_online_account_row_get_type ())

G_DECLARE_FINAL_TYPE (CcOnlineAccountRow, cc_online_account_row, CC, ONLINE_ACCOUNT_ROW, AdwActionRow)

CcOnlineAccountRow *cc_online_account_row_new        (GoaObject *object);

GoaObject          *cc_online_account_row_get_object (CcOnlineAccountRow *row);

G_END_DECLS
