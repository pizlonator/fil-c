/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.c: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
 * Copyright (C) 2003, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gtkfilechooserutils.h"
#include "deprecated/gtkfilechooser.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include <glib/gi18n-lib.h>


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static gboolean       delegate_set_current_folder     (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static GFile *        delegate_get_current_folder     (GtkFileChooser    *chooser);
static void           delegate_set_current_name       (GtkFileChooser    *chooser,
						       const char        *name);
static char *        delegate_get_current_name       (GtkFileChooser    *chooser);
static gboolean       delegate_select_file            (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static void           delegate_unselect_file          (GtkFileChooser    *chooser,
						       GFile             *file);
static void           delegate_select_all             (GtkFileChooser    *chooser);
static void           delegate_unselect_all           (GtkFileChooser    *chooser);
static GListModel *   delegate_get_files              (GtkFileChooser    *chooser);
static void           delegate_add_filter             (GtkFileChooser    *chooser,
						       GtkFileFilter     *filter);
static void           delegate_remove_filter          (GtkFileChooser    *chooser,
						       GtkFileFilter     *filter);
static GListModel *   delegate_get_filters            (GtkFileChooser    *chooser);
static gboolean       delegate_add_shortcut_folder    (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static gboolean       delegate_remove_shortcut_folder (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static GListModel *   delegate_get_shortcut_folders   (GtkFileChooser    *chooser);
static void           delegate_notify                 (GObject           *object,
						       GParamSpec        *pspec,
						       gpointer           data);

static void           delegate_add_choice             (GtkFileChooser  *chooser,
                                                       const char      *id,
                                                       const char      *label,
                                                       const char     **options,
                                                       const char     **option_labels);
static void           delegate_remove_choice          (GtkFileChooser  *chooser,
                                                       const char      *id);
static void           delegate_set_choice             (GtkFileChooser  *chooser,
                                                       const char      *id,
                                                       const char      *option);
static const char *   delegate_get_choice             (GtkFileChooser  *chooser,
                                                       const char      *id);


/**
 * _gtk_file_chooser_install_properties:
 * @klass: the class structure for a type deriving from `GObject`
 *
 * Installs the necessary properties for a class implementing
 * `GtkFileChooser`.
 *
 * A `GtkParamSpecOverride` property is installed for each property,
 * using the values from the `GtkFileChooserProp` enumeration. The
 * caller must make sure itself that the enumeration values don’t
 * collide with some other property values they are using.
 */
void
_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_ACTION,
                                    "action");
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_FILTER,
                                    "filter");
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
                                    "select-multiple");
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS,
                                    "create-folders");
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_FILTERS,
                                    "filters");
  g_object_class_override_property (klass,
                                    GTK_FILE_CHOOSER_PROP_SHORTCUT_FOLDERS,
                                    "shortcut-folders");
}

/**
 * _gtk_file_chooser_delegate_iface_init:
 * @iface: a `GtkFileChoserIface` structure
 *
 * An interface-initialization function for use in cases where
 * an object is simply delegating the methods, signals of
 * the `GtkFileChooser` interface to another object.
 *
 * _gtk_file_chooser_set_delegate() must be called on each
 * instance of the object so that the delegate object can
 * be found.
 **/
void
_gtk_file_chooser_delegate_iface_init (GtkFileChooserIface *iface)
{
  iface->set_current_folder = delegate_set_current_folder;
  iface->get_current_folder = delegate_get_current_folder;
  iface->set_current_name = delegate_set_current_name;
  iface->get_current_name = delegate_get_current_name;
  iface->select_file = delegate_select_file;
  iface->unselect_file = delegate_unselect_file;
  iface->select_all = delegate_select_all;
  iface->unselect_all = delegate_unselect_all;
  iface->get_files = delegate_get_files;
  iface->add_filter = delegate_add_filter;
  iface->remove_filter = delegate_remove_filter;
  iface->get_filters = delegate_get_filters;
  iface->add_shortcut_folder = delegate_add_shortcut_folder;
  iface->remove_shortcut_folder = delegate_remove_shortcut_folder;
  iface->get_shortcut_folders = delegate_get_shortcut_folders;
  iface->add_choice = delegate_add_choice;
  iface->remove_choice = delegate_remove_choice;
  iface->set_choice = delegate_set_choice;
  iface->get_choice = delegate_get_choice;
}

