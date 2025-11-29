/* GTK - The GIMP Toolkit
 * Copyright (C) 1998, 2001 Tim Janik
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
#include <string.h>
#include <stdlib.h>

#include "gtkaccelgroup.h"
#include "gtkaccelgroupprivate.h"
#include <glib/gi18n-lib.h>
#include "gtkmarshalers.h"
#include "gtkprivate.h"

/* --- functions --- */
/**
 * gtk_accelerator_valid:
 * @keyval: a GDK keyval
 * @modifiers: modifier mask
 *
 * Determines whether a given keyval and modifier mask constitute
 * a valid keyboard accelerator.
 *
 * For example, the %GDK_KEY_a keyval plus %GDK_CONTROL_MASK mark is valid,
 * and matches the “Ctrl+a” accelerator. But, you can't, for instance, use
 * the %GDK_KEY_Control_L keyval as an accelerator.
 *
 * Returns: %TRUE if the accelerator is valid
 */
gboolean
gtk_accelerator_valid (guint           keyval,
                       GdkModifierType modifiers)
{
  static const guint invalid_accelerator_vals[] = {
    GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Shift_Lock,
    GDK_KEY_Caps_Lock, GDK_KEY_ISO_Lock, GDK_KEY_Control_L,
    GDK_KEY_Control_R, GDK_KEY_Meta_L, GDK_KEY_Meta_R,
    GDK_KEY_Alt_L, GDK_KEY_Alt_R, GDK_KEY_Super_L, GDK_KEY_Super_R,
    GDK_KEY_Hyper_L, GDK_KEY_Hyper_R, GDK_KEY_ISO_Level3_Shift,
    GDK_KEY_ISO_Next_Group, GDK_KEY_ISO_Prev_Group,
    GDK_KEY_ISO_First_Group, GDK_KEY_ISO_Last_Group,
    GDK_KEY_Mode_switch, GDK_KEY_Num_Lock, GDK_KEY_Multi_key,
    GDK_KEY_Scroll_Lock, GDK_KEY_Sys_Req,
    GDK_KEY_Tab, GDK_KEY_ISO_Left_Tab, GDK_KEY_KP_Tab,
    GDK_KEY_First_Virtual_Screen, GDK_KEY_Prev_Virtual_Screen,
    GDK_KEY_Next_Virtual_Screen, GDK_KEY_Last_Virtual_Screen,
    GDK_KEY_Terminate_Server, GDK_KEY_AudibleBell_Enable,
    0
  };
  static const guint invalid_unmodified_vals[] = {
    GDK_KEY_Up, GDK_KEY_Down, GDK_KEY_Left, GDK_KEY_Right,
    GDK_KEY_KP_Up, GDK_KEY_KP_Down, GDK_KEY_KP_Left, GDK_KEY_KP_Right,
    0
  };
  const guint *ac_val;

  modifiers &= GDK_MODIFIER_MASK;

  if (keyval <= 0xFF)
    return keyval >= 0x20;

  ac_val = invalid_accelerator_vals;
  while (*ac_val)
    {
      if (keyval == *ac_val++)
        return FALSE;
    }

  if (!modifiers)
    {
      ac_val = invalid_unmodified_vals;
      while (*ac_val)
        {
          if (keyval == *ac_val++)
            return FALSE;
        }
    }

  return TRUE;
}

static inline gboolean
is_alt (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'a' || string[1] == 'A') &&
          (string[2] == 'l' || string[2] == 'L') &&
          (string[3] == 't' || string[3] == 'T') &&
          (string[4] == '>'));
}

static inline gboolean
is_ctl (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'c' || string[1] == 'C') &&
          (string[2] == 't' || string[2] == 'T') &&
          (string[3] == 'l' || string[3] == 'L') &&
          (string[4] == '>'));
}

static inline gboolean
is_ctrl (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'c' || string[1] == 'C') &&
          (string[2] == 't' || string[2] == 'T') &&
          (string[3] == 'r' || string[3] == 'R') &&
          (string[4] == 'l' || string[4] == 'L') &&
          (string[5] == '>'));
}

