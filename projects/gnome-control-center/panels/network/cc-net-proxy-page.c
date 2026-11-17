/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-net-proxy-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "panels/common/cc-list-row.h"
#include "cc-net-proxy-page.h"

struct _CcNetProxyPage
{
  AdwNavigationPage    parent_instance;

  AdwComboRow         *proxy_type_row;

  GtkStack            *main_stack;
  AdwPreferencesGroup *automatic_view;
  GtkBox              *manual_view;

  /* Automatic view */
  GtkEntry            *proxy_url_entry;
  GtkEntry            *proxy_warning_label;

  /* Manual view */
  AdwEntryRow         *http_host_entry;
  GtkAdjustment       *http_port_adjustment;
  AdwEntryRow         *https_host_entry;
  GtkAdjustment       *https_port_adjustment;
  AdwEntryRow         *ftp_host_entry;
  GtkAdjustment       *ftp_port_adjustment;
  AdwEntryRow         *socks_host_entry;
  GtkAdjustment       *socks_port_adjustment;
  AdwEntryRow         *proxy_ignore_entry;

  GSettings           *settings;
  char                *state_text;

  gboolean             is_loading;
};

G_DEFINE_TYPE (CcNetProxyPage, cc_net_proxy_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum
{
  MODE_DISABLED,
  MODE_MANUAL,
  MODE_AUTOMATIC
} ProxyMode;

typedef enum
{
  ROW_AUTOMATIC,
  ROW_MANUAL
} RowValue;

enum {
  PROP_0,
  PROP_MODIFIED,
  PROP_STATE_TEXT,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
get_ignore_hosts (GValue   *value,
                  GVariant *variant,
                  gpointer  user_data)
{
  g_autofree const char **strv = NULL;

  strv = g_variant_get_strv (variant, NULL);
  g_value_take_string (value, g_strjoinv (", ", (char **)strv));

  return TRUE;
}

static GVariant *
set_ignore_hosts (const GValue       *value,
                  const GVariantType *expected_type,
                  gpointer            user_data)
{
  const char *sv;
  char **strv;
  g_autoptr(GPtrArray) str_array = NULL;
  guint i = 0;

  sv = g_value_get_string (value);
  strv = g_strsplit_set (sv, ", ", 0);

  /* Remove empty strings */
  str_array = g_ptr_array_new_take_null_terminated ((gpointer) strv, g_free);

  while (i < str_array->len)
    if (*(const char *) g_ptr_array_index (str_array, i) == '\0')
      g_ptr_array_remove_index (str_array, i);
    else
      i++;

  return g_variant_new_strv ((const char *const *) str_array->pdata, -1);
}

/*
 * Get the currently selected mode, which may not have saved
 * to settings yet.  This method will always return one of
 * %MODE_AUTOMATIC or %MODE_MANUAL, regardless of whether the
 * proxy is enabled or not.
 */
static ProxyMode
proxy_get_selected_mode (CcNetProxyPage *self)
{
  guint selected;

  g_assert (CC_IS_NET_PROXY_PAGE (self));

  selected = adw_combo_row_get_selected (self->proxy_type_row);

  if (selected == ROW_AUTOMATIC)
    return MODE_AUTOMATIC;

  if (selected == ROW_MANUAL)
    return MODE_MANUAL;

  g_assert_not_reached ();

  return -1;
}

/*
 * Get the current mode, which may not have saved
 * to settings yet
 */
static ProxyMode
proxy_get_current_mode (CcNetProxyPage *self)
{
  ProxyMode mode;

  g_assert (CC_IS_NET_PROXY_PAGE (self));

  /*
   * Disabled state is immediately applied on change.  So get
   * it from the settings as we don't store it locally
   */
  mode = g_settings_get_enum (self->settings, "mode");
  if (mode == MODE_DISABLED)
    return MODE_DISABLED;

  if (self->is_loading)
    return mode;

  return proxy_get_selected_mode (self);
}

static void
proxy_update_state_text (CcNetProxyPage *self)
{
  ProxyMode mode;

  g_assert (CC_IS_NET_PROXY_PAGE (self));

  mode = proxy_get_current_mode (self);

  if (mode == MODE_DISABLED)
    self->state_text = _("Off");
  else if (mode == MODE_AUTOMATIC)
    self->state_text = _("Automatic");
  else
    self->state_text = _("Manual");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE_TEXT]);
}

