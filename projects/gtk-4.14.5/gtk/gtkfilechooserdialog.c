/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserdialog.c: File selector dialog
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

#include "deprecated/gtkfilechooserdialog.h"

#include "gtkfilechooserprivate.h"
#include "deprecated/gtkfilechooserwidget.h"
#include "gtkfilechooserwidgetprivate.h"
#include "gtkfilechooserutils.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include <glib/gi18n-lib.h>
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkheaderbar.h"
#include "deprecated/gtkdialogprivate.h"
#include "gtklabel.h"
#include "gtkfilechooserentry.h"
#include "gtkbox.h"

#include <stdarg.h>


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkFileChooserDialog:
 *
 * `GtkFileChooserDialog` is a dialog suitable for use with
 * “File Open” or “File Save” commands.
 *
 * ![An example GtkFileChooserDialog](filechooser.png)
 *
 * This widget works by putting a [class@Gtk.FileChooserWidget]
 * inside a [class@Gtk.Dialog]. It exposes the [iface@Gtk.FileChooser]
 * interface, so you can use all of the [iface@Gtk.FileChooser] functions
 * on the file chooser dialog as well as those for [class@Gtk.Dialog].
 *
 * Note that `GtkFileChooserDialog` does not have any methods of its
 * own. Instead, you should use the functions that work on a
 * [iface@Gtk.FileChooser].
 *
 * If you want to integrate well with the platform you should use the
 * [class@Gtk.FileChooserNative] API, which will use a platform-specific
 * dialog if available and fall back to `GtkFileChooserDialog`
 * otherwise.
 *
 * ## Typical usage
 *
 * In the simplest of cases, you can the following code to use
 * `GtkFileChooserDialog` to select a file for opening:
 *
 * ```c
 * static void
 * on_open_response (GtkDialog *dialog,
 *                   int        response)
 * {
 *   if (response == GTK_RESPONSE_ACCEPT)
 *     {
 *       GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
 *
 *       g_autoptr(GFile) file = gtk_file_chooser_get_file (chooser);
 *
 *       open_file (file);
 *     }
 *
 *   gtk_window_destroy (GTK_WINDOW (dialog));
 * }
 *
 *   // ...
 *   GtkWidget *dialog;
 *   GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
 *
 *   dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                         parent_window,
 *                                         action,
 *                                         _("_Cancel"),
 *                                         GTK_RESPONSE_CANCEL,
 *                                         _("_Open"),
 *                                         GTK_RESPONSE_ACCEPT,
 *                                         NULL);
 *
 *   gtk_window_present (GTK_WINDOW (dialog));
 *
 *   g_signal_connect (dialog, "response",
 *                     G_CALLBACK (on_open_response),
 *                     NULL);
 * ```
 *
 * To use a dialog for saving, you can use this:
 *
 * ```c
 * static void
 * on_save_response (GtkDialog *dialog,
 *                   int        response)
 * {
 *   if (response == GTK_RESPONSE_ACCEPT)
 *     {
 *       GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
 *
 *       g_autoptr(GFile) file = gtk_file_chooser_get_file (chooser);
 *
 *       save_to_file (file);
 *     }
 *
 *   gtk_window_destroy (GTK_WINDOW (dialog));
 * }
 *
 *   // ...
 *   GtkWidget *dialog;
 *   GtkFileChooser *chooser;
 *   GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
 *
 *   dialog = gtk_file_chooser_dialog_new ("Save File",
 *                                         parent_window,
 *                                         action,
 *                                         _("_Cancel"),
 *                                         GTK_RESPONSE_CANCEL,
 *                                         _("_Save"),
 *                                         GTK_RESPONSE_ACCEPT,
 *                                         NULL);
 *   chooser = GTK_FILE_CHOOSER (dialog);
 *
 *   if (user_edited_a_new_document)
 *     gtk_file_chooser_set_current_name (chooser, _("Untitled document"));
 *   else
 *     gtk_file_chooser_set_file (chooser, existing_filename);
 *
 *   gtk_window_present (GTK_WINDOW (dialog));
 *
 *   g_signal_connect (dialog, "response",
 *                     G_CALLBACK (on_save_response),
 *                     NULL);
 * ```
 *
 * ## Setting up a file chooser dialog
 *
 * There are various cases in which you may need to use a `GtkFileChooserDialog`:
 *
 * - To select a file for opening, use %GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * - To save a file for the first time, use %GTK_FILE_CHOOSER_ACTION_SAVE,
 *   and suggest a name such as “Untitled” with
 *   [method@Gtk.FileChooser.set_current_name].
 *
 * - To save a file under a different name, use %GTK_FILE_CHOOSER_ACTION_SAVE,
 *   and set the existing file with [method@Gtk.FileChooser.set_file].
 *
 * - To choose a folder instead of a filem use %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 *
 * In general, you should only cause the file chooser to show a specific
 * folder when it is appropriate to use [method@Gtk.FileChooser.set_file],
 * i.e. when you are doing a “Save As” command and you already have a file
 * saved somewhere.

 * ## Response Codes
 *
 * `GtkFileChooserDialog` inherits from [class@Gtk.Dialog], so buttons that
 * go in its action area have response codes such as %GTK_RESPONSE_ACCEPT and
 * %GTK_RESPONSE_CANCEL. For example, you could call
 * [ctor@Gtk.FileChooserDialog.new] as follows:
 *
 * ```c
 * GtkWidget *dialog;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       action,
 *                                       _("_Cancel"),
 *                                       GTK_RESPONSE_CANCEL,
 *                                       _("_Open"),
 *                                       GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 * ```
 *
 * This will create buttons for “Cancel” and “Open” that use predefined
 * response identifiers from [enum@Gtk.ResponseType].  For most dialog
 * boxes you can use your own custom response codes rather than the
 * ones in [enum@Gtk.ResponseType], but `GtkFileChooserDialog` assumes that
 * its “accept”-type action, e.g. an “Open” or “Save” button,
 * will have one of the following response codes:
 *
 * - %GTK_RESPONSE_ACCEPT
 * - %GTK_RESPONSE_OK
 * - %GTK_RESPONSE_YES
 * - %GTK_RESPONSE_APPLY
 *
 * This is because `GtkFileChooserDialog` must intercept responses and switch
 * to folders if appropriate, rather than letting the dialog terminate — the
 * implementation uses these known response codes to know which responses can
 * be blocked if appropriate.
 *
 * To summarize, make sure you use a predefined response code
 * when you use `GtkFileChooserDialog` to ensure proper operation.
 *
 * ## CSS nodes
 *
 * `GtkFileChooserDialog` has a single CSS node with the name `window` and style
 * class `.filechooser`.
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */

