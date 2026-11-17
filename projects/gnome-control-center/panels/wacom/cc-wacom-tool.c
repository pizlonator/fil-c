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

#include "cc-wacom-tool.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_SERIAL,
	PROP_ID,
	PROP_DEVICE,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0 };

typedef struct _CcWacomTool CcWacomTool;

struct _CcWacomTool {
	GObject parent_instance;
	guint64 serial;
	guint64 id;

	CcWacomDevice *device; /* Only set for tools with no serial */

	GSettings *settings;
	const WacomStylus *wstylus;
};

static void cc_wacom_tool_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcWacomTool, cc_wacom_tool, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						cc_wacom_tool_initable_iface_init))

static void
cc_wacom_tool_init (CcWacomTool *tool)
{
}

static void
cc_wacom_tool_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	CcWacomTool *tool = CC_WACOM_TOOL (object);

	switch (prop_id) {
	case PROP_SERIAL:
		tool->serial = g_value_get_uint64 (value);
		break;
	case PROP_ID:
		tool->id = g_value_get_uint64 (value);
		break;
	case PROP_DEVICE:
		tool->device = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_tool_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	CcWacomTool *tool = CC_WACOM_TOOL (object);

	switch (prop_id) {
	case PROP_SERIAL:
		g_value_set_uint64 (value, tool->serial);
		break;
	case PROP_ID:
		g_value_set_uint64 (value, tool->id);
		break;
	case PROP_DEVICE:
		g_value_set_object (value, tool->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_tool_finalize (GObject *object)
{
	CcWacomTool *tool = CC_WACOM_TOOL (object);

	g_clear_object (&tool->settings);

	G_OBJECT_CLASS (cc_wacom_tool_parent_class)->finalize (object);
}

static void
cc_wacom_tool_class_init (CcWacomToolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = cc_wacom_tool_set_property;
	object_class->get_property = cc_wacom_tool_get_property;
	object_class->finalize = cc_wacom_tool_finalize;

	props[PROP_SERIAL] =
		g_param_spec_uint64 ("serial",
				     "serial",
				     "serial",
				     0, G_MAXUINT64, 0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	props[PROP_ID] =
		g_param_spec_uint64 ("id",
				     "id",
				     "id",
				     0, G_MAXUINT64, 0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	props[PROP_DEVICE] =
		g_param_spec_object ("device",
				     "device",
				     "device",
				     CC_TYPE_WACOM_DEVICE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static const WacomStylus *
find_stylus_for_id (WacomDeviceDatabase  *wacom_db,
		    CcWacomTool		*tool)
{
	const WacomStylus *eraser;
	const gint *all_ids;
	gint n_supported;
	gint i;

	eraser = libwacom_stylus_get_for_id (wacom_db, tool->id);
	if (!eraser)
		return NULL;

	if (!libwacom_stylus_is_eraser (eraser) ||
	    libwacom_stylus_get_eraser_type (eraser) == WACOM_ERASER_BUTTON)
		return eraser;

	all_ids = cc_wacom_device_get_supported_tools (tool->device, &n_supported);
	for (i = 0; i < n_supported; i++) {
		const WacomStylus *stylus;
		const gint *paired_ids;
		gint n_paired;
		gint j;

		if (all_ids[i] == tool->id)
			continue;

		stylus = libwacom_stylus_get_for_id (wacom_db, all_ids[i]);
		paired_ids = libwacom_stylus_get_paired_ids (stylus, &n_paired);
		for (j = 0; j < n_paired; j++) {
			if (paired_ids[j] == tool->id) {
				return libwacom_stylus_get_for_id (wacom_db, all_ids[i]);
			}
		}
	}

	return eraser;
}

static gboolean
cc_wacom_tool_initable_init (GInitable     *initable,
			     GCancellable  *cancellable,
			     GError       **error)
{
	CcWacomTool *tool = CC_WACOM_TOOL (initable);
	WacomDeviceDatabase *wacom_db;
	gchar *path;

	wacom_db = cc_wacom_device_database_get ();

	if (tool->id == 0 && tool->device) {
		const gint *ids;
		gint n_supported;

		ids = cc_wacom_device_get_supported_tools (tool->device, &n_supported);
		if (n_supported > 0)
			tool->id = ids[0];
	}

	if (tool->id == 0) {
		tool->wstylus = libwacom_stylus_get_for_id (wacom_db, 0xfffff);
	} else {
		tool->wstylus = find_stylus_for_id (wacom_db, tool);
		if (tool->wstylus)
			tool->id = libwacom_stylus_get_id (tool->wstylus);
	}

	if (!tool->wstylus) {
		g_set_error (error, 0, 0, "Stylus description not found");
		return FALSE;
	}

	if (tool->serial == 0) {
		const gchar *vendor, *product;
		GsdDevice *gsd_device;

		gsd_device = cc_wacom_device_get_device (tool->device);
		gsd_device_get_device_ids (gsd_device, &vendor, &product);
		path = g_strdup_printf ("/org/gnome/desktop/peripherals/stylus/default-%s:%s/",
					vendor, product);
        } else {
		path = g_strdup_printf ("/org/gnome/desktop/peripherals/stylus/%lx/", tool->serial);
        }

	tool->settings = g_settings_new_with_path ("org.gnome.desktop.peripherals.tablet.stylus",
						   path);
	g_free (path);

	return TRUE;
}

static void
cc_wacom_tool_initable_iface_init (GInitableIface *iface)
{
	iface->init = cc_wacom_tool_initable_init;
}

CcWacomTool *
cc_wacom_tool_new (guint64        serial,
		   guint64        id,
		   CcWacomDevice *device)
{
	g_return_val_if_fail (serial != 0 || CC_IS_WACOM_DEVICE (device), NULL);

	return g_initable_new (CC_TYPE_WACOM_TOOL,
			       NULL, NULL,
			       "serial", serial,
			       "id", id,
			       "device", device,
			       NULL);
}

guint64
cc_wacom_tool_get_serial (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), 0);

	return tool->serial;
}

guint64
cc_wacom_tool_get_id (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), 0);

	return tool->id;
}

const gchar *
cc_wacom_tool_get_name (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), NULL);

	return libwacom_stylus_get_name (tool->wstylus);
}

static const char *
get_icon_name_from_type (const WacomStylus *wstylus)
{
	WacomStylusType type = libwacom_stylus_get_type (wstylus);

	switch (type) {
	case WSTYLUS_INKING:
	case WSTYLUS_STROKE:
		/* The stroke pen is the same as the inking pen with
		 * a different nib */
		return "wacom-stylus-inking";
	case WSTYLUS_AIRBRUSH:
		return "wacom-stylus-airbrush";
	case WSTYLUS_MARKER:
		return "wacom-stylus-art-pen";
	case WSTYLUS_CLASSIC:
		return "wacom-stylus-classic";
	case WSTYLUS_3D:
		return "wacom-stylus-3btn-no-eraser";
	case WSTYLUS_MOBILE:
		return "wacom-stylus-mobile";
	default:
		if (!libwacom_stylus_has_eraser (wstylus)) {
			if (libwacom_stylus_get_num_buttons (wstylus) >= 3)
				return "wacom-stylus-3btn-no-eraser";
			else
				return "wacom-stylus-no-eraser";
		}
		else {
			if (libwacom_stylus_get_num_buttons (wstylus) >= 3)
				return "wacom-stylus-3btn";
			else
				return "wacom-stylus";
		}
	}
}

const gchar *
cc_wacom_tool_get_icon_name (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), NULL);

	return get_icon_name_from_type (tool->wstylus);
}

