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
#define G_LOG_DOMAIN "cc-ua-typing-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gdesktop-enums.h>

#include "cc-ua-macros.h"
#include "cc-ua-typing-page.h"

struct _CcUaTypingPage
{
  AdwNavigationPage   parent_instance;

  AdwSwitchRow       *screen_kb_row;
  AdwSwitchRow       *shortcuts_by_kb_row;

  AdwSwitchRow       *cursor_blink_row;
  GtkScale           *blink_time_scale;
  AdwComboRow        *flash_type_row;

  AdwExpanderRow     *repeat_keys_row;
  GtkScale           *repeat_speed_scale;
  GtkScale           *repeat_delay_scale;

  AdwExpanderRow     *sticky_keys_row;
  AdwSwitchRow       *disable_sticky_keys_row;
  AdwSwitchRow       *beep_sticky_keys_row;

  AdwExpanderRow     *slow_keys_row;
  GtkScale           *slow_keys_delay_scale;
  AdwSwitchRow       *slow_keys_beep_row;
  AdwSwitchRow       *slow_keys_beep_accept_row;
  AdwSwitchRow       *slow_keys_beep_reject_row;

  AdwExpanderRow     *bounce_keys_row;
  GtkScale           *bounce_keys_delay_scale;
  AdwSwitchRow       *bounce_keys_beep_reject_row;

  GSettings          *application_settings;
  GSettings          *interface_settings;
  GSettings          *kb_desktop_settings;
  GSettings          *kb_settings;
};

G_DEFINE_TYPE (CcUaTypingPage, cc_ua_typing_page, ADW_TYPE_NAVIGATION_PAGE)

/*
 * In the UI We are showing the speed, but in the settings we
 * are storing the duration.  So we want to show the inverse
 * of the value.  But since GtkAdjustment doesn't allow
 * upper < lower, and GtkRange:inverted is not what we need,
 * use negative values for lower, upper bounds and invert sign
 * of them when read/write.
 */
static gboolean
get_inverted_mapping (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  const GVariantType *type;
  gint64 val;

  type = g_variant_get_type (variant);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    val = g_variant_get_int32 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    val = g_variant_get_uint32 (variant);
  else
    return FALSE;

  g_value_set_double (value, val * - 1);

  return TRUE;
}

static GVariant *
set_inverted_mapping (const GValue       *value,
                      const GVariantType *expected_type,
                      gpointer            user_data)
{
  int val;

  /* in UI, we show speed from slow to fast, but we store the time, not speed */
  val = (int)g_value_get_double (value);

  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT32))
    return g_variant_new_int32 (val * -1);

  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT32))
    return g_variant_new_uint32 (val * -1);

  return NULL;
}

static void
bind_scale_with_mapping (CcUaTypingPage *self,
                         GSettings      *settings,
                         const char     *settings_key,
                         GtkScale       *scale)
{
  GtkAdjustment *adj;

  adj = gtk_range_get_adjustment (GTK_RANGE (scale));

  g_settings_bind_with_mapping (settings, settings_key,
                                adj, "value",
                                G_SETTINGS_BIND_DEFAULT,
                                get_inverted_mapping,
                                set_inverted_mapping,
                                self, NULL);
}

static void
cc_ua_typing_page_dispose (GObject *object)
{
  CcUaTypingPage *self = (CcUaTypingPage *)object;

  g_clear_object (&self->application_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->kb_desktop_settings);
  g_clear_object (&self->kb_settings);

  G_OBJECT_CLASS (cc_ua_typing_page_parent_class)->dispose (object);
}

static void
cc_ua_typing_page_class_init (CcUaTypingPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_ua_typing_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "universal-access/cc-ua-typing-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, screen_kb_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, shortcuts_by_kb_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, cursor_blink_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, blink_time_scale);

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, repeat_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, repeat_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, repeat_delay_scale);

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, sticky_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, disable_sticky_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, beep_sticky_keys_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, slow_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, slow_keys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, slow_keys_beep_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, slow_keys_beep_accept_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, slow_keys_beep_reject_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, bounce_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, bounce_keys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaTypingPage, bounce_keys_beep_reject_row);
}

static void
cc_ua_typing_page_init (CcUaTypingPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->application_settings = g_settings_new (APPLICATION_SETTINGS);
  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->kb_desktop_settings = g_settings_new (KEYBOARD_DESKTOP_SETTINGS);
  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);

  g_settings_bind (self->application_settings, KEY_SCREEN_KEYBOARD_ENABLED,
                   self->screen_kb_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_KEYBOARD_TOGGLE,
                   self->shortcuts_by_kb_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Text Cursor */
  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING,
                   self->cursor_blink_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  bind_scale_with_mapping (self, self->interface_settings,
                           KEY_CURSOR_BLINKING_TIME,
                           self->blink_time_scale);

  /* Repeat keys */
  g_settings_bind (self->kb_desktop_settings, KEY_REPEAT_KEYS,
                   self->repeat_keys_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  bind_scale_with_mapping (self, self->kb_desktop_settings,
                           KEY_REPEAT_INTERVAL,
                           self->repeat_speed_scale);
  g_settings_bind (self->kb_desktop_settings, KEY_REPEAT_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->repeat_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Sticky keys */
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_ENABLED,
                   self->sticky_keys_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_TWO_KEY_OFF,
                   self->disable_sticky_keys_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_MODIFIER_BEEP,
                   self->beep_sticky_keys_row, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  /* Slow Keys */
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_ENABLED,
                   self->slow_keys_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->slow_keys_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_PRESS,
                   self->slow_keys_beep_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_ACCEPT,
                   self->slow_keys_beep_accept_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_REJECT,
                   self->slow_keys_beep_reject_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Bounce Keys */
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_ENABLED,
                   self->bounce_keys_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->bounce_keys_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_BEEP_REJECT,
                   self->bounce_keys_beep_reject_row, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
}
