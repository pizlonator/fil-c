/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfiledialog.h"

#include "deprecated/gtkdialog.h"
#include "gtkfilechoosernativeprivate.h"
#include "gtkdialogerror.h"
#include <glib/gi18n-lib.h>
#include "gdk/gdkprivate.h"
#include "gdk/gdkdebugprivate.h"

/**
 * GtkFileDialog:
 *
 * A `GtkFileDialog` object collects the arguments that
 * are needed to present a file chooser dialog to the
 * user, such as a title for the dialog and whether it
 * should be modal.
 *
 * The dialog is shown with [method@Gtk.FileDialog.open],
 * [method@Gtk.FileDialog.save], etc. These APIs follow the
 * GIO async pattern, and the result can be obtained by calling
 * the corresponding finish function, for example
 * [method@Gtk.FileDialog.open_finish].
 *
 * Since: 4.10
 */

/* {{{ GObject implementation */

struct _GtkFileDialog
{
  GObject parent_instance;

  char *title;
  char *accept_label;
  unsigned int modal : 1;

  GListModel *filters;
  GtkFileFilter *default_filter;
  GFile *initial_folder;
  char *initial_name;
  GFile *initial_file;
};

enum
{
  PROP_0,
  PROP_ACCEPT_LABEL,
  PROP_DEFAULT_FILTER,
  PROP_FILTERS,
  PROP_INITIAL_FILE,
  PROP_INITIAL_FOLDER,
  PROP_INITIAL_NAME,
  PROP_MODAL,
  PROP_TITLE,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkFileDialog, gtk_file_dialog, G_TYPE_OBJECT)

static void
gtk_file_dialog_init (GtkFileDialog *self)
{
  self->modal = TRUE;
}

static void
gtk_file_dialog_finalize (GObject *object)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  g_free (self->title);
  g_free (self->accept_label);
  g_clear_object (&self->filters);
  g_clear_object (&self->default_filter);
  g_clear_object (&self->initial_folder);
  g_free (self->initial_name);

  G_OBJECT_CLASS (gtk_file_dialog_parent_class)->finalize (object);
}

