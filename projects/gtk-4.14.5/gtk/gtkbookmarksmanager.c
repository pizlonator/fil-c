/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkbookmarksmanager.c: Utilities to manage and monitor ~/.gtk-bookmarks
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2007-2008 Carlos Garnacho
 * Copyright (C) 2011 Suse
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Federico Mena Quintero <federico@gnome.org>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkbookmarksmanagerprivate.h"
#include "deprecated/gtkfilechooser.h" /* for the GError types */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
_gtk_bookmark_free (gpointer data)
{
  GtkBookmark *bookmark = data;

  g_object_unref (bookmark->file);
  g_free (bookmark->label);
  g_free (bookmark);
}

static void
set_error_bookmark_doesnt_exist (GFile *file, GError **error)
{
  char *uri = g_file_get_uri (file);

  g_set_error (error,
               GTK_FILE_CHOOSER_ERROR,
               GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
               _("%s does not exist in the bookmarks list"),
               uri);

  g_free (uri);
}

static GFile *
get_legacy_bookmarks_file (void)
{
  GFile *file;
  char *filename;

  filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GFile *
get_bookmarks_file (void)
{
  GFile *file;
  char *filename;

  /* Use gtk-3.0's bookmarks file as the format didn't change.
   * Add the 3.0 file format to get_legacy_bookmarks_file() when
   * the format does change.
   */
  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GSList *
parse_bookmarks (const char *contents)
{
  char **lines, *space;
  GSList *bookmarks = NULL;
  int i;

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i]; i++)
    {
      GtkBookmark *bookmark;

      if (!*lines[i])
	continue;

      if (!g_utf8_validate (lines[i], -1, NULL))
	continue;

      bookmark = g_new0 (GtkBookmark, 1);

      if ((space = strchr (lines[i], ' ')) != NULL)
	{
	  space[0] = '\0';
	  bookmark->label = g_strdup (space + 1);
	}

      bookmark->file = g_file_new_for_uri (lines[i]);
      bookmarks = g_slist_prepend (bookmarks, bookmark);
    }

  bookmarks = g_slist_reverse (bookmarks);
  g_strfreev (lines);

  return bookmarks;
}

static GSList *
read_bookmarks (GFile *file)
{
  char *contents;
  GSList *bookmarks = NULL;

  if (!g_file_load_contents (file, NULL, &contents,
			     NULL, NULL, NULL))
    return NULL;

  bookmarks = parse_bookmarks (contents);

  g_free (contents);

  return bookmarks;
}

static void
notify_changed (GtkBookmarksManager *manager)
{
  if (manager->changed_func)
    manager->changed_func (manager->changed_func_data);
}

static void
read_bookmarks_finish (GObject      *source,
                       GAsyncResult *result,
                       gpointer      data)
{
  GFile *file = G_FILE (source);
  GtkBookmarksManager *manager = data;
  char *contents = NULL;
  GError *error = NULL;

  if (!g_file_load_contents_finish (file, result, &contents, NULL, NULL, &error)) 
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to load '%s': %s", g_file_peek_path (file), error->message);
      g_error_free (error);
      return;
    }

  g_slist_free_full (manager->bookmarks, _gtk_bookmark_free);
  manager->bookmarks = parse_bookmarks (contents);

  g_free (contents);

  notify_changed (manager);
}

static void
save_bookmarks (GFile  *bookmarks_file,
		GSList *bookmarks)
{
  GError *error = NULL;
  GString *contents;
  GSList *l;
  GFile *parent = NULL;

  contents = g_string_new ("");

  for (l = bookmarks; l; l = l->next)
    {
      GtkBookmark *bookmark = l->data;
      char *uri;

      uri = g_file_get_uri (bookmark->file);
      if (!uri)
	continue;

      g_string_append (contents, uri);

      if (bookmark->label && g_utf8_validate (bookmark->label, -1, NULL))
	g_string_append_printf (contents, " %s", bookmark->label);

      g_string_append_c (contents, '\n');
      g_free (uri);
    }

  parent = g_file_get_parent (bookmarks_file);
  if (!g_file_make_directory_with_parents (parent, NULL, &error))
    {
       if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
	 g_clear_error (&error);
       else
	 goto out;
    }
  if (!g_file_replace_contents (bookmarks_file,
				contents->str,
				contents->len,
				NULL, FALSE, 0, NULL,
				NULL, &error))
    goto out;

 out:
  if (error)
    {
      g_critical ("%s", error->message);
      g_error_free (error);
    }
  g_clear_object (&parent);
  g_string_free (contents, TRUE);
}

