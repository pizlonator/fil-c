/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 *          Ondrej Holy <oholy@redhat.com>
 *
 */

#include <gdesktop-enums.h>
#include <gtk/gtk.h>

#include "cc-illustrated-row.h"
#include "cc-split-row.h"
#include "cc-list-row-info-button.h"

#include "cc-mouse-caps-helper.h"
#include "cc-mouse-panel.h"
#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"
#include "gsd-device-manager.h"
#include "gsd-input-helper.h"

struct _CcMousePanel
{
  CcPanel            parent_instance;

  GtkSwitch         *mouse_accel_switch;
  AdwPreferencesGroup *mouse_group;
  CcSplitRow        *mouse_scroll_direction_row;
  GtkScale          *mouse_speed_scale;
  GtkWindow         *mouse_test;
  GtkBox            *primary_button_box;
  GtkToggleButton   *primary_button_left;
  GtkToggleButton   *primary_button_right;
  AdwPreferencesPage*preferences;
  CcSplitRow        *two_finger_push_row;
  GtkStack          *title_stack;
  CcIllustratedRow  *tap_to_click_row;
  GtkSwitch         *tap_to_click_switch;
  AdwPreferencesGroup *touchpad_group;
  AdwViewStackPage  *touchpad_stack_page;
  CcSplitRow        *touchpad_scroll_direction_row;
  CcSplitRow        *touchpad_scroll_method_row;
  GtkListBoxRow     *touchpad_speed_row;
  GtkScale          *touchpad_speed_scale;
  AdwSwitchRow      *touchpad_toggle_row;
  AdwSwitchRow      *touchpad_typing_row;

  GSettings         *mouse_settings;
  GSettings         *touchpad_settings;

  gboolean           have_mouse;
  gboolean           have_touchpad;
  gboolean           have_touchscreen;
  gboolean           have_synaptics;

  gboolean           left_handed;
  GtkGesture        *left_gesture;
  GtkGesture        *right_gesture;
};

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

#define ASSET_RESOURCES_PREFIX "/org/gnome/control-center/mouse/assets/"

static void
setup_illustrations (CcMousePanel *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  gboolean use_dark = adw_style_manager_get_dark (style_manager);
  struct {
    CcSplitRow *row;
    const gchar *default_resource;
    const gchar *alternative_resource;
  } row_resources[] = {
    { self->mouse_scroll_direction_row, "scroll-traditional", "scroll-natural" },
    { self->two_finger_push_row, "push-to-click-anywhere", "push-areas" },
    { self->touchpad_scroll_method_row, "scroll-2finger", "edge-scroll" },
    { self->touchpad_scroll_direction_row, "touch-scroll-traditional", "touch-scroll-natural" },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (row_resources); i++)
    {
      g_autofree gchar *alternative_resource = NULL;
      g_autofree gchar *default_resource = NULL;
      const gchar *style_suffix;

      style_suffix = use_dark ? "d" : "l";
      default_resource = g_strdup_printf (ASSET_RESOURCES_PREFIX "%s-%s.webm",
                                          row_resources[i].default_resource,
                                          style_suffix);
      alternative_resource = g_strdup_printf (ASSET_RESOURCES_PREFIX "%s-%s.webm",
                                              row_resources[i].alternative_resource,
                                              style_suffix);

      cc_split_row_set_default_illustration_resource (row_resources[i].row, default_resource);
      cc_split_row_set_alternative_illustration_resource (row_resources[i].row, alternative_resource);
    }

  /* Tap to click */
  {
    g_autofree gchar *resource = NULL;

    resource = g_strdup_printf (ASSET_RESOURCES_PREFIX "%s-%s.webm",
                                "tap-to-click",
                                use_dark ? "d" : "l");

    cc_illustrated_row_set_resource (self->tap_to_click_row, resource);
  }
}

static void
setup_touchpad_options (CcMousePanel *self)
{
  gboolean have_two_finger_scrolling;
  gboolean have_edge_scrolling;
  gboolean have_tap_to_click;

  if (self->have_synaptics || !self->have_touchpad) {
    adw_view_stack_page_set_visible (self->touchpad_stack_page, FALSE);
    gtk_stack_set_visible_child_name (self->title_stack, "title");
    return;
  }

  cc_touchpad_check_capabilities (&have_two_finger_scrolling, &have_edge_scrolling, &have_tap_to_click);

  adw_view_stack_page_set_visible (self->touchpad_stack_page, TRUE);
  gtk_stack_set_visible_child_name (self->title_stack, "switcher");

  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_scroll_method_row), have_two_finger_scrolling);
  gtk_widget_set_visible (GTK_WIDGET (self->tap_to_click_row), have_tap_to_click);
}