typedef struct _GtkFileChooserDialogPrivate GtkFileChooserDialogPrivate;
typedef struct _GtkFileChooserDialogClass   GtkFileChooserDialogClass;

struct _GtkFileChooserDialog
{
  GtkDialog parent_instance;
};

struct _GtkFileChooserDialogClass
{
  GtkDialogClass parent_class;
};

struct _GtkFileChooserDialogPrivate
{
  GtkWidget *widget;

  GtkSizeGroup *buttons;

  /* for use with GtkFileChooserEmbed */
  gboolean response_requested;
  gboolean search_setup;
  gboolean has_entry;
};

static void     gtk_file_chooser_dialog_dispose      (GObject               *object);
static void     gtk_file_chooser_dialog_set_property (GObject               *object,
                                                      guint                  prop_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_get_property (GObject               *object,
                                                      guint                  prop_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_notify       (GObject               *object,
                                                      GParamSpec            *pspec);

static void     gtk_file_chooser_dialog_realize      (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_map          (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_unmap        (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_size_allocate (GtkWidget            *widget,
                                                       int                   width,
                                                       int                   height,
                                                       int                    baseline);
static void     gtk_file_chooser_dialog_activate_response (GtkWidget        *widget,
                                                           const char       *action_name,
                                                           GVariant         *parameters);

static void response_cb (GtkDialog *dialog,
                         int        response_id);

static void setup_save_entry (GtkFileChooserDialog *dialog);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserDialog, gtk_file_chooser_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (GtkFileChooserDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
                                                _gtk_file_chooser_delegate_iface_init))

static void
gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_file_chooser_dialog_dispose;
  gobject_class->set_property = gtk_file_chooser_dialog_set_property;
  gobject_class->get_property = gtk_file_chooser_dialog_get_property;
  gobject_class->notify = gtk_file_chooser_dialog_notify;

  widget_class->realize = gtk_file_chooser_dialog_realize;
  widget_class->map = gtk_file_chooser_dialog_map;
  widget_class->unmap = gtk_file_chooser_dialog_unmap;
  widget_class->size_allocate = gtk_file_chooser_dialog_size_allocate;

  _gtk_file_chooser_install_properties (gobject_class);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkfilechooserdialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserDialog, widget);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserDialog, buttons);
  gtk_widget_class_bind_template_callback (widget_class, response_cb);

  /**
   * GtkFileChooserDialog|response.activate:
   *
   * Activate the default response of the dialog.
   */
  gtk_widget_class_install_action (widget_class, "response.activate", NULL, gtk_file_chooser_dialog_activate_response);
}

static void
gtk_file_chooser_dialog_init (GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);

  priv->response_requested = FALSE;

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (dialog),
                                  GTK_FILE_CHOOSER (priv->widget));
}

