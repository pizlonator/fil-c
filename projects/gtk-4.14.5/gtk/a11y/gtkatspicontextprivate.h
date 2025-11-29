/* gtkatspicontextprivate.h: AT-SPI GtkATContext implementation
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkatcontextprivate.h"
#include "gtkatspiprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_AT_SPI_CONTEXT (gtk_at_spi_context_get_type())

G_DECLARE_FINAL_TYPE (GtkAtSpiContext, gtk_at_spi_context, GTK, AT_SPI_CONTEXT, GtkATContext)

GtkATContext *
gtk_at_spi_create_context (GtkAccessibleRole  accessible_role,
                           GtkAccessible     *accessible,
                           GdkDisplay        *display);

const char *
gtk_at_spi_context_get_context_path (GtkAtSpiContext *self);

GVariant *
gtk_at_spi_context_to_ref (GtkAtSpiContext *self);

GtkAtSpiRoot *
gtk_at_spi_context_get_root (GtkAtSpiContext *self);

GVariant *
gtk_at_spi_context_get_parent_ref (GtkAtSpiContext *self);

GVariant *
gtk_at_spi_context_get_interfaces (GtkAtSpiContext *self);

GVariant *
gtk_at_spi_context_get_states (GtkAtSpiContext *self);

int
gtk_at_spi_context_get_index_in_parent (GtkAtSpiContext *self);

int
gtk_at_spi_context_get_child_count (GtkAtSpiContext *self);

G_END_DECLS
