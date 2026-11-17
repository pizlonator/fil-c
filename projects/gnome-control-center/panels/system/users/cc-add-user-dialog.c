/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <adwaita.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <act/act.h>

#include "cc-add-user-dialog.h"
#include "cc-list-row.h"
#include "user-utils.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600

static void   dialog_validate      (CcAddUserDialog *self);

static void   add_button_clicked_cb (CcAddUserDialog *self);

struct _CcAddUserDialog {
        AdwWindow parent_instance;

        GtkButton          *add_button;
        GtkSwitch          *local_account_type_switch;
        GtkEntry           *local_name_entry;
        GtkImage           *local_name_status_icon;
        AdwPreferencesPage *local_page;
        AdwActionRow       *local_password_row;
        GtkImage           *local_password_status_icon;
        GtkLevelBar        *local_strength_indicator;
        GtkComboBoxText    *local_username_combo;
        GtkListStore       *local_username_model;
        GtkPasswordEntry   *local_password_entry;
        GtkLabel           *local_password_hint;
        GtkCheckButton     *local_password_radio;
        GtkEntry           *local_username_entry;
        AdwActionRow       *local_username_row;
        GtkImage           *local_username_status_icon;
        GtkPasswordEntry   *local_verify_entry;
        AdwActionRow       *local_verify_password_row;
        GtkImage           *local_verify_status_icon;
        AdwPreferencesGroup *password_group;
        GtkSpinner         *spinner;

        GCancellable       *cancellable;
        GPermission        *permission;
        ActUser            *user;

        gboolean            has_custom_username;
        gint                local_name_timeout_id;
        gint                local_username_timeout_id;
        ActUserPasswordMode local_password_mode;
        gint                local_password_timeout_id;
        gboolean            local_valid_username;
};

G_DEFINE_TYPE (CcAddUserDialog, cc_add_user_dialog, ADW_TYPE_WINDOW);

static void
show_error_dialog (CcAddUserDialog *self,
                   const gchar     *message,
                   GError          *error)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", message);

        if (error != NULL) {
                g_dbus_error_strip_remote_error (error);
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", error->message);
        }

        g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
begin_action (CcAddUserDialog *self)
{
        g_debug ("Beginning action, disabling dialog controls");

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

        gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);
        gtk_spinner_start (self->spinner);
}

static void
finish_action (CcAddUserDialog *self)
{
        g_debug ("Completed domain action");

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), TRUE);

        gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
        gtk_spinner_stop (self->spinner);
}

static void
user_loaded_cb (CcAddUserDialog *self,
                GParamSpec      *pspec,
                ActUser         *user)
{
  const gchar *password;

  finish_action (self);

  /* Set a password for the user */
  password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_entry));
  act_user_set_password_mode (user, self->local_password_mode);
  if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR)
        act_user_set_password (user, password, "");

  self->user = g_object_ref (user);
  gtk_window_close (GTK_WINDOW (self));
}

static void
create_user_done (ActUserManager  *manager,
                  GAsyncResult    *res,
                  CcAddUserDialog *self)
{
        ActUser *user;
        g_autoptr(GError) error = NULL;

        /* Note that user is returned without an extra reference */

        user = act_user_manager_create_user_finish (manager, res, &error);

        if (user == NULL) {
                finish_action (self);
                g_debug ("Failed to create user: %s", error->message);
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR, ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                gtk_widget_grab_focus (GTK_WIDGET (self->local_name_entry));
        } else {
                g_debug ("Created user: %s", act_user_get_user_name (user));

                /* Check if the returned object is fully loaded before returning it */
                if (act_user_is_loaded (user))
                        user_loaded_cb (self, NULL, user);
                else
                        g_signal_connect_object (user, "notify::is-loaded", G_CALLBACK (user_loaded_cb), self, G_CONNECT_SWAPPED);
        }
}

static void
local_create_user (CcAddUserDialog *self)
{
        ActUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;

        begin_action (self);

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_name_entry));
        username = gtk_combo_box_text_get_active_text (self->local_username_combo);
        account_type = gtk_switch_get_active (self->local_account_type_switch) ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;

        g_debug ("Creating local user: %s", username);

        manager = act_user_manager_get_default ();
        act_user_manager_create_user_async (manager,
                                            username,
                                            name,
                                            account_type,
                                            self->cancellable,
                                            (GAsyncReadyCallback)create_user_done,
                                            self);
}

