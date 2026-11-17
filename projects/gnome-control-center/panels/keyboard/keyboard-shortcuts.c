/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2014 Red Hat, Inc
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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 *          Christophe Fergeau <cfergeau@redhat.com>
 */

#include <config.h>

#include <glib/gi18n.h>

#include "keyboard-shortcuts.h"

#define CUSTOM_KEYS_BASENAME  "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings"

static char *
replace_pictures_folder (const char *description)
{
  g_autoptr(GRegex) pictures_regex = NULL;
  const char *path;
  g_autofree gchar *dirname = NULL;
  g_autofree gchar *ret = NULL;

  if (description == NULL)
    return NULL;

  if (strstr (description, "$PICTURES") == NULL)
    return g_strdup (description);

  pictures_regex = g_regex_new ("\\$PICTURES", 0, 0, NULL);
  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  dirname = g_filename_display_basename (path);
  ret = g_regex_replace (pictures_regex, description, -1,
                         0, dirname, 0, NULL);

  if (ret == NULL)
    return g_strdup (description);

  return g_steal_pointer (&ret);
}

static void
parse_start_tag (GMarkupParseContext *ctx,
                 const gchar         *element_name,
                 const gchar        **attr_names,
                 const gchar        **attr_values,
                 gpointer             user_data,
                 GError             **error)
{
  KeyList *keylist = (KeyList *) user_data;
  KeyListEntry key;
  const char *name, *schema, *description, *package, *context, *orig_description, *reverse_entry;
  gboolean is_reversed, hidden;

  name = NULL;
  schema = NULL;
  package = NULL;
  context = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *group = NULL;

      while (*attr_names && *attr_values)
        {
          if (g_str_equal (*attr_names, "name"))
            {
              if (**attr_values)
                name = *attr_values;
            } else if (g_str_equal (*attr_names, "group")) {
              if (**attr_values)
                group = *attr_values;
            } else if (g_str_equal (*attr_names, "wm_name")) {
              if (**attr_values)
                wm_name = *attr_values;
	    } else if (g_str_equal (*attr_names, "schema")) {
	      if (**attr_values)
	        schema = *attr_values;
            } else if (g_str_equal (*attr_names, "package")) {
              if (**attr_values)
                package = *attr_values;
            }
          ++attr_names;
          ++attr_values;
        }

      if (name)
        {
          if (keylist->name)
            g_warning ("Duplicate section name");
          g_free (keylist->name);
          keylist->name = g_strdup (name);
        }
      if (wm_name)
        {
          if (keylist->wm_name)
            g_warning ("Duplicate window manager name");
          g_free (keylist->wm_name);
          keylist->wm_name = g_strdup (wm_name);
        }
      if (package)
        {
          if (keylist->package)
            g_warning ("Duplicate gettext package name");
          g_free (keylist->package);
          keylist->package = g_strdup (package);
	  bind_textdomain_codeset (keylist->package, "UTF-8");
        }
      if (group)
        {
          if (keylist->group)
            g_warning ("Duplicate group");
          g_free (keylist->group);
          keylist->group = g_strdup (group);
        }
      if (schema)
        {
          if (keylist->schema)
            g_warning ("Duplicate schema");
          g_free (keylist->schema);
          keylist->schema = g_strdup (schema);
	}
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  schema = NULL;
  description = NULL;
  context = NULL;
  orig_description = NULL;
  reverse_entry = NULL;
  is_reversed = FALSE;
  hidden = FALSE;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
          /* skip if empty */
          if (**attr_values)
            name = *attr_values;
	} else if (g_str_equal (*attr_names, "schema")) {
	  if (**attr_values) {
	   schema = *attr_values;
	  }
	} else if (g_str_equal (*attr_names, "description")) {
          if (**attr_values)
	    orig_description = *attr_values;
        } else if (g_str_equal (*attr_names, "msgctxt")) {
          if (**attr_values)
            context = *attr_values;
	} else if (g_str_equal (*attr_names, "reverse-entry")) {
	  if (**attr_values)
	    reverse_entry = *attr_values;
	} else if (g_str_equal (*attr_names, "is-reversed")) {
	    if (g_str_equal (*attr_values, "true"))
	      is_reversed = TRUE;
	} else if (g_str_equal (*attr_names, "hidden")) {
	    if (g_str_equal (*attr_values, "true"))
	      hidden = TRUE;
	}

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  if (schema == NULL &&
      keylist->schema == NULL) {
    g_debug ("Ignored GConf keyboard shortcut '%s'", name);
    return;
  }

  if (context != NULL)
    description = g_dpgettext2 (keylist->package, context, orig_description);
  else
    description = dgettext (keylist->package, orig_description);

  key.name = g_strdup (name);
  key.type = CC_KEYBOARD_ITEM_TYPE_GSETTINGS;
  key.description = replace_pictures_folder (description);
  key.schema = schema ? g_strdup (schema) : g_strdup (keylist->schema);
  key.reverse_entry = g_strdup (reverse_entry);
  key.is_reversed = is_reversed;
  key.hidden = hidden;
  g_array_append_val (keylist->entries, key);
}

