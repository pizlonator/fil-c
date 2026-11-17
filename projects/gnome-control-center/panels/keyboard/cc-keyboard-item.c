/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011, 2014 Red Hat, Inc.
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "cc-util.h"
#include "keyboard-shortcuts.h"
#include "cc-keyboard-item.h"

#define CUSTOM_KEYS_SCHEMA "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"
#define SHORTCUT_DELIMITERS "+ "

struct _CcKeyboardItem
{
  GObject parent_instance;

  CcKeyboardItem *reverse_item;
  gboolean is_reversed;
  gboolean hidden;

  CcKeyboardItemType type;

  BindingGroupType group;
  char *description;
  gboolean editable;
  GList *key_combos;
  GList *default_combos;
  gboolean can_set_multiple;

  /* GSettings path */
  char *gsettings_path;
  gboolean desc_editable;
  char *command;
  gboolean cmd_editable;

  /* GSettings */
  char *schema;
  char *key;
  GSettings *settings;
};

enum
{
  PROP_0,
  PROP_DESCRIPTION,
  PROP_EDITABLE,
  PROP_TYPE,
  PROP_IS_VALUE_DEFAULT,
  PROP_COMMAND,
  PROP_KEY_COMBOS
};

static void     cc_keyboard_item_class_init     (CcKeyboardItemClass *klass);
static void     cc_keyboard_item_init           (CcKeyboardItem      *keyboard_item);
static void     cc_keyboard_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcKeyboardItem, cc_keyboard_item, G_TYPE_OBJECT)

static const CcKeyCombo EMPTY_COMBO = { 0, 0, 0 };

static gboolean
combo_equal (CcKeyCombo *a, CcKeyCombo *b)
{
  return (a->keyval == b->keyval
       && a->keycode == b->keycode
       && a->mask == b->mask);
}

static gboolean
combos_contains (GList *combos, CcKeyCombo *needle)
{
  for (GList *l = combos; l != NULL; l = l->next)
    {
      if (combo_equal (l->data, needle))
        return TRUE;
    }

  return FALSE;
}

static gboolean
combos_equal (GList *a, GList *b)
{
  // Should be efficient enough for any sane number of bindings

  for (GList *l = a; l != NULL; l = l->next)
    {
      if (!combos_contains (b, l->data))
        return FALSE;
    }

  for (GList *l = b; l != NULL; l = l->next)
    {
      if (!combos_contains (a, l->data))
        return FALSE;
    }

    return TRUE;
}

static gboolean
binding_from_string (const char *str,
                     CcKeyCombo *combo)
{
  g_return_val_if_fail (combo != NULL, FALSE);
  g_autofree guint *keycodes = NULL;

  if (str == NULL || strcmp (str, "disabled") == 0)
    {
      memset (combo, 0, sizeof(CcKeyCombo));
      return TRUE;
    }

  gtk_accelerator_parse_with_keycode (str,
                                      gdk_display_get_default (),
                                      &combo->keyval,
                                      &keycodes,
                                      &combo->mask);

  combo->keycode = (keycodes ? keycodes[0] : 0);

  if (combo->keyval == 0)
    return FALSE;
  else
    return TRUE;
}

static void
_set_description (CcKeyboardItem *item,
                  const char       *value)
{
  g_free (item->description);
  item->description = g_strdup (value);
}

const char *
cc_keyboard_item_get_description (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);

  return item->description;
}

gboolean
cc_keyboard_item_get_desc_editable (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), FALSE);

  return item->desc_editable;
}

static void
_set_type (CcKeyboardItem *item,
           gint            value)
{
  item->type = value;
}

static void
_set_command (CcKeyboardItem *item,
              const char       *value)
{
  g_free (item->command);
  item->command = g_strdup (value);
}

const char *
cc_keyboard_item_get_command (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);

  return item->command;
}

gboolean
cc_keyboard_item_get_cmd_editable (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), FALSE);

  return item->cmd_editable;
}

