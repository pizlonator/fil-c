/* gtp-dynamic-panel.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "gtp-dynamic-panel"

#include "gtp-dynamic-panel.h"

#include "shell/cc-application.h"
#include "shell/cc-shell-model.h"

struct _GtpDynamicPanel
{
  CcPanel parent;
};

G_DEFINE_TYPE (GtpDynamicPanel, gtp_dynamic_panel, CC_TYPE_PANEL)

/* Auxiliary methods */

static void
set_visibility (CcPanelVisibility visibility)
{
  GApplication *application;
  CcShellModel *model;

  application = g_application_get_default ();
  model = cc_application_get_model (CC_APPLICATION (application));

  cc_shell_model_set_panel_visibility (model, "dynamic-panel", visibility);
}

/* Callbacks */

static gboolean
show_panel_cb (gpointer data)
{
  g_debug ("Showing panel");

  set_visibility (CC_PANEL_VISIBLE);

  return G_SOURCE_REMOVE;
}

static void
on_button_clicked_cb (GtkButton       *button,
                      GtpDynamicPanel *self)
{
  g_debug ("Hiding panel");

  set_visibility (CC_PANEL_HIDDEN);
  g_timeout_add_seconds (3, show_panel_cb, self);
}

static void
gtp_dynamic_panel_class_init (GtpDynamicPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/tests/panels/gtp-dynamic-panel.ui");

  gtk_widget_class_bind_template_callback (widget_class, on_button_clicked_cb);
}

static void
gtp_dynamic_panel_init (GtpDynamicPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
