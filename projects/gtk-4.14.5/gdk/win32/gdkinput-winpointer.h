/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 the GTK team
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

#include "winpointer.h"

gboolean gdk_winpointer_initialize (void);

void gdk_winpointer_initialize_surface (GdkSurface *surface);
void gdk_winpointer_finalize_surface (GdkSurface *surface);

typedef void
(*crossing_cb_t)(GdkDevice *physical_device,
                 GdkSurface *surface,
                 POINT *screen_pt,
                 guint32 time_);

gboolean gdk_winpointer_should_forward_message (MSG *msg);
void     gdk_winpointer_input_events (GdkSurface *surface,
                                      crossing_cb_t crossing_cb,
                                      MSG *msg);
gboolean gdk_winpointer_get_message_info (MSG *msg,
                                          GdkDevice **device,
                                          guint32 *time_);
void     gdk_winpointer_interaction_ended (MSG *msg);