static inline gboolean
is_shft (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 's' || string[1] == 'S') &&
          (string[2] == 'h' || string[2] == 'H') &&
          (string[3] == 'f' || string[3] == 'F') &&
          (string[4] == 't' || string[4] == 'T') &&
          (string[5] == '>'));
}

static inline gboolean
is_shift (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 's' || string[1] == 'S') &&
          (string[2] == 'h' || string[2] == 'H') &&
          (string[3] == 'i' || string[3] == 'I') &&
          (string[4] == 'f' || string[4] == 'F') &&
          (string[5] == 't' || string[5] == 'T') &&
          (string[6] == '>'));
}

static inline gboolean
is_control (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'c' || string[1] == 'C') &&
          (string[2] == 'o' || string[2] == 'O') &&
          (string[3] == 'n' || string[3] == 'N') &&
          (string[4] == 't' || string[4] == 'T') &&
          (string[5] == 'r' || string[5] == 'R') &&
          (string[6] == 'o' || string[6] == 'O') &&
          (string[7] == 'l' || string[7] == 'L') &&
          (string[8] == '>'));
}

static inline gboolean
is_meta (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'm' || string[1] == 'M') &&
          (string[2] == 'e' || string[2] == 'E') &&
          (string[3] == 't' || string[3] == 'T') &&
          (string[4] == 'a' || string[4] == 'A') &&
          (string[5] == '>'));
}

static inline gboolean
is_super (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 's' || string[1] == 'S') &&
          (string[2] == 'u' || string[2] == 'U') &&
          (string[3] == 'p' || string[3] == 'P') &&
          (string[4] == 'e' || string[4] == 'E') &&
          (string[5] == 'r' || string[5] == 'R') &&
          (string[6] == '>'));
}

static inline gboolean
is_hyper (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'h' || string[1] == 'H') &&
          (string[2] == 'y' || string[2] == 'Y') &&
          (string[3] == 'p' || string[3] == 'P') &&
          (string[4] == 'e' || string[4] == 'E') &&
          (string[5] == 'r' || string[5] == 'R') &&
          (string[6] == '>'));
}

static inline gboolean
is_primary (const char *string)
{
  return ((string[0] == '<') &&
          (string[1] == 'p' || string[1] == 'P') &&
          (string[2] == 'r' || string[2] == 'R') &&
          (string[3] == 'i' || string[3] == 'I') &&
          (string[4] == 'm' || string[4] == 'M') &&
          (string[5] == 'a' || string[5] == 'A') &&
          (string[6] == 'r' || string[6] == 'R') &&
          (string[7] == 'y' || string[7] == 'Y') &&
          (string[8] == '>'));
}

static inline gboolean
is_keycode (const char *string)
{
  return (string[0] == '0' &&
          string[1] == 'x' &&
          g_ascii_isxdigit (string[2]) &&
          g_ascii_isxdigit (string[3]));
}

/**
 * gtk_accelerator_parse_with_keycode:
 * @accelerator: string representing an accelerator
 * @display: (nullable): the `GdkDisplay` to look up @accelerator_codes in
 * @accelerator_key: (out) (optional): return location for accelerator keyval
 * @accelerator_codes: (out) (array zero-terminated=1) (transfer full) (optional):
 *   return location for accelerator keycodes
 * @accelerator_mods: (out) (optional): return location for accelerator
 *   modifier mask
 *
 * Parses a string representing an accelerator.
 *
 * This is similar to [func@Gtk.accelerator_parse] but handles keycodes as
 * well. This is only useful for system-level components, applications should
 * use [func@Gtk.accelerator_parse] instead.
 *
 * If @accelerator_codes is given and the result stored in it is non-%NULL,
 * the result must be freed with g_free().
 *
 * If a keycode is present in the accelerator and no @accelerator_codes
 * is given, the parse will fail.
 *
 * If the parse fails, @accelerator_key, @accelerator_mods and
 * @accelerator_codes will be set to 0 (zero).
 *
 * Returns: %TRUE if parsing succeeded
 */
