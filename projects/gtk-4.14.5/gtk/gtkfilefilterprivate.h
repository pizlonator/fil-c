/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc
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

#include <gtk/gtkfilefilter.h>
#include <gdk/gdkconfig.h>

#ifdef GDK_WINDOWING_MACOS
#import <Foundation/Foundation.h>
#endif

G_BEGIN_DECLS

char ** _gtk_file_filter_get_as_patterns (GtkFileFilter      *filter);

#ifdef GDK_WINDOWING_MACOS
NSArray * _gtk_file_filter_get_as_pattern_nsstrings (GtkFileFilter *filter);
#endif


G_END_DECLS

