/* gtkplacesview.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <gio/gvfs.h>
#include <gtk/gtk.h>

#include "gtkprivate.h"
#include <glib/gi18n-lib.h>
#include "gtkmarshalers.h"
#include "gtkplacesviewprivate.h"
#include "gtkplacesviewrowprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkeventcontrollerkey.h"
#include "gtkpopovermenu.h"

/*< private >
 * GtkPlacesView:
 *
 * GtkPlacesView is a widget that displays a list of persistent drives
 * such as harddisk partitions and networks.  GtkPlacesView does not monitor
 * removable devices.
 *
 * The places view displays drives and networks, and will automatically mount
 * them when the user activates. Network addresses are stored even if they fail
 * to connect. When the connection is successful, the connected network is
 * shown at the network list.
 *
 * To make use of the places view, an application at least needs to connect
 * to the GtkPlacesView::open-location signal. This is emitted when the user
 * selects a location to open in the view.
 */

struct _GtkPlacesViewClass
{
  GtkBoxClass parent_class;

  void     (* open_location)        (GtkPlacesView          *view,
                                     GFile                  *location,
                                     GtkPlacesOpenFlags  open_flags);

  void    (* show_error_message)     (GtkPlacesSidebar      *sidebar,
                                      const char            *primary,
                                      const char            *secondary);
};

struct _GtkPlacesView
{
  GtkBox parent_instance;

  GVolumeMonitor                *volume_monitor;
  GtkPlacesOpenFlags             open_flags;
  GtkPlacesOpenFlags             current_open_flags;

  GFile                         *server_list_file;
  GFileMonitor                  *server_list_monitor;
  GFileMonitor                  *network_monitor;

  GCancellable                  *cancellable;

  char                          *search_query;

  GtkWidget                     *actionbar;
  GtkWidget                     *address_entry;
  GtkWidget                     *connect_button;
  GtkWidget                     *listbox;
  GtkWidget                     *popup_menu;
  GtkWidget                     *recent_servers_listbox;
  GtkWidget                     *recent_servers_popover;
  GtkWidget                     *recent_servers_stack;
  GtkWidget                     *stack;
  GtkWidget                     *server_adresses_popover;
  GtkWidget                     *available_protocols_grid;
  GtkWidget                     *network_placeholder;
  GtkWidget                     *network_placeholder_label;

  GtkSizeGroup                  *path_size_group;
  GtkSizeGroup                  *space_size_group;

  GCancellable                  *networks_fetching_cancellable;

  GtkPlacesViewRow              *row_for_action;

  guint                          should_open_location : 1;
  guint                          should_pulse_entry : 1;
  guint                          entry_pulse_timeout_id;
  guint                          connecting_to_server : 1;
  guint                          mounting_volume : 1;
  guint                          unmounting_mount : 1;
  guint                          fetching_networks : 1;
  guint                          loading : 1;
  guint                          destroyed : 1;
};

static void        mount_volume                                  (GtkPlacesView *view,
                                                                  GVolume       *volume);

static void        on_eject_button_clicked                       (GtkWidget        *widget,
                                                                  GtkPlacesViewRow *row);

static gboolean on_row_popup_menu (GtkWidget *widget,
                                   GVariant  *args,
                                   gpointer   user_data);

static void click_cb (GtkGesture *gesture,
                      int         n_press,
                      double      x,
                      double      y,
                      gpointer    user_data);

static void        populate_servers                              (GtkPlacesView *view);

static gboolean    gtk_places_view_get_fetching_networks         (GtkPlacesView *view);

static void        gtk_places_view_set_fetching_networks         (GtkPlacesView *view,
                                                                  gboolean       fetching_networks);

static void        gtk_places_view_set_loading                   (GtkPlacesView *view,
                                                                  gboolean       loading);

static void        update_loading                                (GtkPlacesView *view);

G_DEFINE_TYPE (GtkPlacesView, gtk_places_view, GTK_TYPE_BOX)

/* GtkPlacesView properties & signals */
enum {
  PROP_0,
  PROP_OPEN_FLAGS,
  PROP_FETCHING_NETWORKS,
  PROP_LOADING,
  LAST_PROP
};

enum {
  OPEN_LOCATION,
  SHOW_ERROR_MESSAGE,
  LAST_SIGNAL
};

const char *unsupported_protocols [] =
{
  "file", "afc", "obex", "http",
  "trash", "burn", "computer",
  "archive", "recent", "localtest",
  NULL
};

static guint places_view_signals [LAST_SIGNAL] = { 0 };
static GParamSpec *properties [LAST_PROP];

static void
emit_open_location (GtkPlacesView      *view,
                    GFile              *location,
                    GtkPlacesOpenFlags  open_flags)
{
  if ((open_flags & view->open_flags) == 0)
    open_flags = GTK_PLACES_OPEN_NORMAL;

  g_signal_emit (view, places_view_signals[OPEN_LOCATION], 0, location, open_flags);
}

static void
emit_show_error_message (GtkPlacesView *view,
                         char          *primary_message,
                         char          *secondary_message)
{
  g_signal_emit (view, places_view_signals[SHOW_ERROR_MESSAGE],
                         0, primary_message, secondary_message);
}

static void
server_file_changed_cb (GtkPlacesView *view)
{
  populate_servers (view);
}

static GBookmarkFile *
server_list_load (GtkPlacesView *view)
{
  GBookmarkFile *bookmarks;
  GError *error = NULL;
  char *datadir;
  char *filename;

  bookmarks = g_bookmark_file_new ();
  datadir = g_build_filename (g_get_user_config_dir (), "gtk-4.0", NULL);
  filename = g_build_filename (datadir, "servers", NULL);

  g_mkdir_with_parents (datadir, 0700);
  g_bookmark_file_load_from_file (bookmarks, filename, &error);

  if (error)
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          /* only warn if the file exists */
          g_warning ("Unable to open server bookmarks: %s", error->message);
          g_clear_pointer (&bookmarks, g_bookmark_file_free);
        }

      g_clear_error (&error);
    }

  /* Monitor the file in case it's modified outside this code */
  if (!view->server_list_monitor)
    {
      view->server_list_file = g_file_new_for_path (filename);

      if (view->server_list_file)
        {
          view->server_list_monitor = g_file_monitor_file (view->server_list_file,
                                                           G_FILE_MONITOR_NONE,
                                                           NULL,
                                                           &error);

          if (error)
            {
              g_warning ("Cannot monitor server file: %s", error->message);
              g_clear_error (&error);
            }
          else
            {
              g_signal_connect_swapped (view->server_list_monitor,
                                        "changed",
                                        G_CALLBACK (server_file_changed_cb),
                                        view);
            }
        }

      g_clear_object (&view->server_list_file);
    }

  g_free (datadir);
  g_free (filename);

  return bookmarks;
}

static void
server_list_save (GBookmarkFile *bookmarks)
{
  char *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "servers", NULL);
  g_bookmark_file_to_file (bookmarks, filename, NULL);
  g_free (filename);
}

static void
server_list_add_server (GtkPlacesView *view,
                        GFile         *file)
{
  GBookmarkFile *bookmarks;
  GFileInfo *info;
  GError *error;
  char *title;
  char *uri;
  GDateTime *now;

  error = NULL;
  bookmarks = server_list_load (view);

  if (!bookmarks)
    return;

  uri = g_file_get_uri (file);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  title = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

  g_bookmark_file_set_title (bookmarks, uri, title);
  now = g_date_time_new_now_utc ();
  g_bookmark_file_set_visited_date_time (bookmarks, uri, now);
  g_date_time_unref (now);
  g_bookmark_file_add_application (bookmarks, uri, NULL, NULL);

  server_list_save (bookmarks);

  g_bookmark_file_free (bookmarks);
  g_clear_object (&info);
  g_free (title);
  g_free (uri);
}

