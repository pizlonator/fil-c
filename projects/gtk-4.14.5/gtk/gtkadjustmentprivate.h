/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc
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


#include <gtk/gtkadjustment.h>


G_BEGIN_DECLS

void gtk_adjustment_enable_animation (GtkAdjustment *adjustment,
                                      GdkFrameClock *clock,
                                      guint          duration);
guint gtk_adjustment_get_animation_duration (GtkAdjustment *adjustment);
void gtk_adjustment_animate_to_value (GtkAdjustment *adjustment,
                                      double         value);
double gtk_adjustment_get_target_value (GtkAdjustment *adjustment);

gboolean gtk_adjustment_is_animating (GtkAdjustment *adjustment);

G_END_DECLS


