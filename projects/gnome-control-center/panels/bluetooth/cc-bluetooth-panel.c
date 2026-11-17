/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <adwaita.h>

#include <shell/cc-shell.h>
#include <shell/cc-object-storage.h>
#include <bluetooth-settings-widget.h>

#include "cc-bluetooth-panel.h"
#include "cc-bluetooth-resources.h"

struct _CcBluetoothPanel {
	CcPanel                  parent_instance;

	AdwStatusPage           *airplane_page;
	AdwStatusPage           *disabled_page;
	GtkSwitch               *enable_switch;
	GtkBox                  *header_box;
	AdwStatusPage           *hw_airplane_page;
	AdwStatusPage           *no_devices_page;
	BluetoothSettingsWidget *settings_widget;
	GtkStack                *stack;

	/* Killswitch */
	GDBusProxy              *rfkill;
	GDBusProxy              *properties;
	gboolean                 airplane_mode;
	gboolean                 bt_airplane_mode;
	gboolean                 hardware_airplane_mode;
	gboolean                 has_airplane_mode;
};

CC_PANEL_REGISTER (CcBluetoothPanel, cc_bluetooth_panel)

static const char *
cc_bluetooth_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/bluetooth";
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	CcBluetoothPanel *self;

	self = CC_BLUETOOTH_PANEL (object);

	g_clear_object (&self->properties);
	g_clear_object (&self->rfkill);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

static void
airplane_mode_changed_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	g_autoptr(GVariant) ret = NULL;
	g_autoptr(GError) error = NULL;

	if (!g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
				       res, &error)) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			CcBluetoothPanel *self = CC_BLUETOOTH_PANEL (user_data);
			gboolean state = gtk_switch_get_active (self->enable_switch);
			g_warning ("Failed to change Bluetooth killswitch state to %s: %s",
				   state ? "on" : "off", error->message);
		}
	} else {
		CcBluetoothPanel *self = CC_BLUETOOTH_PANEL (user_data);
		gboolean state = gtk_switch_get_active (self->enable_switch);

		g_debug ("Changed Bluetooth killswitch state to %s",
			 state ? "on" : "off");

		gtk_switch_set_state (self->enable_switch, state);
		if (!bluetooth_settings_widget_get_default_adapter_powered (self->settings_widget))
			bluetooth_settings_widget_set_default_adapter_powered(self->settings_widget, TRUE);
	}
}

static gboolean
enable_switch_state_set_cb (CcBluetoothPanel *self, gboolean state)
{
	g_debug ("Power switched to %s", state ? "on" : "off");
	g_dbus_proxy_call (self->properties,
			   "Set",
			   g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', %v)",
						 g_variant_new_boolean (!state)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cc_panel_get_cancellable (CC_PANEL (self)),
			   airplane_mode_changed_cb, self);

	return TRUE;
}

static void
adapter_status_changed_cb (CcBluetoothPanel *self)
{
	GtkAlign valign;
	gboolean sensitive, powered;
	GtkWidget *page;

	g_debug ("Updating airplane mode: BluetoothHasAirplaneMode %d, BluetoothHardwareAirplaneMode %d, BluetoothAirplaneMode %d, AirplaneMode %d",
		 self->has_airplane_mode, self->hardware_airplane_mode, self->bt_airplane_mode, self->airplane_mode);

	valign = GTK_ALIGN_CENTER;

	if (self->has_airplane_mode == FALSE) {
		g_debug ("No Bluetooth available");
		sensitive = FALSE;
		powered = FALSE;
		page = GTK_WIDGET (self->no_devices_page);
	} else if (self->hardware_airplane_mode) {
		g_debug ("Bluetooth is Hard blocked");
		sensitive = FALSE;
		powered = FALSE;
		page = GTK_WIDGET (self->hw_airplane_page);
	} else if (self->airplane_mode) {
		g_debug ("Airplane mode is on, Wi-Fi and Bluetooth are disabled");
		sensitive = FALSE;
		powered = FALSE;
		page = GTK_WIDGET (self->airplane_page);
	} else if (self->bt_airplane_mode ||
		   !bluetooth_settings_widget_get_default_adapter_powered (self->settings_widget)) {
		g_debug ("Default adapter is unpowered");
		sensitive = TRUE;
		powered = FALSE;
		page = GTK_WIDGET (self->disabled_page);
	} else {
		g_debug ("Bluetooth is available and powered");
		sensitive = TRUE;
		powered = TRUE;
		page = GTK_WIDGET (self->settings_widget);
		valign = GTK_ALIGN_FILL;
	}

	gtk_widget_set_valign (GTK_WIDGET (self->stack), valign);
	gtk_widget_set_sensitive (GTK_WIDGET (self->header_box), sensitive);
	g_signal_handlers_block_by_func (self->enable_switch, enable_switch_state_set_cb, self);
	gtk_switch_set_active (self->enable_switch, powered);
	g_signal_handlers_unblock_by_func (self->enable_switch, enable_switch_state_set_cb, self);

	gtk_stack_set_visible_child (self->stack, page);
}

