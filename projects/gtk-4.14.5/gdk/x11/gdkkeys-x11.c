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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include "gdkkeys-x11.h"

#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>

/* OSF-4.0 is apparently missing this macro
 */
#  ifndef XkbKeySymEntry
#    define XkbKeySymEntry(d,k,sl,g) \
        (XkbKeySym(d,k,((XkbKeyGroupsWidth(d,k)*(g))+(sl))))
#  endif
#endif /* HAVE_XKB */

typedef struct _DirectionCacheEntry DirectionCacheEntry;

struct _DirectionCacheEntry
{
  guint serial;
  PangoDirection direction;
  Atom group_atom;
};

struct _GdkX11Keymap
{
  GdkKeymap     parent_instance;

  int min_keycode;
  int max_keycode;
  KeySym* keymap;
  int keysyms_per_keycode;
  XModifierKeymap* mod_keymap;
  guint lock_keysym;
  GdkModifierType group_switch_mask;
  GdkModifierType num_lock_mask;
  GdkModifierType scroll_lock_mask;
  GdkModifierType modmap[8];
  PangoDirection current_direction;
  guint have_direction    : 1;
  guint have_lock_state   : 1;
  guint caps_lock_state   : 1;
  guint num_lock_state    : 1;
  guint scroll_lock_state : 1;
  guint modifier_state;
  guint current_serial;

#ifdef HAVE_XKB
  XkbDescPtr xkb_desc;
  /* We cache the directions */
  Atom current_group_atom;
  guint current_cache_serial;
  /* A cache of size four should be more than enough, people usually
   * have two groups around, and the xkb limit is four.  It still
   * works correct for more than four groups.  It's just the
   * cache.
   */
  DirectionCacheEntry group_direction_cache[4];
#endif
};

struct _GdkX11KeymapClass
{
  GdkKeymapClass parent_class;
};

#define KEYMAP_USE_XKB(keymap) GDK_X11_DISPLAY ((keymap)->display)->use_xkb
#define KEYMAP_XDISPLAY(keymap) GDK_DISPLAY_XDISPLAY ((keymap)->display)

G_DEFINE_TYPE (GdkX11Keymap, gdk_x11_keymap, GDK_TYPE_KEYMAP)

static void
gdk_x11_keymap_init (GdkX11Keymap *keymap)
{
  keymap->min_keycode = 0;
  keymap->max_keycode = 0;

  keymap->keymap = NULL;
  keymap->keysyms_per_keycode = 0;
  keymap->mod_keymap = NULL;

  keymap->num_lock_mask = 0;
  keymap->scroll_lock_mask = 0;
  keymap->group_switch_mask = 0;
  keymap->lock_keysym = GDK_KEY_Caps_Lock;
  keymap->have_direction = FALSE;
  keymap->have_lock_state = FALSE;
  keymap->current_serial = 0;

#ifdef HAVE_XKB
  keymap->xkb_desc = NULL;
  keymap->current_group_atom = 0;
  keymap->current_cache_serial = 0;
#endif

}

static void
gdk_x11_keymap_finalize (GObject *object)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (object);

  if (keymap_x11->keymap)
    XFree (keymap_x11->keymap);

  if (keymap_x11->mod_keymap)
    XFreeModifiermap (keymap_x11->mod_keymap);

#ifdef HAVE_XKB
  if (keymap_x11->xkb_desc)
    XkbFreeKeyboard (keymap_x11->xkb_desc, XkbAllComponentsMask, True);
#endif

  G_OBJECT_CLASS (gdk_x11_keymap_parent_class)->finalize (object);
}

static inline void
update_keyrange (GdkX11Keymap *keymap_x11)
{
  if (keymap_x11->max_keycode == 0)
    XDisplayKeycodes (KEYMAP_XDISPLAY (GDK_KEYMAP (keymap_x11)),
                      &keymap_x11->min_keycode, &keymap_x11->max_keycode);
}

#ifdef HAVE_XKB

static void
update_modmap (Display      *display,
               GdkX11Keymap *keymap_x11)
{
  static struct {
    const char *name;
    Atom atom;
    GdkModifierType mask;
  } vmods[] = {
    { "Meta", 0, GDK_META_MASK },
    { "Super", 0, GDK_SUPER_MASK },
    { "Hyper", 0, GDK_HYPER_MASK },
    { NULL, 0, 0 }
  };

  int i, j, k;

  if (!vmods[0].atom)
    for (i = 0; vmods[i].name; i++)
      vmods[i].atom = XInternAtom (display, vmods[i].name, FALSE);

  for (i = 0; i < 8; i++)
    keymap_x11->modmap[i] = 1 << i;

  for (i = 0; i < XkbNumVirtualMods; i++)
    {
      for (j = 0; vmods[j].atom; j++)
        {
          if (keymap_x11->xkb_desc->names->vmods[i] == vmods[j].atom)
            {
              for (k = 0; k < 8; k++)
                {
                  if (keymap_x11->xkb_desc->server->vmods[i] & (1 << k))
                    keymap_x11->modmap[k] |= vmods[j].mask;
                }
            }
        }
    }
}