/**
 * _gtk_file_chooser_set_delegate:
 * @receiver: a `GObject` implementing `GtkFileChooser`
 * @delegate: another `GObject` implementing `GtkFileChooser`
 *
 * Establishes that calls on @receiver for `GtkFileChooser`
 * methods should be delegated to @delegate, and that
 * `GtkFileChooser` signals emitted on @delegate should be
 * forwarded to @receiver. Must be used in conjunction with
 * _gtk_file_chooser_delegate_iface_init().
 **/
void
_gtk_file_chooser_set_delegate (GtkFileChooser *receiver,
				GtkFileChooser *delegate)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (receiver));
  g_return_if_fail (GTK_IS_FILE_CHOOSER (delegate));

  g_object_set_data (G_OBJECT (receiver), I_("gtk-file-chooser-delegate"), delegate);
  g_signal_connect (delegate, "notify",
		    G_CALLBACK (delegate_notify), receiver);
}

GQuark
_gtk_file_chooser_delegate_get_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gtk-file-chooser-delegate");
  
  return quark;
}

static GtkFileChooser *
get_delegate (GtkFileChooser *receiver)
{
  return g_object_get_qdata (G_OBJECT (receiver),
			     GTK_FILE_CHOOSER_DELEGATE_QUARK);
}

static gboolean
delegate_select_file (GtkFileChooser    *chooser,
		      GFile             *file,
		      GError           **error)
{
  return gtk_file_chooser_select_file (get_delegate (chooser), file, error);
}

static void
delegate_unselect_file (GtkFileChooser *chooser,
			GFile          *file)
{
  gtk_file_chooser_unselect_file (get_delegate (chooser), file);
}

static void
delegate_select_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_select_all (get_delegate (chooser));
}

static void
delegate_unselect_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_unselect_all (get_delegate (chooser));
}

static GListModel *
delegate_get_files (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_files (get_delegate (chooser));
}

static void
delegate_add_filter (GtkFileChooser *chooser,
		     GtkFileFilter  *filter)
{
  gtk_file_chooser_add_filter (get_delegate (chooser), filter);
}

static void
delegate_remove_filter (GtkFileChooser *chooser,
			GtkFileFilter  *filter)
{
  gtk_file_chooser_remove_filter (get_delegate (chooser), filter);
}

static GListModel *
delegate_get_filters (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_filters (get_delegate (chooser));
}

static gboolean
delegate_add_shortcut_folder (GtkFileChooser  *chooser,
			      GFile           *file,
			      GError         **error)
{
  return gtk_file_chooser_add_shortcut_folder (get_delegate (chooser), file, error);
}

static gboolean
delegate_remove_shortcut_folder (GtkFileChooser  *chooser,
				 GFile           *file,
				 GError         **error)
{
  return gtk_file_chooser_remove_shortcut_folder (get_delegate (chooser), file, error);
}

static GListModel *
delegate_get_shortcut_folders (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_shortcut_folders (get_delegate (chooser));
}

static gboolean
delegate_set_current_folder (GtkFileChooser  *chooser,
			     GFile           *file,
			     GError         **error)
{
  return gtk_file_chooser_set_current_folder (get_delegate (chooser), file, error);
}

static GFile *
delegate_get_current_folder (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_current_folder (get_delegate (chooser));
}

static void
delegate_set_current_name (GtkFileChooser *chooser,
			   const char     *name)
{
  gtk_file_chooser_set_current_name (get_delegate (chooser), name);
}

static char *
delegate_get_current_name (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_current_name (get_delegate (chooser));
}

static void
delegate_notify (GObject    *object,
		 GParamSpec *pspec,
		 gpointer    data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)),
				 gtk_file_chooser_get_type ());
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (data, pspec->name);
}

GSettings *
_gtk_file_chooser_get_settings_for_widget (GtkWidget *widget)
{
  static GQuark file_chooser_settings_quark = 0;
  GtkSettings *gtksettings;
  GSettings *settings;

  if (G_UNLIKELY (file_chooser_settings_quark == 0))
    file_chooser_settings_quark = g_quark_from_static_string ("-gtk-file-chooser-settings");

  gtksettings = gtk_widget_get_settings (widget);
  settings = g_object_get_qdata (G_OBJECT (gtksettings), file_chooser_settings_quark);

  if (G_UNLIKELY (settings == NULL))
    {
      settings = g_settings_new ("org.gtk.gtk4.Settings.FileChooser");
      g_settings_delay (settings);

      g_object_set_qdata_full (G_OBJECT (gtksettings),
                               file_chooser_settings_quark,
                               settings,
                               g_object_unref);
    }

  return settings;
}

