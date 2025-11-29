/* GTK - The GIMP Toolkit
 * gtkfilechooser.c: Abstract interface for file selector GUIs
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
#include "gtkfilechooser.h"
#include "gtkfilechooserprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkFileChooser:
 *
 * `GtkFileChooser` is an interface that can be implemented by file
 * selection widgets.
 *
 * In GTK, the main objects that implement this interface are
 * [class@Gtk.FileChooserWidget] and [class@Gtk.FileChooserDialog].
 *
 * You do not need to write an object that implements the `GtkFileChooser`
 * interface unless you are trying to adapt an existing file selector to
 * expose a standard programming interface.
 *
 * `GtkFileChooser` allows for shortcuts to various places in the filesystem.
 * In the default implementation these are displayed in the left pane. It
 * may be a bit confusing at first that these shortcuts come from various
 * sources and in various flavours, so lets explain the terminology here:
 *
 * - Bookmarks: are created by the user, by dragging folders from the
 *   right pane to the left pane, or by using the “Add”. Bookmarks
 *   can be renamed and deleted by the user.
 *
 * - Shortcuts: can be provided by the application. For example, a Paint
 *   program may want to add a shortcut for a Clipart folder. Shortcuts
 *   cannot be modified by the user.
 *
 * - Volumes: are provided by the underlying filesystem abstraction. They are
 *   the “roots” of the filesystem.
 *
 * # File Names and Encodings
 *
 * When the user is finished selecting files in a `GtkFileChooser`, your
 * program can get the selected filenames as `GFile`s.
 *
 * # Adding options
 *
 * You can add extra widgets to a file chooser to provide options
 * that are not present in the default design, by using
 * [method@Gtk.FileChooser.add_choice]. Each choice has an identifier and
 * a user visible label; additionally, each choice can have multiple
 * options. If a choice has no option, it will be rendered as a
 * check button with the given label; if a choice has options, it will
 * be rendered as a combo box.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */


typedef GtkFileChooserIface GtkFileChooserInterface;
G_DEFINE_INTERFACE (GtkFileChooser, gtk_file_chooser, G_TYPE_OBJECT);

static void
gtk_file_chooser_default_init (GtkFileChooserInterface *iface)
{
  /**
   * GtkFileChooser:action: (attributes org.gtk.Property.get=gtk_file_chooser_get_action org.gtk.Property.set=gtk_file_chooser_set_action)
   *
   * The type of operation that the file chooser is performing.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("action", NULL, NULL,
                                                          GTK_TYPE_FILE_CHOOSER_ACTION,
                                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                                          GTK_PARAM_READWRITE));


  /**
   * GtkFileChooser:filter: (attributes org.gtk.Property.get=gtk_file_chooser_get_filter org.gtk.Property.set=gtk_file_chooser_set_filter)
   *
   * The current filter for selecting files that are displayed.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("filter", NULL, NULL,
                                                            GTK_TYPE_FILE_FILTER,
                                                            GTK_PARAM_READWRITE));

  /**
   * GtkFileChooser:select-multiple: (attributes org.gtk.Property.get=gtk_file_chooser_get_select_multiple org.gtk.Property.set=gtk_file_chooser_set_select_multiple)
   *
   * Whether to allow multiple files to be selected.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("select-multiple", NULL, NULL,
                                                             FALSE,
                                                             GTK_PARAM_READWRITE));

  /**
   * GtkFileChooser:filters: (attributes org.gtk.Property.get=gtk_file_chooser_get_filters)
   *
   * A `GListModel` containing the filters that have been
   * added with gtk_file_chooser_add_filter().
   *
   * The returned object should not be modified. It may
   * or may not be updated for later changes.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("filters", NULL, NULL,
                                                          G_TYPE_LIST_MODEL,
                                                          GTK_PARAM_READABLE));

  /**
   * GtkFileChooser:shortcut-folders: (attributes org.gtk.Property.get=gtk_file_chooser_get_shortcut_folders)
   *
   * A `GListModel` containing the shortcut folders that have been
   * added with gtk_file_chooser_add_shortcut_folder().
   *
   * The returned object should not be modified. It may
   * or may not be updated for later changes.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("shortcut-folders", NULL, NULL,
                                                          G_TYPE_LIST_MODEL,
                                                          GTK_PARAM_READABLE));

  /**
   * GtkFileChooser:create-folders: (attributes org.gtk.Property.get=gtk_file_chooser_get_create_folders org.gtk.Property.set=gtk_file_chooser_set_create_folders)
   *
   * Whether a file chooser not in %GTK_FILE_CHOOSER_ACTION_OPEN mode
   * will offer the user to create new folders.
   *
   * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("create-folders", NULL, NULL,
                                                             TRUE,
                                                             GTK_PARAM_READWRITE));
}

/**
 * gtk_file_chooser_error_quark:
 *
 * Registers an error quark for `GtkFileChooser` errors.
 *
 * Returns: The error quark used for `GtkFileChooser` errors.
 **/
GQuark
gtk_file_chooser_error_quark (void)
{
  return g_quark_from_static_string ("gtk-file-chooser-error-quark");
}

/**
 * gtk_file_chooser_set_action: (attributes org.gtk.Method.set_property=action)
 * @chooser: a `GtkFileChooser`
 * @action: the action that the file selector is performing
 *
 * Sets the type of operation that the chooser is performing.
 *
 * The user interface is adapted to suit the selected action.
 *
 * For example, an option to create a new folder might be shown
 * if the action is %GTK_FILE_CHOOSER_ACTION_SAVE but not if the
 * action is %GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 **/
void
gtk_file_chooser_set_action (GtkFileChooser       *chooser,
                             GtkFileChooserAction  action)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "action", action, NULL);
}

