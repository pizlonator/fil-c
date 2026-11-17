/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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
 */

#include "config.h"

#include <gdesktop-enums.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>

#include "cc-night-light-page.h"

#include "shell/cc-object-storage.h"
#include "cc-hostname.h"
#include "cc-display-config-manager-dbus.h"

struct _CcNightLightPage {
  AdwBin               parent;

  GtkWidget           *night_light_settings;
  GtkWidget           *box_manual;
  GtkButton           *button_from_am;
  GtkButton           *button_from_pm;
  GtkButton           *button_to_am;
  GtkButton           *button_to_pm;
  GtkWidget           *infobar_unsupported;
  GtkWidget           *infobar_unsupported_description;
  GtkWidget           *infobar_disabled;
  GtkWidget           *scale_color_temperature;
  AdwSwitchRow        *night_light_toggle_row;
  AdwComboRow         *schedule_type_row;
  GtkWidget           *from_spinbuttons_box;
  GtkSpinButton       *spinbutton_from_hours;
  GtkSpinButton       *spinbutton_from_minutes;
  GtkWidget           *to_spinbuttons_box;
  GtkSpinButton       *spinbutton_to_hours;
  GtkSpinButton       *spinbutton_to_minutes;
  GtkStack            *stack_from;
  GtkStack            *stack_to;

  GtkAdjustment       *adjustment_from_hours;
  GtkAdjustment       *adjustment_from_minutes;
  GtkAdjustment       *adjustment_to_hours;
  GtkAdjustment       *adjustment_to_minutes;
  GtkAdjustment       *adjustment_color_temperature;

  GSettings           *settings_display;
  GSettings           *settings_clock;
  GDBusProxy          *proxy_color;
  GDBusProxy          *proxy_color_props;
  GCancellable        *cancellable;
  gboolean             ignore_value_changed;
  guint                timer_id;
  GDesktopClockFormat  clock_format;

  CcDisplayConfigManager *config_manager;
};

G_DEFINE_TYPE (CcNightLightPage, cc_night_light_page, ADW_TYPE_BIN);

#define CLOCK_SCHEMA     "org.gnome.desktop.interface"
#define DISPLAY_SCHEMA   "org.gnome.settings-daemon.plugins.color"
#define CLOCK_FORMAT_KEY "clock-format"
#define NIGHT_LIGHT_PREVIEW_TIMEOUT_SECONDS 5

static void
dialog_adjustments_set_frac_hours (CcNightLightPage *self,
                                   gdouble           value,
                                   GtkAdjustment    *adj_hours,
                                   GtkAdjustment    *adj_mins,
                                   GtkStack         *stack,
                                   GtkButton        *button_am,
                                   GtkButton        *button_pm)
{
  gdouble hours;
  gdouble mins = 0.f;
  gboolean is_pm = FALSE;
  gboolean is_24h;

  /* display the right thing for AM/PM */
  is_24h = self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H;
  mins = modf (value, &hours) * 60.f;
  if (!is_24h)
    {
      if (hours > 12)
        {
          hours -= 12;
          is_pm = TRUE;
        }
      else if (hours < 1.0)
        {
          hours += 12;
          is_pm = FALSE;
        }
      else if (hours == 12.f)
        {
          is_pm = TRUE;
        }
    }

  g_debug ("setting adjustment %.3f to %.0f:%02.0f", value, hours, mins);

  self->ignore_value_changed = TRUE;
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj_hours), hours);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj_mins), mins);
  self->ignore_value_changed = FALSE;

  gtk_widget_set_visible (GTK_WIDGET (stack), !is_24h);
  gtk_stack_set_visible_child (stack, is_pm ? GTK_WIDGET (button_pm) : GTK_WIDGET (button_am));
}