static XkbDescPtr
get_xkb (GdkX11Keymap *keymap_x11)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (GDK_KEYMAP (keymap_x11)->display);
  Display *xdisplay = display_x11->xdisplay;

  update_keyrange (keymap_x11);

  if (keymap_x11->xkb_desc == NULL)
    {
      keymap_x11->xkb_desc = XkbGetMap (xdisplay, XkbKeySymsMask | XkbKeyTypesMask | XkbModifierMapMask | XkbVirtualModsMask, XkbUseCoreKbd);
      if (keymap_x11->xkb_desc == NULL)
        {
          g_error ("Failed to get keymap");
          return NULL;
        }

      XkbGetNames (xdisplay, XkbGroupNamesMask | XkbVirtualModNamesMask, keymap_x11->xkb_desc);

      update_modmap (xdisplay, keymap_x11);
    }
  else if (keymap_x11->current_serial != display_x11->keymap_serial)
    {
      XkbGetUpdatedMap (xdisplay, XkbKeySymsMask | XkbKeyTypesMask | XkbModifierMapMask | XkbVirtualModsMask,
                        keymap_x11->xkb_desc);
      XkbGetNames (xdisplay, XkbGroupNamesMask | XkbVirtualModNamesMask, keymap_x11->xkb_desc);

      update_modmap (xdisplay, keymap_x11);
    }

  keymap_x11->current_serial = display_x11->keymap_serial;

  if (keymap_x11->num_lock_mask == 0)
    keymap_x11->num_lock_mask = XkbKeysymToModifiers (KEYMAP_XDISPLAY (GDK_KEYMAP (keymap_x11)), GDK_KEY_Num_Lock);

  if (keymap_x11->scroll_lock_mask == 0)
    keymap_x11->scroll_lock_mask = XkbKeysymToModifiers (KEYMAP_XDISPLAY (GDK_KEYMAP (keymap_x11)), GDK_KEY_Scroll_Lock);


  return keymap_x11->xkb_desc;
}
#endif /* HAVE_XKB */

/* Whether we were able to turn on detectable-autorepeat using
 * XkbSetDetectableAutorepeat. If FALSE, we’ll fall back
 * to checking the next event with XPending().
 */

/* Find the index of the group/level pair within the keysyms for a key.
 * We round up the number of keysyms per keycode to the next even number,
 * otherwise we lose a whole group of keys
 */
#define KEYSYM_INDEX(keymap_impl, group, level) \
  (2 * ((group) % (int)((keymap_impl->keysyms_per_keycode + 1) / 2)) + (level))
#define KEYSYM_IS_KEYPAD(s) (((s) >= 0xff80 && (s) <= 0xffbd) || \
                             ((s) >= 0x11000000 && (s) <= 0x1100ffff))

static int
get_symbol (const KeySym *syms,
            GdkX11Keymap *keymap_x11,
            int           group,
            int           level)
{
  int index;

  index = KEYSYM_INDEX(keymap_x11, group, level);
  if (index >= keymap_x11->keysyms_per_keycode)
      return NoSymbol;

  return syms[index];
}

static void
set_symbol (KeySym       *syms,
            GdkX11Keymap *keymap_x11,
            int           group,
            int           level,
            KeySym        sym)
{
  int index;

  index = KEYSYM_INDEX(keymap_x11, group, level);
  if (index >= keymap_x11->keysyms_per_keycode)
      return;

  syms[index] = sym;
}

static void
update_keymaps (GdkX11Keymap *keymap_x11)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (GDK_KEYMAP (keymap_x11)->display);
  Display *xdisplay = display_x11->xdisplay;

#ifdef HAVE_XKB
  g_assert (!KEYMAP_USE_XKB (GDK_KEYMAP (keymap_x11)));
