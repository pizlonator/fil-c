/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2022 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-keyboard-shortcut-group"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "cc-keyboard-item.h"
#include "cc-keyboard-shortcut-row.h"
#include "cc-keyboard-shortcut-group.h"

struct _CcKeyboardShortcutGroup
{
  AdwPreferencesGroup       parent_instance;

  GtkListBox               *shortcut_list_box;

  GtkSizeGroup             *accelerator_size_group;
  CcKeyboardShortcutEditor *shortcut_editor;

  GListModel               *shortcut_items;
  GtkFilterListModel       *filtered_shortcuts;
  GtkCustomFilter          *custom_filter;

  CcKeyboardManager        *keyboard_manager;
  char                    **search_terms;
  char                     *section_id;
  char                     *section_title;
  /* The text representing the count of shortcuts changed */
  char                     *modified_text;

  gboolean                 is_empty;
};

G_DEFINE_TYPE (CcKeyboardShortcutGroup, cc_keyboard_shortcut_group, ADW_TYPE_PREFERENCES_GROUP)

enum {
  PROP_0,
  PROP_EMPTY,
  PROP_MODIFIED_TEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
shortcut_group_row_activated_cb (CcKeyboardShortcutGroup *self,
                                 GtkListBoxRow           *row)
{
  g_assert (CC_IS_KEYBOARD_SHORTCUT_GROUP (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if (CC_IS_KEYBOARD_SHORTCUT_ROW (row))
    {
      CcKeyboardItem *item;

      item = cc_keyboard_shortcut_row_get_item (CC_KEYBOARD_SHORTCUT_ROW (row));
      cc_keyboard_shortcut_editor_set_mode (self->shortcut_editor, CC_SHORTCUT_EDITOR_EDIT);
      cc_keyboard_shortcut_editor_set_item (self->shortcut_editor, item);
    }
  else  /* Add shortcut row */
    {
      cc_keyboard_shortcut_editor_set_mode (self->shortcut_editor, CC_SHORTCUT_EDITOR_CREATE);
      cc_keyboard_shortcut_editor_set_item (self->shortcut_editor, NULL);
    }

  gtk_widget_set_visible (GTK_WIDGET (self->shortcut_editor), TRUE);
}

static void
group_shortcut_changed_cb (CcKeyboardShortcutGroup *self)
{
  g_assert (CC_IS_KEYBOARD_SHORTCUT_GROUP (self));

  /* Free the modified text, so that it will be regenerated when someone asks for one */
  g_clear_pointer (&self->modified_text, g_free);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED_TEXT]);
}

static void
shortcut_group_update_modified_text (CcKeyboardShortcutGroup *self)
{
  guint n_items, n_modified = 0;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_GROUP (self));

  if (self->modified_text)
    return;

  n_items = g_list_model_get_n_items (self->shortcut_items);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CcKeyboardItem) item = NULL;

      item = g_list_model_get_item (self->shortcut_items, i);

      if (!cc_keyboard_item_is_value_default (item))
        n_modified++;
    }

  if (n_modified == 0)
    self->modified_text = g_strdup ("");
  else
    self->modified_text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                        "%d modified",
                                                        "%d modified",
                                                        n_modified),
                                           n_modified);
}

