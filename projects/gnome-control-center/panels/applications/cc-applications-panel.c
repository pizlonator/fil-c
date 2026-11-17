/* cc-applications-panel.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-applications-panel"

#include <config.h>
#include <glib/gi18n.h>
#ifdef HAVE_MALCONTENT
#include <libmalcontent/malcontent.h>
#endif

#include <gio/gdesktopappinfo.h>

#include "cc-applications-panel.h"
#include "cc-applications-row.h"
#include "cc-info-row.h"
#include "cc-default-apps-page.h"
#include "cc-removable-media-settings.h"
#include "cc-applications-resources.h"
#ifdef HAVE_SNAP
#include "cc-snapd-client.h"
#include "cc-snap-row.h"
#endif
#include "cc-util.h"
#include "globs.h"
#include "search.h"
#include "utils.h"

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"

#define PORTAL_SNAP_PREFIX "snap."

struct _CcApplicationsPanel
{
  CcPanel          parent;

  CcDefaultAppsPage        *default_apps_page;
  AdwSwitchRow             *autorun_never_row;
  CcRemovableMediaSettings *removable_media_settings;

  AdwNavigationView *navigation_view;
  AdwNavigationPage *app_settings_page;
  GtkListBox      *app_listbox;
  GtkEntry        *app_search_entry;
  GtkWidget       *no_apps_page;
  GtkStack        *app_listbox_stack;
  GAppInfoMonitor *monitor;
  gulong           monitor_id;
  GListModel      *app_model;
  GListModel      *filter_model;
  GtkFilter       *filter;
#ifdef HAVE_MALCONTENT
  GCancellable    *cancellable;

  MctAppFilter    *app_filter;
  MctManager      *manager;
  guint            app_filter_id;
#endif

  gchar           *current_app_id;
  GAppInfo        *current_app_info;
  gchar           *current_portal_app_id;

  GHashTable      *globs;
  GHashTable      *search_providers;

  GtkImage        *app_icon_image;
  GtkLabel        *app_name_label;
  GtkButton       *launch_button;
  GtkButton       *view_details_button;
  AdwBanner       *sandbox_banner;
  GtkWidget       *sandbox_info_button;

  GDBusProxy      *perm_store;
  GSettings       *media_handling_settings;
  GtkListBoxRow   *perm_store_pending_row;
  GSettings       *notification_settings;
  GSettings       *location_settings;
  GSettings       *privacy_settings;
  GSettings       *search_settings;

  GtkButton       *install_button;

  AdwPreferencesGroup *integration_section;
  AdwSwitchRow    *notification;
  AdwSwitchRow    *background;
  AdwSwitchRow    *wallpaper;
  AdwSwitchRow    *screenshot;
  AdwSwitchRow    *sound;
  CcInfoRow       *no_sound;
  AdwSwitchRow    *search;
  CcInfoRow       *no_search;
  AdwSwitchRow    *camera;
  CcInfoRow       *no_camera;
  AdwSwitchRow    *location;
  CcInfoRow       *no_location;
  AdwSwitchRow    *shortcuts;
  AdwSwitchRow    *microphone;
  CcInfoRow       *no_microphone;
  AdwPreferencesGroup *other_permissions_section;
  CcInfoRow       *builtin;
  GtkWindow       *builtin_dialog;
  AdwPreferencesPage *builtin_page;
  GtkListBox      *builtin_list;
  GList           *snap_permission_rows;

  GtkButton       *handler_reset;
  GtkWindow       *handler_dialog;
  AdwPreferencesPage *handler_page;
  CcInfoRow       *handler_row;
  AdwPreferencesGroup *handler_file_group;
  AdwPreferencesGroup *handler_link_group;
  GList           *file_handler_rows;
  GList           *link_handler_rows;

  GtkWidget       *usage_section;
  CcInfoRow       *storage;
  GtkWindow       *storage_dialog;
  CcInfoRow       *app;
  CcInfoRow       *data;
  CcInfoRow       *cache;
  CcInfoRow       *total;
  GtkButton       *clear_cache_button;

  guint64          app_size;
  guint64          cache_size;
  guint64          data_size;
};

static void select_app (CcApplicationsPanel *self,
                        const gchar         *app_id,
                        gboolean             emit_activate);

static void update_handler_dialog (CcApplicationsPanel *self, GAppInfo *info);

G_DEFINE_TYPE (CcApplicationsPanel, cc_applications_panel, CC_TYPE_PANEL)

enum
{
  PROP_0,
  PROP_PARAMETERS
};

static gboolean
gnome_software_is_installed (void)
{
  g_autofree gchar *path = g_find_program_in_path ("gnome-software");
  return path != NULL;
}

/* Callbacks */

static gboolean
privacy_link_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));
  g_autoptr(GError) error = NULL;

  if (!cc_shell_set_active_panel_from_id (shell, "location", NULL, &error))
    g_warning ("Failed to switch to privacy panel: %s", error->message);

  return TRUE;
}

static void
open_software_cb (CcApplicationsPanel *self)
{
  const gchar *argv[] = { "gnome-software", "--details", "appid", NULL };
  g_autofree gchar *argv_app_id = NULL;

  if (self->current_app_id == NULL)
    argv[1] = NULL;
  else if (g_str_has_prefix (self->current_app_id, "org.gnome.Epiphany.WebApp_"))
    /* GNOME Software only shows info on the webapp desktop file itself */
    argv_app_id = g_strdup_printf ("%s.desktop", self->current_app_id);
  else
    argv_app_id = g_strdup (self->current_app_id);

  argv[2] = argv_app_id;

  g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

/* --- portal permissions and utilities --- */

static gchar **
get_portal_permissions (CcApplicationsPanel *self,
                        const gchar         *table,
                        const gchar         *id,
                        const gchar         *app_id)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  const gchar *key = NULL;
  GStrv val;
  GStrv result = NULL;

  ret = g_dbus_proxy_call_sync (self->perm_store,
                                "Lookup",
                                g_variant_new ("(ss)", table, id),
                                0, G_MAXINT, NULL, NULL);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(a{sas}v)", &iter, NULL);

  while (g_variant_iter_loop (iter, "{&s^a&s}", &key, &val))
    {
      if (strcmp (key, app_id) == 0 && result == NULL)
        result = g_strdupv (val);
    }

  return result;
}

static void
set_portal_permissions (CcApplicationsPanel *self,
                        const gchar *table,
                        const gchar *id,
                        const gchar *app_id,
                        const gchar * const *permissions)
{
  g_autoptr(GError) error = NULL;

  g_dbus_proxy_call_sync (self->perm_store,
                          "SetPermission",
                          g_variant_new ("(sbss^as)", table, TRUE, id, app_id, permissions),
                          0,
                          G_MAXINT,
                          NULL,
                          &error);
  if (error)
    g_warning ("Error setting portal permissions: %s", error->message);
}