static void
server_list_remove_server (GtkPlacesView *view,
                           const char    *uri)
{
  GBookmarkFile *bookmarks;

  bookmarks = server_list_load (view);

  if (!bookmarks)
    return;

  g_bookmark_file_remove_item (bookmarks, uri, NULL);
  server_list_save (bookmarks);

  g_bookmark_file_free (bookmarks);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  if (GTK_IS_WINDOW (toplevel))
    return GTK_WINDOW (toplevel);
  else
    return NULL;
}

static void
set_busy_cursor (GtkPlacesView *view,
                 gboolean       busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;

  toplevel = get_toplevel (GTK_WIDGET (view));
  widget = GTK_WIDGET (toplevel);
  if (!toplevel || !gtk_widget_get_realized (widget))
    return;

  if (busy)
    gtk_widget_set_cursor_from_name (widget, "progress");
  else
    gtk_widget_set_cursor (widget, NULL);
}

/* Activates the given row, with the given flags as parameter */
static void
activate_row (GtkPlacesView      *view,
              GtkPlacesViewRow   *row,
              GtkPlacesOpenFlags  flags)
{
  GVolume *volume;
  GMount *mount;
  GFile *file;

  mount = gtk_places_view_row_get_mount (row);
  volume = gtk_places_view_row_get_volume (row);
  file = gtk_places_view_row_get_file (row);

  if (file)
    {
      emit_open_location (view, file, flags);
    }
  else if (mount)
    {
      GFile *location = g_mount_get_default_location (mount);

      emit_open_location (view, location, flags);

      g_object_unref (location);
    }
  else if (volume && g_volume_can_mount (volume))
    {
      /*
       * When the row is activated, the unmounted volume shall
       * be mounted and opened right after.
       */
      view->should_open_location = TRUE;

      gtk_places_view_row_set_busy (row, TRUE);
      mount_volume (view, volume);
    }
}

static void update_places (GtkPlacesView *view);

static void
gtk_places_view_finalize (GObject *object)
{
  GtkPlacesView *view = (GtkPlacesView *)object;

  if (view->entry_pulse_timeout_id > 0)
    g_source_remove (view->entry_pulse_timeout_id);

  g_clear_pointer (&view->search_query, g_free);
  g_clear_object (&view->server_list_file);
  g_clear_object (&view->server_list_monitor);
  g_clear_object (&view->volume_monitor);
  g_clear_object (&view->network_monitor);
  g_clear_object (&view->cancellable);
  g_clear_object (&view->networks_fetching_cancellable);
  g_clear_object (&view->path_size_group);
  g_clear_object (&view->space_size_group);

  G_OBJECT_CLASS (gtk_places_view_parent_class)->finalize (object);
}

static void
gtk_places_view_dispose (GObject *object)
{
  GtkPlacesView *view = (GtkPlacesView *)object;

  view->destroyed = 1;

  g_signal_handlers_disconnect_by_func (view->volume_monitor, update_places, object);

  if (view->network_monitor)
    g_signal_handlers_disconnect_by_func (view->network_monitor, update_places, object);

  if (view->server_list_monitor)
    g_signal_handlers_disconnect_by_func (view->server_list_monitor, server_file_changed_cb, object);

  g_cancellable_cancel (view->cancellable);
  g_cancellable_cancel (view->networks_fetching_cancellable);
  g_clear_pointer (&view->popup_menu, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_places_view_parent_class)->dispose (object);
}

static void
gtk_places_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkPlacesView *self = GTK_PLACES_VIEW (object);

  switch (prop_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, gtk_places_view_get_loading (self));
      break;

    case PROP_OPEN_FLAGS:
      g_value_set_flags (value, gtk_places_view_get_open_flags (self));
      break;

    case PROP_FETCHING_NETWORKS:
      g_value_set_boolean (value, gtk_places_view_get_fetching_networks (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkPlacesView *self = GTK_PLACES_VIEW (object);

  switch (prop_id)
    {
    case PROP_OPEN_FLAGS:
      gtk_places_view_set_open_flags (self, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
is_external_volume (GVolume *volume)
{
  gboolean is_external;
  GDrive *drive;
  char *id;

  drive = g_volume_get_drive (volume);
  id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

  is_external = g_volume_can_eject (volume);

  /* NULL volume identifier only happens on removable devices */
  is_external |= !id;

  if (drive)
    is_external |= g_drive_is_removable (drive);

  g_clear_object (&drive);
  g_free (id);

  return is_external;
}

typedef struct
{
  char          *uri;
  GtkPlacesView *view;
} RemoveServerData;

static void
on_remove_server_button_clicked (RemoveServerData *data)
{
  server_list_remove_server (data->view, data->uri);

  populate_servers (data->view);
}

static void
populate_servers (GtkPlacesView *view)
{
  GBookmarkFile *server_list;
  GtkWidget *child;
  char **uris;
  gsize num_uris;
  int i;

  server_list = server_list_load (view);

  if (!server_list)
    return;

  uris = g_bookmark_file_get_uris (server_list, &num_uris);

  gtk_stack_set_visible_child_name (GTK_STACK (view->recent_servers_stack),
                                    num_uris > 0 ? "list" : "empty");

  if (!uris)
    {
      g_bookmark_file_free (server_list);
      return;
    }

  /* clear previous items */
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (view->recent_servers_listbox))))
    gtk_list_box_remove (GTK_LIST_BOX (view->recent_servers_listbox), child);

  for (i = 0; i < num_uris; i++)
    {
      RemoveServerData *data;
      GtkWidget *row;
      GtkWidget *grid;
      GtkWidget *button;
      GtkWidget *label;
      char *name;
      char *dup_uri;

      name = g_bookmark_file_get_title (server_list, uris[i], NULL);
      dup_uri = g_strdup (uris[i]);

      /* add to the recent servers listbox */
      row = gtk_list_box_row_new ();

      grid = g_object_new (GTK_TYPE_GRID,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           NULL);

      /* name of the connected uri, if any */
      label = gtk_label_new (name);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

      /* the uri itself */
      label = gtk_label_new (uris[i]);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

      /* remove button */
      button = gtk_button_new_from_icon_name ("window-close-symbolic");
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
      gtk_widget_add_css_class (button, "sidebar-button");
      gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 2);

      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), grid);
      gtk_list_box_insert (GTK_LIST_BOX (view->recent_servers_listbox), row, -1);

      /* custom data */
      data = g_new0 (RemoveServerData, 1);
      data->view = view;
      data->uri = dup_uri;

      g_object_set_data_full (G_OBJECT (row), "uri", dup_uri, g_free);
      g_object_set_data_full (G_OBJECT (row), "remove-server-data", data, g_free);

      g_signal_connect_swapped (button,
                                "clicked",
                                G_CALLBACK (on_remove_server_button_clicked),
                                data);

      g_free (name);
    }

  g_strfreev (uris);
  g_bookmark_file_free (server_list);
}

