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

#include "gtkfontdialog.h"

#include "deprecated/gtkfontchooser.h"
#include "gtkfontchooserdialogprivate.h"
#include "gtkbutton.h"
#include "gtkdialogerror.h"
#include "gtktypebuiltins.h"
#include <glib/gi18n-lib.h>

/**
 * GtkFontDialog:
 *
 * A `GtkFontDialog` object collects the arguments that
 * are needed to present a font chooser dialog to the
 * user, such as a title for the dialog and whether it
 * should be modal.
 *
 * The dialog is shown with the [method@Gtk.FontDialog.choose_font]
 * function or its variants. This API follows the GIO async pattern,
 * and the result can be obtained by calling the corresponding
 * finish function, such as [method@Gtk.FontDialog.choose_font_finish].
 *
 * See [class@Gtk.FontDialogButton] for a convenient control
 * that uses `GtkFontDialog` and presents the results.
 *
 * Since: 4.10
 */

/* {{{ GObject implementation */

struct _GtkFontDialog
{
  GObject parent_instance;

  char *title;
  PangoLanguage *language;
  PangoFontMap *fontmap;

  unsigned int modal : 1;

  GtkFilter *filter;
};

enum
{
  PROP_TITLE = 1,
  PROP_MODAL,
  PROP_LANGUAGE,
  PROP_FONT_MAP,
  PROP_FILTER,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkFontDialog, gtk_font_dialog, G_TYPE_OBJECT)

static void
gtk_font_dialog_init (GtkFontDialog *self)
{
  self->modal = TRUE;
  self->language = pango_language_get_default ();
}