static gchar *
get_portal_app_id (GAppInfo *info)
{
  if (G_IS_DESKTOP_APP_INFO (info))
    {
      g_autofree gchar *snap_name = NULL;
      gchar *flatpak_id;

      flatpak_id = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (info), "X-Flatpak");
      if (flatpak_id != NULL)
        return flatpak_id;

      snap_name = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (info), "X-SnapInstanceName");
      if (snap_name != NULL)
        return g_strdup_printf ("%s%s", PORTAL_SNAP_PREFIX, snap_name);
    }

  return NULL;
}

static GFile *
get_flatpak_app_dir (const gchar *app_id,
                     const gchar *subdir)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) appdir = NULL;

  path = g_build_filename (g_get_home_dir (), ".var", "app", app_id, NULL);
  appdir = g_file_new_for_path (path);

  return g_file_get_child (appdir, subdir);
}

/* --- search settings --- */

static void
set_search_enabled (CcApplicationsPanel *self,
                    const gchar         *app_id,
                    gboolean             enabled)
{
  g_autoptr(GPtrArray) new_apps = NULL;
  g_autofree gchar *desktop_id = NULL;
  g_auto(GStrv) apps = NULL;
  gpointer key, value;
  gboolean default_disabled;
  gint i;

  desktop_id = g_strconcat (app_id, ".desktop", NULL);

  if (!g_hash_table_lookup_extended (self->search_providers, app_id, &key, &value))
    {
      g_warning ("Trying to configure search for a provider-less app - this shouldn't happen");
      return;
    }

  default_disabled = GPOINTER_TO_INT (value);

  new_apps = g_ptr_array_new_with_free_func (g_free);
  if (default_disabled)
    {
      apps = g_settings_get_strv (self->search_settings, "enabled");
      for (i = 0; apps[i]; i++)
        {
          if (strcmp (apps[i], desktop_id) != 0)
            g_ptr_array_add (new_apps, g_strdup (apps[i]));
        }
      if (enabled)
        g_ptr_array_add (new_apps, g_strdup (desktop_id));
      g_ptr_array_add (new_apps, NULL);
      g_settings_set_strv (self->search_settings, "enabled",  (const gchar * const *)new_apps->pdata);
    }
  else
    {
      apps = g_settings_get_strv (self->search_settings, "disabled");
      for (i = 0; apps[i]; i++)
        {
          if (strcmp (apps[i], desktop_id) != 0)
            g_ptr_array_add (new_apps, g_strdup (apps[i]));
        }
      if (!enabled)
        g_ptr_array_add (new_apps, g_strdup (desktop_id));
      g_ptr_array_add (new_apps, NULL);
      g_settings_set_strv (self->search_settings, "disabled", (const gchar * const *)new_apps->pdata);
    }
}

static gboolean
search_contains_string_for_app (CcApplicationsPanel *self,
                                const gchar         *app_id,
                                const gchar         *setting)
{
  g_autofree gchar *desktop_id = NULL;
  g_auto(GStrv) apps = NULL;

  desktop_id = g_strconcat (app_id, ".desktop", NULL);
  apps = g_settings_get_strv (self->search_settings, setting);

  return g_strv_contains ((const gchar * const *)apps, desktop_id);
}

static gboolean
search_enabled_for_app (CcApplicationsPanel *self,
                        const gchar         *app_id)
{
  return search_contains_string_for_app (self, app_id, "enabled");
}

static gboolean
search_disabled_for_app (CcApplicationsPanel *self,
                         const gchar         *app_id)
{
  return search_contains_string_for_app (self, app_id, "disabled");
}

static void
get_search_enabled (CcApplicationsPanel *self,
                    const gchar         *app_id,
                    gboolean            *set,
                    gboolean            *enabled)
{
  gpointer key, value;

  *enabled = FALSE;
  *set = g_hash_table_lookup_extended (self->search_providers, app_id, &key, &value);
  if (!*set)
    return;

  if (search_enabled_for_app (self, app_id))
    *enabled = TRUE;
  else if (search_disabled_for_app (self, app_id))
    *enabled = FALSE;
  else
    *enabled = !GPOINTER_TO_INT (value);
}

static void
search_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_search_enabled (self,
                        self->current_app_id,
                        adw_switch_row_get_active (self->search));
}

/* --- notification permissions (flatpaks and non-flatpak) --- */

static void
get_notification_allowed (CcApplicationsPanel *self,
                          const gchar         *app_id,
                          gboolean            *set,
                          gboolean            *allowed)
{
  if (self->notification_settings)
    {
      /* FIXME */
      *set = TRUE;
      *allowed = g_settings_get_boolean (self->notification_settings, "enable");
    }
  else
    {
      g_auto(GStrv) perms = get_portal_permissions (self, "notifications", "notification", app_id);
      *set = perms != NULL;
      /* FIXME: needs unreleased xdg-desktop-portals to write permissions on use */
      *set = TRUE;
      *allowed = perms == NULL || g_strcmp0 (perms[0], "no") != 0;
    }
}

static void
set_notification_allowed (CcApplicationsPanel *self,
                          gboolean             allowed)
{
  if (self->notification_settings)
    {
      g_settings_set_boolean (self->notification_settings, "enable", allowed);
    }
  else
    {
      const gchar *perms[2] = { NULL, NULL };

      perms[0] = allowed ? "yes" : "no";
      set_portal_permissions (self, "notifications", "notification", self->current_portal_app_id, perms);
    }
}

static void
notification_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_notification_allowed (self, adw_switch_row_get_active (self->notification));
}

static gchar *
munge_app_id (const gchar *app_id)
{
  gchar *id = g_strdup (app_id);
  gint i;

  g_strcanon (id,
              "0123456789"
              "abcdefghijklmnopqrstuvwxyz"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "-",
              '-');
  for (i = 0; id[i] != '\0'; i++)
    id[i] = g_ascii_tolower (id[i]);

  return id;
}

static GSettings *
get_notification_settings (const gchar *app_id)
{
  g_autofree gchar *munged_app_id = munge_app_id (app_id);
  g_autofree gchar *path = g_strconcat (APP_PREFIX, munged_app_id, "/", NULL);
  return g_settings_new_with_path (APP_SCHEMA, path);
}


/* --- background --- */

static void
get_background_allowed (CcApplicationsPanel *self,
                        const gchar         *app_id,
                        gboolean            *set,
                        gboolean            *allowed)
{
  g_auto(GStrv) perms = get_portal_permissions (self, "background", "background", app_id);
  *set = TRUE;
  *allowed = perms == NULL || g_strcmp0 (perms[0], "no") != 0;
}

