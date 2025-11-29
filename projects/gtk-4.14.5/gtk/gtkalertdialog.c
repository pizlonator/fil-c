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

#include "gtkalertdialog.h"

#include "gtkbutton.h"
#include "gtkdialogerror.h"
#include "deprecated/gtkmessagedialog.h"
#include <glib/gi18n-lib.h>

/**
 * GtkAlertDialog:
 *
 * A `GtkAlertDialog` object collects the arguments that
 * are needed to present a message to the user.
 *
 * The message is shown with the [method@Gtk.AlertDialog.choose]
 * function. This API follows the GIO async pattern, and the result can
 * be obtained by calling [method@Gtk.AlertDialog.choose_finish].
 *
 * If you don't need to wait for a button to be clicked, you can use
 * [method@Gtk.AlertDialog.show].
 *
 * Since: 4.10
 */

/* {{{ GObject implementation */

struct _GtkAlertDialog
{
  GObject parent_instance;

  char *message;
  char *detail;
  char **buttons;

  int cancel_button;
  int default_button;

  int cancel_return;

  unsigned int modal : 1;
};

enum
{
  PROP_MODAL = 1,
  PROP_MESSAGE,
  PROP_DETAIL,
  PROP_BUTTONS,
  PROP_CANCEL_BUTTON,
  PROP_DEFAULT_BUTTON,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkAlertDialog, gtk_alert_dialog, G_TYPE_OBJECT)

static void
gtk_alert_dialog_init (GtkAlertDialog *self)
{
  self->modal = TRUE;
  self->cancel_button = -1;
  self->default_button = -1;
}

static void
gtk_alert_dialog_finalize (GObject *object)
{
  GtkAlertDialog *self = GTK_ALERT_DIALOG (object);

  g_free (self->message);
  g_free (self->detail);
  g_strfreev (self->buttons);

  G_OBJECT_CLASS (gtk_alert_dialog_parent_class)->finalize (object);
}