static void
airplane_mode_changed (CcBluetoothPanel *self)
{
	g_autoptr(GVariant) airplane_mode = NULL;
	g_autoptr(GVariant) bluetooth_airplane_mode = NULL;
	g_autoptr(GVariant) bluetooth_hardware_airplane_mode = NULL;
	g_autoptr(GVariant) bluetooth_has_airplane_mode = NULL;

	airplane_mode = g_dbus_proxy_get_cached_property (self->rfkill, "AirplaneMode");
	self->airplane_mode = g_variant_get_boolean (airplane_mode);

	bluetooth_airplane_mode = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothAirplaneMode");
	self->bt_airplane_mode = g_variant_get_boolean (bluetooth_airplane_mode);

	bluetooth_hardware_airplane_mode = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothHardwareAirplaneMode");
	self->hardware_airplane_mode = g_variant_get_boolean (bluetooth_hardware_airplane_mode);

	bluetooth_has_airplane_mode = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothHasAirplaneMode");
	self->has_airplane_mode = g_variant_get_boolean (bluetooth_has_airplane_mode);

	adapter_status_changed_cb (self);
}

static void
airplane_mode_off_button_clicked_cb (CcBluetoothPanel *self)
{
	g_debug ("Airplane Mode Off clicked, disabling airplane mode");
	g_dbus_proxy_call (self->rfkill,
			   "org.freedesktop.DBus.Properties.Set",
			   g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
						 "'AirplaneMode', %v)",
						 g_variant_new_boolean (FALSE)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cc_panel_get_cancellable (CC_PANEL (self)),
			   NULL, NULL);
}

static void
panel_changed_cb (CcBluetoothPanel *self,
                  const char       *panel)
{
	CcShell *shell;
	g_autoptr(GError) error = NULL;

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (cc_shell_set_active_panel_from_id (shell, panel, NULL, &error) == FALSE)
		g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
}

static void
cc_bluetooth_panel_class_init (CcBluetoothPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	object_class->finalize = cc_bluetooth_panel_finalize;

	panel_class->get_help_uri = cc_bluetooth_panel_get_help_uri;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/bluetooth/cc-bluetooth-panel.ui");

	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, airplane_page);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, disabled_page);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, enable_switch);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, header_box);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, no_devices_page);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, hw_airplane_page);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, settings_widget);
	gtk_widget_class_bind_template_child (widget_class, CcBluetoothPanel, stack);

	gtk_widget_class_bind_template_callback (widget_class, adapter_status_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, airplane_mode_off_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, enable_switch_state_set_cb);
	gtk_widget_class_bind_template_callback (widget_class, panel_changed_cb);
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	bluetooth_settings_widget_get_type ();
	g_resources_register (cc_bluetooth_get_resource ());

	gtk_widget_init_template (GTK_WIDGET (self));

	/* RFKill */
	self->rfkill = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
								 G_DBUS_PROXY_FLAGS_NONE,
								 "org.gnome.SettingsDaemon.Rfkill",
								 "/org/gnome/SettingsDaemon/Rfkill",
								 "org.gnome.SettingsDaemon.Rfkill",
								 NULL, NULL);
	self->properties = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
								     G_DBUS_PROXY_FLAGS_NONE,
								     "org.gnome.SettingsDaemon.Rfkill",
								     "/org/gnome/SettingsDaemon/Rfkill",
								     "org.freedesktop.DBus.Properties",
								     NULL, NULL);

	airplane_mode_changed (self);
	g_signal_connect_object (self->rfkill, "g-properties-changed",
				 G_CALLBACK (airplane_mode_changed), self, G_CONNECT_SWAPPED);
}
