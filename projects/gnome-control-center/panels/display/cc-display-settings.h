/* -*- mode: c; style: linux -*-
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>
#include "cc-display-config.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_SETTINGS cc_display_settings_get_type ()
G_DECLARE_FINAL_TYPE (CcDisplaySettings, cc_display_settings, CC, DISPLAY_SETTINGS, GtkBox);

CcDisplaySettings*  cc_display_settings_new                 (void);

gboolean            cc_display_settings_get_has_accelerometer (CcDisplaySettings    *settings);
void                cc_display_settings_set_has_accelerometer (CcDisplaySettings    *settings,
                                                               gboolean              has_accelerometer);
CcDisplayConfig*    cc_display_settings_get_config            (CcDisplaySettings    *settings);
void                cc_display_settings_set_config            (CcDisplaySettings    *settings,
                                                               CcDisplayConfig      *config);
CcDisplayMonitor*   cc_display_settings_get_selected_output   (CcDisplaySettings    *settings);
void                cc_display_settings_set_selected_output   (CcDisplaySettings    *settings,
                                                               CcDisplayMonitor     *output);
void                cc_display_settings_refresh_layout        (CcDisplaySettings    *settings,
                                                               gboolean              collapsed);
void                cc_display_settings_set_multimonitor      (CcDisplaySettings    *self,
                                                               gboolean              multimonitor);

G_END_DECLS

