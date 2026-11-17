/*
 * cc-user-page.c
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
#define G_LOG_DOMAIN "cc-user-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-user-page.h"
#include "cc-avatar-chooser.h"
#include "cc-fingerprint-dialog.h"
#include "cc-fingerprint-manager.h"
#include "cc-language-chooser.h"
#include "cc-list-row.h"
#include "cc-password-dialog.h"
#include "cc-permission-infobar.h"
#include "user-utils.h"

#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#ifdef HAVE_MALCONTENT
#include <libmalcontent/malcontent.h>
#endif

struct _CcUserPage {
    AdwNavigationPage    parent_instance;

    CcListRow           *account_type_row;
    GtkSwitch           *account_type_switch;
    GtkBox              *action_area;
    AdwAvatar           *avatar;
    CcAvatarChooser     *avatar_chooser;
    GtkMenuButton       *avatar_edit_button;
    GtkButton           *avatar_remove_button;
    AdwSwitchRow        *auto_login_row;
    CcListRow           *fingerprint_row;
    CcListRow           *language_row;
    AdwEntryRow         *fullname_row;
#ifdef HAVE_MALCONTENT
    CcListRow           *parental_controls_row;
#endif
    CcListRow           *password_row;
    CcPermissionInfobar *permission_infobar;
    AdwSwitchRow        *remove_local_files_choice;
    GtkWidget           *remove_user_button;
    AdwMessageDialog    *remove_local_user_dialog;

    ActUser              *user;
    GSettings            *login_screen_settings;
    GPermission          *permission;
    CcFingerprintManager *fingerprint_manager;

    gboolean              locked;
    gboolean              editable;
    gboolean              can_be_demoted;
};

static GtkBuildableIface *parent_buildable_iface;
static void cc_user_page_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcUserPage, cc_user_page, ADW_TYPE_NAVIGATION_PAGE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_user_page_buildable_init))

enum {
    PROP_0,
    PROP_LOCKED,
    PROP_EDITABLE,
    PROP_IS_ADMIN,
    PROP_IS_CURRENT_USER
};

static guint
get_num_active_admin (ActUserManager *um)
{
    g_autoptr(GSList) list = NULL;
    GSList *l;
    guint num_admin = 0;

    list = act_user_manager_list_users (um);
    for (l = list; l != NULL; l = l->next) {
        ActUser *u = l->data;
        if (act_user_get_account_type (u) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR && !act_user_get_locked (u)) {
            num_admin++;
        }
    }

    return num_admin;
}

static gboolean
would_demote_only_admin (ActUser *user)
{
    ActUserManager *um = act_user_manager_get_default ();

    /* Prevent the user from demoting the only admin account.
     * Returns TRUE when user is an administrator and there is only
     * one enabled administrator. */
    if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_STANDARD || act_user_get_locked (user)) {
        return FALSE;
    }

    if (get_num_active_admin (um) > 1) {
        return FALSE;
    }

    return TRUE;
}

static gboolean
get_autologin_possible (ActUser *user)
{
    gboolean locked;
    gboolean set_password_at_login;

    locked = act_user_get_locked (user);
    set_password_at_login = (act_user_get_password_mode (user) == ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);

    return !(locked || set_password_at_login);
}

static gchar *
get_user_language (ActUser *user)
{
    g_autofree gchar *lang = NULL;

    lang = g_strdup (act_user_get_language (user));
    if (lang && *lang != '\0') {
        return gnome_get_language_from_locale (lang, NULL);
    }

    return g_strdup ("—");
}

static const gchar *
get_invisible_text (void)
{
     GtkWidget *entry;
     gunichar invisible_char;
     static gchar invisible_text[40];
     gchar *p;
     gint i;

     entry = gtk_entry_new ();
     invisible_char = gtk_entry_get_invisible_char (GTK_ENTRY (entry));
     if (invisible_char == 0) {
        invisible_char = 0x2022;
     }

     g_object_ref_sink (entry);
     g_object_unref (entry);

     /* five bullets */
     p = invisible_text;
     for (i = 0; i < 5; i++) {
        p += g_unichar_to_utf8 (invisible_char, p);
     }
     *p = 0;

     return invisible_text;
}

