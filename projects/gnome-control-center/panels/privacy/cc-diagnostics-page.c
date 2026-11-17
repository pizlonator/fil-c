/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "cc-diagnostics-page.h"
#include "cc-util.h"
#include "shell/cc-application.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcDiagnosticsPage
{
  AdwNavigationPage    parent_instance;

  AdwPreferencesGroup *diagnostics_group;
  AdwSwitchRow        *abrt_row;

  GSettings           *privacy_settings;
};

G_DEFINE_TYPE (CcDiagnosticsPage, cc_diagnostics_page, ADW_TYPE_NAVIGATION_PAGE)

/* Static init function */

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  CcDiagnosticsPage *self = CC_DIAGNOSTICS_PAGE (user_data);

  g_debug ("ABRT appeared");
  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  CcDiagnosticsPage *self = CC_DIAGNOSTICS_PAGE (user_data);

  g_debug ("ABRT vanished");
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
cc_diagnostics_page_finalize (GObject *object)
{
  CcDiagnosticsPage *self = CC_DIAGNOSTICS_PAGE (object);

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_diagnostics_page_parent_class)->finalize (object);
}

static void
cc_diagnostics_page_class_init (CcDiagnosticsPageClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_diagnostics_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-diagnostics-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPage, diagnostics_group);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPage, abrt_row);
}

static void
cc_diagnostics_page_init (CcDiagnosticsPage *self)
{
  g_autofree gchar *os_name = NULL;
  g_autofree gchar *url = NULL;
  g_autofree gchar *msg = NULL;
  g_autofree gchar *link = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                    "org.freedesktop.problems.daemon",
                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                    abrt_appeared_cb,
                    abrt_vanished_cb,
                    self,
                    NULL);

  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings, "report-technical-problems",
                   self->abrt_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  if (!os_name)
    os_name = g_strdup ("GNOME");
  url = g_get_os_info (G_OS_INFO_KEY_PRIVACY_POLICY_URL);
  if (!url)
    url = g_strdup ("http://www.gnome.org/privacy-policy");
  /* translators: Text used in link to privacy policy */
  link = g_strdup_printf ("<a href=\"%s\">%s</a>", url, _("Learn more"));
  /* translators: The first '%s' is the distributor's name, such as 'Fedora', the second '%s' is a link to the privacy policy */
  msg = g_strdup_printf (_("Sending reports of technical problems helps us improve %s. Reports "
                           "are sent anonymously and are scrubbed of personal data. %s"),
                         os_name, link);
  adw_preferences_group_set_description (self->diagnostics_group, msg);
}
