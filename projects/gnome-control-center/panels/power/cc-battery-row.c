/* cc-brightness-scale.c
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010,2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 System76, Inc.
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib/gi18n.h>

#include "cc-battery-row.h"

struct _CcBatteryRow {
  GtkListBoxRow parent_instance;

  GtkBox       *battery_box;
  GtkLabel     *details_label;
  GtkImage     *icon;
  GtkLevelBar  *levelbar;
  GtkLabel     *name_label;
  GtkLabel     *percentage_label;
  GtkBox       *primary_bottom_box;
  GtkLabel     *primary_percentage_label;

  UpDeviceKind  kind;
  gboolean      primary;
};

G_DEFINE_TYPE (CcBatteryRow, cc_battery_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_battery_row_class_init (CcBatteryRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-battery-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, battery_box);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, details_label);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, icon);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, levelbar);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, percentage_label);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, primary_bottom_box);
  gtk_widget_class_bind_template_child (widget_class, CcBatteryRow, primary_percentage_label);
}

static void
cc_battery_row_init (CcBatteryRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static gchar *
get_timestring (guint64 time_secs)
{
  gchar* timestring = NULL;
  gint  hours;
  gint  minutes;

  /* Add 0.5 to do rounding */
  minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

  if (minutes == 0)
    return g_strdup (_("Unknown time"));

  if (minutes < 60)
    return timestring = g_strdup_printf (ngettext ("%i minute",
                                         "%i minutes",
                                         minutes), minutes);

  hours = minutes / 60;
  minutes = minutes % 60;

  if (minutes == 0)
    return timestring = g_strdup_printf (ngettext (
                                         "%i hour",
                                         "%i hours",
                                         hours), hours);

  /* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
   * Swap order with "%2$s %2$i %1$s %1$i if needed */
  return timestring = g_strdup_printf (_("%i %s %i %s"),
                                       hours, ngettext ("hour", "hours", hours),
                                       minutes, ngettext ("minute", "minutes", minutes));
}

static gchar *
get_details_string (gdouble percentage, UpDeviceState state, guint64 time)
{
  g_autofree gchar *details = NULL;

  if (time > 0)
    {
      g_autofree gchar *time_string = NULL;

      time_string = get_timestring (time);
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
            /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
            details = g_strdup_printf (_("%s until fully charged"), time_string);
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            if (percentage < 20)
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf (_("Caution: %s remaining"), time_string);
              }
            else
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf (_("%s remaining"), time_string);
              }
            break;
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Fully charged"));
            break;
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Not charging"));
            break;
          case UP_DEVICE_STATE_EMPTY:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Empty"));
            break;
          default:
            details = g_strdup_printf ("error: %s", up_device_state_to_string (state));
            break;
        }
    }
  else
    {
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Charging"));
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Discharging"));
            break;
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Fully charged"));
            break;
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Not charging"));
            break;
          case UP_DEVICE_STATE_EMPTY:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Empty"));
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }

  return g_steal_pointer (&details);
}

static const char *
kind_to_description (UpDeviceKind kind)
{
  switch (kind)
    {
      case UP_DEVICE_KIND_MOUSE:
        /* TRANSLATORS: secondary battery */
        return N_("Wireless mouse");
      case UP_DEVICE_KIND_KEYBOARD:
        /* TRANSLATORS: secondary battery */
        return N_("Wireless keyboard");
      case UP_DEVICE_KIND_UPS:
        /* TRANSLATORS: secondary battery */
        return N_("Uninterruptible power supply");
      case UP_DEVICE_KIND_PDA:
        /* TRANSLATORS: secondary battery */
        return N_("Personal digital assistant");
      case UP_DEVICE_KIND_PHONE:
        /* TRANSLATORS: secondary battery */
        return N_("Cellphone");
      case UP_DEVICE_KIND_MEDIA_PLAYER:
        /* TRANSLATORS: secondary battery */
        return N_("Media player");
      case UP_DEVICE_KIND_TABLET:
        /* TRANSLATORS: secondary battery */
        return N_("Tablet");
      case UP_DEVICE_KIND_COMPUTER:
        /* TRANSLATORS: secondary battery */
        return N_("Computer");
      case UP_DEVICE_KIND_GAMING_INPUT:
        /* TRANSLATORS: secondary battery */
        return N_("Gaming input device");
      default:
        /* TRANSLATORS: secondary battery, misc */
        return N_("Battery");
    }

  g_assert_not_reached ();
}