static void
bookmarks_file_changed (GFileMonitor      *monitor,
			GFile             *file,
			GFile             *other_file,
			GFileMonitorEvent  event,
			gpointer           data)
{
  GtkBookmarksManager *manager = data;

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
      g_file_load_contents_async (file, manager->cancellable, read_bookmarks_finish, manager);
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
    default:
      /* ignore at the moment */
      break;
    }
}

GtkBookmarksManager *
_gtk_bookmarks_manager_new (GtkBookmarksChangedFunc changed_func, gpointer changed_func_data)
{
  GtkBookmarksManager *manager;
  GFile *bookmarks_file;
  GError *error;

  manager = g_new0 (GtkBookmarksManager, 1);

  manager->changed_func = changed_func;
  manager->changed_func_data = changed_func_data;

  manager->cancellable = g_cancellable_new ();

  bookmarks_file = get_bookmarks_file ();
  if (!g_file_query_exists (bookmarks_file, NULL))
    {
      GFile *legacy_bookmarks_file;

      /* Read the legacy one and write it to the new one */
      legacy_bookmarks_file = get_legacy_bookmarks_file ();
      manager->bookmarks = read_bookmarks (legacy_bookmarks_file);
      if (manager->bookmarks)
	save_bookmarks (bookmarks_file, manager->bookmarks);

      g_object_unref (legacy_bookmarks_file);
    }
  else
    g_file_load_contents_async (bookmarks_file, manager->cancellable, read_bookmarks_finish, manager);

  error = NULL;
  manager->bookmarks_monitor = g_file_monitor_file (bookmarks_file,
						    G_FILE_MONITOR_NONE,
						    NULL, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    manager->bookmarks_monitor_changed_id = g_signal_connect (manager->bookmarks_monitor, "changed",
							      G_CALLBACK (bookmarks_file_changed), manager);


  g_object_unref (bookmarks_file);

  return manager;
}

void
_gtk_bookmarks_manager_free (GtkBookmarksManager *manager)
{
  g_return_if_fail (manager != NULL);

  g_cancellable_cancel (manager->cancellable);
  g_object_unref (manager->cancellable);

  if (manager->bookmarks_monitor)
    {
      g_file_monitor_cancel (manager->bookmarks_monitor);
      g_signal_handler_disconnect (manager->bookmarks_monitor, manager->bookmarks_monitor_changed_id);
      manager->bookmarks_monitor_changed_id = 0;
      g_object_unref (manager->bookmarks_monitor);
    }

  g_slist_free_full (manager->bookmarks, _gtk_bookmark_free);

  g_free (manager);
}

GSList *
_gtk_bookmarks_manager_list_bookmarks (GtkBookmarksManager *manager)
{
  GSList *bookmarks, *files = NULL;

  g_return_val_if_fail (manager != NULL, NULL);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      GtkBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      files = g_slist_prepend (files, g_object_ref (bookmark->file));
    }

  return g_slist_reverse (files);
}

static GSList *
find_bookmark_link_for_file (GSList *bookmarks, GFile *file, int *position_ret)
{
  int pos;

  pos = 0;
  for (; bookmarks; bookmarks = bookmarks->next)
    {
      GtkBookmark *bookmark = bookmarks->data;

      if (g_file_equal (file, bookmark->file))
	{
	  if (position_ret)
	    *position_ret = pos;

	  return bookmarks;
	}

      pos++;
    }

  if (position_ret)
    *position_ret = -1;

  return NULL;
}

gboolean
_gtk_bookmarks_manager_has_bookmark (GtkBookmarksManager *manager,
                                     GFile               *file)
{
  GSList *link;

  link = find_bookmark_link_for_file (manager->bookmarks, file, NULL);
  return (link != NULL);
}