static void
update_view_mode (GtkPlacesView *view)
{
  GtkWidget *child;
  gboolean show_listbox;

  show_listbox = FALSE;

  /* drives */
  for (child = gtk_widget_get_first_child (GTK_WIDGET (view->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      /* GtkListBox filter rows by changing their GtkWidget::child-visible property */
      if (gtk_widget_get_child_visible (child))
        {
          show_listbox = TRUE;
          break;
        }
    }

  if (!show_listbox &&
      view->search_query &&
      view->search_query[0] != '\0')
    {
        gtk_stack_set_visible_child_name (GTK_STACK (view->stack), "empty-search");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (view->stack), "browse");
    }
}

static void
insert_row (GtkPlacesView *view,
            GtkWidget     *row,
            gboolean       is_network)
{
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;
  GtkGesture *gesture;

  g_object_set_data (G_OBJECT (row), "is-network", GINT_TO_POINTER (is_network));

  controller = gtk_shortcut_controller_new ();
  trigger = gtk_alternative_trigger_new (gtk_keyval_trigger_new (GDK_KEY_F10, GDK_SHIFT_MASK),
                                         gtk_keyval_trigger_new (GDK_KEY_Menu, 0));
  action = gtk_callback_action_new (on_row_popup_menu, row, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (GTK_WIDGET (row), controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (click_cb), row);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  g_signal_connect (gtk_places_view_row_get_eject_button (GTK_PLACES_VIEW_ROW (row)),
                    "clicked",
                    G_CALLBACK (on_eject_button_clicked),
                    row);

  gtk_places_view_row_set_path_size_group (GTK_PLACES_VIEW_ROW (row), view->path_size_group);
  gtk_places_view_row_set_space_size_group (GTK_PLACES_VIEW_ROW (row), view->space_size_group);

  gtk_list_box_insert (GTK_LIST_BOX (view->listbox), row, -1);
}

static void
add_volume (GtkPlacesView *view,
            GVolume       *volume)
{
  gboolean is_network;
  GMount *mount;
  GFile *root;
  GIcon *icon;
  char *identifier;
  char *name;
  char *path;

  if (is_external_volume (volume))
    return;

  identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);
  is_network = g_strcmp0 (identifier, "network") == 0;

  mount = g_volume_get_mount (volume);
  root = mount ? g_mount_get_default_location (mount) : NULL;
  icon = g_volume_get_icon (volume);
  name = g_volume_get_name (volume);
  path = !is_network ? g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE) : NULL;

  if (!mount || !g_mount_is_shadowed (mount))
    {
      GtkWidget *row;

      row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                          "icon", icon,
                          "name", name,
                          "path", path ? path : "",
                          "volume", volume,
                          "mount", mount,
                          "file", NULL,
                          "is-network", is_network,
                          NULL);

      insert_row (view, row, is_network);
    }

  g_clear_object (&root);
  g_clear_object (&icon);
  g_clear_object (&mount);
  g_free (identifier);
  g_free (name);
  g_free (path);
}

static void
add_mount (GtkPlacesView *view,
           GMount        *mount)
{
  gboolean is_network;
  GFile *root;
  GIcon *icon;
  char *name;
  char *path;
  char *uri;
  char *schema;

  icon = g_mount_get_icon (mount);
  name = g_mount_get_name (mount);
  root = g_mount_get_default_location (mount);
  path = root ? g_file_get_parse_name (root) : NULL;
  uri = g_file_get_uri (root);
  schema = g_uri_parse_scheme (uri);
  is_network = g_strcmp0 (schema, "file") != 0;

  if (is_network)
    g_clear_pointer (&path, g_free);

  if (!g_mount_is_shadowed (mount))
    {
      GtkWidget *row;

      row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                          "icon", icon,
                          "name", name,
                          "path", path ? path : "",
                          "volume", NULL,
                          "mount", mount,
                          "file", NULL,
                          "is-network", is_network,
                          NULL);

      insert_row (view, row, is_network);
    }

  g_clear_object (&root);
  g_clear_object (&icon);
  g_free (name);
  g_free (path);
  g_free (uri);
  g_free (schema);
}

static void
add_drive (GtkPlacesView *view,
           GDrive        *drive)
{
  GList *volumes;
  GList *l;

  volumes = g_drive_get_volumes (drive);

  for (l = volumes; l != NULL; l = l->next)
    add_volume (view, l->data);

  g_list_free_full (volumes, g_object_unref);
}

static void
add_file (GtkPlacesView *view,
          GFile         *file,
          GIcon         *icon,
          const char    *display_name,
          const char    *path,
          gboolean       is_network)
{
  GtkWidget *row;
  row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                      "icon", icon,
                      "name", display_name,
                      "path", path,
                      "volume", NULL,
                      "mount", NULL,
                      "file", file,
                      "is_network", is_network,
                      NULL);

  insert_row (view, row, is_network);
}

static gboolean
has_networks (GtkPlacesView *view)
{
  GtkWidget *child;
  gboolean has_network = FALSE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (view->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "is-network")) &&
          g_object_get_data (G_OBJECT (child), "is-placeholder") == NULL)
      {
        has_network = TRUE;
        break;
      }
    }

  return has_network;
}

static void
update_network_state (GtkPlacesView *view)
{
  if (view->network_placeholder == NULL)
    {
      view->network_placeholder = gtk_list_box_row_new ();
      view->network_placeholder_label = gtk_label_new ("");
      gtk_label_set_xalign (GTK_LABEL (view->network_placeholder_label), 0.0);
      gtk_widget_set_margin_start (view->network_placeholder_label, 12);
      gtk_widget_set_margin_end (view->network_placeholder_label, 12);
      gtk_widget_set_margin_top (view->network_placeholder_label, 6);
      gtk_widget_set_margin_bottom (view->network_placeholder_label, 6);
      gtk_widget_set_hexpand (view->network_placeholder_label, TRUE);
      gtk_widget_set_sensitive (view->network_placeholder, FALSE);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (view->network_placeholder),
                                  view->network_placeholder_label);
      g_object_set_data (G_OBJECT (view->network_placeholder),
                         "is-network", GINT_TO_POINTER (TRUE));
      /* mark the row as placeholder, so it always goes first */
      g_object_set_data (G_OBJECT (view->network_placeholder),
                         "is-placeholder", GINT_TO_POINTER (TRUE));
      gtk_list_box_insert (GTK_LIST_BOX (view->listbox), view->network_placeholder, -1);
    }

  if (gtk_places_view_get_fetching_networks (view))
    {
      /* only show a placeholder with a message if the list is empty.
       * otherwise just show the spinner in the header */
      if (!has_networks (view))
        {
          gtk_widget_set_visible (view->network_placeholder, TRUE);
          gtk_label_set_text (GTK_LABEL (view->network_placeholder_label),
                              _("Searching for network locations"));
        }
    }
  else if (!has_networks (view))
    {
      gtk_widget_set_visible (view->network_placeholder, TRUE);
      gtk_label_set_text (GTK_LABEL (view->network_placeholder_label),
                          _("No network locations found"));
    }
  else
    {
      gtk_widget_set_visible (view->network_placeholder, FALSE);
    }
}

static void
monitor_network (GtkPlacesView *view)
{
  GFile *network_file;
  GError *error;

  if (view->network_monitor)
    return;

  error = NULL;
  network_file = g_file_new_for_uri ("network:///");
  view->network_monitor = g_file_monitor (network_file,
                                          G_FILE_MONITOR_NONE,
                                          NULL,
                                          &error);

  g_clear_object (&network_file);

  if (error)
    {
      g_warning ("Error monitoring network: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_signal_connect_swapped (view->network_monitor,
                            "changed",
                            G_CALLBACK (update_places),
                            view);
}

static void
populate_networks (GtkPlacesView   *view,
                   GFileEnumerator *enumerator,
                   GList           *detected_networks)
{
  GList *l;
  GFile *file;
  GFile *activatable_file;
  char *uri;
  GFileType type;
  GIcon *icon;
  char *display_name;

  for (l = detected_networks; l != NULL; l = l->next)
    {
      file = g_file_enumerator_get_child (enumerator, l->data);
      type = g_file_info_get_file_type (l->data);
      if (type == G_FILE_TYPE_SHORTCUT || type == G_FILE_TYPE_MOUNTABLE)
        uri = g_file_info_get_attribute_as_string (l->data, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
      else
        uri = g_file_get_uri (file);
      activatable_file = g_file_new_for_uri (uri);
      display_name = g_file_info_get_attribute_as_string (l->data, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
      icon = g_file_info_get_icon (l->data);

      add_file (view, activatable_file, icon, display_name, NULL, TRUE);

      g_free (uri);
      g_free (display_name);
      g_clear_object (&file);
      g_clear_object (&activatable_file);
    }
}

static void
network_enumeration_next_files_finished (GObject      *source_object,
                                         GAsyncResult *res,
                                         gpointer      user_data)
{
  GtkPlacesView *view;
  GList *detected_networks;
  GError *error;

  view = GTK_PLACES_VIEW (user_data);
  error = NULL;

  detected_networks = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source_object),
                                                           res, &error);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          g_object_unref (view);
          return;
        }

      g_warning ("Failed to fetch network locations: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      gtk_places_view_set_fetching_networks (view, FALSE);
      populate_networks (view, G_FILE_ENUMERATOR (source_object), detected_networks);

      g_list_free_full (detected_networks, g_object_unref);
    }

  update_network_state (view);
  monitor_network (view);
  update_loading (view);

  g_object_unref (view);
}