static void
cancel_clicked_cb (CcNetProxyPage *self)
{
  g_assert (CC_IS_NET_PROXY_PAGE (self));

  cc_net_proxy_page_cancel_changes (CC_NET_PROXY_PAGE (self));
}

static void
save_clicked_cb (CcNetProxyPage *self)
{
  g_assert (CC_IS_NET_PROXY_PAGE (self));

  cc_net_proxy_page_save_changes (CC_NET_PROXY_PAGE (self));
}


static void
proxy_configuration_changed_cb (CcNetProxyPage *self)
{
  GtkWidget *child;
  ProxyMode mode;

  g_assert (CC_IS_NET_PROXY_PAGE (self));

  if (adw_combo_row_get_selected (self->proxy_type_row) == ROW_AUTOMATIC)
    child = GTK_WIDGET (self->automatic_view);
  else
    child = GTK_WIDGET (self->manual_view);

  gtk_stack_set_visible_child (self->main_stack, child);

  if (self->is_loading)
    return;

  mode = proxy_get_current_mode (self);
  g_settings_set_enum (self->settings, "mode", mode);
}

static void
proxy_settings_has_unapplied_cb (CcNetProxyPage *self)
{
  g_assert (CC_IS_NET_PROXY_PAGE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED]);
}

static void
proxy_settings_changed_cb (CcNetProxyPage *self)
{
  g_autofree char *url = NULL;
  ProxyMode mode;

  url = g_settings_get_string (self->settings, "autoconfig-url");
  mode = proxy_get_current_mode (self);

  /* Show warning if autoconfig URL is not set */
  gtk_widget_set_visible (GTK_WIDGET (self->proxy_warning_label), !url || !*url);
  gtk_widget_set_sensitive (GTK_WIDGET (self->main_stack), mode != MODE_DISABLED);
  gtk_widget_set_sensitive (GTK_WIDGET (self->proxy_type_row), mode != MODE_DISABLED);

  if (mode == MODE_AUTOMATIC)
    adw_combo_row_set_selected (self->proxy_type_row, ROW_AUTOMATIC);
  else if (mode == MODE_MANUAL)
    adw_combo_row_set_selected (self->proxy_type_row, ROW_MANUAL);

  proxy_update_state_text (self);
}

static void
cc_net_proxy_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcNetProxyPage *self = (CcNetProxyPage *)object;

  switch (prop_id)
    {
    case PROP_MODIFIED:
      g_value_set_boolean (value, cc_net_proxy_page_has_modified (self));
      break;

    case PROP_STATE_TEXT:
      g_value_set_string (value, self->state_text);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, cc_net_proxy_page_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_net_proxy_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcNetProxyPage *self = (CcNetProxyPage *)object;

  switch (prop_id)
    {
    case PROP_MODIFIED:
    case PROP_STATE_TEXT:
      g_warning ("%s is not a writeable property", g_param_spec_get_name (pspec));
      break;

    case PROP_ENABLED:
      cc_net_proxy_page_set_enabled (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_net_proxy_page_finalize (GObject *object)
{
  CcNetProxyPage *self = (CcNetProxyPage *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (cc_net_proxy_page_parent_class)->finalize (object);
}

static void
cc_net_proxy_page_class_init (CcNetProxyPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_net_proxy_page_get_property;
  object_class->set_property = cc_net_proxy_page_set_property;
  object_class->finalize = cc_net_proxy_page_finalize;

  properties[PROP_STATE_TEXT] =
    g_param_spec_string ("state-text",
                         "Proxy state text",
                         "Human readable Proxy state text",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Proxy settings modified",
                          "Proxy settings modified",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "", "", FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/cc-net-proxy-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, proxy_type_row);

  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, automatic_view);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, manual_view);

  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, proxy_url_entry);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, proxy_warning_label);

  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, http_host_entry);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, http_port_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, https_host_entry);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, https_port_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, ftp_host_entry);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, ftp_port_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, socks_host_entry);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, socks_port_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcNetProxyPage, proxy_ignore_entry);

  gtk_widget_class_bind_template_callback (widget_class, proxy_configuration_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_clicked_cb);
}

