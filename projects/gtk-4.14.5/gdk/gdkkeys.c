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

#include "gdkkeysyms.h"
#include "gdkkeysprivate.h"
#include "gdkdisplay.h"
#include "gdkdisplaymanagerprivate.h"

#include "keynamesprivate.h"
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>

enum {
  PROP_0,
  PROP_DISPLAY,
  LAST_PROP
};

enum {
  DIRECTION_CHANGED,
  KEYS_CHANGED,
  STATE_CHANGED,
  LAST_SIGNAL
};

static GParamSpec *props[LAST_PROP] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkKeymap, gdk_keymap, G_TYPE_OBJECT)

static void
gdk_keymap_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkKeymap *keymap = GDK_KEYMAP (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, keymap->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_keymap_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkKeymap *keymap = GDK_KEYMAP (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      keymap->display = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_keymap_finalize (GObject *object)
{
  GdkKeymap *keymap = GDK_KEYMAP (object);

  g_array_free (keymap->cached_keys, TRUE);
  g_hash_table_unref (keymap->cache);

  G_OBJECT_CLASS (gdk_keymap_parent_class)->finalize (object);
}

static void
gdk_keymap_keys_changed (GdkKeymap *keymap)
{
  GdkKeymapKey key;

  g_array_set_size (keymap->cached_keys, 0);

  key.keycode = 0;
  key.group = 0;
  key.level = 0;

  g_array_append_val (keymap->cached_keys, key);

  g_hash_table_remove_all (keymap->cache);
}

static void
gdk_keymap_class_init (GdkKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_keymap_finalize;
  object_class->get_property = gdk_keymap_get_property;
  object_class->set_property = gdk_keymap_set_property;

  klass->keys_changed = gdk_keymap_keys_changed;

  props[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /**
   * GdkKeymap::direction-changed:
   * @keymap: the object on which the signal is emitted
   *
   * Emitted when the direction of the keymap changes.
   *
   * See gdk_keymap_get_direction().
   */
  signals[DIRECTION_CHANGED] =
    g_signal_new (g_intern_static_string ("direction-changed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkKeymapClass, direction_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE,
		  0);

  /**
   * GdkKeymap::keys-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::keys-changed signal is emitted when the mapping represented by
   * @keymap changes.
   */
  signals[KEYS_CHANGED] =
    g_signal_new (g_intern_static_string ("keys-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GdkKeymapClass, keys_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GdkKeymap::state-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::state-changed signal is emitted when the state of the
   * keyboard changes, e.g when Caps Lock is turned on or off.
   * See gdk_keymap_get_caps_lock_state().
   */
  signals[STATE_CHANGED] =
    g_signal_new (g_intern_static_string ("state-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkKeymapClass, state_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gdk_keymap_init (GdkKeymap *keymap)
{
  GdkKeymapKey key;

  keymap->cached_keys = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  key.keycode = 0;
  key.group = 0;
  key.level = 0;

  g_array_append_val (keymap->cached_keys, key);

  keymap->cache = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/* Other key-handling stuff
 */

/**
 * gdk_keyval_to_upper:
 * @keyval: a key value.
 *
 * Converts a key value to upper case, if applicable.
 *
 * Returns: the upper case form of @keyval, or @keyval itself if it is already
 *   in upper case or it is not subject to case conversion.
 */
guint
gdk_keyval_to_upper (guint keyval)
{
  guint result;

  gdk_keyval_convert_case (keyval, NULL, &result);

  return result;
}

/**
 * gdk_keyval_to_lower:
 * @keyval: a key value.
 *
 * Converts a key value to lower case, if applicable.
 *
 * Returns: the lower case form of @keyval, or @keyval itself if it is already
 *  in lower case or it is not subject to case conversion.
 */
guint
gdk_keyval_to_lower (guint keyval)
{
  guint result;

  gdk_keyval_convert_case (keyval, &result, NULL);

  return result;
}

/**
 * gdk_keyval_is_upper:
 * @keyval: a key value.
 *
 * Returns %TRUE if the given key value is in upper case.
 *
 * Returns: %TRUE if @keyval is in upper case, or if @keyval is not subject to
 *  case conversion.
 */
gboolean
gdk_keyval_is_upper (guint keyval)
{
  if (keyval)
    {
      guint upper_val = 0;

      gdk_keyval_convert_case (keyval, NULL, &upper_val);
      return upper_val == keyval;
    }
  return FALSE;
}

/**
 * gdk_keyval_is_lower:
 * @keyval: a key value.
 *
 * Returns %TRUE if the given key value is in lower case.
 *
 * Returns: %TRUE if @keyval is in lower case, or if @keyval is not
 *   subject to case conversion.
 */
gboolean
gdk_keyval_is_lower (guint keyval)
{
  if (keyval)
    {
      guint lower_val = 0;

      gdk_keyval_convert_case (keyval, &lower_val, NULL);
      return lower_val == keyval;
    }
  return FALSE;
}

/*< private >
 * gdk_keymap_get_direction:
 * @keymap: a `GdkKeymap`
 *
 * Returns the direction of effective layout of the keymap.
 *
 * The direction of a layout is the direction of the majority of its
 * symbols. See pango_unichar_direction().
 *
 * Returns: %PANGO_DIRECTION_LTR or %PANGO_DIRECTION_RTL
 *   if it can determine the direction. %PANGO_DIRECTION_NEUTRAL
 *   otherwise.
 */
PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), PANGO_DIRECTION_LTR);

  return GDK_KEYMAP_GET_CLASS (keymap)->get_direction (keymap);
}

/*< private >
 * gdk_keymap_have_bidi_layouts:
 * @keymap: a `GdkKeymap`
 *
 * Determines if keyboard layouts for both right-to-left and left-to-right
 * languages are in use.
 *
 * Returns: %TRUE if there are layouts in both directions, %FALSE otherwise
 */
gboolean
gdk_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->have_bidi_layouts (keymap);
}

/*< private >
 * gdk_keymap_get_caps_lock_state:
 * @keymap: a `GdkKeymap`
 *
 * Returns whether the Caps Lock modifier is locked.
 *
 * Returns: %TRUE if Caps Lock is on
 */
gboolean
gdk_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->get_caps_lock_state (keymap);
}

/*< private >
 * gdk_keymap_get_num_lock_state:
 * @keymap: a `GdkKeymap`
 *
 * Returns whether the Num Lock modifier is locked.
 *
 * Returns: %TRUE if Num Lock is on
 */
gboolean
gdk_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->get_num_lock_state (keymap);
}

/*< private >
 * gdk_keymap_get_scroll_lock_state:
 * @keymap: a `GdkKeymap`
 *
 * Returns whether the Scroll Lock modifier is locked.
 *
 * Returns: %TRUE if Scroll Lock is on
 */
gboolean
gdk_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->get_scroll_lock_state (keymap);
}

/*< private >
 * gdk_keymap_get_modifier_state:
 * @keymap: a `GdkKeymap`
 *
 * Returns the current modifier state.
 *
 * Returns: the current modifier state.
 */
guint
gdk_keymap_get_modifier_state (GdkKeymap *keymap)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  if (GDK_KEYMAP_GET_CLASS (keymap)->get_modifier_state)
    return GDK_KEYMAP_GET_CLASS (keymap)->get_modifier_state (keymap);

  return 0;
}

/*< private >
 * gdk_keymap_get_entries_for_keyval:
 * @keymap: a `GdkKeymap`
 * @keyval: a keyval, such as %GDK_KEY_a, %GDK_KEY_Up, %GDK_KEY_Return, etc.
 * @keys: (out) (array length=n_keys) (transfer full): return location
 *   for an array of `GdkKeymapKey`
 * @n_keys: return location for number of elements in returned array
 *
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval. Groups and levels are two kinds of keyboard mode;
 * in general, the level determines whether the top or bottom symbol
 * on a key is used, and the group determines whether the left or
 * right symbol is used. On US keyboards, the shift key changes the
 * keyboard level, and there are no groups. A group switch key might
 * convert a keyboard between Hebrew to English modes, for example.
 * `GdkEventKey` contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 * The returned array should be freed
 * with g_free().
 *
 * Returns: %TRUE if keys were found and returned
 **/
gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   int           *n_keys)
{
  GArray *array;

  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  GDK_KEYMAP_GET_CLASS (keymap)->get_entries_for_keyval (keymap, keyval, array);

  *n_keys = array->len;
  *keys = (GdkKeymapKey *)g_array_free (array, FALSE);

  return TRUE;
}

void
gdk_keymap_get_cached_entries_for_keyval (GdkKeymap     *keymap,
                                          guint          keyval,
                                          GdkKeymapKey **keys,
                                          guint         *n_keys)
{
  guint cached;
  guint offset;
  guint len;

  /* avoid using the first entry in cached_keys, so we can
   * use 0 to mean 'not cached'
   */
  cached = GPOINTER_TO_UINT (g_hash_table_lookup (keymap->cache, GUINT_TO_POINTER (keyval)));
  if (cached == 0)
    {
      offset = keymap->cached_keys->len;

      GDK_KEYMAP_GET_CLASS (keymap)->get_entries_for_keyval (keymap, keyval, keymap->cached_keys);

      len = keymap->cached_keys->len - offset;
      g_assert (len <= 255);

      cached = (offset << 8) | len;

      g_hash_table_insert (keymap->cache, GUINT_TO_POINTER (keyval), GUINT_TO_POINTER (cached));
    }
  else
    {
      len = cached & 255;
      offset = cached >> 8;
    }

  *n_keys = len;
  *keys = (GdkKeymapKey *)&g_array_index (keymap->cached_keys, GdkKeymapKey, offset);
}

/*< private >
 * gdk_keymap_get_entries_for_keycode:
 * @keymap: a `GdkKeymap`
 * @hardware_keycode: a keycode
 * @keys: (out) (array length=n_entries) (transfer full) (optional): return
 *   location for array of `GdkKeymapKey`
 * @keyvals: (out) (array length=n_entries) (transfer full) (optional): return
 *   location for array of keyvals
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @hardware_keycode.
 * The Nth `GdkKeymapKey` in @keys is bound to the Nth
 * keyval in @keyvals. Free the returned arrays with g_free().
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level. See gdk_keymap_translate_keyboard_state().
 *
 * Returns: %TRUE if there were any entries
 **/
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    int           *n_entries)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->get_entries_for_keycode (keymap, hardware_keycode,
                                                                 keys, keyvals, n_entries);
}

