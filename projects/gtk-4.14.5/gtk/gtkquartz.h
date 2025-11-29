/* gtkquartz.h: Utility functions used by the Quartz port
 *
 * Copyright (C) 2006 Imendio AB
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
 */

#pragma once

#import <Cocoa/Cocoa.h>

#include <cairo.h>
#include <glib.h>

G_BEGIN_DECLS

NSImage    *_gtk_quartz_create_image_from_surface (cairo_surface_t *surface);
const char *_gtk_get_datadir                      (void);
const char *_gtk_get_libdir                       (void);
const char *_gtk_get_localedir                    (void);
const char *_gtk_get_sysconfdir                   (void);
const char *_gtk_get_data_prefix                  (void);

G_END_DECLS

