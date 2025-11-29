/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Benjamin Otte
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif


#include <gtk/gtkscrollinfo.h>

G_BEGIN_DECLS

void                    gtk_scroll_info_compute_scroll          (GtkScrollInfo                  *self,
                                                                 const cairo_rectangle_int_t    *area,
                                                                 const cairo_rectangle_int_t    *viewport,
                                                                 int                            *out_x,
                                                                 int                            *out_y);
int                     gtk_scroll_info_compute_for_orientation (GtkScrollInfo                  *self,
                                                                 GtkOrientation                  orientation,
                                                                 int                             area_origin,
                                                                 int                             area_size,
                                                                 int                             viewport_origin,
                                                                 int                             viewport_size);

G_END_DECLS

