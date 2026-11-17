/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2023 Cyber Phantom <inam123451@gmail.com>
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
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_INFO_ENTRY (cc_info_entry_get_type ())
G_DECLARE_FINAL_TYPE (CcInfoEntry, cc_info_entry, CC, INFO_ENTRY, GtkBox)

void cc_info_entry_set_value (CcInfoEntry   *self,
                              const gchar   *value);
GtkWidget *cc_info_entry_new (const gchar   *label,
                              const gchar   *value);

G_END_DECLS