char *
_gtk_file_chooser_label_for_file (GFile *file)
{
  const char *path, *start, *end, *p;
  char *uri, *host, *label;

  uri = g_file_get_uri (file);

  start = strstr (uri, "://");
  if (start)
    {
      start += 3;
      path = strchr (start, '/');
      if (path)
        end = path;
      else
        {
          end = uri + strlen (uri);
          path = "/";
        }

      /* strip username */
      p = strchr (start, '@');
      if (p && p < end)
        start = p + 1;

      p = strchr (start, ':');
      if (p && p < end)
        end = p;

      host = g_strndup (start, end - start);
      /* Translators: the first string is a path and the second string 
       * is a hostname. Nautilus and the panel contain the same string 
       * to translate. 
       */
      label = g_strdup_printf (_("%1$s on %2$s"), path, host);

      g_free (host);
    }
  else
    {
      label = g_strdup (uri);
    }

  g_free (uri);

  return label;
}

static void
delegate_add_choice (GtkFileChooser *chooser,
                     const char      *id,
                     const char      *label,
                     const char     **options,
                     const char     **option_labels)
{
  gtk_file_chooser_add_choice (get_delegate (chooser),
                               id, label, options, option_labels);
}
static void
delegate_remove_choice (GtkFileChooser  *chooser,
                        const char      *id)
{
  gtk_file_chooser_remove_choice (get_delegate (chooser), id);
}

static void
delegate_set_choice (GtkFileChooser  *chooser,
                     const char      *id,
                     const char      *option)
{
  gtk_file_chooser_set_choice (get_delegate (chooser), id, option);
}


static const char *
delegate_get_choice (GtkFileChooser  *chooser,
                     const char      *id)
{
  return gtk_file_chooser_get_choice (get_delegate (chooser), id);
}

gboolean
_gtk_file_info_consider_as_directory (GFileInfo *info)
{
  GFileType type = g_file_info_get_file_type (info);

  return (type == G_FILE_TYPE_DIRECTORY ||
          type == G_FILE_TYPE_MOUNTABLE ||
          type == G_FILE_TYPE_SHORTCUT);
}

gboolean
_gtk_file_has_native_path (GFile *file)
{
  char *local_file_path;
  gboolean has_native_path;

  /* Don't use g_file_is_native(), as we want to support FUSE paths if available */
  local_file_path = g_file_get_path (file);
  has_native_path = (local_file_path != NULL);
  g_free (local_file_path);

  return has_native_path;
}

gboolean
_gtk_file_consider_as_remote (GFile *file)
{
  GFileInfo *info;
  gboolean is_remote;

  info = g_file_query_filesystem_info (file, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE, NULL, NULL);
  if (info)
    {
      is_remote = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);

      g_object_unref (info);
    }
  else
    is_remote = FALSE;

  return is_remote;
}

GIcon *
_gtk_file_info_get_icon (GFileInfo    *info,
                         int           icon_size,
                         int           scale,
                         GtkIconTheme *icon_theme)
{
  GIcon *icon;
  GdkPixbuf *pixbuf;
  const char *thumbnail_path;

  thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

  if (thumbnail_path)
    {
      pixbuf = gdk_pixbuf_new_from_file_at_size (thumbnail_path,
                                                 icon_size*scale, icon_size*scale,
                                                 NULL);

      if (pixbuf != NULL)
        return G_ICON (pixbuf);
    }

  icon = g_file_info_get_icon (info);
  if (icon && gtk_icon_theme_has_gicon (icon_theme, icon))
    return g_object_ref (icon);

  /* Use general fallback for all files without icon */
  icon = g_themed_icon_new ("text-x-generic");
  return icon;
}

GFile *
_gtk_file_info_get_file (GFileInfo *info)
{
  g_assert (G_IS_FILE_INFO (info));
  g_assert (g_file_info_has_attribute (info, "standard::file"));

  return G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
}
