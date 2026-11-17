/*
 * Copyright © 2016 Red Hat, Inc.
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *
 */

#include "config.h"

#include <string.h>
#include "cc-wacom-device.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_DEVICE,
	N_PROPS
};

GParamSpec *props[N_PROPS] = { 0 };

typedef struct _CcWacomDevice CcWacomDevice;

struct _CcWacomDevice {
	GObject parent_instance;

	GsdDevice *device;
	WacomDevice *wdevice;
	gboolean is_fallback;
};

static void cc_wacom_device_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcWacomDevice, cc_wacom_device, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						cc_wacom_device_initable_iface_init))

WacomDeviceDatabase *
cc_wacom_device_database_get (void)
{
	static WacomDeviceDatabase *db = NULL;

	if (g_once_init_enter (&db)) {
		gpointer p = libwacom_database_new ();
		g_once_init_leave (&db, p);
	}

	return db;
}

static void
cc_wacom_device_init (CcWacomDevice *device)
{
}

static void
cc_wacom_device_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	switch (prop_id) {
	case PROP_DEVICE:
		device->device = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_device_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	switch (prop_id) {
	case PROP_DEVICE:
		g_value_set_object (value, device->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_device_finalize (GObject *object)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	g_clear_pointer (&device->wdevice, libwacom_destroy);

	G_OBJECT_CLASS (cc_wacom_device_parent_class)->finalize (object);
}

static void
cc_wacom_device_class_init (CcWacomDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = cc_wacom_device_set_property;
	object_class->get_property = cc_wacom_device_get_property;
	object_class->finalize = cc_wacom_device_finalize;

	props[PROP_DEVICE] =
		g_param_spec_object ("device",
				     "device",
				     "device",
				     GSD_TYPE_DEVICE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static gboolean
cc_wacom_device_initable_init (GInitable     *initable,
			       GCancellable  *cancellable,
			       GError       **error)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (initable);
	WacomDeviceDatabase *wacom_db;
	WacomError *wacom_error;
	const gchar *node_path;

	wacom_db = cc_wacom_device_database_get ();
	node_path = gsd_device_get_device_file (device->device);
	wacom_error = libwacom_error_new ();
	device->wdevice = libwacom_new_from_path (wacom_db, node_path, WFALLBACK_NONE, wacom_error);
	if (!device->wdevice) {
		device->wdevice = libwacom_new_from_path (wacom_db, node_path, WFALLBACK_GENERIC, wacom_error);
		device->is_fallback = TRUE;
	}

	if (!device->wdevice) {
		g_debug ("libwacom_new_from_path() failed: %s", libwacom_error_get_message (wacom_error));
		libwacom_error_free (&wacom_error);
		g_set_error (error, 0, 0, "Tablet description not found");
		return FALSE;
	}
	libwacom_error_free (&wacom_error);

	return TRUE;
}

static void
cc_wacom_device_initable_iface_init (GInitableIface *iface)
{
	iface->init = cc_wacom_device_initable_init;
}

CcWacomDevice *
cc_wacom_device_new (GsdDevice *device)
{
	return g_initable_new (CC_TYPE_WACOM_DEVICE,
			       NULL, NULL,
			       "device", device,
			       NULL);
}

CcWacomDevice *
cc_wacom_device_new_fake (const gchar *name)
{
	CcWacomDevice *device;
	WacomDevice *wacom_device;
	WacomError *wacom_error;

	device = g_object_new (CC_TYPE_WACOM_DEVICE,
			       NULL);

	wacom_error = libwacom_error_new ();
	wacom_device = libwacom_new_from_name (cc_wacom_device_database_get(),
					       name, wacom_error);
	if (wacom_device == NULL) {
		g_debug ("libwacom_new_fake() failed: %s", libwacom_error_get_message (wacom_error));
		g_clear_object (&device);
	} else {
		device->wdevice = wacom_device;
	}
	libwacom_error_free (&wacom_error);

	return device;
}

const gchar *
cc_wacom_device_get_name (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return libwacom_get_name (device->wdevice);
}

const gchar *
cc_wacom_device_get_icon_name (CcWacomDevice *device)
{
	WacomIntegrationFlags integration_flags;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	integration_flags = libwacom_get_integration_flags (device->wdevice);

	if (integration_flags & WACOM_DEVICE_INTEGRATED_SYSTEM) {
		return "wacom-tablet-pc";
	} else if (integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY) {
		return "wacom-tablet-cintiq";
	} else {
		return "wacom-tablet";
	}
}

gboolean
cc_wacom_device_is_reversible (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), FALSE);

	return libwacom_is_reversible (device->wdevice);
}

gboolean
cc_wacom_device_is_fallback (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), FALSE);

	return device->is_fallback;
}

gboolean
cc_wacom_device_is_remote (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), FALSE);

	return libwacom_get_class (device->wdevice) == WCLASS_REMOTE;
}

WacomIntegrationFlags
cc_wacom_device_get_integration_flags (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), 0);

	return libwacom_get_integration_flags (device->wdevice);
}

GsdDevice *
cc_wacom_device_get_device (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return device->device;
}

GSettings *
cc_wacom_device_get_settings (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return gsd_device_get_settings (device->device);
}

const gint *
cc_wacom_device_get_supported_tools (CcWacomDevice *device,
				     gint          *n_tools)
{
	*n_tools = 0;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return libwacom_get_supported_styli (device->wdevice, n_tools);
}