#endif

  if (keymap_x11->keymap == NULL ||
      keymap_x11->current_serial != display_x11->keymap_serial)
    {
      int i;
      int map_size;
      int keycode;

      keymap_x11->current_serial = display_x11->keymap_serial;

      update_keyrange (keymap_x11);

      if (keymap_x11->keymap)
        XFree (keymap_x11->keymap);

      if (keymap_x11->mod_keymap)
        XFreeModifiermap (keymap_x11->mod_keymap);

      keymap_x11->keymap = XGetKeyboardMapping (xdisplay, keymap_x11->min_keycode,
                                                keymap_x11->max_keycode - keymap_x11->min_keycode + 1,
                                                &keymap_x11->keysyms_per_keycode);


      /* GDK_KEY_ISO_Left_Tab, as usually configured through XKB, really messes
       * up the whole idea of "consumed modifiers" because shift is consumed.
       * However, <shift>Tab is not usually GDK_KEY_ISO_Left_Tab without XKB,
       * we fudge the map here.
       */
      keycode = keymap_x11->min_keycode;
      while (keycode <= keymap_x11->max_keycode)
        {
          KeySym *syms = keymap_x11->keymap + (keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
          /* Check both groups */
          for (i = 0 ; i < 2 ; i++)
            {
              if (get_symbol (syms, keymap_x11, i, 0) == GDK_KEY_Tab)
                set_symbol (syms, keymap_x11, i, 1, GDK_KEY_ISO_Left_Tab);
            }

          /*
           * If there is one keysym and the key symbol has upper and lower
           * case variants fudge the keymap
           */
          if (get_symbol (syms, keymap_x11, 0, 1) == 0)
            {
              guint lower;
              guint upper;

              gdk_keyval_convert_case (get_symbol (syms, keymap_x11, 0, 0), &lower, &upper);
              if (lower != upper)
                {
                  set_symbol (syms, keymap_x11, 0, 0, lower);
                  set_symbol (syms, keymap_x11, 0, 1, upper);
                }
            }

          ++keycode;
        }

      keymap_x11->mod_keymap = XGetModifierMapping (xdisplay);

      keymap_x11->lock_keysym = GDK_KEY_VoidSymbol;
      keymap_x11->group_switch_mask = 0;
      keymap_x11->num_lock_mask = 0;
      keymap_x11->scroll_lock_mask = 0;

      for (i = 0; i < 8; i++)
        keymap_x11->modmap[i] = 1 << i;

      /* There are 8 sets of modifiers, with each set containing
       * max_keypermod keycodes.
       */
      map_size = 8 * keymap_x11->mod_keymap->max_keypermod;
      for (i = 0; i < map_size; i++)
        {
          /* Get the key code at this point in the map. */
          int code = keymap_x11->mod_keymap->modifiermap[i];
          int j;
          KeySym *syms;
          guint mask;

          /* Ignore invalid keycodes. */
          if (code < keymap_x11->min_keycode ||
              code > keymap_x11->max_keycode)
            continue;

          syms = keymap_x11->keymap + (code - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

          mask = 0;
          for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
            {
              if (syms[j] == GDK_KEY_Meta_L ||
                  syms[j] == GDK_KEY_Meta_R)
                mask |= GDK_META_MASK;
              else if (syms[j] == GDK_KEY_Hyper_L ||
                       syms[j] == GDK_KEY_Hyper_R)
                mask |= GDK_HYPER_MASK;
              else if (syms[j] == GDK_KEY_Super_L ||
                       syms[j] == GDK_KEY_Super_R)
                mask |= GDK_SUPER_MASK;
            }

          keymap_x11->modmap[i / keymap_x11->mod_keymap->max_keypermod] |= mask;

          /* The fourth modifier, GDK_ALT_MASK is 1 << 3.
           * Each group of max_keypermod entries refers to the same modifier.
           */
          mask = 1 << (i / keymap_x11->mod_keymap->max_keypermod);

          switch (mask)
            {
            case GDK_LOCK_MASK:
              /* Get the Lock keysym.  If any keysym bound to the Lock modifier
               * is Caps_Lock, we will interpret the modifier as Caps_Lock;
               * otherwise, if any is bound to Shift_Lock, we will interpret
               * the modifier as Shift_Lock. Otherwise, the lock modifier
               * has no effect.
               */
              for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
                {
                  if (syms[j] == GDK_KEY_Caps_Lock)
                    keymap_x11->lock_keysym = GDK_KEY_Caps_Lock;
                  else if (syms[j] == GDK_KEY_Shift_Lock &&
                           keymap_x11->lock_keysym == GDK_KEY_VoidSymbol)
                    keymap_x11->lock_keysym = GDK_KEY_Shift_Lock;
                }
              break;

            case GDK_CONTROL_MASK:
            case GDK_SHIFT_MASK:
            case GDK_ALT_MASK:
              /* Some keyboard maps are known to map Mode_Switch as an
               * extra Mod1 key. In circumstances like that, it won't be
               * used to switch groups.
               */
              break;

            default:
              /* Find the Mode_Switch, Num_Lock and Scroll_Lock modifiers. */
              for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
                {
                  if (syms[j] == GDK_KEY_Mode_switch)
                    {
                      /* This modifier swaps groups */
                      keymap_x11->group_switch_mask |= mask;
                    }
                  else if (syms[j] == GDK_KEY_Num_Lock)
                    {
                      /* This modifier is used for Num_Lock */
                      keymap_x11->num_lock_mask |= mask;
                    }
                  else if (syms[j] == GDK_KEY_Scroll_Lock)
                    {
                      /* This modifier is used for Scroll_Lock */
                      keymap_x11->scroll_lock_mask |= mask;
                    }
                }
              break;
            }
        }
    }
}

static const KeySym*
get_keymap (GdkX11Keymap *keymap_x11)
{
  update_keymaps (keymap_x11);

  return keymap_x11->keymap;
}

#ifdef HAVE_XKB
static PangoDirection
get_direction (XkbDescRec *xkb,
               int         group)
{
  int code;

  int rtl_minus_ltr = 0; /* total number of RTL keysyms minus LTR ones */

  for (code = xkb->min_key_code; code <= xkb->max_key_code; code++)
    {
      int level = 0;
      KeySym sym = XkbKeySymEntry (xkb, code, level, group);
      PangoDirection dir = gdk_unichar_direction (gdk_keyval_to_unicode (sym));

      switch (dir)
        {
        case PANGO_DIRECTION_RTL:
          rtl_minus_ltr++;
          break;
        case PANGO_DIRECTION_LTR:
          rtl_minus_ltr--;
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

  if (rtl_minus_ltr > 0)
    return PANGO_DIRECTION_RTL;
  else
    return PANGO_DIRECTION_LTR;
}

static PangoDirection
get_direction_from_cache (GdkX11Keymap *keymap_x11,
                          XkbDescPtr    xkb,
                          int           group)
{
  Atom group_atom = xkb->names->groups[group];

  gboolean cache_hit = FALSE;
  DirectionCacheEntry *cache = keymap_x11->group_direction_cache;

  PangoDirection direction = PANGO_DIRECTION_NEUTRAL;
  int i;

  if (keymap_x11->have_direction)
    {
      /* lookup in cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
      {
        if (cache[i].group_atom == group_atom)
          {
            cache_hit = TRUE;
            cache[i].serial = keymap_x11->current_cache_serial++; /* freshen */
            direction = cache[i].direction;
            group_atom = cache[i].group_atom;
            break;
          }
      }
    }
  else
    {
      /* initialize cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
        {
          cache[i].group_atom = 0;
          cache[i].serial = keymap_x11->current_cache_serial;
        }
      keymap_x11->current_cache_serial++;
    }

  /* insert in cache */
  if (!cache_hit)
    {
      int oldest = 0;

      direction = get_direction (xkb, group);

      /* remove the oldest entry */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
        {
          if (cache[i].serial < cache[oldest].serial)
            oldest = i;
        }

      cache[oldest].group_atom = group_atom;
      cache[oldest].direction = direction;
      cache[oldest].serial = keymap_x11->current_cache_serial++;
    }

  return direction;
}

static int
get_num_groups (GdkKeymap *keymap,
                XkbDescPtr xkb)
{
      Display *display = KEYMAP_XDISPLAY (keymap);
      XkbGetControls(display, XkbSlowKeysMask, xkb);
      XkbGetUpdatedMap (display, XkbKeySymsMask | XkbKeyTypesMask |
                        XkbModifierMapMask | XkbVirtualModsMask, xkb);
      return xkb->ctrls->num_groups;
}

static gboolean
update_direction (GdkX11Keymap *keymap_x11,
                  GdkDevice    *keyboard,
                  int           group)
{
  XkbDescPtr xkb = get_xkb (keymap_x11);
  Atom group_atom;
  gboolean had_direction;
  PangoDirection old_direction;

  had_direction = keymap_x11->have_direction;
  old_direction = keymap_x11->current_direction;

  group_atom = xkb->names->groups[group];

  /* a group change? */
  if (!keymap_x11->have_direction || keymap_x11->current_group_atom != group_atom)
    {
      keymap_x11->current_direction = get_direction_from_cache (keymap_x11, xkb, group);
      keymap_x11->current_group_atom = group_atom;
      keymap_x11->have_direction = TRUE;
    }

  if (had_direction && old_direction != keymap_x11->current_direction)
    {
      g_object_notify (G_OBJECT (keyboard), "direction");
      return TRUE;
    }

  return FALSE;
}

static gboolean
update_lock_state (GdkX11Keymap *keymap_x11,
                   GdkDevice    *keyboard,
                   int           locked_mods,
                   int           effective_mods)
{
  XkbDescPtr xkb G_GNUC_UNUSED;
  gboolean have_lock_state;
  gboolean caps_lock_state;
  gboolean num_lock_state;
  gboolean scroll_lock_state;
  guint modifier_state;

  /* ensure keymap_x11->num_lock_mask is initialized */
  xkb = get_xkb (keymap_x11);

  have_lock_state = keymap_x11->have_lock_state;
  caps_lock_state = keymap_x11->caps_lock_state;
  num_lock_state = keymap_x11->num_lock_state;
  scroll_lock_state = keymap_x11->scroll_lock_state;
  modifier_state = keymap_x11->modifier_state;

  keymap_x11->have_lock_state = TRUE;
  keymap_x11->caps_lock_state = (locked_mods & GDK_LOCK_MASK) != 0;
  keymap_x11->num_lock_state = (locked_mods & keymap_x11->num_lock_mask) != 0;
  keymap_x11->scroll_lock_state = (locked_mods & keymap_x11->scroll_lock_mask) != 0;
  /* FIXME: sanitize this */
  keymap_x11->modifier_state = (guint)effective_mods;

  if (caps_lock_state != keymap_x11->caps_lock_state)
    g_object_notify (G_OBJECT (keyboard), "caps-lock-state");
  if (num_lock_state != keymap_x11->num_lock_state)
    g_object_notify (G_OBJECT (keyboard), "num-lock-state");
  if (scroll_lock_state != keymap_x11->scroll_lock_state)
    g_object_notify (G_OBJECT (keyboard), "scroll-lock-state");
  if (modifier_state != keymap_x11->modifier_state)
    g_object_notify (G_OBJECT (keyboard), "modifier-state");

  return !have_lock_state
         || (caps_lock_state != keymap_x11->caps_lock_state)
         || (num_lock_state != keymap_x11->num_lock_state)
         || (scroll_lock_state != keymap_x11->scroll_lock_state)
         || (modifier_state != keymap_x11->modifier_state);
}

/* keep this in sync with the XkbSelectEventDetails()
 * call in gdk_display_open()
 */
void
_gdk_x11_keymap_state_changed (GdkDisplay   *display,
                               const XEvent *xevent)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  const XkbEvent *xkb_event = (const XkbEvent *) xevent;

  if (display_x11->keymap)
    {
      GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (display_x11->keymap);
      GdkDevice *keyboard;

      keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (display));

      if (update_direction (keymap_x11, keyboard, XkbStateGroup (&xkb_event->state)))
        g_signal_emit_by_name (keymap_x11, "direction-changed");

      if (update_lock_state (keymap_x11,
                             keyboard,
                             xkb_event->state.locked_mods,
                             xkb_event->state.mods))
        g_signal_emit_by_name (keymap_x11, "state-changed");
    }
}