/*< private >
 * gdk_keymap_lookup_key:
 * @keymap: a `GdkKeymap`
 * @key: a `GdkKeymapKey` with keycode, group, and level initialized
 *
 * Looks up the keyval mapped to a keycode/group/level triplet.
 * If no keyval is bound to @key, returns 0. For normal user input,
 * you want to use gdk_keymap_translate_keyboard_state() instead of
 * this function, since the effective group/level may not be
 * the same as the current keyboard state.
 *
 * Returns: a keyval, or 0 if none was mapped to the given @key
 **/
guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);

  return GDK_KEYMAP_GET_CLASS (keymap)->lookup_key (keymap, key);
}

/*< private >
 * gdk_keymap_translate_keyboard_state:
 * @keymap: a `GdkKeymap`
 * @hardware_keycode: a keycode
 * @state: a modifier state
 * @group: active keyboard group
 * @keyval: (out) (optional): return location for keyval
 * @effective_group: (out) (optional): return location for effective group
 * @level: (out) (optional): return location for level
 * @consumed_modifiers: (out) (optional): return location for modifiers
 *   that were used to determine the group or level
 *
 * Translates the contents of a `GdkEventKey` into a keyval, effective
 * group, and level. Modifiers that affected the translation and
 * are thus unavailable for application use are returned in
 * @consumed_modifiers.
 * See [Groups][key-group-explanation] for an explanation of
 * groups and levels. The @effective_group is the group that was
 * actually used for the translation; some keys such as Enter are not
 * affected by the active keyboard group. The @level is derived from
 * @state. For convenience, `GdkEventKey` already contains the translated
 * keyval, so this function isn’t as useful as you might think.
 *
 * @consumed_modifiers gives modifiers that should be masked outfrom @state
 * when comparing this key press to a hot key. For instance, on a US keyboard,
 * the `plus` symbol is shifted, so when comparing a key press to a
 * `<Control>plus` accelerator `<Shift>` should be masked out.
 *
 * |[<!-- language="C" -->
 * // We want to ignore irrelevant modifiers like ScrollLock
 * #define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK)
 * state = gdk_event_get_modifier_state (event);
 * gdk_keymap_translate_keyboard_state (keymap,
 *                                      gdk_key_event_get_keycode (event),
 *                                      state,
 *                                      gdk_key_event_get_group (event),
 *                                      &keyval, NULL, NULL, &consumed);
 * if (keyval == GDK_PLUS &&
 *     (state & ~consumed & ALL_ACCELS_MASK) == GDK_CONTROL_MASK)
 *   // Control was pressed
 * ]|
 * 
 * An older interpretation @consumed_modifiers was that it contained
 * all modifiers that might affect the translation of the key;
 * this allowed accelerators to be stored with irrelevant consumed
 * modifiers, by doing:
 * |[<!-- language="C" -->
 * // XXX Don’t do this XXX
 * if (keyval == accel_keyval &&
 *     (state & ~consumed & ALL_ACCELS_MASK) == (accel_mods & ~consumed))
 *   // Accelerator was pressed
 * ]|
 *
 * However, this did not work if multi-modifier combinations were
 * used in the keymap, since, for instance, `<Control>` would be
 * masked out even if only `<Control><Alt>` was used in the keymap.
 * To support this usage as well as well as possible, all single
 * modifier combinations that could affect the key for any combination
 * of modifiers will be returned in @consumed_modifiers; multi-modifier
 * combinations are returned only when actually found in @state. When
 * you store accelerators, you should always store them with consumed
 * modifiers removed. Store `<Control>plus`, not `<Control><Shift>plus`,
 *
 * Returns: %TRUE if there was a keyval bound to the keycode/state/group
 **/
gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     int              group,
                                     guint           *keyval,
                                     int             *effective_group,
                                     int             *level,
                                     GdkModifierType *consumed_modifiers)
{
  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), FALSE);

  return GDK_KEYMAP_GET_CLASS (keymap)->translate_keyboard_state (keymap,
								  hardware_keycode,
								  state,
								  group,
								  keyval,
								  effective_group,
								  level,
								  consumed_modifiers);
}

static int
gdk_keys_keyval_compare (const void *pkey, const void *pbase)
{
  return (*(int *) pkey) - ((gdk_key *) pbase)->keyval;
}

static char *
_gdk_keyval_name (guint keyval)
{
  static char buf[100];
  gdk_key *found;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((keyval & 0xff000000) == 0x01000000)
    {
      g_sprintf (buf, "U+%.04X", (keyval & 0x00ffffff));
      return buf;
    }

  found = bsearch (&keyval,
                   gdk_keys_by_keyval, G_N_ELEMENTS (gdk_keys_by_keyval),
                   sizeof (gdk_key),
                   gdk_keys_keyval_compare);

  if (found != NULL)
    {
      while ((found > gdk_keys_by_keyval) &&
             ((found - 1)->keyval == keyval))
        found--;

      return (char *) (keynames + found->offset);
    }
  else if (keyval != 0)
    {
      g_sprintf (buf, "%#x", keyval);
      return buf;
    }

  return NULL;
}