CcBatteryRow*
cc_battery_row_new (UpDevice *device,
                    gboolean  primary)
{
  g_autofree gchar *details = NULL;
  gdouble percentage;
  UpDeviceKind kind;
  UpDeviceState state;
  g_autofree gchar *s = NULL;
  g_autofree gchar *icon_name = NULL;
  const gchar *name;
  CcBatteryRow *self;
  guint64 time_empty, time_full, time;
  gdouble energy_full, energy_rate;
  gboolean is_kind_battery;
  UpDeviceLevel battery_level;

  self = g_object_new (CC_TYPE_BATTERY_ROW, NULL);

  g_object_get (device,
                "kind", &kind,
                "state", &state,
                "model", &name,
                "percentage", &percentage,
                "icon-name", &icon_name,
                "time-to-empty", &time_empty,
                "time-to-full", &time_full,
                "energy-full", &energy_full,
                "energy-rate", &energy_rate,
                "battery-level", &battery_level,
                NULL);
  if (state == UP_DEVICE_STATE_DISCHARGING)
    time = time_empty;
  else
    time = time_full;

  is_kind_battery = (kind == UP_DEVICE_KIND_BATTERY || kind == UP_DEVICE_KIND_UPS);

  /* Name label */
  if (is_kind_battery)
    {
      if (g_object_get_data (G_OBJECT (device), "is-main-battery") != NULL)
        name = C_("Battery name", "Main");
      else
        name = C_("Battery name", "Extra");
    }
  else if (name == NULL || name[0] == '\0')
    {
      name = _(kind_to_description (kind));
    }
  gtk_label_set_text (self->name_label, name);

  /* Icon */
  if (is_kind_battery && icon_name != NULL && icon_name[0] != '\0')
    {
      gtk_image_set_from_icon_name (self->icon, icon_name);
      gtk_widget_set_visible (GTK_WIDGET (self->icon), TRUE);
    }
  else
    gtk_widget_set_visible (GTK_WIDGET (self->icon), FALSE);

  /* Percentage label */
  if (battery_level == UP_DEVICE_LEVEL_NONE)
  {
    /* TRANSLATORS: This is the battery charge level percentage (e.g. 50%). This could be localized to account for local Percent sign formatting guidelines. See https://en.wikipedia.org/wiki/Percent_sign#Correct_style */
    s = g_strdup_printf (_("%d %%"), (int)percentage);
    gtk_label_set_text (self->percentage_label, s);
    gtk_label_set_text (self->primary_percentage_label, s);
  }

  /* Level bar */
  gtk_level_bar_set_value (self->levelbar, percentage / 100.0);

  /* Details label (primary only) */
  details = get_details_string (percentage, state, time);
  gtk_label_set_text (self->details_label, details);

  /* Handle "primary" row differently */
  gtk_widget_set_visible (GTK_WIDGET (self->battery_box), !primary);
  gtk_widget_set_visible (GTK_WIDGET (self->percentage_label), !primary);
  gtk_widget_set_visible (GTK_WIDGET (self->primary_bottom_box), primary);
  /*
  gtk_accessible_update_relation (GTK_ACCESSIBLE (self->levelbar),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, primary ? self->primary_percentage_label
                                                                               : self->percentage_label,
                                  NULL);
   */

  self->kind = kind;
  self->primary = primary;

  return self;
}



gboolean
cc_battery_row_get_primary (CcBatteryRow *self)
{
  return self->primary;
}

UpDeviceKind
cc_battery_row_get_kind (CcBatteryRow *self)
{
  return self->kind;
}
