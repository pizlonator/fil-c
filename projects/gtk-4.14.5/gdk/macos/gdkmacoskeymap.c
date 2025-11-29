/*
 * Copyright © 2000-2020 Red Hat, Inc.
 * Copyright © 2005 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
/* Some parts of this code come from quartzKeyboard.c,
 * from the Apple X11 Server.
 *
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation files
 *  (the "Software"), to deal in the Software without restriction,
 *  including without limitation the rights to use, copy, modify, merge,
 *  publish, distribute, sublicense, and/or sell copies of the Software,
 *  and to permit persons to whom the Software is furnished to do so,
 *  subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 *  HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 *  Except as contained in this notice, the name(s) of the above
 *  copyright holders shall not be used in advertising or otherwise to
 *  promote the sale, use or other dealings in this Software without
 *  prior written authorization.
 */

#include "config.h"

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <gdk/gdk.h>

#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"
#include "gdkmacoskeymap-private.h"

struct _GdkMacosKeymap
{
  GdkKeymap parent_instance;
};

struct _GdkMacosKeymapClass
{
  GdkKeymapClass parent_instance;
};

G_DEFINE_TYPE (GdkMacosKeymap, gdk_macos_keymap, GDK_TYPE_KEYMAP)

/* This is a table of all keyvals. Each keycode gets KEYVALS_PER_KEYCODE entries.
 * TThere is 1 keyval per modifier (Nothing, Shift, Alt, Shift+Alt);
 */
static guint *keyval_array = NULL;

#define NUM_KEYCODES 128
#define KEYVALS_PER_KEYCODE 4
#define GET_KEYVAL(keycode, group, level) \
  (keyval_array[(keycode * KEYVALS_PER_KEYCODE + group * 2 + level)])

const static struct {
  guint keycode;
  guint keyval;
  unsigned int modmask; /* So we can tell when a mod key is pressed/released */
} modifier_keys[] = {
  {  54, GDK_KEY_Meta_R,    NSEventModifierFlagCommand },
  {  55, GDK_KEY_Meta_L,    NSEventModifierFlagCommand },
  {  56, GDK_KEY_Shift_L,   NSEventModifierFlagShift },
  {  57, GDK_KEY_Caps_Lock, NSEventModifierFlagCapsLock },
  {  58, GDK_KEY_Alt_L,     NSEventModifierFlagOption },
  {  59, GDK_KEY_Control_L, NSEventModifierFlagControl },
  {  60, GDK_KEY_Shift_R,   NSEventModifierFlagShift },
  {  61, GDK_KEY_Alt_R,     NSEventModifierFlagOption },
  {  62, GDK_KEY_Control_R, NSEventModifierFlagControl }
};

const static struct {
  guint keycode;
  guint keyval;
} function_keys[] = {
  { 122, GDK_KEY_F1 },
  { 120, GDK_KEY_F2 },
  {  99, GDK_KEY_F3 },
  { 118, GDK_KEY_F4 },
  {  96, GDK_KEY_F5 },
  {  97, GDK_KEY_F6 },
  {  98, GDK_KEY_F7 },
  { 100, GDK_KEY_F8 },
  { 101, GDK_KEY_F9 },
  { 109, GDK_KEY_F10 },
  { 103, GDK_KEY_F11 },
  { 111, GDK_KEY_F12 },
  { 105, GDK_KEY_F13 },
  { 107, GDK_KEY_F14 },
  { 113, GDK_KEY_F15 },
  { 106, GDK_KEY_F16 }
};

