/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 - 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

#include "cc-online-accounts-panel.h"
#include "cc-online-account-provider-row.h"
#include "cc-online-account-row.h"
#include "cc-online-accounts-resources.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

struct _CcOnlineAccountsPanel
{
  CcPanel parent_instance;

  GtkFrame      *accounts_frame;
  GtkListBox    *accounts_listbox;
  AdwBanner     *offline_banner;
  GtkListBox    *providers_listbox;

  GoaClient *client;
  GVariant *parameters;
};

CC_PANEL_REGISTER (CcOnlineAccountsPanel, cc_online_accounts_panel);

enum {
  PROP_0,
  PROP_PARAMETERS
};

/* Rows methods */

typedef void (*RowForAccountCallback) (CcOnlineAccountsPanel *self, GtkWidget *row, GList *other_rows);

static void
remove_row_for_account_cb (CcOnlineAccountsPanel *self,
                           GtkWidget             *row,
                           GList                 *other_rows)
{
  gtk_list_box_remove (self->accounts_listbox, row);
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), other_rows != NULL);
}

static void
modify_row_for_account (CcOnlineAccountsPanel *self,
                        GoaObject             *object,
                        RowForAccountCallback  callback)
{
  GtkWidget *child;
  GList *children = NULL;
  GList *l;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->accounts_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      children = g_list_prepend (children, child);
    }

  children = g_list_reverse (children);

  for (l = children; l != NULL; l = l->next)
    {
      GoaObject *row_object;

      row_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (l->data));
      if (row_object == object)
        {
          GtkWidget *row = GTK_WIDGET (l->data);

          children = g_list_remove_link (children, l);
          callback (self, row, children);
          g_list_free (l);
          break;
        }
    }

  g_list_free (children);
}

static void
show_account_cb (GoaProvider *provider,
                 GAsyncResult *result,
                 CcOnlineAccountsPanel *self)
{
  g_autoptr (GError) error = NULL;

  if (!goa_provider_show_account_finish (provider, result, &error))
    {
      if (!g_error_matches (error, GOA_ERROR, GOA_ERROR_DIALOG_DISMISSED) && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error showing account: %s", error->message);
    }
}

static void
show_account (CcOnlineAccountsPanel *self,
              GoaObject             *object)
{
  g_autoptr (GoaProvider) provider = NULL;
  GtkRoot *root;
  GoaAccount *account;
  const char *provider_type;

  /* Find the provider with a matching type */
  account = goa_object_peek_account (object);
  provider_type = goa_account_get_provider_type (account);
  provider = goa_provider_get_for_provider_type (provider_type);
  if (provider == NULL)
    {
      g_warning ("Error showing account: Unsupported provider");
      return;
    }

  root = gtk_widget_get_root (GTK_WIDGET (self));
  goa_provider_show_account (provider,
                             self->client,
                             object,
                             GTK_WINDOW (root),
                             cc_panel_get_cancellable (CC_PANEL (self)),
                             (GAsyncReadyCallback) show_account_cb,
                             self);
}

static void
create_account_cb (GoaProvider *provider,
                   GAsyncResult *result,
                   CcOnlineAccountsPanel *self)
{
  g_autoptr (GoaObject) object = NULL;
  g_autoptr (GError) error = NULL;

  object = goa_provider_add_account_finish (provider, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, GOA_ERROR, GOA_ERROR_DIALOG_DISMISSED) && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error creating account: %s", error->message);

      return;
    }

  show_account (self, object);
}

static void
create_account (CcOnlineAccountsPanel *self,
                GoaProvider *provider)
{
  GtkRoot *parent;

  g_return_if_fail (GOA_IS_PROVIDER (provider));

  parent = gtk_widget_get_root (GTK_WIDGET (self));
  goa_provider_add_account (provider,
                            self->client,
                            GTK_WINDOW (parent),
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            (GAsyncReadyCallback) create_account_cb,
                            self);
}

static void
select_account_by_id (CcOnlineAccountsPanel  *self,
                      const gchar            *account_id)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->accounts_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GoaAccount *account;
      GoaObject *row_object;

      row_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (child));
      account = goa_object_peek_account (row_object);

      if (g_strcmp0 (goa_account_get_id (account), account_id) == 0)
        {
          show_account (self, row_object);
          break;
        }
    }
}

