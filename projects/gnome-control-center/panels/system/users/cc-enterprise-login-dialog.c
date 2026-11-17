/*
 * cc-enterprise-login-dialog.c
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
 *   Ondrej Holy <oholy@redhat.com>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-enterprise-login-dialog"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cc-enterprise-login-dialog.h"
#include "cc-entry-feedback.h"
#include "cc-realm-manager.h"
#include "cc-list-row.h"
#include "user-utils.h"

#include <adwaita.h>
#include <act/act.h>
#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define DOMAIN_CHECK_TIMEOUT 600

struct _CcEnterpriseLoginDialog
{
  AdwWindow            parent_instance;

  AdwNavigationView   *navigation;
  AdwNavigationPage   *offline_page;
  AdwNavigationPage   *main_page;
  AdwNavigationPage   *main_preferences_page;
  AdwNavigationPage   *enroll_page;
  AdwPreferencesPage  *enroll_preferences_page;
  AdwToastOverlay     *toast_overlay;

  GtkButton           *add_button;
  GtkSpinner          *main_page_spinner;
  AdwEntryRow         *domain_row;
  CcEntryFeedback     *domain_feedback;
  gint                 domain_timeout_id;
  AdwEntryRow         *username_row;
  AdwPasswordEntryRow *password_row;

  GtkButton           *enroll_button;
  GtkSpinner          *enroll_page_spinner;
  AdwEntryRow         *admin_name_row;
  AdwPasswordEntryRow *admin_password_row;

  CcRealmManager      *realm_manager;
  CcRealmObject       *selected_realm;
  GNetworkMonitor     *network_monitor;

  GCancellable        *cancellable;
};

G_DEFINE_TYPE (CcEnterpriseLoginDialog, cc_enterprise_login_dialog, ADW_TYPE_WINDOW)

static gboolean
add_button_is_valid (CcEnterpriseLoginDialog *self)
{
  return (self->selected_realm != NULL) &&
         strlen (gtk_editable_get_text (GTK_EDITABLE (self->username_row))) > 0 &&
         strlen (gtk_editable_get_text (GTK_EDITABLE (self->password_row))) > 0;
}

static gboolean
enroll_button_is_valid (CcEnterpriseLoginDialog *self)
{
  return strlen (gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row))) > 0 &&
         strlen (gtk_editable_get_text (GTK_EDITABLE (self->admin_password_row))) > 0;
}

static void
show_operation_progress (CcEnterpriseLoginDialog *self,
                         gboolean                 show)
{
  gtk_widget_set_visible (GTK_WIDGET (self->main_page_spinner), show);
  gtk_widget_set_visible (GTK_WIDGET (self->enroll_page_spinner), show);

  gtk_widget_set_sensitive (GTK_WIDGET (self->main_preferences_page), !show);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), !show && add_button_is_valid (self));
  gtk_widget_set_sensitive (GTK_WIDGET (self->enroll_preferences_page), !show);
  gtk_widget_set_sensitive (GTK_WIDGET (self->enroll_button), !show && enroll_button_is_valid (self));

  /* Hide passwords during operations. */
  if (show)
    {
      GtkEditable *delegate;

      delegate = gtk_editable_get_delegate (GTK_EDITABLE (self->password_row));
      gtk_text_set_visibility (GTK_TEXT (delegate), FALSE);

      delegate = gtk_editable_get_delegate (GTK_EDITABLE (self->admin_password_row));
      gtk_text_set_visibility (GTK_TEXT (delegate), FALSE);
    }
}

static void
cache_user_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr (ActUser) user = NULL;
  g_autoptr(GError) error = NULL;

  user = act_user_manager_cache_user_finish (ACT_USER_MANAGER (source), result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* This is where we're finally done */
  if (user != NULL)
    {
      g_debug ("Successfully cached remote user: %s", act_user_get_user_name (user));

      gtk_window_close (GTK_WINDOW (self));
    }
  else
    {
      g_message ("Couldn't cache user account: %s", error->message);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Failed to register account")));
      show_operation_progress (self, FALSE);
    }
}

