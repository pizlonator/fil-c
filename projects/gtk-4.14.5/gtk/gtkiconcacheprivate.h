/* gtkiconcache.h
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkiconthemeprivate.h>

G_BEGIN_DECLS

/* These are (mostly, see below) the on disk flags for each icon file, don't change */
typedef enum
{
  ICON_CACHE_FLAG_NONE = 0,
  ICON_CACHE_FLAG_XPM_SUFFIX = 1 << 0,
  ICON_CACHE_FLAG_SVG_SUFFIX = 1 << 1,
  ICON_CACHE_FLAG_PNG_SUFFIX = 1 << 2,
  ICON_CACHE_FLAG_HAS_ICON_FILE = 1 << 3,

  /* This is a virtual flag we recreate in memory as the file format actually stores .symbolic.png as png */
  ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX = 1 << 4,
} IconCacheFlag;


typedef struct _GtkIconCache GtkIconCache;

GtkIconCache *gtk_icon_cache_new                        (const char   *data);
GtkIconCache *gtk_icon_cache_new_for_path               (const char   *path);
GHashTable   *gtk_icon_cache_list_icons_in_directory    (GtkIconCache *cache,
                                                         const char   *directory,
                                                         GtkStringSet *set);
GtkIconCache *gtk_icon_cache_ref                        (GtkIconCache *cache);
void          gtk_icon_cache_unref                      (GtkIconCache *cache);

G_END_DECLS

