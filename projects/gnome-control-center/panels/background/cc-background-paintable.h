/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2021 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <gtk/gtk.h>

#include "bg-source.h"
#include "cc-background-item.h"
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_PAINTABLE (cc_background_paintable_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundPaintable, cc_background_paintable, CC, BACKGROUND_PAINTABLE, GObject)

typedef enum {
    CC_BACKGROUND_PAINT_LIGHT = 1 << 0,
    CC_BACKGROUND_PAINT_DARK  = 1 << 1
} CcBackgroundPaintFlags;

#define CC_BACKGROUND_PAINT_LIGHT_DARK (CC_BACKGROUND_PAINT_LIGHT |	\
                                        CC_BACKGROUND_PAINT_DARK)

CcBackgroundPaintable * cc_background_paintable_new (GnomeDesktopThumbnailFactory *thumbnail_factory,
                                                     CcBackgroundItem             *item,
                                                     CcBackgroundPaintFlags        paint_flags,
                                                     int                           width,
                                                     int                           height,
                                                     int                           scale_factor);

G_END_DECLS