static int
gdk_keys_name_compare (const void *pkey, const void *pbase)
{
  return strcmp ((const char *) pkey,
                 (const char *) (keynames + ((const gdk_key *) pbase)->offset));
}

static guint
_gdk_keyval_from_name (const char *keyval_name)
{
  gdk_key *found;

  g_return_val_if_fail (keyval_name != NULL, 0);

  if (strncmp (keyval_name,"XF86", 4) == 0)
    keyval_name += 4;

  found = bsearch (keyval_name,
                   gdk_keys_by_name, G_N_ELEMENTS (gdk_keys_by_name),
                   sizeof (gdk_key),
                   gdk_keys_name_compare);
  if (found != NULL)
    return found->keyval;
  else
    return GDK_KEY_VoidSymbol;
}

/**
 * gdk_keyval_name:
 * @keyval: a key value
 *
 * Converts a key value into a symbolic name.
 *
 * The names are the same as those in the
 * `gdk/gdkkeysyms.h` header file
 * but without the leading “GDK_KEY_”.
 *
 * Returns: (nullable) (transfer none): a string containing the name
 *   of the key
 */
const char *
gdk_keyval_name (guint keyval)
{
  return _gdk_keyval_name (keyval);
}

/**
 * gdk_keyval_from_name:
 * @keyval_name: a key name
 *
 * Converts a key name to a key value.
 *
 * The names are the same as those in the
 * `gdk/gdkkeysyms.h` header file
 * but without the leading “GDK_KEY_”.
 *
 * Returns: the corresponding key value, or %GDK_KEY_VoidSymbol
 *   if the key name is not a valid key
 */
guint
gdk_keyval_from_name (const char *keyval_name)
{
  return _gdk_keyval_from_name (keyval_name);
}

/**
 * gdk_keyval_convert_case:
 * @symbol: a keyval
 * @lower: (out): return location for lowercase version of @symbol
 * @upper: (out): return location for uppercase version of @symbol
 *
 * Obtains the upper- and lower-case versions of the keyval @symbol.
 *
 * Examples of keyvals are `GDK_KEY_a`, `GDK_KEY_Enter`, `GDK_KEY_F1`, etc.
 */