static void
gtk_alert_dialog_get_property (GObject      *object,
                              unsigned int  property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkAlertDialog *self = GTK_ALERT_DIALOG (object);

  switch (property_id)
    {
    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, self->message);
      break;

    case PROP_DETAIL:
      g_value_set_string (value, self->detail);
      break;

    case PROP_BUTTONS:
      g_value_set_boxed (value, self->buttons);
      break;

    case PROP_CANCEL_BUTTON:
      g_value_set_int (value, self->cancel_button);
      break;

    case PROP_DEFAULT_BUTTON:
      g_value_set_int (value, self->default_button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_alert_dialog_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkAlertDialog *self = GTK_ALERT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MODAL:
      gtk_alert_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_MESSAGE:
      gtk_alert_dialog_set_message (self, g_value_get_string (value));
      break;

    case PROP_DETAIL:
      gtk_alert_dialog_set_detail (self, g_value_get_string (value));
      break;

    case PROP_BUTTONS:
      gtk_alert_dialog_set_buttons (self, g_value_get_boxed (value));
      break;

    case PROP_CANCEL_BUTTON:
      gtk_alert_dialog_set_cancel_button (self, g_value_get_int (value));
      break;

    case PROP_DEFAULT_BUTTON:
      gtk_alert_dialog_set_default_button (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_alert_dialog_class_init (GtkAlertDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_alert_dialog_finalize;
  object_class->get_property = gtk_alert_dialog_get_property;
  object_class->set_property = gtk_alert_dialog_set_property;

  /**
   * GtkAlertDialog:modal: (attributes org.gtk.Property.get=gtk_alert_dialog_get_modal org.gtk.Property.set=gtk_alert_dialog_set_modal)
   *
   * Whether the alert is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAlertDialog:message: (attributes org.gtk.Property.get=gtk_alert_dialog_get_message org.gtk.Property.set=gtk_alert_dialog_set_message)
   *
   * The message for the alert.
   *
   * Since: 4.10
   */
  properties[PROP_MESSAGE] =
      g_param_spec_string ("message", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAlertDialog:detail: (attributes org.gtk.Property.get=gtk_alert_dialog_get_detail org.gtk.Property.set=gtk_alert_dialog_set_detail)
   *
   * The detail text for the alert.
   *
   * Since: 4.10
   */
  properties[PROP_DETAIL] =
      g_param_spec_string ("detail", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAlertDialog:buttons: (attributes org.gtk.Property.get=gtk_alert_dialog_get_buttons org.gtk.Property.set=gtk_alert_dialog_set_buttons)
   *
   * Labels for buttons to show in the alert.
   *
   * The labels should be translated and may contain
   * a _ to indicate the mnemonic character.
   *
   * If this property is not set, then a 'Close' button is
   * automatically created.
   *
   * Since: 4.10
   */
  properties[PROP_BUTTONS] =
      g_param_spec_boxed ("buttons", NULL, NULL,
                          G_TYPE_STRV,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAlertDialog:cancel-button: (attributes org.gtk.Property.get=gtk_alert_dialog_get_cancel_button org.gtk.Property.set=gtk_alert_dialog_set_cancel_button)
   *
   * This property determines what happens when the Escape key is
   * pressed while the alert is shown.
   *
   * If this property holds the index of a button in [property@Gtk.AlertDialog:buttons],
   * then pressing Escape is treated as if that button was pressed. If it is -1
   * or not a valid index for the `buttons` array, then an error is returned.
   *
   * If `buttons` is `NULL`, then the automatically created 'Close' button
   * is treated as both cancel and default button, so 0 is returned.
   *
   * Since: 4.10
   */
  properties[PROP_CANCEL_BUTTON] =
      g_param_spec_int ("cancel-button", NULL, NULL,
                        -1, G_MAXINT, -1,
                        G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAlertDialog:default-button: (attributes org.gtk.Property.get=gtk_alert_dialog_get_default_button org.gtk.Property.set=gtk_alert_dialog_set_default_button)
   *
   * This property determines what happens when the Return key is
   * pressed while the alert is shown.
   *
   * If this property holds the index of a button in [property@Gtk.AlertDialog:buttons],
   * then pressing Return is treated as if that button was pressed. If it is -1
   * or not a valid index for the `buttons` array, then nothing happens.
   *
   * If `buttons` is `NULL`, then the automatically created 'Close' button
   * is treated as both cancel and default button, so 0 is returned.
   *
   * Since: 4.10
   */
  properties[PROP_DEFAULT_BUTTON] =
      g_param_spec_int ("default-button", NULL, NULL,
                        -1, G_MAXINT, -1,
                        G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_alert_dialog_new:
 * @format: printf()-style format string
 * @...: arguments for @format
 *
 * Creates a new `GtkAlertDialog` object.
 *
 * The message will be set to the formatted string
 * resulting from the arguments.
 *
 * Returns: the new `GtkAlertDialog`
 *
 * Since: 4.10
 */
GtkAlertDialog *
gtk_alert_dialog_new (const char *format,
                      ...)
{
  va_list args;
  char *message;
  GtkAlertDialog *dialog;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  dialog = g_object_new (GTK_TYPE_ALERT_DIALOG,
                         "message", message,
                         NULL);
  g_free (message);

  return dialog;
}

/* }}} */
/* {{{ API: Getters and setters */

/**
 * gtk_alert_dialog_get_modal:
 * @self: a `GtkAlertDialog`
 *
 * Returns whether the alert blocks interaction
 * with the parent window while it is presented.
 *
 * Returns: `TRUE` if the alert is modal
 *
 * Since: 4.10
 */
gboolean
gtk_alert_dialog_get_modal (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_alert_dialog_set_modal:
 * @self: a `GtkAlertDialog`
 * @modal: the new value
 *
 * Sets whether the alert blocks interaction
 * with the parent window while it is presented.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_modal (GtkAlertDialog *self,
                            gboolean        modal)
{
  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_alert_dialog_get_message:
 * @self: a `GtkAlertDialog`
 *
 * Returns the message that will be shown in the alert.
 *
 * Returns: the message
 *
 * Since: 4.10
 */
const char *
gtk_alert_dialog_get_message (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), NULL);

  return self->message;
}

/**
 * gtk_alert_dialog_set_message:
 * @self: a `GtkAlertDialog`
 * @message: the new message
 *
 * Sets the message that will be shown in the alert.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_message (GtkAlertDialog *self,
                              const char     *message)
{
  char *new_message;

  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));
  g_return_if_fail (message != NULL);

  if (g_strcmp0 (self->message, message) == 0)
    return;

  new_message = g_strdup (message);
  g_free (self->message);
  self->message = new_message;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MESSAGE]);
}

/**
 * gtk_alert_dialog_get_detail:
 * @self: a `GtkAlertDialog`
 *
 * Returns the detail text that will be shown in the alert.
 *
 * Returns: the detail text
 *
 * Since: 4.10
 */
const char *
gtk_alert_dialog_get_detail (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), NULL);

  return self->detail;
}

/**
 * gtk_alert_dialog_set_detail:
 * @self: a `GtkAlertDialog`
 * @detail: the new detail text
 *
 * Sets the detail text that will be shown in the alert.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_detail (GtkAlertDialog *self,
                             const char     *detail)
{
  char *new_detail;

  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));
  g_return_if_fail (detail != NULL);

  if (g_strcmp0 (self->detail, detail) == 0)
    return;

  new_detail = g_strdup (detail);
  g_free (self->detail);
  self->detail = new_detail;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DETAIL]);
}

/**
 * gtk_alert_dialog_get_buttons:
 * @self: a `GtkAlertDialog`
 *
 * Returns the button labels for the alert.
 *
 * Returns: (nullable) (transfer none) (array zero-terminated=1): the button labels
 *
 * Since: 4.10
 */
const char * const *
gtk_alert_dialog_get_buttons (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), NULL);

  return (const char * const *) self->buttons;
}

/**
 * gtk_alert_dialog_set_buttons:
 * @self: a `GtkAlertDialog`
 * @labels: (array zero-terminated=1): the new button labels
 *
 * Sets the button labels for the alert.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_buttons (GtkAlertDialog     *self,
                              const char * const *labels)
{
  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));
  g_return_if_fail (labels != NULL);

  g_strfreev (self->buttons);
  self->buttons = g_strdupv ((char **)labels);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUTTONS]);
}

/**
 * gtk_alert_dialog_get_cancel_button:
 * @self: a `GtkAlertDialog`
 *
 * Returns the index of the cancel button.
 *
 * Returns: the index of the cancel button, or -1
 *
 * Since: 4.10
 */
int
gtk_alert_dialog_get_cancel_button (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), -1);

  return self->cancel_button;
}

/**
 * gtk_alert_dialog_set_cancel_button:
 * @self: a `GtkAlertDialog`
 * @button: the new cancel button
 *
 * Sets the index of the cancel button.
 *
 * See [property@Gtk.AlertDialog:cancel-button] for
 * details of how this value is used.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_cancel_button (GtkAlertDialog *self,
                                    int             button)
{
  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));

  if (self->cancel_button == button)
    return;

  self->cancel_button = button;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CANCEL_BUTTON]);
}

/**
 * gtk_alert_dialog_get_default_button:
 * @self: a `GtkAlertDialog`
 *
 * Returns the index of the default button.
 *
 * Returns: the index of the default button, or -1
 *
 * Since: 4.10
 */
int
gtk_alert_dialog_get_default_button (GtkAlertDialog *self)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), -1);

  return self->default_button;
}