static void
dialog_update_state (CcNightLightPage *self)
{
  if (cc_display_config_manager_get_night_light_supported (self->config_manager))
    {
      gboolean automatic;
      gboolean disabled_until_tomorrow = FALSE;
      gboolean enabled;
      gdouble value = 0.f;

      /* only show the infobar if we are disabled */
      if (self->proxy_color != NULL)
        {
          g_autoptr(GVariant) disabled = NULL;
          disabled = g_dbus_proxy_get_cached_property (self->proxy_color,
                                                       "DisabledUntilTomorrow");
          if (disabled != NULL)
            disabled_until_tomorrow = g_variant_get_boolean (disabled);
        }
      gtk_widget_set_visible (self->infobar_disabled, disabled_until_tomorrow);

      /* make things insensitive if the switch is disabled */
      enabled = g_settings_get_boolean (self->settings_display, "night-light-enabled");
      automatic = g_settings_get_boolean (self->settings_display, "night-light-schedule-automatic");

      gtk_widget_set_sensitive (self->box_manual, enabled && !automatic);

      adw_combo_row_set_selected (self->schedule_type_row, automatic ? 0 : 1);

      /* set from */
      if (automatic && self->proxy_color != NULL)
        {
          g_autoptr(GVariant) sunset = NULL;
          sunset = g_dbus_proxy_get_cached_property (self->proxy_color, "Sunset");
          if (sunset != NULL)
            {
              value = g_variant_get_double (sunset);
            }
          else
            {
              value = 16.0f;
              g_warning ("no sunset data, using %02.2f", value);
            }
        }
      else
        {
          value = g_settings_get_double (self->settings_display, "night-light-schedule-from");
          value = fmod (value, 24.f);
        }
      dialog_adjustments_set_frac_hours (self, value,
                                         self->adjustment_from_hours,
                                         self->adjustment_from_minutes,
                                         self->stack_from,
                                         self->button_from_am,
                                         self->button_from_pm);

      /* set to */
      if (automatic && self->proxy_color != NULL)
        {
          g_autoptr(GVariant) sunset = NULL;
          sunset = g_dbus_proxy_get_cached_property (self->proxy_color, "Sunrise");
          if (sunset != NULL)
            {
              value = g_variant_get_double (sunset);
            }
          else
            {
              value = 8.0f;
              g_warning ("no sunrise data, using %02.2f", value);
            }
        }
      else
        {
          value = g_settings_get_double (self->settings_display, "night-light-schedule-to");
          value = fmod (value, 24.f);
        }
      dialog_adjustments_set_frac_hours (self, value,
                                         self->adjustment_to_hours,
                                         self->adjustment_to_minutes,
                                         self->stack_to,
                                         self->button_to_am,
                                         self->button_to_pm);

      self->ignore_value_changed = TRUE;
      value = (gdouble) g_settings_get_uint (self->settings_display, "night-light-temperature");
      gtk_adjustment_set_value (self->adjustment_color_temperature, value);
      self->ignore_value_changed = FALSE;
    }
  else
    {
      gtk_widget_set_visible (self->infobar_unsupported, TRUE);
      gtk_widget_set_visible (self->infobar_disabled, FALSE);
      gtk_widget_set_sensitive (self->night_light_settings, FALSE);

      if (cc_hostname_is_vm_chassis (cc_hostname_get_default ()))
        {
          gtk_label_set_text (GTK_LABEL (self->infobar_unsupported_description),
                              _("Night Light cannot be used from a virtual machine."));
        }
    }
}

static void
build_schedule_combo_row (CcNightLightPage *self)
{
  gboolean automatic;
  gboolean enabled;

  self->ignore_value_changed = TRUE;


  enabled = g_settings_get_boolean (self->settings_display, "night-light-enabled");
  automatic = g_settings_get_boolean (self->settings_display, "night-light-schedule-automatic");

  gtk_widget_set_sensitive (self->box_manual, enabled && !automatic);

  adw_combo_row_set_selected (self->schedule_type_row, automatic ? 0 : 1);

  self->ignore_value_changed = FALSE;
}

static void
on_schedule_type_row_selected_changed_cb (CcNightLightPage *self)
{
  guint selected;
  gboolean automatic;

  if (self->ignore_value_changed)
    return;

  selected = adw_combo_row_get_selected (self->schedule_type_row);
  automatic = selected == 0;;

  g_settings_set_boolean (self->settings_display, "night-light-schedule-automatic", automatic);
}

static gboolean
dialog_tick_cb (gpointer user_data)
{
  CcNightLightPage *self = (CcNightLightPage *) user_data;
  dialog_update_state (self);
  return G_SOURCE_CONTINUE;
}

