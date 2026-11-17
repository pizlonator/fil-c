/* cc-wacom-stylus-action-dialog.c
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

#include <config.h>
#include <glib/gi18n.h>
#include <adwaita.h>

#include <gdesktop-enums.h>

#include "cc-wacom-stylus-action-dialog.h"
#include "cc-wacom-panel.h"

struct _CcWacomStylusActionDialog
{
	AdwWindow             parent_instance;

	AdwPreferencesGroup  *preferences_group;

	AdwActionRow         *left_button_row;
	AdwActionRow         *right_button_row;
	AdwActionRow         *middle_button_row;
	AdwActionRow         *back_row;
	AdwActionRow         *forward_row;

	GSettings            *settings;
	char                 *key;
};

G_DEFINE_TYPE (CcWacomStylusActionDialog, cc_wacom_stylus_action_dialog, ADW_TYPE_WINDOW)

static void
cc_wacom_stylus_action_dialog_finalize (GObject *object)
{
	CcWacomStylusActionDialog *self = CC_WACOM_STYLUS_ACTION_DIALOG (object);

	g_clear_pointer (&self->key, free);

	G_OBJECT_CLASS (cc_wacom_stylus_action_dialog_parent_class)->finalize (object);
}

static void
left_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);
}

static void
right_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT);
}

static void
middle_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE);
}

static void
back_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_BACK);
}

static void
forward_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD);
}

static void
cc_wacom_stylus_action_dialog_class_init (CcWacomStylusActionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = cc_wacom_stylus_action_dialog_finalize;

	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/control-center/"
						     "wacom/cc-wacom-stylus-action-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, preferences_group);

	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, left_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, right_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, middle_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, back_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, forward_row);

	gtk_widget_class_bind_template_callback (widget_class, left_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, middle_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, right_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, forward_activated);
	gtk_widget_class_bind_template_callback (widget_class, back_activated);
}

static void
cc_wacom_stylus_action_dialog_init (CcWacomStylusActionDialog *self)
{
	const char *text;

	gtk_widget_init_template (GTK_WIDGET (self));

	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->left_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->middle_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->right_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_BACK);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->back_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forward_row), text);
}

GtkWidget*
cc_wacom_stylus_action_dialog_new (GSettings   *settings,
				   const char  *stylus_name,
				   guint        button,
				   const char  *key)
{
	CcWacomStylusActionDialog *dialog = g_object_new (CC_TYPE_WACOM_STYLUS_ACTION_DIALOG, NULL);
	GDesktopStylusButtonAction action;
	AdwActionRow *row = NULL;
	g_autofree char *text = NULL;
	g_autofree char *title = NULL;

	g_return_val_if_fail(button > 0 && button <= 3, NULL);

	dialog->settings = settings;
	dialog->key = g_strdup (key);

	gtk_window_set_title (GTK_WINDOW (dialog), stylus_name);

	text = g_strdup_printf (_("Choose an action when button %d on the stylus is pressed"), button);
	adw_preferences_group_set_description (dialog->preferences_group, text);

	title = g_strdup_printf (_("Button %d Mapping"), button);
	adw_preferences_group_set_title (dialog->preferences_group, title);

	action = g_settings_get_enum (settings, key);
	switch (action) {
		case G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT:
			row = dialog->left_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE:
			row = dialog->middle_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT:
			row = dialog->right_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_BACK:
			row = dialog->back_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD:
			row = dialog->forward_row;
			break;
	}

	if (row)
		adw_action_row_activate (row);

	return GTK_WIDGET (dialog);
}
