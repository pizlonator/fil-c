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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>

#include "gdk.h"
#include "gdkwayland.h"

#include "gdkprivate-wayland.h"
#include "gdkprivate.h"
#include "gdkkeysprivate.h"

#include <xkbcommon/xkbcommon.h>

typedef struct _GdkWaylandKeymap          GdkWaylandKeymap;
typedef struct _GdkWaylandKeymapClass     GdkWaylandKeymapClass;

struct _GdkWaylandKeymap
{
  GdkKeymap parent_instance;

  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  PangoDirection *direction;
  gboolean bidi;
};

struct _GdkWaylandKeymapClass
{
  GdkKeymapClass parent_class;
};

#define GDK_TYPE_WAYLAND_KEYMAP          (_gdk_wayland_keymap_get_type ())
#define GDK_WAYLAND_KEYMAP(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_KEYMAP, GdkWaylandKeymap))
#define GDK_IS_WAYLAND_KEYMAP(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_KEYMAP))

GType _gdk_wayland_keymap_get_type (void);

G_DEFINE_TYPE (GdkWaylandKeymap, _gdk_wayland_keymap, GDK_TYPE_KEYMAP)

static void
gdk_wayland_keymap_finalize (GObject *object)
{
  GdkWaylandKeymap *keymap = GDK_WAYLAND_KEYMAP (object);

  xkb_keymap_unref (keymap->xkb_keymap);
  xkb_state_unref (keymap->xkb_state);
  g_free (keymap->direction);

  G_OBJECT_CLASS (_gdk_wayland_keymap_parent_class)->finalize (object);
}

static PangoDirection
gdk_wayland_keymap_get_direction (GdkKeymap *keymap)
{
  GdkWaylandKeymap *keymap_wayland = GDK_WAYLAND_KEYMAP (keymap);
  int i;

  for (i = 0; i < xkb_keymap_num_layouts (keymap_wayland->xkb_keymap); i++)
    {
      if (xkb_state_layout_index_is_active (keymap_wayland->xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE))
        return keymap_wayland->direction[i];
    }

  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_wayland_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  GdkWaylandKeymap *keymap_wayland = GDK_WAYLAND_KEYMAP (keymap);

  return keymap_wayland->bidi;
}

static gboolean
gdk_wayland_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return xkb_state_led_name_is_active (GDK_WAYLAND_KEYMAP (keymap)->xkb_state,
                                       XKB_LED_NAME_CAPS);
}

static gboolean
gdk_wayland_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return xkb_state_led_name_is_active (GDK_WAYLAND_KEYMAP (keymap)->xkb_state,
                                       XKB_LED_NAME_NUM);
}

static gboolean
gdk_wayland_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  return xkb_state_led_name_is_active (GDK_WAYLAND_KEYMAP (keymap)->xkb_state,
                                       XKB_LED_NAME_SCROLL);
}

static gboolean
gdk_wayland_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                           guint      keyval,
                                           GArray    *retval)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
  guint keycode;
  xkb_keycode_t min_keycode, max_keycode;
  guint len = retval->len;

  min_keycode = xkb_keymap_min_keycode (xkb_keymap);
  max_keycode = xkb_keymap_max_keycode (xkb_keymap);
  for (keycode = min_keycode; keycode < max_keycode; keycode++)
    {
      int num_layouts, layout;
      num_layouts = xkb_keymap_num_layouts_for_key (xkb_keymap, keycode);
      for (layout = 0; layout < num_layouts; layout++)
        {
          int num_levels, level;
          num_levels = xkb_keymap_num_levels_for_key (xkb_keymap, keycode, layout);
          for (level = 0; level < num_levels; level++)
            {
              const xkb_keysym_t *syms;
              int num_syms, sym;
              num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap, keycode, layout, level, &syms);
              for (sym = 0; sym < num_syms; sym++)
                {
                  if (syms[sym] == keyval)
                    {
                      GdkKeymapKey key;

                      key.keycode = keycode;
                      key.group = layout;
                      key.level = level;

                      g_array_append_val (retval, key);
                    }
                }
            }
        }
    }

  return retval->len > len;
}

