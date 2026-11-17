/*
 * cc-users-page.c
 *
 * Copyright 2023 Red Hat Inc
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-users-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-add-user-dialog.h"
#include "cc-enterprise-login-dialog.h"
#include "cc-users-page.h"
#include "cc-list-row.h"
#include "cc-user-page.h"
#include "user-utils.h"

#include <act/act.h>
#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#define USER_ACCOUNTS_PERMISSION "org.gnome.controlcenter.user-accounts.administration"

struct _CcUsersPage {
    AdwNavigationPage  parent_instance;

    GtkButton         *add_user_button;
    GtkButton         *add_enterprise_user_button;
    CcUserPage        *current_user_page;
    AdwNavigationView *navigation;
    GtkWidget         *other_users_group;
    GtkListBox        *user_list;

    GListStore        *model;
    GPermission       *permission;
    ActUserManager    *user_manager;
};

G_DEFINE_TYPE (CcUsersPage, cc_users_page, ADW_TYPE_NAVIGATION_PAGE)

static void
add_enterprise_user (CcUsersPage *self)
{
    CcEnterpriseLoginDialog *dialog = cc_enterprise_login_dialog_new ();

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
add_user (CcUsersPage *self)
{
    CcAddUserDialog *dialog = cc_add_user_dialog_new (self->permission);

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
on_user_row_activated (CcUsersPage  *self,
                       AdwActionRow *row)
{
    CcUserPage *user_page;
    ActUser *user;
    uid_t uid;

    uid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "uid"));
    user = act_user_manager_get_user_by_id (act_user_manager_get_default (), uid);

    user_page = cc_user_page_new ();
    cc_user_page_set_user (user_page, user, self->permission);
    adw_navigation_view_push (self->navigation, ADW_NAVIGATION_PAGE (user_page));
}

static void
on_other_users_model_changed (CcUsersPage *self)
{
    gtk_widget_set_visible (self->other_users_group,
                            g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0);
}

static GtkWidget *
create_user_row (gpointer item, gpointer user_data)
{
    ActUser *user;
    GtkWidget *row, *user_image;

    row = g_object_new (CC_TYPE_LIST_ROW, "show-arrow", TRUE, NULL);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
    adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (row), FALSE);

    user = item;
    g_object_set_data (G_OBJECT (row), "uid", GINT_TO_POINTER (act_user_get_uid (user)));
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                   get_real_or_user_name (user));
    user_image = adw_avatar_new (32, NULL, TRUE);
    setup_avatar_for_user (ADW_AVATAR (user_image), user);
    adw_action_row_add_prefix (ADW_ACTION_ROW (row), user_image);

    return row;
}

static gint
sort_users (gconstpointer a, gconstpointer b, gpointer user_data)
{
    ActUser *ua, *ub;
    g_autofree gchar *name1 = NULL;
    g_autofree gchar *name2 = NULL;

    ua = ACT_USER ((gpointer*)a);
    ub = ACT_USER ((gpointer*)b);

    /* Make sure the current user is shown first */
    if (act_user_get_uid (ua) == getuid ()) {
        return -G_MAXINT32;
    } else if (act_user_get_uid (ub) == getuid ()) {
        return G_MAXINT32;
    }

    name1 = g_utf8_collate_key (get_real_or_user_name (ua), -1);
    name2 = g_utf8_collate_key (get_real_or_user_name (ub), -1);

    return strcmp (name1, name2);
}

static void
on_user_changed (CcUsersPage *self,
                 ActUser     *user)
{
  CcUserPage *page;

  /* Refresh the users list */
  g_list_store_sort (self->model, sort_users, self);

  /* If the user has a page open, refresh that page */
  page = CC_USER_PAGE (adw_navigation_view_find_page (self->navigation, act_user_get_user_name (user)));
  if (page != NULL)
    cc_user_page_set_user (page, user, self->permission);
}

static void
on_user_added (CcUsersPage *self,
               ActUser     *user)
{
  CcUserPage *page;
  g_list_store_insert_sorted (self->model, user, sort_users, self);

  page = CC_USER_PAGE (adw_navigation_view_get_visible_page (self->navigation));
  if (page != self->current_user_page)
    return;

  /* We're on the current user's page and a new user was just created. It's very likely the user
   * was just created by our own add-user dialog. So let's display the new user */

  page = cc_user_page_new ();
  cc_user_page_set_user (page, user, self->permission);
  adw_navigation_view_push (self->navigation, ADW_NAVIGATION_PAGE (page));
}

static void
on_user_removed (CcUsersPage *self,
                 ActUser     *user)
{
  AdwNavigationPage *page;
  guint position;

  if (g_list_store_find (self->model, user, &position))
    g_list_store_remove (self->model, position);

  page = adw_navigation_view_find_page (self->navigation, act_user_get_user_name (user));
  if (page != NULL)
    adw_navigation_view_pop_to_page (self->navigation, ADW_NAVIGATION_PAGE (self->current_user_page));
}

