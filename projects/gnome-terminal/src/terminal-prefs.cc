/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
 * Copyright © 2008, 2011, 2012, 2013 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <uuid.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "profile-editor.hh"
#include "terminal-prefs.hh"
#include "terminal-accels.hh"
#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-schemas.hh"
#include "terminal-util.hh"
#include "terminal-profiles-list.hh"
#include "terminal-libgsystem.hh"

PrefData *the_pref_data = nullptr;  /* global */

/* Bottom */

static void
prefs_dialog_help_button_clicked_cb (GtkWidget *button,
                                     PrefData *data)
{
  terminal_util_show_help ("pref");
}

static void
prefs_dialog_close_button_clicked_cb (GtkWidget *button,
                                      PrefData *data)
{
  gtk_widget_destroy (data->dialog);
}

/* Sidebar */

static inline GSimpleAction *
lookup_action (GtkWindow *window,
               const char *name)
{
  GAction *action;

  action = g_action_map_lookup_action (G_ACTION_MAP (window), name);
  g_return_val_if_fail (action != nullptr, nullptr);

  return G_SIMPLE_ACTION (action);
}

/* Update the sidebar (visibility of icons, sensitivity of menu entries) to reflect the default and the selected profiles. */
static void
listbox_update (GtkListBox *box)
{
  int i;
  GtkListBoxRow *row;
  GSettings *profile;
  gs_unref_object GSettings *default_profile;
  GtkStack *stack;
  GtkMenuButton *button;

  default_profile = terminal_settings_list_ref_default_child (the_pref_data->profiles_list);

  /* GTK+ doesn't seem to like if a popover is assigned to multiple buttons at once
   * (not even temporarily), so make sure to remove it from the previous button first. */
  for (i = 0; (row = gtk_list_box_get_row_at_index (box, i)) != nullptr; i++) {
    button = (GtkMenuButton*)g_object_get_data (G_OBJECT (row), "popover-button");
    gtk_menu_button_set_popover (button, nullptr);
  }

  for (i = 0; (row = gtk_list_box_get_row_at_index (box, i)) != nullptr; i++) {
    profile = (GSettings*)g_object_get_data (G_OBJECT (row), "gsettings");

    gboolean is_selected_profile = (profile != nullptr && profile == the_pref_data->selected_profile);
    gboolean is_default_profile = (profile != nullptr && profile == default_profile);

    stack = (GtkStack*)g_object_get_data (G_OBJECT (row), "home-stack");
    gtk_stack_set_visible_child_name (stack, is_default_profile ? "home" : "placeholder");

    stack = (GtkStack*)g_object_get_data (G_OBJECT (row), "popover-stack");
    gtk_stack_set_visible_child_name (stack, is_selected_profile ? "button" : "placeholder");
    if (is_selected_profile) {
      g_simple_action_set_enabled (lookup_action (GTK_WINDOW (the_pref_data->dialog), "delete"), !is_default_profile);
      g_simple_action_set_enabled (lookup_action (GTK_WINDOW (the_pref_data->dialog), "set-as-default"), !is_default_profile);

      GtkPopover *popover_menu = GTK_POPOVER (gtk_builder_get_object (the_pref_data->builder, "popover-menu"));
      button = (GtkMenuButton*)g_object_get_data (G_OBJECT (row), "popover-button");
      gtk_menu_button_set_popover (button, GTK_WIDGET (popover_menu));
      gtk_popover_set_relative_to (popover_menu, GTK_WIDGET (button));
    }
  }
}

static void
update_window_title (void)
{
  GtkListBoxRow *row = the_pref_data->selected_list_box_row;
  if (row == nullptr)
    return;

  GSettings *profile = (GSettings*)g_object_get_data (G_OBJECT (row), "gsettings");
  GtkLabel *label = (GtkLabel*)g_object_get_data (G_OBJECT (row), "label");
  const char *text = gtk_label_get_text (label);
  gs_free char *subtitle;
  gs_free char *title;

  if (profile == nullptr) {
    subtitle = g_strdup (text);
  } else {
    subtitle = g_strdup_printf (_("Profile “%s”"), text);
  }

  title = g_strdup_printf (_("Preferences – %s"), subtitle);
  gtk_window_set_title (GTK_WINDOW (the_pref_data->dialog), title);
}

