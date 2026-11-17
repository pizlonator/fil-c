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
#define G_LOG_DOMAIN "cc-power-profile-info-row"

#include <config.h>

#include <glib/gi18n.h>
#include "cc-power-profile-info-row.h"

struct _CcPowerProfileInfoRow
{
  GtkListBoxRow parent_instance;

  GtkLabel       *title_label;
};

G_DEFINE_TYPE (CcPowerProfileInfoRow, cc_power_profile_info_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_power_profile_info_row_class_init (CcPowerProfileInfoRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-power-profile-info-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPowerProfileInfoRow, title_label);
}

static void
cc_power_profile_info_row_init (CcPowerProfileInfoRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcPowerProfileInfoRow *
cc_power_profile_info_row_new (const char *text)
{
  CcPowerProfileInfoRow *self;

  self = g_object_new (CC_TYPE_POWER_PROFILE_INFO_ROW, NULL);
  gtk_label_set_markup (self->title_label, text);

  return self;
}
