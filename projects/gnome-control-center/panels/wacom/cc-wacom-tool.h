/*
 * Copyright © 2016 Red Hat, Inc.
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *
 */

#pragma once

#include "config.h"
#include "gsd-device-manager.h"
#include "cc-wacom-device.h"
#include <glib.h>

#define CC_TYPE_WACOM_TOOL (cc_wacom_tool_get_type ())
G_DECLARE_FINAL_TYPE (CcWacomTool, cc_wacom_tool, CC, WACOM_TOOL, GObject)

CcWacomTool   * cc_wacom_tool_new             (guint64        serial,
					       guint64        id,
					       CcWacomDevice *device);

guint64         cc_wacom_tool_get_serial      (CcWacomTool   *tool);
guint64         cc_wacom_tool_get_id          (CcWacomTool   *tool);

const gchar   * cc_wacom_tool_get_name        (CcWacomTool   *tool);
const gchar   * cc_wacom_tool_get_icon_name   (CcWacomTool   *tool);

GSettings     * cc_wacom_tool_get_settings    (CcWacomTool   *tool);

guint           cc_wacom_tool_get_num_buttons (CcWacomTool   *tool);
gboolean        cc_wacom_tool_get_has_paired_eraser  (CcWacomTool   *tool);

const gchar   * cc_wacom_tool_get_description (CcWacomTool   *tool);
