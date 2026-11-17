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
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

G_BEGIN_DECLS

#define CC_TYPE_ONLINE_ACCOUNT_PROVIDER_ROW (cc_online_account_provider_row_get_type ())

G_DECLARE_FINAL_TYPE (CcOnlineAccountProviderRow, cc_online_account_provider_row, CC, ONLINE_ACCOUNT_PROVIDER_ROW, AdwActionRow)

CcOnlineAccountProviderRow *cc_online_account_provider_row_new (GoaProvider *provider);

GoaProvider *cc_online_account_provider_row_get_provider (CcOnlineAccountProviderRow *row);

G_END_DECLS
