/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2015 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include "config.h"

#include "gdkwaylandseat.h"
#include "gdk/gdkseatprivate.h"

#define GDK_WAYLAND_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))
#define GDK_IS_WAYLAND_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_SEAT))
#define GDK_WAYLAND_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))

struct _GdkWaylandSeatClass
{
  GdkSeatClass parent_class;
};

void gdk_wayland_seat_update_cursor_scale (GdkWaylandSeat *seat);

void gdk_wayland_seat_clear_touchpoints (GdkWaylandSeat *seat,
                                         GdkSurface     *surface);