static gboolean
gdk_wayland_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
					    guint          hardware_keycode,
					    GdkKeymapKey **keys,
					    guint        **keyvals,
					    int           *n_entries)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
  int num_layouts, layout;
  int num_entries;
  int i;

  num_layouts = xkb_keymap_num_layouts_for_key (xkb_keymap, hardware_keycode);

  num_entries = 0;
  for (layout = 0; layout < num_layouts; layout++)
    num_entries += xkb_keymap_num_levels_for_key (xkb_keymap, hardware_keycode,  layout);

 if (n_entries)
    *n_entries = num_entries;
  if (keys)
    *keys = g_new0 (GdkKeymapKey, num_entries);
  if (keyvals)
    *keyvals = g_new0 (guint, num_entries);

  i = 0;
  for (layout = 0; layout < num_layouts; layout++)
    {
      int num_levels, level;
      num_levels = xkb_keymap_num_levels_for_key (xkb_keymap, hardware_keycode, layout);
      for (level = 0; level < num_levels; level++)
        {
          const xkb_keysym_t *syms;
          int num_syms;
          num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap, hardware_keycode, layout, level, &syms);
          if (keys)
            {
              (*keys)[i].keycode = hardware_keycode;
              (*keys)[i].group = layout;
              (*keys)[i].level = level;
            }
          if (keyvals && num_syms > 0)
            (*keyvals)[i] = syms[0];

          i++;
        }
    }

  return num_entries > 0;
}

static guint
gdk_wayland_keymap_lookup_key (GdkKeymap          *keymap,
			       const GdkKeymapKey *key)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
  const xkb_keysym_t *syms;
  int num_syms;

  num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap,
                                               key->keycode,
                                               key->group,
                                               key->level,
                                               &syms);
  if (num_syms > 0)
    return syms[0];
  else
    return XKB_KEY_NoSymbol;
}

static guint32
get_xkb_modifiers (struct xkb_keymap *xkb_keymap,
                   GdkModifierType    state)
{
  guint32 mods = 0;

  if (state & GDK_SHIFT_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT);
  if (state & GDK_LOCK_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS);
  if (state & GDK_CONTROL_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL);
  if (state & GDK_ALT_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT);
  if (state & GDK_SUPER_MASK)
    mods |= (1 << xkb_keymap_mod_get_index (xkb_keymap, "Super") | 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO));
  if (state & GDK_HYPER_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Hyper");
  if (state & GDK_META_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Meta");

  return mods;
}

static GdkModifierType
get_gdk_modifiers (struct xkb_keymap *xkb_keymap,
                   guint32            mods)
{
  GdkModifierType state = 0;

  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT)))
    state |= GDK_SHIFT_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS)))
    state |= GDK_LOCK_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL)))
    state |= GDK_CONTROL_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT)))
    state |= GDK_ALT_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO)))
    state |= GDK_SUPER_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Super")))
    state |= GDK_SUPER_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Hyper")))
    state |= GDK_HYPER_MASK;
  /* GTK treats MOD1 as a synonym for Alt, and does not expect it to
   * be mapped around, so we should avoid adding GDK_META_MASK if MOD1
   * is already included to avoid confusing GTK and applications that
   * rely on that behavior.
   */
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Meta")) &&
      (state & GDK_ALT_MASK) == 0)
    state |= GDK_META_MASK;

  return state;
}

GdkModifierType
gdk_wayland_keymap_get_gdk_modifiers (GdkKeymap *keymap,
                                      guint32    mods)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;

  return get_gdk_modifiers (xkb_keymap, mods);
}

static gboolean
gdk_wayland_keymap_translate_keyboard_state (GdkKeymap       *keymap,
					     guint            hardware_keycode,
					     GdkModifierType  state,
					     int              group,
					     guint           *keyval,
					     int             *effective_group,
					     int             *effective_level,
					     GdkModifierType *consumed_modifiers)
{
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  guint32 modifiers;
  guint32 consumed;
  xkb_layout_index_t layout;
  xkb_level_index_t level;
  xkb_keysym_t sym;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);

  xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;

  modifiers = get_xkb_modifiers (xkb_keymap, state);

  xkb_state = xkb_state_new (xkb_keymap);

  xkb_state_update_mask (xkb_state, modifiers, 0, 0, group, 0, 0);

  layout = xkb_state_key_get_layout (xkb_state, hardware_keycode);
  level = xkb_state_key_get_level (xkb_state, hardware_keycode, layout);
  sym = xkb_state_key_get_one_sym (xkb_state, hardware_keycode);
  consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (xkb_state, hardware_keycode, modifiers);

  xkb_state_unref (xkb_state);

  if (keyval)
    *keyval = sym;
  if (effective_group)
    *effective_group = layout;
  if (effective_level)
    *effective_level = level;
  if (consumed_modifiers)
    *consumed_modifiers = get_gdk_modifiers (xkb_keymap, consumed);

  return (sym != XKB_KEY_NoSymbol);
}

