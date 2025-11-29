/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay.h"
#include "gdkmacosseat.h"

#include "gdkseatprivate.h"

G_BEGIN_DECLS

GdkSeat *_gdk_macos_seat_new (GdkMacosDisplay *display);

void _gdk_macos_seat_handle_tablet_tool_event (GdkMacosSeat *seat,
                                               NSEvent      *nsevent);

gboolean _gdk_macos_seat_get_tablet (GdkMacosSeat   *seat,
                                     GdkDevice     **device,
                                     GdkDeviceTool **tool);

double *_gdk_macos_seat_get_tablet_axes_from_nsevent (GdkMacosSeat *seat,
                                                      NSEvent      *nsevent);

G_END_DECLS