static GtkWidget *
get_accept_action_widget (GtkDialog *dialog,
                          gboolean   sensitive_only)
{
  int response[] = {
    GTK_RESPONSE_ACCEPT,
    GTK_RESPONSE_OK,
    GTK_RESPONSE_YES,
    GTK_RESPONSE_APPLY
  };
  int i;
  GtkWidget *widget;

  for (i = 0; i < G_N_ELEMENTS (response); i++)
    {
      widget = gtk_dialog_get_widget_for_response (dialog, response[i]);
      if (widget)
        {
          if (!sensitive_only)
            return widget;

          if (gtk_widget_is_sensitive (widget))
            return widget;
        }
    }

  return NULL;
}

static gboolean
is_accept_response_id (int response_id)
{
  return (response_id == GTK_RESPONSE_ACCEPT ||
          response_id == GTK_RESPONSE_OK ||
          response_id == GTK_RESPONSE_YES ||
          response_id == GTK_RESPONSE_APPLY);
}

static void
gtk_file_chooser_dialog_activate_response (GtkWidget  *widget,
                                           const char *action_name,
                                           GVariant   *parameters)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);
  GtkWidget *button;

  priv->response_requested = TRUE;

  button = get_accept_action_widget (GTK_DIALOG (dialog), TRUE);
  if (button)
    {
      gtk_widget_activate (button);
      return;
    }

  priv->response_requested = FALSE;
}

static void
gtk_file_chooser_dialog_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), GTK_TYPE_FILE_CHOOSER_DIALOG);

  G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->dispose (object);
}

static void
gtk_file_chooser_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)

{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (object));

  g_object_set_property (G_OBJECT (priv->widget), pspec->name, value);
}

static void
gtk_file_chooser_dialog_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (object));

  g_object_get_property (G_OBJECT (priv->widget), pspec->name, value);
}

static void
gtk_file_chooser_dialog_notify (GObject    *object,
                                GParamSpec *pspec)
{
  if (strcmp (pspec->name, "action") == 0)
    setup_save_entry (GTK_FILE_CHOOSER_DIALOG (object));

  if (G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->notify)
    G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->notify (object, pspec);
}

static void
add_button (GtkWidget *button, gpointer data)
{
  GtkFileChooserDialog *dialog = data;
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);

  if (GTK_IS_BUTTON (button))
    gtk_size_group_add_widget (priv->buttons, button);
}

static gboolean
translate_subtitle_to_visible (GBinding     *binding,
                               const GValue *from_value,
                               GValue       *to_value,
                               gpointer      user_data)
{
  const char *subtitle = g_value_get_string (from_value);

  g_value_set_boolean (to_value, subtitle != NULL);

  return TRUE;
}

static void
setup_search (GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);
  gboolean use_header;
  GtkWidget *child;

  if (priv->search_setup)
    return;

  priv->search_setup = TRUE;

  g_object_get (dialog, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
      GtkWidget *button;
      GtkWidget *header;
      GtkWidget *box;
      GtkWidget *label;

      button = gtk_toggle_button_new ();
      gtk_widget_set_focus_on_click (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_button_set_icon_name (GTK_BUTTON (button), "edit-find-symbolic");

      header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      g_object_bind_property (button, "active",
                              priv->widget, "search-mode",
                              G_BINDING_BIDIRECTIONAL);

      if (!priv->has_entry)
        {
          box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
          gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

          label = gtk_label_new (NULL);
          gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
          gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
          gtk_label_set_width_chars (GTK_LABEL (label), 5);
          gtk_widget_add_css_class (label, "title");
          gtk_widget_set_parent (label, box);

          g_object_bind_property (dialog, "title",
                                  label, "label",
                                  G_BINDING_SYNC_CREATE);

          label = gtk_label_new (NULL);
          gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
          gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
          gtk_widget_add_css_class (label, "subtitle");
          gtk_widget_set_parent (label, box);

          g_object_bind_property (priv->widget, "subtitle",
                                  label, "label",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property_full (priv->widget, "subtitle",
                                       label, "visible",
                                       G_BINDING_SYNC_CREATE,
                                       translate_subtitle_to_visible,
                                       NULL, NULL, NULL);

          gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), box);
        }

      for (child = gtk_widget_get_first_child (header);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        add_button (child, dialog);
    }
}

