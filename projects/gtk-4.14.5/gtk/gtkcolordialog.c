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

#include "gtkcolordialog.h"

#include "deprecated/gtkcolorchooserdialog.h"
#include "deprecated/gtkcolorchooser.h"
#include "gtkbutton.h"
#include "gtkdialogerror.h"
#include <glib/gi18n-lib.h>

/**
 * GtkColorDialog:
 *
 * A `GtkColorDialog` object collects the arguments that
 * are needed to present a color chooser dialog to the
 * user, such as a title for the dialog and whether it
 * should be modal.
 *
 * The dialog is shown with the [method@Gtk.ColorDialog.choose_rgba]
 * function. This API follows the GIO async pattern, and the
 * result can be obtained by calling
 * [method@Gtk.ColorDialog.choose_rgba_finish].
 *
 * See [class@Gtk.ColorDialogButton] for a convenient control
 * that uses `GtkColorDialog` and presents the results.
 *
 * Since: 4.10
 */
/* {{{ GObject implementation */

struct _GtkColorDialog
{
  GObject parent_instance;

  char *title;

  unsigned int modal : 1;
  unsigned int with_alpha : 1;
};

enum
{
  PROP_TITLE = 1,
  PROP_MODAL,
  PROP_WITH_ALPHA,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkColorDialog, gtk_color_dialog, G_TYPE_OBJECT)

static void
gtk_color_dialog_init (GtkColorDialog *self)
{
  self->modal = TRUE;
  self->with_alpha = TRUE;
}

static void
gtk_color_dialog_finalize (GObject *object)
{
  GtkColorDialog *self = GTK_COLOR_DIALOG (object);

  g_free (self->title);

  G_OBJECT_CLASS (gtk_color_dialog_parent_class)->finalize (object);
}