static GtkWidget *
shortcut_group_row_new (gpointer item,
                        gpointer user_data)
{
  CcKeyboardShortcutGroup *self = user_data;
  GtkWidget *row;

  /* Row to add custom shortcut */
  if (GTK_IS_STRING_OBJECT (item))
    {
      GtkWidget *icon;

      row = adw_preferences_row_new ();
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
      gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
      gtk_accessible_update_property (GTK_ACCESSIBLE (row),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL,
                                      _("Add a Shortcut"),
                                      -1);
      icon = gtk_image_new_from_icon_name ("list-add-symbolic");
      gtk_widget_set_margin_top (icon, 15);
      gtk_widget_set_margin_bottom (icon, 15);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), icon);

      return row;

    }

  row = GTK_WIDGET (cc_keyboard_shortcut_row_new (item,
                                                  self->keyboard_manager,
                                                  self->shortcut_editor,
                                                  self->accelerator_size_group));

  g_signal_connect_object (item, "notify::is-value-default",
                           G_CALLBACK (group_shortcut_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

static gboolean
shortcut_group_filter_cb (gpointer item,
                          gpointer  user_data)
{
  CcKeyboardShortcutGroup *self = user_data;

  if (!self->search_terms)
    return TRUE;

  /* We don't want to show the "Add new shortcut" row in search results */
  if (GTK_IS_STRING_OBJECT (item))
    return FALSE;

  return cc_keyboard_item_matches_string (item, self->search_terms);
}

static void
group_filter_list_changed_cb (CcKeyboardShortcutGroup *self)
{
  guint n_items;
  gboolean empty;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_GROUP (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filtered_shortcuts));
  empty = n_items == 0;

  if (self->is_empty == empty)
    return;

  self->is_empty = empty;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMPTY]);
}

static void
shortcut_group_set_list_model (CcKeyboardShortcutGroup *self,
                               GListModel              *shortcut_items)
{
  GtkExpression *expression;
  GtkSortListModel *sort_model;
  GtkStringSorter *sorter;
  GListModel *model = NULL;

  g_assert (!self->shortcut_items);
  self->shortcut_items = g_object_ref (shortcut_items);

  /* Sort shortcuts by description */
  expression = gtk_property_expression_new (CC_TYPE_KEYBOARD_ITEM, NULL, "description");
  sorter = gtk_string_sorter_new (expression);
  sort_model = gtk_sort_list_model_new (g_object_ref (shortcut_items), GTK_SORTER (sorter));

  /*
   * This is a workaround to add an additional item to the end
   * of the shortcut list, which will be used to show "+" row
   * to add more items.  We do this way instead of appending a
   * row to avoid some imperfections in the GUI.
   */
  if (g_strcmp0 (self->section_id, "custom") == 0)
    {
      g_autoptr(GListStore) add_shortcut = NULL;
      g_autoptr(GtkStringObject) str = NULL;
      GtkFlattenListModel *flat_model;
      GListStore *shortcut_store;

      shortcut_store = g_list_store_new (G_TYPE_LIST_MODEL);
      add_shortcut = g_list_store_new (GTK_TYPE_STRING_OBJECT);

      str = gtk_string_object_new ("add-shortcut");
      g_list_store_append (add_shortcut, str);

      g_list_store_append (shortcut_store, sort_model);
      g_list_store_append (shortcut_store, add_shortcut);

      flat_model = gtk_flatten_list_model_new (G_LIST_MODEL (shortcut_store));
      model = G_LIST_MODEL (flat_model);
    }

  if (!model)
    model = G_LIST_MODEL (sort_model);

  self->custom_filter = gtk_custom_filter_new (shortcut_group_filter_cb, self, NULL);
  self->filtered_shortcuts = gtk_filter_list_model_new (model, GTK_FILTER (self->custom_filter));

  g_signal_connect_object (self->filtered_shortcuts, "items-changed",
                           G_CALLBACK (group_filter_list_changed_cb),
                           self, G_CONNECT_SWAPPED);
  group_filter_list_changed_cb (self);
}

static void
shortcut_group_update_title (CcKeyboardShortcutGroup *self)
{
  const char *title = NULL;
  guint n_items;
  gboolean show_title = TRUE;

  if (!self->section_title)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filtered_shortcuts));

  if (!self->search_terms || n_items == 0)
    show_title = FALSE;

  if (show_title)
    title = _(self->section_title);

  adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self), title);
}