static gint
update_password_strength (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *username;
        const gchar *hint;
        const gchar *verify;
        gint strength_level;

        password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_entry));
        username = gtk_combo_box_text_get_active_text (self->local_username_combo);

        pw_strength (password, NULL, username, &hint, &strength_level);

        gtk_level_bar_set_value (self->local_strength_indicator, strength_level);
        gtk_label_set_label (self->local_password_hint, hint);

        if (strength_level > 1) {
                gtk_image_set_from_icon_name (self->local_password_status_icon, "emblem-ok-symbolic");
        } else if (strlen (password) == 0) {
                gtk_image_set_from_icon_name (self->local_password_status_icon, "dialog-warning-symbolic");
        } else {
                gtk_image_set_from_icon_name (self->local_password_status_icon, "dialog-warning-symbolic");
        }

        verify = gtk_editable_get_text (GTK_EDITABLE (self->local_verify_entry));
        if (strlen (verify) == 0) {
                gtk_widget_set_sensitive (GTK_WIDGET (self->local_verify_entry), strength_level > 1);
        }

        return strength_level;
}

static gboolean
local_validate (CcAddUserDialog *self)
{
        gboolean valid_name;
        gboolean valid_password;
        const gchar *name;
        const gchar *password;
        const gchar *verify;
        gint strength;

        if (self->local_valid_username) {
                gtk_image_set_from_icon_name (self->local_username_status_icon, "emblem-ok-symbolic");
        }

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_name_entry));
        valid_name = is_valid_name (name);
        if (valid_name) {
                gtk_image_set_from_icon_name (self->local_name_status_icon, "emblem-ok-symbolic");
        }

        password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_entry));
        verify = gtk_editable_get_text (GTK_EDITABLE (self->local_verify_entry));
        if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                strength = update_password_strength (self);
                valid_password = strength > 1 && strcmp (password, verify) == 0;
        } else {
                valid_password = TRUE;
        }

        return valid_name && self->local_valid_username && valid_password;
}

static void local_username_is_valid_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;
        g_autofree gchar *tip = NULL;
        g_autofree gchar *name = NULL;
        g_autofree gchar *username = NULL;
        gboolean valid;

        valid = is_valid_username_finish (result, &tip, &username, &error);
        if (error != NULL) {
                g_warning ("Could not check username by usermod: %s", error->message);
                valid = TRUE;
        }

        name = gtk_combo_box_text_get_active_text (self->local_username_combo);
        if (g_strcmp0 (name, username) == 0) {
                self->local_valid_username = valid;
                adw_action_row_set_subtitle (ADW_ACTION_ROW (self->local_username_row), tip);
                dialog_validate (self);
        }
}

static gboolean
local_username_timeout (CcAddUserDialog *self)
{
        g_autofree gchar *name = NULL;

        self->local_username_timeout_id = 0;

        name = gtk_combo_box_text_get_active_text (self->local_username_combo);
        is_valid_username_async (name, NULL, local_username_is_valid_cb, g_object_ref (self));

        return FALSE;
}

static gboolean
local_username_combo_focus_out_event_cb (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->local_username_timeout_id, g_source_remove);

        local_username_timeout (self);

        return FALSE;
}

static void
local_username_combo_changed_cb (CcAddUserDialog *self)
{
        const gchar *username;

        username = gtk_editable_get_text (GTK_EDITABLE (self->local_username_entry));
        if (*username == '\0')
                self->has_custom_username = FALSE;
        else if (gtk_widget_has_focus (GTK_WIDGET (self->local_username_entry)) ||
                 gtk_combo_box_get_active (GTK_COMBO_BOX (self->local_username_combo)) > 0)
                self->has_custom_username = TRUE;

        g_clear_handle_id (&self->local_username_timeout_id, g_source_remove);

        gtk_image_set_from_icon_name (self->local_username_status_icon, "dialog-warning-symbolic");
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

        self->local_valid_username = FALSE;
        self->local_username_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_username_timeout, self);
}

static gboolean
local_name_timeout (CcAddUserDialog *self)
{
        self->local_name_timeout_id = 0;

        dialog_validate (self);

        return FALSE;
}

static gboolean
local_name_entry_focus_out_event_cb (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->local_name_timeout_id, g_source_remove);

        local_name_timeout (self);

        return FALSE;
}