#endif /* HAVE_XKB */

static void
ensure_lock_state (GdkKeymap *keymap)
{
#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
      GdkDevice *keyboard;

      keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (keymap->display));

      if (!keymap_x11->have_lock_state)
        {
          GdkDisplay *display = keymap->display;
          XkbStateRec state_rec;

          XkbGetState (GDK_DISPLAY_XDISPLAY (display), XkbUseCoreKbd, &state_rec);
          update_lock_state (keymap_x11, keyboard, state_rec.locked_mods, state_rec.mods);
        }
    }
#endif /* HAVE_XKB */
}

void
_gdk_x11_keymap_keys_changed (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  ++display_x11->keymap_serial;

  if (display_x11->keymap)
    g_signal_emit_by_name (display_x11->keymap, "keys_changed", 0);
}

static PangoDirection
gdk_x11_keymap_get_direction (GdkKeymap *keymap)
{
#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

      if (!keymap_x11->have_direction)
        {
          GdkDisplay *display = keymap->display;
          GdkDevice *keyboard;
          XkbStateRec state_rec;

          keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (display));

          XkbGetState (GDK_DISPLAY_XDISPLAY (display), XkbUseCoreKbd,
                       &state_rec);
          update_direction (keymap_x11, keyboard, XkbStateGroup (&state_rec));
        }

      return keymap_x11->current_direction;
    }
  else