static void
network_enumeration_finished (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (user_data);
  GFileEnumerator *enumerator;
  GError *error;

  error = NULL;
  enumerator = g_file_enumerate_children_finish (G_FILE (source_object), res, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("Failed to fetch network locations: %s", error->message);

      g_clear_error (&error);
      g_object_unref (view);
    }
  else
    {
      g_file_enumerator_next_files_async (enumerator,
                                          G_MAXINT32,
                                          G_PRIORITY_DEFAULT,
                                          view->networks_fetching_cancellable,
                                          network_enumeration_next_files_finished,
                                          user_data);
      g_object_unref (enumerator);
    }
}

static void
fetch_networks (GtkPlacesView *view)
{
  GFile *network_file;
  const char * const *supported_uris;
  gboolean found;

  supported_uris = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

  for (found = FALSE; !found && supported_uris && supported_uris[0]; supported_uris++)
    if (g_strcmp0 (supported_uris[0], "network") == 0)
      found = TRUE;

  if (!found)
    return;

  network_file = g_file_new_for_uri ("network:///");

  g_cancellable_cancel (view->networks_fetching_cancellable);
  g_clear_object (&view->networks_fetching_cancellable);
  view->networks_fetching_cancellable = g_cancellable_new ();
  gtk_places_view_set_fetching_networks (view, TRUE);
  update_network_state (view);

  g_object_ref (view);
  g_file_enumerate_children_async (network_file,
                                   "standard::type,standard::target-uri,standard::name,standard::display-name,standard::icon",
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   view->networks_fetching_cancellable,
                                   network_enumeration_finished,
                                   view);

  g_clear_object (&network_file);
}

static void
update_places (GtkPlacesView *view)
{
  GList *mounts;
  GList *volumes;
  GList *drives;
  GList *l;
  GIcon *icon;
  GFile *file;
  GtkWidget *child;

  /* Clear all previously added items */
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (view->listbox))))
    gtk_list_box_remove (GTK_LIST_BOX (view->listbox), child);

  view->network_placeholder = NULL;
  /* Inform clients that we started loading */
  gtk_places_view_set_loading (view, TRUE);

  /* Add "Computer" row */
  file = g_file_new_for_path ("/");
  icon = g_themed_icon_new_with_default_fallbacks ("drive-harddisk");

  add_file (view, file, icon, _("Computer"), "/", FALSE);

  g_clear_object (&file);
  g_clear_object (&icon);

  /* Add currently connected drives */
  drives = g_volume_monitor_get_connected_drives (view->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    add_drive (view, l->data);

  g_list_free_full (drives, g_object_unref);

  /*
   * Since all volumes with an associated GDrive were already added with
   * add_drive before, add all volumes that aren't associated with a
   * drive.
   */
  volumes = g_volume_monitor_get_volumes (view->volume_monitor);

  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume;
      GDrive *drive;

      volume = l->data;
      drive = g_volume_get_drive (volume);

      if (drive)
        {
          g_object_unref (drive);
          continue;
        }

      add_volume (view, volume);
    }

  g_list_free_full (volumes, g_object_unref);

  /*
   * Now that all necessary drives and volumes were already added, add mounts
   * that have no volume, such as /etc/mtab mounts, ftp, sftp, etc.
   */
  mounts = g_volume_monitor_get_mounts (view->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      GMount *mount;
      GVolume *volume;

      mount = l->data;
      volume = g_mount_get_volume (mount);

      if (volume)
        {
          g_object_unref (volume);
          continue;
        }

      add_mount (view, mount);
    }

  g_list_free_full (mounts, g_object_unref);

  /* load saved servers */
  populate_servers (view);

  /* fetch networks and add them asynchronously */
  fetch_networks (view);

  update_view_mode (view);
  /* Check whether we still are in a loading state */
  update_loading (view);
}

static void
server_mount_ready_cb (GObject      *source_file,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (user_data);
  gboolean should_show;
  GError *error;
  GFile *location;

  location = G_FILE (source_file);
  should_show = TRUE;
  error = NULL;

  g_file_mount_enclosing_volume_finish (location, res, &error);
  if (error)
    {
      should_show = FALSE;

      if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
        {
          /*
           * Already mounted volume is not a critical error
           * and we can still continue with the operation.
           */
          should_show = TRUE;
        }
      else if (error->domain != G_IO_ERROR ||
               (error->code != G_IO_ERROR_CANCELLED &&
                error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (view, _("Unable to access location"), error->message);
        }

      /* The operation got cancelled by the user and or the error
         has been handled already. */
      g_clear_error (&error);
    }

  if (view->destroyed)
    {
      g_object_unref (view);
      return;
    }

  view->should_pulse_entry = FALSE;
  gtk_entry_set_progress_fraction (GTK_ENTRY (view->address_entry), 0);

  /* Restore from Cancel to Connect */
  gtk_button_set_label (GTK_BUTTON (view->connect_button), _("Con_nect"));
  gtk_widget_set_sensitive (view->address_entry, TRUE);
  view->connecting_to_server = FALSE;

  if (should_show)
    {
      server_list_add_server (view, location);

      /*
       * Only clear the entry if it successfully connects to the server.
       * Otherwise, the user would lost the typed address even if it fails
       * to connect.
       */
      gtk_editable_set_text (GTK_EDITABLE (view->address_entry), "");

      if (view->should_open_location)
        {
          GMount *mount;
          GFile *root;

          /*
           * If the mount is not found at this point, it is probably user-
           * invisible, which happens e.g for smb-browse, but the location
           * should be opened anyway...
           */
          mount = g_file_find_enclosing_mount (location, view->cancellable, NULL);
          if (mount)
            {
              root = g_mount_get_default_location (mount);

              emit_open_location (view, root, view->open_flags);

              g_object_unref (root);
              g_object_unref (mount);
            }
          else
            {
              emit_open_location (view, location, view->open_flags);
            }
        }
    }

  update_places (view);
  g_object_unref (view);
}

static void
volume_mount_ready_cb (GObject      *source_volume,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (user_data);
  gboolean should_show;
  GVolume *volume;
  GError *error;

  volume = G_VOLUME (source_volume);
  should_show = TRUE;
  error = NULL;

  g_volume_mount_finish (volume, res, &error);

  if (error)
    {
      should_show = FALSE;

      if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
        {
          /*
           * If the volume was already mounted, it's not a hard error
           * and we can still continue with the operation.
           */
          should_show = TRUE;
        }
      else if (error->domain != G_IO_ERROR ||
               (error->code != G_IO_ERROR_CANCELLED &&
                error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (GTK_PLACES_VIEW (user_data), _("Unable to access location"), error->message);
          should_show = FALSE;
        }

      /* The operation got cancelled by the user and or the error
         has been handled already. */
      g_clear_error (&error);
    }

  if (view->destroyed)
    {
      g_object_unref(view);
      return;
    }

  view->row_for_action = NULL;
  view->mounting_volume = FALSE;
  update_loading (view);

  if (should_show)
    {
      GMount *mount;
      GFile *root;

      mount = g_volume_get_mount (volume);
      root = g_mount_get_default_location (mount);

      if (view->should_open_location)
        emit_open_location (GTK_PLACES_VIEW (user_data), root, view->open_flags);

      g_object_unref (mount);
      g_object_unref (root);
    }

  update_places (view);
  g_object_unref (view);
}

static void
unmount_ready_cb (GObject      *source_mount,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GtkPlacesView *view;
  GMount *mount;
  GError *error;

  view = GTK_PLACES_VIEW (user_data);
  mount = G_MOUNT (source_mount);
  error = NULL;

  g_mount_unmount_with_operation_finish (mount, res, &error);

  if (error)
    {
      if (error->domain != G_IO_ERROR ||
          (error->code != G_IO_ERROR_CANCELLED &&
           error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (view, _("Unable to unmount volume"), error->message);
        }

      g_clear_error (&error);
    }

  if (view->destroyed) {
    g_object_unref (view);
    return;
  }

  view->unmounting_mount = FALSE;
  update_loading (view);

  g_object_unref (view);
}

static gboolean
pulse_entry_cb (gpointer user_data)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (user_data);

  if (view->destroyed)
    {
      view->entry_pulse_timeout_id = 0;

      return G_SOURCE_REMOVE;
    }
  else if (view->should_pulse_entry)
    {
      gtk_entry_progress_pulse (GTK_ENTRY (view->address_entry));

      return G_SOURCE_CONTINUE;
    }
  else
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (view->address_entry), 0);
      view->entry_pulse_timeout_id = 0;

      return G_SOURCE_REMOVE;
    }
}