static void
generate_username_choices (const gchar  *name,
                           GtkListStore *store)
{
        gboolean in_use, same_as_initial;
        g_autofree gchar *lc_name = NULL;
        g_autofree gchar *ascii_name = NULL;
        g_autofree gchar *stripped_name = NULL;
        g_auto(GStrv) words1 = NULL;
        char **w1, **w2;
        char *c;
        char *unicode_fallback = "?";
        g_autoptr(GString) first_word = NULL;
        g_autoptr(GString) last_word = NULL;
        g_autoptr(GString) item0 = NULL;
        g_autoptr(GString) item1 = NULL;
        g_autoptr(GString) item2 = NULL;
        g_autoptr(GString) item3 = NULL;
        g_autoptr(GString) item4 = NULL;
        int len;
        int nwords1, nwords2, i;
        g_autoptr(GHashTable) items = NULL;
        GtkTreeIter iter;
        gsize max_name_length;

        gtk_list_store_clear (store);

        ascii_name = g_convert_with_fallback (name, -1, "ASCII//TRANSLIT", "UTF-8",
                                              unicode_fallback, NULL, NULL, NULL);
        /* Re-try without TRANSLIT. musl does not implement it */
        if (ascii_name == NULL)
                ascii_name = g_convert_with_fallback (name, -1, "ASCII", "UTF-8",
                                                      unicode_fallback, NULL, NULL, NULL);
        if (ascii_name == NULL)
                return;

        lc_name = g_ascii_strdown (ascii_name, -1);

        /* Remove all non ASCII alphanumeric chars from the name,
         * apart from the few allowed symbols.
         *
         * We do remove '.', even though it is usually allowed,
         * since it often comes in via an abbreviated middle name,
         * and the dot looks just wrong in the proposals then.
         */
        stripped_name = g_strnfill (strlen (lc_name) + 1, '\0');
        i = 0;
        for (c = lc_name; *c; c++) {
                if (!(g_ascii_isdigit (*c) || g_ascii_islower (*c) ||
                    *c == ' ' || *c == '-' || *c == '_' ||
                    /* used to track invalid words, removed below */
                    *c == '?') )
                        continue;

                    stripped_name[i] = *c;
                    i++;
        }

        if (strlen (stripped_name) == 0) {
                return;
        }

        /* we split name on spaces, and then on dashes, so that we can treat
         * words linked with dashes the same way, i.e. both fully shown, or
         * both abbreviated
         */
        words1 = g_strsplit_set (stripped_name, " ", -1);
        len = g_strv_length (words1);

        /* The default item is a concatenation of all words without ? */
        item0 = g_string_sized_new (strlen (stripped_name));

        /* Concatenate the whole first word with the first letter of each
         * word (item1), and the last word with the first letter of each
         * word (item2). item3 and item4 are symmetrical respectively to
         * item1 and item2.
         *
         * Constant 5 is the max reasonable number of words we may get when
         * splitting on dashes, since we can't guess it at this point,
         * and reallocating would be too bad.
         */
        item1 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);
        item3 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);

        item2 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);
        item4 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);

        /* again, guess at the max size of names */
        first_word = g_string_sized_new (20);
        last_word = g_string_sized_new (20);

        nwords1 = 0;
        nwords2 = 0;
        for (w1 = words1; *w1; w1++) {
                g_auto(GStrv) words2 = NULL;

                if (strlen (*w1) == 0)
                        continue;

                /* skip words with string '?', most likely resulting
                 * from failed transliteration to ASCII
                 */
                if (strstr (*w1, unicode_fallback) != NULL)
                        continue;

                nwords1++; /* count real words, excluding empty string */

                item0 = g_string_append (item0, *w1);

                words2 = g_strsplit_set (*w1, "-", -1);
                /* reset last word if a new non-empty word has been found */
                if (strlen (*words2) > 0)
                        last_word = g_string_set_size (last_word, 0);

                for (w2 = words2; *w2; w2++) {
                        if (strlen (*w2) == 0)
                                continue;

                        nwords2++;

                        /* part of the first "toplevel" real word */
                        if (nwords1 == 1) {
                                item1 = g_string_append (item1, *w2);
                                first_word = g_string_append (first_word, *w2);
                        }
                        else {
                                item1 = g_string_append_unichar (item1,
                                                                 g_utf8_get_char (*w2));
                                item3 = g_string_append_unichar (item3,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* not part of the last "toplevel" word */
                        if (w1 != words1 + len - 1) {
                                item2 = g_string_append_unichar (item2,
                                                                 g_utf8_get_char (*w2));
                                item4 = g_string_append_unichar (item4,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* always save current word so that we have it if last one reveals empty */
                        last_word = g_string_append (last_word, *w2);
                }
        }
        item2 = g_string_append (item2, last_word->str);
        item3 = g_string_append (item3, first_word->str);
        item4 = g_string_prepend (item4, last_word->str);

        max_name_length = get_username_max_length ();

        g_string_truncate (first_word, max_name_length);
        g_string_truncate (last_word, max_name_length);

        g_string_truncate (item0, max_name_length);
        g_string_truncate (item1, max_name_length);
        g_string_truncate (item2, max_name_length);
        g_string_truncate (item3, max_name_length);
        g_string_truncate (item4, max_name_length);

        items = g_hash_table_new (g_str_hash, g_str_equal);

        in_use = is_username_used (item0->str);
        if (!in_use && !g_ascii_isdigit (item0->str[0])) {
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, 0, item0->str, -1);
                g_hash_table_insert (items, item0->str, item0->str);
        }

        in_use = is_username_used (item1->str);
        same_as_initial = (g_strcmp0 (item0->str, item1->str) == 0);
        if (!same_as_initial && nwords2 > 0 && !in_use && !g_ascii_isdigit (item1->str[0])) {
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, 0, item1->str, -1);
                g_hash_table_insert (items, item1->str, item1->str);
        }

        /* if there's only one word, would be the same as item1 */
        if (nwords2 > 1) {
                /* add other items */
                in_use = is_username_used (item2->str);
                if (!in_use && !g_ascii_isdigit (item2->str[0]) &&
                    !g_hash_table_lookup (items, item2->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item2->str, -1);
                        g_hash_table_insert (items, item2->str, item2->str);
                }

                in_use = is_username_used (item3->str);
                if (!in_use && !g_ascii_isdigit (item3->str[0]) &&
                    !g_hash_table_lookup (items, item3->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item3->str, -1);
                        g_hash_table_insert (items, item3->str, item3->str);
                }

                in_use = is_username_used (item4->str);
                if (!in_use && !g_ascii_isdigit (item4->str[0]) &&
                    !g_hash_table_lookup (items, item4->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item4->str, -1);
                        g_hash_table_insert (items, item4->str, item4->str);
                }

                /* add the last word */
                in_use = is_username_used (last_word->str);
                if (!in_use && !g_ascii_isdigit (last_word->str[0]) &&
                    !g_hash_table_lookup (items, last_word->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, last_word->str, -1);
                        g_hash_table_insert (items, last_word->str, last_word->str);
                }

                /* ...and the first one */
                in_use = is_username_used (first_word->str);
                if (!in_use && !g_ascii_isdigit (first_word->str[0]) &&
                    !g_hash_table_lookup (items, first_word->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, first_word->str, -1);
                        g_hash_table_insert (items, first_word->str, first_word->str);
                }
        }
}

static void
local_name_entry_changed_cb (CcAddUserDialog *self)
{
        const char *name;

        gtk_list_store_clear (self->local_username_model);

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_name_entry));
        if ((name == NULL || strlen (name) == 0) && !self->has_custom_username) {
                gtk_editable_set_text (GTK_EDITABLE (self->local_username_entry), "");
        } else if (name != NULL && strlen (name) != 0) {
                generate_username_choices (name, self->local_username_model);
                if (!self->has_custom_username)
                        gtk_combo_box_set_active (GTK_COMBO_BOX (self->local_username_combo), 0);
        }

        g_clear_handle_id (&self->local_name_timeout_id, g_source_remove);

        gtk_image_set_from_icon_name (self->local_name_status_icon, "dialog-warning-symbolic");
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

        self->local_name_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_name_timeout, self);
}