static void
command_add (CcOnlineAccountsPanel *self,
             GVariant              *parameters)
{
  const gchar *provider_name = NULL;
  g_autoptr (GVariant) v = NULL;

  g_assert (self != NULL);
  g_assert (parameters != NULL);

  switch (g_variant_n_children (parameters))
    {
      case 2:
        g_variant_get_child (parameters, 1, "v", &v);
        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
          provider_name = g_variant_get_string (v, NULL);
        else
          g_warning ("Wrong type for the second argument (provider name) GVariant, expected 's' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        break;

      default:
        g_warning ("Unexpected parameters found, ignore request");
        return;
    }

  if (provider_name != NULL)
    {
      GtkWidget *child;
      GoaProvider *provider;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (self->providers_listbox));
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          const char *provider_type = NULL;

          provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (child));
          provider_type = goa_provider_get_provider_type (provider);

          if (g_strcmp0 (provider_type, provider_name) == 0)
            break;
        }

      if (child == NULL)
        {
          g_warning ("Unable to get a provider for type '%s'", provider_name);
          return;
        }

      create_account (self, provider);
    }
}

static void
load_custom_css (void)
{
  g_autoptr (GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/online-accounts/online-accounts.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/* Callbacks */

static int
goa_provider_priority (const char *provider_type)
{
  static const char *goa_priority[] = {
    "owncloud",     /* Nextcloud */
    "google",       /* Google */
    "windows_live", /* Microsoft Personal */
    "ms_graph",     /* Microsoft 365 */
    "exchange",     /* Microsoft Exchange */
    "imap_smtp",    /* Email (IMAP and SMTP) */
    "webdav",       /* Calendars, Contacts, Files (WebDAV) */
    "kerberos",     /* Enterprise Login (Kerberos) */
    "fedora",       /* Fedora */
  };

  for (size_t i = 0; i < G_N_ELEMENTS (goa_priority); i++)
    {
      if (g_str_equal (goa_priority[i], provider_type))
        return i;
    }

  /* New or unknown providers are sorted last */
  return G_N_ELEMENTS (goa_priority) + 1;
}

static int
sort_accounts_func (GtkListBoxRow *a,
                    GtkListBoxRow *b,
                    gpointer user_data)
{
  GoaAccount *a_account, *b_account;
  GoaObject *a_object, *b_object;
  const char *a_name, *b_name;

  a_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (a));
  a_account = goa_object_peek_account (a_object);
  a_name = goa_account_get_provider_type (a_account);

  b_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (b));
  b_account = goa_object_peek_account (b_object);
  b_name = goa_account_get_provider_type (b_account);

  return goa_provider_priority (a_name) - goa_provider_priority (b_name);
}

static int
sort_providers_func (GtkListBoxRow *a,
                     GtkListBoxRow *b,
                     gpointer user_data)
{
  GoaProvider *a_provider, *b_provider;
  const char *a_name, *b_name;

  a_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (a));
  a_name = goa_provider_get_provider_type (a_provider);

  b_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (b));
  b_name = goa_provider_get_provider_type (b_provider);

  return goa_provider_priority (a_name) - goa_provider_priority (b_name);
}

static void
add_account (CcOnlineAccountsPanel *self,
             GoaObject *object)
{
  CcOnlineAccountRow *row;

  row = cc_online_account_row_new (object);
  gtk_list_box_append (self->accounts_listbox, GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), TRUE);
}

static void
add_provider (CcOnlineAccountsPanel *self,
              GoaProvider *provider)
{
  CcOnlineAccountProviderRow *row;

  row = cc_online_account_provider_row_new (provider);
  gtk_list_box_append (self->providers_listbox, GTK_WIDGET (row));
}

static void
on_account_added_cb (CcOnlineAccountsPanel *self,
                     GoaObject             *object)
{
  add_account (self, object);
}

static void
on_account_removed_cb (CcOnlineAccountsPanel *self,
                       GoaObject             *object)
{
  modify_row_for_account (self, object, remove_row_for_account_cb);
}

static void
on_accounts_listbox_row_activated (CcOnlineAccountsPanel *self,
                                   GtkListBoxRow         *activated_row)
{
  GoaObject *object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (activated_row));

  show_account (self, object);
}

static void
on_provider_row_activated_cb (CcOnlineAccountsPanel *self,
                              GtkListBoxRow *activated_row)
{
  GoaProvider *provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (activated_row));

  create_account (self, provider);
}