static const gchar *
get_password_mode_text (ActUser *user)
{
    const gchar *text;

    if (act_user_get_locked (user)) {
        text = C_("Password mode", "Account disabled");
    } else {
        switch (act_user_get_password_mode (user)) {
        case ACT_USER_PASSWORD_MODE_REGULAR:
            text = get_invisible_text ();
            break;
        case ACT_USER_PASSWORD_MODE_SET_AT_LOGIN:
            text = C_("Password mode", "To be set at next login");
            break;
        case ACT_USER_PASSWORD_MODE_NONE:
            text = C_("Password mode", "None");
            break;
        default:
            g_assert_not_reached ();
        }
    }

    return text;
}

static void
account_type_changed (CcUserPage *self)
{
    ActUserAccountType account_type;
    gboolean is_admin;

    is_admin = gtk_switch_get_active (self->account_type_switch);
    account_type = is_admin ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;

    if (account_type != act_user_get_account_type (self->user)) {
        act_user_set_account_type (self->user, account_type);
    }
}

static void
update_generated_avatar (CcUserPage *self)
{
        g_autoptr(GdkTexture) texture = NULL;

        adw_avatar_set_custom_image (self->avatar, NULL);

        texture = draw_avatar_to_texture (self->avatar, AVATAR_PIXEL_SIZE);
        set_user_icon_data (self->user, texture, IMAGE_SOURCE_VALUE_GENERATED);
}

static void
fullname_entry_apply_cb (CcUserPage *self)
{
    const gchar *text;

    text = gtk_editable_get_text (GTK_EDITABLE (self->fullname_row));
    if (g_strcmp0 (text, act_user_get_real_name (self->user)) != 0 && is_valid_name (text)) {
        adw_avatar_set_text (self->avatar, text);

        act_user_set_real_name (self->user, text);

        if (adw_avatar_get_custom_image (self->avatar) == NULL) {
            update_generated_avatar (self);
        }

        g_debug ("Updating real name to: %s", text);
    }
}

static void
language_chooser_response (CcUserPage        *self,
                           guint              response_id,
                           CcLanguageChooser *chooser)
{
    g_autofree gchar *language_name = NULL;
    const gchar *selected_language;

    if (response_id != GTK_RESPONSE_OK) {
        gtk_window_destroy (GTK_WINDOW (chooser));

        return;
    }

    selected_language = cc_language_chooser_get_language (chooser);
    if (!selected_language) {
        return;
    }

    if (g_strcmp0 (selected_language, act_user_get_language (self->user)) == 0) {
        return;
    }

    act_user_set_language (self->user, selected_language);

    language_name = gnome_get_language_from_locale (selected_language, NULL);
    cc_list_row_set_secondary_label (self->language_row, language_name);

    gtk_window_close (GTK_WINDOW (chooser));
}

static void
change_language (CcUserPage *self)
{
    CcLanguageChooser *language_chooser;
    const gchar *current_language;

    current_language = act_user_get_language (self->user);
    language_chooser = cc_language_chooser_new ();

    g_signal_connect_object (language_chooser, "response",
                             G_CALLBACK (language_chooser_response), self,
                             G_CONNECT_SWAPPED);

    if (current_language && *current_language != '\0') {
        cc_language_chooser_set_language (language_chooser, current_language);
    }

    gtk_window_set_transient_for (GTK_WINDOW (language_chooser),
                                  GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
    gtk_window_present (GTK_WINDOW (language_chooser));
}

static void
change_password (CcUserPage *self)
{
    CcPasswordDialog *dialog = cc_password_dialog_new (self->user);
    GtkWindow *parent;

    parent = (GtkWindow *)gtk_widget_get_native (GTK_WIDGET (self));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
autologin_changed (CcUserPage *self)
{
    ActUserManager *user_manager = act_user_manager_get_default ();
    gboolean active;

    active = adw_switch_row_get_active (self->auto_login_row);
    if (active != act_user_get_automatic_login (self->user)) {
        act_user_set_automatic_login (self->user, active);

        if (act_user_get_automatic_login (self->user)) {
            g_autoptr(GSList) list = NULL;
            GSList *l;
            list = act_user_manager_list_users (user_manager);
            for (l = list; l != NULL; l = l->next) {
                ActUser *u = l->data;
                if (act_user_get_uid (u) != act_user_get_uid (self->user)) {
                    act_user_set_automatic_login (self->user, FALSE);
                }
            }
        }
    }
}

static void
update_fingerprint_row_state (CcUserPage           *self,
                              GParamSpec           *spec,
                              CcFingerprintManager *manager)
{
    CcFingerprintState state = cc_fingerprint_manager_get_state (manager);
    gboolean visible = FALSE;

    visible = (act_user_get_uid (self->user) == getuid () &&
               act_user_is_local_account (self->user) &&
               (self->login_screen_settings &&
                g_settings_get_boolean (self->login_screen_settings,
                                        "enable-fingerprint-authentication")));
    gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row), visible);
    if (!visible)
        return;

    if (state != CC_FINGERPRINT_STATE_UPDATING)
        gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row),
                                state != CC_FINGERPRINT_STATE_NONE);

    gtk_widget_set_sensitive (GTK_WIDGET (self->fingerprint_row),
                              state != CC_FINGERPRINT_STATE_UPDATING);

    if (state == CC_FINGERPRINT_STATE_ENABLED)
        cc_list_row_set_secondary_label (self->fingerprint_row, _("Enabled"));
    else if (state == CC_FINGERPRINT_STATE_DISABLED)
        cc_list_row_set_secondary_label (self->fingerprint_row, _("Disabled"));
}