const static struct {
  guint keycode;
  guint normal_keyval, keypad_keyval;
} known_numeric_keys[] = {
  { 65, GDK_KEY_period, GDK_KEY_KP_Decimal },
  { 67, GDK_KEY_asterisk, GDK_KEY_KP_Multiply },
  { 69, GDK_KEY_plus, GDK_KEY_KP_Add },
  { 75, GDK_KEY_slash, GDK_KEY_KP_Divide },
  { 76, GDK_KEY_Return, GDK_KEY_KP_Enter },
  { 78, GDK_KEY_minus, GDK_KEY_KP_Subtract },
  { 81, GDK_KEY_equal, GDK_KEY_KP_Equal },
  { 82, GDK_KEY_0, GDK_KEY_KP_0 },
  { 83, GDK_KEY_1, GDK_KEY_KP_1 },
  { 84, GDK_KEY_2, GDK_KEY_KP_2 },
  { 85, GDK_KEY_3, GDK_KEY_KP_3 },
  { 86, GDK_KEY_4, GDK_KEY_KP_4 },
  { 87, GDK_KEY_5, GDK_KEY_KP_5 },
  { 88, GDK_KEY_6, GDK_KEY_KP_6 },
  { 89, GDK_KEY_7, GDK_KEY_KP_7 },
  { 91, GDK_KEY_8, GDK_KEY_KP_8 },
  { 92, GDK_KEY_9, GDK_KEY_KP_9 }
};

/* Keys only in JIS layout.
 * The rationale of these key codes is <HIToolbox/Events.h> in Carbon.
 */
const static struct {
  guint keycode;
  guint keyval;
} jis_keys[] = {
#if 0
  /* Although These keys are also defined in <HIToolbox/Events.h>, they can be
   * translated by UCKeyTranslate correctly.
   */
  { 0x5D, GDK_KEY_yen },
  { 0x5E, GDK_KEY_underscore },
  { 0x5F, GDK_KEY_comma },
#endif
  /* These keys are unexpectedly translated to Space key by UCKeyTranslate,
   * and there is no suitable ucs value for them to add to special_ucs_table.
   * So we should translate them particularly.
   */
  { 0x66 /* 102 */, GDK_KEY_Eisu_toggle },
  { 0x68 /* 104 */, GDK_KEY_Hiragana }
};

/* These values aren't covered by gdk_unicode_to_keyval */
const static struct {
  gunichar ucs_value;
  guint keyval;
} special_ucs_table [] = {
  { 0x0001, GDK_KEY_Home },
  { 0x0003, GDK_KEY_Return },
  { 0x0004, GDK_KEY_End },
  { 0x0008, GDK_KEY_BackSpace },
  { 0x0009, GDK_KEY_Tab },
  { 0x000b, GDK_KEY_Page_Up },
  { 0x000c, GDK_KEY_Page_Down },
  { 0x000d, GDK_KEY_Return },
  { 0x001b, GDK_KEY_Escape },
  { 0x001c, GDK_KEY_Left },
  { 0x001d, GDK_KEY_Right },
  { 0x001e, GDK_KEY_Up },
  { 0x001f, GDK_KEY_Down },
  { 0x007f, GDK_KEY_Delete },
  { 0xf027, GDK_KEY_dead_acute },
  { 0xf060, GDK_KEY_dead_grave },
  { 0xf300, GDK_KEY_dead_grave },
  { 0xf0b4, GDK_KEY_dead_acute },
  { 0xf301, GDK_KEY_dead_acute },
  { 0xf385, GDK_KEY_dead_acute },
  { 0xf05e, GDK_KEY_dead_circumflex },
  { 0xf2c6, GDK_KEY_dead_circumflex },
  { 0xf302, GDK_KEY_dead_circumflex },
  { 0xf07e, GDK_KEY_dead_tilde },
  { 0xf2dc, GDK_KEY_dead_tilde },
  { 0xf303, GDK_KEY_dead_tilde },
  { 0xf342, GDK_KEY_dead_perispomeni },
  { 0xf0af, GDK_KEY_dead_macron },
  { 0xf304, GDK_KEY_dead_macron },
  { 0xf2d8, GDK_KEY_dead_breve },
  { 0xf306, GDK_KEY_dead_breve },
  { 0xf2d9, GDK_KEY_dead_abovedot },
  { 0xf307, GDK_KEY_dead_abovedot },
  { 0xf0a8, GDK_KEY_dead_diaeresis },
  { 0xf308, GDK_KEY_dead_diaeresis },
  { 0xf2da, GDK_KEY_dead_abovering },
  { 0xf30A, GDK_KEY_dead_abovering },
  { 0xf022, GDK_KEY_dead_doubleacute },
  { 0xf2dd, GDK_KEY_dead_doubleacute },
  { 0xf30B, GDK_KEY_dead_doubleacute },
  { 0xf2c7, GDK_KEY_dead_caron },
  { 0xf30C, GDK_KEY_dead_caron },
  { 0xf0be, GDK_KEY_dead_cedilla },
  { 0xf327, GDK_KEY_dead_cedilla },
  { 0xf2db, GDK_KEY_dead_ogonek },
  { 0xf328, GDK_KEY_dead_ogonek },
  { 0xfe5d, GDK_KEY_dead_iota },
  { 0xf323, GDK_KEY_dead_belowdot },
  { 0xf309, GDK_KEY_dead_hook },
  { 0xf31B, GDK_KEY_dead_horn },
  { 0xf02d, GDK_KEY_dead_stroke },
  { 0xf335, GDK_KEY_dead_stroke },
  { 0xf336, GDK_KEY_dead_stroke },
  { 0xf313, GDK_KEY_dead_abovecomma },
  /*  { 0xf313, GDK_KEY_dead_psili }, */
  { 0xf314, GDK_KEY_dead_abovereversedcomma },
  /*  { 0xf314, GDK_KEY_dead_dasia }, */
  { 0xf30F, GDK_KEY_dead_doublegrave },
  { 0xf325, GDK_KEY_dead_belowring },
  { 0xf2cd, GDK_KEY_dead_belowmacron },
  { 0xf331, GDK_KEY_dead_belowmacron },
  { 0xf32D, GDK_KEY_dead_belowcircumflex },
  { 0xf330, GDK_KEY_dead_belowtilde },
  { 0xf32E, GDK_KEY_dead_belowbreve },
  { 0xf324, GDK_KEY_dead_belowdiaeresis },
  { 0xf311, GDK_KEY_dead_invertedbreve },
  { 0xf02c, GDK_KEY_dead_belowcomma },
  { 0xf326, GDK_KEY_dead_belowcomma }
};

