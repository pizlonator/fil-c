/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 Red Hat, Inc.
 */

#include "config.h"

#include <adwaita.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "vpn-helpers.h"

NMVpnEditorPlugin *
vpn_get_plugin_by_service (const char *service)
{
        NMVpnPluginInfo *plugin_info;

        g_return_val_if_fail (service != NULL, NULL);

        plugin_info = nm_vpn_plugin_info_list_find_by_service (vpn_get_plugins (), service);
        if (plugin_info)
                return nm_vpn_plugin_info_get_editor_plugin (plugin_info);
        return NULL;
}

static gint
_sort_vpn_plugins (NMVpnPluginInfo *aa, NMVpnPluginInfo *bb)
{
	return strcmp (nm_vpn_plugin_info_get_name (aa), nm_vpn_plugin_info_get_name (bb));
}

GSList *
vpn_get_plugins (void)
{
	static GSList *plugins = NULL;
	GSList *p;

	p = nm_vpn_plugin_info_list_load ();
	plugins = NULL;
	while (p) {
		g_autoptr(NMVpnPluginInfo) plugin_info = NM_VPN_PLUGIN_INFO (p->data);
		g_autoptr(GError) error = NULL;

		/* load the editor plugin, and preserve only those NMVpnPluginInfo that can
		 * successfully load the plugin. */
		if (nm_vpn_plugin_info_load_editor_plugin (plugin_info, &error))
			plugins = g_slist_prepend (plugins, g_steal_pointer (&plugin_info));
		else {
			if (   !nm_vpn_plugin_info_get_plugin (plugin_info)
			    && nm_vpn_plugin_info_lookup_property (plugin_info, NM_VPN_PLUGIN_INFO_KF_GROUP_GNOME, "properties")) {
				g_message ("vpn: (%s,%s) cannot load legacy-only plugin",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info));
			} else if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
				g_message ("vpn: (%s,%s) file \"%s\" not found. Did you install the client package?",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info),
				           nm_vpn_plugin_info_get_plugin (plugin_info));
			} else {
				g_warning ("vpn: (%s,%s) could not load plugin: %s",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info),
				           error->message);
			}
		}
		p = g_slist_delete_link (p, p);
	}

	/* sort the list of plugins alphabetically. */
	plugins = g_slist_sort (plugins, (GCompareFunc) _sort_vpn_plugins);
	return plugins;
}

typedef struct {
	VpnImportCallback callback;
	gpointer user_data;
	GtkWindow *parent;
} ActionInfo;

static void
import_vpn_from_file_cb (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	ActionInfo *info = (ActionInfo *) user_data;
	GtkFileDialog *dialog;
	NMConnection *connection = NULL;
	g_autoptr(GError) error = NULL;
	GSList *iter;

	dialog = GTK_FILE_DIALOG (source_object);
	file = gtk_file_dialog_open_finish (dialog, result, &error);
	if (!file) {
		g_warning ("%s: didn't get a filename back from the chooser!", __func__);

		return;
	}

	filename = g_file_get_path (file);

#if NM_CHECK_VERSION (1,40,0)
	connection = nm_conn_wireguard_import (filename, &error);
#endif

	for (iter = vpn_get_plugins (); !connection && iter; iter = iter->next) {
		NMVpnEditorPlugin *plugin;

		plugin = nm_vpn_plugin_info_get_editor_plugin (iter->data);
		g_clear_error (&error);
		connection = nm_vpn_editor_plugin_import (plugin, filename, &error);
	}

	if (!connection) {
		GtkWidget *err_dialog;
		g_autofree gchar *bname;

		bname = g_path_get_basename (filename);
		err_dialog = adw_message_dialog_new (info->parent,
											 _("Cannot Import VPN Connection"),
											 NULL);

		adw_message_dialog_format_body (ADW_MESSAGE_DIALOG (err_dialog),
										_("The file “%s” could not be read or does not contain recognized VPN connection information\n\nError: %s."),
										bname, error ? error->message : "unknown error");
		adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (err_dialog),
										 "close", _("_Close"));

		gtk_window_present (GTK_WINDOW (err_dialog));
	}

	info->callback (connection, info->user_data);
	g_free (info);
}

void
vpn_import (GtkWindow *parent, VpnImportCallback callback, gpointer user_data)
{
	g_autoptr(GFile) home_folder = NULL;
	GtkFileDialog *dialog;
	ActionInfo *info;

	dialog = gtk_file_dialog_new ();
	gtk_file_dialog_set_title (dialog, _("Select file to import"));
	gtk_file_dialog_set_modal (dialog, TRUE);
	home_folder = g_file_new_for_path (g_get_home_dir ());
	gtk_file_dialog_set_initial_folder (dialog, home_folder);

	info = g_malloc0 (sizeof (ActionInfo));
	info->callback = callback;
	info->user_data = user_data;
	info->parent = parent;

	gtk_file_dialog_open (dialog, parent, NULL, import_vpn_from_file_cb, info);
}