static void
users_loaded (CcUsersPage *self)
{
    g_autoptr(GSList) user_list = NULL;
    GSList *l;
    guint n_users;

    user_list = act_user_manager_list_users (self->user_manager);
    for (l = user_list; l; l = l->next) {
        ActUser *user = ACT_USER (l->data);

        if (act_user_is_system_account (user)) {
            continue;
        }

        /* Increase the user count for all accounts except for "system" accounts, such
         * as "root" or "nobody". */
        n_users++;
        if (act_user_get_uid (user) == getuid ()) {
            cc_user_page_set_user (self->current_user_page, user, self->permission);

            continue;
        }

        g_list_store_insert_sorted (self->model,
                                    user,
                                    sort_users,
                                    self);
    }
}

static void
check_realmd_list_names_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    g_autoptr(CcUsersPage) self = CC_USERS_PAGE (user_data);
    g_autoptr(GVariant) ret = NULL;
    g_autoptr(GVariant) names = NULL;
    GVariantIter iter;
    gchar *name;
    g_autoptr(GError) error = NULL;

    ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
    if (ret == NULL) {
        g_warning ("Unable to query dbus: %s", error->message);
        return;
    }

    names = g_variant_get_child_value (ret, 0);
    g_variant_iter_init (&iter, names);
    while (g_variant_iter_loop (&iter, "&s", &name)) {
        if (g_str_equal (name, "org.freedesktop.realmd"))
            gtk_widget_set_visible (GTK_WIDGET (self->add_enterprise_user_button), TRUE);
    }
}

static void
check_realmd (CcUsersPage *self)
{
    g_autoptr(GDBusConnection) connection = NULL;
    g_autoptr(GError) error = NULL;

    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL) {
        g_warning ("Unable to connect to dbus: %s", error->message);
        return;
    }

    g_dbus_connection_call (connection,
                            "org.freedesktop.DBus",
                            "/org/freedesktop/DBus",
                            "org.freedesktop.DBus",
                            "ListActivatableNames",
                            NULL,
                            G_VARIANT_TYPE ("(as)"),
                            G_DBUS_CALL_FLAGS_NONE,
                            -1, NULL,
                            check_realmd_list_names_cb,
                            g_object_ref (self));
}

static void
cc_users_page_dispose (GObject *object)
{
    CcUsersPage *self = CC_USERS_PAGE (object);

    g_clear_object (&self->model);
    g_clear_object (&self->permission);

    G_OBJECT_CLASS (cc_users_page_parent_class)->dispose (object);
}

static void
cc_users_page_init (CcUsersPage *self)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GtkCssProvider) provider = NULL;
    gboolean is_loaded = FALSE;

    gtk_widget_init_template (GTK_WIDGET (self));

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider,
                                         "/org/gnome/control-center/system/users/user-accounts-dialog.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    self->model = g_list_store_new (ACT_TYPE_USER);
    gtk_list_box_bind_model (self->user_list,
                             G_LIST_MODEL (self->model),
                             (GtkListBoxCreateWidgetFunc)create_user_row,
                             self,
                             NULL);
    g_signal_connect_object (self->model,
                             "items-changed",
                             G_CALLBACK (on_other_users_model_changed),
                             self,
                             G_CONNECT_SWAPPED);
    self->permission = (GPermission *)polkit_permission_new_sync (USER_ACCOUNTS_PERMISSION, NULL, NULL, &error);
    if (self->permission == NULL) {
        g_warning ("Cannot create '%s' permission: %s", USER_ACCOUNTS_PERMISSION, error->message);
    }

    g_object_bind_property (self->permission, "allowed", self->add_user_button, "sensitive", G_BINDING_SYNC_CREATE);

    self->user_manager = act_user_manager_get_default ();
    g_signal_connect_object (self->user_manager,
                             "notify::is-loaded",
                             G_CALLBACK (users_loaded),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->user_manager,
                             "user-added",
                             G_CALLBACK (on_user_added),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->user_manager,
                             "user-removed",
                             G_CALLBACK (on_user_removed),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->user_manager,
                             "user-changed",
                             G_CALLBACK (on_user_changed),
                             self,
                             G_CONNECT_SWAPPED);

    g_object_get (self->user_manager, "is-loaded", &is_loaded, NULL);
    if (is_loaded) {
        users_loaded (self);
    }

    check_realmd (self);
}

static void
cc_users_page_class_init (CcUsersPageClass * klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = cc_users_page_dispose;

    g_type_ensure (CC_TYPE_LIST_ROW);
    g_type_ensure (CC_TYPE_USER_PAGE);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-users-page.ui");

    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, add_enterprise_user_button);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, add_user_button);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, current_user_page);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, navigation);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, other_users_group);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, user_list);

    gtk_widget_class_bind_template_callback (widget_class, add_user);
    gtk_widget_class_bind_template_callback (widget_class, add_enterprise_user);
    gtk_widget_class_bind_template_callback (widget_class, on_user_row_activated);
}
