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
#define G_LOG_DOMAIN "cc-ua-zoom-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-ua-macros.h"
#include "cc-ua-zoom-page.h"

struct _CcUaZoomPage
{
  AdwNavigationPage   parent_instance;

  AdwSwitchRow       *desktop_zoom_row;
  AdwSpinRow         *magnify_factor_spin_row;
  AdwComboRow        *magnify_view_row;

  AdwSwitchRow       *magnify_outside_screen_row;
  AdwComboRow        *zoom_screen_area_row;
  AdwComboRow        *zoom_follow_behaviour_row;

  AdwSwitchRow       *crosshair_row;
  AdwSwitchRow       *crosshair_overlap_mouse_row;
  GtkScale           *crosshair_thickness_scale;
  GtkScale           *crosshair_length_scale;
  GtkColorButton     *crosshair_color_button;

  AdwSwitchRow       *color_inverted_row;
  GtkScale           *brightness_scale;
  GtkScale           *contrast_scale;
  GtkScale           *grayscale_scale;

  GSettings          *magnifier_settings;
  GSettings          *application_settings;

  gboolean            is_self_update;
};

G_DEFINE_TYPE (CcUaZoomPage, cc_ua_zoom_page, ADW_TYPE_NAVIGATION_PAGE)

static void
ua_zoom_magnifier_settings_changed_cb (CcUaZoomPage *self,
                                       char         *key)
{
  GSettings *settings;
  guint selected_index;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  self->is_self_update = TRUE;
  settings = self->magnifier_settings;

  if (!key || g_str_equal (key, "lens-mode"))
    {
      gboolean lens_mode;

      lens_mode = g_settings_get_boolean (settings, "lens-mode");

      if (lens_mode)
        selected_index = 0;
      else
        selected_index = 1;

      adw_combo_row_set_selected (self->magnify_view_row, selected_index);
      gtk_widget_set_sensitive (GTK_WIDGET (self->magnify_outside_screen_row), !lens_mode);
    }

  if (!key || g_str_equal (key, "mouse-tracking"))
    {
      g_autofree char *tracking = NULL;

      tracking = g_settings_get_string (settings, "mouse-tracking");

      if (g_strcmp0 (tracking, "proportional") == 0)
        selected_index = 0;
      else if (g_strcmp0 (tracking, "push") == 0)
        selected_index = 1;
      else
        selected_index = 2;

      adw_combo_row_set_selected (self->zoom_follow_behaviour_row, selected_index);
    }

  if (!key || g_str_equal (key, "screen-position"))
    {
      g_autofree char *position = NULL;

      position = g_settings_get_string (settings, "screen-position");

      if (g_strcmp0 (position, "top-half") == 0)
        selected_index = 1;
      else if (g_strcmp0 (position, "bottom-half") == 0)
        selected_index = 2;
      else if (g_strcmp0 (position, "left-half") == 0)
        selected_index = 3;
      else if (g_strcmp0 (position, "right-half") == 0)
        selected_index = 4;
      else
        selected_index = 0;

      adw_combo_row_set_selected (self->zoom_screen_area_row, selected_index);
    }

  if (!key || g_str_has_prefix (key, "cross-hairs-"))
    {
      g_autofree char *color = NULL;
      GdkRGBA rgba;

      color = g_settings_get_string (self->magnifier_settings, "cross-hairs-color");
      gdk_rgba_parse (&rgba, color);

      rgba.alpha = g_settings_get_double (self->magnifier_settings, "cross-hairs-opacity");
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->crosshair_color_button), &rgba);
    }

  if (!key || g_str_has_prefix (key, "brightness-"))
    {
      gdouble red, green, blue, value;

      red = g_settings_get_double (settings, "brightness-red");
      green = g_settings_get_double (settings, "brightness-green");
      blue = g_settings_get_double (settings, "brightness-blue");

      if (red == green && green == blue)
        value = red;
      else
        /* use NTSC conversion weights for reasonable average */
        value = 0.299 * red + 0.587 * green + 0.114 * blue;

      gtk_range_set_value (GTK_RANGE (self->brightness_scale), value);
    }

  if (!key || g_str_has_prefix (key, "contrast-"))
    {
      gdouble red, green, blue, value;

      red = g_settings_get_double (settings, "contrast-red");
      green = g_settings_get_double (settings, "contrast-green");
      blue = g_settings_get_double (settings, "contrast-blue");

      if (red == green && green == blue)
        value = red;
      else
        /* use NTSC conversion weights for reasonable average */
        value = 0.299 * red + 0.587 * green + 0.114 * blue;

      gtk_range_set_value (GTK_RANGE (self->contrast_scale), value);
    }

  self->is_self_update = FALSE;
}

static void
ua_zoom_magnify_postion_row_changed_cb (CcUaZoomPage *self)
{
  guint selected_index;
  gboolean is_lens;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  selected_index = adw_combo_row_get_selected (self->magnify_view_row);
  is_lens = selected_index == 0;
  gtk_widget_set_sensitive (GTK_WIDGET (self->magnify_outside_screen_row), !is_lens);

  if (!self->is_self_update)
    g_settings_set_boolean (self->magnifier_settings, "lens-mode", is_lens);
}

