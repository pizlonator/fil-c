/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Red Hat, Inc.
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

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gdesktop-enums.h>
#include <gnome-bg/gnome-bg.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_ITEM (cc_background_item_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundItem, cc_background_item, CC, BACKGROUND_ITEM, GObject)

typedef enum {
	CC_BACKGROUND_ITEM_HAS_SHADING   = 1 << 0,
	CC_BACKGROUND_ITEM_HAS_PLACEMENT = 1 << 1,
	CC_BACKGROUND_ITEM_HAS_PCOLOR    = 1 << 2,
	CC_BACKGROUND_ITEM_HAS_SCOLOR    = 1 << 3,
	CC_BACKGROUND_ITEM_HAS_URI       = 1 << 4,
	CC_BACKGROUND_ITEM_HAS_URI_DARK  = 1 << 5
} CcBackgroundItemFlags;

#define CC_BACKGROUND_ITEM_HAS_ALL (CC_BACKGROUND_ITEM_HAS_SHADING &	\
				    CC_BACKGROUND_ITEM_HAS_PLACEMENT &	\
				    CC_BACKGROUND_ITEM_HAS_PCOLOR &	\
				    CC_BACKGROUND_ITEM_HAS_SCOLOR &	\
				    CC_BACKGROUND_ITEM_HAS_FNAME)

CcBackgroundItem * cc_background_item_new                 (const char                   *uri);
CcBackgroundItem * cc_background_item_copy                (CcBackgroundItem             *item);
gboolean           cc_background_item_load                (CcBackgroundItem *item,
                                                           GFileInfo        *info);
gboolean           cc_background_item_changes_with_time   (CcBackgroundItem             *item);
gboolean           cc_background_item_has_dark_version    (CcBackgroundItem             *item);

GdkPixbuf *        cc_background_item_get_thumbnail       (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height,
                                                           int                           scale_factor,
                                                           gboolean                      dark);
GdkPixbuf *        cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height,
                                                           int                           scale_factor,
                                                           int                           frame,
                                                           gboolean                      dark);

GDesktopBackgroundStyle   cc_background_item_get_placement  (CcBackgroundItem *item);
GDesktopBackgroundShading cc_background_item_get_shading    (CcBackgroundItem *item);
const char *              cc_background_item_get_uri        (CcBackgroundItem *item);
const char *              cc_background_item_get_uri_dark   (CcBackgroundItem *item);
const char *              cc_background_item_get_source_url (CcBackgroundItem *item);
const char *              cc_background_item_get_source_xml (CcBackgroundItem *item);
CcBackgroundItemFlags     cc_background_item_get_flags      (CcBackgroundItem *item);
const char *              cc_background_item_get_pcolor     (CcBackgroundItem *item);
const char *              cc_background_item_get_scolor     (CcBackgroundItem *item);
const char *              cc_background_item_get_name       (CcBackgroundItem *item);
const char *              cc_background_item_get_size       (CcBackgroundItem *item);
gboolean                  cc_background_item_get_needs_download (CcBackgroundItem *item);
guint64                   cc_background_item_get_modified   (CcBackgroundItem *item);

gboolean                  cc_background_item_compare        (CcBackgroundItem *saved,
							     CcBackgroundItem *configured);
void                      cc_background_item_dump           (CcBackgroundItem *item);

G_END_DECLS