static guint
gdk_wayland_keymap_get_modifier_state (GdkKeymap *keymap)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
  struct xkb_state *xkb_state = GDK_WAYLAND_KEYMAP (keymap)->xkb_state;
  xkb_mod_mask_t mods;

  mods = xkb_state_serialize_mods (xkb_state, XKB_STATE_MODS_EFFECTIVE);

  return get_gdk_modifiers (xkb_keymap, mods);
}

static void
_gdk_wayland_keymap_class_init (GdkWaylandKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_wayland_keymap_finalize;

  keymap_class->get_direction = gdk_wayland_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_wayland_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_wayland_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_wayland_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_wayland_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_wayland_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_wayland_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_wayland_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_wayland_keymap_translate_keyboard_state;
  keymap_class->get_modifier_state = gdk_wayland_keymap_get_modifier_state;
}

static void
_gdk_wayland_keymap_init (GdkWaylandKeymap *keymap)
{
}

static void
update_direction (GdkWaylandKeymap *keymap)
{
  int num_layouts;
  int i;
  int *rtl;
  xkb_keycode_t min_keycode, max_keycode;
  guint key;
  gboolean have_rtl, have_ltr;

  num_layouts = xkb_keymap_num_layouts (keymap->xkb_keymap);

  keymap->direction = g_renew (PangoDirection, keymap->direction, num_layouts);
  rtl = g_newa (int, num_layouts);
  for (i = 0; i < num_layouts; i++)
    rtl[i] = 0;

  min_keycode = xkb_keymap_min_keycode (keymap->xkb_keymap);
  max_keycode = xkb_keymap_max_keycode (keymap->xkb_keymap);
  for (key = min_keycode; key < max_keycode; key++)
    {
       int layouts, layout;

       layouts = xkb_keymap_num_layouts_for_key (keymap->xkb_keymap, key);
       g_assert (layouts <= num_layouts);
       for (layout = 0; layout < layouts; layout++)
         {
           const xkb_keysym_t *syms;
           int num_syms;
           int sym;

           num_syms = xkb_keymap_key_get_syms_by_level (keymap->xkb_keymap, key, layout, 0, &syms);
           for (sym = 0; sym < num_syms; sym++)
             {
               PangoDirection dir;

               dir = gdk_unichar_direction (xkb_keysym_to_utf32 (syms[sym]));
               switch (dir)
                 {
                 case PANGO_DIRECTION_RTL:
                   rtl[layout]++;
                   break;
                 case PANGO_DIRECTION_LTR:
                   rtl[layout]--;
                   break;
                 case PANGO_DIRECTION_TTB_LTR:
                 case PANGO_DIRECTION_TTB_RTL:
                 case PANGO_DIRECTION_WEAK_LTR:
                 case PANGO_DIRECTION_WEAK_RTL:
                 case PANGO_DIRECTION_NEUTRAL:
                 default:
                   break;
                 }
             }
         }
    }

  have_rtl = have_ltr = FALSE;
  for (i = 0; i < num_layouts; i++)
    {
      if (rtl[i] > 0)
        {
          keymap->direction[i] = PANGO_DIRECTION_RTL;
          have_rtl = TRUE;
        }
      else
        {
          keymap->direction[i] = PANGO_DIRECTION_LTR;
          have_ltr = TRUE;
        }
    }

  if (have_rtl && have_ltr)
    keymap->bidi = TRUE;
}