static void
gdk_macos_keymap_update (GdkMacosKeymap *self)
{
  const void *chr_data = NULL;
  guint *p;
  int i;

  TISInputSourceRef new_layout = TISCopyCurrentKeyboardLayoutInputSource ();
  CFDataRef layout_data_ref;

  g_free (keyval_array);
  keyval_array = g_new0 (guint, NUM_KEYCODES * KEYVALS_PER_KEYCODE);

  layout_data_ref = (CFDataRef)TISGetInputSourceProperty (new_layout, kTISPropertyUnicodeKeyLayoutData);

  if (layout_data_ref)
    chr_data = CFDataGetBytePtr (layout_data_ref);

  if (chr_data == NULL)
    {
      g_error ("cannot get keyboard layout data");
      return;
    }

  for (i = 0; i < NUM_KEYCODES; i++)
    {
      int j;
      UInt32 modifiers[] = {0, shiftKey, optionKey, shiftKey | optionKey};
      UniChar chars[4];
      UniCharCount nChars;

      p = keyval_array + i * KEYVALS_PER_KEYCODE;

      for (j = 0; j < KEYVALS_PER_KEYCODE; j++)
        {
          UInt32 state = 0;
          OSStatus err;
          UniChar uc;

          err = UCKeyTranslate (chr_data, i, kUCKeyActionDisplay,
                                (modifiers[j] >> 8) & 0xFF,
                                LMGetKbdType(),
                                0,
                                &state, 4, &nChars, chars);

          /* FIXME: Theoretically, we can get multiple UTF-16
           * values; we should convert them to proper unicode and
           * figure out whether there are really keyboard layouts
           * that give us more than one character for one
           * keypress.
           */
          if (err == noErr && nChars == 1)
            {
              int k;
              gboolean found = FALSE;

              /* A few <Shift><Option>keys return two characters,
               * the first of which is U+00a0, which isn't
               * interesting; so we return the second. More
               * sophisticated handling is the job of a
               * GtkIMContext.
               *
               * If state isn't zero, it means that it's a dead
               * key of some sort. Some of those are enumerated in
               * the special_ucs_table with the high nibble set to
               * f to push it into the private use range. Here we
               * do the same.
               */
              if (state != 0)
                chars[nChars - 1] |= 0xf000;
              uc = chars[nChars - 1];

              for (k = 0; k < G_N_ELEMENTS (special_ucs_table); k++)
                {
                  if (special_ucs_table[k].ucs_value == uc)
                    {
                      p[j] = special_ucs_table[k].keyval;
                      found = TRUE;
                      break;
                    }
                }

              /* Special-case shift-tab since GTK expects
               * GDK_KEY_ISO_Left_Tab for that.
               */
              if (found && p[j] == GDK_KEY_Tab && modifiers[j] == shiftKey)
                p[j] = GDK_KEY_ISO_Left_Tab;

              if (!found)
                p[j] = gdk_unicode_to_keyval (uc);
            }
        }

      if (p[3] == p[2])
        p[3] = 0;
      if (p[2] == p[1])
        p[2] = 0;
      if (p[1] == p[0])
        p[1] = 0;
      if (p[0] == p[2] &&
          p[1] == p[3])
        p[2] = p[3] = 0;
    }

  for (i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      p = keyval_array + modifier_keys[i].keycode * KEYVALS_PER_KEYCODE;

      if (p[0] == 0 && p[1] == 0 &&
          p[2] == 0 && p[3] == 0)
        p[0] = modifier_keys[i].keyval;
    }

  for (i = 0; i < G_N_ELEMENTS (function_keys); i++)
    {
      p = keyval_array + function_keys[i].keycode * KEYVALS_PER_KEYCODE;

      p[0] = function_keys[i].keyval;
      p[1] = p[2] = p[3] = 0;
    }

  for (i = 0; i < G_N_ELEMENTS (known_numeric_keys); i++)
    {
      p = keyval_array + known_numeric_keys[i].keycode * KEYVALS_PER_KEYCODE;

      if (p[0] == known_numeric_keys[i].normal_keyval)
        p[0] = known_numeric_keys[i].keypad_keyval;
    }

  for (i = 0; i < G_N_ELEMENTS (jis_keys); i++)
    {
      p = keyval_array + jis_keys[i].keycode * KEYVALS_PER_KEYCODE;
      p[0] = jis_keys[i].keyval;
      p[1] = p[2] = p[3] = 0;
    }

  g_signal_emit_by_name (self, "keys-changed");
}

