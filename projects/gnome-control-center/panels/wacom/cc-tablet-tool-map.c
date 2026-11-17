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
#include "cc-tablet-tool-map.h"

#define KEY_TOOL_ID "ID"
#define KEY_DEVICE_STYLI "Styli"
#define GENERIC_STYLUS "generic"

typedef struct _CcTabletToolMap CcTabletToolMap;

struct _CcTabletToolMap {
	GObject parent_instance;
	GKeyFile *tablets;
	GKeyFile *tools;
	GHashTable *tool_map;
	GHashTable *tablet_map;
	GHashTable *no_serial_tool_map;

	gchar *tablet_path;
	gchar *tool_path;
};

G_DEFINE_TYPE (CcTabletToolMap, cc_tablet_tool_map, G_TYPE_OBJECT)

static void
load_keyfiles (CcTabletToolMap *map)
{
	g_autoptr(GError) devices_error = NULL;
	g_autoptr(GError) tools_error = NULL;
	g_autofree gchar *dir = NULL;

	dir = g_build_filename (g_get_user_cache_dir (), "gnome-control-center", "wacom", NULL);

	if (g_mkdir_with_parents (dir, 0700) < 0) {
		g_warning ("Could not create directory '%s', expect stylus mapping oddities: %m", dir);
		return;
	}

	map->tablet_path = g_build_filename (dir, "devices", NULL);
	g_key_file_load_from_file (map->tablets, map->tablet_path,
				   G_KEY_FILE_NONE, &devices_error);

	if (devices_error && !g_error_matches (devices_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
		g_warning ("Could not load tablets keyfile '%s': %s",
			   map->tablet_path, devices_error->message);
	}

	map->tool_path = g_build_filename (dir, "tools", NULL);
	g_key_file_load_from_file (map->tools, map->tool_path,
				   G_KEY_FILE_NONE, &tools_error);

	if (tools_error && !g_error_matches (tools_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
		g_warning ("Could not load tools keyfile '%s': %s",
			   map->tool_path, tools_error->message);
	}
}

static void
cache_tools (CcTabletToolMap *map)
{
	g_auto(GStrv) serials = NULL;
	gsize n_serials, i;

	serials = g_key_file_get_groups (map->tools, &n_serials);

	for (i = 0; i < n_serials; i++) {
		g_autofree gchar *str = NULL;
		gchar *end;
		guint64 serial, id;
		g_autoptr(GError) error = NULL;
		CcWacomTool *tool;

		serial = g_ascii_strtoull (serials[i], &end, 16);

		if (*end != '\0') {
			g_warning ("Invalid tool serial %s", serials[i]);
			continue;
		}

		str = g_key_file_get_string (map->tools, serials[i], KEY_TOOL_ID, &error);
		if (str == NULL) {
			g_warning ("Could not get cached ID for tool with serial %s: %s",
				   serials[i], error->message);
			continue;
		}

		id = g_ascii_strtoull (str, &end, 16);
		if (*end != '\0') {
			g_warning ("Invalid tool ID %s", str);
			continue;
		}

		tool = cc_wacom_tool_new (serial, id, NULL);
		g_hash_table_insert (map->tool_map, g_strdup (serials[i]), tool);
	}
}

static void
cache_devices (CcTabletToolMap *map)
{
	gchar **ids;
	gsize n_ids, i;

	ids = g_key_file_get_groups (map->tablets, &n_ids);

	for (i = 0; i < n_ids; i++) {
		gchar **styli;
		gsize n_styli, j;
		g_autoptr(GError) error = NULL;
		GList *tools = NULL;

		styli = g_key_file_get_string_list (map->tablets, ids[i], KEY_DEVICE_STYLI, &n_styli, &error);
		if (styli == NULL) {
			g_warning ("Could not get cached styli for with ID %s: %s",
				   ids[i], error->message);
			continue;
		}

		for (j = 0; j < n_styli; j++) {
			CcWacomTool *tool;

			if (g_str_equal (styli[j], GENERIC_STYLUS)) {
				/* We don't have a GsdDevice yet to create the
				 * serial=0 CcWacomTool, insert a NULL and defer
				 * to device lookups.
				 */
				g_hash_table_insert (map->no_serial_tool_map,
						     g_strdup (ids[i]), NULL);
			}

			tool = g_hash_table_lookup (map->tool_map, styli[j]);

			if (tool)
				tools = g_list_prepend (tools, tool);
		}

		if (tools) {
			g_hash_table_insert (map->tablet_map, g_strdup (ids[i]), tools);
		}

		g_strfreev (styli);
	}

	g_strfreev (ids);
}

static void
cc_tablet_tool_map_finalize (GObject *object)
{
	CcTabletToolMap *map = CC_TABLET_TOOL_MAP (object);

	g_key_file_unref (map->tools);
	g_key_file_unref (map->tablets);
	g_hash_table_destroy (map->tool_map);
	g_hash_table_destroy (map->tablet_map);
	g_hash_table_destroy (map->no_serial_tool_map);
	g_free (map->tablet_path);
	g_free (map->tool_path);

	G_OBJECT_CLASS (cc_tablet_tool_map_parent_class)->finalize (object);
}

static void
null_safe_unref (gpointer data)
{
	if (data != NULL)
		g_object_unref (data);
}

static void
cc_tablet_tool_map_init (CcTabletToolMap *map)
{
	map->tablets = g_key_file_new ();
	map->tools = g_key_file_new ();
	map->tool_map = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) g_object_unref);
	map->tablet_map = g_hash_table_new_full (g_str_hash, g_str_equal,
						 (GDestroyNotify) g_free,
						 (GDestroyNotify) g_list_free);
	map->no_serial_tool_map = g_hash_table_new_full (g_str_hash, g_str_equal,
							 (GDestroyNotify) g_free,
							 (GDestroyNotify) null_safe_unref);
	load_keyfiles (map);
	cache_tools (map);
	cache_devices (map);
}

