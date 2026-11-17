/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gdesktop-enums.h>

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_WACOM_PANEL (cc_wacom_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcWacomPanel, cc_wacom_panel, CC, WACOM_PANEL, CcPanel)

void cc_wacom_panel_static_init_func (void);

void  cc_wacom_panel_switch_to_panel (CcWacomPanel *self,
				      const char   *panel);

void  cc_wacom_panel_set_osd_visibility (CcWacomPanel *self,
                                         guint32        device_id);

GDBusProxy * cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self);

GDBusProxy * cc_wacom_panel_get_input_mapping_bus_proxy (CcWacomPanel *self);

const char * cc_wacom_panel_get_stylus_button_action_label (GDesktopStylusButtonAction action);

G_END_DECLS
