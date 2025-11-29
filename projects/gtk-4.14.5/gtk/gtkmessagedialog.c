/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "deprecated/gtkmessagedialog.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "deprecated/gtkdialogprivate.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkMessageDialog:
 *
 * `GtkMessageDialog` presents a dialog with some message text.
 *
 * ![An example GtkMessageDialog](messagedialog.png)
 *
 * It’s simply a convenience widget; you could construct the equivalent of
 * `GtkMessageDialog` from `GtkDialog` without too much effort, but
 * `GtkMessageDialog` saves typing.
 *
 * The easiest way to do a modal message dialog is to use the %GTK_DIALOG_MODAL
 * flag, which will call [method@Gtk.Window.set_modal] internally. The dialog will
 * prevent interaction with the parent window until it's hidden or destroyed.
 * You can use the [signal@Gtk.Dialog::response] signal to know when the user
 * dismissed the dialog.
 *
 * An example for using a modal dialog:
 * ```c
 * GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL;
 * dialog = gtk_message_dialog_new (parent_window,
 *                                  flags,
 *                                  GTK_MESSAGE_ERROR,
 *                                  GTK_BUTTONS_CLOSE,
 *                                  "Error reading “%s”: %s",
 *                                  filename,
 *                                  g_strerror (errno));
 * // Destroy the dialog when the user responds to it
 * // (e.g. clicks a button)
 *
 * g_signal_connect (dialog, "response",
 *                   G_CALLBACK (gtk_window_destroy),
 *                   NULL);
 * ```
 *
 * You might do a non-modal `GtkMessageDialog` simply by omitting the
 * %GTK_DIALOG_MODAL flag:
 *
 * ```c
 * GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 * dialog = gtk_message_dialog_new (parent_window,
 *                                  flags,
 *                                  GTK_MESSAGE_ERROR,
 *                                  GTK_BUTTONS_CLOSE,
 *                                  "Error reading “%s”: %s",
 *                                  filename,
 *                                  g_strerror (errno));
 *
 * // Destroy the dialog when the user responds to it
 * // (e.g. clicks a button)
 * g_signal_connect (dialog, "response",
 *                   G_CALLBACK (gtk_window_destroy),
 *                   NULL);
 * ```
 *
 * # GtkMessageDialog as GtkBuildable
 *
 * The `GtkMessageDialog` implementation of the `GtkBuildable` interface exposes
 * the message area as an internal child with the name “message_area”.
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */

typedef struct
{
  GtkWidget     *label;
  GtkWidget     *message_area; /* vbox for the primary and secondary labels, and any extra content from the caller */
  GtkWidget     *secondary_label;

  guint          has_primary_markup : 1;
  guint          has_secondary_text : 1;
  guint          message_type       : 3;
} GtkMessageDialogPrivate;

struct _GtkMessageDialogClass
{
  GtkDialogClass parent_class;
};

enum {
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_BUTTONS,
  PROP_TEXT,
  PROP_USE_MARKUP,
  PROP_SECONDARY_TEXT,
  PROP_SECONDARY_USE_MARKUP,
  PROP_IMAGE,
  PROP_MESSAGE_AREA
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkMessageDialog, gtk_message_dialog, GTK_TYPE_DIALOG)

static void
setup_type (GtkMessageDialog *dialog,
            GtkMessageType    type)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (dialog);

  if (priv->message_type == type)
    return;

  priv->message_type = type;

  g_object_notify (G_OBJECT (dialog), "message-type");
}

static void
gtk_message_dialog_add_buttons (GtkMessageDialog *message_dialog,
                                GtkButtonsType    buttons)
{
  GtkDialog* dialog = GTK_DIALOG (message_dialog);

  switch (buttons)
    {
    case GTK_BUTTONS_NONE:
      /* nothing */
      break;

    case GTK_BUTTONS_OK:
      gtk_dialog_add_button (dialog, _("_OK"), GTK_RESPONSE_OK);
      break;

    case GTK_BUTTONS_CLOSE:
      gtk_dialog_add_button (dialog, _("_Close"), GTK_RESPONSE_CLOSE);
      break;

    case GTK_BUTTONS_CANCEL:
      gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);
      break;

    case GTK_BUTTONS_YES_NO:
      gtk_dialog_add_button (dialog, _("_No"), GTK_RESPONSE_NO);
      gtk_dialog_add_button (dialog, _("_Yes"), GTK_RESPONSE_YES);
      break;

    case GTK_BUTTONS_OK_CANCEL:
      gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (dialog, _("_OK"), GTK_RESPONSE_OK);
      break;

    default:
      g_warning ("Unknown GtkButtonsType");
      break;
    }

  g_object_notify (G_OBJECT (message_dialog), "buttons");
}