static void
set_background_allowed (CcApplicationsPanel *self,
                        gboolean             allowed)
{
  const gchar *perms[2] = { NULL, NULL };

  perms[0] = allowed ? "yes" : "no";
  set_portal_permissions (self, "background", "background", self->current_portal_app_id, perms);
}

static void
background_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_background_allowed (self, adw_switch_row_get_active (self->background));
}

/* --- wallpaper --- */

static void
get_wallpaper_allowed (CcApplicationsPanel *self,
                       const gchar         *app_id,
                       gboolean            *set,
                       gboolean            *allowed)
{
  g_auto(GStrv) perms = get_portal_permissions (self, "wallpaper", "wallpaper", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || g_strcmp0 (perms[0], "no") != 0;
}

static void
set_wallpaper_allowed (CcApplicationsPanel *self,
                       gboolean             allowed)
{
  const gchar *perms[2] = { NULL, NULL };

  perms[0] = allowed ? "yes" : "no";
  set_portal_permissions (self, "wallpaper", "wallpaper", self->current_app_id, perms);
}

static void
wallpaper_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_wallpaper_allowed (self, adw_switch_row_get_active (self->wallpaper));
}

/* --- screenshot --- */

static void
get_screenshot_allowed (CcApplicationsPanel *self,
                        const gchar         *app_id,
                        gboolean            *set,
                        gboolean            *allowed)
{
  g_auto(GStrv) perms = get_portal_permissions (self, "screenshot", "screenshot", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || g_strcmp0 (perms[0], "no") != 0;
}

static void
set_screenshot_allowed (CcApplicationsPanel *self,
                        gboolean             allowed)
{
  const gchar *perms[2] = { NULL, NULL };

  perms[0] = allowed ? "yes" : "no";
  set_portal_permissions (self, "screenshot", "screenshot", self->current_app_id, perms);
}

static void
screenshot_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_screenshot_allowed (self, adw_switch_row_get_active (self->screenshot));
}

/* --- shortcuts permissions (flatpak) --- */

static void
get_shortcuts_allowed (CcApplicationsPanel *self,
                       const gchar         *app_id,
                       gboolean            *set,
                       gboolean            *granted)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "gnome", "shortcuts-inhibitor", app_id);

  /* GNOME Shell's "inhibit shortcut dialog" sets the permission to "GRANTED" if
   * the user allowed for the keyboard shortcuts to be inhibited, check for that
   * string value here.
   */
  *set = perms != NULL;
  *granted = (perms != NULL) && (perms[0] != NULL) && g_ascii_strcasecmp (perms[0], "GRANTED") == 0;
}

static void
set_shortcuts_allowed (CcApplicationsPanel *self,
                       gboolean             granted)
{
  const gchar *perms[2];
  g_autofree gchar *desktop_id = g_strconcat (self->current_app_id, ".desktop", NULL);

  /* "GRANTED" and "DENIED" here match the values set by the "inhibit shortcut
   * dialog" is GNOME Shell:
   * https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/js/ui/inhibitShortcutsDialog.js
   */
  perms[0] = granted ? "GRANTED" : "DENIED";
  perms[1] = NULL;

  set_portal_permissions (self, "gnome", "shortcuts-inhibitor", desktop_id, perms);
}

static void
shortcuts_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_shortcuts_allowed (self, adw_switch_row_get_active (self->shortcuts));
}

/* --- device (microphone, camera, speaker) permissions (flatpak) --- */

static void
get_device_allowed (CcApplicationsPanel *self,
                    const gchar         *device,
                    const gchar         *app_id,
                    gboolean            *set,
                    gboolean            *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "devices", device, app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || g_strcmp0 (perms[0], "no") != 0;
}

static void
set_device_allowed (CcApplicationsPanel *self,
                    const gchar         *device,
                    gboolean             allowed)
{
  const gchar *perms[2];

  perms[0] = allowed ? "yes" : "no";
  perms[1] = NULL;

  set_portal_permissions (self, "devices", device, self->current_portal_app_id, perms);
}

static void
microphone_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_device_allowed (self, "microphone", adw_switch_row_get_active (self->microphone));
}

static void
sound_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
   set_device_allowed (self, "speakers", adw_switch_row_get_active (self->sound));
}

static void
camera_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_device_allowed (self, "camera", adw_switch_row_get_active (self->camera));
}

/* --- location permissions (flatpak) --- */

static void
get_location_allowed (CcApplicationsPanel *self,
                      const gchar         *app_id,
                      gboolean            *set,
                      gboolean            *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "location", "location", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || g_strcmp0 (perms[0], "NONE") != 0;
}

static void
set_location_allowed (CcApplicationsPanel *self,
                      gboolean             allowed)
{
  const gchar *perms[3];

  /* FIXME allow setting accuracy */
  perms[0] = allowed ? "EXACT" : "NONE";
  perms[1] = "0";
  perms[2] = NULL;

  set_portal_permissions (self, "location", "location", self->current_portal_app_id, perms);
}

static void
location_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_location_allowed (self, adw_switch_row_get_active (self->location));
}

/* --- permissions section --- */

#ifdef HAVE_SNAP
static void
remove_snap_permissions (CcApplicationsPanel *self)
{
  GList *l;

  for (l = self->snap_permission_rows; l; l = l->next)
    adw_preferences_group_remove (self->integration_section, l->data);
  g_clear_pointer (&self->snap_permission_rows, g_list_free);
}

static gboolean
add_snap_permissions (CcApplicationsPanel *self,
                      GAppInfo            *info,
                      const gchar         *app_id)
{
  const gchar *snap_name;
  g_autoptr(CcSnapdClient) client = NULL;
  g_autoptr(JsonArray) plugs = NULL;
  g_autoptr(JsonArray) slots = NULL;
  gint added = 0;
  g_autoptr(GError) error = NULL;
  g_autoptr(GError) interfaces_error = NULL;

  if (!g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX))
    return FALSE;
  snap_name = app_id + strlen (PORTAL_SNAP_PREFIX);

  client = cc_snapd_client_new ();

  if (!cc_snapd_client_get_all_connections_sync (client, &plugs, &slots, cc_panel_get_cancellable (CC_PANEL (self)), &error))
    {
      g_warning ("Failed to get snap connections: %s", error->message);
      return FALSE;
    }

  for (guint i = 0; i < json_array_get_length (plugs); i++)
    {
      JsonObject *plug = json_array_get_object_element (plugs, i);
      const gchar *plug_interface;
      CcSnapRow *row;
      g_autoptr(JsonArray) available_slots = NULL;
      const gchar * const hidden_interfaces[] = { "content",
                                                  "desktop", "desktop-legacy",
                                                  "mir",
                                                  "unity7", "unity8",
                                                  "wayland",
                                                  "x11",
                                                  NULL };

      /* Skip if not relating to this snap */
      if (g_strcmp0 (json_object_get_string_member (plug, "snap"), snap_name) != 0)
        continue;

      /* Ignore interfaces that are too low level to make sense to show or disable */
      plug_interface = json_object_get_string_member (plug, "interface");
      if (g_strv_contains (hidden_interfaces, plug_interface))
        continue;

      available_slots = json_array_new ();
      for (guint j = 0; j < json_array_get_length (slots); j++)
        {
          JsonObject *slot = json_array_get_object_element (slots, j);
          if (g_strcmp0 (plug_interface, json_object_get_string_member (slot, "interface")) != 0)
            continue;

          json_array_add_object_element (available_slots, slot);
        }

      row = cc_snap_row_new (cc_panel_get_cancellable (CC_PANEL (self)), plug, available_slots);
      adw_preferences_group_add (self->integration_section, GTK_WIDGET (row));
      self->snap_permission_rows = g_list_prepend (self->snap_permission_rows, row);
      added++;
    }

    return added > 0;
}
#endif