static void
dialog_enabled_notify_cb (GtkSwitch        *sw,
                          GParamSpec       *pspec,
                          CcNightLightPage *self)
{
  g_settings_set_boolean (self->settings_display, "night-light-enabled",
                          gtk_switch_get_active (sw));
}

static void
dialog_undisable_call_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) error = NULL;

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res, &error);
  if (val == NULL)
    {
      g_warning ("failed to undisable: %s", error->message);
      return;
    }
}

static void
dialog_undisable_clicked_cb (CcNightLightPage *self)
{
  g_dbus_proxy_call (self->proxy_color_props,
                     "Set",
                     g_variant_new ("(ssv)",
                                    "org.gnome.SettingsDaemon.Color",
                                    "DisabledUntilTomorrow",
                                    g_variant_new_boolean (FALSE)),
                     G_DBUS_CALL_FLAGS_NONE,
                     5000,
                     self->cancellable,
                     dialog_undisable_call_cb,
                     self);
}

static gdouble
dialog_adjustments_get_frac_hours (CcNightLightPage *self,
                                   GtkAdjustment    *adj_hours,
                                   GtkAdjustment    *adj_mins,
                                   GtkStack         *stack)
{
  gdouble value;

  value = gtk_adjustment_get_value (adj_hours);
  value += gtk_adjustment_get_value (adj_mins) / 60.0f;

  if (g_strcmp0 (gtk_stack_get_visible_child_name (stack), "pm") == 0)
    value += 12.f;

  return value;
}

static void
dialog_time_from_value_changed_cb (CcNightLightPage *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                             self->adjustment_from_hours,
                                             self->adjustment_from_minutes,
                                             self->stack_from);

  if (value >= 24.f)
    value = fmod (value, 24);

  g_debug ("new value = %.3f", value);

  g_settings_set_double (self->settings_display, "night-light-schedule-from", value);
}

static void
dialog_time_to_value_changed_cb (CcNightLightPage *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                             self->adjustment_to_hours,
                                             self->adjustment_to_minutes,
                                             self->stack_to);
  if (value >= 24.f)
    value = fmod (value, 24);

  g_debug ("new value = %.3f", value);

  g_settings_set_double (self->settings_display, "night-light-schedule-to", value);
}

static void
dialog_color_temperature_value_changed_cb (CcNightLightPage *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = gtk_adjustment_get_value (self->adjustment_color_temperature);

  g_debug ("new value = %.0f", value);

  if (self->proxy_color != NULL)
    g_dbus_proxy_call (self->proxy_color,
                       "NightLightPreview",
                       g_variant_new ("(u)", NIGHT_LIGHT_PREVIEW_TIMEOUT_SECONDS),
                       G_DBUS_CALL_FLAGS_NONE,
                       5000,
                       NULL,
                       NULL,
                       NULL);

  g_settings_set_uint (self->settings_display, "night-light-temperature", (guint) value);
}

static void
dialog_color_properties_changed_cb (CcNightLightPage *self)
{
  dialog_update_state (self);
}

static void
dialog_got_proxy_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcNightLightPage *self = (CcNightLightPage *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color = proxy;

  g_signal_connect_object (self->proxy_color, "g-properties-changed",
                           G_CALLBACK (dialog_color_properties_changed_cb), self, G_CONNECT_SWAPPED);
  dialog_update_state (self);
  self->timer_id = g_timeout_add_seconds (10, dialog_tick_cb, self);
}

static void
dialog_got_proxy_props_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  CcNightLightPage *self = (CcNightLightPage *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color_props = proxy;
}

static void
dialog_format_minutes_combobox (GtkSpinButton *spin)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  adjustment = gtk_spin_button_get_adjustment (spin);
  text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_editable_set_text (GTK_EDITABLE (spin), text);
}

static gboolean
dialog_format_from_minutes_combobox (CcNightLightPage *self)
{
  dialog_format_minutes_combobox (self->spinbutton_from_minutes);
  return TRUE;
}

static gboolean
dialog_format_to_minutes_combobox (CcNightLightPage *self)
{
  dialog_format_minutes_combobox (self->spinbutton_to_minutes);
  return TRUE;
}