static GnomeRROutput *
find_output_by_edid (GnomeRRScreen *rr_screen,
		     const gchar   *vendor,
		     const gchar   *product,
		     const gchar   *serial,
		     const gchar   *name)
{
	GnomeRROutput **rr_outputs;
	GnomeRROutput *retval = NULL;
	guint i;

	rr_outputs = gnome_rr_screen_list_outputs (rr_screen);

	for (i = 0; rr_outputs[i] != NULL; i++) {
		g_autofree gchar *o_vendor = NULL;
		g_autofree gchar *o_product = NULL;
		g_autofree gchar *o_serial = NULL;
		const gchar *o_name;
		gboolean match;

		o_name = gnome_rr_output_get_name (rr_outputs[i]);
		gnome_rr_output_get_ids_from_edid (rr_outputs[i],
						   &o_vendor,
						   &o_product,
						   &o_serial);

		g_debug ("Checking for match between '%s','%s','%s', '%s' and '%s','%s','%s', '%s'", \
		         vendor, product, serial, name, o_vendor, o_product, o_serial, o_name);

		match = (g_strcmp0 (vendor,  o_vendor)  == 0) && \
		        (g_strcmp0 (product, o_product) == 0) && \
		        (g_strcmp0 (serial,  o_serial)  == 0) && \
		        (g_strcmp0 (name,    o_name)    == 0);

		if (match) {
			retval = rr_outputs[i];
			break;
		}
	}

	if (retval == NULL)
		g_debug ("Did not find a matching output for EDID '%s,%s,%s'",
			 vendor, product, serial);

	return retval;
}

static GnomeRROutput *
find_output (GnomeRRScreen *rr_screen,
	     CcWacomDevice *device)
{
	g_autoptr(GSettings) settings = NULL;
	g_autoptr(GVariant) variant = NULL;
	g_autofree const gchar **edid = NULL;
	const gchar *connector_name = NULL;
	gsize n;

	settings = cc_wacom_device_get_settings (device);
	variant = g_settings_get_value (settings, "output");
	edid = g_variant_get_strv (variant, &n);

	if (n < 3) {
		g_critical ("Expected 'output' key to store at least %d values; got %"G_GSIZE_FORMAT".", 3, n);
		return NULL;
	}
	if (n >= 4)
		connector_name = edid[3];

	if (strlen (edid[0]) == 0 || strlen (edid[1]) == 0 || strlen (edid[2]) == 0)
		return NULL;

	return find_output_by_edid (rr_screen, edid[0], edid[1], edid[2], connector_name);
}

GnomeRROutput *
cc_wacom_device_get_output (CcWacomDevice *device,
			    GnomeRRScreen *rr_screen)
{
	GnomeRROutput *rr_output;
	GnomeRRCrtc *crtc;

        g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);
        g_return_val_if_fail (GNOME_RR_IS_SCREEN (rr_screen), NULL);

	rr_output = find_output (rr_screen, device);
	if (rr_output == NULL) {
		return NULL;
	}

	crtc = gnome_rr_output_get_crtc (rr_output);

	if (!crtc || gnome_rr_crtc_get_current_mode (crtc) == NULL) {
		g_debug ("Output is not active.");
		return NULL;
	}

	return rr_output;
}

void
cc_wacom_device_set_output (CcWacomDevice *device,
			    GnomeRROutput *output)
{
	g_autoptr(GSettings) settings = NULL;
	g_autofree gchar *vendor = NULL;
	g_autofree gchar *product = NULL;
	g_autofree gchar *serial = NULL;
	const gchar *values[] = { "", "", "", NULL };

        g_return_if_fail (CC_IS_WACOM_DEVICE (device));

	vendor = product = serial = NULL;
	settings = cc_wacom_device_get_settings (device);

	if (output != NULL) {
		gnome_rr_output_get_ids_from_edid (output,
						   &vendor,
						   &product,
						   &serial);
		values[0] = vendor;
		values[1] = product;
		values[2] = serial;
	}

	g_settings_set_strv (settings, "output", values);
}

guint
cc_wacom_device_get_num_buttons (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), 0);

	return libwacom_get_num_buttons (device->wdevice);
}

GSettings *
cc_wacom_device_get_button_settings (CcWacomDevice *device,
				     guint          button)
{
	g_autoptr(GSettings) tablet_settings = NULL;
	GSettings *settings;
	g_autofree gchar *path = NULL;
	g_autofree gchar *button_path = NULL;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	if (button > cc_wacom_device_get_num_buttons (device))
		return NULL;

	tablet_settings = cc_wacom_device_get_settings (device);
	g_object_get (tablet_settings, "path", &path, NULL);

	button_path = g_strdup_printf ("%sbutton%c/", path, 'A' + button);
	settings = g_settings_new_with_path ("org.gnome.desktop.peripherals.tablet.pad-button",
					     button_path);

	return settings;
}

const gchar *
cc_wacom_device_get_description (CcWacomDevice *device)
{
	WacomIntegrationFlags integration_flags;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	integration_flags = libwacom_get_integration_flags (device->wdevice);

	if (integration_flags & WACOM_DEVICE_INTEGRATED_SYSTEM) {
		return _("Tablet mounted on laptop panel");
	} else if (integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY) {
		return _("Tablet mounted on external display");
	} else {
		return _("External tablet device");
	}
}