/**
 * gtk_file_chooser_get_action: (attributes org.gtk.Method.get_property=action)
 * @chooser: a `GtkFileChooser`
 *
 * Gets the type of operation that the file chooser is performing.
 *
 * Returns: the action that the file selector is performing
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GtkFileChooserAction
gtk_file_chooser_get_action (GtkFileChooser *chooser)
{
  GtkFileChooserAction action;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "action", &action, NULL);

  return action;
}

/**
 * gtk_file_chooser_set_select_multiple: (attributes org.gtk.Method.set_property=select-multiple)
 * @chooser: a `GtkFileChooser`
 * @select_multiple: %TRUE if multiple files can be selected.
 *
 * Sets whether multiple files can be selected in the file chooser.
 *
 * This is only relevant if the action is set to be
 * %GTK_FILE_CHOOSER_ACTION_OPEN or
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_set_select_multiple (GtkFileChooser *chooser,
                                      gboolean        select_multiple)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "select-multiple", select_multiple, NULL);
}

/**
 * gtk_file_chooser_get_select_multiple: (attributes org.gtk.Method.get_property=select-multiple)
 * @chooser: a `GtkFileChooser`
 *
 * Gets whether multiple files can be selected in the file
 * chooser.
 *
 * Returns: %TRUE if multiple files can be selected.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_get_select_multiple (GtkFileChooser *chooser)
{
  gboolean select_multiple;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "select-multiple", &select_multiple, NULL);

  return select_multiple;
}

/**
 * gtk_file_chooser_set_create_folders: (attributes org.gtk.Method.set_property=create-folders)
 * @chooser: a `GtkFileChooser`
 * @create_folders: %TRUE if the Create Folder button should be displayed
 *
 * Sets whether file chooser will offer to create new folders.
 *
 * This is only relevant if the action is not set to be
 * %GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_set_create_folders (GtkFileChooser *chooser,
                                     gboolean        create_folders)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "create-folders", create_folders, NULL);
}

/**
 * gtk_file_chooser_get_create_folders: (attributes org.gtk.Method.get_property=create-folders)
 * @chooser: a `GtkFileChooser`
 *
 * Gets whether file chooser will offer to create new folders.
 *
 * Returns: %TRUE if the Create Folder button should be displayed.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_get_create_folders (GtkFileChooser *chooser)
{
  gboolean create_folders;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "create-folders", &create_folders, NULL);

  return create_folders;
}

/**
 * gtk_file_chooser_set_current_name:
 * @chooser: a `GtkFileChooser`
 * @name: (type utf8): the filename to use, as a UTF-8 string
 *
 * Sets the current name in the file selector, as if entered
 * by the user.
 *
 * Note that the name passed in here is a UTF-8 string rather
 * than a filename. This function is meant for such uses as a
 * suggested name in a “Save As...” dialog.  You can pass
 * “Untitled.doc” or a similarly suitable suggestion for the @name.
 *
 * If you want to preselect a particular existing file, you should
 * use [method@Gtk.FileChooser.set_file] instead.
 *
 * Please see the documentation for those functions for an example
 * of using [method@Gtk.FileChooser.set_current_name] as well.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 **/