static void
proxy_bind_settings (CcNetProxyPage *self,
                     const char     *type,
                     gpointer        url_entry,
                     gpointer        port_adjustment)
{
  g_autoptr(GSettings) settings = NULL;

  g_assert (type && *type);

  settings = g_settings_get_child (self->settings, type);
  g_settings_bind (settings, "host",
                   url_entry, "text",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "port",
                   port_adjustment, "value",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
cc_net_proxy_page_init (CcNetProxyPage *self)
{
  self->is_loading = TRUE;
  self->settings = g_settings_new ("org.gnome.system.proxy");

  gtk_widget_init_template (GTK_WIDGET (self));

  /* We should save the changes only when asked to */
  g_settings_delay (self->settings);
  g_signal_connect_object (self->settings, "notify::has-unapplied",
                           G_CALLBACK (proxy_settings_has_unapplied_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (proxy_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  proxy_settings_changed_cb (self);

  g_settings_bind (self->settings, "autoconfig-url",
                   self->proxy_url_entry, "text",
                   G_SETTINGS_BIND_DEFAULT);

  proxy_bind_settings (self, "http", self->http_host_entry, self->http_port_adjustment);
  proxy_bind_settings (self, "https", self->https_host_entry, self->https_port_adjustment);
  proxy_bind_settings (self, "ftp", self->ftp_host_entry, self->ftp_port_adjustment);
  proxy_bind_settings (self, "socks", self->socks_host_entry, self->socks_port_adjustment);

  g_settings_bind_with_mapping (self->settings, "ignore-hosts",
                                self->proxy_ignore_entry, "text",
                                G_SETTINGS_BIND_DEFAULT,
                                get_ignore_hosts, set_ignore_hosts,
                                NULL, NULL);

  proxy_update_state_text (self);

  self->is_loading = FALSE;
}

gboolean
cc_net_proxy_page_get_enabled (CcNetProxyPage *self)
{
  ProxyMode mode;

  g_return_val_if_fail (CC_IS_NET_PROXY_PAGE (self), FALSE);

  mode = proxy_get_current_mode (self);

  return mode != MODE_DISABLED;
}

void
cc_net_proxy_page_set_enabled (CcNetProxyPage *self,
                               gboolean        enable)
{
  ProxyMode mode;

  g_return_if_fail (CC_IS_NET_PROXY_PAGE (self));

  mode = g_settings_get_enum (self->settings, "mode");

  /*
   * Don't change if that's already the case to avoid marking
   * the settings as modified
   */
  if (enable && mode != MODE_DISABLED)
    return;

  if (!enable && mode == MODE_DISABLED)
    return;

  if (enable)
    mode = proxy_get_selected_mode (self);
  else
    mode = MODE_DISABLED;

  g_settings_set_enum (self->settings, "mode", mode);

  /* Apply changes immediately */
  cc_net_proxy_page_save_changes (self);
}

gboolean
cc_net_proxy_page_has_modified (CcNetProxyPage *self)
{
  g_return_val_if_fail (CC_IS_NET_PROXY_PAGE (self), FALSE);

  return g_settings_get_has_unapplied (self->settings);
}

void
cc_net_proxy_page_save_changes (CcNetProxyPage *self)
{
  g_return_if_fail (CC_IS_NET_PROXY_PAGE (self));

  g_settings_apply (self->settings);
}

void
cc_net_proxy_page_cancel_changes (CcNetProxyPage *self)
{
  g_return_if_fail (CC_IS_NET_PROXY_PAGE (self));

  g_settings_revert (self->settings);

  /* Update widgets from the stored settings, not from the UI values */
  self->is_loading = TRUE;
  proxy_settings_changed_cb (self);
  self->is_loading = FALSE;
}
