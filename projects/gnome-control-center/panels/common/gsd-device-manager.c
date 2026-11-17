/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <string.h>
#include <gudev/gudev.h>

#include "gsd-device-manager.h"
#include "gsd-common-enums.h"
#include "gnome-settings-bus.h"
#include "gsd-input-helper.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/extensions/XInput2.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

typedef struct
{
	gchar *name;
	gchar *device_file;
	gchar *vendor_id;
	gchar *product_id;
	gchar *group;
	GsdDeviceType type;
	guint width;
	guint height;
} GsdDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GsdDevice, gsd_device, G_TYPE_OBJECT)

typedef struct
{
        GObject parent_instance;
	GHashTable *devices;
	GUdevClient *udev_client;
} GsdDeviceManagerPrivate;

enum {
	PROP_NAME = 1,
	PROP_DEVICE_FILE,
	PROP_VENDOR_ID,
	PROP_PRODUCT_ID,
	PROP_TYPE,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_GROUP
};

enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	DEVICE_CHANGED,
	N_SIGNALS
};

/* Index matches GsdDeviceType */
const gchar *udev_ids[] = {
	"ID_INPUT_MOUSE",
	"ID_INPUT_KEYBOARD",
	"ID_INPUT_TOUCHPAD",
	"ID_INPUT_TABLET",
	"ID_INPUT_TOUCHSCREEN",
	"ID_INPUT_TABLET_PAD",
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GsdDeviceManager, gsd_device_manager, G_TYPE_OBJECT)

static void
gsd_device_init (GsdDevice *device)
{
}