static void
gtk_file_dialog_get_property (GObject      *object,
                              unsigned int  property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_FILTERS:
      g_value_set_object (value, self->filters);
      break;

    case PROP_DEFAULT_FILTER:
      g_value_set_object (value, self->default_filter);
      break;

    case PROP_INITIAL_FILE:
      g_value_set_object (value, self->initial_file);
      break;

    case PROP_INITIAL_FOLDER:
      g_value_set_object (value, self->initial_folder);
      break;

    case PROP_INITIAL_NAME:
      g_value_set_string (value, self->initial_name);
      break;

    case PROP_ACCEPT_LABEL:
      g_value_set_string (value, self->accept_label);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_file_dialog_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_file_dialog_set_title (self, g_value_get_string (value));
      break;

    case PROP_MODAL:
      gtk_file_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_FILTERS:
      gtk_file_dialog_set_filters (self, g_value_get_object (value));
      break;

    case PROP_DEFAULT_FILTER:
      gtk_file_dialog_set_default_filter (self, g_value_get_object (value));
      break;

    case PROP_INITIAL_FILE:
      gtk_file_dialog_set_initial_file (self, g_value_get_object (value));
      break;

    case PROP_INITIAL_FOLDER:
      gtk_file_dialog_set_initial_folder (self, g_value_get_object (value));
      break;

    case PROP_INITIAL_NAME:
      gtk_file_dialog_set_initial_name (self, g_value_get_string (value));
      break;

    case PROP_ACCEPT_LABEL:
      gtk_file_dialog_set_accept_label (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_dialog_class_init (GtkFileDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_file_dialog_finalize;
  object_class->get_property = gtk_file_dialog_get_property;
  object_class->set_property = gtk_file_dialog_set_property;

  /**
   * GtkFileDialog:title: (attributes org.gtk.Property.get=gtk_file_dialog_get_title org.gtk.Property.set=gtk_file_dialog_set_title)
   *
   * A title that may be shown on the file chooser dialog.
   *
   * Since: 4.10
   */
  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:modal: (attributes org.gtk.Property.get=gtk_file_dialog_get_modal org.gtk.Property.set=gtk_file_dialog_set_modal)
   *
   * Whether the file chooser dialog is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:filters: (attributes org.gtk.Property.get=gtk_file_dialog_get_filters org.gtk.Property.set=gtk_file_dialog_set_filters)
   *
   * The list of filters.
   *
   * See [property@Gtk.FileDialog:default-filter] about how those two properties interact.
   *
   * Since: 4.10
   */
  properties[PROP_FILTERS] =
      g_param_spec_object ("filters", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:default-filter: (attributes org.gtk.Property.get=gtk_file_dialog_get_default_filter org.gtk.Property.set=gtk_file_dialog_set_default_filter)
   *
   * The default filter, that is, the filter that is initially
   * active in the file chooser dialog.
   *
   * If the default filter is %NULL, the first filter of [property@Gtk.FileDialog:filters]
   * is used as the default filter. If that property contains no filter, the dialog will
   * be unfiltered.
   *
   * If [property@Gtk.FileDialog:filters] is not %NULL, the default filter should be part
   * of the list. If it is not, the dialog may choose to not make it available.
   *
   * Since: 4.10
   */
  properties[PROP_DEFAULT_FILTER] =
      g_param_spec_object ("default-filter", NULL, NULL,
                           GTK_TYPE_FILE_FILTER,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:initial-file: (attributes org.gtk.Property.get=gtk_file_dialog_get_initial_file org.gtk.Property.set=gtk_file_dialog_set_initial_file)
   *
   * The initial file, that is, the file that is initially selected
   * in the file chooser dialog
   *
   * This is a utility property that sets both [property@Gtk.FileDialog:initial-folder] and
   * [property@Gtk.FileDialog:initial-name].
   *
   * Since: 4.10
   */
  properties[PROP_INITIAL_FILE] =
      g_param_spec_object ("initial-file", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:initial-folder: (attributes org.gtk.Property.get=gtk_file_dialog_get_initial_folder org.gtk.Property.set=gtk_file_dialog_set_initial_folder)
   *
   * The initial folder, that is, the directory that is initially
   * opened in the file chooser dialog
   *
   * Since: 4.10
   */
  properties[PROP_INITIAL_FOLDER] =
      g_param_spec_object ("initial-folder", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:initial-name: (attributes org.gtk.Property.get=gtk_file_dialog_get_initial_name org.gtk.Property.set=gtk_file_dialog_set_initial_name)
   *
   * The initial name, that is, the filename that is initially
   * selected in the file chooser dialog.
   *
   * Since: 4.10
   */
  properties[PROP_INITIAL_NAME] =
      g_param_spec_string ("initial-name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:accept-label: (attributes org.gtk.Property.get=gtk_file_dialog_get_accept_label org.gtk.Property.set=gtk_file_dialog_set_accept_label)
   *
   * Label for the file chooser's accept button.
   *
   * Since: 4.10
   */
  properties[PROP_ACCEPT_LABEL] =
      g_param_spec_string ("accept-label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Utilities */

static void
file_chooser_set_filters (GtkFileChooser *chooser,
                          GListModel     *filters)
{
  if (!filters)
    return;

  for (unsigned int i = 0; i < g_list_model_get_n_items (filters); i++)
    {
      GtkFileFilter *filter = g_list_model_get_item (filters, i);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_file_chooser_add_filter (chooser, filter);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (filter);
    }
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_file_dialog_new:
 *
 * Creates a new `GtkFileDialog` object.
 *
 * Returns: the new `GtkFileDialog`
 *
 * Since: 4.10
 */
GtkFileDialog *
gtk_file_dialog_new (void)
{
  return g_object_new (GTK_TYPE_FILE_DIALOG, NULL);
}

/* }}} */
/* {{{ API: Getters and setters */

/**
 * gtk_file_dialog_get_title:
 * @self: a `GtkFileDialog`
 *
 * Returns the title that will be shown on the
 * file chooser dialog.
 *
 * Returns: the title
 *
 * Since: 4.10
 */
const char *
gtk_file_dialog_get_title (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->title;
}

/**
 * gtk_file_dialog_set_title:
 * @self: a `GtkFileDialog`
 * @title: the new title
 *
 * Sets the title that will be shown on the
 * file chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_title (GtkFileDialog *self,
                           const char    *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (self->title, title) == 0)
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_file_dialog_get_modal:
 * @self: a `GtkFileDialog`
 *
 * Returns whether the file chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Returns: `TRUE` if the file chooser dialog is modal
 *
 * Since: 4.10
 */
gboolean
gtk_file_dialog_get_modal (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_file_dialog_set_modal:
 * @self: a `GtkFileDialog`
 * @modal: the new value
 *
 * Sets whether the file chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_modal (GtkFileDialog *self,
                           gboolean       modal)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_file_dialog_get_filters:
 * @self: a `GtkFileDialog`
 *
 * Gets the filters that will be offered to the user
 * in the file chooser dialog.
 *
 * Returns: (transfer none) (nullable): the filters, as
 *   a `GListModel` of `GtkFileFilters`
 *
 * Since: 4.10
 */
GListModel *
gtk_file_dialog_get_filters (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->filters;
}

/**
 * gtk_file_dialog_set_filters:
 * @self: a `GtkFileDialog`
 * @filters: (nullable): a `GListModel` of `GtkFileFilters`
 *
 * Sets the filters that will be offered to the user
 * in the file chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_filters (GtkFileDialog *self,
                             GListModel    *filters)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (filters == NULL || G_IS_LIST_MODEL (filters));

  if (!g_set_object (&self->filters, filters))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTERS]);
}

/**
 * gtk_file_dialog_get_default_filter:
 * @self: a `GtkFileDialog`
 *
 * Gets the filter that will be selected by default
 * in the file chooser dialog.
 *
 * Returns: (transfer none) (nullable): the current filter
 *
 * Since: 4.10
 */
GtkFileFilter *
gtk_file_dialog_get_default_filter (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->default_filter;
}

/**
 * gtk_file_dialog_set_default_filter:
 * @self: a `GtkFileDialog`
 * @filter: (nullable): a `GtkFileFilter`
 *
 * Sets the filter that will be selected by default
 * in the file chooser dialog.
 *
 * If set to %NULL, the first item in [property@Gtk.FileDialog:filters]
 * will be used as the default filter. If that list is empty, the dialog
 * will be unfiltered.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_default_filter (GtkFileDialog *self,
                                    GtkFileFilter *filter)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (filter == NULL || GTK_IS_FILE_FILTER (filter));

  if (!g_set_object (&self->default_filter, filter))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_FILTER]);
}

/**
 * gtk_file_dialog_get_initial_folder:
 * @self: a `GtkFileDialog`
 *
 * Gets the folder that will be set as the
 * initial folder in the file chooser dialog.
 *
 * Returns: (nullable) (transfer none): the folder
 *
 * Since: 4.10
 */
GFile *
gtk_file_dialog_get_initial_folder (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->initial_folder;
}

/**
 * gtk_file_dialog_set_initial_folder:
 * @self: a `GtkFileDialog`
 * @folder: (nullable): a `GFile`
 *
 * Sets the folder that will be set as the
 * initial folder in the file chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_initial_folder (GtkFileDialog *self,
                                    GFile         *folder)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (folder == NULL || G_IS_FILE (folder));

  if (!g_set_object (&self->initial_folder, folder))
    return;

  if (self->initial_name && self->initial_folder)
    {
      g_clear_object (&self->initial_file);
      self->initial_file = g_file_get_child_for_display_name (self->initial_folder,
                                                              self->initial_name,
                                                              NULL);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FILE]);
    }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FOLDER]);
}

/**
 * gtk_file_dialog_get_initial_name:
 * @self: a `GtkFileDialog`
 *
 * Gets the name for the file that should be initially set.
 *
 * Returns: (nullable) (transfer none): the name
 *
 * Since: 4.10
 */
const char *
gtk_file_dialog_get_initial_name (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->initial_name;
}

/**
 * gtk_file_dialog_set_initial_name:
 * @self: a `GtkFileDialog`
 * @name: (nullable): a UTF8 string
 *
 * Sets the name for the file that should be initially set.
 * For saving dialogs, this will usually be pre-entered into the name field.
 *
 * If a file with this name already exists in the directory set via
 * [property@Gtk.FileDialog:initial-folder], the dialog should preselect it.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_initial_name (GtkFileDialog *self,
                                  const char    *name)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (!g_set_str (&self->initial_name, name))
    return;

  if (self->initial_name && self->initial_folder)
    {
      g_clear_object (&self->initial_file);
      self->initial_file = g_file_get_child_for_display_name (self->initial_folder,
                                                              self->initial_name,
                                                              NULL);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FILE]);
    }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_NAME]);
}

/**
 * gtk_file_dialog_get_initial_file:
 * @self: a `GtkFileDialog`
 *
 * Gets the file that will be initially selected in
 * the file chooser dialog.
 *
 * Returns: (nullable) (transfer none): the file
 *
 * Since: 4.10
 */
GFile *
gtk_file_dialog_get_initial_file (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->initial_file;
}

/**
 * gtk_file_dialog_set_initial_file:
 * @self: a `GtkFileDialog`
 * @file: (nullable): a `GFile`
 *
 * Sets the file that will be initially selected in
 * the file chooser dialog.
 *
 * This function is a shortcut for calling both
 * gtk_file_dialog_set_initial_folder() and
 * gtk_file_dialog_set_initial_name() with the directory and
 * name of @file respectively.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_initial_file (GtkFileDialog *self,
                                  GFile         *file)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  g_object_freeze_notify (G_OBJECT (self));

  if (file != NULL)
    {
      GFile *folder;
      GFileInfo *info;
   
      if (self->initial_file && g_file_equal (self->initial_file, file))
        return;

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FILE]);

      folder = g_file_get_parent (file);
      if (folder == NULL)
        goto invalid_file;

      if (g_set_object (&self->initial_folder, folder))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FOLDER]);

      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME, 0, NULL, NULL);
      if (info && g_file_info_get_edit_name (info) != NULL)
        {
          if (g_set_str (&self->initial_name, g_file_info_get_edit_name (info)))
            g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_NAME]);
        }
      else
        {
          char *relative, *name;

          relative = g_file_get_relative_path (folder, file);
          name = g_filename_display_name (relative);
          if (g_set_str (&self->initial_name, name))
            g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_NAME]);

          g_free (name);
          g_free (relative);
        }
      g_clear_object (&info);
      g_object_unref (folder);
    }
  else
    {
invalid_file:
      if (g_set_object (&self->initial_file, NULL))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FILE]);
      if (g_set_object (&self->initial_folder, NULL))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_FOLDER]);
      if (g_set_str (&self->initial_name, NULL))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_NAME]);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

/* }}} */
/* {{{ Async implementation */

static void response_cb (GTask *task,
                         int    response);

static void
cancelled_cb (GCancellable *cancellable,
              GTask        *task)
{
  response_cb (task, GTK_RESPONSE_CLOSE);
}

static void
response_cb (GTask *task,
             int    response)
{
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (response == GTK_RESPONSE_ACCEPT)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      GtkFileChooser *chooser;
      GListModel *files;

      chooser = GTK_FILE_CHOOSER (g_task_get_task_data (task));
      files = gtk_file_chooser_get_files (chooser);
      g_task_return_pointer (task, files, g_object_unref);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
  else if (response == GTK_RESPONSE_CLOSE)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by application");
  else if (response == GTK_RESPONSE_CANCEL ||
           response == GTK_RESPONSE_DELETE_EVENT)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "Dismissed by user");
  else
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "Unknown failure (%d)", response);

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (g_task_get_task_data (task)));

  g_object_unref (task);
}

static void
dialog_response (GtkDialog *dialog,
                 int        response,
                 GTask     *task)
{
  response_cb (task, response);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GtkFileChooserNative *
create_file_chooser (GtkFileDialog        *self,
                     GtkWindow            *parent,
                     GtkFileChooserAction  action,
                     gboolean              select_multiple)
{
  GtkFileChooserNative *chooser;
  const char *default_accept_label, *accept;
  const char *default_title, *title;
  GdkDisplay *display G_GNUC_UNUSED;

  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      default_accept_label = _("_Open");
      default_title = select_multiple ? _("Pick Files") : _("Pick a File");
      break;

    case GTK_FILE_CHOOSER_ACTION_SAVE:
      default_accept_label = _("_Save");
      default_title = _("Save a File");
      break;

    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      default_accept_label = _("_Select");
      default_title = select_multiple ? _("Select Folders") : _("Select a Folder");
      break;

    default:
      g_assert_not_reached ();
    }

  if (self->title)
    title = self->title;
  else
    title = default_title;

  if (self->accept_label)
    accept = self->accept_label;
  else
    accept = default_accept_label;

  chooser = gtk_file_chooser_native_new (title, parent, action, accept, _("_Cancel"));

  if (parent)
    display = gtk_widget_get_display (GTK_WIDGET (parent));
  else
    display = gdk_display_get_default ();

  if (GDK_DISPLAY_DEBUG_CHECK (display, NO_PORTALS))
    gtk_file_chooser_native_set_use_portal (chooser, FALSE);
  else
    gtk_file_chooser_native_set_use_portal (chooser, TRUE);

  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), self->modal);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), select_multiple);

  file_chooser_set_filters (GTK_FILE_CHOOSER (chooser), self->filters);
  if (self->default_filter)
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), self->default_filter);
  else if (self->filters)
    {
      GtkFileFilter *filter = g_list_model_get_item (self->filters, 0);
      if (filter)
        {
          gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);
          g_object_unref (filter);
        }
    }

  if (self->initial_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), self->initial_folder, NULL);
  if (self->initial_name && action == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), self->initial_name);

  return chooser;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static GFile *
