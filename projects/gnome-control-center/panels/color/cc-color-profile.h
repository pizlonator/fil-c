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

#define CC_TYPE_COLOR_PROFILE (cc_color_profile_get_type ())
G_DECLARE_FINAL_TYPE (CcColorProfile, cc_color_profile, CC, COLOR_PROFILE, GtkListBoxRow)

GtkWidget   *cc_color_profile_new              (CdDevice        *device,
                                                CdProfile       *profile,
                                                gboolean         is_default);
gboolean     cc_color_profile_get_is_default   (CcColorProfile  *color_profile);
void         cc_color_profile_set_is_default   (CcColorProfile  *color_profile,
                                                gboolean         profile_is_default);
CdDevice    *cc_color_profile_get_device       (CcColorProfile  *color_profile);
CdProfile   *cc_color_profile_get_profile      (CcColorProfile  *color_profile);
const gchar *cc_color_profile_get_sortable     (CcColorProfile  *color_profile);

G_END_DECLS

