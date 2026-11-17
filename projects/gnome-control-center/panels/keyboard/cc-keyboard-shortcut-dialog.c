/* cc-keyboard-shortcut-dialog.c
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
 * Copyright (C) 2020 System76, Inc.
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *         Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-keyboard-shortcut-dialog.h"
#include "cc-keyboard-item.h"
#include "cc-keyboard-manager.h"
#include "cc-keyboard-shortcut-editor.h"
#include "cc-keyboard-shortcut-group.h"
#include "cc-keyboard-shortcut-row.h"
#include "cc-list-row.h"
#include "cc-util.h"
#include "keyboard-shortcuts.h"

struct _CcKeyboardShortcutDialog
{
  AdwWindow             parent_instance;

  AdwNavigationView    *navigation_view;
  AdwNavigationPage    *main_page;
  GtkButton            *reset_all_button;
  GtkSearchEntry       *search_entry;
  GtkStack             *section_stack;
  AdwPreferencesPage   *section_list_page;
  GtkListBox           *section_list_box;
  AdwPreferencesPage   *search_result_page;
  AdwStatusPage        *empty_results_page;

  AdwNavigationPage    *subview_page;
  GtkStack             *subview_stack;
  GtkStack             *shortcut_list_stack;
  AdwStatusPage        *empty_custom_shortcut_page;
  GtkSizeGroup         *accelerator_size_group;

  /* A GListStore of sections containing a GListStore of CcKeyboardItem */
  GListStore           *sections;
  GListStore           *visible_section;
  GtkFlattenListModel  *filtered_shortcuts;

  CcKeyboardManager    *manager;
  GtkWidget            *shortcut_editor;
  GStrv                 search_terms;
 };

G_DEFINE_TYPE (CcKeyboardShortcutDialog, cc_keyboard_shortcut_dialog, ADW_TYPE_WINDOW)


static GListStore *
keyboard_shortcut_get_section_store (CcKeyboardShortcutDialog *self,
                                     const char               *section_id,
                                     const char               *section_title)
{
  g_autoptr(GListStore) section = NULL;
  GtkWidget *group;
  guint n_items;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_DIALOG (self));
  g_assert (section_id && *section_id);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->sections));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = NULL;
      const char *item_section_id;

      item = g_list_model_get_item (G_LIST_MODEL (self->sections), i);
      item_section_id = g_object_get_data (item, "id");

      if (g_str_equal (item_section_id, section_id))
        return G_LIST_STORE (item);
    }

  /* Found no matching section, so create one */
  section = g_list_store_new (CC_TYPE_KEYBOARD_ITEM);
  g_object_set_data_full (G_OBJECT (section), "id", g_strdup (section_id), g_free);
  g_object_set_data_full (G_OBJECT (section), "title", g_strdup (section_title), g_free);

  /* This group shall be shown in the search results page */
  group = cc_keyboard_shortcut_group_new (G_LIST_MODEL (section),
                                          section_id, section_title,
                                          self->manager,
                                          CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor),
                                          self->accelerator_size_group);
  g_object_set_data (G_OBJECT (section), "search-group", group);

  /* This group shall be shown when a section title row is activated */
  group = cc_keyboard_shortcut_group_new (G_LIST_MODEL (section),
                                          section_id, NULL,
                                          self->manager,
                                          CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor),
                                          self->accelerator_size_group);
  g_object_set_data (G_OBJECT (section), "group", group);

  g_list_store_append (self->sections, section);

  return section;
}

static void
shortcut_added_cb (CcKeyboardShortcutDialog *self,
                   CcKeyboardItem           *item,
                   const char               *section_id,
                   const char               *section_title)
{
  GListStore *section;

  section = keyboard_shortcut_get_section_store (self, section_id, section_title);
  g_object_set_data (G_OBJECT (item), "section", section);
  g_list_store_append (section, item);
}

static void
shortcut_removed_cb (CcKeyboardShortcutDialog *self,
                     CcKeyboardItem           *item)
{
  GListStore *section;
  guint position;

  section = g_object_get_data (G_OBJECT (item), "section");
  g_return_if_fail (section);

  if (g_list_store_find (section, item, &position))
    g_list_store_remove (section, position);
}

