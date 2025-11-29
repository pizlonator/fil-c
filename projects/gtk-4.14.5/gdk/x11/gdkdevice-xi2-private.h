/* gdkdevice-xi2-private.h: Private header for GdkX11DeviceXI2
 *
 * Copyright 2020  Red Hat
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkx11device-xi2.h"

G_BEGIN_DECLS

void gdk_x11_device_xi2_query_state (GdkDevice        *device,
                                     GdkSurface       *surface,
                                     double           *win_x,
                                     double           *win_y,
                                     GdkModifierType  *mask);

GdkX11DeviceType gdk_x11_device_xi2_get_device_type (GdkX11DeviceXI2 *device);
void             gdk_x11_device_xi2_set_device_type (GdkX11DeviceXI2  *device,
                                                     GdkX11DeviceType  type);

G_END_DECLS

