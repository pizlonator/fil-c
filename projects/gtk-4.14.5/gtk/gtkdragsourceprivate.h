/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Alexander Mikhaylenko
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

#include "gtkdragsource.h"

G_BEGIN_DECLS

gboolean gtk_drag_check_threshold_double (GtkWidget *widget,
                                          double     start_x,
                                          double     start_y,
                                          double     current_x,
                                          double     current_y);

G_END_DECLS