static void
goa_provider_get_all_cb (GObject *object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  g_autoptr (CcOnlineAccountsPanel) self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autolist (GoaProvider) providers = NULL;
  g_autolist (GoaAccount) accounts = NULL;
  g_autoptr (GError) error = NULL;

  /* goa_provider_get_all() doesn't have a cancellable argument, so check if
   * the panel cancellable was triggered.
   */
  if (g_cancellable_is_cancelled (cc_panel_get_cancellable (CC_PANEL (self))))
    return;

  if (!goa_provider_get_all_finish (&providers, res, &error))
    {
      g_warning ("Error listing providers: %s", error->message);
      return;
    }

  for (const GList *iter = providers; iter != NULL; iter = iter->next)
    add_provider (self, GOA_PROVIDER (iter->data));

  /* Load existing accounts */
  accounts = goa_client_get_accounts (self->client);

  for (const GList *iter = accounts; iter != NULL; iter = iter->next)
    add_account (self, GOA_OBJECT (iter->data));

  g_signal_connect_swapped (self->client,
                            "account-added",
                            G_CALLBACK (on_account_added_cb),
                            self);

  g_signal_connect_swapped (self->client,
                            "account-removed",
                            G_CALLBACK (on_account_removed_cb),
                            self);

  /* With the client ready, check if we have a pending command */
  gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);

  if (self->parameters != NULL)
    {
      g_autoptr (GVariant) parameters = NULL;

      parameters = g_steal_pointer (&self->parameters);
      g_object_set (self, "parameters", parameters, NULL);
    }
}

static void
goa_client_new_cb (GObject *object,
                   GAsyncResult *res,
                   gpointer user_data)
{
  g_autoptr (CcOnlineAccountsPanel) self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autoptr (GError) error = NULL;

  self->client = goa_client_new_finish (res, &error);
  if (self->client == NULL)
    {
      g_warning ("Error connect to service: %s", error->message);
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      return;
    }

  goa_provider_get_all (goa_provider_get_all_cb, g_object_ref (self));
}

/* CcPanel overrides */

static const char *
cc_online_accounts_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

/* GObject overrides */

static void
cc_online_accounts_panel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (object);

  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters;
          g_autoptr (GVariant) v = NULL;
          const gchar *first_arg = NULL;

          parameters = g_value_get_variant (value);
          if (parameters == NULL)
            return;

          if (g_variant_n_children (parameters) > 0)
            {
                g_variant_get_child (parameters, 0, "v", &v);
                if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
                  first_arg = g_variant_get_string (v, NULL);
                else
                  g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                             (gchar *)g_variant_get_type (v));
            }

          /* Waiting for the client to load */
          if (self->client == NULL)
            {
              g_clear_pointer (&self->parameters, g_variant_unref);
              self->parameters = g_value_dup_variant (value);
            }
          else if (g_strcmp0 (first_arg, "add") == 0)
            {
              command_add (CC_ONLINE_ACCOUNTS_PANEL (object), parameters);
            }
          else if (first_arg != NULL)
            {
              select_account_by_id (CC_ONLINE_ACCOUNTS_PANEL (object), first_arg);
            }

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_online_accounts_panel_finalize (GObject *object)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (object);

  g_clear_object (&self->client);
  g_clear_pointer (&self->parameters, g_variant_unref);

  G_OBJECT_CLASS (cc_online_accounts_panel_parent_class)->finalize (object);
}

static void
cc_online_accounts_panel_class_init (CcOnlineAccountsPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_online_accounts_panel_get_help_uri;

  object_class->set_property = cc_online_accounts_panel_set_property;
  object_class->finalize = cc_online_accounts_panel_finalize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/cc-online-accounts-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_frame);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, offline_banner);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, providers_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_accounts_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated_cb);
}

static void
cc_online_accounts_panel_init (CcOnlineAccountsPanel *self)
{
  GNetworkMonitor *monitor;

  g_resources_register (cc_online_accounts_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->accounts_listbox,
                              sort_accounts_func,
                              self,
                              NULL);

  gtk_list_box_set_sort_func (self->providers_listbox,
                              sort_providers_func,
                              self,
                              NULL);

  monitor = g_network_monitor_get_default();
  g_object_bind_property (monitor,
                          "network-available",
                          self->offline_banner,
                          "revealed",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (monitor,
                          "network-available",
                          self->providers_listbox,
                          "sensitive",
                          G_BINDING_SYNC_CREATE);

  load_custom_css ();

  /* Disable the panel while we wait for the client */
  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
  goa_client_new (cc_panel_get_cancellable (CC_PANEL (self)),
                  goa_client_new_cb,
                  g_object_ref (self));
}