static void
cc_tablet_tool_map_class_init (CcTabletToolMapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cc_tablet_tool_map_finalize;
}

CcTabletToolMap *
cc_tablet_tool_map_new (void)
{
	return g_object_new (CC_TYPE_TABLET_TOOL_MAP, NULL);
}

static gchar *
get_device_key (CcWacomDevice *device)
{
	const gchar *vendor, *product;
	GsdDevice *gsd_device;

	gsd_device = cc_wacom_device_get_device (device);
	gsd_device_get_device_ids (gsd_device, &vendor, &product);

	return g_strdup_printf ("%s:%s", vendor, product);
}

static gchar *
get_tool_key (guint64 serial)
{
	return g_strdup_printf ("%lx", serial);
}

GList *
cc_tablet_tool_map_list_tools (CcTabletToolMap *map,
			       CcWacomDevice   *device)
{
	CcWacomTool *no_serial_tool;
	GList *styli;
	g_autofree gchar *key = NULL;

	g_return_val_if_fail (CC_IS_TABLET_TOOL_MAP (map), NULL);
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	key = get_device_key (device);
	styli = g_list_copy (g_hash_table_lookup (map->tablet_map, key));

	if (g_hash_table_lookup_extended (map->no_serial_tool_map, key,
					  NULL, (gpointer) &no_serial_tool)) {
		if (!no_serial_tool) {
			no_serial_tool = cc_wacom_tool_new (0, 0, device);
			g_hash_table_replace (map->no_serial_tool_map,
					      g_strdup (key),
					      no_serial_tool);
		}

		styli = g_list_prepend (styli, no_serial_tool);
	}

	return styli;
}