static void
ua_zoom_screen_area_row_changed_cb (CcUaZoomPage *self)
{
  const char *position;
  guint selected_index;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  if (self->is_self_update)
    return;

  selected_index = adw_combo_row_get_selected (self->zoom_screen_area_row);

  if (selected_index == 1)
    position = "top-half";
  else if (selected_index == 2)
    position = "bottom-half";
  else if (selected_index == 3)
    position = "left-half";
  else if (selected_index == 4)
    position = "right-half";
  else
    position = "full-screen";

  g_settings_set_string (self->magnifier_settings, "screen-position", position);
}

static void
ua_zoom_behaviour_row_changed_cb (CcUaZoomPage *self)
{
  const char *tracking;
  guint selected_index;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  if (self->is_self_update)
    return;

  selected_index = adw_combo_row_get_selected (self->zoom_follow_behaviour_row);

  if (selected_index == 0)
    tracking = "proportional";
  else if (selected_index == 1)
    tracking = "push";
  else
    tracking = "centered";

  g_settings_set_string (self->magnifier_settings, "mouse-tracking", tracking);
}

#define TO_HEX(x) (int) ((gdouble) x * 255.0)
static void
ua_zoom_crosshair_color_set_cb (CcUaZoomPage *self)
{
  g_autofree char *color = NULL;
  GdkRGBA rgba;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  if (self->is_self_update)
    return;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->crosshair_color_button), &rgba);
  color = g_strdup_printf ("#%02x%02x%02x",
                           TO_HEX (rgba.red),
                           TO_HEX (rgba.green),
                           TO_HEX (rgba.blue));

  g_settings_set_string (self->magnifier_settings, "cross-hairs-color", color);
  g_settings_set_double (self->magnifier_settings, "cross-hairs-opacity", rgba.alpha);
}

static void
ua_zoom_brightness_value_changed_cb (CcUaZoomPage *self)
{
  gdouble value;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  if (self->is_self_update)
    return;

  value = gtk_range_get_value (GTK_RANGE (self->brightness_scale));
  g_settings_set_double (self->magnifier_settings, "brightness-red", value);
  g_settings_set_double (self->magnifier_settings, "brightness-green", value);
  g_settings_set_double (self->magnifier_settings, "brightness-blue", value);
}

static void
ua_zoom_contrast_value_changed_cb (CcUaZoomPage *self)
{
  gdouble value;

  g_assert (CC_IS_UA_ZOOM_PAGE (self));

  if (self->is_self_update)
    return;

  value = gtk_range_get_value (GTK_RANGE (self->contrast_scale));
  g_settings_set_double (self->magnifier_settings, "contrast-red", value);
  g_settings_set_double (self->magnifier_settings, "contrast-green", value);
  g_settings_set_double (self->magnifier_settings, "contrast-blue", value);
}

static void
cc_ua_zoom_page_dispose (GObject *object)
{
  CcUaZoomPage *self = (CcUaZoomPage *)object;

  g_clear_object (&self->magnifier_settings);
  g_clear_object (&self->application_settings);

  G_OBJECT_CLASS (cc_ua_zoom_page_parent_class)->dispose (object);
}

static void
cc_ua_zoom_page_class_init (CcUaZoomPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_ua_zoom_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "universal-access/cc-ua-zoom-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, desktop_zoom_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, magnify_factor_spin_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, magnify_view_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, magnify_outside_screen_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, zoom_screen_area_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, zoom_follow_behaviour_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_overlap_mouse_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_thickness_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_length_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_length_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, crosshair_color_button);

  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, color_inverted_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, brightness_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, contrast_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaZoomPage, grayscale_scale);

  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_magnify_postion_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_screen_area_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_behaviour_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_crosshair_color_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_brightness_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_zoom_contrast_value_changed_cb);
}

static void
cc_ua_zoom_page_init (CcUaZoomPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->magnifier_settings = g_settings_new (A11Y_SETTINGS ".magnifier");
  self->application_settings = g_settings_new (APPLICATION_SETTINGS);

  g_settings_bind (self->application_settings, KEY_SCREEN_MAGNIFIER_ENABLED,
                   self->desktop_zoom_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->magnifier_settings, "mag-factor",
                   adw_spin_row_get_adjustment (self->magnify_factor_spin_row),
                   "value", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->magnifier_settings, "scroll-at-edges",
                   self->magnify_outside_screen_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Cross hairs */
  g_settings_bind (self->magnifier_settings, "show-cross-hairs",
                   self->crosshair_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->magnifier_settings, "cross-hairs-clip",
                   self->crosshair_overlap_mouse_row, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN | G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->magnifier_settings, "cross-hairs-thickness",
                   gtk_range_get_adjustment (GTK_RANGE (self->crosshair_thickness_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->magnifier_settings, "cross-hairs-length",
                   gtk_range_get_adjustment (GTK_RANGE (self->crosshair_length_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Cross hairs effects */
  g_settings_bind (self->magnifier_settings, "invert-lightness",
                   self->color_inverted_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->magnifier_settings, "color-saturation",
                   gtk_range_get_adjustment (GTK_RANGE (self->grayscale_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->magnifier_settings, "changed",
                           G_CALLBACK (ua_zoom_magnifier_settings_changed_cb),
                           self, G_CONNECT_SWAPPED);
  ua_zoom_magnifier_settings_changed_cb (self, NULL);
}
