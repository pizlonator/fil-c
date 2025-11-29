/* GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc
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

#include <glib-object.h>
#include <cairo.h>

#include "gtkcsstypesprivate.h"
#include "gtkcssboxesprivate.h"
#include "gtktypes.h"

G_BEGIN_DECLS

void            gtk_css_style_snapshot_layout (GtkCssBoxes    *boxes,
                                               GtkSnapshot    *snapshot,
                                               int             x,
                                               int             y,
                                               PangoLayout    *layout);

void            gtk_css_style_snapshot_caret  (GtkCssBoxes    *boxes,
                                               GdkDisplay     *display,
                                               GtkSnapshot    *snapshot,
                                               int             x,
                                               int             y,
                                               PangoLayout    *layout,
                                               int             index,
                                               PangoDirection  direction);


G_END_DECLS

