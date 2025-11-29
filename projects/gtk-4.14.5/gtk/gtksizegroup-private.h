/* GTK - The GIMP Toolkit
 * gtksizegroup-private.h:
 * Copyright (C) 2000-2010 Red Hat Software
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

#include <gtk/gtksizegroup.h>

G_BEGIN_DECLS

GHashTable * _gtk_size_group_get_widget_peers (GtkWidget           *for_widget,
                                               GtkOrientation       orientation);

G_END_DECLS

