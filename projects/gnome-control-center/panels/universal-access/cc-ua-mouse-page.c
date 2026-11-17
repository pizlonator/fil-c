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
#define G_LOG_DOMAIN "cc-ua-mouse-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-ua-macros.h"
#include "cc-ua-mouse-page.h"

struct _CcUaMousePage
{
  AdwNavigationPage   parent_instance;

  AdwSwitchRow       *mouse_keys_row;
  AdwSwitchRow       *locate_pointer_row;
  GtkScale           *double_click_delay_scale;

  AdwExpanderRow     *secondary_click_row;
  GtkScale           *secondary_delay_scale;

  AdwExpanderRow     *hover_click_row;
  GtkScale           *hover_delay_scale;
  GtkScale           *motion_threshold_scale;

  GSettings          *kb_settings;
  GSettings          *mouse_settings;
  GSettings          *interface_settings;
  GSettings          *gds_mouse_settings;
};

G_DEFINE_TYPE (CcUaMousePage, cc_ua_mouse_page, ADW_TYPE_NAVIGATION_PAGE)

static void
cc_ua_mouse_page_dispose (GObject *object)
{
  CcUaMousePage *self = (CcUaMousePage *)object;

  g_clear_object (&self->gds_mouse_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->kb_settings);

  G_OBJECT_CLASS (cc_ua_mouse_page_parent_class)->dispose (object);
}

static void
cc_ua_mouse_page_class_init (CcUaMousePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_ua_mouse_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "universal-access/cc-ua-mouse-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, mouse_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, locate_pointer_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, double_click_delay_scale);

  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, secondary_click_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, secondary_delay_scale);

  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, hover_click_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, hover_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaMousePage, motion_threshold_scale);
}

static void
cc_ua_mouse_page_init (CcUaMousePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->gds_mouse_settings = g_settings_new (MOUSE_PERIPHERAL_SETTINGS);
  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->mouse_settings = g_settings_new (MOUSE_SETTINGS);
  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);

  g_settings_bind (self->kb_settings, KEY_MOUSEKEYS_ENABLED,
                   self->mouse_keys_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->interface_settings, KEY_LOCATE_POINTER,
                   self->locate_pointer_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->gds_mouse_settings, "double-click",
                   gtk_range_get_adjustment (GTK_RANGE (self->double_click_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Click Assist */
  /* Simulated Secondary Click */
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED,
                   self->secondary_click_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->secondary_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Hover Click */
  g_settings_bind (self->mouse_settings, KEY_DWELL_CLICK_ENABLED,
                   self->hover_click_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->mouse_settings, KEY_DWELL_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->hover_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->mouse_settings, KEY_DWELL_THRESHOLD,
                   gtk_range_get_adjustment (GTK_RANGE (self->motion_threshold_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
}