static void
update_sandbox_banner (CcApplicationsPanel *self,
                       gboolean             is_sandboxed)
{

  gtk_widget_set_visible (GTK_WIDGET (self->sandbox_banner), !is_sandboxed);
  if (is_sandboxed)
    return;

  adw_banner_set_title (self->sandbox_banner, _("App is not sandboxed"));
}

static gint
add_static_permission_row (CcApplicationsPanel *self,
                           const gchar         *title,
                           const gchar         *subtitle)
{
  GtkWidget *row;

  row = g_object_new (CC_TYPE_INFO_ROW,
                      "title", title,
                      "info", subtitle,
                      NULL);
  gtk_list_box_append (self->builtin_list, row);

  return 1;
}

static gboolean
add_static_permissions (CcApplicationsPanel *self,
                        GAppInfo            *info,
                        const gchar         *app_id)
{
  g_autoptr(GKeyFile) keyfile = NULL;
  g_auto(GStrv) sockets = NULL;
  g_auto(GStrv) devices = NULL;
  g_auto(GStrv) shared = NULL;
  g_auto(GStrv) filesystems = NULL;
  g_autofree gchar *str = NULL;
  gint added = 0;
  g_autofree gchar *text = NULL;
  gboolean is_sandboxed, is_snap = FALSE;

  is_snap = app_id && g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX);
  if (app_id && !is_snap)
    keyfile = get_flatpak_metadata (app_id);

  is_sandboxed = (keyfile != NULL) || is_snap;
  update_sandbox_banner (self, is_sandboxed);
  if (keyfile == NULL)
    return FALSE;

  sockets = g_key_file_get_string_list (keyfile, "Context", "sockets", NULL, NULL);
  if (sockets && g_strv_contains ((const gchar * const*)sockets, "system-bus"))
    added += add_static_permission_row (self, _("System Bus"), _("Full access"));
  if (sockets && g_strv_contains ((const gchar * const*)sockets, "session-bus"))
    added += add_static_permission_row (self, _("Session Bus"), _("Full access"));

  devices = g_key_file_get_string_list (keyfile, "Context", "devices", NULL, NULL);
  if (devices && g_strv_contains ((const gchar * const*)devices, "all"))
    added += add_static_permission_row (self, _("Devices"), _("Full access to /dev"));

  shared = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
  if (shared && g_strv_contains ((const gchar * const*)shared, "network"))
    added += add_static_permission_row (self, _("Network"), _("Has network access"));

  filesystems = g_key_file_get_string_list (keyfile, "Context", "filesystems", NULL, NULL);
  if (filesystems && (g_strv_contains ((const gchar * const *)filesystems, "home") ||
               g_strv_contains ((const gchar * const *)filesystems, "home:rw")))
    added += add_static_permission_row (self, _("Home"), _("Full access"));
  else if (filesystems && g_strv_contains ((const gchar * const *)filesystems, "home:ro"))
    added += add_static_permission_row (self, _("Home"), _("Read-only"));
  if (filesystems && (g_strv_contains ((const gchar * const *)filesystems, "host") ||
               g_strv_contains ((const gchar * const *)filesystems, "host:rw")))
    added += add_static_permission_row (self, _("File System"), _("Full access"));
  else if (filesystems && g_strv_contains ((const gchar * const *)filesystems, "host:ro"))
    added += add_static_permission_row (self, _("File System"), _("Read-only"));

  str = g_key_file_get_string (keyfile, "Session Bus Policy", "ca.desrt.dconf", NULL);
  if (str && g_str_equal (str, "talk"))
    added += add_static_permission_row (self, _("Settings"), _("Can change settings"));

  text = g_strdup_printf (_("<b>%s</b> requires access to the following system resources. To stop this access, the app must be removed."), g_app_info_get_display_name (info));
  adw_preferences_page_set_description (self->builtin_page, text);

  return added > 0;
}

static void
remove_static_permissions (CcApplicationsPanel *self)
{
  gtk_list_box_remove_all (self->builtin_list);
}

/* --- header section --- */

static void
update_header_section (CcApplicationsPanel *self,
                       GAppInfo            *info)
{
  GIcon *icon;

  icon = g_app_info_get_icon (info);
  gtk_image_set_from_gicon (self->app_icon_image, icon);

  gtk_label_set_label (self->app_name_label,
                       g_app_info_get_display_name (info));
}


/* --- gintegration section --- */

static void
update_integration_section (CcApplicationsPanel *self,
                            GAppInfo            *info)
{
  g_autofree gchar *app_id = get_app_id (info);
  g_autofree gchar *portal_app_id = get_portal_app_id (info);
  gboolean set, allowed, disabled;
  gboolean has_any = FALSE;

  disabled = g_settings_get_boolean (self->search_settings, "disable-external");
  get_search_enabled (self, app_id, &set, &allowed);
  adw_switch_row_set_active (self->search, allowed);
  gtk_widget_set_visible (GTK_WIDGET (self->search), set && !disabled);
  gtk_widget_set_visible (GTK_WIDGET (self->no_search), set && disabled);

  if (app_id != NULL)
    {
      g_autofree gchar *desktop_id = g_strconcat (app_id, ".desktop", NULL);
      get_shortcuts_allowed (self, desktop_id, &set, &allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->shortcuts), set);
      adw_switch_row_set_active (self->shortcuts, allowed);
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->shortcuts), FALSE);
    }

#ifdef HAVE_SNAP
  remove_snap_permissions (self);