static void
cc_keyboard_shortcut_group_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  CcKeyboardShortcutGroup *self = (CcKeyboardShortcutGroup *)object;

  switch (prop_id)
    {
    case PROP_EMPTY:
      g_value_set_boolean (value, self->is_empty);
      break;

    case PROP_MODIFIED_TEXT:
      shortcut_group_update_modified_text (self);
      g_value_set_string (value, self->modified_text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_keyboard_shortcut_group_finalize (GObject *object)
{
  CcKeyboardShortcutGroup *self = (CcKeyboardShortcutGroup *)object;

  g_clear_pointer (&self->section_id, g_free);
  g_clear_pointer (&self->section_title, g_free);
  g_clear_object (&self->shortcut_items);
  g_clear_object (&self->filtered_shortcuts);

  G_OBJECT_CLASS (cc_keyboard_shortcut_group_parent_class)->finalize (object);
}

static void
cc_keyboard_shortcut_group_class_init (CcKeyboardShortcutGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_keyboard_shortcut_group_get_property;
  object_class->finalize = cc_keyboard_shortcut_group_finalize;

  /**
   * CcKeyboardShortcutGroup:empty:
   *
   * Whether the list of shortcuts is empty
   */
  properties[PROP_EMPTY] =
    g_param_spec_boolean ("empty",
                          "Empty Shorcuts",
                          "Whether the group contain no shorcuts",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CcKeyboardShortcutGroup:modified_text:
   *
   * A string the represents the number of modified keys
   * present in the group, translated to current locale.
   * This is a string property so that it can be bound to
   * UI label as such.
   *
   * Shall be any empty string if no shortcut is modified.
   */
  properties[PROP_MODIFIED_TEXT] =
    g_param_spec_string ("modified-text",
                         "Modified Text",
                         "A string representing the number of modified shortcut items",
                         "",
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_keyboard_shortcut_group_init (CcKeyboardShortcutGroup *self)
{
  self->shortcut_list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->shortcut_list_box), "boxed-list");
  g_signal_connect_object (self->shortcut_list_box, "row-activated",
                           G_CALLBACK (shortcut_group_row_activated_cb),
                           self, G_CONNECT_SWAPPED);

  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self),
                             GTK_WIDGET (self->shortcut_list_box));
}

GtkWidget *
cc_keyboard_shortcut_group_new (GListModel               *shortcut_items,
                                const char               *section_id,
                                const char               *section_title,
                                CcKeyboardManager        *manager,
                                CcKeyboardShortcutEditor *shortcut_editor,
                                GtkSizeGroup             *size_group)
{
  CcKeyboardShortcutGroup *self;

  g_return_val_if_fail (section_id && *section_id, NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (shortcut_items), NULL);
  g_return_val_if_fail (g_list_model_get_item_type (shortcut_items) == CC_TYPE_KEYBOARD_ITEM, NULL);

  self = g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_GROUP, NULL);
  self->section_title = g_strdup (section_title);
  self->section_id = g_strdup (section_id);

  self->keyboard_manager = manager;
  self->shortcut_editor = shortcut_editor;
  self->accelerator_size_group = size_group;

  shortcut_group_set_list_model (self, shortcut_items);
  shortcut_group_update_title (self);

  g_object_bind_property (self, "empty",
                          self, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  gtk_list_box_bind_model (self->shortcut_list_box,
                           G_LIST_MODEL (self->filtered_shortcuts),
                           shortcut_group_row_new,
                           self, NULL);

  return GTK_WIDGET (self);
}

/**
 * cc_keyboard_shortcut_group_get_model:
 * self: A #CcKeyboardShortcutGroup
 *
 * Get the #GListModel used to create shortcut rows.
 * The content of the model can be different from
 * the #GListModel given to create @self if a search
 * is in progress.
 *
 * Returns: (transfer none): A #GListModel
 */
GListModel *
cc_keyboard_shortcut_group_get_model (CcKeyboardShortcutGroup *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_GROUP (self), NULL);

  return G_LIST_MODEL (self->filtered_shortcuts);
}

void
cc_keyboard_shortcut_group_set_filter (CcKeyboardShortcutGroup *self,
                                       GStrv                    search_terms)
{
  g_return_if_fail (CC_IS_KEYBOARD_SHORTCUT_GROUP (self));

  self->search_terms = search_terms;
  gtk_filter_changed (GTK_FILTER (self->custom_filter), GTK_FILTER_CHANGE_DIFFERENT);
  shortcut_group_update_title (self);
}