static void
unmount_mount (GtkPlacesView *view,
               GMount        *mount)
{
  GMountOperation *operation;
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (view)));

  g_cancellable_cancel (view->cancellable);
  g_clear_object (&view->cancellable);
  view->cancellable = g_cancellable_new ();

  view->unmounting_mount = TRUE;
  update_loading (view);

  g_object_ref (view);

  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));
  g_mount_unmount_with_operation (mount,
                                  0,
                                  operation,
                                  view->cancellable,
                                  unmount_ready_cb,
                                  view);
  g_object_unref (operation);
}

static void
mount_server (GtkPlacesView *view,
              GFile         *location)
{
  GMountOperation *operation;
  GtkWidget *toplevel;

  g_cancellable_cancel (view->cancellable);
  g_clear_object (&view->cancellable);
  /* User cliked when the operation was ongoing, so wanted to cancel it */
  if (view->connecting_to_server)
    return;

  view->cancellable = g_cancellable_new ();
  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (view)));
  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

  view->should_pulse_entry = TRUE;
  gtk_entry_set_progress_pulse_step (GTK_ENTRY (view->address_entry), 0.1);
  gtk_entry_set_progress_fraction (GTK_ENTRY (view->address_entry), 0.1);
  /* Allow to cancel the operation */
  gtk_button_set_label (GTK_BUTTON (view->connect_button), _("Cance_l"));
  gtk_widget_set_sensitive (view->address_entry, FALSE);
  view->connecting_to_server = TRUE;
  update_loading (view);

  if (view->entry_pulse_timeout_id == 0)
    view->entry_pulse_timeout_id = g_timeout_add (100, (GSourceFunc) pulse_entry_cb, view);

  g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_FOR_SESSION);

  /* make sure we keep the view around for as long as we are running */
  g_object_ref (view);

  g_file_mount_enclosing_volume (location,
                                 0,
                                 operation,
                                 view->cancellable,
                                 server_mount_ready_cb,
                                 view);

  /* unref operation here - g_file_mount_enclosing_volume() does ref for itself */
  g_object_unref (operation);
}

static void
mount_volume (GtkPlacesView *view,
              GVolume       *volume)
{
  GMountOperation *operation;
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (view)));
  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

  g_cancellable_cancel (view->cancellable);
  g_clear_object (&view->cancellable);
  view->cancellable = g_cancellable_new ();

  view->mounting_volume = TRUE;
  update_loading (view);

  g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_FOR_SESSION);

  /* make sure we keep the view around for as long as we are running */
  g_object_ref (view);

  g_volume_mount (volume,
                  0,
                  operation,
                  view->cancellable,
                  volume_mount_ready_cb,
                  view);

  /* unref operation here - g_file_mount_enclosing_volume() does ref for itself */
  g_object_unref (operation);
}

static void
open_cb (GtkWidget  *widget,
         const char *action_name,
         GVariant   *parameter)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (widget);
  GtkPlacesOpenFlags flags = GTK_PLACES_OPEN_NORMAL;

  if (view->row_for_action == NULL)
    return;

  if (strcmp (action_name, "location.open") == 0)
    flags = GTK_PLACES_OPEN_NORMAL;
  else if (strcmp (action_name, "location.open-tab") == 0)
    flags = GTK_PLACES_OPEN_NEW_TAB;
  else if (strcmp (action_name, "location.open-window") == 0)
    flags = GTK_PLACES_OPEN_NEW_WINDOW;

  activate_row (view, view->row_for_action, flags);
}

static void
mount_cb (GtkWidget  *widget,
          const char *action_name,
          GVariant   *parameter)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (widget);
  GVolume *volume;

  if (view->row_for_action == NULL)
    return;

  volume = gtk_places_view_row_get_volume (view->row_for_action);

  /*
   * When the mount item is activated, it's expected that
   * the volume only gets mounted, without opening it after
   * the operation is complete.
   */
  view->should_open_location = FALSE;

  gtk_places_view_row_set_busy (view->row_for_action, TRUE);
  mount_volume (view, volume);
}

static void
unmount_cb (GtkWidget  *widget,
            const char *action_name,
            GVariant   *parameter)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (widget);
  GMount *mount;

  if (view->row_for_action == NULL)
    return;

  mount = gtk_places_view_row_get_mount (view->row_for_action);

  gtk_places_view_row_set_busy (view->row_for_action, TRUE);

  unmount_mount (view, mount);
}

static void
attach_protocol_row_to_grid (GtkGrid     *grid,
                             const char *protocol_name,
                             const char *protocol_prefix)
{
  GtkWidget *name_label;
  GtkWidget *prefix_label;

  name_label = gtk_label_new (protocol_name);
  gtk_widget_set_halign (name_label, GTK_ALIGN_START);
  gtk_grid_attach_next_to (grid, name_label, NULL, GTK_POS_BOTTOM, 1, 1);

  prefix_label = gtk_label_new (protocol_prefix);
  gtk_widget_set_halign (prefix_label, GTK_ALIGN_START);
  gtk_grid_attach_next_to (grid, prefix_label, name_label, GTK_POS_RIGHT, 1, 1);
}

static void
populate_available_protocols_grid (GtkGrid *grid)
{
  const char * const *supported_protocols;
  gboolean has_any = FALSE;

  supported_protocols = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

  if (g_strv_contains (supported_protocols, "afp"))
    {
      attach_protocol_row_to_grid (grid, _("AppleTalk"), "afp://");
      has_any = TRUE;
    }

  if (g_strv_contains (supported_protocols, "ftp"))
    {
      attach_protocol_row_to_grid (grid, _("File Transfer Protocol"),
                                   /* Translators: do not translate ftp:// and ftps:// */
                                   _("ftp:// or ftps://"));
      has_any = TRUE;
    }

  if (g_strv_contains (supported_protocols, "nfs"))
    {
      attach_protocol_row_to_grid (grid, _("Network File System"), "nfs://");
      has_any = TRUE;
    }

  if (g_strv_contains (supported_protocols, "smb"))
    {
      attach_protocol_row_to_grid (grid, _("Samba"), "smb://");
      has_any = TRUE;
    }

  if (g_strv_contains (supported_protocols, "ssh"))
    {
      attach_protocol_row_to_grid (grid, _("SSH File Transfer Protocol"),
                                   /* Translators: do not translate sftp:// and ssh:// */
                                   _("sftp:// or ssh://"));
      has_any = TRUE;
    }

  if (g_strv_contains (supported_protocols, "dav"))
    {
      attach_protocol_row_to_grid (grid, _("WebDAV"),
                                   /* Translators: do not translate dav:// and davs:// */
                                   _("dav:// or davs://"));
      has_any = TRUE;
    }

  if (!has_any)
    gtk_widget_set_visible (GTK_WIDGET (grid), FALSE);
}