#endif

  if (portal_app_id != NULL)
    {
      g_clear_object (&self->notification_settings);
      get_notification_allowed (self, portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->notification, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->notification), set);
      has_any |= set;

      get_background_allowed (self, portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->background, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->background), set);
      has_any |= set;

      get_wallpaper_allowed (self, portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->wallpaper, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->wallpaper), set);
      has_any |= set;

      get_screenshot_allowed (self, portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->screenshot, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->screenshot), set);
      has_any |= set;

      disabled = g_settings_get_boolean (self->privacy_settings, "disable-sound-output");
      get_device_allowed (self, "speakers", portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->sound, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->sound), set && !disabled);
      gtk_widget_set_visible (GTK_WIDGET (self->no_sound), set && disabled);

      disabled = g_settings_get_boolean (self->privacy_settings, "disable-camera");
      get_device_allowed (self, "camera", portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->camera, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->camera), set && !disabled);
      gtk_widget_set_visible (GTK_WIDGET (self->no_camera), set && disabled);
      has_any |= set;

      disabled = g_settings_get_boolean (self->privacy_settings, "disable-microphone");
      get_device_allowed (self, "microphone", portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->microphone, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->microphone), set && !disabled);
      gtk_widget_set_visible (GTK_WIDGET (self->no_microphone), set && disabled);
      has_any |= set;

      disabled = !g_settings_get_boolean (self->location_settings, "enabled");
      get_location_allowed (self, portal_app_id, &set, &allowed);
      adw_switch_row_set_active (self->location, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->location), set && !disabled);
      gtk_widget_set_visible (GTK_WIDGET (self->no_location), set && disabled);
      has_any |= set;

#ifdef HAVE_SNAP
      has_any |= add_snap_permissions (self, info, portal_app_id);
#endif
    }
  else
    {
      g_set_object (&self->notification_settings, get_notification_settings (app_id));
      get_notification_allowed (self, app_id, &set, &allowed);
      adw_switch_row_set_active (self->notification, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->notification), set);
      has_any |= set;

      gtk_widget_set_visible (GTK_WIDGET (self->background), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->wallpaper), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->screenshot), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->sound), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->no_sound), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->camera), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->no_camera), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->microphone), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->no_microphone), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->location), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->no_location), FALSE);
    }

  gtk_widget_set_visible (GTK_WIDGET (self->integration_section), has_any);
}

/* --- handler section --- */

static void
unset_cb (CcApplicationsPanel *self,
          GtkButton           *button)
{
  const gchar *type;

  type = (const gchar *)g_object_get_data (G_OBJECT (button), "type");

  g_app_info_remove_supports_type (self->current_app_info, type, NULL);
  update_handler_dialog (self, self->current_app_info);
}

static void
add_scheme (CcApplicationsPanel *self,
            const gchar         *type)
{
  g_autofree gchar *title = NULL;
  GtkWidget *button;
  GtkWidget *row;
  gchar *scheme;

  scheme = strrchr (type, '/') + 1;
  title = g_strdup_printf ("%s://", scheme);

  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);

  button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_add_css_class (button, "circular");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), button);
  g_object_set_data_full (G_OBJECT (button), "type", g_strdup (type), g_free);
  g_signal_connect_object (button, "clicked", G_CALLBACK (unset_cb), self, G_CONNECT_SWAPPED);

  gtk_widget_set_visible (GTK_WIDGET (self->handler_link_group), TRUE);
  adw_preferences_group_add (self->handler_link_group, GTK_WIDGET (row));

  self->link_handler_rows = g_list_prepend (self->link_handler_rows, row);
}

static void
add_file_type (CcApplicationsPanel *self,
               const gchar         *type)
{
  g_autofree gchar *desc = NULL;
  const gchar *glob;
  GtkWidget *button;
  GtkWidget *row;

  glob = g_hash_table_lookup (self->globs, type);

  desc = g_content_type_get_description (type);
  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), desc);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), glob ? glob : "");

  button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_add_css_class (button, "circular");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), button);
  g_object_set_data_full (G_OBJECT (button), "type", g_strdup (type), g_free);
  g_signal_connect_object (button, "clicked", G_CALLBACK (unset_cb), self, G_CONNECT_SWAPPED);

  gtk_widget_set_visible (GTK_WIDGET (self->handler_file_group), TRUE);
  adw_preferences_group_add (self->handler_file_group, GTK_WIDGET (row));

  self->file_handler_rows = g_list_prepend (self->file_handler_rows, row);
}

static void
add_handler_row (CcApplicationsPanel *self,
                 const gchar         *type)
{
  gtk_widget_set_visible (GTK_WIDGET (self->handler_row), TRUE);

  if (g_content_type_is_a (type, "x-scheme-handler/*"))
    add_scheme (self, type);
  else
    add_file_type (self, type);
}

static gboolean
app_info_recommended_for (GAppInfo    *info,
                          const gchar *type)
{
  /* this is horribly inefficient. I blame the mime system */
  g_autolist(GObject) list = NULL;
  GList *l;
  gboolean ret = FALSE;

  list = g_app_info_get_recommended_for_type (type);
  for (l = list; l; l = l->next)
    {
      GAppInfo *ri = l->data;

      if (g_app_info_equal (info, ri))
        {
          ret = TRUE;
          break;
        }
    }

  return ret;
}

static void
handler_reset_cb (CcApplicationsPanel *self)
{
  const gchar **types;
  gint i;

  types = g_app_info_get_supported_types (self->current_app_info);
  if (types == NULL || types[0] == NULL)
    return;

  g_signal_handler_block (self->monitor, self->monitor_id);
  for (i = 0; types[i]; i++)
    {
      gchar *ctype = g_content_type_from_mime_type (types[i]);
      g_app_info_add_supports_type (self->current_app_info, ctype, NULL);
    }
  g_signal_handler_unblock (self->monitor, self->monitor_id);
  g_signal_emit_by_name (self->monitor, "changed");
  update_handler_dialog(self, self->current_app_info);
}

static void
remove_all_handler_rows (CcApplicationsPanel *self)
{
  GList *l;

  for (l = self->file_handler_rows; l; l = l->next)
    adw_preferences_group_remove (self->handler_file_group, l->data);
  g_clear_pointer (&self->file_handler_rows, g_list_free);

  for (l = self->link_handler_rows; l; l = l->next)
    adw_preferences_group_remove (self->handler_link_group, l->data);
  g_clear_pointer (&self->link_handler_rows, g_list_free);
}