static void
gsd_device_set_property (GObject      *object,
			 guint	       prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
	GsdDevicePrivate *priv;

	priv = gsd_device_get_instance_private (GSD_DEVICE (object));

	switch (prop_id) {
	case PROP_NAME:
		priv->name = g_value_dup_string (value);
		break;
	case PROP_DEVICE_FILE:
		priv->device_file = g_value_dup_string (value);
		break;
	case PROP_VENDOR_ID:
		priv->vendor_id = g_value_dup_string (value);
		break;
	case PROP_PRODUCT_ID:
		priv->product_id = g_value_dup_string (value);
		break;
	case PROP_TYPE:
		priv->type = g_value_get_flags (value);
		break;
	case PROP_WIDTH:
		priv->width = g_value_get_uint (value);
		break;
	case PROP_HEIGHT:
		priv->height = g_value_get_uint (value);
		break;
	case PROP_GROUP:
		priv->group = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gsd_device_get_property (GObject    *object,
			 guint	     prop_id,
			 GValue	    *value,
			 GParamSpec *pspec)
{
	GsdDevicePrivate *priv;

	priv = gsd_device_get_instance_private (GSD_DEVICE (object));

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_DEVICE_FILE:
		g_value_set_string (value, priv->device_file);
		break;
	case PROP_VENDOR_ID:
		g_value_set_string (value, priv->vendor_id);
		break;
	case PROP_PRODUCT_ID:
		g_value_set_string (value, priv->product_id);
		break;
	case PROP_TYPE:
		g_value_set_flags (value, priv->type);
		break;
	case PROP_WIDTH:
		g_value_set_uint (value, priv->width);
		break;
	case PROP_HEIGHT:
		g_value_set_uint (value, priv->height);
		break;
	case PROP_GROUP:
		g_value_set_string (value, priv->group);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gsd_device_finalize (GObject *object)
{
	GsdDevicePrivate *priv;

	priv = gsd_device_get_instance_private (GSD_DEVICE (object));

	g_free (priv->name);
	g_free (priv->vendor_id);
	g_free (priv->product_id);
	g_free (priv->device_file);
	g_free (priv->group);

	G_OBJECT_CLASS (gsd_device_parent_class)->finalize (object);
}

static void
gsd_device_class_init (GsdDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gsd_device_set_property;
	object_class->get_property = gsd_device_get_property;
	object_class->finalize = gsd_device_finalize;

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Name",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_DEVICE_FILE,
					 g_param_spec_string ("device-file",
							      "Device file",
							      "Device file",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_VENDOR_ID,
					 g_param_spec_string ("vendor-id",
							      "Vendor ID",
							      "Vendor ID",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_PRODUCT_ID,
					 g_param_spec_string ("product-id",
							      "Product ID",
							      "Product ID",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_flags ("type",
							     "Device type",
							     "Device type",
							     GSD_TYPE_DEVICE_TYPE, 0,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_WIDTH,
					 g_param_spec_uint ("width",
							    "Width",
							    "Width",
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE |
							    G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_HEIGHT,
					 g_param_spec_uint ("height",
							    "Height",
							    "Height",
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE |
							    G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_GROUP,
					 g_param_spec_string ("group",
							      "Group",
							      "Group",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
gsd_device_manager_finalize (GObject *object)
{
	GsdDeviceManager *manager = GSD_DEVICE_MANAGER (object);
        GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);

	g_hash_table_destroy (priv->devices);
	g_object_unref (priv->udev_client);

	G_OBJECT_CLASS (gsd_device_manager_parent_class)->finalize (object);
}

static GList *
gsd_device_manager_real_list_devices (GsdDeviceManager *manager,
				      GsdDeviceType	type)
{
        GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);
	GsdDeviceType device_type;
	GList *devices = NULL;
	GHashTableIter iter;
	GsdDevice *device;

	g_hash_table_iter_init (&iter, priv->devices);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &device)) {
		device_type = gsd_device_get_device_type (device);

		if ((device_type & type) == type)
			devices = g_list_prepend (devices, device);
	}

	return devices;
}

static GsdDevice *
gsd_device_manager_real_lookup_device (GsdDeviceManager *manager,
                                       GdkDevice	*gdk_device)
{
	GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);
	GdkDisplay *display = gdk_device_get_display (gdk_device);
	const gchar *node_path = NULL;
	GHashTableIter iter;
	GsdDevice *device;

#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (display)) {
                XIDeviceInfo *info;
                int n_infos, i, source_id = 0;

                gdk_x11_display_error_trap_push (display);
                info = XIQueryDevice (gdk_x11_display_get_xdisplay (display),
                                      gdk_x11_device_get_id (gdk_device),
                                      &n_infos);
                if (gdk_x11_display_error_trap_pop (display) != 0)
                        return NULL;
                if (!info || n_infos != 1)
                        return NULL;

                for (i = 0; i < info->num_classes; i++) {
                        if (info->classes[i]->type == XIValuatorClass)
                                source_id = info->classes[i]->sourceid;
                }

                if (source_id != 0)
                        node_path = xdevice_get_device_node (source_id);
        }
#endif
#ifdef GDK_WINDOWING_WAYLAND
	if (GDK_IS_WAYLAND_DISPLAY (display))
		node_path = g_strdup (gdk_wayland_device_get_node_path (gdk_device));
#endif
	if (!node_path)
		return NULL;

	g_hash_table_iter_init (&iter, priv->devices);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &device)) {
		if (g_strcmp0 (node_path,
			       gsd_device_get_device_file (device)) == 0) {
			return device;
		}
	}

	return NULL;
}

static void
gsd_device_manager_class_init (GsdDeviceManagerClass *klass)
{
	GsdDeviceManagerClass *manager_class = GSD_DEVICE_MANAGER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gsd_device_manager_finalize;
	manager_class->list_devices = gsd_device_manager_real_list_devices;
	manager_class->lookup_device = gsd_device_manager_real_lookup_device;

	/* Fil-C: GType is a pointer in the pizlonated glib, so the
	 * type-flag ORs below need (GType)((uintptr_t)A | (uintptr_t)B). */
	signals[DEVICE_ADDED] =
		g_signal_new ("device-added",
			      GSD_TYPE_DEVICE_MANAGER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsdDeviceManagerClass, device_added),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 1,
			      (GType)((uintptr_t)GSD_TYPE_DEVICE | (uintptr_t)G_SIGNAL_TYPE_STATIC_SCOPE));

	signals[DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      GSD_TYPE_DEVICE_MANAGER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsdDeviceManagerClass, device_removed),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 1,
			      (GType)((uintptr_t)GSD_TYPE_DEVICE | (uintptr_t)G_SIGNAL_TYPE_STATIC_SCOPE));

	signals[DEVICE_CHANGED] =
		g_signal_new ("device-changed",
			      GSD_TYPE_DEVICE_MANAGER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsdDeviceManagerClass, device_changed),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 1,
			      (GType)((uintptr_t)GSD_TYPE_DEVICE | (uintptr_t)G_SIGNAL_TYPE_STATIC_SCOPE));
}

static GsdDeviceType
udev_device_get_device_type (GUdevDevice *device)
{
	GsdDeviceType type = 0;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (udev_ids); i++) {
		if (g_udev_device_get_property_as_boolean (device, udev_ids[i]))
			type |= (1 << i);
	}

	return type;
}