static void
change_fingerprint (CcUserPage *self)
{
    CcFingerprintDialog *dialog;

    dialog = cc_fingerprint_dialog_new (self->fingerprint_manager);
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
delete_user_done (ActUserManager *manager,
                  GAsyncResult   *res)
{
    g_autoptr(GError) error = NULL;

    if (!act_user_manager_delete_user_finish (manager, res, &error)) {
        if (!g_error_matches (error, ACT_USER_MANAGER_ERROR,
                              ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
            g_critical ("Failed to delete user: %s", error->message);
    }
}

static void
remove_local_user_response (CcUserPage       *self,
                            gchar            *response,
                            AdwMessageDialog *dialog)
{
    gboolean remove_files;

    g_assert (ADW_IS_SWITCH_ROW (self->remove_local_files_choice));

    if (g_strcmp0 (response, "cancel") == 0) {
        return;
    }

    /* remove autologin */
    if (act_user_get_automatic_login (self->user)) {
        act_user_set_automatic_login (self->user, FALSE);
    }

    /* Prevent user to click again while deleting, issue #2341 */
    gtk_widget_set_sensitive (GTK_WIDGET (self->remove_user_button), FALSE);

    remove_files = adw_switch_row_get_active (self->remove_local_files_choice);
    act_user_manager_delete_user_async (act_user_manager_get_default (),
                                        self->user,
                                        remove_files,
                                        NULL,
                                        (GAsyncReadyCallback)delete_user_done,
                                        NULL);
}

static void
remove_user (CcUserPage *self)
{
    GtkWindow *parent;

    // TODO: Handle enterprise accounts
    parent = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self)));
    gtk_window_set_transient_for (GTK_WINDOW (self->remove_local_user_dialog), parent);
    adw_message_dialog_format_heading (self->remove_local_user_dialog, _("Remove %s?"), get_real_or_user_name (self->user));
    gtk_window_present (GTK_WINDOW (self->remove_local_user_dialog));
}

static void
remove_avatar (CcUserPage *self)
{
    gtk_widget_set_visible (GTK_WIDGET (self->avatar_remove_button), FALSE);
    update_generated_avatar (self);
}

static void
cc_user_page_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const gchar  *type)
{
    CcUserPage *self = CC_USER_PAGE (buildable);

    gtk_box_append (self->action_area, GTK_WIDGET (child));
}

static void
cc_user_page_buildable_init (GtkBuildableIface *iface)
{
    parent_buildable_iface = g_type_interface_peek_parent (iface);
    iface->add_child = cc_user_page_buildable_add_child;
}

static gboolean
is_current_user (ActUser *user)
{
    if (!user)
        return FALSE;

    return act_user_get_uid (user) == getuid ();
}

static void
update_editable_state (CcUserPage *self)
{
    self->editable = (is_current_user (self->user) || !self->locked) && act_user_is_local_account (self->user);
    g_object_notify (G_OBJECT (self), "editable");
}