static void
update_handler_dialog (CcApplicationsPanel *self,
                       GAppInfo            *info)
{
  g_autofree gchar *header_title = NULL;
  g_autoptr(GHashTable) hash = NULL;
  const gchar **types;
  guint n_associations = 0;
  gint i;

  remove_all_handler_rows (self);

  gtk_widget_set_visible (GTK_WIDGET (self->handler_row), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->handler_file_group), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->handler_link_group), FALSE);

  types = g_app_info_get_supported_types (info);
  if (types == NULL || types[0] == NULL)
    return;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  gtk_widget_set_sensitive (GTK_WIDGET (self->handler_reset), FALSE);
  for (i = 0; types[i]; i++)
    {
      g_autofree gchar *ctype = g_content_type_from_mime_type (types[i]);

      if (g_hash_table_contains (hash, ctype))
        continue;

      if (!app_info_recommended_for (info, ctype))
        {
          gtk_widget_set_sensitive (GTK_WIDGET (self->handler_reset), TRUE);
          continue;
        }

      add_handler_row (self, ctype);
      g_hash_table_add (hash, g_steal_pointer (&ctype));

      n_associations++;
    }

  if (n_associations > 0)
    {
      g_autofree gchar *subtitle = NULL;

      subtitle = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                               "%u file and link type that is opened by the app",
                                               "%u file and link types that are opened by the app",
                                               n_associations),
                                  n_associations);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (self->handler_row), subtitle);
    }

  header_title = g_strdup_printf (_("<b>%s</b> is used to open the following types of files and links."),
                                  g_app_info_get_display_name (info));
  adw_preferences_page_set_description (self->handler_page, header_title);
}

/* --- usage section --- */

static void
on_builtin_row_activated_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));

  gtk_window_set_transient_for (self->builtin_dialog,
                                GTK_WINDOW (cc_shell_get_toplevel (shell)));
  gtk_window_present (self->builtin_dialog);
}

static void
on_handler_row_activated_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));

  gtk_window_set_transient_for (self->handler_dialog,
                                GTK_WINDOW (cc_shell_get_toplevel (shell)));
  gtk_window_present (self->handler_dialog);
}

static void
on_storage_row_activated_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));

  gtk_window_set_transient_for (self->storage_dialog,
                                GTK_WINDOW (cc_shell_get_toplevel (shell)));
  gtk_window_present (self->storage_dialog);
}

static void
on_items_changed_cb (GListModel *list,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    data)
{
  CcApplicationsPanel *self = data;

  if (g_list_model_get_n_items (list) == 0)
    gtk_stack_set_visible_child (self->app_listbox_stack,
                                 self->no_apps_page);
  else
    gtk_stack_set_visible_child (self->app_listbox_stack,
                                 GTK_WIDGET (self->app_listbox));
}

static void
update_total_size (CcApplicationsPanel *self)
{
  g_autofree gchar *formatted_size = NULL;
  g_autofree gchar *subtitle = NULL;
  guint64 total;

  total = self->app_size + self->data_size + self->cache_size;
  formatted_size = g_format_size (total);
  g_object_set (self->total, "info", formatted_size, NULL);

  /* Translators: '%s' is the formatted size, e.g. "26.2 MB" */
  subtitle = g_strdup_printf (_("%s of disk space used"), formatted_size);
  g_object_set (self->storage, "subtitle", subtitle, NULL);
}

static void
set_cache_size (GObject      *source,
                GAsyncResult *res,
                gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autofree gchar *formatted_size = NULL;
  guint64 size;
  g_autoptr(GError) error = NULL;

  if (!file_size_finish (G_FILE (source), res, &size, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get flatpak cache size: %s", error->message);
      return;
    }
  self->cache_size = size;

  formatted_size = g_format_size (self->cache_size);
  g_object_set (self->cache, "info", formatted_size, NULL);

  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_cache_button), self->cache_size > 0);

  update_total_size (self);
}

static void
update_cache_row (CcApplicationsPanel *self,
                  const gchar         *app_id)
{
  g_autoptr(GFile) dir = get_flatpak_app_dir (app_id, "cache");
  g_object_set (self->cache, "info", "...", NULL);
  file_size_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), set_cache_size, self);
}

static void
set_data_size (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autofree gchar *formatted_size = NULL;
  guint64 size;
  g_autoptr(GError) error = NULL;

  if (!file_size_finish (G_FILE (source), res, &size, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get flatpak data size: %s", error->message);
      return;
    }
  self->data_size = size;

  formatted_size = g_format_size (self->data_size);
  g_object_set (self->data, "info", formatted_size, NULL);

  update_total_size (self);
}

static void
update_data_row (CcApplicationsPanel *self,
                 const gchar          *app_id)
{
  g_autoptr(GFile) dir = get_flatpak_app_dir (app_id, "data");

  g_object_set (self->data, "info", "...", NULL);
  file_size_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), set_data_size, self);
}

static void
cache_cleared (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autoptr(GError) error = NULL;

  if (!file_remove_finish (G_FILE (source), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to remove cache: %s", error->message);
      return;
    }

  update_cache_row (self, self->current_app_id);
}

static void
clear_cache_cb (CcApplicationsPanel *self)
{
  g_autoptr(GFile) dir = NULL;

  if (self->current_app_id == NULL)
    return;

  dir = get_flatpak_app_dir (self->current_app_id, "cache");
  file_remove_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), cache_cleared, self);
}

static void
update_app_row (CcApplicationsPanel *self,
                const gchar         *app_id)
{
  g_autofree gchar *formatted_size = NULL;

  if (g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX))
    self->app_size = get_snap_app_size (app_id + strlen (PORTAL_SNAP_PREFIX));
  else
    self->app_size = get_flatpak_app_size (app_id);
  formatted_size = g_format_size (self->app_size);
  g_object_set (self->app, "info", formatted_size, NULL);
  update_total_size (self);
}

static void
update_app_sizes (CcApplicationsPanel *self,
                  const gchar         *app_id)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_cache_button), FALSE);

  self->app_size = self->data_size = self->cache_size = 0;

  update_app_row (self, app_id);
  update_cache_row (self, app_id);
  update_data_row (self, app_id);
}

static void
update_usage_section (CcApplicationsPanel *self,
                      GAppInfo            *info)
{
  g_autofree gchar *portal_app_id = get_portal_app_id (info);
  gboolean has_builtin = FALSE;

  if (portal_app_id != NULL)
    update_app_sizes (self, portal_app_id);

  remove_static_permissions (self);
  has_builtin = add_static_permissions (self, info, portal_app_id);
  gtk_widget_set_visible (GTK_WIDGET (self->other_permissions_section), has_builtin);

  gtk_widget_set_visible (GTK_WIDGET (self->usage_section), portal_app_id || has_builtin);
}

/* --- panel setup --- */