static GMenuModel *
get_menu_model (void)
{
  GMenu *menu;
  GMenu *section;
  GMenuItem *item;

  menu = g_menu_new ();
  section = g_menu_new ();
  item = g_menu_item_new (_("_Open"), "location.open");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new (_("Open in New _Tab"), "location.open-tab");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new (_("Open in New _Window"), "location.open-window");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();
  item = g_menu_item_new (_("_Disconnect"), "location.disconnect");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new (_("_Unmount"), "location.unmount");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);


  item = g_menu_item_new (_("_Connect"), "location.connect");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new (_("_Mount"), "location.mount");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  return G_MENU_MODEL (menu);
}

static void
_popover_set_pointing_to_widget (GtkPopover *popover,
                                 GtkWidget  *target)
{
  GtkWidget *parent;
  graphene_point_t p;
  double w, h;

  parent = gtk_widget_get_parent (GTK_WIDGET (popover));

  if (!gtk_widget_compute_point (target, parent, &GRAPHENE_POINT_INIT (0, 0), &p))
    return;

  w = gtk_widget_get_width (target);
  h = gtk_widget_get_height (target);

  gtk_popover_set_pointing_to (popover, &(GdkRectangle){p.x, p.y, w, h});
}

static gboolean
real_popup_menu (GtkWidget *widget,
                 double x,
                 double y)
{
  GtkPlacesViewRow *row = GTK_PLACES_VIEW_ROW (widget);
  GtkPlacesView *view;
  GMount *mount;
  GFile *file;
  gboolean is_network;

  view = GTK_PLACES_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW));

  mount = gtk_places_view_row_get_mount (row);
  file = gtk_places_view_row_get_file (row);
  is_network = gtk_places_view_row_get_is_network (row);

  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.disconnect",
                                 !file && mount && is_network);
  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.unmount",
                                 !file && mount && !is_network);
  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.connect",
                                 !file && !mount && is_network);
  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.mount",
                                 !file && !mount && !is_network);

  if (!view->popup_menu)
    {
      GMenuModel *model = get_menu_model ();

      view->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_popover_set_position (GTK_POPOVER (view->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (view->popup_menu), FALSE);
      gtk_widget_set_halign (view->popup_menu, GTK_ALIGN_START);
      gtk_widget_set_parent (view->popup_menu, GTK_WIDGET (view));

      g_object_unref (model);
    }

  if (view->row_for_action)
    g_object_set_data (G_OBJECT (view->row_for_action), "menu", NULL);

  if (x == -1 && y == -1)
    _popover_set_pointing_to_widget (GTK_POPOVER (view->popup_menu), GTK_WIDGET (row));
  else
    {
      graphene_point_t p;
      if (!gtk_widget_compute_point (widget, GTK_WIDGET (view),
                                     &GRAPHENE_POINT_INIT (x, y),
                                     &p))
        graphene_point_init (&p, x, y);
      gtk_popover_set_pointing_to (GTK_POPOVER (view->popup_menu),
                                   &(GdkRectangle){p.x, p.y, 0, 0});
    }

  view->row_for_action = row;
  if (view->row_for_action)
    g_object_set_data (G_OBJECT (view->row_for_action), "menu", view->popup_menu);

  gtk_popover_popup (GTK_POPOVER (view->popup_menu));

  return TRUE;
}

static gboolean
on_row_popup_menu (GtkWidget *widget,
                   GVariant *args,
                   gpointer user_data)
{
  return real_popup_menu (widget, -1, -1);
}

static void
click_cb (GtkGesture *gesture,
          int         n_press,
          double      x,
          double      y,
          gpointer    user_data)
{
  real_popup_menu (GTK_WIDGET (user_data), x, y);
}

static gboolean
on_key_press_event (GtkEventController *controller,
                    guint               keyval,
                    guint               keycode,
                    GdkModifierType     state,
                    GtkPlacesView      *view)
{
  GdkModifierType modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_space)
    {
      GtkWidget *focus_widget;
      GtkWindow *toplevel;

      view->current_open_flags = GTK_PLACES_OPEN_NORMAL;
      toplevel = get_toplevel (GTK_WIDGET (view));

      if (!toplevel)
        return FALSE;

      focus_widget = gtk_root_get_focus (GTK_ROOT (toplevel));

      if (!GTK_IS_PLACES_VIEW_ROW (focus_widget))
        return FALSE;

      if ((state & modifiers) == GDK_SHIFT_MASK)
        view->current_open_flags = GTK_PLACES_OPEN_NEW_TAB;
      else if ((state & modifiers) == GDK_CONTROL_MASK)
        view->current_open_flags = GTK_PLACES_OPEN_NEW_WINDOW;

      activate_row (view, GTK_PLACES_VIEW_ROW (focus_widget), view->current_open_flags);

      return TRUE;
    }

  return FALSE;
}

static void
on_middle_click_row_event (GtkGestureClick *gesture,
                           guint            n_press,
                           double           x,
                           double           y,
                           GtkPlacesView   *view)
{
  GtkListBoxRow *row;

  if (n_press != 1)
    return;

  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (view->listbox), y);
  if (row != NULL && gtk_widget_is_sensitive (GTK_WIDGET (row)))
    activate_row (view, GTK_PLACES_VIEW_ROW (row), GTK_PLACES_OPEN_NEW_TAB);
}


static void
on_eject_button_clicked (GtkWidget        *widget,
                         GtkPlacesViewRow *row)
{
  if (row)
    {
      GtkWidget *view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);

      unmount_mount (GTK_PLACES_VIEW (view), gtk_places_view_row_get_mount (row));
    }
}

static void
on_connect_button_clicked (GtkPlacesView *view)
{
  const char *uri;
  GFile *file;

  file = NULL;

  /*
   * Since the 'Connect' button is updated whenever the typed
   * address changes, it is sufficient to check if it's sensitive
   * or not, in order to determine if the given address is valid.
   */
  if (!gtk_widget_get_sensitive (view->connect_button))
    return;

  uri = gtk_editable_get_text (GTK_EDITABLE (view->address_entry));

  if (uri != NULL && uri[0] != '\0')
    file = g_file_new_for_commandline_arg (uri);

  if (file)
    {
      view->should_open_location = TRUE;

      mount_server (view, file);
    }
  else
    {
      emit_show_error_message (view, _("Unable to get remote server location"), NULL);
    }
}

static void
on_address_entry_text_changed (GtkPlacesView *view)
{
  const char * const *supported_protocols;
  char *address, *scheme;
  gboolean supported;

  supported = FALSE;
  supported_protocols = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
  address = g_strdup (gtk_editable_get_text (GTK_EDITABLE (view->address_entry)));
  scheme = g_uri_parse_scheme (address);

  if (!supported_protocols)
    goto out;

  if (!scheme)
    goto out;

  supported = g_strv_contains (supported_protocols, scheme) &&
              !g_strv_contains (unsupported_protocols, scheme);

out:
  gtk_widget_set_sensitive (view->connect_button, supported);
  if (scheme && !supported)
    gtk_widget_add_css_class (view->address_entry, "error");
  else
    gtk_widget_remove_css_class (view->address_entry, "error");

  g_free (address);
  g_free (scheme);
}