static void
dialog_format_hours_combobox (CcNightLightPage *self, GtkSpinButton *spin)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  adjustment = gtk_spin_button_get_adjustment (spin);
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    text = g_strdup_printf ("%.0f", gtk_adjustment_get_value (adjustment));
  else
    text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_editable_set_text (GTK_EDITABLE (spin), text);
}

static gboolean
dialog_format_from_hours_combobox (CcNightLightPage *self)
{
  dialog_format_hours_combobox (self, self->spinbutton_from_hours);
  return TRUE;
}

static gboolean
dialog_format_to_hours_combobox (CcNightLightPage *self)
{
  dialog_format_hours_combobox (self, self->spinbutton_to_hours);
  return TRUE;
}

static void
dialog_update_adjustments (CcNightLightPage *self)
{
  /* from */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (self->adjustment_from_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (self->adjustment_from_hours) > 12)
          gtk_stack_set_visible_child (self->stack_from, GTK_WIDGET (self->button_from_pm));

      gtk_adjustment_set_lower (self->adjustment_from_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 12);
    }

  /* to */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (self->adjustment_to_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (self->adjustment_to_hours) > 12)
          gtk_stack_set_visible_child (self->stack_to, GTK_WIDGET (self->button_to_pm));

      gtk_adjustment_set_lower (self->adjustment_to_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 12);
    }
}

static void
dialog_settings_changed_cb (CcNightLightPage *self)
{
  dialog_update_state (self);
}

static void
dialog_clock_settings_changed_cb (CcNightLightPage *self)
{
  self->clock_format = g_settings_get_enum (self->settings_clock, CLOCK_FORMAT_KEY);

  /* uncontionally widen this to avoid truncation */
  gtk_adjustment_set_lower (self->adjustment_from_hours, 0);
  gtk_adjustment_set_upper (self->adjustment_from_hours, 23);
  gtk_adjustment_set_lower (self->adjustment_to_hours, 0);
  gtk_adjustment_set_upper (self->adjustment_to_hours, 23);

  /* update spinbuttons */
  gtk_spin_button_update (self->spinbutton_from_hours);
  gtk_spin_button_update (self->spinbutton_to_hours);

  /* update UI */
  dialog_update_state (self);
  dialog_update_adjustments (self);
}

static void
dialog_am_pm_from_button_clicked_cb (CcNightLightPage *self)
{
  gdouble value;
  value = g_settings_get_double (self->settings_display, "night-light-schedule-from");
  if (value > 12.f)
    value -= 12.f;
  else
    value += 12.f;
  if (value >= 24.f)
    value = fmod (value, 24);
  g_settings_set_double (self->settings_display, "night-light-schedule-from", value);
  g_debug ("new value = %.3f", value);
}

static void
dialog_am_pm_to_button_clicked_cb (CcNightLightPage *self)
{
  gdouble value;
  value = g_settings_get_double (self->settings_display, "night-light-schedule-to");
  if (value > 12.f)
    value -= 12.f;
  else
    value += 12.f;
  if (value >= 24.f)
    value = fmod (value, 24);
  g_settings_set_double (self->settings_display, "night-light-schedule-to", value);
  g_debug ("new value = %.3f", value);
}

static void
config_manager_changed_cb (CcNightLightPage *self)
{
  dialog_update_state (self);
}

/* GObject overrides */
static void
cc_night_light_page_finalize (GObject *object)
{
  CcNightLightPage *self = CC_NIGHT_LIGHT_PAGE (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->config_manager);
  g_clear_object (&self->proxy_color);
  g_clear_object (&self->proxy_color_props);
  g_clear_object (&self->settings_display);
  g_clear_object (&self->settings_clock);
  g_clear_handle_id (&self->timer_id, g_source_remove);

  G_OBJECT_CLASS (cc_night_light_page_parent_class)->finalize (object);
}