static void
update_panel (CcApplicationsPanel *self,
              GtkListBoxRow       *row)
{
  GAppInfo *info;

  if (self->perm_store == NULL)
    {
      /* Async permission store not initialized, row will be re-activated in the callback */
      self->perm_store_pending_row = row;
      return;
    }

  if (row == NULL)
    {
      g_message ("No app selected, try again");
      return;
    }

  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (row));

  adw_navigation_page_set_title (self->app_settings_page,
                                 g_app_info_get_display_name (info));
  adw_navigation_view_push_by_tag(self->navigation_view, "settings-box");
  gtk_widget_set_visible (GTK_WIDGET (self->view_details_button), gnome_software_is_installed ());

  g_clear_pointer (&self->current_app_id, g_free);
  g_clear_pointer (&self->current_portal_app_id, g_free);

  update_header_section (self, info);
  update_integration_section (self, info);
  update_handler_dialog (self, info);
  update_usage_section (self, info);

  g_set_object (&self->current_app_info, info);
  self->current_app_id = get_app_id (info);
  self->current_portal_app_id = get_portal_app_id (info);
}


static gint
compare_rows (gconstpointer  a,
              gconstpointer  b,
              gpointer       data)
{
  GAppInfo *item1 = (GAppInfo *) a;
  GAppInfo *item2 = (GAppInfo *) b;

  g_autofree gchar *key1 = NULL;
  g_autofree gchar *key2 = NULL;

  key1 = g_utf8_casefold (g_app_info_get_display_name (item1), -1);
  key2 = g_utf8_casefold (g_app_info_get_display_name (item2), -1);

  const gchar *sort_key1 = g_utf8_collate_key (key1, -1);
  const gchar *sort_key2 = g_utf8_collate_key (key2, -1);

  return strcmp (sort_key1, sort_key2);
}

static void
populate_applications (CcApplicationsPanel *self)
{
  g_autolist(GObject) infos = NULL;
  GList *l;

  g_list_store_remove_all (G_LIST_STORE (self->app_model));
#ifdef HAVE_MALCONTENT
  g_signal_handler_block (self->manager, self->app_filter_id);
#endif

  infos = g_app_info_get_all ();

  if (!infos)
    gtk_widget_set_visible (GTK_WIDGET (self->app_search_entry), 0);
  else
    gtk_widget_set_visible (GTK_WIDGET (self->app_search_entry), 1);

  for (l = infos; l; l = l->next)
    {
      GAppInfo *info = l->data;
      GtkWidget *row;
      g_autofree gchar *id = NULL;

      if (!g_app_info_should_show (info))
        continue;

#ifdef HAVE_MALCONTENT
      if (!mct_app_filter_is_appinfo_allowed (self->app_filter, info))
        continue;
#endif

      row = GTK_WIDGET (cc_applications_row_new (info));
      g_list_store_insert_sorted (G_LIST_STORE (self->app_model), info, compare_rows, NULL);

      id = get_app_id (info);
      if (g_strcmp0 (id, self->current_app_id) == 0)
        gtk_list_box_select_row (self->app_listbox, GTK_LIST_BOX_ROW (row));
    }
#ifdef HAVE_MALCONTENT
  g_signal_handler_unblock (self->manager, self->app_filter_id);
#endif
}

static gboolean
filter_app_rows (GObject   *item,
                 gpointer   data)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (data);
  g_autofree gchar *app_name = NULL;
  g_autofree gchar *search_text = NULL;
  const gchar *text;
  GAppInfo *info = G_APP_INFO (item);

  text = gtk_editable_get_text (GTK_EDITABLE (self->app_search_entry));

  /* Only filter after the second character */
  if (g_utf8_strlen (text, -1) < 2)
    return TRUE;

  app_name = cc_util_normalize_casefold_and_unaccent (g_app_info_get_name (info));
  search_text = cc_util_normalize_casefold_and_unaccent (text);

  return g_strstr_len (app_name, -1, search_text) != NULL;
}

#ifdef HAVE_MALCONTENT
static void
app_filter_changed_cb (MctAppFilter        *app_filter,
                       uid_t               uid,
                       CcApplicationsPanel *self)
{
  populate_applications (self);
}
#endif

static void
apps_changed (CcApplicationsPanel *self)
{
  populate_applications (self);
}

static void
row_activated_cb (CcApplicationsPanel *self,
                  GtkListBoxRow       *row)
{
  update_panel (self, row);
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      data)
{
  CcApplicationsPanel *self = data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to portal permission store: %s",
                     error->message);
      return;
    }

  self->perm_store = proxy;

  if (self->perm_store_pending_row)
    g_signal_emit_by_name (self->perm_store_pending_row, "activate");

  self->perm_store_pending_row = NULL;
}

static void
select_app (CcApplicationsPanel *self,
            const gchar         *app_id,
            gboolean             emit_activate)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->app_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      CcApplicationsRow *row = CC_APPLICATIONS_ROW (child);
      GAppInfo *info = cc_applications_row_get_info (row);
      if (g_str_has_prefix (g_app_info_get_id (info), app_id))
        {
          gtk_list_box_select_row (self->app_listbox, GTK_LIST_BOX_ROW (row));
          if (emit_activate)
            g_signal_emit_by_name (row, "activate");
          break;
        }
    }
}

static void
on_launch_button_clicked_cb (CcApplicationsPanel *self)
{
  g_autoptr(GdkAppLaunchContext) context = NULL;
  g_autoptr(GError) error = NULL;
  GdkDisplay *display;

  if (!self->current_app_info)
    return;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  context = gdk_display_get_app_launch_context (display);

  g_app_info_launch (self->current_app_info,
                     NULL,
                     G_APP_LAUNCH_CONTEXT (context),
                     &error);

  if (error)
    g_warning ("Error launching app: %s", error->message);
}

static void
on_app_search_entry_activated_cb (CcApplicationsPanel *self)
{
  GtkListBoxRow *row;

  row = gtk_list_box_get_row_at_y (self->app_listbox, 0);

  if (!row)
    return;

  /* Show the app */
  gtk_list_box_select_row (self->app_listbox, row);
  g_signal_emit_by_name (row, "activate");

  /* Cleanup the entry */
  gtk_editable_set_text (GTK_EDITABLE (self->app_search_entry), "");
  gtk_widget_grab_focus (GTK_WIDGET (self->app_search_entry));
}

static void
on_app_search_entry_search_changed_cb (CcApplicationsPanel *self)
{
  gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
on_app_search_entry_search_stopped_cb (CcApplicationsPanel *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->app_search_entry), "");
}

static void
cc_applications_panel_dispose (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);

  g_clear_pointer (&self->sandbox_info_button, gtk_widget_unparent);
  g_clear_pointer (&self->builtin_dialog, gtk_window_destroy);
  g_clear_pointer (&self->handler_dialog, gtk_window_destroy);
  g_clear_pointer (&self->storage_dialog, gtk_window_destroy);

  remove_all_handler_rows (self);
