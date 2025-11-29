/* gtkatspirootprivate.h: AT-SPI root object
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

#include <gio/gio.h>

#include "gtkatcontextprivate.h"
#include "gtkatspiprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_AT_SPI_ROOT (gtk_at_spi_root_get_type())

G_DECLARE_FINAL_TYPE (GtkAtSpiRoot, gtk_at_spi_root, GTK, AT_SPI_ROOT, GObject)

GtkAtSpiRoot *
gtk_at_spi_root_new (const char *bus_address);

typedef void (* GtkAtSpiRootRegisterFunc) (GtkAtSpiRoot *root,
                                           GtkAtSpiContext *context);

void
gtk_at_spi_root_queue_register (GtkAtSpiRoot *self,
                                GtkAtSpiContext *context,
                                GtkAtSpiRootRegisterFunc func);

void
gtk_at_spi_root_unregister (GtkAtSpiRoot *self,
                            GtkAtSpiContext *context);

GDBusConnection *
gtk_at_spi_root_get_connection (GtkAtSpiRoot *self);

GtkAtSpiCache *
gtk_at_spi_root_get_cache (GtkAtSpiRoot *self);

const char *
gtk_at_spi_root_get_base_path (GtkAtSpiRoot *self);

GVariant *
gtk_at_spi_root_to_ref (GtkAtSpiRoot *self);

void
gtk_at_spi_root_child_changed (GtkAtSpiRoot             *self,
                               GtkAccessibleChildChange  change,
                               GtkAccessible            *child);

G_END_DECLS
