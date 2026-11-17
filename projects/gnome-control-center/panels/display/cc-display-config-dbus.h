/*
 * Copyright (C) 2017  Red Hat, Inc.
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
 *
 */

#pragma once

#include <glib-object.h>

#include "cc-display-config.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_MODE_DBUS (cc_display_mode_dbus_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayModeDBus, cc_display_mode_dbus,
                      CC, DISPLAY_MODE_DBUS, CcDisplayMode)

#define CC_TYPE_DISPLAY_MONITOR_DBUS (cc_display_monitor_dbus_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayMonitorDBus, cc_display_monitor_dbus,
                      CC, DISPLAY_MONITOR_DBUS, CcDisplayMonitor)

#define CC_TYPE_DISPLAY_CONFIG_DBUS (cc_display_config_dbus_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayConfigDBus, cc_display_config_dbus,
                      CC, DISPLAY_CONFIG_DBUS, CcDisplayConfig)

G_END_DECLS