static PangoDirection
gdk_macos_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_macos_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_macos_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_macos_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_macos_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static guint
gdk_macos_keymap_lookup_key (GdkKeymap          *keymap,
                             const GdkKeymapKey *key)
{
  GdkMacosKeymap *self = (GdkMacosKeymap *)keymap;

  g_assert (GDK_IS_MACOS_KEYMAP (self));
  g_assert (key != NULL);

  return GET_KEYVAL (key->keycode, key->group, key->level);
}

static guint
translate_keysym (guint            hardware_keycode,
                  int              group,
                  GdkModifierType  state,
                  int             *effective_group,
                  int             *effective_level)
{
  int level;
  guint tmp_keyval;

  level = (state & GDK_SHIFT_MASK) ? 1 : 0;

  if (!(GET_KEYVAL (hardware_keycode, group, 0) || GET_KEYVAL (hardware_keycode, group, 1)) &&
      (GET_KEYVAL (hardware_keycode, 0, 0) || GET_KEYVAL (hardware_keycode, 0, 1)))
    group = 0;

  if (!GET_KEYVAL (hardware_keycode, group, level) &&
      GET_KEYVAL (hardware_keycode, group, 0))
    level = 0;

  tmp_keyval = GET_KEYVAL (hardware_keycode, group, level);

  if (state & GDK_LOCK_MASK)
    {
      guint upper = gdk_keyval_to_upper (tmp_keyval);
      if (upper != tmp_keyval)
        tmp_keyval = upper;
    }

  if (effective_group)
    *effective_group = group;
  if (effective_level)
    *effective_level = level;

  return tmp_keyval;
}