#endif /* HAVE_XKB */
    return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_x11_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
      XkbDescPtr xkb = get_xkb (keymap_x11);
      int num_groups = get_num_groups (keymap, xkb);

      int i;
      gboolean have_ltr_keyboard = FALSE;
      gboolean have_rtl_keyboard = FALSE;

      for (i = 0; i < num_groups; i++)
      {
        if (get_direction_from_cache (keymap_x11, xkb, i) == PANGO_DIRECTION_RTL)
          have_rtl_keyboard = TRUE;
        else
          have_ltr_keyboard = TRUE;
      }

      return have_ltr_keyboard && have_rtl_keyboard;
    }
  else
#endif /* HAVE_XKB */
    return FALSE;
}

static gboolean
gdk_x11_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

  ensure_lock_state (keymap);

  return keymap_x11->caps_lock_state;
}

static gboolean
gdk_x11_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

  ensure_lock_state (keymap);

  return keymap_x11->num_lock_state;
}

static gboolean
gdk_x11_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

  ensure_lock_state (keymap);

  return keymap_x11->scroll_lock_state;
}

static guint
gdk_x11_keymap_get_modifier_state (GdkKeymap *keymap)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

  ensure_lock_state (keymap);

  return keymap_x11->modifier_state;
}

static gboolean
gdk_x11_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                       guint      keyval,
                                       GArray    *retval)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
  guint len = retval->len;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb (keymap_x11);
      int keycode;

      keycode = keymap_x11->min_keycode;

      while (keycode <= keymap_x11->max_keycode)
        {
          int max_shift_levels = XkbKeyGroupsWidth (xkb, keycode); /* "key width" */
          int group = 0;
          int level = 0;
          int total_syms = XkbKeyNumSyms (xkb, keycode);
          int i = 0;
          KeySym *entry;

          /* entry is an array with all syms for group 0, all
           * syms for group 1, etc. and for each group the
           * shift level syms are in order
           */
          entry = XkbKeySymsPtr (xkb, keycode);

          while (i < total_syms)
            {
              /* check out our cool loop invariant */
              g_assert (i == (group * max_shift_levels + level));

              if (entry[i] == keyval)
                {
                  /* Found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;
                  key.group = group;
                  key.level = level;

                  g_array_append_val (retval, key);

                  g_assert (XkbKeySymEntry (xkb, keycode, level, group) == keyval);
                }

              ++level;

              if (level == max_shift_levels)
                {
                  level = 0;
                  ++group;
                }

              ++i;
            }

          ++keycode;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      int keycode;

      keycode = keymap_x11->min_keycode;
      while (keycode <= keymap_x11->max_keycode)
        {
          const KeySym *syms = map + (keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
          int i = 0;

          while (i < keymap_x11->keysyms_per_keycode)
            {
              if (syms[i] == keyval)
                {
                  /* found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;

                  /* The "classic" non-XKB keymap has 2 levels per group */
                  key.group = i / 2;
                  key.level = i % 2;

                  g_array_append_val (retval, key);
                }

              ++i;
            }

          ++keycode;
        }
    }

  return retval->len > len;
}