GSettings *
cc_wacom_tool_get_settings (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), NULL);

	return tool->settings;
}

guint
cc_wacom_tool_get_num_buttons (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), 0);

	return libwacom_stylus_get_num_buttons (tool->wstylus);
}

gboolean
cc_wacom_tool_get_has_paired_eraser (CcWacomTool *tool)
{
	g_return_val_if_fail (CC_IS_WACOM_TOOL (tool), FALSE);

	/* True if there is some other tool that is an eraser and
	 * is physically associated with this tool */
	return libwacom_stylus_has_eraser (tool->wstylus);
}

const gchar *
cc_wacom_tool_get_description (CcWacomTool *tool)
{
	WacomAxisTypeFlags axes;

	axes = libwacom_stylus_get_axes (tool->wstylus);

	if ((~axes & (WACOM_AXIS_TYPE_TILT | WACOM_AXIS_TYPE_PRESSURE | WACOM_AXIS_TYPE_SLIDER)) == 0)
		return _("Airbrush stylus with pressure, tilt, and integrated slider");
	else if ((~axes & (WACOM_AXIS_TYPE_TILT | WACOM_AXIS_TYPE_PRESSURE | WACOM_AXIS_TYPE_ROTATION_Z)) == 0)
		return _("Art pen with pressure, tilt, and rotation");
	else if ((~axes & (WACOM_AXIS_TYPE_TILT | WACOM_AXIS_TYPE_PRESSURE)) == 0)
		return _("Stylus with pressure and tilt");
	else if ((~axes & WACOM_AXIS_TYPE_PRESSURE) == 0)
		return _("Stylus with pressure");

	return NULL;
}