static void
update_password_match (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *verify;
        const gchar *message = "";

        password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_entry));
        verify = gtk_editable_get_text (GTK_EDITABLE (self->local_verify_entry));
        if (strlen (verify) != 0) {
                if (strcmp (password, verify) != 0) {
                        message = _("The passwords do not match.");
                } else {
                        gtk_image_set_from_icon_name (self->local_verify_status_icon, "emblem-ok-symbolic");
                }
        }
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->local_verify_password_row), message);
}

static void
generate_password (CcAddUserDialog *self)
{
        g_autofree gchar *pwd = NULL;

        pwd = pw_generate ();
        if (pwd == NULL)
                return;

        gtk_editable_set_text (GTK_EDITABLE (self->local_password_entry), pwd);
        gtk_editable_set_text (GTK_EDITABLE (self->local_verify_entry), pwd);
        gtk_widget_set_sensitive (GTK_WIDGET (self->local_verify_entry), TRUE);
}

static gboolean
local_password_timeout (CcAddUserDialog *self)
{
        self->local_password_timeout_id = 0;

        dialog_validate (self);
        update_password_match (self);

        return FALSE;
}

static gboolean
password_focus_out_event_cb (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->local_password_timeout_id, g_source_remove);

        local_password_timeout (self);

        return FALSE;
}