CcWacomTool *
cc_tablet_tool_map_lookup_tool (CcTabletToolMap *map,
				CcWacomDevice   *device,
				guint64          serial)
{
	CcWacomTool *tool = NULL;
	g_autofree gchar *key = NULL;

	g_return_val_if_fail (CC_IS_TABLET_TOOL_MAP (map), FALSE);
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), FALSE);

	if (serial == 0) {
		key = get_device_key (device);
		tool = g_hash_table_lookup (map->no_serial_tool_map, key);
	} else {
		key = get_tool_key (serial);
		tool = g_hash_table_lookup (map->tool_map, key);
	}

	return tool;
}

static void
keyfile_add_device_stylus (CcTabletToolMap *map,
			   const gchar     *device_key,
			   const gchar     *tool_key)
{
	g_autoptr(GArray) array = NULL;
	g_auto(GStrv) styli = NULL;
	gsize n_styli;

	array = g_array_new (FALSE, FALSE, sizeof (gchar *));
	styli = g_key_file_get_string_list (map->tablets, device_key,
					    KEY_DEVICE_STYLI, &n_styli,
					    NULL);

	if (styli) {
		g_array_append_vals (array, styli, n_styli);
	}

	g_array_append_val (array, tool_key);
	g_key_file_set_string_list (map->tablets, device_key, KEY_DEVICE_STYLI,
				    (const gchar **) array->data, array->len);
}

static void
keyfile_add_stylus (CcTabletToolMap *map,
		    const gchar     *tool_key,
		    guint64          id)
{
	g_autofree gchar *str = NULL;

	/* Also works for IDs */
	str = get_tool_key (id);
	g_key_file_set_string (map->tools, tool_key, KEY_TOOL_ID, str);
}

void
cc_tablet_tool_map_add_relation (CcTabletToolMap *map,
				 CcWacomDevice   *device,
				 CcWacomTool     *tool)
{
	gboolean tablets_changed = FALSE, tools_changed = FALSE;
	gboolean new_tool_without_serial = FALSE;
	g_autofree gchar *tool_key = NULL;
	g_autofree gchar *device_key = NULL;
	guint64 serial, id;
	GList *styli;

	g_return_if_fail (CC_IS_TABLET_TOOL_MAP (map));
	g_return_if_fail (CC_IS_WACOM_DEVICE (device));
	g_return_if_fail (CC_IS_WACOM_TOOL (tool));

	serial = cc_wacom_tool_get_serial (tool);
	id = cc_wacom_tool_get_id (tool);
	device_key = get_device_key (device);

	if (serial == 0) {
		tool_key = g_strdup (GENERIC_STYLUS);

		if (!g_hash_table_contains (map->no_serial_tool_map, device_key)) {
			g_hash_table_insert (map->no_serial_tool_map,
					     g_strdup (device_key),
					     g_object_ref (tool));
			new_tool_without_serial = TRUE;
		}
	} else {
		tool_key = get_tool_key (serial);

		if (!g_hash_table_contains (map->tool_map, tool_key)) {
			keyfile_add_stylus (map, tool_key, id);
			tools_changed = TRUE;
			g_hash_table_insert (map->tool_map,
					     g_strdup (tool_key),
					     g_object_ref (tool));
		}
	}

	styli = g_hash_table_lookup (map->tablet_map, device_key);

	if (!g_list_find (styli, tool)) {
		styli = g_list_prepend (styli, tool);
		g_hash_table_replace (map->tablet_map,
				      g_strdup (device_key),
				      g_list_copy (styli));

		if (serial || new_tool_without_serial) {
			tablets_changed = TRUE;
			keyfile_add_device_stylus (map, device_key, tool_key);
		}
	}

	if (tools_changed) {
		g_autoptr(GError) error = NULL;

		if (!g_key_file_save_to_file (map->tools, map->tool_path, &error)) {
			g_warning ("Error saving tools keyfile: %s",
				   error->message);
		}
	}

	if (tablets_changed) {
		g_autoptr(GError) error = NULL;

		if (!g_key_file_save_to_file (map->tablets, map->tablet_path, &error)) {
			g_warning ("Error saving tablets keyfile: %s",
				   error->message);
		}
	}
}
