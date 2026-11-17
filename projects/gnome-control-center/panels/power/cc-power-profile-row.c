/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
 *
 * Copyright 2020 Red Hat Inc.
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
 * Author(s):
 *   Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-power-profile-row"

#include <config.h>

#include <glib/gi18n.h>
#include "cc-power-profile-row.h"

struct _CcPowerProfileRow
{
  AdwActionRow parent_instance;

  GtkCheckButton *button;

  CcPowerProfile power_profile;
};

G_DEFINE_TYPE (CcPowerProfileRow, cc_power_profile_row, ADW_TYPE_ACTION_ROW)

enum {
  BUTTON_TOGGLED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
cc_power_profile_row_button_toggled_cb (CcPowerProfileRow *self)
{
  g_signal_emit (self, signals[BUTTON_TOGGLED], 0);
}

static void
cc_power_profile_row_class_init (CcPowerProfileRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-power-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPowerProfileRow, button);

  gtk_widget_class_bind_template_callback (widget_class, cc_power_profile_row_button_toggled_cb);

  signals[BUTTON_TOGGLED] =
    g_signal_new ("button-toggled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
cc_power_profile_row_init (CcPowerProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcPowerProfile
cc_power_profile_row_get_profile (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), -1);

  return self->power_profile;
}

GtkCheckButton *
cc_power_profile_row_get_radio_button (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), NULL);

  return self->button;
}

void
cc_power_profile_row_set_active (CcPowerProfileRow *self,
                                 gboolean           active)
{
  g_return_if_fail (CC_IS_POWER_PROFILE_ROW (self));

  gtk_check_button_set_active (GTK_CHECK_BUTTON (self->button), active);
}

gboolean
cc_power_profile_row_get_active (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), FALSE);

  return gtk_check_button_get_active (GTK_CHECK_BUTTON (self->button));
}

CcPowerProfileRow *
cc_power_profile_row_new (CcPowerProfile power_profile)
{
  CcPowerProfileRow *self;
  const char *text, *subtext;

  self = g_object_new (CC_TYPE_POWER_PROFILE_ROW, NULL);

  self->power_profile = power_profile;
  switch (self->power_profile)
    {
      case CC_POWER_PROFILE_PERFORMANCE:
        text = C_("Power profile", "P_erformance");
        subtext = _("High performance and power usage");
        break;
      case CC_POWER_PROFILE_BALANCED:
        text = C_("Power profile", "Ba_lanced");
        subtext = _("Standard performance and power usage");
        break;
      case CC_POWER_PROFILE_POWER_SAVER:
        text = C_("Power profile", "P_ower Saver");
        subtext = _("Reduced performance and power usage");
        break;
      default:
        g_assert_not_reached ();
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), text);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), subtext);

  return self;
}

CcPowerProfile
cc_power_profile_from_str (const char *profile)
{
  if (g_strcmp0 (profile, "power-saver") == 0)
    return CC_POWER_PROFILE_POWER_SAVER;
  if (g_strcmp0 (profile, "balanced") == 0)
    return CC_POWER_PROFILE_BALANCED;
  if (g_strcmp0 (profile, "performance") == 0)
    return CC_POWER_PROFILE_PERFORMANCE;

  g_assert_not_reached ();
}

const char *
cc_power_profile_to_str (CcPowerProfile profile)
{
  switch (profile)
  {
  case CC_POWER_PROFILE_POWER_SAVER:
    return "power-saver";
  case CC_POWER_PROFILE_BALANCED:
    return "balanced";
  case CC_POWER_PROFILE_PERFORMANCE:
    return "performance";
  default:
    g_assert_not_reached ();
  }
}