static void
shortcut_custom_items_changed (CcKeyboardShortcutDialog *self)
{
  GListStore *section;
  GtkWidget *page;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_DIALOG (self));

  section = keyboard_shortcut_get_section_store (self, "custom", "Custom Shortcuts");

  if (self->visible_section == section)
    {
      guint n_items;

      n_items = g_list_model_get_n_items (G_LIST_MODEL (section));

      if (n_items)
        page = GTK_WIDGET (self->shortcut_list_stack);
      else
        page = GTK_WIDGET (self->empty_custom_shortcut_page);
    }
  else
    page = GTK_WIDGET (self->shortcut_list_stack);

  gtk_stack_set_visible_child (self->subview_stack, page);
}

static int
compare_sections_title (gconstpointer a,
                        gconstpointer b,
                        gpointer     user_data)
{
  GObject *obj_a, *obj_b;

  const char *title_a, *title_b, *id_a, *id_b;

  obj_a = G_OBJECT (a);
  obj_b = G_OBJECT (b);

  id_a = g_object_get_data (obj_a, "id");
  id_b = g_object_get_data (obj_b, "id");

  /* Always place custom row as the last item */
  if (g_str_equal (id_a, "custom"))
    return 1;

  if (g_str_equal (id_b, "custom"))
    return -1;

  title_a = _(g_object_get_data (obj_a, "title"));
  title_b = _(g_object_get_data (obj_b, "title"));

  return g_strcmp0 (title_a, title_b);
}

static void
shortcut_search_result_changed_cb (CcKeyboardShortcutDialog *self)
{
  GListModel *model;
  GtkWidget *page;
  guint n_items;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_DIALOG (self));

  /* If a section is already shown, it is handled in search change callback */
  if (self->visible_section)
    return;

  model = G_LIST_MODEL (self->filtered_shortcuts);
  n_items = g_list_model_get_n_items (model);

  if (n_items == 0)
    page = GTK_WIDGET (self->empty_results_page);
  else if (self->search_terms)
    page = GTK_WIDGET (self->search_result_page);
  else
    page = GTK_WIDGET (self->section_list_page);

  gtk_stack_set_visible_child (self->section_stack, page);
}

/* All items have loaded, now sort the groups and add them to the page */
static void
shortcuts_loaded_cb (CcKeyboardShortcutDialog *self)
{
  g_autoptr(GPtrArray) filtered_items = NULL;
  g_autoptr(GPtrArray) widgets = NULL;
  GListStore *filtered_lists;
  GListStore *custom_store;
  guint n_items;

  /* Ensure that custom shorcuts section exists */
  custom_store = keyboard_shortcut_get_section_store (self, "custom", "Custom Shortcuts");
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->sections));
  widgets = g_ptr_array_new ();
  filtered_items = g_ptr_array_new ();
  filtered_lists = g_list_store_new (G_TYPE_LIST_MODEL);

  g_signal_connect_object (custom_store, "items-changed",
                           G_CALLBACK (shortcut_custom_items_changed),
                           self, G_CONNECT_SWAPPED);

  g_list_store_sort (self->sections, compare_sections_title, NULL);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = NULL;
      CcKeyboardShortcutGroup *group;
      GListModel *model;
      GtkWidget *page;

      item = g_list_model_get_item (G_LIST_MODEL (self->sections), i);
      group = g_object_get_data (item, "search-group");
      g_ptr_array_add (widgets, group);

      model = cc_keyboard_shortcut_group_get_model (group);
      g_ptr_array_add (filtered_items, model);

      /* Populate shortcut section page */
      group = g_object_get_data (item, "group");
      page = adw_preferences_page_new ();
      g_object_set_data (item, "page", page);
      adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));
      gtk_stack_add_child (self->shortcut_list_stack, page);
    }

  /* Populate search results page */
  for (guint i = 0; i < widgets->len; i++)
    adw_preferences_page_add (self->search_result_page, widgets->pdata[i]);

  /* Keep track of search results so as to update empty state */
  g_list_store_splice (filtered_lists, 0, 0, filtered_items->pdata, filtered_items->len);
  self->filtered_shortcuts = gtk_flatten_list_model_new (G_LIST_MODEL (filtered_lists));

  g_signal_connect_object (self->filtered_shortcuts, "items-changed",
                           G_CALLBACK (shortcut_search_result_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

static GtkWidget *
shortcut_dialog_row_new (gpointer item,
                         gpointer user_data)
{
  GtkWidget *row, *group;
  const char *title;

  group = g_object_get_data (item, "search-group");
  title = g_object_get_data (item, "title");
  row = g_object_new (CC_TYPE_LIST_ROW, NULL);

  g_object_set_data (G_OBJECT (row), "section", item);
  cc_list_row_set_show_arrow (CC_LIST_ROW (row), TRUE);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _(title));

  g_object_bind_property (group, "modified-text",
                          row, "secondary-label",
                          G_BINDING_SYNC_CREATE);

  return row;
}

static void
add_custom_shortcut_clicked_cb (CcKeyboardShortcutDialog *self)
{
  CcKeyboardShortcutEditor *editor;

  editor = CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor);

  cc_keyboard_shortcut_editor_set_mode (editor, CC_SHORTCUT_EDITOR_CREATE);
  cc_keyboard_shortcut_editor_set_item (editor, NULL);

  gtk_widget_set_visible (self->shortcut_editor, TRUE);
}

