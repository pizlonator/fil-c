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

#include "gdkmacossurface-private.h"

G_BEGIN_DECLS

typedef struct _GdkMacosPopupSurface      GdkMacosPopupSurface;
typedef struct _GdkMacosPopupSurfaceClass GdkMacosPopupSurfaceClass;

#define GDK_TYPE_MACOS_POPUP_SURFACE       (_gdk_macos_popup_surface_get_type())
#define GDK_MACOS_POPUP_SURFACE(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MACOS_POPUP_SURFACE, GdkMacosPopupSurface))
#define GDK_IS_MACOS_POPUP_SURFACE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MACOS_POPUP_SURFACE))

GType            _gdk_macos_popup_surface_get_type           (void);
GdkMacosSurface *_gdk_macos_popup_surface_new                (GdkMacosDisplay      *display,
                                                              GdkSurface           *parent,
                                                              GdkFrameClock        *frame_clock);
void             _gdk_macos_popup_surface_attach_to_parent   (GdkMacosPopupSurface *self);
void             _gdk_macos_popup_surface_detach_from_parent (GdkMacosPopupSurface *self);
void             _gdk_macos_popup_surface_reposition         (GdkMacosPopupSurface *self);

G_END_DECLS