GdkKeymap *
_gdk_wayland_keymap_new (GdkDisplay *display)
{
  GdkWaylandKeymap *keymap;
  struct xkb_context *context;
  struct xkb_rule_names names;

  keymap = g_object_new (_gdk_wayland_keymap_get_type(), NULL);
  GDK_KEYMAP (keymap)->display = display;

  context = xkb_context_new (0);

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";
  keymap->xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
  keymap->xkb_state = xkb_state_new (keymap->xkb_keymap);
  xkb_context_unref (context);

  update_direction (keymap);

  return GDK_KEYMAP (keymap);
}

static void
print_modifiers (GdkDisplay        *display,
                 struct xkb_keymap *keymap)
{
  int i, j;
  uint32_t real;
  struct xkb_state *state;
  GString *str;

  if (!GDK_DISPLAY_DEBUG_CHECK (display, INPUT))
    return;

  str = g_string_new ("");

  g_string_append (str, "modifiers:\n");
  for (i = 0; i < xkb_keymap_num_mods (keymap); i++)
    g_string_append_printf (str, "%s ", xkb_keymap_mod_get_name (keymap, i));
  g_string_append (str, "\n\n");

  g_string_append (str, "modifier mapping\n");
  state = xkb_state_new (keymap);
  for (i = 0; i < 8; i++)
    {
      gboolean need_arrow = TRUE;
      g_string_append_printf (str, "%s ", xkb_keymap_mod_get_name (keymap, i));
      for (j = 8; j < xkb_keymap_num_mods (keymap); j++)
        {
          xkb_state_update_mask (state, 1 << j, 0, 0, 0, 0, 0);
          real = xkb_state_serialize_mods (state, XKB_STATE_MODS_EFFECTIVE);
          if (real & (1 << i))
            {
              if (need_arrow)
                {
                  g_string_append (str, "-> ");
                  need_arrow = FALSE;
                }
              g_string_append_printf (str, "%s ", xkb_keymap_mod_get_name (keymap, j));
            }
        }
    }

  gdk_debug_message ("%s", str->str);

  g_string_free (str, TRUE);

  xkb_state_unref (state);
}

void
_gdk_wayland_keymap_update_from_fd (GdkKeymap *keymap,
                                    uint32_t   format,
                                    uint32_t   fd,
                                    uint32_t   size)
{
  GdkWaylandKeymap *keymap_wayland = GDK_WAYLAND_KEYMAP (keymap);
  struct xkb_context *context;
  struct xkb_keymap *xkb_keymap;
  char *map_str;

  context = xkb_context_new (0);

  map_str = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map_str == MAP_FAILED)
    {
      close(fd);
      return;
    }

  GDK_DISPLAY_DEBUG (keymap->display, INPUT, "keymap:\n%s", map_str);

  xkb_keymap = xkb_keymap_new_from_string (context, map_str, format, 0);
  munmap (map_str, size);
  close (fd);

  if (!xkb_keymap)
    {
      g_warning ("Got invalid keymap from compositor, keeping previous/default one");
      xkb_context_unref (context);
      return;
    }

  print_modifiers (keymap->display, xkb_keymap);

  xkb_keymap_unref (keymap_wayland->xkb_keymap);
  keymap_wayland->xkb_keymap = xkb_keymap;

  xkb_state_unref (keymap_wayland->xkb_state);
  keymap_wayland->xkb_state = xkb_state_new (keymap_wayland->xkb_keymap);

  xkb_context_unref (context);

  update_direction (keymap_wayland);
}

struct xkb_keymap *
_gdk_wayland_keymap_get_xkb_keymap (GdkKeymap *keymap)
{
  return GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
}

struct xkb_state *
_gdk_wayland_keymap_get_xkb_state (GdkKeymap *keymap)
{
  return GDK_WAYLAND_KEYMAP (keymap)->xkb_state;
}

gboolean
_gdk_wayland_keymap_key_is_modifier (GdkKeymap *keymap,
                                     guint      keycode)
{
  struct xkb_keymap *xkb_keymap = GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
  struct xkb_state *xkb_state;
  gboolean is_modifier;

  is_modifier = FALSE;

  xkb_state = xkb_state_new (xkb_keymap);

  if (xkb_state_update_key (xkb_state, keycode, XKB_KEY_DOWN) & XKB_STATE_MODS_EFFECTIVE)
    is_modifier = TRUE;

  xkb_state_unref (xkb_state);

  return is_modifier;
}