static gboolean
gdk_macos_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                         guint      keyval,
                                         GArray    *keys)
{
  gboolean ret = FALSE;

  g_assert (GDK_IS_MACOS_KEYMAP (keymap));
  g_assert (keys != NULL);

  for (guint i = 0; i < NUM_KEYCODES * KEYVALS_PER_KEYCODE; i++)
    {
      GdkKeymapKey key;

      if (keyval_array[i] != keyval)
        continue;

      key.keycode = i / KEYVALS_PER_KEYCODE;
      key.group = (i % KEYVALS_PER_KEYCODE) >= 2;
      key.level = i % 2;

      g_array_append_val (keys, key);

      ret = TRUE;
    }

  return ret;
}

static gboolean
gdk_macos_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                          guint          hardware_keycode,
                                          GdkKeymapKey **keys,
                                          guint        **keyvals,
                                          int           *n_entries)
{
  GArray *keys_array;
  GArray *keyvals_array;
  guint *p;
  guint i;

  g_assert (GDK_IS_MACOS_KEYMAP (keymap));
  g_assert (keyvals != NULL);
  g_assert (n_entries != NULL);

  *keyvals = NULL;
  *n_entries = 0;

  if (hardware_keycode > NUM_KEYCODES)
    return FALSE;

  if (keys)
    keys_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    keys_array = NULL;

  if (keyvals)
    keyvals_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyvals_array = NULL;

  p = keyval_array + hardware_keycode * KEYVALS_PER_KEYCODE;

  for (i = 0; i < KEYVALS_PER_KEYCODE; i++)
    {
      if (!p[i])
        continue;

      (*n_entries)++;

      if (keyvals_array)
        g_array_append_val (keyvals_array, p[i]);

      if (keys_array)
        {
          GdkKeymapKey key;

          key.keycode = hardware_keycode;
          key.group = i >= 2;
          key.level = i % 2;

          g_array_append_val (keys_array, key);
        }
    }

  if (keys)
    *keys = (GdkKeymapKey *)(gpointer)g_array_free (keys_array, FALSE);

  if (keyvals)
    *keyvals = (guint *)(gpointer)g_array_free (keyvals_array, FALSE);

  return *n_entries > 0;
}

static gboolean
gdk_macos_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                           guint            hardware_keycode,
                                           GdkModifierType  state,
                                           int              group,
                                           guint           *keyval,
                                           int             *effective_group,
                                           int             *level,
                                           GdkModifierType *consumed_modifiers)
{
  guint tmp_keyval;
  GdkModifierType bit;

  g_assert (GDK_IS_MACOS_KEYMAP (keymap));

  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  if (hardware_keycode < 0 || hardware_keycode >= NUM_KEYCODES)
    return FALSE;

  tmp_keyval = translate_keysym (hardware_keycode, group, state, level, effective_group);

  /* Check if modifiers modify the keyval */
  if (consumed_modifiers)
    {
      guint tmp_modifiers = (state & GDK_MODIFIER_MASK);

      for (bit = 1; bit <= tmp_modifiers; bit <<= 1)
        {
          if ((bit & tmp_modifiers) &&
              translate_keysym (hardware_keycode, group, state & ~bit,
                                NULL, NULL) == tmp_keyval)
            tmp_modifiers &= ~bit;
        }

      *consumed_modifiers = tmp_modifiers;
    }

  if (keyval)
    *keyval = tmp_keyval;

  return TRUE;
}

static void
input_sources_changed_notification (CFNotificationCenterRef  center,
                                    void                    *observer,
                                    CFStringRef              name,
                                    const void              *object,
                                    CFDictionaryRef          userInfo)
{
  GdkMacosKeymap *self = observer;

  g_assert (GDK_IS_MACOS_KEYMAP (self));

  gdk_macos_keymap_update (self);
}