static void
cc_keyboard_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CcKeyboardItem *self;

  self = CC_KEYBOARD_ITEM (object);

  switch (prop_id) {
  case PROP_DESCRIPTION:
    _set_description (self, g_value_get_string (value));
    break;
  case PROP_COMMAND:
    _set_command (self, g_value_get_string (value));
    break;
  case PROP_TYPE:
    _set_type (self, g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_keyboard_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CcKeyboardItem *self;

  self = CC_KEYBOARD_ITEM (object);

  switch (prop_id) {
  case PROP_DESCRIPTION:
    g_value_set_string (value, self->description);
    break;
  case PROP_EDITABLE:
    g_value_set_boolean (value, self->editable);
    break;
  case PROP_COMMAND:
    g_value_set_string (value, self->command);
    break;
  case PROP_IS_VALUE_DEFAULT:
    g_value_set_boolean (value, cc_keyboard_item_is_value_default (self));
    break;
  case PROP_KEY_COMBOS:
    g_value_set_pointer (value, self->key_combos);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_keyboard_item_class_init (CcKeyboardItemClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_keyboard_item_get_property;
  object_class->set_property = cc_keyboard_item_set_property;
  object_class->finalize = cc_keyboard_item_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DESCRIPTION,
                                   g_param_spec_string ("description",
                                                        "description",
                                                        "description",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_TYPE,
                                   g_param_spec_int ("type",
                                                     NULL,
                                                     NULL,
                                                     CC_KEYBOARD_ITEM_TYPE_NONE,
                                                     CC_KEYBOARD_ITEM_TYPE_GSETTINGS,
                                                     CC_KEYBOARD_ITEM_TYPE_NONE,
                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "command",
                                                        "command",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_IS_VALUE_DEFAULT,
                                   g_param_spec_boolean ("is-value-default",
                                                         "is value default",
                                                         "is value default",
                                                         TRUE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_KEY_COMBOS,
                                   g_param_spec_pointer ("key-combos",
                                                         "key combos",
                                                         "key combos",
                                                         G_PARAM_READABLE));
}

static void
cc_keyboard_item_init (CcKeyboardItem *item)
{
}

static void
cc_keyboard_item_finalize (GObject *object)
{
  CcKeyboardItem *item;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_KEYBOARD_ITEM (object));

  item = CC_KEYBOARD_ITEM (object);

  if (item->settings != NULL)
    g_object_unref (item->settings);

  /* Free memory */
  g_free (item->gsettings_path);
  g_free (item->description);
  g_free (item->command);
  g_free (item->schema);
  g_free (item->key);
  g_list_free_full (item->key_combos, g_free);
  g_list_free_full (item->default_combos, g_free);

  G_OBJECT_CLASS (cc_keyboard_item_parent_class)->finalize (object);
}

CcKeyboardItem *
cc_keyboard_item_new (CcKeyboardItemType type)
{
  GObject *object;

  object = g_object_new (CC_TYPE_KEYBOARD_ITEM,
                         "type", type,
                         NULL);

  return CC_KEYBOARD_ITEM (object);
}

/* Copied from cc-keyboard-shortcut-dialog.c */
static gboolean
strv_contains_prefix_or_match (char       **strv,
                               const char  *prefix)
{
  const struct {
    const gchar *key;
    const gchar *untranslated;
    const gchar *synonym;
  } key_aliases[] =
    {
      { "ctrl",   "Ctrl",  "ctrl" },
      { "win",    "Super", "super" },
      { "option",  NULL,   "alt" },
      { "command", NULL,   "super" },
      { "apple",   NULL,   "super" },
    };

  for (guint i = 0; strv[i]; i++)
    {
      if (g_str_has_prefix (strv[i], prefix))
        return TRUE;
    }

  for (guint i = 0; i < G_N_ELEMENTS (key_aliases); i++)
    {
      g_autofree char *alias = NULL;
      const char *synonym;

      if (!g_str_has_prefix (key_aliases[i].key, prefix))
        continue;

      if (key_aliases[i].untranslated)
        {
          const char *translated_label;

          /* Steal GTK+'s translation */
          translated_label = g_dpgettext2 ("gtk40", "keyboard label", key_aliases[i].untranslated);
          alias = g_utf8_strdown (translated_label, -1);
        }

      synonym = key_aliases[i].synonym;

      /* If a translation or synonym of the key is in the accelerator, and we typed
       * the key, also consider that a prefix */
      if ((alias && g_strv_contains ((const char * const *) strv, alias)) ||
          (synonym && g_strv_contains ((const char * const *) strv, synonym)))
        return TRUE;
    }

  return FALSE;
}

/* Copied from cc-keyboard-shortcut-dialog.c */
static gboolean
search_match_shortcut (CcKeyboardItem *item,
                       const char     *search)
{
  g_auto(GStrv) shortcut_tokens = NULL, search_tokens = NULL;
  g_autofree char *normalized_accel = NULL;
  g_autofree char *accel = NULL;
  GList *key_combos;
  CcKeyCombo *combo;
  gboolean match = TRUE;

  key_combos = cc_keyboard_item_get_key_combos (item);
  for (GList *l = key_combos; l != NULL; l = l->next)
    {
      combo = l->data;

      if (is_empty_binding (combo))
        continue;

      accel = convert_keysym_state_to_string (combo);
      normalized_accel = cc_util_normalize_casefold_and_unaccent (accel);

      shortcut_tokens = g_strsplit_set (normalized_accel, SHORTCUT_DELIMITERS, -1);
      search_tokens = g_strsplit_set (search, SHORTCUT_DELIMITERS, -1);

      for (guint i = 0; search_tokens[i]; i++)
        {
          match = match && strv_contains_prefix_or_match (shortcut_tokens, search_tokens[i]);

          if (!match)
            break;
        }

      if (match)
        return TRUE;
    }

  return FALSE;
}

gboolean
cc_keyboard_item_matches_string (CcKeyboardItem *self,
                                 GStrv           search_terms)
{
  g_autofree char *name = NULL;

  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (self), FALSE);

  if (!search_terms || !*search_terms || !self->description)
    return TRUE;

  name = cc_util_normalize_casefold_and_unaccent (self->description);

  for (guint i = 0; search_terms[i]; i++)
    {
      gboolean match;

      match = strstr (name, search_terms[i]) || search_match_shortcut (self, search_terms[i]);

      if (!match)
        return FALSE;
    }

  return TRUE;
}

static guint *
get_above_tab_keysyms (void)
{
  guint keycode = 0x29 /* KEY_GRAVE */ + 8;
  g_autofree guint *keyvals = NULL;
  GArray *keysyms;
  int n_entries, i, j;

  keysyms = g_array_new (TRUE, FALSE, sizeof (guint));

  if (!gdk_display_map_keycode (gdk_display_get_default (),
                                keycode,
                                NULL,
                                &keyvals,
                                &n_entries))
    goto out;

  for (i = 0; i < n_entries; i++)
    {
      gboolean found = FALSE;

      for (j = 0; j < keysyms->len; j++)
        if (g_array_index (keysyms, guint, j) == keyvals[i])
          {
            found = TRUE;
            break;
          }

      if (!found)
        g_array_append_val (keysyms, keyvals[i]);
    }

out:
  return (guint *)g_array_free (keysyms, FALSE);
}

/*
 * translate_above_tab:
 *
 * @original_bindings: A list of accelerator strings
 * @new_bindings: (out): Translated bindings if translation is needed
 *
 * Translate accelerator strings that contain the Above_Tab fake keysym
 * used by mutter to strings that use the real keysyms that correspond
 * to the key that is located physically above the tab key.
 *
 * Returns: %TRUE if strings were translated, %FALSE if @original_bindings
 *   can be used unmodified
 */
static gboolean
translate_above_tab (char **original_bindings,
                     char ***new_bindings)
{
  GPtrArray *replaced_bindings;
  g_autofree guint *above_tab_keysyms = NULL;
  gboolean needs_translation = FALSE;
  char **str;

  for (str = original_bindings; *str && !needs_translation; str++)
    needs_translation = strstr (*str, "Above_Tab") != NULL;

  if (!needs_translation)
    return FALSE;

  above_tab_keysyms = get_above_tab_keysyms ();

  replaced_bindings = g_ptr_array_new ();

  for (str = original_bindings; *str; str++)
    {
      if (strstr (*str, "Above_Tab") == NULL)
        {
          g_ptr_array_add (replaced_bindings, g_strdup (*str));
        }
      else
        {
          g_auto (GStrv) split_str = g_strsplit (*str, "Above_Tab", -1);
          int i;

          for (i = 0; above_tab_keysyms[i]; i++)
            {
              g_autofree char *sym = NULL;

              sym = gtk_accelerator_name (above_tab_keysyms[i], 0);
              g_ptr_array_add (replaced_bindings, g_strjoinv (sym, split_str));
            }
        }
      g_ptr_array_add (replaced_bindings, NULL);
    }

  *new_bindings = (char **)g_ptr_array_free (replaced_bindings, FALSE);
  return TRUE;
}

static GList *
variant_get_key_combos (GVariant *variant)
{
  GList *combos = NULL;
  char **translated_bindings, **str;
  g_auto(GStrv) bindings = NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    {
      bindings = g_malloc0_n (2, sizeof(char *));
      if (g_strcmp0 (g_variant_get_string (variant, NULL), "") != 0)
        bindings[0] = g_variant_dup_string (variant, NULL);
    }
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    {
      bindings = g_variant_dup_strv (variant, NULL);
    }

  if (translate_above_tab (bindings, &translated_bindings))
    {
      g_strfreev (bindings);
      bindings = translated_bindings;
    }

  for (str = bindings; *str; str++)
    {
      g_autofree CcKeyCombo *combo = g_new (CcKeyCombo, 1);

      binding_from_string (*str, combo);

      if (combo->keyval != 0 || combo->keycode != 0 || combo->mask != 0)
        combos = g_list_prepend (combos, g_steal_pointer (&combo));
    }

  return g_list_reverse (combos);
}

static GList *
settings_get_key_combos (GSettings  *settings,
                         const char *key,
                         gboolean    use_default)
{
  GList *key_combos;
  g_autoptr(GVariant) variant = NULL;

  if (use_default)
    variant = g_settings_get_default_value (settings, key);
  else
    variant = g_settings_get_value (settings, key);
  key_combos = variant_get_key_combos (variant);

  return key_combos;
}

static void
binding_changed (CcKeyboardItem *item,
		 const char *key)
{
  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  item->editable = g_settings_is_writable (item->settings, item->key);

  g_object_notify (G_OBJECT (item), "key-combos");
  g_object_notify (G_OBJECT (item), "is-value-default");
  if (item->reverse_item)
    g_object_notify (G_OBJECT (item->reverse_item), "is-value-default");
}

gboolean
cc_keyboard_item_load_from_gsettings_path (CcKeyboardItem *item,
                                           const char     *path,
                                           gboolean        reset)
{
  g_autoptr(GVariant) variant = NULL;

  item->schema = g_strdup (CUSTOM_KEYS_SCHEMA);
  item->gsettings_path = g_strdup (path);
  item->key = g_strdup ("binding");
  item->settings = g_settings_new_with_path (item->schema, path);
  item->editable = g_settings_is_writable (item->settings, item->key);
  item->desc_editable = g_settings_is_writable (item->settings, "name");
  item->cmd_editable = g_settings_is_writable (item->settings, "command");

  variant = g_settings_get_value (item->settings, item->key);
  item->can_set_multiple = g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY);

  if (reset)
    {
      g_settings_reset (item->settings, "name");
      g_settings_reset (item->settings, "command");
      g_settings_reset (item->settings, "binding");
    }

  g_settings_bind (item->settings, "name",
                   G_OBJECT (item), "description", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (item->settings, "command",
                   G_OBJECT (item), "command", G_SETTINGS_BIND_DEFAULT);

  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  g_signal_connect_object (G_OBJECT (item->settings), "changed::binding",
                           G_CALLBACK (binding_changed), item, G_CONNECT_SWAPPED);

  return TRUE;
}

gboolean
cc_keyboard_item_load_from_gsettings (CcKeyboardItem *item,
				      const char *description,
				      const char *schema,
				      const char *key)
{
  g_autofree char *signal_name = NULL;
  g_autoptr(GVariant) variant = NULL;

  item->schema = g_strdup (schema);
  item->key = g_strdup (key);
  item->description = g_strdup (description);

  item->settings = g_settings_new (item->schema);
  item->editable = g_settings_is_writable (item->settings, item->key);

  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  g_list_free_full (item->default_combos, g_free);
  item->default_combos = settings_get_key_combos (item->settings, item->key, TRUE);

  variant = g_settings_get_value (item->settings, item->key);
  item->can_set_multiple = g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY);

  signal_name = g_strdup_printf ("changed::%s", item->key);
  g_signal_connect_object (G_OBJECT (item->settings), signal_name,
                           G_CALLBACK (binding_changed), item, G_CONNECT_SWAPPED);

  return TRUE;
}

gboolean
cc_keyboard_item_equal (CcKeyboardItem *a,
			CcKeyboardItem *b)
{
  if (a->type != b->type)
    return FALSE;
  switch (a->type)
    {
      case CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH:
	return g_str_equal (a->gsettings_path, b->gsettings_path);
      case CC_KEYBOARD_ITEM_TYPE_GSETTINGS:
	return (g_str_equal (a->schema, b->schema) &&
		g_str_equal (a->key, b->key));
      default:
	g_assert_not_reached ();
    }

}

void
cc_keyboard_item_add_reverse_item (CcKeyboardItem *item,
				   CcKeyboardItem *reverse_item,
				   gboolean is_reversed)
{
  g_return_if_fail (item->key != NULL);

  item->reverse_item = reverse_item;
  if (reverse_item->reverse_item == NULL)
    {
      reverse_item->reverse_item = item;
      reverse_item->is_reversed = !is_reversed;
    }
  else
    g_warn_if_fail (reverse_item->is_reversed == !!is_reversed);

  item->is_reversed = !!is_reversed;
}

CcKeyboardItem *
cc_keyboard_item_get_reverse_item (CcKeyboardItem *item)
{
  return item->reverse_item;
}


void
cc_keyboard_item_set_hidden (CcKeyboardItem *item, gboolean hidden)
{
  item->hidden = !!hidden;
}


gboolean
cc_keyboard_item_is_hidden (CcKeyboardItem *item)
{
  return item->hidden;
}

/**
 * cc_keyboard_item_is_value_default:
 * @self: a #CcKeyboardItem
 *
 * Retrieves whether the shortcut is the default value or not.
 *
 * Returns: %TRUE if the shortcut is the default value, %FALSE otherwise.
 */
gboolean
cc_keyboard_item_is_value_default (CcKeyboardItem *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (self), FALSE);

  /*
   * When the shortcut is custom, we don't treat it as modified
   * since we don't know what would be its default value.
   */
  if (self->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return TRUE;

  return combos_equal (self->default_combos, self->key_combos);
}

/**
 * cc_keyboard_item_reset:
 * @self: a #CcKeyboardItem
 *
 * Reset the keyboard binding to the default value.
 */
void
cc_keyboard_item_reset (CcKeyboardItem *self)
{
  CcKeyboardItem *reverse;

  g_return_if_fail (CC_IS_KEYBOARD_ITEM (self));

  reverse = self->reverse_item;

  g_settings_reset (self->settings, self->key);
  g_object_notify (G_OBJECT (self), "is-value-default");

  /* Also reset the reverse item */
  if (reverse)
    {
      g_settings_reset (reverse->settings, reverse->key);
      g_object_notify (G_OBJECT (reverse), "is-value-default");
    }
}

GList *
cc_keyboard_item_get_key_combos (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->key_combos;
}

GList *
cc_keyboard_item_get_default_combos (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->default_combos;
}

CcKeyCombo
cc_keyboard_item_get_primary_combo (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), EMPTY_COMBO);
  return (item->key_combos) ? *((CcKeyCombo*)item->key_combos->data) : EMPTY_COMBO;
}

