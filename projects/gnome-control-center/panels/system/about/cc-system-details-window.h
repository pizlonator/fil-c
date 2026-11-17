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

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_SYSTEM_DETAILS_WINDOW (cc_system_details_window_get_type ())
G_DECLARE_FINAL_TYPE (CcSystemDetailsWindow, cc_system_details_window, CC, SYSTEM_DETAILS_WINDOW, AdwWindow)

CcSystemDetailsWindow   *cc_system_details_window_new   (void);
char                    *get_hardware_model_string      (void);
char                    *get_cpu_info                   (void);
char                    *get_os_name                    (void);
guint64                  get_ram_size_dmi               (void);
guint64                  get_ram_size_libgtop           (void);
char                    *get_primary_disk_info          (void);
G_END_DECLS