static void
on_reset_all_dialog_response_cb (CcKeyboardShortcutDialog *self,
                                 gchar                    *response,
                                 AdwMessageDialog         *dialog)
{
  guint n_items, j_items;

  gtk_window_destroy (GTK_WINDOW (dialog));

  if (g_strcmp0 (response, "cancel") == 0)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->sections));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GListModel) section = NULL;

      section = g_list_model_get_item (G_LIST_MODEL (self->sections), i);
      j_items = g_list_model_get_n_items (section);

      for (guint j = 0; j < j_items; j++)
        {
          g_autoptr(CcKeyboardItem) item = NULL;

          item = g_list_model_get_item (section, j);

          /* Don't reset custom shortcuts */
          if (cc_keyboard_item_get_item_type (item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
            return;

          /* cc_keyboard_manager_reset_shortcut() already resets conflicting shortcuts,
           * so no other check is needed here. */
          cc_keyboard_manager_reset_shortcut (self->manager, item);
        }
    }
}

static void
reset_all_clicked_cb (CcKeyboardShortcutDialog *self)
{
  GtkWidget *dialog;

  dialog = adw_message_dialog_new (GTK_WINDOW (self),
                                   _("Reset All Shortcuts?"),
                                   NULL);

  adw_message_dialog_format_body (ADW_MESSAGE_DIALOG (dialog),
                                  _("All changes to keyboard shortcuts will be lost."));

  adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                    "cancel",    _("_Cancel"),
                                    "reset_all", _("_Reset All"),
                                    NULL);

  adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                              "reset_all",
                                              ADW_RESPONSE_DESTRUCTIVE);

  adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog),
                                           "cancel");

  adw_message_dialog_set_close_response (ADW_MESSAGE_DIALOG (dialog),
                                         "cancel");

  g_signal_connect_swapped (dialog,
                            "response",
                            G_CALLBACK (on_reset_all_dialog_response_cb),
                            self);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
shortcut_dialog_visible_page_changed_cb (CcKeyboardShortcutDialog *self)
{
  gpointer visible_page;
  gboolean is_main_view;

  visible_page = adw_navigation_view_get_visible_page (self->navigation_view);
  is_main_view = visible_page == self->main_page;

  if (is_main_view)
    {
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
      gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));

      self->visible_section = NULL;
    }
  else if (self->visible_section)
    {
      const char *title;

      title = g_object_get_data (G_OBJECT (self->visible_section), "title");
      adw_navigation_page_set_title (self->subview_page, _(title) ?: "");
    }
}

static void
shortcut_search_entry_changed_cb (CcKeyboardShortcutDialog *self)
{
  g_autofree char *search = NULL;
  const char *search_text;
  guint n_items;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_DIALOG (self));

  /* Don't update search if we are in a subview */
  if (self->visible_section)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->sections));
  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  search = cc_util_normalize_casefold_and_unaccent (search_text);

  g_clear_pointer (&self->search_terms, g_strfreev);
  if (search && *search && *search != ' ')
    self->search_terms = g_strsplit (search, " ", -1);

  /* "Reset all..." button should be sensitive only if the search is not active */
  gtk_widget_set_sensitive (GTK_WIDGET (self->reset_all_button), !self->search_terms);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = NULL;
      CcKeyboardShortcutGroup *group;

      item = g_list_model_get_item (G_LIST_MODEL (self->sections), i);
      group = g_object_get_data (item, "search-group");

      cc_keyboard_shortcut_group_set_filter (group, self->search_terms);
    }

  shortcut_search_result_changed_cb (self);
}