static void
setup_save_entry (GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);
  gboolean use_header;
  GtkFileChooserAction action;
  gboolean need_entry;
  GtkWidget *header;

  g_object_get (dialog,
                "use-header-bar", &use_header,
                "action", &action,
                NULL);

  if (!use_header)
    return;

  header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));

  need_entry = action == GTK_FILE_CHOOSER_ACTION_SAVE;

  if (need_entry && !priv->has_entry)
    {
      GtkWidget *box;
      GtkWidget *label;
      GtkWidget *entry;

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      label = gtk_label_new_with_mnemonic (_("_Name"));
      entry = _gtk_file_chooser_entry_new (FALSE, FALSE);
      g_object_set (label, "margin-start", 6, "margin-end", 6, NULL);
      g_object_set (entry, "margin-start", 6, "margin-end", 6, NULL);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
      gtk_box_append (GTK_BOX (box), label);
      gtk_box_append (GTK_BOX (box), entry);

      gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), box);
      gtk_file_chooser_widget_set_save_entry (GTK_FILE_CHOOSER_WIDGET (priv->widget), entry);
    }
  else if (!need_entry && priv->has_entry)
    {
      gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), NULL);
      gtk_file_chooser_widget_set_save_entry (GTK_FILE_CHOOSER_WIDGET (priv->widget), NULL);
    }

  priv->has_entry = need_entry;
}

static void
ensure_default_response (GtkFileChooserDialog *dialog)
{
  GtkWidget *widget;

  widget = get_accept_action_widget (GTK_DIALOG (dialog), TRUE);
  if (widget)
    gtk_window_set_default_widget (GTK_WINDOW (dialog), widget);
}

static void
gtk_file_chooser_dialog_realize (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GSettings *settings;
  int width, height;

  settings = _gtk_file_chooser_get_settings_for_widget (widget);
  g_settings_get (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", &width, &height);

  if (width != 0 && height != 0)
    gtk_window_set_default_size (GTK_WINDOW (dialog), width, height);

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->realize (widget);
}

static void
gtk_file_chooser_dialog_map (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (dialog);

  setup_search (dialog);
  setup_save_entry (dialog);
  ensure_default_response (dialog);

  gtk_file_chooser_widget_initial_focus (GTK_FILE_CHOOSER_WIDGET (priv->widget));

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->map (widget);
}

static void
save_dialog_geometry (GtkFileChooserDialog *dialog)
{
  GtkWindow *window;
  GSettings *settings;
  int old_width, old_height;
  int width, height;

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (dialog));

  window = GTK_WINDOW (dialog);

  gtk_window_get_default_size (window, &width, &height);

  g_settings_get (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", &old_width, &old_height);
  if (old_width != width || old_height != height)
    g_settings_set (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", width, height);

  g_settings_apply (settings);
}

static void
gtk_file_chooser_dialog_unmap (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);

  save_dialog_geometry (dialog);

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->unmap (widget);
}

static void
gtk_file_chooser_dialog_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->size_allocate (widget,
                                                                          width,
                                                                          height,
                                                                          baseline);
  if (gtk_widget_is_drawable (widget))
    save_dialog_geometry (GTK_FILE_CHOOSER_DIALOG (widget));
}

/* We do a signal connection here rather than overriding the method in
 * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
 * handler to be run *first*, regardless of whether the user installs response
 * handlers of his own.
 */
static void
response_cb (GtkDialog *dialog,
             int        response_id)
{
  GtkFileChooserDialogPrivate *priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (dialog));

  /* Act only on response IDs we recognize */
  if (is_accept_response_id (response_id) &&
      !priv->response_requested &&
      !gtk_file_chooser_widget_should_respond (GTK_FILE_CHOOSER_WIDGET (priv->widget)))
    {
      g_signal_stop_emission_by_name (dialog, "response");
    }

  priv->response_requested = FALSE;
}

static GtkWidget *
gtk_file_chooser_dialog_new_valist (const char           *title,
                                    GtkWindow            *parent,
                                    GtkFileChooserAction  action,
                                    const char           *first_button_text,
                                    va_list               varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  int response_id;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "title", title,
                         "action", action,
                         NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  while (button_text)
    {
      response_id = va_arg (varargs, int);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const char *);
    }

  return result;
}

/**
 * gtk_file_chooser_dialog_new:
 * @title: (nullable): Title of the dialog
 * @parent: (nullable): Transient parent of the dialog
 * @action: Open or save mode for the dialog
 * @first_button_text: (nullable): text to go in the first button
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new `GtkFileChooserDialog`.
 *
 * This function is analogous to [ctor@Gtk.Dialog.new_with_buttons].
 *
 * Returns: a new `GtkFileChooserDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.FileDialog] instead
 */
GtkWidget *
gtk_file_chooser_dialog_new (const char           *title,
                             GtkWindow            *parent,
                             GtkFileChooserAction  action,
                             const char           *first_button_text,
                             ...)
{
  GtkWidget *result;
  va_list varargs;

  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
                                               first_button_text,
                                               varargs);
  va_end (varargs);

  return result;
}