static void
gtk_message_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (dialog);

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      setup_type (dialog, g_value_get_enum (value));
      break;
    case PROP_BUTTONS:
      gtk_message_dialog_add_buttons (dialog, g_value_get_enum (value));
      break;
    case PROP_TEXT:
      if (priv->has_primary_markup)
        gtk_label_set_markup (GTK_LABEL (priv->label), g_value_get_string (value));
      else
        gtk_label_set_text (GTK_LABEL (priv->label), g_value_get_string (value));
      break;
    case PROP_USE_MARKUP:
      if (priv->has_primary_markup != g_value_get_boolean (value))
        {
          priv->has_primary_markup = g_value_get_boolean (value);
          gtk_label_set_use_markup (GTK_LABEL (priv->label), priv->has_primary_markup);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SECONDARY_TEXT:
      {
        const char *txt = g_value_get_string (value);

        if (gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)))
          gtk_label_set_markup (GTK_LABEL (priv->secondary_label), txt);
        else
          gtk_label_set_text (GTK_LABEL (priv->secondary_label), txt);

        if (txt)
          {
            priv->has_secondary_text = TRUE;
            gtk_widget_add_css_class (priv->label, "title");
          }
        else
          {
            priv->has_secondary_text = FALSE;
            gtk_widget_remove_css_class (priv->label, "title");
          }

        gtk_widget_set_visible (priv->secondary_label, priv->has_secondary_text);
      }
      break;
    case PROP_SECONDARY_USE_MARKUP:
      if (gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)) != g_value_get_boolean (value))
        {
          gtk_label_set_use_markup (GTK_LABEL (priv->secondary_label), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_message_dialog_get_property (GObject     *object,
                                 guint        prop_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (dialog);

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, (GtkMessageType) priv->message_type);
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->label)));
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->has_primary_markup);
      break;
    case PROP_SECONDARY_TEXT:
      if (priv->has_secondary_text)
      g_value_set_string (value,
                          gtk_label_get_label (GTK_LABEL (priv->secondary_label)));
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_SECONDARY_USE_MARKUP:
      if (priv->has_secondary_text)
        g_value_set_boolean (value,
                             gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)));
      else
        g_value_set_boolean (value, FALSE);
      break;
    case PROP_MESSAGE_AREA:
      g_value_set_object (value, priv->message_area);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_title (GObject    *dialog,
              GParamSpec *pspec,
              GtkWidget  *label)
{
  const char *title;

  title = gtk_window_get_title (GTK_WINDOW (dialog));
  gtk_label_set_label (GTK_LABEL (label), title);
  gtk_widget_set_visible (label, title && title[0]);
}

static void
gtk_message_dialog_constructed (GObject *object)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  gboolean use_header;

  G_OBJECT_CLASS (gtk_message_dialog_parent_class)->constructed (object);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (dialog)),
                "gtk-dialogs-use-header", &use_header,
                NULL);

  if (use_header)
    {
      GtkWidget *box;
      GtkWidget *label;

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request (box, -1, 16);
      label = gtk_label_new ("");
      gtk_widget_set_visible (label, FALSE);
      gtk_widget_set_margin_top (label, 6);
      gtk_widget_set_margin_bottom (label, 6);
      gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_add_css_class (label, "title");
      gtk_box_append (GTK_BOX (box), label);
      g_signal_connect_object (dialog, "notify::title", G_CALLBACK (update_title), label, 0);

      gtk_window_set_titlebar (GTK_WINDOW (dialog), box);
    }
}