static void
change_login_policy_cb (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;

  cc_realm_common_call_change_login_policy_finish (CC_REALM_COMMON (source), result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error == NULL)
    {
      ActUserManager *manager;
      g_autofree gchar *login = NULL;

      /*
       * Now tell the account service about this user. The account service
       * should also lookup information about this via the realm and make
       * sure all that is functional.
       */
      manager = act_user_manager_get_default ();
      login = cc_realm_calculate_login (CC_REALM_COMMON (source), gtk_editable_get_text (GTK_EDITABLE (self->username_row)));

      g_debug ("Caching remote user: %s", login);

      act_user_manager_cache_user_async (manager, login, self->cancellable, cache_user_cb, g_object_ref (self));
    }
  else
    {
      g_message ("Couldn't permit logins on account: %s", error->message);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Failed to register account")));
      show_operation_progress (self, FALSE);
   }
}

static void
permit_user_login (CcEnterpriseLoginDialog *self)
{
  g_autoptr(CcRealmCommon) common = NULL;

  common = cc_realm_object_get_common (self->selected_realm);
  if (common == NULL)
    {
      g_debug ("Failed to register account: failed to get d-bus interface");

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Failed to register account")));
      show_operation_progress (self, FALSE);
    }
  else
    {
      g_autofree gchar *login = NULL;
      const gchar *add[2];
      const gchar *remove[1];
      GVariant *options;

      login = cc_realm_calculate_login (common, gtk_editable_get_text (GTK_EDITABLE (self->username_row)));

      g_debug ("Permitting login for: %s", login);

      add[0] = login;
      add[1] = NULL;
      remove[0] = NULL;

      options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

      cc_realm_common_call_change_login_policy (common, "",
                                                add, remove, options,
                                                self->cancellable,
                                                change_login_policy_cb,
                                                g_object_ref (self));
    }
}

static void
realm_join_as_admin_cb (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;
  const gchar *message = NULL;

  cc_realm_join_finish (CC_REALM_OBJECT (source), result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* Yay, joined the domain, register the user locally */
  if (error == NULL)
    {
      g_debug ("Joining realm completed successfully");

      permit_user_login (self);
      return;
    }
  /* Other failure */
  else
    {
      g_message ("Failed to join the domain: %s", error->message);

      message = _("Failed to join domain");
    }

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (message));
  show_operation_progress (self, FALSE);
}

static void
realm_login_as_admin_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) creds = NULL;
  const gchar *message = NULL;

  creds = cc_realm_login_finish (result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* Logged in as admin successfully, use creds to join domain */
  if (creds != NULL)
    {
      if (cc_realm_join_as_admin (self->selected_realm,
                                  gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row)),
                                  gtk_editable_get_text (GTK_EDITABLE (self->admin_password_row)),
                                  creds, self->cancellable,
                                  realm_join_as_admin_cb, g_object_ref (self)))
        {
          return;
        }

      g_message ("Authenticating as admin is not supported by the realm");

      message = _("No supported way to authenticate with this domain");
    }

  show_operation_progress (self, FALSE);

  /* A problem with the admin's login name or password */
  if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN))
    {
      g_debug ("Bad admin login: %s", error->message);

      message = _("That login name didn’t work");
      gtk_widget_grab_focus (GTK_WIDGET (self->admin_name_row));
    }
  else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_PASSWORD))
    {
      g_debug ("Bad admin password: %s", error->message);

      message = _("That login password didn’t work");
      gtk_widget_grab_focus (GTK_WIDGET (self->admin_password_row));
    }
  else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_HOSTNAME))
    {
      g_debug ("Bad host name: %s", error->message);

      message = _("That hostname didn’t work");
    }
  /* Other login failure */
  else
    {
      g_message ("Admin login failure: %s", error->message);

      message = _("Failed to log into domain");
    }

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (message));
}

static void
on_enroll_button_clicked_cb (CcEnterpriseLoginDialog *self)
{
  g_debug ("Logging in as admin user: %s", gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row)));

  show_operation_progress (self, TRUE);

  /* Prompted for some admin credentials, try to use them to log in */
  cc_realm_login (self->selected_realm,
                  gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row)),
                  gtk_editable_get_text (GTK_EDITABLE (self->admin_password_row)),
                  self->cancellable,
                  realm_login_as_admin_cb,
                  g_object_ref (self));
}

static void
enroll_page_validate (CcEnterpriseLoginDialog *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->enroll_button), enroll_button_is_valid (self));
}

static void
clear_enroll_page (CcEnterpriseLoginDialog *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->admin_name_row), "");
  gtk_editable_set_text (GTK_EDITABLE (self->admin_password_row), "");
}

