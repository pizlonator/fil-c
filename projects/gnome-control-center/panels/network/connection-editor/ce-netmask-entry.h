/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (CENetmaskEntry, ce_netmask_entry, CE, NETMASK_ENTRY, GtkEntry)

CENetmaskEntry *ce_netmask_entry_new        (void);

gboolean        ce_netmask_entry_is_empty   (CENetmaskEntry *entry);

gboolean        ce_netmask_entry_is_valid   (CENetmaskEntry *entry);

guint32         ce_netmask_entry_get_prefix (CENetmaskEntry *entry);

G_END_DECLS