finish_file_op (GtkFileDialog  *self,
                GTask          *task,
                GError        **error)
{
  GListModel *files;

  files = g_task_propagate_pointer (task, error);
  if (files)
    {
      GFile *file = NULL;

      if (g_list_model_get_n_items (files) > 0)
        file = g_list_model_get_item (files, 0);
      else
        g_set_error_literal (error,
                             GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                             "No file selected");

      g_object_unref (files);

      return file;
    }

  return NULL;
}

static GListModel *
finish_multiple_files_op (GtkFileDialog  *self,
                          GTask          *task,
                          GError        **error)
{
  return G_LIST_MODEL (g_task_propagate_pointer (task, error));
}

/* }}} */
/* {{{ Async API */

/**
 * gtk_file_dialog_open:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a file selection operation by
 * presenting a file chooser dialog to the user.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FileDialog.open_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_open (GtkFileDialog       *self,
                      GtkWindow           *parent,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GtkFileChooserNative *chooser;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  chooser = create_file_chooser (self, parent, GTK_FILE_CHOOSER_ACTION_OPEN, FALSE);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_dialog_open);
  g_task_set_task_data (task, chooser, g_object_unref);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (chooser, "response", G_CALLBACK (dialog_response), task);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

/**
 * gtk_file_dialog_open_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FileDialog.open] call and
 * returns the resulting file.
 *
 * Returns: (nullable) (transfer full): the file that was selected.
 *   Otherwise, `NULL` is returned and @error is set
 *
 * Since: 4.10
 */