static void
gtk_color_dialog_get_property (GObject      *object,
                               unsigned int  property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkColorDialog *self = GTK_COLOR_DIALOG (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_WITH_ALPHA:
      g_value_set_boolean (value, self->with_alpha);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_dialog_set_property (GObject      *object,
                               unsigned int  prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorDialog *self = GTK_COLOR_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_color_dialog_set_title (self, g_value_get_string (value));
      break;

    case PROP_MODAL:
      gtk_color_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_WITH_ALPHA:
      gtk_color_dialog_set_with_alpha (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_dialog_class_init (GtkColorDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_color_dialog_finalize;
  object_class->get_property = gtk_color_dialog_get_property;
  object_class->set_property = gtk_color_dialog_set_property;

  /**
   * GtkColorDialog:title: (attributes org.gtk.Property.get=gtk_color_dialog_get_title org.gtk.Property.set=gtk_color_dialog_set_title)
   *
   * A title that may be shown on the color chooser
   * dialog that is presented by [method@Gtk.ColorDialog.choose_rgba].
   *
   * Since: 4.10
   */
  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColorDialog:modal: (attributes org.gtk.Property.get=gtk_color_dialog_get_modal org.gtk.Property.set=gtk_color_dialog_set_modal)
   *
   * Whether the color chooser dialog is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColorDialog:with-alpha: (attributes org.gtk.Property.get=gtk_color_dialog_get_with_alpha org.gtk.Property.set=gtk_color_dialog_set_with_alpha)
   *
   * Whether colors may have alpha (translucency).
   *
   * When with-alpha is %FALSE, the color that is selected
   * will be forced to have alpha == 1.
   *
   * Since: 4.10
   */
  properties[PROP_WITH_ALPHA] =
      g_param_spec_boolean ("with-alpha", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_color_dialog_new:
 *
 * Creates a new `GtkColorDialog` object.
 *
 * Returns: the new `GtkColorDialog`
 *
 * Since: 4.10
 */
GtkColorDialog *
gtk_color_dialog_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_DIALOG, NULL);
}

/* }}} */
/* {{{ API: Getters and setters */

/**
 * gtk_color_dialog_get_title:
 * @self: a `GtkColorDialog`
 *
 * Returns the title that will be shown on the
 * color chooser dialog.
 *
 * Returns: the title
 *
 * Since: 4.10
 */
const char *
gtk_color_dialog_get_title (GtkColorDialog *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_DIALOG (self), NULL);

  return self->title;
}

/**
 * gtk_color_dialog_set_title:
 * @self: a `GtkColorDialog`
 * @title: the new title
 *
 * Sets the title that will be shown on the
 * color chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_color_dialog_set_title (GtkColorDialog *self,
                            const char     *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_COLOR_DIALOG (self));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (self->title, title) == 0)
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_color_dialog_get_modal:
 * @self: a `GtkColorDialog`
 *
 * Returns whether the color chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Returns: `TRUE` if the color chooser dialog is modal
 *
 * Since: 4.10
 */
gboolean
gtk_color_dialog_get_modal (GtkColorDialog *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_color_dialog_set_modal:
 * @self: a `GtkColorDialog`
 * @modal: the new value
 *
 * Sets whether the color chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Since: 4.10
 */
void
gtk_color_dialog_set_modal (GtkColorDialog *self,
                            gboolean        modal)
{
  g_return_if_fail (GTK_IS_COLOR_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_color_dialog_get_with_alpha:
 * @self: a `GtkColorDialog`
 *
 * Returns whether colors may have alpha.
 *
 * Returns: `TRUE` if colors may have alpha
 *
 * Since: 4.10
 */
gboolean
gtk_color_dialog_get_with_alpha (GtkColorDialog *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_DIALOG (self), TRUE);

  return self->with_alpha;
}

/**
 * gtk_color_dialog_set_with_alpha:
 * @self: a `GtkColorDialog`
 * @with_alpha: the new value
 *
 * Sets whether colors may have alpha.
 *
 * Since: 4.10
 */
void
gtk_color_dialog_set_with_alpha (GtkColorDialog *self,
                                 gboolean        with_alpha)
{
  g_return_if_fail (GTK_IS_COLOR_DIALOG (self));

  if (self->with_alpha == with_alpha)
    return;

  self->with_alpha = with_alpha;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WITH_ALPHA]);
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

  if (response == GTK_RESPONSE_OK)
    {
      GtkColorChooserDialog *window;
      GdkRGBA color;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      window = GTK_COLOR_CHOOSER_DIALOG (g_task_get_task_data (task));
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (window), &color);
G_GNUC_END_IGNORE_DEPRECATIONS

      g_task_return_pointer (task, gdk_rgba_copy (&color), (GDestroyNotify) gdk_rgba_free);
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

static GtkWidget *
create_color_chooser (GtkColorDialog *self,
                      GtkWindow      *parent,
                      const GdkRGBA  *initial_color)
{
  GtkWidget *window;
  char *title;

  if (self->title)
    title = self->title;
  else
    title = _("Pick a Color");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  window = gtk_color_chooser_dialog_new (title, parent);
  if (initial_color)
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (window), initial_color);
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (window), self->with_alpha);
  gtk_window_set_modal (GTK_WINDOW (window), self->modal);
G_GNUC_END_IGNORE_DEPRECATIONS

  return window;
}

/* }}} */
/* {{{ Async API */

/**
 * gtk_color_dialog_choose_rgba:
 * @self: a `GtkColorDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_color: (nullable): the color to select initially
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a color choice operation by
 * presenting a color chooser dialog to the user.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.ColorDialog.choose_rgba_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_color_dialog_choose_rgba (GtkColorDialog       *self,
                              GtkWindow            *parent,
                              const GdkRGBA        *initial_color,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  GtkWidget *window;
  GTask *task;

  g_return_if_fail (GTK_IS_COLOR_DIALOG (self));

  window = create_color_chooser (self, parent, initial_color);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_color_dialog_choose_rgba);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);
  g_signal_connect_swapped (window, "response", G_CALLBACK (response_cb), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_color_dialog_choose_rgba_finish:
 * @self: a `GtkColorDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.ColorDialog.choose_rgba] call and
 * returns the resulting color.
 *
 * Returns: (nullable) (transfer full): the selected color, or
 *   `NULL` and @error is set
 *
 * Since: 4.10
 */
GdkRGBA *
gtk_color_dialog_choose_rgba_finish (GtkColorDialog  *self,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  g_return_val_if_fail (GTK_IS_COLOR_DIALOG (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_color_dialog_choose_rgba, NULL);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