/**
 * gtk_alert_dialog_set_default_button:
 * @self: a `GtkAlertDialog`
 * @button: the new default button
 *
 * Sets the index of the default button.
 *
 * See [property@Gtk.AlertDialog:default-button] for
 * details of how this value is used.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_set_default_button (GtkAlertDialog *self,
                                     int             button)
{
  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));

  if (self->default_button == button)
    return;

  self->default_button = button;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_BUTTON]);
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

  if (response == GTK_RESPONSE_CLOSE)
    {
      g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by application");
    }
  else if (response >= 0)
    {
      g_task_return_int (task, response);
    }
  else
    {
      GtkAlertDialog *self = GTK_ALERT_DIALOG (g_task_get_source_object (task));

      if (self->cancel_return >= 0)
        g_task_return_int (task, self->cancel_return);
      else
        g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "Dismissed by user");
    }

  g_object_unref (task);
}

static void
dialog_response (GtkDialog *dialog,
                 int        response,
                 GTask     *task)
{
  response_cb (task, response);
}

static GtkWidget *
create_message_dialog (GtkAlertDialog *self,
                       GtkWindow      *parent)
{
  GtkWidget *window;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  window = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "transient-for", parent,
                         "destroy-with-parent", TRUE,
                         "modal", self->modal,
                         "text", self->message,
                         "secondary-text", self->detail,
                         NULL);

  if (self->buttons && self->buttons[0])
    {
      self->cancel_return = -1;
      for (int i = 0; self->buttons[i]; i++)
        {
          gtk_dialog_add_button (GTK_DIALOG (window), self->buttons[i], i);
          if (self->default_button == i)
            gtk_dialog_set_default_response (GTK_DIALOG (window), i);
          if (self->cancel_button == i)
            self->cancel_return = i;
        }
    }
  else
    {
      gtk_dialog_add_button (GTK_DIALOG (window), _("_Close"), 0);
      gtk_dialog_set_default_response (GTK_DIALOG (window), 0);
      self->cancel_return = 0;
    }
G_GNUC_END_IGNORE_DEPRECATIONS

  return window;
}

/* }}} */
/* {{{ Async API */