static void
gtk_message_dialog_class_init (GtkMessageDialogClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;

  widget_class = GTK_WIDGET_CLASS (class);
  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->constructed = gtk_message_dialog_constructed;
  gobject_class->set_property = gtk_message_dialog_set_property;
  gobject_class->get_property = gtk_message_dialog_get_property;

  /**
   * GtkMessageDialog:message-type:
   *
   * The type of the message.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message-type", NULL, NULL,
                                                      GTK_TYPE_MESSAGE_TYPE,
                                                      GTK_MESSAGE_INFO,
                                                      GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_BUTTONS,
                                   g_param_spec_enum ("buttons", NULL, NULL,
                                                      GTK_TYPE_BUTTONS_TYPE,
                                                      GTK_BUTTONS_NONE,
                                                      GTK_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
  /**
   * GtkMessageDialog:text:
   *
   * The primary text of the message dialog.
   *
   * If the dialog has a secondary text, this will appear as the title.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text", NULL, NULL,
                                                        "",
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkMessageDialog:use-markup:
   *
   * %TRUE if the primary text of the dialog includes Pango markup.
   *
   * See [func@Pango.parse_markup].
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkMessageDialog:secondary-text:
   *
   * The secondary text of the message dialog.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SECONDARY_TEXT,
                                   g_param_spec_string ("secondary-text", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkMessageDialog:secondary-use-markup:
   *
   * %TRUE if the secondary text of the dialog includes Pango markup.
   *
   * See [func@Pango.parse_markup].
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SECONDARY_USE_MARKUP,
                                   g_param_spec_boolean ("secondary-use-markup", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkMessageDialog:message-area:
   *
   * The `GtkBox` that corresponds to the message area of this dialog.
   *
   * See [method@Gtk.MessageDialog.get_message_area] for a detailed
   * description of this area.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_AREA,
                                   g_param_spec_object ("message-area", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkmessagedialog.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageDialog, label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageDialog, secondary_label);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkMessageDialog, message_area);
}

static void
gtk_message_dialog_init (GtkMessageDialog *dialog)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (dialog);
  GtkWidget *action_area;
  GtkSettings *settings;
  gboolean use_caret;

  priv->has_primary_markup = FALSE;
  priv->has_secondary_text = FALSE;
  priv->message_type = GTK_MESSAGE_OTHER;

  gtk_widget_add_css_class (GTK_WIDGET (dialog), "message");

  gtk_widget_init_template (GTK_WIDGET (dialog));
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
  gtk_widget_set_halign (action_area, GTK_ALIGN_FILL);
  gtk_box_set_homogeneous (GTK_BOX (action_area), TRUE);

  settings = gtk_widget_get_settings (GTK_WIDGET (dialog));
  g_object_get (settings, "gtk-keynav-use-caret", &use_caret, NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->label), use_caret);
  gtk_label_set_selectable (GTK_LABEL (priv->secondary_label), use_caret);
}

/**
 * gtk_message_dialog_new:
 * @parent: (nullable): transient parent
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: (nullable): printf()-style format string
 * @...: arguments for @message_format
 *
 * Creates a new message dialog.
 *
 * This is a simple dialog with some text the user may want to see.
 * When the user clicks a button a “response” signal is emitted with
 * response IDs from [enum@Gtk.ResponseType]. See [class@Gtk.Dialog]
 * for more details.
 *
 * Returns: (transfer none): a new `GtkMessageDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */
GtkWidget*
gtk_message_dialog_new (GtkWindow     *parent,
                        GtkDialogFlags flags,
                        GtkMessageType type,
                        GtkButtonsType buttons,
                        const char    *message_format,
                        ...)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  char * msg = NULL;
  va_list args;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  widget = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "use-header-bar", FALSE,
                         "message-type", type,
                         "buttons", buttons,
                         NULL);
  dialog = GTK_DIALOG (widget);

  if (message_format)
    {
      GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private ((GtkMessageDialog*)dialog);
      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_text (GTK_LABEL (priv->label), msg);

      g_free (msg);
    }

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (parent));

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return widget;
}

/**
 * gtk_message_dialog_new_with_markup:
 * @parent: (nullable): transient parent
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: (nullable): printf()-style format string
 * @...: arguments for @message_format
 *
 * Creates a new message dialog.
 *
 * This is a simple dialog with some text that is marked up with
 * Pango markup. When the user clicks a button a “response” signal
 * is emitted with response IDs from [enum@Gtk.ResponseType]. See
 * [class@Gtk.Dialog] for more details.
 *
 * Special XML characters in the printf() arguments passed to this
 * function will automatically be escaped as necessary.
 * (See g_markup_printf_escaped() for how this is implemented.)
 * Usually this is what you want, but if you have an existing
 * Pango markup string that you want to use literally as the
 * label, then you need to use [method@Gtk.MessageDialog.set_markup]
 * instead, since you can’t pass the markup string either
 * as the format (it might contain “%” characters) or as a string
 * argument.
 *
 * ```c
 * GtkWidget *dialog;
 * GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 * dialog = gtk_message_dialog_new (parent_window,
 *                                  flags,
 *                                  GTK_MESSAGE_ERROR,
 *                                  GTK_BUTTONS_CLOSE,
 *                                  NULL);
 * gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
 *                                markup);
 * ```
 *
 * Returns: a new `GtkMessageDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 **/