static gboolean
gdk_x11_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                        guint          hardware_keycode,
                                        GdkKeymapKey **keys,
                                        guint        **keyvals,
                                        int           *n_entries)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
  GArray *key_array;
  GArray *keyval_array;

  update_keyrange (keymap_x11);

  if (hardware_keycode < keymap_x11->min_keycode ||
      hardware_keycode > keymap_x11->max_keycode)
    {
      if (keys)
        *keys = NULL;
      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
      return FALSE;
    }

  if (keys)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;

  if (keyvals)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb (keymap_x11);
      int max_shift_levels;
      int group = 0;
      int level = 0;
      int total_syms;
      int i = 0;
      KeySym *entry;

      max_shift_levels = XkbKeyGroupsWidth (xkb, hardware_keycode); /* "key width" */
      total_syms = XkbKeyNumSyms (xkb, hardware_keycode);

      /* entry is an array with all syms for group 0, all
       * syms for group 1, etc. and for each group the
       * shift level syms are in order
       */
      entry = XkbKeySymsPtr (xkb, hardware_keycode);

      while (i < total_syms)
        {
          /* check out our cool loop invariant */
          g_assert (i == (group * max_shift_levels + level));

          if (key_array)
            {
              GdkKeymapKey key;

              key.keycode = hardware_keycode;
              key.group = group;
              key.level = level;

              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, entry[i]);

          ++level;

          if (level == max_shift_levels)
            {
              level = 0;
              ++group;
            }

          ++i;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      const KeySym *syms;
      int i = 0;

      syms = map + (hardware_keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

      while (i < keymap_x11->keysyms_per_keycode)
        {
          if (key_array)
            {
              GdkKeymapKey key;

              key.keycode = hardware_keycode;

              /* The "classic" non-XKB keymap has 2 levels per group */
              key.group = i / 2;
              key.level = i % 2;

              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, syms[i]);

          ++i;
        }
    }

  *n_entries = 0;

  if (keys)
    {
      *n_entries = key_array->len;
      *keys = (GdkKeymapKey*) g_array_free (key_array, FALSE);
    }

  if (keyvals)
    {
      *n_entries = keyval_array->len;
      *keyvals = (guint*) g_array_free (keyval_array, FALSE);
    }

  return *n_entries > 0;
}

static guint
gdk_x11_keymap_lookup_key (GdkKeymap          *keymap,
                           const GdkKeymapKey *key)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);

  g_return_val_if_fail (key->group < 4, 0);

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);

      return XkbKeySymEntry (xkb, key->keycode, key->level, key->group);
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      const KeySym *syms = map + (key->keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
      return get_symbol (syms, keymap_x11, key->group, key->level);
    }
}

#ifdef HAVE_XKB
/* This is copied straight from XFree86 Xlib, to:
 *  - add the group and level return.
 *  - change the interpretation of mods_rtrn as described
 *    in the docs for gdk_keymap_translate_keyboard_state()
 * It’s unchanged for ease of diff against the Xlib sources; don't
 * reformat it.
 */
static Bool
MyEnhancedXkbTranslateKeyCode(register XkbDescPtr     xkb,
                              KeyCode                 key,
                              register unsigned int   mods,
                              unsigned int *          mods_rtrn,
                              KeySym *                keysym_rtrn,
                              int *                   group_rtrn,
                              int *                   level_rtrn)
{
    XkbKeyTypeRec *type;
    int col,nKeyGroups;
    unsigned preserve,effectiveGroup;
    KeySym *syms;
    int found_col = 0;

    if (mods_rtrn!=NULL)
        *mods_rtrn = 0;

    nKeyGroups= XkbKeyNumGroups(xkb,key);
    if ((!XkbKeycodeInRange(xkb,key))||(nKeyGroups==0)) {
        if (keysym_rtrn!=NULL)
            *keysym_rtrn = NoSymbol;
        return False;
    }

    syms = XkbKeySymsPtr(xkb,key);

    /* find the offset of the effective group */
    col = 0;
    effectiveGroup= XkbGroupForCoreState(mods);
    if ( effectiveGroup>=nKeyGroups ) {
        unsigned groupInfo= XkbKeyGroupInfo(xkb,key);
        switch (XkbOutOfRangeGroupAction(groupInfo)) {
            default:
                effectiveGroup %= nKeyGroups;
                break;
            case XkbClampIntoRange:
                effectiveGroup = nKeyGroups-1;
                break;
            case XkbRedirectIntoRange:
                effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
                if (effectiveGroup>=nKeyGroups)
                    effectiveGroup= 0;
                break;
        }
    }
    found_col = col= effectiveGroup*XkbKeyGroupsWidth(xkb,key);
    type = XkbKeyKeyType(xkb,key,effectiveGroup);

    preserve= 0;
    if (type->map) { /* find the column (shift level) within the group */
        register int i;
        register XkbKTMapEntryPtr entry;
        /* ---- Begin section modified for GDK  ---- */
        int found = 0;

        for (i=0,entry=type->map;i<type->map_count;i++,entry++) {
            if (!entry->active || syms[col+entry->level] == syms[col])
              continue;
            if (mods_rtrn) {
                int bits = 0;
                unsigned long tmp = entry->mods.mask;
                while (tmp) {
                    if ((tmp & 1) == 1)
                        bits++;
                    tmp >>= 1;
                }
                /* We always add one-modifiers levels to mods_rtrn since
                 * they can't wipe out bits in the state unless the
                 * level would be triggered. But not if they don't change
                 * the symbol (otherwise we can't discriminate Shift-F10
                 * and F10 anymore). And don't add modifiers that are
                 * explicitly marked as preserved, either.
                 */
                if (bits == 1 ||
                    (mods&type->mods.mask) == entry->mods.mask)
                  {
                    if (type->preserve)
                      *mods_rtrn |= (entry->mods.mask & ~type->preserve[i].mask);
                    else
                      *mods_rtrn |= entry->mods.mask;
                  }
            }

            if (!found && ((mods&type->mods.mask) == entry->mods.mask)) {
                found_col= col + entry->level;
                if (type->preserve)
                    preserve= type->preserve[i].mask;

                if (level_rtrn)
                  *level_rtrn = entry->level;

                found = 1;
            }
        }
        /* ---- End section modified for GDK ---- */
    }

    if (keysym_rtrn!=NULL)
        *keysym_rtrn= syms[found_col];
    if (mods_rtrn) {
        /* ---- Begin section modified for GDK  ---- */
        *mods_rtrn &= ~preserve;
        /* ---- End section modified for GDK ---- */

        /* ---- Begin stuff GDK comments out of the original Xlib version ---- */
        /* This is commented out because xkb_info is a private struct */

#if 0
        /* The Motif VTS doesn't get the help callback called if help
         * is bound to Shift+<whatever>, and it appears as though it
         * is XkbTranslateKeyCode that is causing the problem.  The
         * core X version of XTranslateKey always OR's in ShiftMask
         * and LockMask for mods_rtrn, so this "fix" keeps this behavior
         * and solves the VTS problem.
         */
        if ((xkb->dpy)&&(xkb->dpy->xkb_info)&&
            (xkb->dpy->xkb_info->xlib_ctrls&XkbLC_AlwaysConsumeShiftAndLock)) {            *mods_rtrn|= (ShiftMask|LockMask);
        }
#endif

        /* ---- End stuff GDK comments out of the original Xlib version ---- */
    }

    /* ---- Begin stuff GDK adds to the original Xlib version ---- */

    if (group_rtrn)
      *group_rtrn = effectiveGroup;

    /* ---- End stuff GDK adds to the original Xlib version ---- */

    return (syms[found_col] != NoSymbol);
}
#endif /* HAVE_XKB */