static void
on_address_entry_show_help_pressed (GtkPlacesView        *view,
                                    GtkEntryIconPosition  icon_pos,
                                    GtkEntry             *entry)
{
  GdkRectangle rect;
  graphene_point_t p;

  /* Setup the auxiliary popover's rectangle */
  gtk_entry_get_icon_area (GTK_ENTRY (view->address_entry),
                           GTK_ENTRY_ICON_SECONDARY,
                           &rect);
  if (!gtk_widget_compute_point (view->address_entry, GTK_WIDGET (view),
                                 &GRAPHENE_POINT_INIT (rect.x, rect.y), &p))
    graphene_point_init (&p, rect.x, rect.y);

  rect.x = p.x;
  rect.y = p.y;
  gtk_popover_set_pointing_to (GTK_POPOVER (view->server_adresses_popover), &rect);
  gtk_widget_set_visible (view->server_adresses_popover, TRUE);
}

static void
on_recent_servers_listbox_row_activated (GtkPlacesView    *view,
                                         GtkPlacesViewRow *row,
                                         GtkWidget        *listbox)
{
  char *uri;

  uri = g_object_get_data (G_OBJECT (row), "uri");

  gtk_editable_set_text (GTK_EDITABLE (view->address_entry), uri);

  gtk_widget_set_visible (view->recent_servers_popover, FALSE);
}

static void
on_listbox_row_activated (GtkPlacesView    *view,
                          GtkPlacesViewRow *row,
                          GtkWidget        *listbox)
{
  activate_row (view, row, view->current_open_flags);
}

static gboolean
listbox_filter_func (GtkListBoxRow *row,
                     gpointer       user_data)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (user_data);
  gboolean is_placeholder;
  gboolean retval;
  gboolean searching;
  char *name;
  char *path;

  retval = FALSE;
  searching = view->search_query && view->search_query[0] != '\0';

  is_placeholder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-placeholder"));

  if (is_placeholder && searching)
    return FALSE;

  if (!searching)
    return TRUE;

  g_object_get (row,
                "name", &name,
                "path", &path,
                NULL);

  if (name)
    {
      char *lowercase_name = g_utf8_strdown (name, -1);

      retval |= strstr (lowercase_name, view->search_query) != NULL;

      g_free (lowercase_name);
    }

  if (path)
    {
      char *lowercase_path = g_utf8_strdown (path, -1);

      retval |= strstr (lowercase_path, view->search_query) != NULL;

      g_free (lowercase_path);
    }

  g_free (name);
  g_free (path);

  return retval;
}

static void
listbox_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  gboolean row_is_network;
  char *text;

  text = NULL;
  row_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-network"));

  if (!before)
    {
      text = g_strdup_printf ("<b>%s</b>", row_is_network ? _("Networks") : _("On This Computer"));
    }
  else
    {
      gboolean before_is_network;

      before_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (before), "is-network"));

      if (before_is_network != row_is_network)
        text = g_strdup_printf ("<b>%s</b>", row_is_network ? _("Networks") : _("On This Computer"));
    }

  if (text)
    {
      GtkWidget *header;
      GtkWidget *label;
      GtkWidget *separator;

      header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_set_margin_top (header, 6);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

      label = g_object_new (GTK_TYPE_LABEL,
                            "use_markup", TRUE,
                            "margin-start", 12,
                            "label", text,
                            "xalign", 0.0f,
                            NULL);
      if (row_is_network)
        {
          GtkWidget *header_name;
          GtkWidget *network_header_spinner;

          gtk_widget_set_margin_end (label, 6);

          header_name = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          network_header_spinner = gtk_spinner_new ();
          gtk_widget_set_margin_end (network_header_spinner, 12);
          g_object_bind_property (GTK_PLACES_VIEW (user_data),
                                  "fetching-networks",
                                  network_header_spinner,
                                  "spinning",
                                  G_BINDING_SYNC_CREATE);

          gtk_box_append (GTK_BOX (header_name), label);
          gtk_box_append (GTK_BOX (header_name), network_header_spinner);
          gtk_box_append (GTK_BOX (header), header_name);
        }
      else
        {
          gtk_widget_set_hexpand (label, TRUE);
          gtk_widget_set_margin_end (label, 12);
          gtk_box_append (GTK_BOX (header), label);
        }

      gtk_box_append (GTK_BOX (header), separator);

      gtk_list_box_row_set_header (row, header);

      g_free (text);
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

static int
listbox_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  gboolean row1_is_network;
  gboolean row2_is_network;
  char *path1;
  char *path2;
  gboolean *is_placeholder1;
  gboolean *is_placeholder2;
  int retval;

  row1_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row1), "is-network"));
  row2_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row2), "is-network"));

  retval = row1_is_network - row2_is_network;

  if (retval != 0)
    return retval;

  is_placeholder1 = g_object_get_data (G_OBJECT (row1), "is-placeholder");
  is_placeholder2 = g_object_get_data (G_OBJECT (row2), "is-placeholder");

  /* we can't have two placeholders for the same section */
  g_assert (!(is_placeholder1 != NULL && is_placeholder2 != NULL));

  if (is_placeholder1)
    return -1;
  if (is_placeholder2)
    return 1;

  g_object_get (row1, "path", &path1, NULL);
  g_object_get (row2, "path", &path2, NULL);

  retval = g_utf8_collate (path1, path2);

  g_free (path1);
  g_free (path2);

  return retval;
}

static void
gtk_places_view_constructed (GObject *object)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (object);

  G_OBJECT_CLASS (gtk_places_view_parent_class)->constructed (object);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (view->listbox),
                              listbox_sort_func,
                              object,
                              NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (view->listbox),
                                listbox_filter_func,
                                object,
                                NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (view->listbox),
                                listbox_header_func,
                                object,
                                NULL);

  /* load drives */
  update_places (view);

  g_signal_connect_swapped (view->volume_monitor,
                            "mount-added",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (view->volume_monitor,
                            "mount-changed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (view->volume_monitor,
                            "mount-removed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (view->volume_monitor,
                            "volume-added",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (view->volume_monitor,
                            "volume-changed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (view->volume_monitor,
                            "volume-removed",
                            G_CALLBACK (update_places),
                            object);
}

static void
gtk_places_view_map (GtkWidget *widget)
{
  GtkPlacesView *view = GTK_PLACES_VIEW (widget);

  gtk_editable_set_text (GTK_EDITABLE (view->address_entry), "");

  GTK_WIDGET_CLASS (gtk_places_view_parent_class)->map (widget);
}

static void
gtk_places_view_class_init (GtkPlacesViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_places_view_finalize;
  object_class->dispose = gtk_places_view_dispose;
  object_class->constructed = gtk_places_view_constructed;
  object_class->get_property = gtk_places_view_get_property;
  object_class->set_property = gtk_places_view_set_property;

  widget_class->map = gtk_places_view_map;

  /*
   * GtkPlacesView::open-location:
   * @view: the object which received the signal.
   * @location: (type Gio.File): GFile to which the caller should switch.
   * @open_flags: a single value from GtkPlacesOpenFlags specifying how the @location
   * should be opened.
   *
   * The places view emits this signal when the user selects a location
   * in it. The calling application should display the contents of that
   * location; for example, a file manager should show a list of files in
   * the specified location.
   */
  places_view_signals [OPEN_LOCATION] =
          g_signal_new (I_("open-location"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesViewClass, open_location),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_FLAGS,
                        G_TYPE_NONE, 2,
                        G_TYPE_OBJECT,
                        GTK_TYPE_PLACES_OPEN_FLAGS);
  g_signal_set_va_marshaller (places_view_signals [OPEN_LOCATION],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__OBJECT_FLAGSv);

  /*
   * GtkPlacesView::show-error-message:
   * @view: the object which received the signal.
   * @primary: primary message with a summary of the error to show.
   * @secondary: secondary message with details of the error to show.
   *
   * The places view emits this signal when it needs the calling
   * application to present an error message.  Most of these messages
   * refer to mounting or unmounting media, for example, when a drive
   * cannot be started for some reason.
   */
  places_view_signals [SHOW_ERROR_MESSAGE] =
          g_signal_new (I_("show-error-message"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesViewClass, show_error_message),
                        NULL, NULL,
                        _gtk_marshal_VOID__STRING_STRING,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING,
                        G_TYPE_STRING);
  g_signal_set_va_marshaller (places_view_signals [SHOW_ERROR_MESSAGE],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__STRING_STRINGv);

  properties[PROP_LOADING] =
          g_param_spec_boolean ("loading", NULL, NULL,
                                FALSE,
                                GTK_PARAM_READABLE);

  properties[PROP_FETCHING_NETWORKS] =
          g_param_spec_boolean ("fetching-networks", NULL, NULL,
                                FALSE,
                                GTK_PARAM_READABLE);

  properties[PROP_OPEN_FLAGS] =
          g_param_spec_flags ("open-flags", NULL, NULL,
                              GTK_TYPE_PLACES_OPEN_FLAGS,
                              GTK_PLACES_OPEN_NORMAL,
                              GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkplacesview.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, actionbar);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, address_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, connect_button);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, listbox);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, recent_servers_listbox);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, recent_servers_popover);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, recent_servers_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, server_adresses_popover);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesView, available_protocols_grid);

  gtk_widget_class_bind_template_callback (widget_class, on_address_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_address_entry_show_help_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_connect_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_recent_servers_listbox_row_activated);

  /**
   * GtkPlacesView|location.open:
   *
   * Opens the location in the current window.
   */
  gtk_widget_class_install_action (widget_class, "location.open", NULL, open_cb);

  /**
   * GtkPlacesView|location.open-tab:
   *
   * Opens the location in a new tab.
   */
  gtk_widget_class_install_action (widget_class, "location.open-tab", NULL, open_cb);

  /**
   * GtkPlacesView|location.open-window:
   *
   * Opens the location in a new window.
   */
  gtk_widget_class_install_action (widget_class, "location.open-window", NULL, open_cb);

  /**
   * GtkPlacesView|location.mount:
   *
   * Mount the location.
   */
  gtk_widget_class_install_action (widget_class, "location.mount", NULL, mount_cb);

  /**
   * GtkPlacesView|location.connect:
   *
   * Connect the location.
   */
  gtk_widget_class_install_action (widget_class, "location.connect", NULL, mount_cb);

  /**
   * GtkPlacesView|location.unmount:
   *
   * Unmount the location.
   */
  gtk_widget_class_install_action (widget_class, "location.unmount", NULL, unmount_cb);

  /**
   * GtkPlacesView|location.disconnect:
   *
   * Disconnect the location.
   */
  gtk_widget_class_install_action (widget_class, "location.disconnect", NULL, unmount_cb);

  gtk_widget_class_set_css_name (widget_class, I_("placesview"));
}