gboolean
gtk_accelerator_parse_with_keycode (const char      *accelerator,
                                    GdkDisplay      *display,
                                    guint           *accelerator_key,
                                    guint          **accelerator_codes,
                                    GdkModifierType *accelerator_mods)
{
  guint keyval;
  GdkModifierType mods;
  int len;
  gboolean error;

  if (accelerator_key)
    *accelerator_key = 0;
  if (accelerator_mods)
    *accelerator_mods = 0;
  if (accelerator_codes)
    *accelerator_codes = NULL;

  g_return_val_if_fail (accelerator != NULL, FALSE);

  if (!display)
    display = gdk_display_get_default ();

  error = FALSE;
  keyval = 0;
  mods = 0;
  len = strlen (accelerator);
  while (len)
    {
      if (*accelerator == '<')
        {
          if (len >= 9 && is_primary (accelerator))
            {
              accelerator += 9;
              len -= 9;
              mods |= GDK_CONTROL_MASK;
            }
          else if (len >= 9 && is_control (accelerator))
            {
              accelerator += 9;
              len -= 9;
              mods |= GDK_CONTROL_MASK;
            }
          else if (len >= 7 && is_shift (accelerator))
            {
              accelerator += 7;
              len -= 7;
              mods |= GDK_SHIFT_MASK;
            }
          else if (len >= 6 && is_shft (accelerator))
            {
              accelerator += 6;
              len -= 6;
              mods |= GDK_SHIFT_MASK;
            }
          else if (len >= 6 && is_ctrl (accelerator))
            {
              accelerator += 6;
              len -= 6;
              mods |= GDK_CONTROL_MASK;
            }
          else if (len >= 5 && is_ctl (accelerator))
            {
              accelerator += 5;
              len -= 5;
              mods |= GDK_CONTROL_MASK;
            }
          else if (len >= 5 && is_alt (accelerator))
            {
              accelerator += 5;
              len -= 5;
              mods |= GDK_ALT_MASK;
            }
          else if (len >= 6 && is_meta (accelerator))
            {
              accelerator += 6;
              len -= 6;
              mods |= GDK_META_MASK;
            }
          else if (len >= 7 && is_hyper (accelerator))
            {
              accelerator += 7;
              len -= 7;
              mods |= GDK_HYPER_MASK;
            }
          else if (len >= 7 && is_super (accelerator))
            {
              accelerator += 7;
              len -= 7;
              mods |= GDK_SUPER_MASK;
            }
          else
            {
              char last_ch;

              last_ch = *accelerator;
              while (last_ch && last_ch != '>')
                {
                  accelerator += 1;
                  len -= 1;
                  last_ch = *accelerator;
                }
            }
        }
      else
        {
          if (len >= 4 && is_keycode (accelerator))
            {
               char keystring[5];
               char *endptr;
               int tmp_keycode;

               memcpy (keystring, accelerator, 4);
               keystring [4] = '\000';

               tmp_keycode = strtol (keystring, &endptr, 16);

               if (endptr == NULL || *endptr != '\000')
                 {
                   error = TRUE;
                   goto out;
                 }
               else if (accelerator_codes != NULL)
                 {
                   /* 0x00 is an invalid keycode too. */
                   if (tmp_keycode == 0)
                     {
                       error = TRUE;
                       goto out;
                     }
                   else
                     {
                       *accelerator_codes = g_new0 (guint, 2);
                       (*accelerator_codes)[0] = tmp_keycode;
                     }
                 }
               else
                 {
                   /* There was a keycode in the string, but
                    * we cannot store it, so we have an error */
                   error = TRUE;
                   goto out;
                 }
            }
          else
            {
              keyval = gdk_keyval_from_name (accelerator);
              if (keyval == GDK_KEY_VoidSymbol)
                {
                  error = TRUE;
                  goto out;
                }
            }

          if (keyval && accelerator_codes != NULL)
            {
              GdkKeymapKey *keys;
              int n_keys, i, j;

              if (!gdk_display_map_keyval (display, keyval, &keys, &n_keys))
                {
                  /* Not in keymap */
                  error = TRUE;
                  goto out;
                }
              else
                {
                  *accelerator_codes = g_new0 (guint, n_keys + 1);

                  /* Prefer level-0 group-0 keys to modified keys */
                  for (i = 0, j = 0; i < n_keys; ++i)
                    {
                      if (keys[i].level == 0 && keys[i].group == 0)
                        (*accelerator_codes)[j++] = keys[i].keycode;
                    }

                  /* No level-0 group-0 keys? Find in the whole group-0 */
                  if (j == 0)
                    {
                      for (i = 0, j = 0; i < n_keys; ++i)
                        {
                          if (keys[i].group == 0)
                            (*accelerator_codes)[j++] = keys[i].keycode;
                        }
                    }

                  /* Still nothing? Try in other groups */
                  if (j == 0)
                    {
                      for (i = 0, j = 0; i < n_keys; ++i)
                        (*accelerator_codes)[j++] = keys[i].keycode;
                    }

                  if (j == 0)
                    {
                      g_free (*accelerator_codes);
                      *accelerator_codes = NULL;
                      /* Not in keymap */
                      error = TRUE;
                      goto out;
                    }
                  g_free (keys);
                }
            }

          accelerator += len;
          len -= len;
        }
    }

out:
  if (error)
    keyval = mods = 0;

  if (accelerator_key)
    *accelerator_key = gdk_keyval_to_lower (keyval);
  if (accelerator_mods)
    *accelerator_mods = mods;

  return !error;
}