/**
 * gtk_alert_dialog_choose:
 * @self: a `GtkAlertDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (nullable) (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function shows the alert to the user.
 *
 * The @callback will be called when the alert is dismissed.
 * It should call [method@Gtk.AlertDialog.choose_finish]
 * to obtain the result.
 *
 * It is ok to pass `NULL` for the callback if the alert
 * does not have more than one button. A simpler API for
 * this case is [method@Gtk.AlertDialog.show].
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_choose (GtkAlertDialog      *self,
                         GtkWindow           *parent,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GtkWidget *window;
  GTask *task;

  g_return_if_fail (GTK_IS_ALERT_DIALOG (self));

  window = create_message_dialog (self, parent);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_alert_dialog_choose);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_alert_dialog_choose_finish:
 * @self: a `GtkAlertDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.AlertDialog.choose] call
 * and returns the index of the button that was clicked.
 *
 * Returns: the index of the button that was clicked, or -1 if
 *   the dialog was cancelled and [property@Gtk.AlertDialog:cancel-button]
 *   is not set
 *
 * Since: 4.10
 */
int
gtk_alert_dialog_choose_finish (GtkAlertDialog  *self,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_return_val_if_fail (GTK_IS_ALERT_DIALOG (self), -1);
  g_return_val_if_fail (g_task_is_valid (result, self), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_alert_dialog_choose, -1);

  /* Destroy the dialog window not to be bound to GTask lifecycle */
  g_task_set_task_data (G_TASK (result), NULL, NULL);

  return (int) g_task_propagate_int (G_TASK (result), error);
}

/**
 * gtk_alert_dialog_show:
 * @self: a `GtkAlertDialog`
 * @parent: (nullable): the parent `GtkWindow`
 *
 * Show the alert to the user.
 *
 * This function is a simple version of [method@Gtk.AlertDialog.choose]
 * intended for dialogs with a single button.
 * If you want to cancel the dialog or if the alert has more than one button,
 * you should use that function instead and provide it with a #GCancellable or
 * callback respectively.
 *
 * Since: 4.10
 */
void
gtk_alert_dialog_show (GtkAlertDialog *self,
                       GtkWindow      *parent)
{
  gtk_alert_dialog_choose (self, parent, NULL, NULL, NULL);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
