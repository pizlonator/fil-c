/*
 * Copyright © 2021 Red Hat, Inc.
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

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>
#include <IOSurface/IOSurface.h>

#include <cairo.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GDK_TYPE_MACOS_BUFFER (gdk_macos_buffer_get_type())

G_DECLARE_FINAL_TYPE (GdkMacosBuffer, gdk_macos_buffer, GDK, MACOS_BUFFER, GObject)

GdkMacosBuffer       *_gdk_macos_buffer_new              (int              width,
                                                          int              height,
                                                          double           device_scale,
                                                          int              bytes_per_element,
                                                          int              bits_per_pixel);
IOSurfaceRef          _gdk_macos_buffer_get_native       (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_lock             (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_unlock           (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_read_lock        (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_read_unlock      (GdkMacosBuffer  *self);
guint                 _gdk_macos_buffer_get_width        (GdkMacosBuffer  *self);
guint                 _gdk_macos_buffer_get_height       (GdkMacosBuffer  *self);
guint                 _gdk_macos_buffer_get_stride       (GdkMacosBuffer  *self);
double                _gdk_macos_buffer_get_device_scale (GdkMacosBuffer  *self);
const cairo_region_t *_gdk_macos_buffer_get_damage       (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_set_damage       (GdkMacosBuffer  *self,
                                                          cairo_region_t  *damage);
gpointer              _gdk_macos_buffer_get_data         (GdkMacosBuffer  *self);
gboolean              _gdk_macos_buffer_get_flipped      (GdkMacosBuffer  *self);
void                  _gdk_macos_buffer_set_flipped      (GdkMacosBuffer  *self,
                                                          gboolean         flipped);

G_END_DECLS