#ifdef HAVE_SNAP
  remove_snap_permissions (self);
#endif
  g_clear_object (&self->monitor);
  g_clear_object (&self->perm_store);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->dispose (object);
}

static void
cc_applications_panel_finalize (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);
#ifdef HAVE_MALCONTENT
  if (self->app_filter != NULL && self->app_filter_id != 0)
    {
      g_signal_handler_disconnect (self->manager, self->app_filter_id);
      self->app_filter_id = 0;
    }
  g_clear_pointer (&self->app_filter, mct_app_filter_unref);

  g_clear_object (&self->manager);
#endif
  g_clear_object (&self->media_handling_settings);
  g_clear_object (&self->notification_settings);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->search_settings);

  g_clear_object (&self->current_app_info);
  g_clear_pointer (&self->current_app_id, g_free);
  g_clear_pointer (&self->current_portal_app_id, g_free);
  g_clear_pointer (&self->globs, g_hash_table_unref);
  g_clear_pointer (&self->search_providers, g_hash_table_unref);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->finalize (object);
}

static void
cc_applications_panel_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters, *v;
          const gchar *first_arg = NULL;

          parameters = g_value_get_variant (value);
          if (parameters == NULL)
            return;

          if (g_variant_n_children (parameters) > 0)
            {
              g_variant_get_child (parameters, 0, "v", &v);
              if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
                first_arg = g_variant_get_string (v, NULL);
              else
                g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                           (gchar *)g_variant_get_type (v));
              g_variant_unref (v);

              select_app (CC_APPLICATIONS_PANEL (object), first_arg, TRUE);
            }

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_applications_panel_constructed (GObject *object)
{
  G_OBJECT_CLASS (cc_applications_panel_parent_class)->constructed (object);
}

static void
cc_applications_panel_class_init (CcApplicationsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_DEFAULT_APPS_PAGE);
  g_type_ensure (CC_TYPE_REMOVABLE_MEDIA_SETTINGS);

  object_class->dispose = cc_applications_panel_dispose;
  object_class->finalize = cc_applications_panel_finalize;
  object_class->constructed = cc_applications_panel_constructed;
  object_class->set_property = cc_applications_panel_set_property;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_icon_image);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_name_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_settings_page);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, other_permissions_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, autorun_never_row);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_page);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, cache);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, clear_cache_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, data);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, default_apps_page);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_apps_page);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app_listbox_stack);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_page);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_file_group);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_link_group);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_row);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_reset);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, install_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, integration_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, launch_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_search);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, notification);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, background);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, wallpaper);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, removable_media_settings);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sandbox_banner);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sandbox_info_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, screenshot);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, shortcuts);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, search);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, total);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, usage_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, view_details_button);

  gtk_widget_class_bind_template_callback (widget_class, camera_cb);
  gtk_widget_class_bind_template_callback (widget_class, location_cb);
  gtk_widget_class_bind_template_callback (widget_class, microphone_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_cb);
  gtk_widget_class_bind_template_callback (widget_class, background_cb);
  gtk_widget_class_bind_template_callback (widget_class, wallpaper_cb);
  gtk_widget_class_bind_template_callback (widget_class, screenshot_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcuts_cb);
  gtk_widget_class_bind_template_callback (widget_class, privacy_link_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_cb);
  gtk_widget_class_bind_template_callback (widget_class, clear_cache_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_software_cb);
  gtk_widget_class_bind_template_callback (widget_class, handler_reset_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_builtin_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_handler_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_launch_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_app_search_entry_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_app_search_entry_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_app_search_entry_search_stopped_cb);

  gtk_widget_class_bind_template_callback (widget_class, on_storage_row_activated_cb);
}

static GtkWidget *
app_row_new (gpointer item,
             gpointer user_data)
{
  GAppInfo *info = item;

  return GTK_WIDGET (cc_applications_row_new (info));
}

static void
cc_applications_panel_init (CcApplicationsPanel *self)
{
#ifdef HAVE_MALCONTENT
  g_autoptr(GDBusConnection) system_bus = NULL;
  g_autoptr(GError) error = NULL;
#endif

  g_resources_register (cc_applications_get_resource ());

  g_type_ensure(CC_TYPE_INFO_ROW);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_visible (GTK_WIDGET (self->install_button), gnome_software_is_installed ());

  g_signal_connect_object (self->app_listbox, "row-activated",
                           G_CALLBACK (row_activated_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->view_details_button,
                           "clicked",
                           G_CALLBACK (open_software_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->filter = GTK_FILTER (gtk_custom_filter_new ((GtkCustomFilterFunc) filter_app_rows,
                                                    self, NULL));

  self->app_model = G_LIST_MODEL (g_list_store_new (G_TYPE_APP_INFO));
  self->filter_model = G_LIST_MODEL (gtk_filter_list_model_new (self->app_model,
                                                                GTK_FILTER (self->filter)));
  g_signal_connect (self->filter_model, "items-changed",
                    G_CALLBACK (on_items_changed_cb), self);

  gtk_list_box_bind_model (self->app_listbox,
                           self->filter_model,
                           app_row_new,
                           NULL,
                           NULL);

  self->location_settings = g_settings_new ("org.gnome.system.location");
  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  self->search_settings = g_settings_new ("org.gnome.desktop.search-providers");
  self->media_handling_settings = g_settings_new ("org.gnome.desktop.media-handling");

  g_settings_bind (self->media_handling_settings,
                   "autorun-never",
                   self->autorun_never_row,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

#ifdef HAVE_MALCONTENT
   /* FIXME: should become asynchronous */
  system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, self->cancellable, &error);
  if (system_bus == NULL)
    {
      g_warning ("Error getting system bus while setting up app permissions: %s", error->message);
      return;
    }

  /* Load the user’s parental controls settings too, so we can filter the list. */
  self->manager = mct_manager_new (system_bus);
  self->app_filter = mct_manager_get_app_filter (self->manager,
                                                 getuid (),
                                                 MCT_MANAGER_GET_VALUE_FLAGS_NONE,
                                                 self->cancellable,
                                                 &error);
  if (error)
    {
      g_warning ("Error retrieving app filter: %s", error->message);
      return;
    }

  self->app_filter_id = g_signal_connect (self->manager, "app-filter-changed",
                                          G_CALLBACK (app_filter_changed_cb), self);
#endif
  populate_applications (self);

  self->monitor = g_app_info_monitor_get ();
  self->monitor_id = g_signal_connect_object (self->monitor, "changed", G_CALLBACK (apps_changed), self, G_CONNECT_SWAPPED);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            on_perm_store_ready,
                            self);

  self->globs = parse_globs ();
  self->search_providers = parse_search_providers ();
}