GtkWidget*
gtk_message_dialog_new_with_markup (GtkWindow     *parent,
                                    GtkDialogFlags flags,
                                    GtkMessageType type,
                                    GtkButtonsType buttons,
                                    const char    *message_format,
                                    ...)
{
  GtkWidget *widget;
  va_list args;
  char *msg = NULL;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  widget = gtk_message_dialog_new (parent, flags, type, buttons, NULL);

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_markup_vprintf_escaped (message_format, args);
      va_end (args);

      gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (widget), msg);

      g_free (msg);
    }

  return widget;
}

/**
 * gtk_message_dialog_set_markup:
 * @message_dialog: a `GtkMessageDialog`
 * @str: string with Pango markup
 *
 * Sets the text of the message dialog.
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */
void
gtk_message_dialog_set_markup (GtkMessageDialog *message_dialog,
                               const char       *str)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (message_dialog);

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv->has_primary_markup = TRUE;
  gtk_label_set_markup (GTK_LABEL (priv->label), str);
}

/**
 * gtk_message_dialog_format_secondary_text:
 * @message_dialog: a `GtkMessageDialog`
 * @message_format: (nullable): printf()-style format string
 * @...: arguments for @message_format
 *
 * Sets the secondary text of the message dialog.
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */
void
gtk_message_dialog_format_secondary_text (GtkMessageDialog *message_dialog,
                                          const char       *message_format,
                                          ...)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (message_dialog);
  va_list args;
  char *msg = NULL;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  if (message_format)
    {
      priv->has_secondary_text = TRUE;
      gtk_widget_add_css_class (priv->label, "title");

      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_text (GTK_LABEL (priv->secondary_label), msg);

      g_free (msg);
    }
  else
    {
      priv->has_secondary_text = FALSE;
      gtk_widget_remove_css_class (priv->label, "title");
    }

  gtk_widget_set_visible (priv->secondary_label, priv->has_secondary_text);
}

/**
 * gtk_message_dialog_format_secondary_markup:
 * @message_dialog: a `GtkMessageDialog`
 * @message_format: printf()-style string with Pango markup
 * @...: arguments for @message_format
 *
 * Sets the secondary text of the message dialog.
 *
 * The @message_format is assumed to contain Pango markup.
 *
 * Due to an oversight, this function does not escape special
 * XML characters like [ctor@Gtk.MessageDialog.new_with_markup]
 * does. Thus, if the arguments may contain special XML characters,
 * you should use g_markup_printf_escaped() to escape it.
 *
 * ```c
 * char *msg;
 *
 * msg = g_markup_printf_escaped (message_format, ...);
 * gtk_message_dialog_format_secondary_markup (message_dialog,
 *                                             "%s", msg);
 * g_free (msg);
 * ```
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */
void
gtk_message_dialog_format_secondary_markup (GtkMessageDialog *message_dialog,
                                            const char       *message_format,
                                            ...)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (message_dialog);
  va_list args;
  char *msg = NULL;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  if (message_format)
    {
      priv->has_secondary_text = TRUE;
      gtk_widget_add_css_class (priv->label, "title");

      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_markup (GTK_LABEL (priv->secondary_label), msg);

      g_free (msg);
    }
  else
    {
      priv->has_secondary_text = FALSE;
      gtk_widget_remove_css_class (priv->label, "title");
    }

  gtk_widget_set_visible (priv->secondary_label, priv->has_secondary_text);
}

/**
 * gtk_message_dialog_get_message_area:
 * @message_dialog: a `GtkMessageDialog`
 *
 * Returns the message area of the dialog.
 *
 * This is the box where the dialog’s primary and secondary labels
 * are packed. You can add your own extra content to that box and it
 * will appear below those labels. See [method@Gtk.Dialog.get_content_area]
 * for the corresponding function in the parent [class@Gtk.Dialog].
 *
 * Returns: (transfer none): A `GtkBox` corresponding to the
 *   “message area” in the @message_dialog
 *
 * Deprecated: 4.10: Use [class@Gtk.AlertDialog] instead
 */
GtkWidget *
gtk_message_dialog_get_message_area (GtkMessageDialog *message_dialog)
{
  GtkMessageDialogPrivate *priv = gtk_message_dialog_get_instance_private (message_dialog);

  g_return_val_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog), NULL);

  return priv->message_area;
}
