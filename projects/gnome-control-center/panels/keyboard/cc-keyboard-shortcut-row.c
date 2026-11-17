/* cc-keyboard-shortcut-row.c
 *
 * Copyright (C) 2020 System76, Inc.
 * Copyright (C) 2022 Purism SPC.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib/gi18n.h>
#include "cc-keyboard-shortcut-row.h"
#include "keyboard-shortcuts.h"

struct _CcKeyboardShortcutRow
{
  AdwActionRow              parent_instance;

  GtkLabel                 *accelerator_label;
  GtkButton                *reset_button;
  GtkRevealer              *reset_revealer;

  CcKeyboardItem           *item;
  CcKeyboardManager        *manager;
  CcKeyboardShortcutEditor *shortcut_editor;
};

G_DEFINE_TYPE (CcKeyboardShortcutRow, cc_keyboard_shortcut_row, ADW_TYPE_ACTION_ROW)

static void
reset_shortcut_cb (CcKeyboardShortcutRow *self)
{
  cc_keyboard_manager_reset_shortcut (self->manager, self->item);
}

static void
cc_keyboard_shortcut_row_class_init (CcKeyboardShortcutRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-shortcut-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutRow, accelerator_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutRow, reset_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutRow, reset_revealer);

  gtk_widget_class_bind_template_callback (widget_class, reset_shortcut_cb);
}

static void
cc_keyboard_shortcut_row_init (CcKeyboardShortcutRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_kbd_shortcut_is_default_changed_cb (CcKeyboardShortcutRow *self)
{
  /* Embolden the label when the shortcut is modified */
  if (cc_keyboard_item_is_value_default (self->item))
    gtk_widget_remove_css_class (GTK_WIDGET (self->accelerator_label), "heading");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self->accelerator_label), "heading");
}

static gboolean
transform_binding_to_accel (GBinding     *binding,
                            const GValue *from_value,
                            GValue       *to_value,
                            gpointer      user_data)
{
  g_autoptr(CcKeyboardItem) item = NULL;
  CcKeyCombo combo;
  gchar *accelerator;

  item = CC_KEYBOARD_ITEM (g_binding_dup_source (binding));
  combo = cc_keyboard_item_get_primary_combo (item);
  accelerator = convert_keysym_state_to_string (&combo);

  g_value_take_string (to_value, accelerator);

  return TRUE;
}

CcKeyboardShortcutRow *
cc_keyboard_shortcut_row_new (CcKeyboardItem           *item,
                              CcKeyboardManager        *manager,
                              CcKeyboardShortcutEditor *shortcut_editor,
                              GtkSizeGroup             *size_group)
{
  CcKeyboardShortcutRow *self;

  self = g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_ROW, NULL);
  self->item = item;
  self->manager = manager;
  self->shortcut_editor = shortcut_editor;

  g_object_bind_property (item, "description",
                          self, "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (item, "is-value-default",
                          self->reset_revealer, "reveal-child",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property_full (item,
                               "key-combos",
                               self->accelerator_label,
                               "label",
                               G_BINDING_SYNC_CREATE,
                               transform_binding_to_accel,
                               NULL, NULL, NULL);

  g_signal_connect_object (item, "notify::is-value-default",
                           G_CALLBACK (cc_kbd_shortcut_is_default_changed_cb),
                           self, G_CONNECT_SWAPPED);
  cc_kbd_shortcut_is_default_changed_cb (self);

  gtk_size_group_add_widget(size_group,
                            GTK_WIDGET (self->accelerator_label));

  return self;
}

CcKeyboardItem *
cc_keyboard_shortcut_row_get_item (CcKeyboardShortcutRow *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_ROW (self), NULL);

  return self->item;
}