const gchar *
cc_keyboard_item_get_key (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->key;
}

CcKeyboardItemType
cc_keyboard_item_get_item_type (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), CC_KEYBOARD_ITEM_TYPE_NONE);
  return item->type;
}

const gchar *
cc_keyboard_item_get_gsettings_path (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->gsettings_path;
}

GSettings *
cc_keyboard_item_get_settings (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->settings;
}

gboolean
cc_keyboard_item_can_set_multiple (CcKeyboardItem *item)
{
  return item->can_set_multiple;
}

static gchar*
combo_get_accelerator (CcKeyCombo *combo)
{
  return gtk_accelerator_name_with_keycode (NULL,
                                            combo->keyval,
                                            combo->keycode,
                                            combo->mask);
}

static void
cc_keyboard_item_add_key_combo_inner (CcKeyboardItem *self,
                                      CcKeyCombo *combo)
{
  g_auto(GStrv) strv = NULL;
  int i;

  if (!self->can_set_multiple)
    {
      g_settings_set_string (self->settings, self->key, combo_get_accelerator (combo));
    }
  else
    {
      strv = g_new0 (gchar*, g_list_length (self->key_combos) + 2);

      i = 0;
      for (GList *l = self->key_combos; l != NULL; l = l->next, i++)
        {
          if (combo_equal (l->data, combo))
            // This combo is already in the list
            return;
          strv[i] = combo_get_accelerator (l->data);
        }
      strv[i] = combo_get_accelerator (combo);

      g_settings_set_strv (self->settings, self->key, (const gchar **)strv);
    }

  binding_changed (self, self->key);
}