static void
on_touchpad_scroll_method_changed_cb (CcMousePanel *self)
{
  gboolean two_finger;

  two_finger = cc_split_row_get_use_default (self->touchpad_scroll_method_row);

  if (g_settings_get_boolean (self->touchpad_settings,
                              "two-finger-scrolling-enabled") == two_finger)
    return;

  g_settings_set_boolean (self->touchpad_settings, "two-finger-scrolling-enabled", two_finger);
}

static gboolean
get_touchpad_enabled (GSettings *settings)
{
  GDesktopDeviceSendEvents send_events;

  send_events = g_settings_get_enum (settings, "send-events");

  return send_events == G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED;
}

static gboolean
can_disable_touchpad (CcMousePanel *self)
{
  if (!self->have_touchpad)
    return FALSE;

  g_debug ("Should we show the row to enable touchpad: have_mouse: %s have_touchscreen: %s\n",
     self->have_mouse ? "true" : "false",
     self->have_touchscreen ? "true" : "false");

  /* Let's show the button when a mouse or touchscreen is present */
  if (self->have_mouse || self->have_touchscreen)
    return TRUE;

  /* Let's also show when the touchpad is disabled. */
  if (!get_touchpad_enabled (self->touchpad_settings))
    return TRUE;

  return FALSE;
}

static gboolean
touchpad_enabled_get_mapping (GValue    *value,
                              GVariant  *variant,
                              gpointer   user_data)
{
  gboolean enabled;

  enabled = g_strcmp0 (g_variant_get_string (variant, NULL), "enabled") == 0;
  g_value_set_boolean (value, enabled);

  return TRUE;
}

static GVariant *
touchpad_enabled_set_mapping (const GValue              *value,
                              const GVariantType        *type,
                              gpointer                   user_data)
{
  gboolean enabled;

  enabled = g_value_get_boolean (value);

  return g_variant_new_string (enabled ? "enabled" : "disabled");
}

static gboolean
click_method_get_mapping (GValue    *value,
                          GVariant  *variant,
                          gpointer   user_data)
{
  gboolean is_default;

  is_default = g_strcmp0 (g_variant_get_string (variant, NULL), "fingers") == 0;
  g_value_set_boolean (value, is_default);

  return TRUE;
}

static GVariant *
click_method_set_mapping (const GValue       *value,
                          const GVariantType *type,
                          gpointer            user_data)
{
  gboolean is_default;

  is_default = g_value_get_boolean (value);

  return g_variant_new_string (is_default ? "fingers" : "areas");
}

static void
pressed_cb (GtkButton *button)
{
  g_signal_emit_by_name (button, "activate");
}

static void
handle_secondary_button (CcMousePanel    *self,
                         GtkToggleButton *button,
                         GtkGesture      *gesture)
{
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (pressed_cb), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (gesture));
}

static gboolean
mouse_accel_get_mapping (GValue    *value,
                         GVariant  *variant,
                         gpointer   user_data)
{
    gboolean enabled;

    enabled = g_strcmp0 (g_variant_get_string (variant, NULL), "flat") != 0;
    g_value_set_boolean (value, enabled);

    return TRUE;
}

static GVariant *
mouse_accel_set_mapping (const GValue       *value,
                         const GVariantType *type,
                         gpointer            user_data)
{
    return g_variant_new_string (g_value_get_boolean (value) ? "default" : "flat");
}