static void
gdk_macos_keymap_finalize (GObject *object)
{
  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      object,
                                      CFSTR ("AppleSelectedInputSourcesChangedNotification"),
                                      NULL);

  G_OBJECT_CLASS (gdk_macos_keymap_parent_class)->finalize (object);
}

static void
gdk_macos_keymap_class_init (GdkMacosKeymapClass *klass)
{
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_macos_keymap_finalize;

  keymap_class->get_caps_lock_state = gdk_macos_keymap_get_caps_lock_state;
  keymap_class->get_direction = gdk_macos_keymap_get_direction;
  keymap_class->get_entries_for_keycode = gdk_macos_keymap_get_entries_for_keycode;
  keymap_class->get_entries_for_keyval = gdk_macos_keymap_get_entries_for_keyval;
  keymap_class->get_num_lock_state = gdk_macos_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_macos_keymap_get_scroll_lock_state;
  keymap_class->have_bidi_layouts = gdk_macos_keymap_have_bidi_layouts;
  keymap_class->lookup_key = gdk_macos_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_macos_keymap_translate_keyboard_state;
}

static void
gdk_macos_keymap_init (GdkMacosKeymap *self)
{
  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   self,
                                   input_sources_changed_notification,
                                   CFSTR ("AppleSelectedInputSourcesChangedNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);
  gdk_macos_keymap_update (self);
}

GdkMacosKeymap *
_gdk_macos_keymap_new (GdkMacosDisplay *display)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_MACOS_KEYMAP,
                       "display", display,
                       NULL);
}

/* What sort of key event is this? Returns one of
 * GDK_KEY_PRESS, GDK_KEY_RELEASE, GDK_NOTHING (should be ignored)
 */
GdkEventType
_gdk_macos_keymap_get_event_type (NSEvent *event)
{
  unsigned short keycode;
  unsigned int flags;

  switch ((int)[event type])
    {
    case NSEventTypeKeyDown:
      return GDK_KEY_PRESS;
    case NSEventTypeKeyUp:
      return GDK_KEY_RELEASE;
    case NSEventTypeFlagsChanged:
      break;
    default:
      g_assert_not_reached ();
    }

  /* For flags-changed events, we have to find the special key that caused the
   * event, and see if it's in the modifier mask. */
  keycode = [event keyCode];
  flags = [event modifierFlags];

  for (guint i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      if (modifier_keys[i].keycode == keycode)
        {
          if (flags & modifier_keys[i].modmask)
            return GDK_KEY_PRESS;
          else
            return GDK_KEY_RELEASE;
        }
    }

  /* Some keypresses (eg: Expose' activations) seem to trigger flags-changed
   * events for no good reason. Ignore them! */
  return 0;
}

gboolean
_gdk_macos_keymap_is_modifier (guint keycode)
{
  for (guint i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      if (modifier_keys[i].modmask == 0)
        break;

      if (modifier_keys[i].keycode == keycode)
        return TRUE;
    }

  return FALSE;
}

/*
 * Code for key code conversion
 *
 * Copyright (C) 2009 Paul Davis
 */