GFile *
gtk_file_dialog_open_finish (GtkFileDialog   *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_dialog_open, NULL);

  return finish_file_op (self, G_TASK (result), error);
}

/**
 * gtk_file_dialog_select_folder:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a directory selection operation by
 * presenting a file chooser dialog to the user.
 *
 * If you pass @initial_folder, the file chooser will initially be
 * opened in the parent directory of that folder, otherwise, it
 * will be in the directory [property@Gtk.FileDialog:initial-folder].
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FileDialog.select_folder_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_select_folder (GtkFileDialog       *self,
                               GtkWindow           *parent,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GtkFileChooserNative *chooser;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  chooser = create_file_chooser (self, parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, FALSE);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_dialog_select_folder);
  g_task_set_task_data (task, chooser, g_object_unref);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (chooser, "response", G_CALLBACK (dialog_response), task);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

/**
 * gtk_file_dialog_select_folder_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FileDialog.select_folder] call and
 * returns the resulting file.
 *
 * Returns: (nullable) (transfer full): the file that was selected.
 *   Otherwise, `NULL` is returned and @error is set
 *
 * Since: 4.10
 */
GFile *
gtk_file_dialog_select_folder_finish (GtkFileDialog  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_dialog_select_folder, NULL);

  return finish_file_op (self, G_TASK (result), error);
}

