/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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
#include <colord.h>

G_BEGIN_DECLS

#define CC_TYPE_COLOR_DEVICE (cc_color_device_get_type ())
G_DECLARE_FINAL_TYPE (CcColorDevice, cc_color_device, CC, COLOR_DEVICE, GtkListBoxRow)

GtkWidget   *cc_color_device_new           (CdDevice       *device);
CdDevice    *cc_color_device_get_device    (CcColorDevice  *color_device);
const gchar *cc_color_device_get_sortable  (CcColorDevice  *color_device);
void         cc_color_device_set_expanded  (CcColorDevice  *color_device,
                                            gboolean        expanded);

G_END_DECLS