gunichar
_gdk_macos_keymap_get_equivalent (guint key)
{
  if (key >= GDK_KEY_A && key <= GDK_KEY_Z)
    return key + (GDK_KEY_a - GDK_KEY_A);

  if (key >= GDK_KEY_space && key <= GDK_KEY_asciitilde)
    return key;

  switch (key)
    {
      case GDK_KEY_BackSpace:
        return NSBackspaceCharacter;
      case GDK_KEY_Delete:
        return NSDeleteFunctionKey;
      case GDK_KEY_Pause:
        return NSPauseFunctionKey;
      case GDK_KEY_Scroll_Lock:
        return NSScrollLockFunctionKey;
      case GDK_KEY_Sys_Req:
        return NSSysReqFunctionKey;
      case GDK_KEY_Home:
        return NSHomeFunctionKey;
      case GDK_KEY_Left:
      case GDK_KEY_leftarrow:
        return NSLeftArrowFunctionKey;
      case GDK_KEY_Up:
      case GDK_KEY_uparrow:
        return NSUpArrowFunctionKey;
      case GDK_KEY_Right:
      case GDK_KEY_rightarrow:
        return NSRightArrowFunctionKey;
      case GDK_KEY_Down:
      case GDK_KEY_downarrow:
        return NSDownArrowFunctionKey;
      case GDK_KEY_Page_Up:
        return NSPageUpFunctionKey;
      case GDK_KEY_Page_Down:
        return NSPageDownFunctionKey;
      case GDK_KEY_End:
        return NSEndFunctionKey;
      case GDK_KEY_Begin:
        return NSBeginFunctionKey;
      case GDK_KEY_Select:
        return NSSelectFunctionKey;
      case GDK_KEY_Print:
        return NSPrintFunctionKey;
      case GDK_KEY_Execute:
        return NSExecuteFunctionKey;
      case GDK_KEY_Insert:
        return NSInsertFunctionKey;
      case GDK_KEY_Undo:
        return NSUndoFunctionKey;
      case GDK_KEY_Redo:
        return NSRedoFunctionKey;
      case GDK_KEY_Menu:
        return NSMenuFunctionKey;
      case GDK_KEY_Find:
        return NSFindFunctionKey;
      case GDK_KEY_Help:
        return NSHelpFunctionKey;
      case GDK_KEY_Break:
        return NSBreakFunctionKey;
      case GDK_KEY_Mode_switch:
        return NSModeSwitchFunctionKey;
      case GDK_KEY_F1:
        return NSF1FunctionKey;
      case GDK_KEY_F2:
        return NSF2FunctionKey;
      case GDK_KEY_F3:
        return NSF3FunctionKey;
      case GDK_KEY_F4:
        return NSF4FunctionKey;
      case GDK_KEY_F5:
        return NSF5FunctionKey;
      case GDK_KEY_F6:
        return NSF6FunctionKey;
      case GDK_KEY_F7:
        return NSF7FunctionKey;
      case GDK_KEY_F8:
        return NSF8FunctionKey;
      case GDK_KEY_F9:
        return NSF9FunctionKey;
      case GDK_KEY_F10:
        return NSF10FunctionKey;
      case GDK_KEY_F11:
        return NSF11FunctionKey;
      case GDK_KEY_F12:
        return NSF12FunctionKey;
      case GDK_KEY_F13:
        return NSF13FunctionKey;
      case GDK_KEY_F14:
        return NSF14FunctionKey;
      case GDK_KEY_F15:
        return NSF15FunctionKey;
      case GDK_KEY_F16:
        return NSF16FunctionKey;
      case GDK_KEY_F17:
        return NSF17FunctionKey;
      case GDK_KEY_F18:
        return NSF18FunctionKey;
      case GDK_KEY_F19:
        return NSF19FunctionKey;
      case GDK_KEY_F20:
        return NSF20FunctionKey;
      case GDK_KEY_F21:
        return NSF21FunctionKey;
      case GDK_KEY_F22:
        return NSF22FunctionKey;
      case GDK_KEY_F23:
        return NSF23FunctionKey;
      case GDK_KEY_F24:
        return NSF24FunctionKey;
      case GDK_KEY_F25:
        return NSF25FunctionKey;
      case GDK_KEY_F26:
        return NSF26FunctionKey;
      case GDK_KEY_F27:
        return NSF27FunctionKey;
      case GDK_KEY_F28:
        return NSF28FunctionKey;
      case GDK_KEY_F29:
        return NSF29FunctionKey;
      case GDK_KEY_F30:
        return NSF30FunctionKey;
      case GDK_KEY_F31:
        return NSF31FunctionKey;
      case GDK_KEY_F32:
        return NSF32FunctionKey;
      case GDK_KEY_F33:
        return NSF33FunctionKey;
      case GDK_KEY_F34:
        return NSF34FunctionKey;
      case GDK_KEY_F35:
        return NSF35FunctionKey;
      default:
        break;
    }

  return '\0';
}