/**
 * gtk_file_dialog_save:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a file save operation by
 * presenting a file chooser dialog to the user.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FileDialog.save_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_save (GtkFileDialog       *self,
                      GtkWindow           *parent,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GtkFileChooserNative *chooser;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  chooser = create_file_chooser (self, parent, GTK_FILE_CHOOSER_ACTION_SAVE, FALSE);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_dialog_save);
  g_task_set_task_data (task, chooser, g_object_unref);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (chooser, "response", G_CALLBACK (dialog_response), task);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

/**
 * gtk_file_dialog_save_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FileDialog.save] call and
 * returns the resulting file.
 *
 * Returns: (nullable) (transfer full): the file that was selected.
 *   Otherwise, `NULL` is returned and @error is set
 *
 * Since: 4.10
 */
GFile *
gtk_file_dialog_save_finish (GtkFileDialog   *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_dialog_save, NULL);

  return finish_file_op (self, G_TASK (result), error);
}

/**
 * gtk_file_dialog_open_multiple:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a multi-file selection operation by
 * presenting a file chooser dialog to the user.
 *
 * The file chooser will initially be opened in the directory
 * [property@Gtk.FileDialog:initial-folder].
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FileDialog.open_multiple_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_open_multiple (GtkFileDialog       *self,
                               GtkWindow           *parent,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GtkFileChooserNative *chooser;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  chooser = create_file_chooser (self, parent, GTK_FILE_CHOOSER_ACTION_OPEN, TRUE);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_dialog_open_multiple);
  g_task_set_task_data (task, chooser, g_object_unref);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (chooser, "response", G_CALLBACK (dialog_response), task);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

/**
 * gtk_file_dialog_open_multiple_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FileDialog.open] call and
 * returns the resulting files in a `GListModel`.
 *
 * Returns: (nullable) (transfer full): the file that was selected,
 *   as a `GListModel` of `GFiles`. Otherwise, `NULL` is returned
 *   and @error is set
 *
 * Since: 4.10
 */