static void
gtk_font_dialog_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkFontDialog *self = GTK_FONT_DIALOG (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_LANGUAGE:
      g_value_set_boxed (value, self->language);
      break;

    case PROP_FONT_MAP:
      g_value_set_object (value, self->fontmap);
      break;

    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_font_dialog_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFontDialog *self = GTK_FONT_DIALOG (object);

  switch (property_id)
    {
    case PROP_TITLE:
      gtk_font_dialog_set_title (self, g_value_get_string (value));
      break;

    case PROP_MODAL:
      gtk_font_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_LANGUAGE:
      gtk_font_dialog_set_language (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_MAP:
      gtk_font_dialog_set_font_map (self, g_value_get_object (value));
      break;

    case PROP_FILTER:
      gtk_font_dialog_set_filter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_font_dialog_finalize (GObject *object)
{
  GtkFontDialog *self = GTK_FONT_DIALOG (object);

  g_free (self->title);
  g_clear_object (&self->fontmap);
  g_clear_object (&self->filter);

  G_OBJECT_CLASS (gtk_font_dialog_parent_class)->finalize (object);
}

static void
gtk_font_dialog_class_init (GtkFontDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_font_dialog_get_property;
  object_class->set_property = gtk_font_dialog_set_property;
  object_class->finalize = gtk_font_dialog_finalize;

  /**
   * GtkFontDialog:title: (attributes org.gtk.Property.get=gtk_font_dialog_get_title org.gtk.Property.set=gtk_font_dialog_set_title)
   *
   * A title that may be shown on the font chooser
   * dialog that is presented by [method@Gtk.FontDialog.choose_font].
   *
   * Since: 4.10
   */
  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialog:modal: (attributes org.gtk.Property.get=gtk_font_dialog_get_modal org.gtk.Property.set=gtk_font_dialog_set_modal)
   *
   * Whether the font chooser dialog is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialog:language: (attributes org.gtk.Property.get=gtk_font_dialog_get_language org.gtk.Property.set=gtk_font_dialog_set_language)
   *
   * The language for which the font features are selected.
   *
   * Since: 4.10
   */
  properties[PROP_LANGUAGE] =
      g_param_spec_boxed ("language", NULL, NULL,
                          PANGO_TYPE_LANGUAGE,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialog:font-map: (attributes org.gtk.Property.get=gtk_font_dialog_get_font_map org.gtk.Property.set=gtk_font_dialog_set_font_map)
   *
   * Sets a custom font map to select fonts from.
   *
   * A custom font map can be used to present application-specific
   * fonts instead of or in addition to the normal system fonts.
   *
   * Since: 4.10
   */
  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map", NULL, NULL,
                           PANGO_TYPE_FONT_MAP,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialog:filter: (attributes org.gtk.Property.get=gtk_font_dialog_get_filter org.gtk.Property.set=gtk_font_dialog_set_filter)
   *
   * Sets a filter to restrict what fonts are shown
   * in the font chooser dialog.
   *
   * Since: 4.10
   */
  properties[PROP_FILTER] =
      g_param_spec_object ("filter", NULL, NULL,
                           GTK_TYPE_FILTER,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_font_dialog_new:
 *
 * Creates a new `GtkFontDialog` object.
 *
 * Returns: the new `GtkFontDialog`
 *
 * Since: 4.10
 */
GtkFontDialog *
gtk_font_dialog_new (void)
{
  return g_object_new (GTK_TYPE_FONT_DIALOG, NULL);
}

/* }}} */
/* {{{ API: Getters and setters */

/**
 * gtk_font_dialog_get_title:
 * @self: a `GtkFontDialog`
 *
 * Returns the title that will be shown on the
 * font chooser dialog.
 *
 * Returns: the title
 *
 * Since: 4.10
 */
const char *
gtk_font_dialog_get_title (GtkFontDialog *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);

  return self->title;
}

/**
 * gtk_font_dialog_set_title:
 * @self: a `GtkFontDialog`
 * @title: the new title
 *
 * Sets the title that will be shown on the
 * font chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_set_title (GtkFontDialog *self,
                           const char    *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_FONT_DIALOG (self));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (self->title, title) == 0)
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_font_dialog_get_modal:
 * @self: a `GtkFontDialog`
 *
 * Returns whether the font chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Returns: `TRUE` if the font chooser dialog is modal
 *
 * Since: 4.10
 */
gboolean
gtk_font_dialog_get_modal (GtkFontDialog *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_font_dialog_set_modal:
 * @self: a `GtkFontDialog`
 * @modal: the new value
 *
 * Sets whether the font chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_set_modal (GtkFontDialog *self,
                           gboolean       modal)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_font_dialog_get_language:
 * @self: a `GtkFontDialog`
 *
 * Returns the language for which font features are applied.
 *
 * Returns: (nullable): the language for font features
 *
 * Since: 4.10
 */
PangoLanguage *
gtk_font_dialog_get_language (GtkFontDialog *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);

  return self->language;
}

/**
 * gtk_font_dialog_set_language:
 * @self: a `GtkFontDialog`
 * @language: the language for font features
 *
 * Sets the language for which font features are applied.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_set_language (GtkFontDialog *self,
                              PangoLanguage *language)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  if (self->language == language)
    return;

  self->language = language;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LANGUAGE]);
}

/**
 * gtk_font_dialog_get_font_map:
 * @self: a `GtkFontDialog`
 *
 * Returns the fontmap from which fonts are selected,
 * or `NULL` for the default fontmap.
 *
 * Returns: (nullable) (transfer none): the fontmap
 *
 * Since: 4.10
 */
PangoFontMap *
gtk_font_dialog_get_font_map (GtkFontDialog *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);

  return self->fontmap;
}

/**
 * gtk_font_dialog_set_font_map:
 * @self: a `GtkFontDialog`
 * @fontmap: (nullable): the fontmap
 *
 * Sets the fontmap from which fonts are selected.
 *
 * If @fontmap is `NULL`, the default fontmap is used.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_set_font_map (GtkFontDialog *self,
                              PangoFontMap  *fontmap)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  if (g_set_object (&self->fontmap, fontmap))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_MAP]);
}

/**
 * gtk_font_dialog_get_filter:
 * @self: a `GtkFontDialog`
 *
 * Returns the filter that decides which fonts to display
 * in the font chooser dialog.
 *
 * Returns: (nullable) (transfer none): the filter
 *
 * Since: 4.10
 */
GtkFilter *
gtk_font_dialog_get_filter (GtkFontDialog *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);

  return self->filter;
}
/**
 * gtk_font_dialog_set_filter:
 * @self: a `GtkFontDialog`
 * @filter: (nullable): a `GtkFilter`
 *
 * Adds a filter that decides which fonts to display
 * in the font chooser dialog.
 *
 * The `GtkFilter` must be able to handle both `PangoFontFamily`
 * and `PangoFontFace` objects.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_set_filter (GtkFontDialog *self,
                            GtkFilter     *filter)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG (self));
  g_return_if_fail (filter == NULL || GTK_IS_FILTER (filter));

  if (g_set_object (&self->filter, filter))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTER]);
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

typedef struct
{
  PangoFontDescription *font_desc;
  char *font_features;
  PangoLanguage *language;
} FontResult;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
response_cb (GTask *task,
             int    response)
{
  GCancellable *cancellable;
  GtkFontChooserDialog *window;
  GtkFontChooserLevel level;

  cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);


  window = GTK_FONT_CHOOSER_DIALOG (g_task_get_task_data (task));
  level = gtk_font_chooser_get_level (GTK_FONT_CHOOSER (window));

  if (response == GTK_RESPONSE_OK)
    {
      if (level & GTK_FONT_CHOOSER_LEVEL_FEATURES)
        {
          FontResult font_result;

          font_result.font_desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (window));
          font_result.font_features = gtk_font_chooser_get_font_features (GTK_FONT_CHOOSER (window));
          font_result.language = pango_language_from_string (gtk_font_chooser_get_language (GTK_FONT_CHOOSER (window)));

          g_task_return_pointer (task, &font_result, NULL);

          g_clear_pointer (&font_result.font_desc, pango_font_description_free);
          g_clear_pointer (&font_result.font_features, g_free);
        }
      else if (level & GTK_FONT_CHOOSER_LEVEL_SIZE)
        {
          PangoFontDescription *font_desc;

          font_desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (window));

          g_task_return_pointer (task, font_desc, (GDestroyNotify) pango_font_description_free);
        }
      else if (level & GTK_FONT_CHOOSER_LEVEL_STYLE)
        {
          PangoFontFace *face;

          face = gtk_font_chooser_get_font_face (GTK_FONT_CHOOSER (window));

          g_task_return_pointer (task, g_object_ref (face), g_object_unref);
        }
      else
        {
          PangoFontFamily *family;

          family = gtk_font_chooser_get_font_family (GTK_FONT_CHOOSER (window));

          g_task_return_pointer (task, g_object_ref (family), g_object_unref);
        }
    }
  else if (response == GTK_RESPONSE_CLOSE)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by application");
  else if (response == GTK_RESPONSE_CANCEL ||
           response == GTK_RESPONSE_DELETE_EVENT)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "Dismissed by user");
  else
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "Unknown failure (%d)", response);

  g_object_unref (task);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
dialog_response (GtkDialog *dialog,
                 int        response,
                 GTask     *task)
{
  response_cb (task, response);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GtkWidget *
create_font_chooser (GtkFontDialog        *self,
                     GtkWindow            *parent,
                     PangoFontDescription *initial_value,
                     GtkFontChooserLevel   level)
{
  GtkWidget *window;
  const char *title;

  if (self->title)
    title = self->title;
  else
    title = _("Pick a Font");

  window = gtk_font_chooser_dialog_new (title, parent);
  gtk_font_chooser_set_level (GTK_FONT_CHOOSER (window), level);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  if (self->language)
    gtk_font_chooser_set_language (GTK_FONT_CHOOSER (window),
                                   pango_language_to_string (self->language));
  if (self->fontmap)
    gtk_font_chooser_set_font_map (GTK_FONT_CHOOSER (window), self->fontmap);
  if (self->filter)
    gtk_font_chooser_dialog_set_filter (GTK_FONT_CHOOSER_DIALOG (window), self->filter);
  if (initial_value)
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (window), initial_value);

  return window;
}
G_GNUC_END_IGNORE_DEPRECATIONS

/* }}} */
/* {{{ Async API */

/**
 * gtk_font_dialog_choose_family:
 * @self: a `GtkFontDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_value: (nullable): the initial value
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a font selection operation by
 * presenting a dialog to the user for selecting a font family.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FontDialog.choose_family_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_choose_family (GtkFontDialog        *self,
                               GtkWindow            *parent,
                               PangoFontFamily      *initial_value,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  GtkWidget *window;
  PangoFontDescription *desc = NULL;
  GTask *task;

  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  if (initial_value)
    {
      desc = pango_font_description_new ();
      pango_font_description_set_family (desc, pango_font_family_get_name (initial_value));
    }

  window = create_font_chooser (self, parent, desc,
                                GTK_FONT_CHOOSER_LEVEL_FAMILY);

  g_clear_pointer (&desc, pango_font_description_free);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_font_dialog_choose_family);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_font_dialog_choose_family_finish:
 * @self: a `GtkFontDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FontDialog.choose_family] call
 * and returns the resulting family.
 *
 * This function never returns an error. If the operation is
 * not finished successfully, the value passed as @initial_value
 * to [method@Gtk.FontDialog.choose_family] is returned.

 * Returns: (nullable) (transfer full): the selected family
 *
 * Since: 4.10
 */
PangoFontFamily *
gtk_font_dialog_choose_family_finish (GtkFontDialog  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_font_dialog_choose_family, NULL);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gtk_font_dialog_choose_face:
 * @self: a `GtkFontDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_value: (nullable): the initial value
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a font selection operation by
 * presenting a dialog to the user for selecting a font face
 * (i.e. a font family and style, but not a specific font size).
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FontDialog.choose_face_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_choose_face (GtkFontDialog       *self,
                             GtkWindow           *parent,
                             PangoFontFace       *initial_value,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GtkWidget *window;
  PangoFontDescription *desc = NULL;
  GTask *task;

  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  if (initial_value)
    desc = pango_font_face_describe (initial_value);

  window = create_font_chooser (self, parent, desc,
                                GTK_FONT_CHOOSER_LEVEL_FAMILY |
                                GTK_FONT_CHOOSER_LEVEL_STYLE);

  g_clear_pointer (&desc, pango_font_description_free);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_font_dialog_choose_face);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_font_dialog_choose_face_finish:
 * @self: a `GtkFontDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FontDialog.choose_face] call
 * and returns the resulting font face.
 *
 * Returns: (nullable) (transfer full): the selected font face
 *
 * Since: 4.10
 */
PangoFontFace *
gtk_font_dialog_choose_face_finish (GtkFontDialog  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_font_dialog_choose_face, NULL);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gtk_font_dialog_choose_font:
 * @self: a `GtkFontDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_value: (nullable): the font to select initially
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a font selection operation by
 * presenting a dialog to the user for selecting a font.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FontDialog.choose_font_finish]
 * to obtain the result.
 *
 * If you want to let the user select font features as well,
 * use [method@Gtk.FontDialog.choose_font_and_features] instead.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_choose_font (GtkFontDialog        *self,
                             GtkWindow            *parent,
                             PangoFontDescription *initial_value,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  GtkWidget *window;
  GTask *task;

  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  window = create_font_chooser (self, parent, initial_value,
                                GTK_FONT_CHOOSER_LEVEL_FAMILY |
                                GTK_FONT_CHOOSER_LEVEL_STYLE |
                                GTK_FONT_CHOOSER_LEVEL_SIZE |
                                GTK_FONT_CHOOSER_LEVEL_VARIATIONS);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_font_dialog_choose_font);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_font_dialog_choose_font_finish:
 * @self: a `GtkFontDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FontDialog.choose_font] call
 * and returns the resulting font description.
 *
 * Returns: (nullable) (transfer full): the selected font
 *
 * Since: 4.10
 */
PangoFontDescription *
gtk_font_dialog_choose_font_finish (GtkFontDialog  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_font_dialog_choose_font, NULL);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gtk_font_dialog_choose_font_and_features:
 * @self: a `GtkFontDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_value: (nullable): the font to select initially
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a font selection operation by
 * presenting a dialog to the user for selecting a font and
 * font features.
 *
 * Font features affect how the font is rendered, for example
 * enabling glyph variants or ligatures.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.FontDialog.choose_font_and_features_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_choose_font_and_features (GtkFontDialog        *self,
                                          GtkWindow            *parent,
                                          PangoFontDescription *initial_value,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  GtkWidget *window;
  GTask *task;

  g_return_if_fail (GTK_IS_FONT_DIALOG (self));

  window = create_font_chooser (self, parent, initial_value,
                                GTK_FONT_CHOOSER_LEVEL_FAMILY |
                                GTK_FONT_CHOOSER_LEVEL_STYLE |
                                GTK_FONT_CHOOSER_LEVEL_SIZE |
                                GTK_FONT_CHOOSER_LEVEL_VARIATIONS |
                                GTK_FONT_CHOOSER_LEVEL_FEATURES);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_font_dialog_choose_font_and_features);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_font_dialog_choose_font_and_features_finish:
 * @self: a `GtkFontDialog`
 * @result: a `GAsyncResult`
 * @font_desc: (out): return location for font description
 * @font_features: (out): return location for font features
 * @language: (out): return location for the language
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.FontDialog.choose_font_and_features]
 * call and returns the resulting font description and font features.
 *
 * Returns: `TRUE` if a font was selected. Otherwise `FALSE` is returned
 *   and @error is set
 *
 * Since: 4.10
 */
gboolean
gtk_font_dialog_choose_font_and_features_finish (GtkFontDialog         *self,
                                                 GAsyncResult          *result,
                                                 PangoFontDescription **font_desc,
                                                 char                 **font_features,
                                                 PangoLanguage        **language,
                                                 GError               **error)
{
  FontResult *font_result;

  g_return_val_if_fail (GTK_IS_FONT_DIALOG (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_font_dialog_choose_font_and_features, FALSE);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);
  font_result = g_task_propagate_pointer (G_TASK (result), error);

  if (font_result)
    {
      *font_desc = g_steal_pointer (&font_result->font_desc);
      *font_features = g_steal_pointer (&font_result->font_features);
      *language = g_steal_pointer (&font_result->language);
    }

  return font_result != NULL;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
