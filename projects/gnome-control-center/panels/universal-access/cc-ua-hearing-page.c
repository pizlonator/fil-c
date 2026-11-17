/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010 Intel, Inc
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
 * Author(s):
 *   Thomas Wood <thomas.wood@intel.com>
 *   Rodrigo Moya <rodrigo@gnome.org>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-ua-hearing-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gdesktop-enums.h>
#include <glib/gi18n-lib.h>

#include "shell/cc-shell.h"
#include "cc-ua-macros.h"
#include "cc-ua-hearing-page.h"

struct _CcUaHearingPage
{
  AdwNavigationPage   parent_instance;

  AdwSwitchRow       *overamplification_row;
  GtkLabel           *sound_settings_label;

  AdwSwitchRow       *visual_alerts_row;
  AdwComboRow        *flash_type_row;

  GSettings          *sound_settings;
  GSettings          *wm_settings;
};

G_DEFINE_TYPE (CcUaHearingPage, cc_ua_hearing_page, ADW_TYPE_NAVIGATION_PAGE)

static void
ua_hearing_flash_type_changed_cb (CcUaHearingPage *self)
{
  GDesktopVisualBellType type;

  type = g_settings_get_enum (self->wm_settings, KEY_VISUAL_BELL_TYPE);
  adw_combo_row_set_selected (self->flash_type_row, type);
}

static gboolean
ua_hearing_sound_settings_clicked_cb (CcUaHearingPage *self)
{
  g_autoptr(GError) error = NULL;
  CcShell *shell;
  CcPanel *panel;

  g_assert (CC_IS_UA_HEARING_PAGE (self));

  panel = (CcPanel *)gtk_widget_get_ancestor (GTK_WIDGET (self), CC_TYPE_PANEL);
  shell = cc_panel_get_shell (CC_PANEL (panel));
  if (!cc_shell_set_active_panel_from_id (shell, "sound", NULL, &error))
    g_warning ("Failed to activate 'sound' panel: %s", error->message);

  return TRUE;
}

static void
ua_hearing_flash_type_row_changed_cb (CcUaHearingPage *self)
{
  guint selected_index;

  g_assert (CC_IS_UA_HEARING_PAGE (self));

  selected_index = adw_combo_row_get_selected (self->flash_type_row);
  g_settings_set_enum (self->wm_settings, KEY_VISUAL_BELL_TYPE, selected_index);
}

static void
ua_hearing_test_flash_clicked_cb (CcUaHearingPage *self)
{
  GdkSurface *surface;
  GtkNative *native;

  g_assert (CC_IS_UA_HEARING_PAGE (self));

  native = gtk_widget_get_native (GTK_WIDGET (self));
  surface = gtk_native_get_surface (native);

  gdk_surface_beep (surface);
}

static void
cc_ua_hearing_page_dispose (GObject *object)
{
  CcUaHearingPage *self = (CcUaHearingPage *)object;

  g_clear_object (&self->sound_settings);
  g_clear_object (&self->wm_settings);

  G_OBJECT_CLASS (cc_ua_hearing_page_parent_class)->dispose (object);
}

static void
cc_ua_hearing_page_class_init (CcUaHearingPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_ua_hearing_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "universal-access/cc-ua-hearing-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaHearingPage, overamplification_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaHearingPage, sound_settings_label);

  gtk_widget_class_bind_template_child (widget_class, CcUaHearingPage, visual_alerts_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaHearingPage, flash_type_row);

  gtk_widget_class_bind_template_callback (widget_class, ua_hearing_sound_settings_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_hearing_flash_type_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_hearing_test_flash_clicked_cb);
}

static void
cc_ua_hearing_page_init (CcUaHearingPage *self)
{
  g_autofree gchar *sound_panel_link = NULL;
  g_autofree gchar *label = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Translators: This will be presented as the text of a link to the Sound panel */
  sound_panel_link = g_strdup_printf ("<a href='#'>%s</a>", C_("Sound panel name", "Sound"));
  /* Translators: %s is a link to the Sound panel with the label "Sound" */
  label = g_strdup_printf (_("System volume can be adjusted in %s settings"), sound_panel_link);

  gtk_label_set_label (self->sound_settings_label, label);

  self->sound_settings = g_settings_new (SOUND_SETTINGS);
  self->wm_settings = g_settings_new (WM_SETTINGS);

  g_settings_bind (self->sound_settings, KEY_SOUND_OVERAMPLIFY,
                   self->overamplification_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->wm_settings, KEY_VISUAL_BELL,
                   self->visual_alerts_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->wm_settings, "changed::" KEY_VISUAL_BELL_TYPE,
                           G_CALLBACK (ua_hearing_flash_type_changed_cb), self, G_CONNECT_SWAPPED);
  ua_hearing_flash_type_changed_cb (self);
}
