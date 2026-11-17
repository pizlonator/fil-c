/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2020 Canonical Ltd.
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
 */

#include <config.h>
#include <glib/gi18n.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include "cc-online-account-provider-row.h"
#include "cc-online-accounts-resources.h"

struct _CcOnlineAccountProviderRow
{
  AdwActionRow parent;

  GtkImage *icon_image;

  GoaProvider *provider;
};

G_DEFINE_TYPE (CcOnlineAccountProviderRow, cc_online_account_provider_row, ADW_TYPE_ACTION_ROW)

static const char *
_goa_provider_get_provider_title (GoaProvider *provider)
{
  const char *provider_type = NULL;

  g_assert (GOA_IS_PROVIDER (provider));

  /* The order here is the same used to sort accounts and providers in the UI,
   * The title, if present, should bump the provider name to subtitle.
   */
  struct
  {
    const char *provider;
    const char *title;
  } goa_metadata[] = {
    { "imap_smtp", N_("Email") },                     /* IMAP and SMTP */
    { "webdav", N_("Calendar, Contacts and Files") }, /* WebDAV */
    { "owncloud", NULL },                             /* Nextcloud */
    { "google", NULL },                               /* Google */
    { "ms_graph", NULL },                             /* Microsoft 365 */
    { "exchange", NULL },                             /* Microsoft Exchange */
    { "windows_live", NULL },                         /* Microsoft Personal */
    { "kerberos", N_("Enterprise Login") },           /* Enterprise Login (Kerberos) */
    { "fedora", NULL },                               /* Fedora */
  };

  provider_type = goa_provider_get_provider_type (provider);
  for (size_t i = 0; i < G_N_ELEMENTS (goa_metadata); i++)
    {
      if (g_str_equal (goa_metadata[i].provider, provider_type))
        return goa_metadata[i].title ? _(goa_metadata[i].title) : NULL;
    }

  return NULL;
}

static gboolean
is_gicon_symbolic (GtkWidget *widget,
                   GIcon     *icon)
{
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  GtkIconTheme *icon_theme;

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                   icon,
                                                   32,
                                                   gtk_widget_get_scale_factor (widget),
                                                   gtk_widget_get_direction (widget),
                                                   0);

  return icon_paintable && gtk_icon_paintable_is_symbolic (icon_paintable);
}

static void
cc_online_account_provider_row_dispose (GObject *object)
{
  CcOnlineAccountProviderRow *self = CC_ONLINE_ACCOUNT_PROVIDER_ROW (object);

  g_clear_object (&self->provider);

  G_OBJECT_CLASS (cc_online_account_provider_row_parent_class)->dispose (object);
}

static void
cc_online_account_provider_row_class_init (CcOnlineAccountProviderRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_online_account_provider_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/cc-online-account-provider-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountProviderRow, icon_image);
}

static void
cc_online_account_provider_row_init (CcOnlineAccountProviderRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcOnlineAccountProviderRow *
cc_online_account_provider_row_new (GoaProvider *provider)
{
  CcOnlineAccountProviderRow *self;
  g_autoptr(GIcon) icon = NULL;
  g_autofree gchar *name = NULL;
  const char *title = NULL;

  self = g_object_new (CC_TYPE_ONLINE_ACCOUNT_PROVIDER_ROW, NULL);

  if (provider == NULL)
    {
      icon = g_themed_icon_new_with_default_fallbacks ("goa-account");
      name = g_strdup (C_("Online Account", "Other"));
    }
  else
    {
      self->provider = g_object_ref (provider);
      icon = goa_provider_get_provider_icon (provider, NULL);
      name = goa_provider_get_provider_name (provider, NULL);
      title = _goa_provider_get_provider_title (provider);
    }

  gtk_image_set_from_gicon (self->icon_image, icon);
  if (is_gicon_symbolic (GTK_WIDGET (self), icon))
    {
      gtk_image_set_icon_size (self->icon_image, GTK_ICON_SIZE_NORMAL);
      gtk_widget_add_css_class (GTK_WIDGET (self->icon_image), "symbolic-circular");
    }
  else
    {
      gtk_image_set_icon_size (self->icon_image, GTK_ICON_SIZE_LARGE);
      gtk_widget_add_css_class (GTK_WIDGET (self->icon_image), "lowres-icon");
    }

  if (title != NULL)
    {
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (self), name);
    }
  else
    {
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), name);
    }

  return self;
}

GoaProvider *
cc_online_account_provider_row_get_provider (CcOnlineAccountProviderRow *self)
{
  g_return_val_if_fail (CC_IS_ONLINE_ACCOUNT_PROVIDER_ROW (self), NULL);
  return self->provider;
}