static const guint forbidden_keyvals[] = {
  /* Navigation keys */
  GDK_KEY_Home,
  GDK_KEY_Left,
  GDK_KEY_Up,
  GDK_KEY_Right,
  GDK_KEY_Down,
  GDK_KEY_Page_Up,
  GDK_KEY_Page_Down,
  GDK_KEY_End,
  GDK_KEY_Tab,

  /* Return */
  GDK_KEY_KP_Enter,
  GDK_KEY_Return,

  GDK_KEY_Mode_switch
};

static gboolean
keyval_is_forbidden (guint keyval)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++) {
    if (keyval == forbidden_keyvals[i])
      return TRUE;
  }

  return FALSE;
}

gboolean
is_valid_binding (const CcKeyCombo *combo)
{
  if ((combo->mask == 0 || combo->mask == GDK_SHIFT_MASK) && combo->keycode != 0)
    {
      guint keyval = combo->keyval;

      if ((keyval >= GDK_KEY_a && keyval <= GDK_KEY_z)
           || (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z)
           || (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
           || (keyval >= GDK_KEY_kana_fullstop && keyval <= GDK_KEY_semivoicedsound)
           || (keyval >= GDK_KEY_Arabic_comma && keyval <= GDK_KEY_Arabic_sukun)
           || (keyval >= GDK_KEY_Serbian_dje && keyval <= GDK_KEY_Cyrillic_HARDSIGN)
           || (keyval >= GDK_KEY_Greek_ALPHAaccent && keyval <= GDK_KEY_Greek_omega)
           || (keyval >= GDK_KEY_hebrew_doublelowline && keyval <= GDK_KEY_hebrew_taf)
           || (keyval >= GDK_KEY_Thai_kokai && keyval <= GDK_KEY_Thai_lekkao)
           || (keyval >= GDK_KEY_Hangul_Kiyeog && keyval <= GDK_KEY_Hangul_J_YeorinHieuh)
           || (keyval == GDK_KEY_space && combo->mask == 0)
           || keyval_is_forbidden (keyval)) {
        return FALSE;
      }
    }
  return TRUE;
}

gboolean
is_empty_binding (const CcKeyCombo *combo)
{
  if (combo->keyval == 0 &&
      combo->mask == 0 &&
      combo->keycode == 0)
    return TRUE;
  return FALSE;
}

gboolean
is_valid_accel (const CcKeyCombo *combo)
{
  /* Unlike gtk_accelerator_valid(), we want to allow Tab when combined
   * with some modifiers (Alt+Tab and friends)
   */
  return gtk_accelerator_valid (combo->keyval, combo->mask) ||
         (combo->keyval == GDK_KEY_Tab && combo->mask != 0);
}

gchar*
find_free_settings_path (GSettings *settings)
{
  g_auto(GStrv) used_names = NULL;
  g_autofree gchar *dir = NULL;
  int i, num, n_names;

  used_names = g_settings_get_strv (settings, "custom-keybindings");
  n_names = g_strv_length (used_names);

  for (num = 0; dir == NULL; num++)
    {
      g_autofree gchar *tmp = NULL;
      gboolean found = FALSE;

      tmp = g_strdup_printf ("%s/custom%d/", CUSTOM_KEYS_BASENAME, num);
      for (i = 0; i < n_names && !found; i++)
        found = strcmp (used_names[i], tmp) == 0;

      if (!found)
        dir = g_steal_pointer (&tmp);
    }

  return g_steal_pointer (&dir);
}

KeyList*
parse_keylist_from_file (const gchar *path)
{
  KeyList *keylist;
  g_autoptr(GError) err = NULL;
  g_autofree gchar *buf = NULL;
  gsize buf_len;
  guint i;

  g_autoptr(GMarkupParseContext) ctx = NULL;
  GMarkupParser parser = { parse_start_tag, NULL, NULL, NULL, NULL };

  /* Parse file */
  if (!g_file_get_contents (path, &buf, &buf_len, &err))
    return NULL;

  keylist = g_new0 (KeyList, 1);
  keylist->entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));
  ctx = g_markup_parse_context_new (&parser, 0, keylist, NULL);

  if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
    {
      g_warning ("Failed to parse '%s': '%s'", path, err->message);
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);

      for (i = 0; i < keylist->entries->len; i++)
        g_free (((KeyListEntry *) &(keylist->entries->data[i]))->name);

      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return NULL;
    }

  return keylist;
}

