/* gtkatspitextbufferprivate.h: Utilities for GtkTextBuffer and AT-SPI
 *
 * Copyright 2020 Red Hat, Inc
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

#include "gtkatspiprivate.h"
#include "gtktextview.h"

G_BEGIN_DECLS

char *gtk_text_view_get_text_before (GtkTextView           *view,
                                     int                    offset,
                                     AtspiTextBoundaryType  boundary_type,
                                     int                   *start_offset,
                                     int                   *end_offset);
char *gtk_text_view_get_text_at     (GtkTextView           *view,
                                     int                    offset,
                                     AtspiTextBoundaryType  boundary_type,
                                     int                   *start_offset,
                                     int                   *end_offset);
char *gtk_text_view_get_text_after  (GtkTextView           *view,
                                     int                    offset,
                                     AtspiTextBoundaryType  boundary_type,
                                     int                   *start_offset,
                                     int                   *end_offset);
char *gtk_text_view_get_string_at   (GtkTextView           *view,
                                     int                    offset,
                                     AtspiTextGranularity   granularity,
                                     int                   *start_offset,
                                     int                   *end_offset);

G_END_DECLS