gboolean
_gtk_bookmarks_manager_insert_bookmark (GtkBookmarksManager *manager,
					GFile               *file,
					int                  position,
					GError             **error)
{
  GSList *link;
  GtkBookmark *bookmark;
  GFile *bookmarks_file;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  link = find_bookmark_link_for_file (manager->bookmarks, file, NULL);

  if (link)
    {
      char *uri;
      bookmark = link->data;
      uri = g_file_get_uri (bookmark->file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		   _("%s already exists in the bookmarks list"),
		   uri);

      g_free (uri);

      return FALSE;
    }

  bookmark = g_new0 (GtkBookmark, 1);
  bookmark->file = g_object_ref (file);

  manager->bookmarks = g_slist_insert (manager->bookmarks, bookmark, position);

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

gboolean
_gtk_bookmarks_manager_remove_bookmark (GtkBookmarksManager *manager,
					GFile               *file,
					GError             **error)
{
  GSList *link;
  GFile *bookmarks_file;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!manager->bookmarks)
    return FALSE;

  link = find_bookmark_link_for_file (manager->bookmarks, file, NULL);
  if (link)
    {
      GtkBookmark *bookmark = link->data;

      manager->bookmarks = g_slist_remove_link (manager->bookmarks, link);
      _gtk_bookmark_free (bookmark);
      g_slist_free_1 (link);
    }
  else
    {
      set_error_bookmark_doesnt_exist (file, error);
      return FALSE;
    }

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

gboolean
_gtk_bookmarks_manager_reorder_bookmark (GtkBookmarksManager *manager,
					 GFile               *file,
					 int                  new_position,
					 GError             **error)
{
  GSList *link;
  GFile *bookmarks_file;
  int old_position;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (new_position >= 0, FALSE);

  if (!manager->bookmarks)
    return FALSE;

  link = find_bookmark_link_for_file (manager->bookmarks, file, &old_position);
  if (new_position == old_position)
    return TRUE;

  if (link)
    {
      GtkBookmark *bookmark = link->data; 

      manager->bookmarks = g_slist_remove_link (manager->bookmarks, link);
      g_slist_free_1 (link);

      if (new_position > old_position)
	new_position--;

      manager->bookmarks = g_slist_insert (manager->bookmarks, bookmark, new_position);
    }
  else
    {
      set_error_bookmark_doesnt_exist (file, error);
      return FALSE;
    }

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

char *
_gtk_bookmarks_manager_get_bookmark_label (GtkBookmarksManager *manager,
					   GFile               *file)
{
  GSList *bookmarks;
  char *label = NULL;

  g_return_val_if_fail (manager != NULL, NULL);
  g_return_val_if_fail (file != NULL, NULL);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      GtkBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
	  label = g_strdup (bookmark->label);
	  break;
	}
    }

  return label;
}

gboolean
_gtk_bookmarks_manager_set_bookmark_label (GtkBookmarksManager *manager,
					   GFile               *file,
					   const char          *label,
					   GError             **error)
{
  GFile *bookmarks_file;
  GSList *link;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (file != NULL, FALSE);

  link = find_bookmark_link_for_file (manager->bookmarks, file, NULL);
  if (link)
    {
      GtkBookmark *bookmark = link->data;

      g_free (bookmark->label);
      bookmark->label = g_strdup (label);
    }
  else
    {
      set_error_bookmark_doesnt_exist (file, error);
      return FALSE;
    }

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

static gboolean
_gtk_bookmarks_manager_get_xdg_type (GtkBookmarksManager *manager,
                                     GFile               *file,
                                     GUserDirectory      *directory)
{
  GSList *link;
  gboolean match;
  GFile *location;
  const char *path;
  GUserDirectory dir;
  GtkBookmark *bookmark;

  link = find_bookmark_link_for_file (manager->bookmarks, file, NULL);
  if (!link)
    return FALSE;

  match = FALSE;
  bookmark = link->data;

  for (dir = 0; dir < G_USER_N_DIRECTORIES; dir++)
    {
      path = g_get_user_special_dir (dir);
      if (!path)
        continue;

      location = g_file_new_for_path (path);
      match = g_file_equal (location, bookmark->file);
      g_object_unref (location);

      if (match)
        break;
    }

  if (match && directory != NULL)
    *directory = dir;

  return match;
}

gboolean
_gtk_bookmarks_manager_get_is_builtin (GtkBookmarksManager *manager,
                                       GFile               *file)
{
  GUserDirectory xdg_type;

  /* if this is not an XDG dir, it's never builtin */
  if (!_gtk_bookmarks_manager_get_xdg_type (manager, file, &xdg_type))
    return FALSE;

  /* exclude XDG locations we don't display by default */
  return _gtk_bookmarks_manager_get_is_xdg_dir_builtin (xdg_type);
}

gboolean
_gtk_bookmarks_manager_get_is_xdg_dir_builtin (GUserDirectory xdg_type)
{
  return (xdg_type != G_USER_DIRECTORY_DESKTOP) &&
    (xdg_type != G_USER_DIRECTORY_TEMPLATES) &&
    (xdg_type != G_USER_DIRECTORY_PUBLIC_SHARE);
}