/* A new entry is selected in the sidebar */
static void
listbox_row_selected_cb (GtkListBox *box,
                         GtkListBoxRow *row,
                         GtkStack *stack)
{
  profile_prefs_unload ();

  /* row can be nullptr intermittently during a profile meta operations */
  g_free (the_pref_data->selected_profile_uuid);
  if (row != nullptr) {
    the_pref_data->selected_profile = (GSettings*)g_object_get_data (G_OBJECT (row), "gsettings");
    the_pref_data->selected_profile_uuid = g_strdup ((char const*)g_object_get_data (G_OBJECT (row), "uuid"));
  } else {
    the_pref_data->selected_profile = nullptr;
    the_pref_data->selected_profile_uuid = nullptr;
  }
  the_pref_data->selected_list_box_row = row;

  listbox_update (box);

  if (row != nullptr) {
    if (the_pref_data->selected_profile != nullptr) {
      profile_prefs_load (the_pref_data->selected_profile_uuid, the_pref_data->selected_profile);
    }

    char const* stack_child_name = (char const*)g_object_get_data (G_OBJECT (row), "stack_child_name");
    gtk_stack_set_visible_child_name (stack, stack_child_name);
  }

  update_window_title ();
}

/* A profile's name changed, perhaps externally */
static void
profile_name_changed_cb (GtkLabel      *label,
                         GParamSpec    *pspec,
                         GtkListBoxRow *row)
{
  gtk_list_box_row_changed (row);  /* trigger re-sorting */

  if (row == the_pref_data->selected_list_box_row)
    update_window_title ();
}

/* Select a profile in the sidebar by UUID */
static gboolean
listbox_select_profile (const char *uuid)
{
  GtkListBoxRow *row;
  for (int i = 0; (row = gtk_list_box_get_row_at_index (the_pref_data->listbox, i)) != nullptr; i++) {
    const char *rowuuid = (char const*) g_object_get_data (G_OBJECT (row), "uuid");
    if (g_strcmp0 (rowuuid, uuid) == 0) {
      g_signal_emit_by_name (row, "activate");
      return TRUE;
    }
  }
  return FALSE;
}

/* Create a new profile now, select it, update the UI. */
static void
profile_new_now (const char *name)
{
  gs_free char *uuid = terminal_app_new_profile (terminal_app_get (), nullptr, name);

  listbox_select_profile (uuid);
}

/* Clone the selected profile now, select it, update the UI. */
static void
profile_clone_now (const char *name)
{
  if (the_pref_data->selected_profile == nullptr)
    return;

  gs_free char *uuid = terminal_app_new_profile (terminal_app_get (), the_pref_data->selected_profile, name);

  listbox_select_profile (uuid);
}

/* Rename the selected profile now, update the UI. */
static void
profile_rename_now (const char *name)
{
  if (the_pref_data->selected_profile == nullptr)
    return;

  /* This will automatically trigger a call to profile_name_changed_cb(). */
  g_settings_set_string (the_pref_data->selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, name);
}

/* Delete the selected profile now, update the UI. */
static void
profile_delete_now (const char *dummy)
{
  if (the_pref_data->selected_profile == nullptr)
    return;

  /* Prepare to select the next one, or if there's no such then the previous one. */
  int index = gtk_list_box_row_get_index (the_pref_data->selected_list_box_row);
  GtkListBoxRow *new_selected_row = gtk_list_box_get_row_at_index (the_pref_data->listbox, index + 1);
  if (new_selected_row == nullptr)
    new_selected_row = gtk_list_box_get_row_at_index (the_pref_data->listbox, index - 1);
  GSettings *new_selected_profile = (GSettings*)g_object_get_data (G_OBJECT (new_selected_row), "gsettings");
  gs_free char *uuid = nullptr;
  if (new_selected_profile != nullptr)
    uuid = terminal_settings_list_dup_uuid_from_child (the_pref_data->profiles_list, new_selected_profile);

  terminal_app_remove_profile (terminal_app_get (), the_pref_data->selected_profile);

  listbox_select_profile (uuid);
}

/* "Set as default" selected. Do it now without asking for confirmation. */
static void
profile_set_as_default_cb (GSimpleAction *simple,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  if (the_pref_data->selected_profile_uuid == nullptr)
    return;

  /* This will automatically trigger a call to listbox_update() via "default-changed". */
  terminal_settings_list_set_default_child (the_pref_data->profiles_list, the_pref_data->selected_profile_uuid);
}