GListModel *
gtk_file_dialog_open_multiple_finish (GtkFileDialog   *self,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_dialog_open_multiple, NULL);

  return finish_multiple_files_op (self, G_TASK (result), error);
}

/**
 * gtk_file_dialog_select_multiple_folders:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a multi-directory selection operation by
 * presenting a file chooser dialog to the user.
 *
 * The file chooser will initially be opened in the directory
 * [property@Gtk.FileDialog:initial-folder].
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FileDialog.select_multiple_folders_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_select_multiple_folders (GtkFileDialog       *self,
                                         GtkWindow           *parent,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GtkFileChooserNative *chooser;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  chooser = create_file_chooser (self, parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, TRUE);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_dialog_select_multiple_folders);
  g_task_set_task_data (task, chooser, g_object_unref);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (chooser, "response", G_CALLBACK (dialog_response), task);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

/**
 * gtk_file_dialog_select_multiple_folders_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FileDialog.select_multiple_folders]
 * call and returns the resulting files in a `GListModel`.
 *
 * Returns: (nullable) (transfer full): the file that was selected,
 *   as a `GListModel` of `GFiles`. Otherwise, `NULL` is returned
 *   and @error is set
 *
 * Since: 4.10
 */
GListModel *
gtk_file_dialog_select_multiple_folders_finish (GtkFileDialog   *self,
                                                GAsyncResult    *result,
                                                GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_dialog_select_multiple_folders, NULL);

  return finish_multiple_files_op (self, G_TASK (result), error);
}

/**
 * gtk_file_dialog_get_accept_label:
 * @self: a `GtkFileDialog`
 *
 * Returns: (nullable): the label shown on the file chooser's accept button.
 *
 * Since: 4.10
 */
const char *
gtk_file_dialog_get_accept_label (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->accept_label;
}

/**
 * gtk_file_dialog_set_accept_label:
 * @self: a `GtkFileDialog`
 * @accept_label: (nullable): the new accept label
 *
 * Sets the label shown on the file chooser's accept button.
 *
 * Leaving the accept label unset or setting it as `NULL` will fall back to
 * a default label, depending on what API is used to launch the file dialog.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_accept_label (GtkFileDialog *self,
                                  const char    *accept_label)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (g_set_str (&self->accept_label, accept_label))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCEPT_LABEL]);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