static gboolean
device_is_evdev (GUdevDevice *device)
{
	const gchar *device_file;

	device_file = g_udev_device_get_device_file (device);

	if (!device_file || strstr (device_file, "/event") == NULL)
		return FALSE;

	return g_udev_device_get_property_as_boolean (device, "ID_INPUT");
}

static GsdDevice *
create_device (GUdevDevice *udev_device, GUdevDevice *parent)
{
	const gchar *vendor, *product, *name, *group;
	guint width, height;

	name = g_udev_device_get_sysfs_attr (parent, "name");
	vendor = g_udev_device_get_property (udev_device, "ID_VENDOR_ID");
	product = g_udev_device_get_property (udev_device, "ID_MODEL_ID");

	if (!vendor || !product) {
		vendor = g_udev_device_get_sysfs_attr (udev_device, "device/id/vendor");
		product = g_udev_device_get_sysfs_attr (udev_device, "device/id/product");
	}

	width = g_udev_device_get_property_as_int (udev_device, "ID_INPUT_WIDTH_MM");
	height = g_udev_device_get_property_as_int (udev_device, "ID_INPUT_HEIGHT_MM");

	group = g_udev_device_get_property (udev_device, "LIBINPUT_DEVICE_GROUP");

	return g_object_new (GSD_TYPE_DEVICE,
			     "name", name,
			     "device-file", g_udev_device_get_device_file (udev_device),
			     "type", udev_device_get_device_type (udev_device),
			     "vendor-id", vendor,
			     "product-id", product,
			     "width", width,
			     "height", height,
			     "group", group,
			     NULL);
}

static void
add_device (GsdDeviceManager *manager,
	    GUdevDevice	     *udev_device)
{
        GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);
	g_autoptr(GUdevDevice) parent = NULL;
	GsdDevice *device;
	const gchar *syspath;

	parent = g_udev_device_get_parent (udev_device);

	if (!parent)
		return;

	device = create_device (udev_device, parent);
	syspath = g_udev_device_get_sysfs_path (udev_device);
	g_hash_table_insert (priv->devices, g_strdup (syspath), device);
	g_signal_emit_by_name (manager, "device-added", device);
}

static void
remove_device (GsdDeviceManager *manager,
	       GUdevDevice	*udev_device)
{
	GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);
	GsdDevice *device;
	const gchar *syspath;

	syspath = g_udev_device_get_sysfs_path (udev_device);
	device = g_hash_table_lookup (priv->devices, syspath);

	if (!device)
		return;

	g_hash_table_steal (priv->devices, syspath);
	g_signal_emit_by_name (manager, "device-removed", device);

	g_object_unref (device);
}

static void
udev_event_cb (GUdevClient	*client,
	       gchar		*action,
	       GUdevDevice	*device,
	       GsdDeviceManager *manager)
{
	if (!device_is_evdev (device))
		return;

	if (g_strcmp0 (action, "add") == 0) {
		add_device (manager, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		remove_device (manager, device);
	}
}

static void
gsd_device_manager_init (GsdDeviceManager *manager)
{
        GsdDeviceManagerPrivate *priv = gsd_device_manager_get_instance_private (manager);
	const gchar *subsystems[] = { "input", NULL };
	g_autoptr(GList) devices = NULL;
	GList *l;

	priv->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) g_object_unref);

	priv->udev_client = g_udev_client_new (subsystems);
	g_signal_connect (priv->udev_client, "uevent",
			  G_CALLBACK (udev_event_cb), manager);

	devices = g_udev_client_query_by_subsystem (priv->udev_client,
						    subsystems[0]);

	for (l = devices; l; l = l->next) {
		g_autoptr(GUdevDevice) device = l->data;

		if (device_is_evdev (device))
			add_device (manager, device);
	}
}

GsdDeviceManager *
gsd_device_manager_get (void)
{
	GsdDeviceManager *manager;
	GdkDisplay *display;

	display = gdk_display_get_default ();
	g_return_val_if_fail (display != NULL, NULL);

	manager = g_object_get_data (G_OBJECT (display), "gsd-device-manager-data");

	if (!manager) {
                manager = g_object_new (GSD_TYPE_DEVICE_MANAGER,
                                        NULL);

		g_object_set_data_full (G_OBJECT (display), "gsd-device-manager-data",
					manager, (GDestroyNotify) g_object_unref);
	}

	return manager;
}