static void
show_enroll_page (CcEnterpriseLoginDialog *self)
{
  const gchar *domain;
  g_autofree gchar *description = NULL;

  domain = gtk_editable_get_text (GTK_EDITABLE (self->domain_row));

  /* Translators: The "%s" is a domain address (e.g. "demo1.freeipa.org"). */
  description = g_strdup_printf (_("To add an enterprise login account, this device needs to be enrolled with <b>%s</b>. "
                                   "To enroll, have your domain administrator enter their name and password."),
                                 domain);
  adw_preferences_page_set_description (self->enroll_preferences_page, description);

  if (strlen (gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row))) == 0)
    {
      g_autoptr(CcRealmKerberosMembership) membership = NULL;
      g_autoptr(CcRealmKerberos) kerberos = NULL;
      const gchar *name;

      kerberos = cc_realm_object_get_kerberos (self->selected_realm);
      membership = cc_realm_object_get_kerberos_membership (self->selected_realm);
      name = cc_realm_kerberos_membership_get_suggested_administrator (membership);
      if (name != NULL && !g_str_equal (name, ""))
        {
          g_debug ("Suggesting admin user: %s", name);

          gtk_editable_set_text (GTK_EDITABLE (self->admin_name_row), name);
        }
    }

  adw_navigation_view_push (self->navigation, self->enroll_page);
}

static void
realm_join_as_user_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;

  cc_realm_join_finish (CC_REALM_OBJECT (source), result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* Yay, joined the domain, register the user locally */
  if (error == NULL)
    {
      g_debug ("Joining realm completed successfully");

      permit_user_login (self);
      return;
    }
  /* Credential failure while joining domain, prompt for admin creds */
  else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN) ||
           g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_PASSWORD) ||
           g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_HOSTNAME))
    {
      g_debug ("Joining realm failed due to credentials");

      show_enroll_page (self);
    }
  /* Other failure */
  else
    {
      g_message ("Failed to join the domain: %s", error->message);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Failed to join domain")));
    }

  show_operation_progress (self, FALSE);
}

static void
realm_login_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) creds = NULL;
  const gchar *message = NULL;

  creds = cc_realm_login_finish (result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /*
   * User login is valid, but cannot authenticate right now (eg: user needs
   * to change password at next login etc.)
   */
  if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_CANNOT_AUTH))
    g_clear_error (&error);

  if (error == NULL)
    {
      /* Already joined to the domain, just register this user */
      if (cc_realm_is_configured (self->selected_realm))
        {
          g_debug ("Already joined to this realm");

          permit_user_login (self);
        }
      /* Join the domain, try using the user's creds */
      else if (creds != NULL &&
               cc_realm_join_as_user (self->selected_realm,
                                      gtk_editable_get_text (GTK_EDITABLE (self->username_row)),
                                      gtk_editable_get_text (GTK_EDITABLE (self->password_row)),
                                      creds, self->cancellable,
                                      realm_join_as_user_cb, g_object_ref (self)))
        {
          return;
        }
      /* If we can't do user auth, try to authenticate as admin */
      else
        {
          g_debug ("Cannot join with user credentials");

          show_enroll_page (self);
          show_operation_progress (self, FALSE);
        }

      return;
    }

  show_operation_progress (self, FALSE);

  /* A problem with the user's login name or password */
  if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN))
    {
      g_debug ("Problem with the user's login: %s", error->message);

      message = _("That login name didn’t work");
      gtk_widget_grab_focus (GTK_WIDGET (self->username_row));
    }
  else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_PASSWORD))
    {
      g_debug ("Problem with the user's password: %s", error->message);

      message = _("That login password didn’t work");
      gtk_widget_grab_focus (GTK_WIDGET (self->password_row));
    }
  /* Other login failure */
  else
    {
      g_message ("Couldn't log in as user: %s", error->message);

      message = _("Failed to log into domain");
      gtk_widget_grab_focus (GTK_WIDGET (self->domain_row));
    }

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (message));
}

static void
on_add_button_clicked_cb (CcEnterpriseLoginDialog *self)
{
  show_operation_progress (self, TRUE);

  cc_realm_login (self->selected_realm,
                  gtk_editable_get_text (GTK_EDITABLE (self->username_row)),
                  gtk_editable_get_text (GTK_EDITABLE (self->password_row)),
                  self->cancellable,
                  realm_login_cb,
                  g_object_ref (self));
}

static void
main_page_validate (CcEnterpriseLoginDialog *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), add_button_is_valid (self));
}

