/* cc-wacom-stylus-action-dialog.h
 *
 * Copyright © 2024 Red Hat, Inc.
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_WACOM_STYLUS_ACTION_DIALOG (cc_wacom_stylus_action_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcWacomStylusActionDialog, cc_wacom_stylus_action_dialog, CC, WACOM_STYLUS_ACTION_DIALOG, AdwWindow)

GtkWidget* cc_wacom_stylus_action_dialog_new (GSettings  *settings,
					      const char *stylus_name,
					      guint       button,
					      const char *key);

G_END_DECLS