/**
 * gtk_accelerator_parse:
 * @accelerator: string representing an accelerator
 * @accelerator_key: (out) (optional): return location for accelerator keyval
 * @accelerator_mods: (out) (optional): return location for accelerator
 *   modifier mask
 *
 * Parses a string representing an accelerator.
 *
 * The format looks like “`<Control>a`” or “`<Shift><Alt>F1`”.
 *
 * The parser is fairly liberal and allows lower or upper case, and also
 * abbreviations such as “`<Ctl>`” and “`<Ctrl>`”.
 *
 * Key names are parsed using [func@Gdk.keyval_from_name]. For character keys
 * the name is not the symbol, but the lowercase name, e.g. one would use
 * “`<Ctrl>minus`” instead of “`<Ctrl>-`”.
 *
 * Modifiers are enclosed in angular brackets `<>`, and match the
 * [flags@Gdk.ModifierType] mask:
 *
 * - `<Shift>` for `GDK_SHIFT_MASK`
 * - `<Ctrl>` for `GDK_CONTROL_MASK`
 * - `<Alt>` for `GDK_ALT_MASK`
 * - `<Meta>` for `GDK_META_MASK`
 * - `<Super>` for `GDK_SUPER_MASK`
 * - `<Hyper>` for `GDK_HYPER_MASK`
 *
 * If the parse operation fails, @accelerator_key and @accelerator_mods will
 * be set to 0 (zero).
 */
gboolean
gtk_accelerator_parse (const char      *accelerator,
                       guint           *accelerator_key,
                       GdkModifierType *accelerator_mods)
{
  return gtk_accelerator_parse_with_keycode (accelerator, NULL, accelerator_key, NULL, accelerator_mods);
}

/**
 * gtk_accelerator_name_with_keycode:
 * @display: (nullable): a `GdkDisplay` or %NULL to use the default display
 * @accelerator_key: accelerator keyval
 * @keycode: accelerator keycode
 * @accelerator_mods: accelerator modifier mask
 *
 * Converts an accelerator keyval and modifier mask
 * into a string parseable by gtk_accelerator_parse_with_keycode().
 *
 * This is similar to [func@Gtk.accelerator_name] but handling keycodes.
 * This is only useful for system-level components, applications
 * should use [func@Gtk.accelerator_name] instead.
 *
 * Returns: a newly allocated accelerator name.
 */
char *
gtk_accelerator_name_with_keycode (GdkDisplay      *display,
                                   guint            accelerator_key,
                                   guint            keycode,
                                   GdkModifierType  accelerator_mods)
{
  char *gtk_name;

  gtk_name = gtk_accelerator_name (accelerator_key, accelerator_mods);

  if (!accelerator_key)
    {
      char *name;
      name = g_strdup_printf ("%s0x%02x", gtk_name, keycode);
      g_free (gtk_name);
      return name;
    }

  return gtk_name;
}

