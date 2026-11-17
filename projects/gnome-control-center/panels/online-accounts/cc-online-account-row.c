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

#include "cc-online-account-row.h"
#include "cc-online-accounts-resources.h"

struct _CcOnlineAccountRow
{
  AdwActionRow parent;

  GtkImage *icon_image;
  GtkImage *warning_image;

  GoaObject *object;
};

G_DEFINE_TYPE (CcOnlineAccountRow, cc_online_account_row, ADW_TYPE_ACTION_ROW)

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
cc_online_account_row_dispose (GObject *object)
{
  CcOnlineAccountRow *self = CC_ONLINE_ACCOUNT_ROW (object);

  g_clear_object (&self->object);

  G_OBJECT_CLASS (cc_online_account_row_parent_class)->dispose (object);
}

static void
cc_online_account_row_class_init (CcOnlineAccountRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_online_account_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/cc-online-account-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountRow, warning_image);
}

static void
cc_online_account_row_init (CcOnlineAccountRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcOnlineAccountRow *
cc_online_account_row_new (GoaObject *object)
{
  CcOnlineAccountRow *self;
  GoaAccount *account;
  g_autoptr(GIcon) gicon = NULL;
  g_autoptr(GError) error = NULL;

  self = g_object_new (CC_TYPE_ONLINE_ACCOUNT_ROW, NULL);

  self->object = g_object_ref (object);

  account = goa_object_peek_account (object);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 goa_account_get_provider_name (account));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self),
                               goa_account_get_presentation_identity (account));

  gicon = g_icon_new_for_string (goa_account_get_provider_icon (account), &error);
  if (error != NULL)
    {
      g_warning ("Error creating GIcon for account: %s (%s, %d)",
                 error->message,
                 g_quark_to_string (error->domain),
                 error->code);
    }
  else
    {
      gtk_image_set_from_gicon (self->icon_image, gicon);

      if (is_gicon_symbolic (GTK_WIDGET (self), gicon))
        {
          gtk_image_set_icon_size (self->icon_image, GTK_ICON_SIZE_NORMAL);
          gtk_widget_add_css_class (GTK_WIDGET (self->icon_image), "symbolic-circular");
        }
      else
        {
          gtk_image_set_icon_size (self->icon_image, GTK_ICON_SIZE_LARGE);
          gtk_widget_add_css_class (GTK_WIDGET (self->icon_image), "lowres-icon");
        }
    }

  g_object_bind_property (account, "attention-needed",
                          self->warning_image, "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  return self;
}

GoaObject *
cc_online_account_row_get_object (CcOnlineAccountRow *self)
{
  g_return_val_if_fail (CC_IS_ONLINE_ACCOUNT_ROW (self), NULL);
  return self->object;
}
