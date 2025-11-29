/* gtkatspisocketprivate.h: AT-SPI socket object
 *
 * Copyright 2024 GNOME Foundation Inc.
 *           2024 Igalia S.L.
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
#include "gtkaccessible.h"
#include "gtkatspisocket.h"

G_BEGIN_DECLS

GVariant * gtk_at_spi_socket_to_ref (GtkAtSpiSocket *self);
void gtk_at_spi_socket_embed (GtkAtSpiSocket  *self,
                              GDBusConnection *connection);

G_END_DECLS
