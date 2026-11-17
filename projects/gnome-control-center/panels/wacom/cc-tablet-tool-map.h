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
#include <gtk/gtk.h>
#include "cc-wacom-device.h"
#include "cc-wacom-tool.h"

G_BEGIN_DECLS

#define CC_TYPE_TABLET_TOOL_MAP (cc_tablet_tool_map_get_type ())
G_DECLARE_FINAL_TYPE (CcTabletToolMap, cc_tablet_tool_map, CC, TABLET_TOOL_MAP, GObject)

CcTabletToolMap * cc_tablet_tool_map_new        (void);

GList           * cc_tablet_tool_map_list_tools  (CcTabletToolMap *map,
						  CcWacomDevice   *device);
CcWacomTool     * cc_tablet_tool_map_lookup_tool (CcTabletToolMap *map,
						  CcWacomDevice   *device,
						  guint64          serial);
void              cc_tablet_tool_map_add_relation (CcTabletToolMap *map,
						   CcWacomDevice   *device,
						   CcWacomTool     *tool);

G_END_DECLS