static void
cc_night_light_page_class_init (CcNightLightPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_night_light_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/display/cc-night-light-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, adjustment_from_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, adjustment_from_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, adjustment_to_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, adjustment_to_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, adjustment_color_temperature);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, night_light_settings);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, box_manual);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, button_from_am);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, button_from_pm);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, button_to_am);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, button_to_pm);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, infobar_unsupported);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, infobar_unsupported_description);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, infobar_disabled);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, night_light_toggle_row);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, schedule_type_row);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, scale_color_temperature);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, from_spinbuttons_box);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, spinbutton_from_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, spinbutton_from_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, to_spinbuttons_box);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, spinbutton_to_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, spinbutton_to_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, stack_from);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightPage, stack_to);

  gtk_widget_class_bind_template_callback (widget_class, dialog_am_pm_from_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_am_pm_to_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_enabled_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_from_hours_combobox);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_to_hours_combobox);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_from_minutes_combobox);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_to_minutes_combobox);   
  gtk_widget_class_bind_template_callback (widget_class, dialog_time_from_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_time_to_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_color_temperature_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_undisable_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_schedule_type_row_selected_changed_cb);

}

static void
cc_night_light_page_init (CcNightLightPage *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_scale_add_mark (GTK_SCALE (self->scale_color_temperature),
                      1700, GTK_POS_BOTTOM,
                      NULL);

  gtk_scale_add_mark (GTK_SCALE (self->scale_color_temperature),
                      2700, GTK_POS_BOTTOM,
                      NULL);

  gtk_scale_add_mark (GTK_SCALE (self->scale_color_temperature),
                      3700, GTK_POS_BOTTOM,
                      NULL);

  gtk_scale_add_mark (GTK_SCALE (self->scale_color_temperature),
                      4700, GTK_POS_BOTTOM,
                      NULL);

  self->cancellable = g_cancellable_new ();
  self->settings_display = g_settings_new (DISPLAY_SCHEMA);

  g_signal_connect_object (self->settings_display, "changed", G_CALLBACK (dialog_settings_changed_cb), self, G_CONNECT_SWAPPED);

  build_schedule_combo_row (self);

  g_settings_bind (self->settings_display, "night-light-enabled",
                   self->night_light_toggle_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_writable (self->settings_display, "night-light-enabled",
                            self->night_light_toggle_row, "sensitive",
                            FALSE);

  g_settings_bind_writable (self->settings_display, "night-light-schedule-from",
                            self->spinbutton_from_hours, "sensitive",
                            FALSE);
  g_settings_bind_writable (self->settings_display, "night-light-schedule-from",
                            self->spinbutton_from_minutes, "sensitive",
                            FALSE);
  g_settings_bind_writable (self->settings_display, "night-light-schedule-to",
                            self->spinbutton_to_minutes, "sensitive",
                            FALSE);
  g_settings_bind_writable (self->settings_display, "night-light-schedule-to",
                            self->spinbutton_to_minutes, "sensitive",
                            FALSE);

  /* use custom CSS */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/display/night-light.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Color",
                                       "/org/gnome/SettingsDaemon/Color",
                                       "org.gnome.SettingsDaemon.Color",
                                       self->cancellable,
                                       dialog_got_proxy_cb,
                                       self);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Color",
                                       "/org/gnome/SettingsDaemon/Color",
                                       "org.freedesktop.DBus.Properties",
                                       self->cancellable,
                                       dialog_got_proxy_props_cb,
                                       self);

  /* clock settings_display */
  self->settings_clock = g_settings_new (CLOCK_SCHEMA);
  self->clock_format = g_settings_get_enum (self->settings_clock, CLOCK_FORMAT_KEY);
  dialog_update_adjustments (self);
  g_signal_connect_object (self->settings_clock,
                           "changed::" CLOCK_FORMAT_KEY,
                           G_CALLBACK (dialog_clock_settings_changed_cb),
                           self, G_CONNECT_SWAPPED);

  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    {
      gtk_widget_set_direction (self->to_spinbuttons_box, GTK_TEXT_DIR_LTR);
      gtk_widget_set_direction (self->from_spinbuttons_box, GTK_TEXT_DIR_LTR);
    }

  self->config_manager = cc_display_config_manager_dbus_new ();
  g_signal_connect_object (self->config_manager, "changed",
                           G_CALLBACK (config_manager_changed_cb), self,
                           G_CONNECT_SWAPPED);

  dialog_update_state (self);
}

CcNightLightPage *
cc_night_light_page_new (void)
{
  return g_object_new (CC_TYPE_NIGHT_LIGHT_PAGE,
                       NULL);
}