#ifdef HAVE_MALCONTENT
static void
spawn_malcontent_control (CcUserPage *self)
{
    g_autoptr(GError) error = NULL;
    const gchar *argv[] = { "malcontent-control",
#ifdef HAVE_MALCONTENT_0_10
                        "--user", act_user_get_user_name (self->user),
#endif  /* HAVE_MALCONTENT_0_10 */
                        NULL
    };

    /* no-op if the user is administrator */
    if (act_user_get_account_type (self->user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR) {
        g_debug ("Not launching malcontent because selected user is an admin");

        return;
    }

    if (!g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
        g_debug ("Couldn't launch malcontent-control: %s", error->message);
    }
}

static gboolean
is_parental_controls_enabled_for_user (ActUser *user)
{
    g_autoptr(MctManager) manager = NULL;
    g_autoptr(MctAppFilter) app_filter = NULL;
    g_autoptr(GDBusConnection) system_bus = NULL;
    g_autoptr(GError) error = NULL;

    /* FIXME: should become asynchronous */
    system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (system_bus == NULL) {
        g_warning ("Error getting system bus while trying to show user details: %s", error->message);
        return FALSE;
    }

    manager = mct_manager_new (system_bus);
    app_filter = mct_manager_get_app_filter (manager,
                                             act_user_get_uid (user),
                                             MCT_MANAGER_GET_VALUE_FLAGS_NONE,
                                             NULL,
                                             &error);
    if (error) {
        if (!g_error_matches (error, MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_DISABLED))
                g_warning ("Error retrieving app filter for user %s: %s",
                           act_user_get_user_name (user),
                           error->message);

        return FALSE;
    }

    return mct_app_filter_is_enabled (app_filter);
}
#endif

static void
cc_user_page_dispose (GObject *object)
{
    CcUserPage *self = CC_USER_PAGE (object);

    g_clear_object (&self->login_screen_settings);

    G_OBJECT_CLASS (cc_user_page_parent_class)->dispose (object);
}

static void
cc_user_page_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    CcUserPage *self = CC_USER_PAGE (object);

    switch (prop_id) {
    case PROP_EDITABLE:
        g_value_set_boolean (value, self->editable);
        break;
    case PROP_LOCKED:
        g_value_set_boolean (value, self->locked);
        break;
    case PROP_IS_ADMIN:
        if (!self->user)
            g_value_set_boolean (value, FALSE);
        else
            g_value_set_boolean (value, act_user_get_account_type (self->user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);

        break;
    case PROP_IS_CURRENT_USER:
        g_value_set_boolean (value, is_current_user (self->user));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_user_page_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    CcUserPage *self = CC_USER_PAGE (object);

    switch (prop_id) {
    case PROP_EDITABLE:
        update_editable_state (self);
        break;
    case PROP_LOCKED:
        self->locked = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_user_page_class_init (CcUserPageClass * klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = cc_user_page_dispose;
    object_class->get_property = cc_user_page_get_property;
    object_class->set_property = cc_user_page_set_property;

    g_object_class_install_property (object_class,
                                     PROP_EDITABLE,
                                     g_param_spec_boolean ("editable",
                                                           "Editable",
                                                           "Whether the panel is editable",
                                                           FALSE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_LOCKED,
                                     g_param_spec_boolean ("locked",
                                                           "Locked",
                                                           "Whether changes require authentication",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_IS_ADMIN,
                                     g_param_spec_boolean ("is-admin",
                                                           "Is Admin",
                                                           "Whether the displayed user is administrator",
                                                           FALSE,
                                                           G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_IS_CURRENT_USER,
                                     g_param_spec_boolean ("is-current-user",
                                                           "Is Current User",
                                                           "Whether the displayed user is the current logged user",
                                                           FALSE,
                                                           G_PARAM_READABLE));
    g_type_ensure (CC_TYPE_LIST_ROW);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-user-page.ui");

    gtk_widget_class_bind_template_child (widget_class, CcUserPage, action_area);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, avatar);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, avatar_edit_button);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, avatar_remove_button);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, account_type_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, account_type_switch);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, auto_login_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, fingerprint_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, fullname_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, language_row);
#ifdef HAVE_MALCONTENT
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, parental_controls_row);
#endif
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, password_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, permission_infobar);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, remove_local_files_choice);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, remove_local_user_dialog);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, remove_user_button);

    gtk_widget_class_bind_template_callback (widget_class, account_type_changed);
    gtk_widget_class_bind_template_callback (widget_class, autologin_changed);
    gtk_widget_class_bind_template_callback (widget_class, change_fingerprint);
    gtk_widget_class_bind_template_callback (widget_class, change_language);
    gtk_widget_class_bind_template_callback (widget_class, change_password);
    gtk_widget_class_bind_template_callback (widget_class, fullname_entry_apply_cb);
    gtk_widget_class_bind_template_callback (widget_class, remove_local_user_response);
    gtk_widget_class_bind_template_callback (widget_class, remove_user);
    gtk_widget_class_bind_template_callback (widget_class, remove_avatar);
}