/**
 * gtk_accelerator_name:
 * @accelerator_key: accelerator keyval
 * @accelerator_mods: accelerator modifier mask
 *
 * Converts an accelerator keyval and modifier mask into a string
 * parseable by gtk_accelerator_parse().
 *
 * For example, if you pass in %GDK_KEY_q and %GDK_CONTROL_MASK,
 * this function returns `<Control>q`.
 *
 * If you need to display accelerators in the user interface,
 * see [func@Gtk.accelerator_get_label].
 *
 * Returns: (transfer full): a newly-allocated accelerator name
 */
char *
gtk_accelerator_name (guint           accelerator_key,
                      GdkModifierType accelerator_mods)
{
#define TXTLEN(s) sizeof (s) - 1
  static const struct {
    guint mask;
    const char *text;
    gsize text_len;
  } mask_text[] = {
    { GDK_SHIFT_MASK,   "<Shift>",   TXTLEN ("<Shift>") },
    { GDK_CONTROL_MASK, "<Control>", TXTLEN ("<Control>") },
    { GDK_ALT_MASK,     "<Alt>",     TXTLEN ("<Alt>") },
    { GDK_META_MASK,    "<Meta>",    TXTLEN ("<Meta>") },
    { GDK_SUPER_MASK,   "<Super>",   TXTLEN ("<Super>") },
    { GDK_HYPER_MASK,   "<Hyper>",   TXTLEN ("<Hyper>") }
  };
#undef TXTLEN
  GdkModifierType saved_mods;
  guint l;
  guint name_len;
  const char *keyval_name;
  char *accelerator;
  int i;

  accelerator_mods &= GDK_MODIFIER_MASK;

  keyval_name = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
  if (!keyval_name)
    keyval_name = "";

  name_len = strlen (keyval_name);

  saved_mods = accelerator_mods;
  for (i = 0; i < G_N_ELEMENTS (mask_text); i++)
    {
      if (accelerator_mods & mask_text[i].mask)
        name_len += mask_text[i].text_len;
    }

  if (name_len == 0)
    return g_strdup (keyval_name);

  name_len += 1; /* NUL byte */
  accelerator = g_new (char, name_len);

  accelerator_mods = saved_mods;
  l = 0;
  for (i = 0; i < G_N_ELEMENTS (mask_text); i++)
    {
      if (accelerator_mods & mask_text[i].mask)
        {
          strcpy (accelerator + l, mask_text[i].text);
          l += mask_text[i].text_len;
        }
    }

  strcpy (accelerator + l, keyval_name);
  accelerator[name_len - 1] = '\0';

  return accelerator;
}

/**
 * gtk_accelerator_get_label_with_keycode:
 * @display: (nullable): a `GdkDisplay` or %NULL to use the default display
 * @accelerator_key: accelerator keyval
 * @keycode: accelerator keycode
 * @accelerator_mods: accelerator modifier mask
 *
 * Converts an accelerator keyval and modifier mask
 * into a string that can be displayed to the user.
 *
 * The string may be translated.
 *
 * This function is similar to [func@Gtk.accelerator_get_label],
 * but handling keycodes. This is only useful for system-level
 * components, applications should use [func@Gtk.accelerator_get_label]
 * instead.
 *
 * Returns: (transfer full): a newly-allocated string representing the accelerator
 */
char *
gtk_accelerator_get_label_with_keycode (GdkDisplay      *display,
                                        guint            accelerator_key,
                                        guint            keycode,
                                        GdkModifierType  accelerator_mods)
{
  char *gtk_label;

  gtk_label = gtk_accelerator_get_label (accelerator_key, accelerator_mods);

  if (!accelerator_key)
    {
      char *label;
      label = g_strdup_printf ("%s0x%02x", gtk_label, keycode);
      g_free (gtk_label);
      return label;
    }

  return gtk_label;
}

/* Underscores in key names are better displayed as spaces
 * E.g., Page_Up should be “Page Up”.
 *
 * Some keynames also have prefixes that are not suitable
 * for display, e.g XF86AudioMute, so strip those out, too.
 *
 * This function is only called on untranslated keynames,
 * so no need to be UTF-8 safe.
 */