/* Translates from keycode/state to keysymbol using the traditional interpretation
 * of the keyboard map. See section 12.7 of the Xlib reference manual
 */
static guint
translate_keysym (GdkX11Keymap   *keymap_x11,
                  guint           hardware_keycode,
                  int             group,
                  GdkModifierType state,
                  int            *effective_group,
                  int            *effective_level)
{
  const KeySym *map = get_keymap (keymap_x11);
  const KeySym *syms = map + (hardware_keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

#define SYM(k,g,l) get_symbol (syms, k,g,l)

  GdkModifierType shift_modifiers;
  int shift_level;
  guint tmp_keyval;

  shift_modifiers = GDK_SHIFT_MASK;
  if (keymap_x11->lock_keysym == GDK_KEY_Shift_Lock)
    shift_modifiers |= GDK_LOCK_MASK;

  /* Fall back to the first group if the passed in group is empty
   */
  if (!(SYM (keymap_x11, group, 0) || SYM (keymap_x11, group, 1)) &&
      (SYM (keymap_x11, 0, 0) || SYM (keymap_x11, 0, 1)))
    group = 0;

  if ((state & keymap_x11->num_lock_mask) &&
      KEYSYM_IS_KEYPAD (SYM (keymap_x11, group, 1)))
    {
      /* Shift, Shift_Lock cancel Num_Lock
       */
      shift_level = (state & shift_modifiers) ? 0 : 1;
      if (!SYM (keymap_x11, group, shift_level) && SYM (keymap_x11, group, 0))
        shift_level = 0;

       tmp_keyval = SYM (keymap_x11, group, shift_level);
    }
  else
    {
      /* Fall back to the first level if no symbol for the level
       * we were passed.
       */
      shift_level = (state & shift_modifiers) ? 1 : 0;
      if (!SYM (keymap_x11, group, shift_level) && SYM (keymap_x11, group, 0))
        shift_level = 0;

      tmp_keyval = SYM (keymap_x11, group, shift_level);

      if (keymap_x11->lock_keysym == GDK_KEY_Caps_Lock && (state & GDK_LOCK_MASK) != 0)
        {
          guint upper = gdk_keyval_to_upper (tmp_keyval);
          if (upper != tmp_keyval)
            tmp_keyval = upper;
        }
    }

  if (effective_group)
    *effective_group = group;

  if (effective_level)
    *effective_level = shift_level;

  return tmp_keyval;

#undef SYM
}

static gboolean
gdk_x11_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                         guint            hardware_keycode,
                                         GdkModifierType  state,
                                         int              group,
                                         guint           *keyval,
                                         int             *effective_group,
                                         int             *level,
                                         GdkModifierType *consumed_modifiers)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
  KeySym tmp_keyval = NoSymbol;
  guint tmp_modifiers;

  g_return_val_if_fail (group < 4, FALSE);

  if (keyval)
    *keyval = NoSymbol;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  update_keyrange (keymap_x11);

  if (hardware_keycode < keymap_x11->min_keycode ||
      hardware_keycode > keymap_x11->max_keycode)
    return FALSE;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);

      /* replace bits 13 and 14 with the provided group */
      state &= ~(1 << 13 | 1 << 14);
      state |= group << 13;

      MyEnhancedXkbTranslateKeyCode (xkb,
                                     hardware_keycode,
                                     state,
                                     &tmp_modifiers,
                                     &tmp_keyval,
                                     effective_group,
                                     level);

      if (state & ~tmp_modifiers & LockMask)
        tmp_keyval = gdk_keyval_to_upper (tmp_keyval);

      /* We need to augment the consumed modifiers with LockMask, since
       * we handle that ourselves, and also with the group bits
       */
      tmp_modifiers |= LockMask | 1 << 13 | 1 << 14;
    }
  else
