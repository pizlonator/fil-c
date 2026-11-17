/* cc-format-preview.c
 *
 * Copyright (C) 2013 Red Hat, Inc.
 * Copyright (C) 2020 System76, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Matthias Clasen
 *     Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_FORMAT_PREVIEW (cc_format_preview_get_type())
G_DECLARE_FINAL_TYPE (CcFormatPreview, cc_format_preview, CC, FORMAT_PREVIEW, GtkBox)

void cc_format_preview_set_region (CcFormatPreview *preview,
                                   const gchar     *region);

G_END_DECLS