void
gtk_file_chooser_set_current_name  (GtkFileChooser *chooser,
                                    const char     *name)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (name != NULL);

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_name (chooser, name);
}

/**
 * gtk_file_chooser_get_current_name:
 * @chooser: a `GtkFileChooser`
 *
 * Gets the current name in the file selector, as entered by the user.
 *
 * This is meant to be used in save dialogs, to get the currently typed
 * filename when the file itself does not exist yet.
 *
 * Returns: (nullable): The raw text from the file chooser’s “Name” entry. Free with
 *   g_free(). Note that this string is not a full pathname or URI; it is
 *   whatever the contents of the entry are. Note also that this string is
 *   in UTF-8 encoding, which is not necessarily the system’s encoding for
 *   filenames.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
char *
gtk_file_chooser_get_current_name (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_name (chooser);
}

void
gtk_file_chooser_select_all (GtkFileChooser *chooser)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_file_chooser_set_current_folder:
 * @chooser: a `GtkFileChooser`
 * @file: (nullable): the `GFile` for the new folder
 * @error: location to store error
 *
 * Sets the current folder for @chooser from a `GFile`.
 *
 * Returns: %TRUE if the folder could be changed successfully, %FALSE
 *   otherwise.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser  *chooser,
                                     GFile           *file,
                                     GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, file, error);
}

/**
 * gtk_file_chooser_get_current_folder:
 * @chooser: a `GtkFileChooser`
 *
 * Gets the current folder of @chooser as `GFile`.
 *
 * Returns: (transfer full) (nullable): the `GFile` for the current folder.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GFile *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_folder (chooser);
}

gboolean
gtk_file_chooser_select_file (GtkFileChooser  *chooser,
                              GFile           *file,
                              GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_file (chooser, file, error);
}

void
gtk_file_chooser_unselect_file (GtkFileChooser *chooser,
                                GFile          *file)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (G_IS_FILE (file));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_file (chooser, file);
}

/**
 * gtk_file_chooser_get_files:
 * @chooser: a `GtkFileChooser`
 *
 * Lists all the selected files and subfolders in the current folder
 * of @chooser as `GFile`.
 *
 * Returns: (transfer full): a list model containing a `GFile` for each
 *   selected file and subfolder in the current folder. Free the returned
 *   list with g_object_unref().
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GListModel *
gtk_file_chooser_get_files (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_files (chooser);
}

/**
 * gtk_file_chooser_set_file:
 * @chooser: a `GtkFileChooser`
 * @file: the `GFile` to set as current
 * @error: (nullable): location to store the error
 *
 * Sets @file as the current filename for the file chooser.
 *
 * This includes changing to the file’s parent folder and actually selecting
 * the file in list. If the @chooser is in %GTK_FILE_CHOOSER_ACTION_SAVE mode,
 * the file’s base name will also appear in the dialog’s file name entry.
 *
 * If the file name isn’t in the current folder of @chooser, then the current
 * folder of @chooser will be changed to the folder containing @file.
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change.
 *
 * If you are implementing a save dialog, you should use this function if
 * you already have a file name to which the user may save; for example,
 * when the user opens an existing file and then does “Save As…”. If you
 * don’t have a file name already — for example, if the user just created
 * a new file and is saving it for the first time, do not call this function.
 *
 * Instead, use something similar to this:
 *
 * ```c
 * static void
 * prepare_file_chooser (GtkFileChooser *chooser,
 *                       GFile          *existing_file)
 * {
 *   gboolean document_is_new = (existing_file == NULL);
 *
 *   if (document_is_new)
 *     {
 *       GFile *default_file_for_saving = g_file_new_for_path ("./out.txt");
 *       // the user just created a new document
 *       gtk_file_chooser_set_current_folder (chooser, default_file_for_saving, NULL);
 *       gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *       g_object_unref (default_file_for_saving);
 *     }
 *   else
 *     {
 *       // the user edited an existing document
 *       gtk_file_chooser_set_file (chooser, existing_file, NULL);
 *     }
 * }
 * ```
 *
 * Returns: Not useful
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_set_file (GtkFileChooser  *chooser,
                           GFile           *file,
                           GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  gtk_file_chooser_unselect_all (chooser);
  return gtk_file_chooser_select_file (chooser, file, error);
}

/**
 * gtk_file_chooser_get_file:
 * @chooser: a `GtkFileChooser`
 *
 * Gets the `GFile` for the currently selected file in
 * the file selector.
 *
 * If multiple files are selected, one of the files will be
 * returned at random.
 *
 * If the file chooser is in folder mode, this function returns
 * the selected folder.
 *
 * Returns: (transfer full) (nullable): a selected `GFile`. You own the
 *   returned file; use g_object_unref() to release it.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GFile *
gtk_file_chooser_get_file (GtkFileChooser *chooser)
{
  GListModel *list;
  GFile *result = NULL;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  list = gtk_file_chooser_get_files (chooser);
  if (g_list_model_get_n_items (list) > 0)
    result = g_list_model_get_item (list, 0);
  g_object_unref (list);

  return result;
}

/**
 * gtk_file_chooser_add_shortcut_folder:
 * @chooser: a `GtkFileChooser`
 * @folder: a `GFile` for the folder to add
 * @error: location to store error
 *
 * Adds a folder to be displayed with the shortcut folders
 * in a file chooser.
 *
 * Returns: %TRUE if the folder could be added successfully,
 *   %FALSE otherwise.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser  *chooser,
                                      GFile           *folder,
                                      GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (folder), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, folder, error);
}

/**
 * gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a `GtkFileChooser`
 * @folder: a `GFile` for the folder to remove
 * @error: location to store error
 *
 * Removes a folder from the shortcut folders in a file chooser.
 *
 * Returns: %TRUE if the folder could be removed successfully,
 *   %FALSE otherwise.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser  *chooser,
                                         GFile           *folder,
                                         GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (folder), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, folder, error);
}

/**
 * gtk_file_chooser_add_filter:
 * @chooser: a `GtkFileChooser`
 * @filter: (transfer none): a `GtkFileFilter`
 *
 * Adds @filter to the list of filters that the user can select between.
 *
 * When a filter is selected, only files that are passed by that
 * filter are displayed.
 *
 * Note that the @chooser takes ownership of the filter if it is floating,
 * so you have to ref and sink it if you want to keep a reference.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_add_filter (GtkFileChooser *chooser,
                             GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_filter (chooser, filter);
}

/**
 * gtk_file_chooser_remove_filter:
 * @chooser: a `GtkFileChooser`
 * @filter: a `GtkFileFilter`
 *
 * Removes @filter from the list of filters that the user can select between.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_remove_filter (GtkFileChooser *chooser,
                                GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_filter (chooser, filter);
}

/**
 * gtk_file_chooser_get_filters: (attributes org.gtk.Method.get_property=filters)
 * @chooser: a `GtkFileChooser`
 *
 * Gets the current set of user-selectable filters, as a list model.
 *
 * See [method@Gtk.FileChooser.add_filter] and
 * [method@Gtk.FileChooser.remove_filter] for changing individual filters.
 *
 * You should not modify the returned list model. Future changes to
 * @chooser may or may not affect the returned model.
 *
 * Returns: (transfer full): a `GListModel` containing the current set
 *   of user-selectable filters.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GListModel *
gtk_file_chooser_get_filters (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_filters (chooser);
}

/**
 * gtk_file_chooser_set_filter: (attributes org.gtk.Method.set_property=filter)
 * @chooser: a `GtkFileChooser`
 * @filter: a `GtkFileFilter`
 *
 * Sets the current filter.
 *
 * Only the files that pass the filter will be displayed.
 * If the user-selectable list of filters is non-empty, then
 * the filter should be one of the filters in that list.
 *
 * Setting the current filter when the list of filters is
 * empty is useful if you want to restrict the displayed
 * set of files without letting the user change it.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_set_filter (GtkFileChooser *chooser,
                             GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  g_object_set (chooser, "filter", filter, NULL);
}

/**
 * gtk_file_chooser_get_filter: (attributes org.gtk.Method.get_property=filter)
 * @chooser: a `GtkFileChooser`
 *
 * Gets the current filter.
 *
 * Returns: (nullable) (transfer none): the current filter
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GtkFileFilter *
gtk_file_chooser_get_filter (GtkFileChooser *chooser)
{
  GtkFileFilter *filter;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  g_object_get (chooser, "filter", &filter, NULL);
  /* Horrid hack; g_object_get() refs returned objects but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (filter)
    g_object_unref (filter);

  return filter;
}

/**
 * gtk_file_chooser_get_shortcut_folders: (attributes org.gtk.Method.get_property=shortcut-folders)
 * @chooser: a `GtkFileChooser`
 *
 * Queries the list of shortcut folders in the file chooser.
 *
 * You should not modify the returned list model. Future changes to
 * @chooser may or may not affect the returned model.
 *
 * Returns: (transfer full): A list model of `GFile`s
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GListModel *
gtk_file_chooser_get_shortcut_folders (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_shortcut_folders (chooser);
}

/**
 * gtk_file_chooser_add_choice:
 * @chooser: a `GtkFileChooser`
 * @id: id for the added choice
 * @label: user-visible label for the added choice
 * @options: (nullable) (array zero-terminated=1): ids for the options of the choice, or %NULL for a boolean choice
 * @option_labels: (nullable) (array zero-terminated=1): user-visible labels for the options, must be the same length as @options
 *
 * Adds a 'choice' to the file chooser.
 *
 * This is typically implemented as a combobox or, for boolean choices,
 * as a checkbutton. You can select a value using
 * [method@Gtk.FileChooser.set_choice] before the dialog is shown,
 * and you can obtain the user-selected value in the
 * [signal@Gtk.Dialog::response] signal handler using
 * [method@Gtk.FileChooser.get_choice].
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_add_choice (GtkFileChooser  *chooser,
                             const char      *id,
                             const char      *label,
                             const char     **options,
                             const char     **option_labels)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->add_choice)
    iface->add_choice (chooser, id, label, options, option_labels);
}

/**
 * gtk_file_chooser_remove_choice:
 * @chooser: a `GtkFileChooser`
 * @id: the ID of the choice to remove
 *
 * Removes a 'choice' that has been added with gtk_file_chooser_add_choice().
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_remove_choice (GtkFileChooser  *chooser,
                                const char      *id)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->remove_choice)
    iface->remove_choice (chooser, id);
}

/**
 * gtk_file_chooser_set_choice:
 * @chooser: a `GtkFileChooser`
 * @id: the ID of the choice to set
 * @option: the ID of the option to select
 *
 * Selects an option in a 'choice' that has been added with
 * gtk_file_chooser_add_choice().
 *
 * For a boolean choice, the possible options are "true" and "false".
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
void
gtk_file_chooser_set_choice (GtkFileChooser  *chooser,
                             const char      *id,
                             const char      *option)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->set_choice)
    iface->set_choice (chooser, id, option);
}

/**
 * gtk_file_chooser_get_choice:
 * @chooser: a `GtkFileChooser`
 * @id: the ID of the choice to get
 *
 * Gets the currently selected option in the 'choice' with the given ID.
 *
 * Returns: (nullable): the ID of the currently selected option
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
const char *
gtk_file_chooser_get_choice (GtkFileChooser  *chooser,
                             const char      *id)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->get_choice)
    return iface->get_choice (chooser, id);

  return NULL;
}
