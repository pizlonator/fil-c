/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
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
 * Author(s):
 *   Thomas Wood <thomas.wood@intel.com>
 *   Rodrigo Moya <rodrigo@gnome.org>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <math.h>
#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

#include "cc-list-row.h"
#include "cc-ua-macros.h"
#include "cc-ua-panel.h"
#include "cc-ua-hearing-page.h"
#include "cc-ua-mouse-page.h"
#include "cc-ua-seeing-page.h"
#include "cc-ua-typing-page.h"
#include "cc-ua-zoom-page.h"
#include "cc-ua-resources.h"

struct _CcUaPanel
{
  CcPanel    parent_instance;

  AdwNavigationView  *main_nav_view;
  AdwSwitchRow       *show_ua_menu_row;
  CcListRow          *seeing_row;
  CcListRow          *hearing_row;
  CcListRow          *typing_row;
  CcListRow          *mouse_row;
  CcListRow          *zoom_row;

  GSettings *a11y_settings;
};

CC_PANEL_REGISTER (CcUaPanel, cc_ua_panel)

static void
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanel *self = CC_UA_PANEL (object);

  g_clear_object (&self->a11y_settings);

  G_OBJECT_CLASS (cc_ua_panel_parent_class)->dispose (object);
}

static const char *
cc_ua_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/a11y";
}

static void
cc_ua_panel_class_init (CcUaPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_ua_panel_get_help_uri;

  object_class->dispose = cc_ua_panel_dispose;

  g_type_ensure (CC_TYPE_LIST_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-ua-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, main_nav_view);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, show_ua_menu_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, seeing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, hearing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, mouse_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, zoom_row);

  g_type_ensure (CC_TYPE_UA_SEEING_PAGE);
  g_type_ensure (CC_TYPE_UA_HEARING_PAGE);
  g_type_ensure (CC_TYPE_UA_TYPING_PAGE);
  g_type_ensure (CC_TYPE_UA_MOUSE_PAGE);
  g_type_ensure (CC_TYPE_UA_ZOOM_PAGE);
}

static void
on_subpage_set (CcUaPanel *self)
{
  AdwNavigationPage *subpage;
  g_autofree gchar *tag = NULL;

  g_object_get (self, "subpage", &tag, NULL);
  if (!tag)
    return;

  subpage = adw_navigation_view_find_page (self->main_nav_view, tag);
  if (subpage)
    adw_navigation_view_push (self->main_nav_view, subpage);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  g_resources_register (cc_universal_access_get_resource ());
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                                    "/org/gnome/control-center/universal-access/icons");

  gtk_widget_init_template (GTK_WIDGET (self));

  self->a11y_settings = g_settings_new (A11Y_SETTINGS);
  g_settings_bind (self->a11y_settings, KEY_ALWAYS_SHOW_STATUS,
                   self->show_ua_menu_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self, "notify::subpage", G_CALLBACK (on_subpage_set), self, G_CONNECT_SWAPPED);
}
