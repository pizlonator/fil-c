/* bg-wallpapers-source.c */
/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "bg-wallpapers-source.h"

#include "cc-background-item.h"
#include "cc-background-xml.h"

#include <cairo-gobject.h>
#include <gio/gio.h>

struct _BgWallpapersSource
{
  BgSource parent_instance;
  CcBackgroundXml *xml;
};

G_DEFINE_TYPE (BgWallpapersSource, bg_wallpapers_source, BG_TYPE_SOURCE)

static int
sort_func (gconstpointer a,
           gconstpointer b,
           gpointer      user_data)
{
  CcBackgroundItem *item_a;
  CcBackgroundItem *item_b;
  const char *name_a;
  const char *name_b;

  item_a = (CcBackgroundItem *) a;
  item_b = (CcBackgroundItem *) b;

  name_a = cc_background_item_get_name (item_a);
  name_b = cc_background_item_get_name (item_b);

  if (name_a && strcmp (name_a, "Default Background") == 0)
    return -1;
  if (name_b && strcmp (name_b, "Default Background") == 0)
    return 1;


  return strcmp (cc_background_item_get_name (item_a),
                 cc_background_item_get_name (item_b));
}

static void
load_wallpapers (gchar              *key,
                 CcBackgroundItem   *item,
                 BgWallpapersSource *source)
{
  GListStore *store = bg_source_get_liststore (BG_SOURCE (source));
  gboolean deleted;

  g_object_get (G_OBJECT (item), "is-deleted", &deleted, NULL);

  if (deleted)
    return;

  g_list_store_insert_sorted (store, item, sort_func, NULL);
}

static void
list_load_cb (GObject *source_object,
	      GAsyncResult *res,
	      gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  if (!cc_background_xml_load_list_finish (CC_BACKGROUND_XML (source_object), res, &error))
    g_warning ("Failed to load background list: %s", error->message);
}

static void
item_added (BgWallpapersSource *self,
            CcBackgroundItem   *item)
{
  load_wallpapers (NULL, item, self);
}

static void
load_default_bg (BgWallpapersSource *self)
{
  const char * const *system_data_dirs;
  guint i;

  /* FIXME We could do this nicer if we had the XML source in GSettings */

  system_data_dirs = g_get_system_data_dirs ();
  for (i = 0; system_data_dirs[i]; i++) {
    g_autofree gchar *filename = NULL;

    filename = g_build_filename (system_data_dirs[i],
				 "gnome-background-properties",
				 "adwaita.xml",
				 NULL);
    if (cc_background_xml_load_xml (self->xml, filename))
      break;
  }
}

static void
bg_wallpapers_source_constructed (GObject *object)
{
  BgWallpapersSource *self = BG_WALLPAPERS_SOURCE (object);

  G_OBJECT_CLASS (bg_wallpapers_source_parent_class)->constructed (object);

  g_signal_connect_object (G_OBJECT (self->xml), "added",
                           G_CALLBACK (item_added), self, G_CONNECT_SWAPPED);

  /* Try adding the default background first */
  load_default_bg (self);

  cc_background_xml_load_list_async (self->xml, NULL, list_load_cb, self);
}

static void
bg_wallpapers_source_dispose (GObject *object)
{
  BgWallpapersSource *self = BG_WALLPAPERS_SOURCE (object);

  g_clear_object (&self->xml);

  G_OBJECT_CLASS (bg_wallpapers_source_parent_class)->dispose (object);
}

static void
bg_wallpapers_source_init (BgWallpapersSource *self)
{
  self->xml = cc_background_xml_new ();
}

static void
bg_wallpapers_source_class_init (BgWallpapersSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = bg_wallpapers_source_constructed;
  object_class->dispose = bg_wallpapers_source_dispose;
}

BgWallpapersSource *
bg_wallpapers_source_new (void)
{
  return g_object_new (BG_TYPE_WALLPAPERS_SOURCE, NULL);
}
