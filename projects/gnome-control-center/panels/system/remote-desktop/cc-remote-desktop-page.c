/*
 * Copyright 2023 Gotam Gorabh <gautamy672@gmail.com>
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
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-remote-desktop-page"

#include "cc-remote-desktop-page.h"
#include "cc-desktop-sharing-page.h"
#include "cc-remote-session-page.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

struct _CcRemoteDesktopPage {
  AdwNavigationPage parent_instance;

  CcDesktopSharingPage *desktop_sharing_page;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (CcRemoteDesktopPage, cc_remote_desktop_page, ADW_TYPE_NAVIGATION_PAGE)

static void
cc_remote_desktop_page_dispose (GObject *object)
{
  CcRemoteDesktopPage *self = (CcRemoteDesktopPage *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (cc_remote_desktop_page_parent_class)->dispose (object);
}

static void
cc_remote_desktop_page_class_init (CcRemoteDesktopPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_remote_desktop_page_dispose;

  g_type_ensure (CC_TYPE_DESKTOP_SHARING_PAGE);
  g_type_ensure (CC_TYPE_REMOTE_SESSION_PAGE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/remote-desktop/cc-remote-desktop-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, desktop_sharing_page);
}

static void
cc_remote_desktop_page_init (CcRemoteDesktopPage *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
}
