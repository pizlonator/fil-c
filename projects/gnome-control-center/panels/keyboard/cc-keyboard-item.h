/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_ITEM (cc_keyboard_item_get_type ())
G_DECLARE_FINAL_TYPE (CcKeyboardItem, cc_keyboard_item, CC, KEYBOARD_ITEM, GObject)

typedef enum
{
  BINDING_GROUP_SYSTEM,
  BINDING_GROUP_APPS,
  BINDING_GROUP_SEPARATOR,
  BINDING_GROUP_USER,
} BindingGroupType;

typedef enum
{
  CC_KEYBOARD_ITEM_TYPE_NONE = 0,
  CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH,
  CC_KEYBOARD_ITEM_TYPE_GSETTINGS
} CcKeyboardItemType;

typedef struct
{
  guint           keyval;
  guint           keycode;
  GdkModifierType mask;
} CcKeyCombo;

CcKeyboardItem*    cc_keyboard_item_new                      (CcKeyboardItemType  type);

gboolean           cc_keyboard_item_matches_string           (CcKeyboardItem *self,
                                                              GStrv           search_terms);

gboolean           cc_keyboard_item_load_from_gsettings_path (CcKeyboardItem     *item,
                                                              const char         *path,
                                                              gboolean            reset);

gboolean           cc_keyboard_item_load_from_gsettings      (CcKeyboardItem     *item,
                                                              const char         *description,
                                                              const char         *schema,
                                                              const char         *key);

const char*        cc_keyboard_item_get_description          (CcKeyboardItem     *item);

gboolean           cc_keyboard_item_get_desc_editable        (CcKeyboardItem     *item);

const char*        cc_keyboard_item_get_command              (CcKeyboardItem     *item);

gboolean           cc_keyboard_item_get_cmd_editable         (CcKeyboardItem     *item);

gboolean           cc_keyboard_item_equal                    (CcKeyboardItem     *a,
                                                              CcKeyboardItem     *b);

void               cc_keyboard_item_add_reverse_item         (CcKeyboardItem     *item,
                                                              CcKeyboardItem     *reverse_item,
                                                              gboolean            is_reversed);

CcKeyboardItem*    cc_keyboard_item_get_reverse_item         (CcKeyboardItem     *item);

void               cc_keyboard_item_set_hidden               (CcKeyboardItem     *item,
                                                              gboolean            hidden);

gboolean           cc_keyboard_item_is_hidden                (CcKeyboardItem     *item);

gboolean           cc_keyboard_item_is_value_default         (CcKeyboardItem     *self);

void               cc_keyboard_item_reset                    (CcKeyboardItem     *self);

GList*             cc_keyboard_item_get_key_combos           (CcKeyboardItem     *self);

GList*             cc_keyboard_item_get_default_combos       (CcKeyboardItem     *self);

CcKeyCombo         cc_keyboard_item_get_primary_combo        (CcKeyboardItem     *self);

const gchar*       cc_keyboard_item_get_key                  (CcKeyboardItem     *self);

CcKeyboardItemType cc_keyboard_item_get_item_type            (CcKeyboardItem     *self);

const gchar*       cc_keyboard_item_get_gsettings_path       (CcKeyboardItem     *self);

GSettings*         cc_keyboard_item_get_settings             (CcKeyboardItem     *self);

gboolean           cc_keyboard_item_can_set_multiple         (CcKeyboardItem     *self);

void               cc_keyboard_item_add_key_combo            (CcKeyboardItem     *self,
                                                              CcKeyCombo         *combo);

void               cc_keyboard_item_remove_key_combo         (CcKeyboardItem     *self,
                                                              CcKeyCombo         *combo);
void               cc_keyboard_item_disable                  (CcKeyboardItem     *self);

G_END_DECLS
