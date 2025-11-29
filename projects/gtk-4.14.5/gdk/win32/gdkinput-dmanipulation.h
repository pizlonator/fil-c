/* gdkinput-dmanipulation.h
 *
 * Copyright © 2022 the GTK team
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void gdk_dmanipulation_initialize (void);

void gdk_dmanipulation_initialize_surface (GdkSurface *surface);
void gdk_dmanipulation_finalize_surface   (GdkSurface *surface);

void gdk_dmanipulation_maybe_add_contact  (GdkSurface *surface,
                                           MSG        *msg);