static void
cc_user_page_init (CcUserPage *self)
{
    g_autofree gchar *malcontent_control_path = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    self->avatar_chooser = cc_avatar_chooser_new ();
    gtk_menu_button_set_popover (self->avatar_edit_button, GTK_WIDGET (self->avatar_chooser));

#ifdef HAVE_MALCONTENT
    /* Parental Controls: Unavailable if user is admin or if
     * malcontent-control is not available (which can happen if
     * libmalcontent is installed but malcontent-control is not). */
    malcontent_control_path = g_find_program_in_path ("malcontent-control");
    if (malcontent_control_path)
        g_object_bind_property (self,
                                "is-admin",
                                self->parental_controls_row,
                                "visible",
                                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_signal_connect_object (self->parental_controls_row,
                             "activated",
                             G_CALLBACK (spawn_malcontent_control),
                             self,
                             G_CONNECT_SWAPPED);
#endif

    self->login_screen_settings = settings_or_null ("org.gnome.login-screen");
}

CcUserPage *
cc_user_page_new (void)
{
    return CC_USER_PAGE (g_object_new (CC_TYPE_USER_PAGE, NULL));
}

void
cc_user_page_set_user (CcUserPage  *self,
                       ActUser     *user,
                       GPermission *permission)
{
    gboolean is_admin = FALSE; 
    g_autofree gchar *user_language = NULL;

    g_assert (CC_IS_USER_PAGE (self));
    g_assert (ACT_IS_USER (user));

    g_clear_object (&self->user);
    self->user = g_object_ref (user);
    g_object_notify (G_OBJECT (self), "is-current-user");
    g_object_notify (G_OBJECT (self), "is-admin");

    if (!is_current_user (user))
      adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), get_real_or_user_name (user));
    adw_navigation_page_set_tag (ADW_NAVIGATION_PAGE (self), act_user_get_user_name (user));

    cc_avatar_chooser_set_user (self->avatar_chooser, self->user);
    setup_avatar_for_user (self->avatar, self->user);
    gtk_widget_set_visible (GTK_WIDGET (self->avatar_remove_button),
                            adw_avatar_get_custom_image (self->avatar) != NULL);


    gtk_editable_set_text (GTK_EDITABLE (self->fullname_row), act_user_get_real_name (user));

    gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), !would_demote_only_admin (user));
    is_admin = act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
    gtk_switch_set_active (self->account_type_switch, is_admin);

#ifdef HAVE_MALCONTENT
    cc_list_row_set_secondary_label (self->parental_controls_row,
                                     is_parental_controls_enabled_for_user (user) ?
                                     /* TRANSLATORS: Status of Parental Controls setup */
                                     _("Enabled") : _("Disabled"));
#endif

    g_signal_handlers_block_by_func (self->auto_login_row, autologin_changed, self);
    gtk_widget_set_visible (GTK_WIDGET (self->auto_login_row), get_autologin_possible (user));
    adw_switch_row_set_active (self->auto_login_row, act_user_get_automatic_login (user));
    g_signal_handlers_unblock_by_func (self->auto_login_row, autologin_changed, self);

    cc_list_row_set_secondary_label (self->password_row, get_password_mode_text (user));
    user_language = get_user_language (user);
    cc_list_row_set_secondary_label (self->language_row, user_language);

    if (!self->fingerprint_manager) {
        self->fingerprint_manager = cc_fingerprint_manager_new (user);
        g_signal_connect_object (self->fingerprint_manager,
                                 "notify::state",
                                 G_CALLBACK (update_fingerprint_row_state),
                                 self,
                                 G_CONNECT_SWAPPED);
        update_fingerprint_row_state (self, NULL, self->fingerprint_manager);
    }

    cc_permission_infobar_set_permission (self->permission_infobar, permission);
    cc_permission_infobar_set_title (self->permission_infobar, _("Some settings are locked"));
    g_object_bind_property (permission, "allowed", self, "locked", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_signal_connect_object (permission, "notify", G_CALLBACK (update_editable_state), self, G_CONNECT_SWAPPED);
    update_editable_state (self);
}

ActUser *
cc_user_page_get_user (CcUserPage *self)
{
    g_assert (CC_IS_USER_PAGE (self));
    g_assert (ACT_IS_USER (self->user));

    return self->user;
}