void
gdk_keyval_convert_case (guint symbol,
                         guint *lower,
                         guint *upper)
{
  guint xlower, xupper;

  xlower = symbol;
  xupper = symbol;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((symbol & 0xff000000) == 0x01000000)
    {
      if (lower)
        *lower = gdk_unicode_to_keyval (g_unichar_tolower (symbol & 0x00ffffff));
      if (upper)
        *upper = gdk_unicode_to_keyval (g_unichar_toupper (symbol & 0x00ffffff));
      return;
    }

  switch (symbol >> 8)
    {
    case 0: /* Latin 1 */
      if ((symbol >= GDK_KEY_A) && (symbol <= GDK_KEY_Z))
        xlower += (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_a) && (symbol <= GDK_KEY_z))
        xupper -= (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_Agrave) && (symbol <= GDK_KEY_Odiaeresis))
        xlower += (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_agrave) && (symbol <= GDK_KEY_odiaeresis))
        xupper -= (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_Ooblique) && (symbol <= GDK_KEY_Thorn))
        xlower += (GDK_KEY_oslash - GDK_KEY_Ooblique);
      else if ((symbol >= GDK_KEY_oslash) && (symbol <= GDK_KEY_thorn))
        xupper -= (GDK_KEY_oslash - GDK_KEY_Ooblique);
      break;

    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol == GDK_KEY_Aogonek)
        xlower = GDK_KEY_aogonek;
      else if (symbol >= GDK_KEY_Lstroke && symbol <= GDK_KEY_Sacute)
        xlower += (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_Scaron && symbol <= GDK_KEY_Zacute)
        xlower += (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_Zcaron && symbol <= GDK_KEY_Zabovedot)
        xlower += (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol == GDK_KEY_aogonek)
        xupper = GDK_KEY_Aogonek;
      else if (symbol >= GDK_KEY_lstroke && symbol <= GDK_KEY_sacute)
        xupper -= (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_scaron && symbol <= GDK_KEY_zacute)
        xupper -= (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_zcaron && symbol <= GDK_KEY_zabovedot)
        xupper -= (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol >= GDK_KEY_Racute && symbol <= GDK_KEY_Tcedilla)
        xlower += (GDK_KEY_racute - GDK_KEY_Racute);
      else if (symbol >= GDK_KEY_racute && symbol <= GDK_KEY_tcedilla)
        xupper -= (GDK_KEY_racute - GDK_KEY_Racute);
      break;

    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Hstroke && symbol <= GDK_KEY_Hcircumflex)
        xlower += (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_Gbreve && symbol <= GDK_KEY_Jcircumflex)
        xlower += (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_hstroke && symbol <= GDK_KEY_hcircumflex)
        xupper -= (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_gbreve && symbol <= GDK_KEY_jcircumflex)
        xupper -= (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_Cabovedot && symbol <= GDK_KEY_Scircumflex)
        xlower += (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      else if (symbol >= GDK_KEY_cabovedot && symbol <= GDK_KEY_scircumflex)
        xupper -= (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      break;

    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Rcedilla && symbol <= GDK_KEY_Tslash)
        xlower += (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol >= GDK_KEY_rcedilla && symbol <= GDK_KEY_tslash)
        xupper -= (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol == GDK_KEY_ENG)
        xlower = GDK_KEY_eng;
      else if (symbol == GDK_KEY_eng)
        xupper = GDK_KEY_ENG;
      else if (symbol >= GDK_KEY_Amacron && symbol <= GDK_KEY_Umacron)
        xlower += (GDK_KEY_amacron - GDK_KEY_Amacron);
      else if (symbol >= GDK_KEY_amacron && symbol <= GDK_KEY_umacron)
        xupper -= (GDK_KEY_amacron - GDK_KEY_Amacron);
      break;

    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Serbian_DJE && symbol <= GDK_KEY_Serbian_DZE)
        xlower -= (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Serbian_dje && symbol <= GDK_KEY_Serbian_dze)
        xupper += (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Cyrillic_YU && symbol <= GDK_KEY_Cyrillic_HARDSIGN)
        xlower -= (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      else if (symbol >= GDK_KEY_Cyrillic_yu && symbol <= GDK_KEY_Cyrillic_hardsign)
        xupper += (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      break;

    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Greek_ALPHAaccent && symbol <= GDK_KEY_Greek_OMEGAaccent)
        xlower += (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_alphaaccent && symbol <= GDK_KEY_Greek_omegaaccent &&
               symbol != GDK_KEY_Greek_iotaaccentdieresis &&
               symbol != GDK_KEY_Greek_upsilonaccentdieresis)
        xupper -= (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_ALPHA && symbol <= GDK_KEY_Greek_OMEGA)
        xlower += (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      else if (symbol == GDK_KEY_Greek_finalsmallsigma)
        xupper = GDK_KEY_Greek_SIGMA;
      else if (symbol >= GDK_KEY_Greek_alpha && symbol <= GDK_KEY_Greek_omega)
        xupper -= (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      break;

    default:
      break;
    }

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
}