static void
gtk_places_view_init (GtkPlacesView *self)
{
  GtkEventController *controller;

  self->volume_monitor = g_volume_monitor_get ();
  self->open_flags = GTK_PLACES_OPEN_NORMAL;
  self->path_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->space_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "location.open-tab", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "location.open-window", FALSE);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_parent (self->server_adresses_popover, GTK_WIDGET (self));
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (on_key_press_event), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  /* We need an additional controller because GtkListBox only
   * activates rows for GDK_BUTTON_PRIMARY clicks
   */
  controller = (GtkEventController *) gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), GDK_BUTTON_MIDDLE);
  g_signal_connect (controller, "released",
                    G_CALLBACK (on_middle_click_row_event), self);
  gtk_widget_add_controller (self->listbox, controller);

  populate_available_protocols_grid (GTK_GRID (self->available_protocols_grid));
}

/*
 * gtk_places_view_new:
 *
 * Creates a new GtkPlacesView widget.
 *
 * The application should connect to at least the
 * GtkPlacesView::open-location signal to be notified
 * when the user makes a selection in the view.
 *
 * Returns: a newly created GtkPlacesView
 */
GtkWidget *
gtk_places_view_new (void)
{
  return g_object_new (GTK_TYPE_PLACES_VIEW, NULL);
}

/*
 * gtk_places_view_set_open_flags:
 * @view: a GtkPlacesView
 * @flags: Bitmask of modes in which the calling application can open locations
 *
 * Sets the way in which the calling application can open new locations from
 * the places view.  For example, some applications only open locations
 * “directly” into their main view, while others may support opening locations
 * in a new notebook tab or a new window.
 *
 * This function is used to tell the places @view about the ways in which the
 * application can open new locations, so that the view can display (or not)
 * the “Open in new tab” and “Open in new window” menu items as appropriate.
 *
 * When the GtkPlacesView::open-location signal is emitted, its flags
 * argument will be set to one of the @flags that was passed in
 * gtk_places_view_set_open_flags().
 *
 * Passing 0 for @flags will cause GTK_PLACES_OPEN_NORMAL to always be sent
 * to callbacks for the “open-location” signal.
 */
void
gtk_places_view_set_open_flags (GtkPlacesView      *view,
                                GtkPlacesOpenFlags  flags)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  if (view->open_flags == flags)
    return;

  view->open_flags = flags;

  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.open-tab",
                                 (flags & GTK_PLACES_OPEN_NEW_TAB) != 0);
  gtk_widget_action_set_enabled (GTK_WIDGET (view), "location.open-window",
                                 (flags & GTK_PLACES_OPEN_NEW_WINDOW) != 0);

  g_object_notify_by_pspec (G_OBJECT (view), properties[PROP_OPEN_FLAGS]);
}

/*
 * gtk_places_view_get_open_flags:
 * @view: a GtkPlacesSidebar
 *
 * Gets the open flags.
 *
 * Returns: the GtkPlacesOpenFlags of @view
 */
GtkPlacesOpenFlags
gtk_places_view_get_open_flags (GtkPlacesView *view)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), 0);

  return view->open_flags;
}

/*
 * gtk_places_view_get_search_query:
 * @view: a GtkPlacesView
 *
 * Retrieves the current search query from @view.
 *
 * Returns: (transfer none): the current search query.
 */
const char *
gtk_places_view_get_search_query (GtkPlacesView *view)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), NULL);

  return view->search_query;
}

/*
 * gtk_places_view_set_search_query:
 * @view: a GtkPlacesView
 * @query_text: the query, or NULL.
 *
 * Sets the search query of @view. The search is immediately performed
 * once the query is set.
 */
void
gtk_places_view_set_search_query (GtkPlacesView *view,
                                  const char    *query_text)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  if (g_strcmp0 (view->search_query, query_text) != 0)
    {
      g_clear_pointer (&view->search_query, g_free);
      view->search_query = g_utf8_strdown (query_text, -1);

      gtk_list_box_invalidate_filter (GTK_LIST_BOX (view->listbox));
      gtk_list_box_invalidate_headers (GTK_LIST_BOX (view->listbox));

      update_view_mode (view);
    }
}

/*
 * gtk_places_view_get_loading:
 * @view: a GtkPlacesView
 *
 * Returns %TRUE if the view is loading locations.
 */
gboolean
gtk_places_view_get_loading (GtkPlacesView *view)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), FALSE);

  return view->loading;
}

static void
update_loading (GtkPlacesView *view)
{
  gboolean loading;

  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  loading = view->fetching_networks || view->connecting_to_server ||
            view->mounting_volume || view->unmounting_mount;

  set_busy_cursor (view, loading);
  gtk_places_view_set_loading (view, loading);
}

static void
gtk_places_view_set_loading (GtkPlacesView *view,
                             gboolean       loading)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  if (view->loading != loading)
    {
      view->loading = loading;
      g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_LOADING]);
    }
}

static gboolean
gtk_places_view_get_fetching_networks (GtkPlacesView *view)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), FALSE);

  return view->fetching_networks;
}

static void
gtk_places_view_set_fetching_networks (GtkPlacesView *view,
                                       gboolean       fetching_networks)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  if (view->fetching_networks != fetching_networks)
    {
      view->fetching_networks = fetching_networks;
      g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_FETCHING_NETWORKS]);
    }
}