void
cc_keyboard_item_add_key_combo (CcKeyboardItem *self,
                                CcKeyCombo *combo)
{
  CcKeyCombo reverse_combo;

  if (self->reverse_item)
    {
      reverse_combo.keyval = combo->keyval;
      reverse_combo.keycode = combo->keycode;
      reverse_combo.mask = combo->mask ^ GDK_SHIFT_MASK;
      cc_keyboard_item_add_key_combo_inner (self->reverse_item, &reverse_combo);
    }

  cc_keyboard_item_add_key_combo_inner (self, combo);
}

static void
cc_keyboard_item_remove_key_combo_inner (CcKeyboardItem *self,
                                         CcKeyCombo *combo)
{
  g_auto(GStrv) strv = NULL;
  gboolean found;
  int i;

  strv = g_new0 (gchar*, g_list_length (self->key_combos) + 1);

  found = FALSE;
  i = 0;
  for (GList *l = self->key_combos; l != NULL; l = l->next, i++)
    {
      if (combo_equal (l->data, combo))
        {
          i--;
          found = TRUE;
        }
      else
        {
          strv[i] = combo_get_accelerator (l->data);
        }
    }

  if (found)
    {
      if (self->can_set_multiple)
        g_settings_set_strv (self->settings, self->key, (const gchar **)strv);
      else
        g_settings_set_string (self->settings, self->key, "");
    }

  binding_changed (self, self->key);
}

void
cc_keyboard_item_remove_key_combo (CcKeyboardItem *self,
                                   CcKeyCombo *combo)
{
  CcKeyCombo reverse_combo;

  if (self->reverse_item)
    {
      reverse_combo.keyval = combo->keyval;
      reverse_combo.keycode = combo->keycode;
      reverse_combo.mask = combo->mask ^ GDK_SHIFT_MASK;
      cc_keyboard_item_remove_key_combo_inner (self->reverse_item, &reverse_combo);
    }

  cc_keyboard_item_remove_key_combo_inner (self, combo);
}

void cc_keyboard_item_disable (CcKeyboardItem *self)
{
  if (!self->can_set_multiple)
    {
      g_settings_set_string (self->settings, self->key, "");
      if (self->reverse_item)
        g_settings_set_string (self->reverse_item->settings, self->reverse_item->key, "");
    }
  else
    {
      g_settings_set_strv (self->settings, self->key, NULL);
      if (self->reverse_item)
        g_settings_set_strv (self->reverse_item->settings, self->reverse_item->key, NULL);
    }

  binding_changed (self, self->key);
}
