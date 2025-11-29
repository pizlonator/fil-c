/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"
#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>
#include <errno.h>

typedef struct _GdkBroadwayKeymap   GdkBroadwayKeymap;
typedef struct _GdkKeymapClass GdkBroadwayKeymapClass;

#define GDK_TYPE_BROADWAY_KEYMAP          (gdk_broadway_keymap_get_type ())
#define GDK_BROADWAY_KEYMAP(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_KEYMAP, GdkBroadwayKeymap))
#define GDK_IS_BROADWAY_KEYMAP(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_KEYMAP))

static GType gdk_broadway_keymap_get_type (void);

typedef struct _DirectionCacheEntry DirectionCacheEntry;

struct _GdkBroadwayKeymap
{
  GdkKeymap     parent_instance;
};

struct _GdkBroadwayKeymapClass
{
  GdkKeymapClass keymap_class;
};

G_DEFINE_TYPE (GdkBroadwayKeymap, gdk_broadway_keymap, GDK_TYPE_KEYMAP)

static void  gdk_broadway_keymap_finalize   (GObject           *object);

static void
gdk_broadway_keymap_init (GdkBroadwayKeymap *keymap)
{
}

static void
gdk_broadway_keymap_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_broadway_keymap_parent_class)->finalize (object);
}

GdkKeymap*
_gdk_broadway_display_get_keymap (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  if (!broadway_display->keymap)
    broadway_display->keymap = g_object_new (gdk_broadway_keymap_get_type (), NULL);

  broadway_display->keymap->display = display;

  return broadway_display->keymap;
}

static PangoDirection
gdk_broadway_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_broadway_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_broadway_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_broadway_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_broadway_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_broadway_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                            guint          keyval,
                                            GArray        *retval)
{
  GdkKeymapKey key;

  key.keycode = keyval;
  key.group = 0;
  key.level = 0;

  g_array_append_val (retval, key);

  return TRUE;
}

static gboolean
gdk_broadway_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
					     guint          hardware_keycode,
					     GdkKeymapKey **keys,
					     guint        **keyvals,
					     int           *n_entries)
{
  if (n_entries)
    *n_entries = 1;
  if (keys)
    {
      *keys = g_new0 (GdkKeymapKey, 1);
      (*keys)->keycode = hardware_keycode;
    }
  if (keyvals)
    {
      *keyvals = g_new0 (guint, 1);
      (*keyvals)[0] = hardware_keycode;
    }
  return TRUE;
}

static guint
gdk_broadway_keymap_lookup_key (GdkKeymap          *keymap,
				const GdkKeymapKey *key)
{
  return key->keycode;
}


static gboolean
gdk_broadway_keymap_translate_keyboard_state (GdkKeymap       *keymap,
					      guint            hardware_keycode,
					      GdkModifierType  state,
					      int              group,
					      guint           *keyval,
					      int             *effective_group,
					      int             *level,
					      GdkModifierType *consumed_modifiers)
{
  if (keyval)
    *keyval = hardware_keycode;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  return TRUE;
}

static void
gdk_broadway_keymap_class_init (GdkBroadwayKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_broadway_keymap_finalize;

  keymap_class->get_direction = gdk_broadway_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_broadway_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_broadway_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_broadway_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_broadway_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_broadway_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_broadway_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_broadway_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_broadway_keymap_translate_keyboard_state;
}