static void
popover_dialog_cancel_clicked_cb (GtkButton *button,
                                  gpointer user_data)
{
  GtkPopover *popover_dialog = GTK_POPOVER (gtk_builder_get_object (the_pref_data->builder, "popover-dialog"));

  gtk_popover_popdown (popover_dialog);
}

static void
popover_dialog_ok_clicked_cb (GtkButton *button,
                              void (*fn) (const char *))
{
  GtkEntry *entry = GTK_ENTRY (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-entry"));
  const char *name = gtk_entry_get_text (entry);

  /* Perform what we came for */
  (*fn) (name);

  /* Hide/popdown the popover */
  popover_dialog_cancel_clicked_cb (button, nullptr);
}

static void
popover_dialog_closed_cb (GtkPopover *popover,
                          gpointer   user_data)
{

  GtkEntry *entry = GTK_ENTRY (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-entry"));
  gtk_entry_set_text (entry, "");

  GtkButton *ok = GTK_BUTTON (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-ok"));
  GtkButton *cancel = GTK_BUTTON (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-cancel"));

  g_signal_handlers_disconnect_matched (ok, G_SIGNAL_MATCH_FUNC, 0, 0, nullptr,
                                        (void*)popover_dialog_ok_clicked_cb, nullptr);
  g_signal_handlers_disconnect_matched (cancel, G_SIGNAL_MATCH_FUNC, 0, 0, nullptr,
                                        (void*)popover_dialog_cancel_clicked_cb, nullptr);
  g_signal_handlers_disconnect_matched (popover, G_SIGNAL_MATCH_FUNC, 0, 0, nullptr,
                                        (void*)popover_dialog_closed_cb, nullptr);
}


/* Updates the OK button's sensitivity (insensitive if entry field is empty or whitespace only).
 * The entry's initial value and OK's initial sensitivity have to match in the .ui file. */
static void
popover_dialog_notify_text_cb (GtkEntry   *entry,
                               GParamSpec *pspec,
                               GtkWidget  *ok)
{
  gs_free char *text = g_strchomp (g_strdup (gtk_entry_get_text (entry)));
  gtk_widget_set_sensitive (ok, text[0] != '\0');
}


/* Common dialog for entering new profile name, or confirming deletion */
static void
profile_popup_dialog (GtkWidget *relative_to,
                      const char *header,
                      const char *body,
                      const char *entry_text,
                      const char *ok_text,
                      void (*fn) (const char *))
{
  GtkLabel *label1 = GTK_LABEL (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-label1"));
  gtk_label_set_text (label1, header);

  GtkLabel *label2 = GTK_LABEL (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-label2"));
  gtk_label_set_text (label2, body);

  GtkEntry *entry = GTK_ENTRY (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-entry"));
  if (entry_text != nullptr) {
    gtk_entry_set_text (entry, entry_text);
    gtk_widget_show (GTK_WIDGET (entry));
  } else {
    gtk_entry_set_text (entry, ".");  /* to make the OK button sensitive */
    gtk_widget_hide (GTK_WIDGET (entry));
  }

  GtkButton *ok = GTK_BUTTON (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-ok"));
  gtk_button_set_label (ok, ok_text);
  GtkButton *cancel = GTK_BUTTON (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-cancel"));
  GtkPopover *popover_dialog = GTK_POPOVER (gtk_builder_get_object (the_pref_data->builder, "popover-dialog"));

  g_signal_connect (ok, "clicked", G_CALLBACK (popover_dialog_ok_clicked_cb), (void*)fn);
  g_signal_connect (cancel, "clicked", G_CALLBACK (popover_dialog_cancel_clicked_cb), nullptr);
  g_signal_connect (popover_dialog, "closed", G_CALLBACK (popover_dialog_closed_cb), nullptr);

  gtk_popover_set_relative_to (popover_dialog, relative_to);
  gtk_popover_set_position (popover_dialog, GTK_POS_BOTTOM);
  gtk_popover_set_default_widget (popover_dialog, GTK_WIDGET (ok));

  gtk_popover_popup (popover_dialog);

  gtk_widget_grab_focus (entry_text != nullptr ? GTK_WIDGET (entry) : GTK_WIDGET (cancel));
}

/* "New" selected, ask for profile name */
static void
profile_new_cb (GtkButton *button,
                gpointer   user_data)
{
  profile_popup_dialog (GTK_WIDGET (the_pref_data->new_profile_button),
                        _("New Profile"),
                        _("Enter name for new profile with default settings:"),
                        "",
                        _("Create"),
                        profile_new_now);
}

/* "Clone" selected, ask for profile name */
static void
profile_clone_cb (GSimpleAction *simple,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  gs_free char *name = g_settings_get_string (the_pref_data->selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  gs_free char *label = g_strdup_printf (_("Enter name for new profile based on “%s”:"), name);
  gs_free char *clone_name = g_strdup_printf (_("%s (Copy)"), name);

  profile_popup_dialog (GTK_WIDGET (the_pref_data->selected_list_box_row),
                        _("Clone Profile"),
                        label,
                        clone_name,
                        _("Clone"),
                        profile_clone_now);
}

/* "Rename" selected, ask for new name */
static void
profile_rename_cb (GSimpleAction *simple,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  if (the_pref_data->selected_profile == nullptr)
    return;

  gs_free char *name = g_settings_get_string (the_pref_data->selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  gs_free char *label = g_strdup_printf (_("Enter new name for profile “%s”:"), name);

  profile_popup_dialog (GTK_WIDGET (the_pref_data->selected_list_box_row),
                        _("Rename Profile"),
                        label,
                        name,
                        _("Rename"),
                        profile_rename_now);
}

/* "Delete" selected, ask for confirmation */
static void
profile_delete_cb (GSimpleAction *simple,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  if (the_pref_data->selected_profile == nullptr)
    return;

  gs_free char *name = g_settings_get_string (the_pref_data->selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  gs_free char *label = g_strdup_printf (_("Really delete profile “%s”?"), name);

  profile_popup_dialog (GTK_WIDGET (the_pref_data->selected_list_box_row),
                        _("Delete Profile"),
                        label,
                        nullptr,
                        _("Delete"),
                        profile_delete_now);
}

/* Create a (non-header) row of the sidebar, either a global or a profile entry. */
static GtkListBoxRow *
listbox_create_row (const char *name,
                    const char *stack_child_name,
                    const char *uuid,
                    GSettings  *gsettings /* adopted */,
                    gpointer    sort_order)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());

  g_object_set_data_full (G_OBJECT (row), "stack_child_name", g_strdup (stack_child_name), g_free);
  g_object_set_data_full (G_OBJECT (row), "uuid", g_strdup (uuid), g_free);
  if (gsettings != nullptr)
    g_object_set_data_full (G_OBJECT (row), "gsettings", gsettings, (GDestroyNotify)g_object_unref);
  g_object_set_data (G_OBJECT (row), "sort_order", sort_order);

  GtkBox *hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_set_margin_start (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (hbox), 6);

  GtkLabel *label = GTK_LABEL (gtk_label_new (name));
  if (gsettings != nullptr) {
    g_signal_connect (label, "notify::label", G_CALLBACK (profile_name_changed_cb), row);
    g_settings_bind (gsettings,
                     TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                     label,
                     "label",
                     G_SETTINGS_BIND_GET);
  }
  gtk_label_set_xalign (label, 0);
  gtk_box_pack_start (hbox, GTK_WIDGET (label), TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (row), "label", label);

  /* Always add the "default" symbol and the "menu" button, even on rows of global prefs.
   * Use GtkStack to possible achieve visibility:hidden on it.
   * This is so that all listbox rows have the same dimensions, and the width doesn't change
   * as you switch the default profile. */

  GtkStack *popover_stack = GTK_STACK (gtk_stack_new ());
  gtk_widget_set_margin_start (GTK_WIDGET (popover_stack), 6);
  GtkMenuButton *popover_button = GTK_MENU_BUTTON (gtk_menu_button_new ());
  gtk_button_set_relief (GTK_BUTTON (popover_button), GTK_RELIEF_NONE);
  gtk_stack_add_named (popover_stack, GTK_WIDGET (popover_button), "button");
  GtkLabel *popover_label = GTK_LABEL (gtk_label_new (""));
  gtk_stack_add_named (popover_stack, GTK_WIDGET (popover_label), "placeholder");
  g_object_set_data (G_OBJECT (row), "popover-stack", popover_stack);
  g_object_set_data (G_OBJECT (row), "popover-button", popover_button);

  gtk_box_pack_end (hbox, GTK_WIDGET (popover_stack), FALSE, FALSE, 0);

  GtkStack *home_stack = GTK_STACK (gtk_stack_new ());
  gtk_widget_set_margin_start (GTK_WIDGET (home_stack), 12);
  GtkImage *home_image = GTK_IMAGE (gtk_image_new_from_icon_name ("emblem-default-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_tooltip_text (GTK_WIDGET (home_image), _("This is the default profile"));
  gtk_stack_add_named (home_stack, GTK_WIDGET (home_image), "home");
  GtkLabel *home_label = GTK_LABEL (gtk_label_new (""));
  gtk_stack_add_named (home_stack, GTK_WIDGET (home_label), "placeholder");
  g_object_set_data (G_OBJECT (row), "home-stack", home_stack);

  gtk_box_pack_end (hbox, GTK_WIDGET (home_stack), FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (hbox));

  gtk_widget_show_all (GTK_WIDGET (row));

  gtk_stack_set_visible_child_name (popover_stack, "placeholder");
  gtk_stack_set_visible_child_name (home_stack, "placeholder");

  return row;
}

/* Add all the non-profile rows to the sidebar */
static void
listbox_add_all_globals (PrefData *data)
{
  GtkListBoxRow *row;

  row = listbox_create_row (_("General"),
                            "general-prefs",
                            nullptr, nullptr, (gpointer) 0);
  gtk_list_box_insert (data->listbox, GTK_WIDGET (row), -1);

  row = listbox_create_row (_("Shortcuts"),
                            "shortcut-prefs",
                            nullptr, nullptr, (gpointer) 1);
  gtk_list_box_insert (data->listbox, GTK_WIDGET (row), -1);
}

/* Remove all the profile rows from the sidebar */
static void
listbox_remove_all_profiles (PrefData *data)
{
  int i = 0;

  data->selected_profile = nullptr;
  g_free (data->selected_profile_uuid);
  data->selected_profile_uuid = nullptr;
  profile_prefs_unload ();

  GtkListBoxRow *row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (the_pref_data->listbox), 0);
  g_signal_emit_by_name (row, "activate");

  while ((row = gtk_list_box_get_row_at_index (data->listbox, i)) != nullptr) {
    if (g_object_get_data (G_OBJECT (row), "gsettings") != nullptr) {
      gtk_widget_destroy (GTK_WIDGET (row));
    } else {
      i++;
    }
  }
}

/* Add all the profiles to the sidebar */
static void
listbox_add_all_profiles (PrefData *data)
{
  GList *list, *l;
  GtkListBoxRow *row;

  list = terminal_settings_list_ref_children (data->profiles_list);

  for (l = list; l != nullptr; l = l->next) {
    GSettings *profile = (GSettings *) l->data;
    gs_free gchar *uuid = terminal_settings_list_dup_uuid_from_child (data->profiles_list, profile);

    row = listbox_create_row (nullptr,
                              "profile-prefs",
                              uuid,
                              profile /* adopts */,
                              (gpointer) 42);
    gtk_list_box_insert (data->listbox, GTK_WIDGET (row), -1);
  }

  g_list_free(list); /* the items themselves were adopted into the model above */

  listbox_update (data->listbox);  /* FIXME: This is not needed but I don't know why :-) */
}

/* Re-add all the profiles to the sidebar.
 * This is called when a profile is added or removed, and also when the list of profiles is
 * modified externally.
 * Try to keep the selected profile, whenever possible.
 * When the list is modified externally, the terminal_settings_list_*() methods seem to preserve
 * the GSettings object for every profile that remains in the list. There's no guarantee however
 * that a newly created GSettings can't receive the same address that a ceased one used to have.
 * So don't rely on GSettings* to keep track of the selected profile, use the UUID instead. */
static void
listbox_readd_profiles (PrefData *data)
{
  gs_free char *uuid = g_strdup (data->selected_profile_uuid);

  listbox_remove_all_profiles (data);
  listbox_add_all_profiles (data);

  if (uuid != nullptr)
    listbox_select_profile (uuid);
}

/* Create a header row ("Global" or "Profiles +") */
static GtkWidget *
listboxrow_create_header (const char *text,
                          gboolean visible_button)
{
  GtkBox *hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_set_margin_start (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (hbox), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (hbox), 6);

  GtkLabel *label = GTK_LABEL (gtk_label_new (nullptr));
  gs_free char *markup = g_markup_printf_escaped ("<b>%s</b>", text);
  gtk_label_set_markup (label, markup);
  gtk_label_set_xalign (label, 0);
  gtk_box_pack_start (hbox, GTK_WIDGET (label), TRUE, TRUE, 0);

  /* Always add a "new profile" button. Use GtkStack to possible achieve visibility:hidden on it.
   * This is so that both header rows have the same dimensions. */

  GtkStack *stack = GTK_STACK (gtk_stack_new ());
  GtkButton *button = GTK_BUTTON (gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_button_set_relief (button, GTK_RELIEF_NONE);
  gtk_stack_add_named (stack, GTK_WIDGET (button), "button");
  GtkLabel *labelx = GTK_LABEL (gtk_label_new (""));
  gtk_stack_add_named (stack, GTK_WIDGET (labelx), "placeholder");

  gtk_box_pack_end (hbox, GTK_WIDGET (stack), FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (hbox));

  if (visible_button) {
    gtk_stack_set_visible_child_name (stack, "button");
    g_signal_connect (button, "clicked", G_CALLBACK (profile_new_cb), nullptr);
    the_pref_data->new_profile_button = GTK_WIDGET (button);
  } else {
    gtk_stack_set_visible_child_name (stack, "placeholder");
  }

  return GTK_WIDGET (hbox);
}

/* Manage the creation or removal of the header row ("Global" or "Profiles +") */
static void
listboxrow_update_header (GtkListBoxRow *row,
                          GtkListBoxRow *before,
                          gpointer       user_data)
{
  if (before == nullptr) {
    if (gtk_list_box_row_get_header (row) == nullptr) {
      gtk_list_box_row_set_header (row, listboxrow_create_header (_("Global"), FALSE));
    }
    return;
  }

  GSettings *profile = (GSettings*)g_object_get_data (G_OBJECT (row), "gsettings");
  if (profile != nullptr) {
    GSettings *profile_before = (GSettings*)g_object_get_data (G_OBJECT (before), "gsettings");
    if (profile_before != nullptr) {
      gtk_list_box_row_set_header (row, nullptr);
    } else {
      if (gtk_list_box_row_get_header (row) == nullptr) {
        gtk_list_box_row_set_header (row, listboxrow_create_header (_("Profiles"), TRUE));
      }
    }
  }
}

/* Sort callback for rows of the sidebar (global and profile ones).
 * Global ones are kept at the top in fixed order. This is implemented via sort_order
 * which is an integer disguised as a pointer for ease of implementation.
 * Profile ones are sorted lexicographically. */
static gint
listboxrow_compare_cb (GtkListBoxRow *row1,
                       GtkListBoxRow *row2,
                       gpointer       user_data)
{
  gpointer sort_order_1 = g_object_get_data (G_OBJECT (row1), "sort_order");
  gpointer sort_order_2 = g_object_get_data (G_OBJECT (row2), "sort_order");

  if (sort_order_1 != sort_order_2)
    return sort_order_1 < sort_order_2 ? -1 : 1;

  GtkLabel *label1 = (GtkLabel*)g_object_get_data (G_OBJECT (row1), "label");
  const char *text1 = gtk_label_get_text (label1);
  GtkLabel *label2 = (GtkLabel*)g_object_get_data (G_OBJECT (row2), "label");
  const char *text2 = gtk_label_get_text (label2);

  return g_utf8_collate (text1, text2);
}

/* Keybindings tab */

/* Make sure the treeview is repainted with the correct text color, see bug 792139. */
static void
shortcuts_button_toggled_cb (GtkWidget *widget,
                             GtkTreeView *tree_view)
{
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

/* misc */

static void
prefs_dialog_destroy_cb (GtkWidget *widget,
                         PrefData *data)
{
  /* Don't run this handler again */
  g_signal_handlers_disconnect_by_func (widget, (void*)prefs_dialog_destroy_cb, data);

  g_signal_handlers_disconnect_by_func (data->profiles_list,
                                        (void*)listbox_readd_profiles, data);
  g_signal_handlers_disconnect_by_func (data->profiles_list,
                                        (void*)listbox_update, data->listbox);

  profile_prefs_destroy ();

  g_object_unref (data->builder);
  g_free (data->selected_profile_uuid);
  g_free (data);
}

static void
make_default_button_clicked_cb(GtkWidget* button,
                               PrefData* data)
{
  terminal_app_make_default_terminal(terminal_app_get());
}

void
terminal_prefs_show_preferences(GSettings* profile,
                                char const* widget_name,
                                unsigned timestamp)
{
  TerminalApp *app = terminal_app_get ();
  PrefData *data;
  GtkWidget *dialog, *tree_view;
  GtkWidget *show_menubar_button, *disable_mnemonics_button, *disable_menu_accel_button;
  GtkWidget *disable_shortcuts_button;
  GtkWidget *theme_variant_label, *theme_variant_combo;
  GtkWidget *new_terminal_mode_label, *new_terminal_mode_combo;
  GtkWidget *new_tab_position_combo;
  GtkWidget *close_button, *help_button;
  GtkWidget *content_box, *general_frame, *keybindings_frame;
  GtkWidget *always_check_default_button, *make_default_button;
  GSettings *settings;

  const GActionEntry action_entries[] = {
    { "clone",          profile_clone_cb,          nullptr, nullptr, nullptr },
    { "rename",         profile_rename_cb,         nullptr, nullptr, nullptr },
    { "delete",         profile_delete_cb,         nullptr, nullptr, nullptr },
    { "set-as-default", profile_set_as_default_cb, nullptr, nullptr, nullptr },
  };

  if (the_pref_data != nullptr)
    goto done;

  {
  the_pref_data = g_new0 (PrefData, 1);
  data = the_pref_data;
  data->profiles_list = terminal_app_get_profiles_list (app);

  /* FIXME this method is only used from here. Inline it here instead. */
  data->builder = terminal_util_load_widgets_resource ("/org/gnome/terminal/ui/preferences.ui",
                                       "preferences-dialog",
                                       "preferences-dialog", &dialog,
                                       "dialogue-content-box", &content_box,
                                       "general-frame", &general_frame,
                                       "keybindings-frame", &keybindings_frame,
                                       "close-button", &close_button,
                                       "help-button", &help_button,
                                       "default-show-menubar-checkbutton", &show_menubar_button,
                                       "theme-variant-label", &theme_variant_label,
                                       "theme-variant-combobox", &theme_variant_combo,
                                       "new-terminal-mode-label", &new_terminal_mode_label,
                                       "new-terminal-mode-combobox", &new_terminal_mode_combo,
                                       "disable-mnemonics-checkbutton", &disable_mnemonics_button,
                                       "disable-shortcuts-checkbutton", &disable_shortcuts_button,
                                       "disable-menu-accel-checkbutton", &disable_menu_accel_button,
                                       "new-tab-position-combobox", &new_tab_position_combo,
                                       "always-check-default-checkbutton", &always_check_default_button,
                                       "make-default-button", &make_default_button,
                                       "accelerators-treeview", &tree_view,
                                       "the-stack", &data->stack,
                                       "the-listbox", &data->listbox,
                                       nullptr);

  data->dialog = dialog;

  gtk_window_set_application (GTK_WINDOW (data->dialog), GTK_APPLICATION (terminal_app_get ()));

  terminal_util_bind_mnemonic_label_sensitivity (dialog);

  settings = terminal_app_get_global_settings (app);

  g_action_map_add_action_entries (G_ACTION_MAP (dialog),
                                   action_entries, G_N_ELEMENTS (action_entries),
                                   data);

  /* Sidebar */

  gtk_list_box_set_header_func (GTK_LIST_BOX (data->listbox),
                                listboxrow_update_header,
                                nullptr,
                                nullptr);
  g_signal_connect (data->listbox, "row-selected", G_CALLBACK (listbox_row_selected_cb), data->stack);
  gtk_list_box_set_sort_func (data->listbox, listboxrow_compare_cb, nullptr, nullptr);

  listbox_add_all_globals (data);
  listbox_add_all_profiles (data);
  g_signal_connect_swapped (data->profiles_list, "children-changed",
                            G_CALLBACK (listbox_readd_profiles), data);
  g_signal_connect_swapped (data->profiles_list, "default-changed",
                            G_CALLBACK (listbox_update), data->listbox);

  GtkEntry *entry = GTK_ENTRY (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-entry"));
  GtkButton *ok = GTK_BUTTON (gtk_builder_get_object (the_pref_data->builder, "popover-dialog-ok"));
  g_signal_connect (entry, "notify::text", G_CALLBACK (popover_dialog_notify_text_cb), ok);

  /* General page */

  gboolean shell_shows_menubar;
  g_object_get (gtk_settings_get_default (),
                "gtk-shell-shows-menubar", &shell_shows_menubar,
                nullptr);
  if (shell_shows_menubar || terminal_app_get_use_headerbar (app)) {
    gtk_widget_set_visible (show_menubar_button, FALSE);
  } else {
    g_settings_bind (settings,
                     TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY,
                     show_menubar_button,
                     "active",
                     GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  }

  g_settings_bind (settings,
                   TERMINAL_SETTING_THEME_VARIANT_KEY,
                   theme_variant_combo,
                   "active-id",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  if (terminal_app_get_menu_unified (app) ||
      terminal_app_get_use_headerbar (app)) {
    g_settings_bind (settings,
                     TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY,
                     new_terminal_mode_combo,
                     "active-id",
                     GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  } else {
    gtk_widget_set_visible (new_terminal_mode_label, FALSE);
    gtk_widget_set_visible (new_terminal_mode_combo, FALSE);
  }

  g_settings_bind (settings,
                   TERMINAL_SETTING_NEW_TAB_POSITION_KEY,
                   new_tab_position_combo,
                   "active-id",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  if (shell_shows_menubar) {
    gtk_widget_set_visible (disable_mnemonics_button, FALSE);
  } else {
    g_settings_bind (settings,
                     TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                     disable_mnemonics_button,
                     "active",
                     GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  }
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                   disable_menu_accel_button,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  g_settings_bind(settings,
                  TERMINAL_SETTING_ALWAYS_CHECK_DEFAULT_KEY,
                  always_check_default_button,
                  "active",
                  GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  g_signal_connect(make_default_button, "clicked",
                   G_CALLBACK(make_default_button_clicked_cb), data);

  g_object_bind_property(app, "is-default-terminal",
                         make_default_button, "sensitive",
                         GBindingFlags(G_BINDING_DEFAULT |
                                       G_BINDING_SYNC_CREATE |
                                       G_BINDING_INVERT_BOOLEAN));

  /* Shortcuts page */

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   disable_shortcuts_button,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  g_signal_connect (disable_shortcuts_button, "toggled",
                    G_CALLBACK (shortcuts_button_toggled_cb), tree_view);

  terminal_accels_fill_treeview (tree_view, disable_shortcuts_button);

  /* Profile page */

  profile_prefs_init ();

  /* Move action widgets to titlebar when headerbar is used */
  if (terminal_app_get_dialog_use_headerbar (app)) {
    GtkWidget *headerbar;
    GtkWidget *bbox;

    headerbar = (GtkWidget*)g_object_new (GTK_TYPE_HEADER_BAR,
					  "show-close-button", TRUE,
					  nullptr);
    bbox = gtk_widget_get_parent (help_button);

    gtk_container_remove (GTK_CONTAINER (bbox), (GtkWidget*)g_object_ref (help_button));
    gtk_header_bar_pack_start (GTK_HEADER_BAR (headerbar), help_button);
    g_object_unref (help_button);

    gtk_style_context_add_class (gtk_widget_get_style_context (help_button),
                                 "text-button");

    gtk_widget_show (headerbar);
    gtk_widget_hide (bbox);

    gtk_window_set_titlebar (GTK_WINDOW (dialog), headerbar);

    /* Remove extra spacing around the content, and extra frames */
    g_object_set (G_OBJECT (content_box), "margin", 0, nullptr);
    gtk_frame_set_shadow_type (GTK_FRAME (general_frame), GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type (GTK_FRAME (keybindings_frame), GTK_SHADOW_NONE);
  }

  /* misc */

  g_signal_connect (close_button, "clicked", G_CALLBACK (prefs_dialog_close_button_clicked_cb), data);
  g_signal_connect (help_button, "clicked", G_CALLBACK (prefs_dialog_help_button_clicked_cb), data);
  g_signal_connect (dialog, "destroy", G_CALLBACK (prefs_dialog_destroy_cb), data);

  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &the_pref_data);
  }

done:
  if (profile != nullptr) {
    gs_free char *uuid = terminal_settings_list_dup_uuid_from_child (the_pref_data->profiles_list, profile);
    listbox_select_profile (uuid);
  } else {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (the_pref_data->listbox), 0);
    g_signal_emit_by_name (row, "activate");
  }

  terminal_util_dialog_focus_widget (the_pref_data->builder, widget_name);

  gtk_window_present_with_time(GTK_WINDOW(the_pref_data->dialog), timestamp);
}