static void
realm_manager_discover_cb (GObject       *source,
                           GAsyncResult  *result,
                           gpointer       user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;
  GList *realms;

  realms = cc_realm_manager_discover_finish (self->realm_manager, result, NULL);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* Found a realm, log user into domain */
  if (realms != NULL)
    {
      self->selected_realm = g_object_ref (realms->data);

      cc_entry_feedback_update (self->domain_feedback, "emblem-default-symbolic", _("Valid domain"));
    }
  else
    {
      cc_entry_feedback_update (self->domain_feedback, "dialog-warning-symbolic", _("Domain not found"));
    }

  main_page_validate (self);

  g_list_free_full (realms, g_object_unref);
}

static void
domain_validate (CcEnterpriseLoginDialog *self)
{
  self->domain_timeout_id = 0;

  /* This is needed to stop previous calls to avoid rewriting feedback. */
  g_cancellable_cancel (self->cancellable);
  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  cc_realm_manager_discover (self->realm_manager,
                             gtk_editable_get_text (GTK_EDITABLE (self->domain_row)),
                             self->cancellable,
                             realm_manager_discover_cb,
                             g_object_ref (self));
}

static void
on_domain_entry_changed_cb (CcEnterpriseLoginDialog *self)
{
  const gchar *domain;

  if (self->realm_manager == NULL)
    return;

  g_clear_handle_id (&self->domain_timeout_id, g_source_remove);
  clear_enroll_page (self);

  domain = gtk_editable_get_text (GTK_EDITABLE (self->domain_row));
  if (strlen (domain) == 0)
    {
      cc_entry_feedback_reset (self->domain_feedback);
      return;
    }

  cc_entry_feedback_update (self->domain_feedback, "process-working-symbolic", _("Checking domain…"));
  self->domain_timeout_id = g_timeout_add (DOMAIN_CHECK_TIMEOUT, (GSourceFunc)domain_validate, self);
}

static void
check_network_availability (CcEnterpriseLoginDialog *self)
{
  if (!g_network_monitor_get_network_available (self->network_monitor))
    {
      adw_navigation_view_pop_to_page (self->navigation, self->offline_page);
    }
  else
    {
      if (adw_navigation_view_get_visible_page (self->navigation) != self->main_page)
        adw_navigation_view_push (self->navigation, self->main_page);
    }
}

static void
on_network_changed_cb (GNetworkMonitor *monitor,
                       gboolean         available,
                       gpointer         user_data)
{
  CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);

  check_network_availability (self);
}

static void
realm_manager_new_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
  g_autoptr(GError) error = NULL;

  self->realm_manager = cc_realm_manager_new_finish (result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error != NULL)
    {
      g_warning ("Couldn't contact realmd service: %s", error->message);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Failed to contact realmd service")));
    }
  else
    {
      show_operation_progress (self, FALSE);
    }
}

static void
cc_enterprise_login_dialog_dispose (GObject *object)
{
  CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (object);

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->realm_manager);

  g_signal_handlers_disconnect_by_data (self->network_monitor, self);

  g_clear_handle_id (&self->domain_timeout_id, g_source_remove);

  G_OBJECT_CLASS (cc_enterprise_login_dialog_parent_class)->dispose (object);
}

static void
cc_enterprise_login_dialog_init (CcEnterpriseLoginDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  self->network_monitor = g_network_monitor_get_default ();
  g_signal_connect_object (self->network_monitor, "network-changed", G_CALLBACK (on_network_changed_cb), self, 0);
  check_network_availability (self);

  show_operation_progress (self, TRUE);
  cc_realm_manager_new (self->cancellable, realm_manager_new_cb, g_object_ref (self));
}

static void
cc_enterprise_login_dialog_class_init (CcEnterpriseLoginDialogClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_enterprise_login_dialog_dispose;

  g_type_ensure (CC_TYPE_ENTRY_FEEDBACK);
  g_type_ensure (CC_TYPE_LIST_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-enterprise-login-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, add_button);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, admin_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, admin_password_row);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, domain_feedback);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, domain_row);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enroll_button);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enroll_page);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enroll_page_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enroll_preferences_page);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, main_page);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, main_page_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, main_preferences_page);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, navigation);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, offline_page);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, password_row);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, username_row);

  gtk_widget_class_bind_template_callback (widget_class, enroll_page_validate);
  gtk_widget_class_bind_template_callback (widget_class, main_page_validate);
  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_domain_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_enroll_button_clicked_cb);
}

CcEnterpriseLoginDialog *
cc_enterprise_login_dialog_new (void)
{
  return g_object_new (CC_TYPE_ENTERPRISE_LOGIN_DIALOG, NULL);
}
