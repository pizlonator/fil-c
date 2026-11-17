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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include <gtk/gtk.h>
#include <shell/cc-panel.h>

#include "cc-keyboard-item.h"

typedef struct {
  /* The untranslated name, combine with ->package to translate */
  char *name;
  /* The group of keybindings (system or application) */
  char *group;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* The GSettings schema for the whole file, if any */
  char *schema;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef struct
{
  CcKeyboardItemType type;
  char *schema; /* GSettings schema name, if any */
  char *description; /* description for GSettings types */
  char *name; /* GSettings schema path, or GSettings key name depending on type */
  char *reverse_entry;
  gboolean is_reversed;
  gboolean hidden;
} KeyListEntry;

typedef struct {
  CcKeyboardItem *orig_item;
  CcKeyboardItem *conflict_item;
  guint new_keyval;
  GdkModifierType new_mask;
  guint new_keycode;
} CcUniquenessData;

enum
{
  SECTION_DESCRIPTION_COLUMN,
  SECTION_ID_COLUMN,
  SECTION_GROUP_COLUMN,
  SECTION_N_COLUMNS
};

gchar*   find_free_settings_path        (GSettings *settings);

gboolean is_valid_binding               (const CcKeyCombo *combo);

gboolean is_empty_binding               (const CcKeyCombo *combo);

gboolean is_valid_accel                 (const CcKeyCombo *combo);

KeyList* parse_keylist_from_file        (const gchar *path);

gchar*   convert_keysym_state_to_string (const CcKeyCombo *combo);

void     normalize_keyval_and_mask      (guint            keyval,
                                         GdkModifierType  mask,
                                         guint            group,
                                         guint           *out_keyval,
                                         GdkModifierType *out_mask);