static void
shortcut_search_entry_stopped_cb (CcKeyboardShortcutDialog *self)
{
  const char *search_text;
  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));

  if (search_text && g_strcmp0 (search_text, "") != 0)
    gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  else
    gtk_window_close (GTK_WINDOW (self));
}

static void
shortcut_section_row_activated_cb (CcKeyboardShortcutDialog *self,
                                   GtkListBoxRow            *row)
{
  GListStore *section;
  GtkWidget *page;

  g_assert (CC_IS_KEYBOARD_SHORTCUT_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  section = g_object_get_data (G_OBJECT (row), "section");
  self->visible_section = section;

  page = g_object_get_data (G_OBJECT (section), "page");
  gtk_stack_set_visible_child (self->shortcut_list_stack, page);
  adw_navigation_view_push (self->navigation_view, self->subview_page);
  shortcut_custom_items_changed (self);
}

static void
cc_keyboard_shortcut_dialog_constructed (GObject *object)
{
  CcKeyboardShortcutDialog *self = CC_KEYBOARD_SHORTCUT_DIALOG (object);

  G_OBJECT_CLASS (cc_keyboard_shortcut_dialog_parent_class)->constructed (object);

  /* Setup the dialog's transient parent */
  gtk_window_set_transient_for (GTK_WINDOW (self->shortcut_editor), GTK_WINDOW (self));
}

static void
cc_keyboard_shortcut_dialog_finalize (GObject *object)
{
  CcKeyboardShortcutDialog *self = CC_KEYBOARD_SHORTCUT_DIALOG (object);

  g_clear_object (&self->manager);
  g_clear_object (&self->sections);
  g_clear_pointer (&self->search_terms, g_strfreev);
  g_clear_object (&self->sections);
  g_clear_object (&self->filtered_shortcuts);
  g_clear_pointer ((GtkWindow**)&self->shortcut_editor, gtk_window_destroy);

  G_OBJECT_CLASS (cc_keyboard_shortcut_dialog_parent_class)->finalize (object);
}

static void
cc_keyboard_shortcut_dialog_class_init (CcKeyboardShortcutDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_keyboard_shortcut_dialog_constructed;
  object_class->finalize = cc_keyboard_shortcut_dialog_finalize;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "keyboard/cc-keyboard-shortcut-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, main_page);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, reset_all_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, section_stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, section_list_page);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, section_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, search_result_page);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, empty_results_page);

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, subview_page);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, subview_stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, shortcut_list_stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, empty_custom_shortcut_page);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, accelerator_size_group);

  gtk_widget_class_bind_template_callback (widget_class, add_custom_shortcut_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_all_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_dialog_visible_page_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_search_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_search_entry_stopped_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_section_row_activated_cb);
}

static void
cc_keyboard_shortcut_dialog_init (CcKeyboardShortcutDialog *self)
{
  GtkWindow *toplevel;

  gtk_widget_init_template (GTK_WIDGET (self));
  shortcut_dialog_visible_page_changed_cb (self);

  self->manager = cc_keyboard_manager_new ();

  toplevel = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self)));
  self->shortcut_editor = cc_keyboard_shortcut_editor_new (toplevel, self->manager);
  shortcut_dialog_visible_page_changed_cb (self);

  self->sections = g_list_store_new (G_TYPE_LIST_STORE);

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (shortcut_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "shortcut-removed",
                           G_CALLBACK (shortcut_removed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "shortcuts-loaded",
                           G_CALLBACK (shortcuts_loaded_cb),
                           self, G_CONNECT_SWAPPED);

  cc_keyboard_manager_load_shortcuts (self->manager);

  gtk_list_box_bind_model (self->section_list_box,
                           G_LIST_MODEL (self->sections),
                           shortcut_dialog_row_new,
                           self, NULL);
}

GtkWidget*
cc_keyboard_shortcut_dialog_new (void)
{
  return g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_DIALOG, NULL);
}