GList *
gsd_device_manager_list_devices (GsdDeviceManager *manager,
				 GsdDeviceType	   type)
{
	g_return_val_if_fail (GSD_IS_DEVICE_MANAGER (manager), NULL);

	return GSD_DEVICE_MANAGER_GET_CLASS (manager)->list_devices (manager, type);
}

GsdDeviceType
gsd_device_get_device_type (GsdDevice *device)
{
	GsdDevicePrivate *priv;

	g_return_val_if_fail (GSD_IS_DEVICE (device), 0);

	priv = gsd_device_get_instance_private (device);

	return priv->type;
}

void
gsd_device_get_device_ids (GsdDevice	*device,
			   const gchar **vendor,
			   const gchar **product)
{
	GsdDevicePrivate *priv;

	g_return_if_fail (GSD_IS_DEVICE (device));

	priv = gsd_device_get_instance_private (device);

	if (vendor)
		*vendor = priv->vendor_id;
	if (product)
		*product = priv->product_id;
}

GSettings *
gsd_device_get_settings (GsdDevice *device)
{
	const gchar *schema = NULL, *vendor, *product;
	GsdDeviceType type;
	g_autofree gchar *path = NULL;

	g_return_val_if_fail (GSD_IS_DEVICE (device), NULL);

	type = gsd_device_get_device_type (device);

	if (type & (GSD_DEVICE_TYPE_TOUCHSCREEN | GSD_DEVICE_TYPE_TABLET)) {
		gsd_device_get_device_ids (device, &vendor, &product);

		if (type & GSD_DEVICE_TYPE_TOUCHSCREEN) {
			schema = "org.gnome.desktop.peripherals.touchscreen";
			path = g_strdup_printf ("/org/gnome/desktop/peripherals/touchscreens/%s:%s/",
						vendor, product);
		} else if (type & GSD_DEVICE_TYPE_TABLET) {
			schema = "org.gnome.desktop.peripherals.tablet";
			path = g_strdup_printf ("/org/gnome/desktop/peripherals/tablets/%s:%s/",
						vendor, product);
		}
	} else if (type & (GSD_DEVICE_TYPE_MOUSE | GSD_DEVICE_TYPE_TOUCHPAD)) {
		schema = "org.gnome.desktop.peripherals.mouse";
	} else if (type & GSD_DEVICE_TYPE_KEYBOARD) {
		schema = "org.gnome.desktop.peripherals.keyboard";
	} else {
		return NULL;
	}

	if (path) {
		return g_settings_new_with_path (schema, path);
	} else {
		return g_settings_new (schema);
	}
}

const gchar *
gsd_device_get_name (GsdDevice *device)
{
	GsdDevicePrivate *priv;

	g_return_val_if_fail (GSD_IS_DEVICE (device), NULL);

	priv = gsd_device_get_instance_private (device);

	return priv->name;
}

const gchar *
gsd_device_get_device_file (GsdDevice *device)
{
	GsdDevicePrivate *priv;

	g_return_val_if_fail (GSD_IS_DEVICE (device), NULL);

	priv = gsd_device_get_instance_private (device);

	return priv->device_file;
}

gboolean
gsd_device_get_dimensions (GsdDevice *device,
			   guint     *width,
			   guint     *height)
{
	GsdDevicePrivate *priv;

	g_return_val_if_fail (GSD_IS_DEVICE (device), FALSE);

	priv = gsd_device_get_instance_private (device);

	if (width)
		*width = priv->width;
	if (height)
		*height = priv->height;

	return priv->width > 0 && priv->height > 0;
}

GsdDevice *
gsd_device_manager_lookup_gdk_device (GsdDeviceManager *manager,
				      GdkDevice	       *gdk_device)
{
	GsdDeviceManagerClass *klass;

	g_return_val_if_fail (GSD_IS_DEVICE_MANAGER (manager), NULL);
	g_return_val_if_fail (GDK_IS_DEVICE (gdk_device), NULL);

	klass = GSD_DEVICE_MANAGER_GET_CLASS (manager);
	if (!klass->lookup_device)
		return NULL;

	return klass->lookup_device (manager, gdk_device);
}

gboolean
gsd_device_shares_group (GsdDevice *device1,
			 GsdDevice *device2)
{
	GsdDevicePrivate *priv1, *priv2;

	priv1 = gsd_device_get_instance_private (GSD_DEVICE (device1));
	priv2 = gsd_device_get_instance_private (GSD_DEVICE (device2));

	/* Don't group NULLs together */
	if (!priv1->group && !priv2->group)
		return FALSE;

	return g_strcmp0 (priv1->group, priv2->group) == 0;
}