static void
update_primary_mouse_button_order (CcMousePanel *self)
{
  /* Manually reorder Left/Right buttons to preserve direction in RTL instead
   * of calling gtk_widget_set_direction (self->primary_button_box, GTK_TEXT_DIR_LTR)
   * which won't preserve the correct behavior of the CSS "linked" style class.
   * See https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/1101
   * and https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/2649 */
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL) {
    gtk_box_reorder_child_after (self->primary_button_box,
                                 GTK_WIDGET (self->primary_button_left),
                                 GTK_WIDGET (self->primary_button_right));
  } else {
    gtk_box_reorder_child_after (self->primary_button_box,
                                 GTK_WIDGET (self->primary_button_right),
                                 GTK_WIDGET (self->primary_button_left));
  }
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMousePanel *self)
{
  GtkToggleButton *button;

  update_primary_mouse_button_order (self);
  self->mouse_test = GTK_WINDOW (cc_mouse_test_new ());

  self->left_handed = g_settings_get_boolean (self->mouse_settings, "left-handed");
  button = self->left_handed ? self->primary_button_right : self->primary_button_left;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

  g_settings_bind (self->mouse_settings, "left-handed",
                   self->primary_button_left, "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (self->mouse_settings, "left-handed",
                   self->primary_button_right, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Allow changing orientation with either button */
  button = self->primary_button_right;
  self->right_gesture = gtk_gesture_click_new ();
  handle_secondary_button (self, button, self->right_gesture);
  button = self->primary_button_left;
  self->left_gesture = gtk_gesture_click_new ();
  handle_secondary_button (self, button, self->left_gesture);

  g_settings_bind (self->mouse_settings, "natural-scroll",
                   self->mouse_scroll_direction_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* Mouse section */
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_group), self->have_mouse);

  g_settings_bind (self->mouse_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->mouse_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->mouse_settings, "accel-profile",
                                self->mouse_accel_switch, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                mouse_accel_get_mapping,
                                mouse_accel_set_mapping,
                                NULL, NULL);

  /* Touchpad section */
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_row), can_disable_touchpad (self));

  g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
                                self->touchpad_toggle_row, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                touchpad_enabled_get_mapping,
                                touchpad_enabled_set_mapping,
                                NULL, NULL);

  g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
                                self->touchpad_typing_row, "sensitive",
                                G_SETTINGS_BIND_GET,
                                touchpad_enabled_get_mapping,
                                touchpad_enabled_set_mapping,
                                NULL, NULL);

  g_settings_bind (self->touchpad_settings, "natural-scroll",
                   self->touchpad_scroll_direction_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->touchpad_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "tap-to-click",
                   self->tap_to_click_switch, "active",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind_with_mapping (self->touchpad_settings, "click-method",
                                self->two_finger_push_row, "use-default",
                                G_SETTINGS_BIND_DEFAULT,
                                click_method_get_mapping,
                                click_method_set_mapping,
                                NULL, NULL);

  g_settings_bind (self->touchpad_settings, "two-finger-scrolling-enabled",
                   self->touchpad_scroll_method_row, "use-default",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "edge-scrolling-enabled",
                   self->touchpad_scroll_method_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "disable-while-typing",
                   self->touchpad_typing_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  setup_touchpad_options (self);

  g_signal_connect_object (adw_style_manager_get_default (),
                           "notify::dark",
                           G_CALLBACK (setup_illustrations),
                           self,
                           G_CONNECT_SWAPPED);
  setup_illustrations (self);
}

/* Callback issued when a button is clicked on the dialog */
static void
device_changed (CcMousePanel *self)
{
  self->have_touchpad = touchpad_is_present ();

  setup_touchpad_options (self);

  self->have_mouse = mouse_is_present ();
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_group), self->have_mouse);
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_row), can_disable_touchpad (self));
}

static void
cc_mouse_panel_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  CcMousePanel *self = CC_MOUSE_PANEL (widget);

  update_primary_mouse_button_order (self);

  GTK_WIDGET_CLASS (cc_mouse_panel_parent_class)->direction_changed (widget, previous_direction);
}

static void
cc_mouse_panel_dispose (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);

  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->touchpad_settings);
  g_clear_pointer (&self->mouse_test, gtk_window_destroy);

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
test_button_clicked_cb (CcMousePanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));

  gtk_window_set_transient_for (self->mouse_test,
                                GTK_WINDOW (cc_shell_get_toplevel (shell)));
  gtk_window_present (self->mouse_test);
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  GsdDeviceManager  *device_manager;

  g_resources_register (cc_mouse_get_resource ());

  cc_mouse_test_get_type ();
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  self->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");

  device_manager = gsd_device_manager_get ();
  g_signal_connect_object (device_manager, "device-added",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (device_manager, "device-removed",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);

  self->have_mouse = mouse_is_present ();
  self->have_touchpad = touchpad_is_present ();
  self->have_touchscreen = touchscreen_is_present ();
  self->have_synaptics = cc_synaptics_check ();
  if (self->have_synaptics)
    g_warning ("Detected synaptics X driver, please migrate to libinput");

  setup_dialog (self);
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->dispose = cc_mouse_panel_dispose;
  widget_class->direction_changed = cc_mouse_panel_direction_changed;

  g_type_ensure (CC_TYPE_ILLUSTRATED_ROW);
  g_type_ensure (CC_TYPE_SPLIT_ROW);
  g_type_ensure (CC_TYPE_LIST_ROW_INFO_BUTTON);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_accel_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_group);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_scroll_direction_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_button_box);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_button_left);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_button_right);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, preferences);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, title_stack);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_group);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_scroll_direction_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_scroll_method_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_speed_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_stack_page);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_toggle_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_typing_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, two_finger_push_row);

  gtk_widget_class_bind_template_callback (widget_class, on_touchpad_scroll_method_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_button_clicked_cb);
}