static gboolean
local_password_entry_key_press_event_cb (CcAddUserDialog       *self,
                                         guint                  keyval,
                                         guint                  keycode,
                                         GdkModifierType        state,
                                         GtkEventControllerKey *controller)
{
        if (keyval == GDK_KEY_Tab)
               local_password_timeout (self);

        return FALSE;
}

static void
recheck_password_match (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->local_password_timeout_id, g_source_remove);

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

        self->local_password_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_password_timeout, self);
}

static void
local_password_entry_changed_cb (CcAddUserDialog *self)
{
        gtk_image_set_from_icon_name (self->local_password_status_icon, "dialog-warning-symbolic");
        gtk_image_set_from_icon_name (self->local_verify_status_icon, "dialog-warning-symbolic");
        recheck_password_match (self);
}

static void
local_verify_entry_changed_cb (CcAddUserDialog *self)
{
        gtk_image_set_from_icon_name (self->local_verify_status_icon, "dialog-warning-symbolic");
        recheck_password_match (self);
}

static void
local_password_radio_changed_cb (CcAddUserDialog *self)
{
        gboolean active;

        active = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->local_password_radio));
        self->local_password_mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;

        gtk_widget_set_sensitive (GTK_WIDGET (self->password_group), active);

        dialog_validate (self);
}

static void
dialog_validate (CcAddUserDialog *self)
{
        gboolean valid = FALSE;

        valid = local_validate (self);

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), valid);
}

static void
cc_add_user_dialog_init (CcAddUserDialog *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();

        self->local_password_mode = ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        dialog_validate (self);
        update_password_strength (self);
        local_username_timeout (self);
}

static void
on_permission_acquired (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;

        /* Paired with begin_action in cc_add_user_dialog_response () */
        finish_action (self);

        if (g_permission_acquire_finish (self->permission, res, &error)) {
                g_return_if_fail (g_permission_get_allowed (self->permission));
                add_button_clicked_cb (self);
        } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                g_warning ("Failed to acquire permission: %s", error->message);
        }
}

static void
add_button_clicked_cb (CcAddUserDialog *self)
{
        /* We don't (or no longer) have necessary permissions */
        if (self->permission && !g_permission_get_allowed (self->permission)) {
                begin_action (self);
                g_permission_acquire_async (self->permission, self->cancellable,
                                            on_permission_acquired, g_object_ref (self));
                return;
        }

        local_create_user (self);
}

static void
cc_add_user_dialog_dispose (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        if (self->cancellable)
                g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->user);

        g_clear_handle_id (&self->local_password_timeout_id, g_source_remove);
        g_clear_handle_id (&self->local_name_timeout_id, g_source_remove);
        g_clear_handle_id (&self->local_username_timeout_id, g_source_remove);

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->dispose (obj);
}

static void
cc_add_user_dialog_finalize (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        g_clear_object (&self->cancellable);
        g_clear_object (&self->permission);

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->finalize (obj);
}

static void
cc_add_user_dialog_class_init (CcAddUserDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = cc_add_user_dialog_dispose;
        object_class->finalize = cc_add_user_dialog_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-add-user-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, add_button);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_account_type_switch);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_page);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_hint);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_status_icon);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_name_entry);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_name_status_icon);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_combo);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_model);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_entry);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_radio);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_entry);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_status_icon);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_strength_indicator);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_verify_entry);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_verify_password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_verify_status_icon);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_group);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, spinner);

        gtk_widget_class_bind_template_callback (widget_class, add_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, dialog_validate);
        gtk_widget_class_bind_template_callback (widget_class, generate_password);
        gtk_widget_class_bind_template_callback (widget_class, local_name_entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_name_entry_focus_out_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_password_entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_password_entry_key_press_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_password_radio_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_username_combo_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_username_combo_focus_out_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_verify_entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_focus_out_event_cb);
}

CcAddUserDialog *
cc_add_user_dialog_new (GPermission *permission)
{
        CcAddUserDialog *self;

        self = g_object_new (CC_TYPE_ADD_USER_DIALOG, NULL);

        if (permission != NULL)
                self->permission = g_object_ref (permission);

        return self;
}

ActUser *
cc_add_user_dialog_get_user (CcAddUserDialog *self)
{
        g_return_val_if_fail (CC_IS_ADD_USER_DIALOG (self), NULL);
        return self->user;
}