static void
append_without_underscores (GString    *s,
                            const char *str)
{
  const char *p;

  if (g_str_has_prefix (str, "XF86"))
    p = str + 4;
  else if (g_str_has_prefix (str, "ISO_"))
    p = str + 4;
  else
    p = str;

  for ( ; *p; p++)
    {
      if (*p == '_')
        g_string_append_c (s, ' ');
      else
        g_string_append_c (s, *p);
    }
}

/* On Mac, if the key has symbolic representation (e.g. arrow keys),
 * append it to gstring and return TRUE; otherwise return FALSE.
 * See http://docs.info.apple.com/article.html?path=Mac/10.5/en/cdb_symbs.html
 * for the list of special keys. */
static gboolean
append_keyval_symbol (guint    accelerator_key,
                      GString *gstring)
{
#ifdef GDK_WINDOWING_MACOS
  switch (accelerator_key)
  {
  case GDK_KEY_Return:
    /* U+21A9 LEFTWARDS ARROW WITH HOOK */
    g_string_append (gstring, "\xe2\x86\xa9");
    return TRUE;

  case GDK_KEY_ISO_Enter:
    /* U+2324 UP ARROWHEAD BETWEEN TWO HORIZONTAL BARS */
    g_string_append (gstring, "\xe2\x8c\xa4");
    return TRUE;

  case GDK_KEY_Left:
    /* U+2190 LEFTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x90");
    return TRUE;

  case GDK_KEY_Up:
    /* U+2191 UPWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x91");
    return TRUE;

  case GDK_KEY_Right:
    /* U+2192 RIGHTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x92");
    return TRUE;

  case GDK_KEY_Down:
    /* U+2193 DOWNWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x93");
    return TRUE;

  case GDK_KEY_Page_Up:
    /* U+21DE UPWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9e");
    return TRUE;

  case GDK_KEY_Page_Down:
    /* U+21DF DOWNWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9f");
    return TRUE;

  case GDK_KEY_Home:
    /* U+2196 NORTH WEST ARROW */
    g_string_append (gstring, "\xe2\x86\x96");
    return TRUE;

  case GDK_KEY_End:
    /* U+2198 SOUTH EAST ARROW */
    g_string_append (gstring, "\xe2\x86\x98");
    return TRUE;

  case GDK_KEY_Escape:
    /* U+238B BROKEN CIRCLE WITH NORTHWEST ARROW */
    g_string_append (gstring, "\xe2\x8e\x8b");
    return TRUE;

  case GDK_KEY_BackSpace:
    /* U+232B ERASE TO THE LEFT */
    g_string_append (gstring, "\xe2\x8c\xab");
    return TRUE;

  case GDK_KEY_Delete:
    /* U+2326 ERASE TO THE RIGHT */
    g_string_append (gstring, "\xe2\x8c\xa6");
    return TRUE;

  default:
    return FALSE;
  }
#else /* !GDK_WINDOWING_MACOS */
  return FALSE;
#endif
}

static void
append_separator (GString *string)
{
#ifndef GDK_WINDOWING_MACOS
  g_string_append (string, "+");
#else
  /* no separator on quartz */
#endif
}

/**
 * gtk_accelerator_get_label:
 * @accelerator_key: accelerator keyval
 * @accelerator_mods: accelerator modifier mask
 *
 * Converts an accelerator keyval and modifier mask into a string
 * which can be used to represent the accelerator to the user.
 *
 * Returns: (transfer full): a newly-allocated string representing the accelerator
 */
char *
gtk_accelerator_get_label (guint           accelerator_key,
                           GdkModifierType accelerator_mods)
{
  GString *gstring;

  gstring = g_string_new (NULL);

  gtk_accelerator_print_label (gstring, accelerator_key, accelerator_mods);

  return g_string_free (gstring, FALSE);
}