/*
 * Stolen from GtkCellRendererAccel:
 * https://git.gnome.org/browse/gtk+/tree/gtk/gtkcellrendereraccel.c#n261
 */
gchar*
convert_keysym_state_to_string (const CcKeyCombo *combo)
{
  gchar *name;

  if (combo->keyval == 0 && combo->keycode == 0)
    {
      /* This label is displayed in a treeview cell displaying
       * a disabled accelerator key combination.
       */
      name = g_strdup (_("Disabled"));
    }
  else
    {
      name = gtk_accelerator_get_label_with_keycode (NULL, combo->keyval, combo->keycode, combo->mask);

      if (name == NULL)
        name = gtk_accelerator_name_with_keycode (NULL, combo->keyval, combo->keycode, combo->mask);
    }

  return name;
}

/* This adjusts the keyval and modifiers such that it matches how
 * gnome-shell detects shortcuts, which works as follows:
 * First for the non-modifier key, the keycode that generates this
 * keyval at the lowest shift level is determined, which might be a
 * level > 0, such as for numbers in the num-row in AZERTY.
 * Next it checks if all the specified modifiers were pressed.
 */
void
normalize_keyval_and_mask (guint            keycode,
                           GdkModifierType  mask,
                           guint            group,
                           guint           *out_keyval,
                           GdkModifierType *out_mask)
{
  guint unmodified_keyval;
  guint shifted_keyval;
  GdkModifierType explicit_modifiers;
  GdkModifierType used_modifiers;

  /* We want shift to always be included as explicit modifier for
   * gnome-shell shortcuts. That's because users usually think of
   * shortcuts as including the shift key rather than being defined
   * for the shifted keyval.
   * This helps with num-row keys which have different keyvals on
   * different layouts for example, but also with keys that have
   * explicit key codes at shift level 0, that gnome-shell would prefer
   * over shifted ones, such the DOLLAR key.
   */
  explicit_modifiers = gtk_accelerator_get_default_mod_mask () | GDK_SHIFT_MASK;
  used_modifiers = mask & explicit_modifiers;  

  /* Find the base keyval of the pressed key without the explicit
   * modifiers. */
  gdk_display_translate_key (gdk_display_get_default (),
                             keycode,
                             mask & ~explicit_modifiers,
                             group,
                             &unmodified_keyval,
                             NULL,
                             NULL,
                             NULL);

  /* Normalize num-row keys to the number value. This allows these
   * shortcuts to work when switching between AZERTY and layouts where
   * the numbers are at shift level 0. */
  gdk_display_translate_key (gdk_display_get_default (),
                             keycode,
                             GDK_SHIFT_MASK | (mask & ~explicit_modifiers),
                             group,
                             &shifted_keyval,
                             NULL,
                             NULL,
                             NULL);

  if (shifted_keyval >= GDK_KEY_0 && shifted_keyval <= GDK_KEY_9)
    unmodified_keyval = shifted_keyval;

  /* Normalise <Tab> */
  if (unmodified_keyval == GDK_KEY_ISO_Left_Tab)
    unmodified_keyval = GDK_KEY_Tab;

  if (unmodified_keyval == GDK_KEY_Sys_Req && (used_modifiers & GDK_ALT_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq */
      unmodified_keyval = GDK_KEY_Print;
    }

  *out_keyval = unmodified_keyval;
  *out_mask = used_modifiers;
}
