/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Authors: Joaquim Rocha <jrocha@redhat.com>
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
 */

#pragma once

#include <gtk/gtk.h>
#include <gdesktop-enums.h>

G_BEGIN_DECLS

#define CC_WACOM_TYPE_BUTTON_ROW (cc_wacom_button_row_get_type ())
G_DECLARE_FINAL_TYPE (CcWacomButtonRow, cc_wacom_button_row, CC, WACOM_BUTTON_ROW, GtkListBoxRow)

static struct {
  GDesktopPadButtonAction  action_type;
  const gchar             *action_name;
} action_table[] = {
  { G_DESKTOP_PAD_BUTTON_ACTION_NONE, NC_("Wacom action-type", "Application defined") },
  { G_DESKTOP_PAD_BUTTON_ACTION_KEYBINDING, NC_("Wacom action-type", "Send Keystroke") },
  { G_DESKTOP_PAD_BUTTON_ACTION_SWITCH_MONITOR, NC_("Wacom action-type", "Switch Monitor") },
  { G_DESKTOP_PAD_BUTTON_ACTION_HELP, NC_("Wacom action-type", "Show On-Screen Help") }
};

GtkWidget * cc_wacom_button_row_new      (guint      button,
                                          GSettings *settings);

G_END_DECLS