#endif
    {
      GdkModifierType bit;

      tmp_modifiers = 0;

      /* We see what modifiers matter by trying the translation with
       * and without each possible modifier
       */
      for (bit = GDK_SHIFT_MASK; bit < GDK_BUTTON1_MASK; bit <<= 1)
        {
          /* Handling of the group here is a bit funky; a traditional
           * X keyboard map can have more than two groups, but no way
           * of accessing the extra groups is defined. We allow a
           * caller to pass in any group to this function, but we
           * only can represent switching between group 0 and 1 in
           * consumed modifiers.
           */
          if (translate_keysym (keymap_x11, hardware_keycode,
                                (bit == keymap_x11->group_switch_mask) ? 0 : group,
                                state & ~bit,
                                NULL, NULL) !=
              translate_keysym (keymap_x11, hardware_keycode,
                                (bit == keymap_x11->group_switch_mask) ? 1 : group,
                                state | bit,
                                NULL, NULL))
            tmp_modifiers |= bit;
        }

      tmp_keyval = translate_keysym (keymap_x11, hardware_keycode,
                                     group, state,
                                     effective_group, level);
    }

  if (consumed_modifiers)
    *consumed_modifiers = tmp_modifiers;

  if (keyval)
    *keyval = tmp_keyval;

  return tmp_keyval != NoSymbol;
}

/*< private >
 * gdk_x11_keymap_get_group_for_state:
 * @keymap: (type GdkX11Keymap): a `GdkX11Keymap`
 * @state: raw state returned from X
 *
 * Extracts the group from the state field sent in an X Key event.
 * This is only needed for code processing raw X events, since `GdkEventKey`
 * directly includes an is_modifier field.
 *
 * Returns: the index of the active keyboard group for the event
 */
int
gdk_x11_keymap_get_group_for_state (GdkKeymap *keymap,
                                    guint      state)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;

  g_return_val_if_fail (GDK_IS_X11_KEYMAP (keymap), 0);

  display = keymap->display;
  display_x11 = GDK_X11_DISPLAY (display);

#ifdef HAVE_XKB
  if (display_x11->use_xkb)
    {
      return XkbGroupForCoreState (state);
    }
  else
#endif
    {
      GdkX11Keymap *keymap_impl = GDK_X11_KEYMAP (gdk_display_get_keymap (display));
      update_keymaps (keymap_impl);
      return (state & keymap_impl->group_switch_mask) ? 1 : 0;
    }
}

void
_gdk_x11_keymap_add_virt_mods (GdkKeymap       *keymap,
                               GdkModifierType *modifiers)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
  int i;

  /*  This loop used to start at 3, which included MOD1 in the
   *  virtual mapping. However, all of GTK+ treats MOD1 as a
   *  synonym for Alt, and does not expect it to be mapped around,
   *  therefore it's more sane to simply treat MOD1 like SHIFT and
   *  CONTROL, which are not mappable either.
   */
  for (i = 4; i < 8; i++)
    {
      if ((1 << i) & *modifiers)
        {
          if (keymap_x11->modmap[i] & GDK_SUPER_MASK)
            *modifiers |= GDK_SUPER_MASK;
          else if (keymap_x11->modmap[i] & GDK_HYPER_MASK)
            *modifiers |= GDK_HYPER_MASK;
          else if (keymap_x11->modmap[i] & GDK_META_MASK)
            *modifiers |= GDK_META_MASK;
        }
    }
}

/*< private >
 * gdk_x11_keymap_key_is_modifier:
 * @keymap: (type GdkX11Keymap): a `GdkX11Keymap`
 * @keycode: the hardware keycode from a key event
 *
 * Determines whether a particular key code represents a key that
 * is a modifier. That is, it’s a key that normally just affects
 * the keyboard state and the behavior of other keys rather than
 * producing a direct effect itself. This is only needed for code
 * processing raw X events, since `GdkEventKey` directly includes
 * an is_modifier field.
 *
 * Returns: %TRUE if the hardware keycode is a modifier key
 */
gboolean
gdk_x11_keymap_key_is_modifier (GdkKeymap *keymap,
                                guint      keycode)
{
  GdkX11Keymap *keymap_x11 = GDK_X11_KEYMAP (keymap);
  int i;

  g_return_val_if_fail (GDK_IS_X11_KEYMAP (keymap), FALSE);

  update_keyrange (keymap_x11);
  if (keycode < keymap_x11->min_keycode ||
      keycode > keymap_x11->max_keycode)
    return FALSE;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);

      if (xkb->map->modmap && xkb->map->modmap[keycode] != 0)
        return TRUE;
    }
  else
#endif
    {
      for (i = 0; i < 8 * keymap_x11->mod_keymap->max_keypermod; i++)
        {
          if (keycode == keymap_x11->mod_keymap->modifiermap[i])
            return TRUE;
        }
    }

  return FALSE;
}

static void
gdk_x11_keymap_class_init (GdkX11KeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_x11_keymap_finalize;

  keymap_class->get_direction = gdk_x11_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_x11_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_x11_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_x11_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_x11_keymap_get_scroll_lock_state;
  keymap_class->get_modifier_state = gdk_x11_keymap_get_modifier_state;
  keymap_class->get_entries_for_keyval = gdk_x11_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_x11_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_x11_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_x11_keymap_translate_keyboard_state;
}