void
gtk_accelerator_print_label (GString        *gstring,
                             guint           accelerator_key,
                             GdkModifierType accelerator_mods)
{
  gboolean seen_mod = FALSE;
  gunichar ch;

  if (accelerator_mods & GDK_SHIFT_MASK)
    {
#ifndef GDK_WINDOWING_MACOS
      /* This is the text that should appear next to menu accelerators
       * that use the shift key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Shift"));
#else
      /* U+21E7 UPWARDS WHITE ARROW */
      g_string_append (gstring, "⇧");
#endif
      seen_mod = TRUE;
    }

  if (accelerator_mods & GDK_CONTROL_MASK)
    {
      if (seen_mod)
        append_separator (gstring);

#ifndef GDK_WINDOWING_MACOS
      /* This is the text that should appear next to menu accelerators
       * that use the control key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Ctrl"));
#else
      /* U+2303 UP ARROWHEAD */
      g_string_append (gstring, "⌃");
#endif
      seen_mod = TRUE;
    }

  if (accelerator_mods & GDK_ALT_MASK)
    {
      if (seen_mod)
        append_separator (gstring);

#ifndef GDK_WINDOWING_MACOS
      /* This is the text that should appear next to menu accelerators
       * that use the alt key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Alt"));
#else
      /* U+2325 OPTION KEY */
      g_string_append (gstring, "⌥");
#endif
      seen_mod = TRUE;
    }

  if (accelerator_mods & GDK_SUPER_MASK)
    {
      if (seen_mod)
        append_separator (gstring);

      /* This is the text that should appear next to menu accelerators
       * that use the super key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Super"));
      seen_mod = TRUE;
    }

  if (accelerator_mods & GDK_HYPER_MASK)
    {
      if (seen_mod)
        append_separator (gstring);

      /* This is the text that should appear next to menu accelerators
       * that use the hyper key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Hyper"));
      seen_mod = TRUE;
    }

  if (accelerator_mods & GDK_META_MASK)
    {
      if (seen_mod)
        append_separator (gstring);

#ifndef GDK_WINDOWING_MACOS
      /* This is the text that should appear next to menu accelerators
       * that use the meta key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Meta"));
#else
      g_string_append (gstring, "⌘");
#endif
      seen_mod = TRUE;
    }

  ch = gdk_keyval_to_unicode (accelerator_key);
  if (ch && (ch == ' ' || g_unichar_isgraph (ch)))
    {
      if (seen_mod)
        append_separator (gstring);

      if (accelerator_key >= GDK_KEY_KP_Space &&
          accelerator_key <= GDK_KEY_KP_Equal)
        {
          /* Translators: "KP" means "numeric key pad". This string will
           * be used in accelerators such as "Ctrl+Shift+KP 1" in menus,
           * and therefore the translation needs to be very short.
           */
          g_string_append (gstring, C_("keyboard label", "KP"));
          g_string_append (gstring, " ");
        }

      switch (ch)
        {
        case ' ':
          g_string_append (gstring, C_("keyboard label", "Space"));
          break;
        case '\\':
          g_string_append (gstring, C_("keyboard label", "Backslash"));
          break;
        default:
          g_string_append_unichar (gstring, g_unichar_toupper (ch));
          break;
        }
    }
  else if (!append_keyval_symbol (accelerator_key, gstring))
    {
      const char *tmp;

      tmp = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
      if (tmp != NULL)
        {
          if (seen_mod)
            append_separator (gstring);

          if (tmp[0] != 0 && tmp[1] == 0)
            g_string_append_c (gstring, g_ascii_toupper (tmp[0]));
          else
            {
              const char *str;
              str = g_dpgettext2 (GETTEXT_PACKAGE, "keyboard label", tmp);
              if (str == tmp)
                append_without_underscores (gstring, tmp);
              else
                g_string_append (gstring, str);
            }
        }
    }
}

/**
 * gtk_accelerator_get_default_mod_mask:
 *
 * Gets the modifier mask.
 *
 * The modifier mask determines which modifiers are considered significant
 * for keyboard accelerators. This includes all keyboard modifiers except
 * for %GDK_LOCK_MASK.
 *
 * Returns: the modifier mask for accelerators
 */
GdkModifierType
gtk_accelerator_get_default_mod_mask (void)
{
  return GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|
         GDK_SUPER_MASK|GDK_HYPER_MASK|GDK_META_MASK;
}
