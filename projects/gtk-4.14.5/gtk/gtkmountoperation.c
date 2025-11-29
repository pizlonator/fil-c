/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) Christian Kellner <gicmo@gnome.org>
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <errno.h>
#include <string.h>

#include "gtkmountoperationprivate.h"
#include "gtkbox.h"
#include "gtkdbusgenerated.h"
#include "gtkentry.h"
#include "gtkbox.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkalertdialog.h"
#include "gtkmountoperation.h"
#include "gtkprivate.h"
#include "gtkcheckbutton.h"
#include "gtkgrid.h"
#include "gtkwindow.h"
#include "gtkscrolledwindow.h"
#include "gtkicontheme.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "deprecated/gtkdialogprivate.h"
#include "gtkpopover.h"
#include "gtksnapshot.h"
#include "gdktextureprivate.h"
#include <glib/gprintf.h>
#include "gtklistview.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtksingleselection.h"
#include "gtkpicture.h"


/**
 * GtkMountOperation:
 *
 * `GtkMountOperation` is an implementation of `GMountOperation`.
 *
 * The functions and objects described here make working with GTK and
 * GIO more convenient.
 *
 * `GtkMountOperation` is needed when mounting volumes:
 * It is an implementation of `GMountOperation` that can be used with
 * GIO functions for mounting volumes such as
 * g_file_mount_enclosing_volume(), g_file_mount_mountable(),
 * g_volume_mount(), g_mount_unmount_with_operation() and others.
 *
 * When necessary, `GtkMountOperation` shows dialogs to let the user
 * enter passwords, ask questions or show processes blocking unmount.
 */

static void   gtk_mount_operation_finalize     (GObject          *object);
static void   gtk_mount_operation_set_property (GObject          *object,
                                                guint             prop_id,
                                                const GValue     *value,
                                                GParamSpec       *pspec);
static void   gtk_mount_operation_get_property (GObject          *object,
                                                guint             prop_id,
                                                GValue           *value,
                                                GParamSpec       *pspec);

static void   gtk_mount_operation_ask_password (GMountOperation *op,
                                                const char      *message,
                                                const char      *default_user,
                                                const char      *default_domain,
                                                GAskPasswordFlags flags);

static void   gtk_mount_operation_ask_question (GMountOperation *op,
                                                const char      *message,
                                                const char      *choices[]);

static void   gtk_mount_operation_show_processes (GMountOperation *op,
                                                  const char      *message,
                                                  GArray          *processes,
                                                  const char      *choices[]);

static void   gtk_mount_operation_aborted      (GMountOperation *op);

struct _GtkMountOperationPrivate {
  GtkWindow *parent_window;
  GtkDialog *dialog;
  GdkDisplay *display;

  /* bus proxy */
  _GtkMountOperationHandler *handler;
  GCancellable *cancellable;
  gboolean handler_showing;

  /* for the ask-password dialog */
  GtkWidget *grid;
  GtkWidget *username_entry;
  GtkWidget *domain_entry;
  GtkWidget *password_entry;
  GtkWidget *pim_entry;
  GtkWidget *anonymous_toggle;
  GtkWidget *tcrypt_hidden_toggle;
  GtkWidget *tcrypt_system_toggle;
  GList *user_widgets;

  GAskPasswordFlags ask_flags;
  GPasswordSave     password_save;
  gboolean          anonymous;

  /* for the show-processes dialog */
  GtkWidget *process_list_view;
  GListStore *process_list_store;
};

enum {
  PROP_0,
  PROP_PARENT,
  PROP_IS_SHOWING,
  PROP_DISPLAY

};

G_DEFINE_TYPE_WITH_PRIVATE (GtkMountOperation, gtk_mount_operation, G_TYPE_MOUNT_OPERATION)

static void
gtk_mount_operation_class_init (GtkMountOperationClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GMountOperationClass *mount_op_class = G_MOUNT_OPERATION_CLASS (klass);

  object_class->finalize     = gtk_mount_operation_finalize;
  object_class->get_property = gtk_mount_operation_get_property;
  object_class->set_property = gtk_mount_operation_set_property;

  mount_op_class->ask_password = gtk_mount_operation_ask_password;
  mount_op_class->ask_question = gtk_mount_operation_ask_question;
  mount_op_class->show_processes = gtk_mount_operation_show_processes;
  mount_op_class->aborted = gtk_mount_operation_aborted;

  /**
   * GtkMountOperation:parent: (attributes org.gtk.Property.get=gtk_mount_operation_get_parent org.gtk.Property.set=gtk_mount_operation_set_parent)
   *
   * The parent window.
   */
  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent", NULL, NULL,
                                                        GTK_TYPE_WINDOW,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMountOperation:is-showing: (attributes org.gtk.Property.get=gtk_mount_operation_is_showing)
   *
   * Whether a dialog is currently shown.
   */
  g_object_class_install_property (object_class,
                                   PROP_IS_SHOWING,
                                   g_param_spec_boolean ("is-showing", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READABLE));

  /**
   * GtkMountOperation:display: (attributes org.gtk.Property.get=gtk_mount_operation_get_display org.gtk.Property.set=gtk_mount_operation_set_display)
   *
   * The display where dialogs will be shown.
   */
  g_object_class_install_property (object_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display", NULL, NULL,
                                                        GDK_TYPE_DISPLAY,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_mount_operation_init (GtkMountOperation *operation)
{
  char *name_owner;

  operation->priv = gtk_mount_operation_get_instance_private (operation);

  operation->priv->handler =
    _gtk_mount_operation_handler_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                         "org.gtk.MountOperationHandler",
                                                         "/org/gtk/MountOperationHandler",
                                                         NULL, NULL);
  if (!operation->priv->handler)
    return;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (operation->priv->handler));
  if (!name_owner)
    g_clear_object (&operation->priv->handler);
  g_free (name_owner);

  if (operation->priv->handler)
    g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (operation->priv->handler), G_MAXINT);
}

static void
parent_destroyed (GtkWidget  *parent,
                  gpointer  **pointer)
{
  *pointer = NULL;
}

static void
gtk_mount_operation_finalize (GObject *object)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);
  GtkMountOperationPrivate *priv = operation->priv;

  if (priv->user_widgets)
    g_list_free (priv->user_widgets);

  if (priv->parent_window)
    {
      g_signal_handlers_disconnect_by_func (priv->parent_window,
                                            parent_destroyed,
                                            &priv->parent_window);
      g_object_unref (priv->parent_window);
    }

  if (priv->display)
    g_object_unref (priv->display);

  if (priv->handler)
    g_object_unref (priv->handler);

  G_OBJECT_CLASS (gtk_mount_operation_parent_class)->finalize (object);
}

static void
gtk_mount_operation_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      gtk_mount_operation_set_parent (operation, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      gtk_mount_operation_set_display (operation, g_value_get_object (value));
      break;

    case PROP_IS_SHOWING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_mount_operation_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);
  GtkMountOperationPrivate *priv = operation->priv;

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, priv->parent_window);
      break;

    case PROP_IS_SHOWING:
      g_value_set_boolean (value, priv->dialog != NULL || priv->handler_showing);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_mount_operation_proxy_finish (GtkMountOperation     *op,
                                  GMountOperationResult  result)
{
  _gtk_mount_operation_handler_call_close (op->priv->handler, NULL, NULL, NULL);

  op->priv->handler_showing = FALSE;
  g_object_notify (G_OBJECT (op), "is-showing");

  g_mount_operation_reply (G_MOUNT_OPERATION (op), result);

  /* drop the reference acquired when calling the proxy method */
  g_object_unref (op);
}

static void
remember_button_toggled (GtkCheckButton    *button,
                         GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;

  if (gtk_check_button_get_active (button))
    {
      gpointer data;

      data = g_object_get_data (G_OBJECT (button), "password-save");
      priv->password_save = GPOINTER_TO_INT (data);
    }
}

static void
pw_dialog_got_response (GtkDialog         *dialog,
                        int                response_id,
                        GtkMountOperation *mount_op)
{
  GtkMountOperationPrivate *priv = mount_op->priv;
  GMountOperation *op = G_MOUNT_OPERATION (mount_op);

  if (response_id == GTK_RESPONSE_OK)
    {
      const char *text;

      if (priv->ask_flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED)
        g_mount_operation_set_anonymous (op, priv->anonymous);

      if (priv->username_entry)
        {
          text = gtk_editable_get_text (GTK_EDITABLE (priv->username_entry));
          g_mount_operation_set_username (op, text);
        }

      if (priv->domain_entry)
        {
          text = gtk_editable_get_text (GTK_EDITABLE (priv->domain_entry));
          g_mount_operation_set_domain (op, text);
        }

      if (priv->password_entry)
        {
          text = gtk_editable_get_text (GTK_EDITABLE (priv->password_entry));
          g_mount_operation_set_password (op, text);
        }

      if (priv->pim_entry)
        {
          text = gtk_editable_get_text (GTK_EDITABLE (priv->pim_entry));
          if (text && strlen (text) > 0)
            {
              guint64 pim;
              char *end = NULL;

              errno = 0;
              pim = g_ascii_strtoull (text, &end, 10);
              if (errno == 0 && pim <= G_MAXUINT && end != text)
                g_mount_operation_set_pim (op, (guint) pim);
            }
        }

      if (priv->tcrypt_hidden_toggle && gtk_check_button_get_active (GTK_CHECK_BUTTON (priv->tcrypt_hidden_toggle)))
        g_mount_operation_set_is_tcrypt_hidden_volume (op, TRUE);

      if (priv->tcrypt_system_toggle && gtk_check_button_get_active (GTK_CHECK_BUTTON (priv->tcrypt_system_toggle)))
        g_mount_operation_set_is_tcrypt_system_volume (op, TRUE);

      if (priv->ask_flags & G_ASK_PASSWORD_SAVING_SUPPORTED)
        g_mount_operation_set_password_save (op, priv->password_save);

      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  if (priv->user_widgets)
    g_list_free (priv->user_widgets);

  priv->user_widgets = NULL;
  priv->dialog = NULL;
  g_object_notify (G_OBJECT (op), "is-showing");
  gtk_window_destroy (GTK_WINDOW (dialog));
  g_object_unref (op);
}

static gboolean
entry_has_input (GtkWidget *entry_widget)
{
  const char *text;

  if (entry_widget == NULL)
    return TRUE;

  text = gtk_editable_get_text (GTK_EDITABLE (entry_widget));

  return text != NULL && text[0] != '\0';
}

static gboolean
pim_entry_is_valid (GtkWidget *entry_widget)
{
  const char *text;
  char *end = NULL;
  guint64 pim;

  if (entry_widget == NULL)
    return TRUE;

  text = gtk_editable_get_text (GTK_EDITABLE (entry_widget));
  /* An empty PIM entry is OK */
  if (text == NULL || text[0] == '\0')
    return TRUE;

  errno = 0;
  pim = g_ascii_strtoull (text, &end, 10);
  if (errno || pim > G_MAXUINT || end == text)
    return FALSE;
  else
    return TRUE;
}

static gboolean
pw_dialog_input_is_valid (GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid = TRUE;

  /* We don't require password to be non-empty here
   * since there are situations where it is not needed,
   * see bug 578365.
   * We may add a way for the backend to specify that it
   * definitively needs a password.
   */
  is_valid = entry_has_input (priv->username_entry) &&
             entry_has_input (priv->domain_entry) &&
             pim_entry_is_valid (priv->pim_entry);

  return is_valid;
}

static void
pw_dialog_verify_input (GtkEditable       *editable,
                        GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid;

  is_valid = pw_dialog_input_is_valid (operation);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_response_sensitive (GTK_DIALOG (priv->dialog),
                                     GTK_RESPONSE_OK,
                                     is_valid);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
pw_dialog_anonymous_toggled (GtkWidget         *widget,
                             GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid;
  GList *l;

  priv->anonymous = widget == priv->anonymous_toggle;

  if (priv->anonymous)
    is_valid = TRUE;
  else
    is_valid = pw_dialog_input_is_valid (operation);

  for (l = priv->user_widgets; l != NULL; l = l->next)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (l->data), !priv->anonymous);
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_response_sensitive (GTK_DIALOG (priv->dialog),
                                     GTK_RESPONSE_OK,
                                     is_valid);
G_GNUC_END_IGNORE_DEPRECATIONS
}


static void
pw_dialog_cycle_focus (GtkWidget         *widget,
                       GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv;
  GtkWidget *next_widget = NULL;

  priv = operation->priv;

  if (widget == priv->username_entry)
    {
      if (priv->domain_entry != NULL)
        next_widget = priv->domain_entry;
      else if (priv->password_entry != NULL)
        next_widget = priv->password_entry;
    }
  else if (widget == priv->domain_entry && priv->password_entry)
    next_widget = priv->password_entry;

  if (next_widget)
    gtk_widget_grab_focus (next_widget);
  else if (pw_dialog_input_is_valid (operation))
    gtk_widget_activate_default (widget);
}

static GtkWidget *
table_add_entry (GtkMountOperation *operation,
                 int                row,
                 const char        *label_text,
                 const char        *value,
                 gpointer           user_data)
{
  GtkWidget *entry;
  GtkWidget *label;

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (label, FALSE);
  operation->priv->user_widgets = g_list_prepend (operation->priv->user_widgets, label);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);

  if (value)
    gtk_editable_set_text (GTK_EDITABLE (entry), value);

  gtk_grid_attach (GTK_GRID (operation->priv->grid), label, 0, row, 1, 1);
  gtk_grid_attach (GTK_GRID (operation->priv->grid), entry, 1, row, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  operation->priv->user_widgets = g_list_prepend (operation->priv->user_widgets, entry);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (pw_dialog_verify_input), user_data);

  g_signal_connect (entry, "activate",
                    G_CALLBACK (pw_dialog_cycle_focus), user_data);

  return entry;
}

static void
gtk_mount_operation_ask_password_do_gtk (GtkMountOperation *operation,
                                         const char        *message,
                                         const char        *default_user,
                                         const char        *default_domain)
{
  GtkMountOperationPrivate *priv;
  GtkWidget *widget;
  GtkDialog *dialog;
  GtkWindow *window;
  GtkWidget *hbox, *main_vbox, *icon;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *content_area;
  gboolean   can_anonymous;
  guint      rows;
  char *primary;
  const char *secondary = NULL;
  gboolean use_header;

  priv = operation->priv;

  g_object_get (gtk_settings_get_default (),
                "gtk-dialogs-use-header", &use_header,
                NULL);
  widget = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", use_header,
                         NULL);
  dialog = GTK_DIALOG (widget);
  window = GTK_WINDOW (widget);

  priv->dialog = dialog;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  content_area = gtk_dialog_get_content_area (dialog);

  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_title (window, "");
  gtk_window_set_icon_name (window, "dialog-password");

  gtk_dialog_add_buttons (dialog,
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("Co_nnect"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);
G_GNUC_END_IGNORE_DEPRECATIONS


  /* Build contents */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  g_object_set (hbox,
                "margin-start", 12,
                "margin-end", 12,
                "margin-top", 12,
                "margin-bottom", 12,
                NULL);
  gtk_box_append (GTK_BOX (content_area), hbox);

  icon = gtk_image_new_from_icon_name ("dialog-password");
  gtk_image_set_icon_size (GTK_IMAGE (icon), GTK_ICON_SIZE_LARGE);

  gtk_widget_set_halign (icon, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (icon, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (hbox), icon);

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
  gtk_box_append (GTK_BOX (hbox), main_vbox);

  primary = strstr (message, "\n");
  if (primary)
    {
      secondary = primary + 1;
      primary = g_strndup (message, primary - message);
    }

  label = gtk_label_new (primary != NULL ? primary : message);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_box_append (GTK_BOX (main_vbox), GTK_WIDGET (label));
  g_free (primary);
  gtk_widget_add_css_class (label, "title-3");

  if (secondary != NULL)
    {
      label = gtk_label_new (secondary);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_box_append (GTK_BOX (main_vbox), GTK_WIDGET (label));
    }

  grid = gtk_grid_new ();
  operation->priv->grid = grid;
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_box_append (GTK_BOX (main_vbox), grid);

  can_anonymous = priv->ask_flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED;

  rows = 0;

  priv->anonymous_toggle = NULL;
  if (can_anonymous)
    {
      GtkWidget *anon_box;
      GtkWidget *choice;

      label = gtk_label_new (_("Connect As"));
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_widget_set_hexpand (label, FALSE);
      gtk_grid_attach (GTK_GRID (grid), label, 0, rows, 1, 1);

      anon_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_grid_attach (GTK_GRID (grid), anon_box, 1, rows++, 1, 1);

      choice = gtk_check_button_new_with_mnemonic (_("_Anonymous"));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (choice), TRUE);
      gtk_box_append (GTK_BOX (anon_box), choice);
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (pw_dialog_anonymous_toggled), operation);
      priv->anonymous_toggle = choice;

      choice = gtk_check_button_new_with_mnemonic (_("Registered U_ser"));
      gtk_check_button_set_group (GTK_CHECK_BUTTON (choice), GTK_CHECK_BUTTON (priv->anonymous_toggle));
      gtk_box_append (GTK_BOX (anon_box), choice);
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (pw_dialog_anonymous_toggled), operation);
    }

  priv->username_entry = NULL;

  if (priv->ask_flags & G_ASK_PASSWORD_NEED_USERNAME)
    priv->username_entry = table_add_entry (operation, rows++, _("_Username"),
                                            default_user, operation);

  priv->domain_entry = NULL;
  if (priv->ask_flags & G_ASK_PASSWORD_NEED_DOMAIN)
    priv->domain_entry = table_add_entry (operation, rows++, _("_Domain"),
                                          default_domain, operation);

  priv->pim_entry = NULL;
  if (priv->ask_flags & G_ASK_PASSWORD_TCRYPT)
    {
      GtkWidget *volume_type_label;
      GtkWidget *volume_type_box;

      volume_type_label = gtk_label_new (_("Volume type"));
      gtk_widget_set_halign (volume_type_label, GTK_ALIGN_END);
      gtk_widget_set_hexpand (volume_type_label, FALSE);
      gtk_grid_attach (GTK_GRID (grid), volume_type_label, 0, rows, 1, 1);
      priv->user_widgets = g_list_prepend (priv->user_widgets, volume_type_label);

      volume_type_box =  gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_grid_attach (GTK_GRID (grid), volume_type_box, 1, rows++, 1, 1);
      priv->user_widgets = g_list_prepend (priv->user_widgets, volume_type_box);

      priv->tcrypt_hidden_toggle = gtk_check_button_new_with_mnemonic (_("_Hidden"));
      gtk_box_append (GTK_BOX (volume_type_box), priv->tcrypt_hidden_toggle);

      priv->tcrypt_system_toggle = gtk_check_button_new_with_mnemonic (_("_Windows system"));
      gtk_box_append (GTK_BOX (volume_type_box), priv->tcrypt_system_toggle);

      priv->pim_entry = table_add_entry (operation, rows++, _("_PIM"), NULL, operation);
    }

  priv->password_entry = NULL;
  if (priv->ask_flags & G_ASK_PASSWORD_NEED_PASSWORD)
    {
      priv->password_entry = table_add_entry (operation, rows++, _("_Password"),
                                              NULL, operation);
      gtk_entry_set_visibility (GTK_ENTRY (priv->password_entry), FALSE);
    }

   if (priv->ask_flags & G_ASK_PASSWORD_SAVING_SUPPORTED)
    {
      GtkWidget    *remember_box;
      GtkWidget    *choice;
      GtkWidget    *group;
      GPasswordSave password_save;

      remember_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_grid_attach (GTK_GRID (grid), remember_box, 0, rows++, 2, 1);
      priv->user_widgets = g_list_prepend (priv->user_widgets, remember_box);

      label = gtk_label_new ("");
      gtk_box_append (GTK_BOX (remember_box), label);

      password_save = g_mount_operation_get_password_save (G_MOUNT_OPERATION (operation));
      priv->password_save = password_save;

      choice = gtk_check_button_new_with_mnemonic (_("Forget password _immediately"));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
                                   password_save == G_PASSWORD_SAVE_NEVER);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_NEVER));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_append (GTK_BOX (remember_box), choice);
      group = choice;

      choice = gtk_check_button_new_with_mnemonic (_("Remember password until you _logout"));
      gtk_check_button_set_group (GTK_CHECK_BUTTON (choice), GTK_CHECK_BUTTON (group));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
                                   password_save == G_PASSWORD_SAVE_FOR_SESSION);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_FOR_SESSION));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_append (GTK_BOX (remember_box), choice);
      group = choice;

      choice = gtk_check_button_new_with_mnemonic (_("Remember _forever"));
      gtk_check_button_set_group (GTK_CHECK_BUTTON (choice), GTK_CHECK_BUTTON (group));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
                                   password_save == G_PASSWORD_SAVE_PERMANENTLY);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_PERMANENTLY));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_append (GTK_BOX (remember_box), choice);
    }

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (pw_dialog_got_response), operation);

  if (can_anonymous)
    {
      /* The anonymous option will be active by default,
       * ensure the toggled signal is emitted for it.
       */
      g_signal_emit_by_name (priv->anonymous_toggle, "toggled");
    }
  else if (! pw_dialog_input_is_valid (operation))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  g_object_notify (G_OBJECT (operation), "is-showing");

  if (priv->parent_window)
    {
      gtk_window_set_transient_for (window, priv->parent_window);
      gtk_window_set_modal (window, TRUE);
    }
  else if (priv->display)
    gtk_window_set_display (GTK_WINDOW (dialog), priv->display);

  gtk_window_present (GTK_WINDOW (dialog));

  g_object_ref (operation);
}

static void
call_password_proxy_cb (GObject      *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  _GtkMountOperationHandler *proxy = _GTK_MOUNT_OPERATION_HANDLER (source);
  GMountOperation *op = user_data;
  GMountOperationResult result;
  GVariant *result_details;
  GVariantIter iter;
  const char *key;
  GVariant *value;
  GError *error = NULL;

  if (!_gtk_mount_operation_handler_call_ask_password_finish (proxy,
                                                              &result,
                                                              &result_details,
                                                              res,
                                                              &error))
    {
      result = G_MOUNT_OPERATION_ABORTED;
      g_warning ("Shell mount operation error: %s", error->message);
      g_error_free (error);
      goto out;
    }

  g_variant_iter_init (&iter, result_details);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      if (strcmp (key, "password") == 0)
        g_mount_operation_set_password (op, g_variant_get_string (value, NULL));
      else if (strcmp (key, "password_save") == 0)
        g_mount_operation_set_password_save (op, g_variant_get_uint32 (value));
      else if (strcmp (key, "hidden_volume") == 0)
        g_mount_operation_set_is_tcrypt_hidden_volume (op, g_variant_get_boolean (value));
      else if (strcmp (key, "system_volume") == 0)
        g_mount_operation_set_is_tcrypt_system_volume (op, g_variant_get_boolean (value));
      else if (strcmp (key, "pim") == 0)
        g_mount_operation_set_pim (op, g_variant_get_uint32 (value));
    }

 out:
  gtk_mount_operation_proxy_finish (GTK_MOUNT_OPERATION (op), result);
}

static void
gtk_mount_operation_ask_password_do_proxy (GtkMountOperation *operation,
                                           const char        *message,
                                           const char        *default_user,
                                           const char        *default_domain)
{
  char id[255];
  g_sprintf(id, "GtkMountOperation%p", operation);

  operation->priv->handler_showing = TRUE;
  g_object_notify (G_OBJECT (operation), "is-showing");

  /* keep a ref to the operation while the handler is showing */
  g_object_ref (operation);

  _gtk_mount_operation_handler_call_ask_password (operation->priv->handler, id,
                                                  message, "drive-harddisk",
                                                  default_user, default_domain,
                                                  operation->priv->ask_flags, NULL,
                                                  call_password_proxy_cb, operation);
}

static void
gtk_mount_operation_ask_password (GMountOperation   *mount_op,
                                  const char        *message,
                                  const char        *default_user,
                                  const char        *default_domain,
                                  GAskPasswordFlags  flags)
{
  GtkMountOperation *operation;
  GtkMountOperationPrivate *priv;
  gboolean use_gtk;

  operation = GTK_MOUNT_OPERATION (mount_op);
  priv = operation->priv;
  priv->ask_flags = flags;

  use_gtk = (operation->priv->handler == NULL) ||
    (priv->ask_flags & G_ASK_PASSWORD_NEED_DOMAIN) ||
    (priv->ask_flags & G_ASK_PASSWORD_NEED_USERNAME);

  if (use_gtk)
    gtk_mount_operation_ask_password_do_gtk (operation, message, default_user, default_domain);
  else
    gtk_mount_operation_ask_password_do_proxy (operation, message, default_user, default_domain);
}

static void
question_dialog_button_clicked (GObject      *source,
                                GAsyncResult *result,
                                void         *user_data)
{
  GtkAlertDialog *dialog = GTK_ALERT_DIALOG (source);
  GMountOperation *op = user_data;
  GtkMountOperation *operation;
  int button;

  operation = GTK_MOUNT_OPERATION (op);

  button = gtk_alert_dialog_choose_finish (dialog, result, NULL);
  if (button >= 0)
    {
      g_mount_operation_set_choice (op, button);
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  g_object_notify (G_OBJECT (operation), "is-showing");
  g_object_unref (op);
}

static void
gtk_mount_operation_ask_question_do_gtk (GtkMountOperation *op,
                                         const char        *message,
                                         const char        *choices[])
{
  GtkMountOperationPrivate *priv;
  GtkAlertDialog *dialog;
  const char *secondary = NULL;
  char       *primary;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (message != NULL);
  g_return_if_fail (choices != NULL);

  priv = op->priv;

  primary = strstr (message, "\n");
  if (primary)
    {
      secondary = primary + 1;
      primary = g_strndup (message, primary - message);
    }

  dialog = gtk_alert_dialog_new ("%s", primary ? primary : message);
  if (secondary)
    gtk_alert_dialog_set_detail (dialog, secondary);

  gtk_alert_dialog_set_buttons (dialog, choices);

  gtk_alert_dialog_choose (dialog, priv->parent_window,
                           NULL,
                           question_dialog_button_clicked, g_object_ref (op));
  g_object_unref (dialog);
  g_free (primary);

  g_object_notify (G_OBJECT (op), "is-showing");
}

static void
call_question_proxy_cb (GObject      *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  _GtkMountOperationHandler *proxy = _GTK_MOUNT_OPERATION_HANDLER (source);
  GMountOperation *op = user_data;
  GMountOperationResult result;
  GVariant *result_details;
  GVariantIter iter;
  const char *key;
  GVariant *value;
  GError *error = NULL;

  if (!_gtk_mount_operation_handler_call_ask_question_finish (proxy,
                                                              &result,
                                                              &result_details,
                                                              res,
                                                              &error))
    {
      result = G_MOUNT_OPERATION_ABORTED;
      g_warning ("Shell mount operation error: %s", error->message);
      g_error_free (error);
      goto out;
    }

  g_variant_iter_init (&iter, result_details);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      if (strcmp (key, "choice") == 0)
        g_mount_operation_set_choice (op, g_variant_get_int32 (value));
    }
 
 out:
  gtk_mount_operation_proxy_finish (GTK_MOUNT_OPERATION (op), result);
}

static void
gtk_mount_operation_ask_question_do_proxy (GtkMountOperation *operation,
                                           const char        *message,
                                           const char        *choices[])
{
  char id[255];
  g_sprintf(id, "GtkMountOperation%p", operation);

  operation->priv->handler_showing = TRUE;
  g_object_notify (G_OBJECT (operation), "is-showing");

  /* keep a ref to the operation while the handler is showing */
  g_object_ref (operation);

  _gtk_mount_operation_handler_call_ask_question (operation->priv->handler, id,
                                                  message, "drive-harddisk",
                                                  choices, NULL,
                                                  call_question_proxy_cb, operation);
}

static void
gtk_mount_operation_ask_question (GMountOperation *op,
                                  const char      *message,
                                  const char      *choices[])
{
  GtkMountOperation *operation;
  gboolean use_gtk;

  operation = GTK_MOUNT_OPERATION (op);
  use_gtk = (operation->priv->handler == NULL);

  if (use_gtk)
    gtk_mount_operation_ask_question_do_gtk (operation, message, choices);
  else
    gtk_mount_operation_ask_question_do_proxy (operation, message, choices);
}

static void
show_processes_button_clicked (GtkWidget       *button,
                               GMountOperation *op)
{
  GtkMountOperationPrivate *priv;
  GtkMountOperation *operation;
  int button_number;
  GtkDialog *dialog;

  button_number = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "choice"));
  dialog = GTK_DIALOG (gtk_widget_get_ancestor (button, GTK_TYPE_DIALOG));


  operation = GTK_MOUNT_OPERATION (op);
  priv = operation->priv;

  if (button_number >= 0)
    {
      g_mount_operation_set_choice (op, button_number);
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  priv->dialog = NULL;
  g_object_notify (G_OBJECT (operation), "is-showing");
  gtk_window_destroy (GTK_WINDOW (dialog));
  g_object_unref (op);
}

static int
pid_equal (gconstpointer a,
           gconstpointer b)
{
  GPid pa, pb;

  pa = *((GPid *) a);
  pb = *((GPid *) b);

  return GPOINTER_TO_INT(pb) - GPOINTER_TO_INT(pa);
}

static void
diff_sorted_arrays (GArray         *array1,
                    GArray         *array2,
                    GCompareFunc   compare,
                    GArray         *added_indices,
                    GArray         *removed_indices)
{
  int order;
  guint n1, n2;
  guint elem_size;

  n1 = n2 = 0;

  elem_size = g_array_get_element_size (array1);
  g_assert (elem_size == g_array_get_element_size (array2));

  while (n1 < array1->len && n2 < array2->len)
    {
      order = (*compare) (((const char*) array1->data) + n1 * elem_size,
                          ((const char*) array2->data) + n2 * elem_size);
      if (order < 0)
        {
          g_array_append_val (removed_indices, n1);
          n1++;
        }
      else if (order > 0)
        {
          g_array_append_val (added_indices, n2);
          n2++;
        }
      else
        { /* same item */
          n1++;
          n2++;
        }
    }

  while (n1 < array1->len)
    {
      g_array_append_val (removed_indices, n1);
      n1++;
    }
  while (n2 < array2->len)
    {
      g_array_append_val (added_indices, n2);
      n2++;
    }
}

static GdkTexture *
render_paintable_to_texture (GdkPaintable *paintable)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  gsk_render_node_unref (node);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

typedef struct _ProcessData ProcessData;

G_DECLARE_FINAL_TYPE (ProcessData, process_data, PROCESS, DATA, GObject);

struct _ProcessData
{
  GObject parent;

  GdkTexture *texture;
  char *name;
  GPid pid;
};

G_DEFINE_TYPE (ProcessData, process_data, G_TYPE_OBJECT);

static void
process_data_init (ProcessData *self)
{
}

static void
process_data_finalize (GObject *object)
{
  ProcessData *pd = PROCESS_DATA (object);

  g_free (pd->name);
  g_object_unref (pd->texture);

  G_OBJECT_CLASS (process_data_parent_class)->finalize (object);
}

static void
process_data_class_init (ProcessDataClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = process_data_finalize;
}

static ProcessData *
process_data_new (const char *name,
                  GPid        pid,
                  GdkTexture *texture)
{
  ProcessData *self;

  self = g_object_new (process_data_get_type (), NULL);
  self->name = g_strdup (name);
  self->pid = pid;
  g_set_object (&self->texture, texture);

  return self;
}

static void
add_pid_to_process_list_store (GtkMountOperation              *mount_operation,
                               GtkMountOperationLookupContext *lookup_context,
                               GListStore                     *list_store,
                               GPid                            pid)
{
  char *command_line;
  char *name;
  GdkTexture *texture;
  char *markup;

  name = NULL;
  texture = NULL;
  command_line = NULL;
  _gtk_mount_operation_lookup_info (lookup_context,
                                    pid,
                                    24,
                                    &name,
                                    &command_line,
                                    &texture);

  if (name == NULL)
    name = g_strdup_printf (_("Unknown Application (PID %d)"), (int) (gssize) pid);

  if (command_line == NULL)
    command_line = g_strdup ("");

  if (texture == NULL)
    {
      GtkIconTheme *theme;
      GtkIconPaintable *icon;

      theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (mount_operation->priv->dialog)));
      icon = gtk_icon_theme_lookup_icon (theme,
                                         "application-x-executable",
                                         NULL,
                                         24, 1,
                                         gtk_widget_get_direction (GTK_WIDGET (mount_operation->priv->dialog)),
                                         0);
      texture = render_paintable_to_texture (GDK_PAINTABLE (icon));
      g_object_unref (icon);
    }

  markup = g_strdup_printf ("<b>%s</b>\n"
                            "<small>%s</small>",
                            name,
                            command_line);

  g_list_store_append (list_store, process_data_new (markup, pid, texture));

  if (texture != NULL)
    g_object_unref (texture);
  g_free (markup);
  g_free (name);
  g_free (command_line);
}

static void
remove_pid_from_process_list_store (GtkMountOperation *mount_operation,
                                    GListStore        *list_store,
                                    GPid               pid)
{
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (list_store)); i++)
    {
      ProcessData *data = g_list_model_get_item (G_LIST_MODEL (list_store), i);

      g_object_unref (data);

      if (data->pid == pid)
        {
          g_list_store_remove (list_store, i);
          break;
        }
    }
}


static void
update_process_list_store (GtkMountOperation *mount_operation,
                           GListStore        *list_store,
                           GArray            *processes)
{
  guint n;
  GtkMountOperationLookupContext *lookup_context;
  GArray *current_pids;
  GArray *pid_indices_to_add;
  GArray *pid_indices_to_remove;
  GPid pid;

  /* Just removing all items and adding new ones will screw up the
   * focus handling in the treeview - so compute the delta, and add/remove
   * items as appropriate
   */
  current_pids = g_array_new (FALSE, FALSE, sizeof (GPid));
  pid_indices_to_add = g_array_new (FALSE, FALSE, sizeof (int));
  pid_indices_to_remove = g_array_new (FALSE, FALSE, sizeof (int));

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (list_store)); i++)
    {
      ProcessData *data = g_list_model_get_item (G_LIST_MODEL (list_store), i);

      g_array_append_val (current_pids, data->pid);

      g_object_unref (data);
    }

  g_array_sort (current_pids, pid_equal);
  g_array_sort (processes, pid_equal);

  diff_sorted_arrays (current_pids, processes, pid_equal, pid_indices_to_add, pid_indices_to_remove);

  for (n = 0; n < pid_indices_to_remove->len; n++)
    {
      pid = g_array_index (current_pids, GPid, n);
      remove_pid_from_process_list_store (mount_operation, list_store, pid);
    }

  if (pid_indices_to_add->len > 0)
    {
      lookup_context = _gtk_mount_operation_lookup_context_get (gtk_widget_get_display (mount_operation->priv->process_list_view));
      for (n = 0; n < pid_indices_to_add->len; n++)
        {
          pid = g_array_index (processes, GPid, n);
          add_pid_to_process_list_store (mount_operation, lookup_context, list_store, pid);
        }
      _gtk_mount_operation_lookup_context_free (lookup_context);
    }

  g_array_unref (current_pids);
  g_array_unref (pid_indices_to_add);
  g_array_unref (pid_indices_to_remove);
}

static void
on_end_process_activated (GtkButton         *button,
                          GtkMountOperation *op)
{
  GtkSelectionModel *selection;
  ProcessData *data;
  GError *error;

  selection = gtk_list_view_get_model (GTK_LIST_VIEW (op->priv->process_list_view));
  if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (selection)) == GTK_INVALID_LIST_POSITION)
    goto out;

  data = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (selection));

  /* TODO: We might want to either
   *
   *       - Be smart about things and send SIGKILL rather than SIGTERM if
   *         this is the second time the user requests killing a process
   *
   *       - Or, easier (but worse user experience), offer both "End Process"
   *         and "Terminate Process" options
   *
   *      But that's not how things work right now....
   */
  error = NULL;
  if (!_gtk_mount_operation_kill_process (data->pid, &error))
    {
      GtkAlertDialog *dialog;

      /* Use GTK_DIALOG_DESTROY_WITH_PARENT here since the parent dialog can be
       * indeed be destroyed via the GMountOperation::abort signal... for example,
       * this is triggered if the user yanks the device while we are showing
       * the dialog...
       */
      dialog = gtk_alert_dialog_new (_("Unable to end process"));
      gtk_alert_dialog_set_detail (dialog, error->message);
      gtk_alert_dialog_show (dialog, GTK_WINDOW (op->priv->dialog));
      g_object_unref (dialog);

      g_error_free (error);
    }

 out:
  ;
}

static void
setup_process_row (GtkListItemFactory *factory,
                   GtkListItem        *item)
{
  GtkWidget *box, *picture, *label;

  picture = gtk_picture_new ();
  label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), picture);
  gtk_box_append (GTK_BOX (box), label);

  gtk_list_item_set_child (item, box);
}

static void
bind_process_row (GtkListItemFactory *factory,
                  GtkListItem        *item)
{
  GtkWidget *box, *picture, *label;
  ProcessData *data;

  data = gtk_list_item_get_item (item);

  box = gtk_list_item_get_child (item);
  picture = gtk_widget_get_first_child (box);
  label = gtk_widget_get_next_sibling (picture);

  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (data->texture));
  gtk_label_set_markup (GTK_LABEL (label), data->name);
}

static GtkWidget *
create_show_processes_dialog (GtkMountOperation *op,
                              const char      *message,
                              const char      *choices[])
{
  GtkMountOperationPrivate *priv;
  GtkWidget  *dialog;
  const char *secondary = NULL;
  char       *primary;
  int        count, len = 0;
  GtkWidget *label;
  GtkWidget *list_view;
  GtkWidget *scrolled_window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *content_area;
  char *s;
  GListStore *store;
  GtkListItemFactory *factory;
  GtkWidget *button;

  priv = op->priv;

  primary = strstr (message, "\n");
  if (primary)
    {
      secondary = primary + 1;
      primary = g_strndup (message, primary - message);
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  dialog = gtk_dialog_new ();
G_GNUC_END_IGNORE_DEPRECATIONS

  if (priv->parent_window != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), priv->parent_window);
  gtk_window_set_title (GTK_WINDOW (dialog), "");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (vbox, 12);
  gtk_widget_set_margin_bottom (vbox, 12);
  gtk_widget_set_margin_start (vbox, 12);
  gtk_widget_set_margin_end (vbox, 12);
  gtk_box_append (GTK_BOX (content_area), vbox);

  if (secondary != NULL)
    s = g_strdup_printf ("<big><b>%s</b></big>\n\n%s", primary, secondary);
  else
    s = g_strdup_printf ("%s", primary);

  g_free (primary);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), s);
  g_free (s);
  gtk_box_append (GTK_BOX (vbox), label);

  /* First count the items in the list then
   * add the buttons in reverse order
   */

  while (choices[len] != NULL)
    len++;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  for (count = len - 1; count >= 0; count--)
    {
      button = gtk_button_new_with_label (choices[count]);
      g_object_set_data (G_OBJECT (button), "choice", GINT_TO_POINTER (count));
      g_signal_connect (button, "clicked", G_CALLBACK (show_processes_button_clicked), op);
      gtk_box_append (GTK_BOX (hbox), button);
    }
  gtk_widget_set_halign (hbox, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (vbox), hbox);

  priv->dialog = GTK_DIALOG (dialog);
  g_object_notify (G_OBJECT (op), "is-showing");

  if (priv->parent_window == NULL && priv->display)
    gtk_window_set_display (GTK_WINDOW (dialog), priv->display);

  store = g_list_store_new (process_data_get_type ());
  factory = gtk_signal_list_item_factory_new ();

  g_signal_connect (factory, "setup", G_CALLBACK (setup_process_row), op);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_process_row), op);

  list_view = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (store))), factory);

  gtk_widget_set_size_request (list_view, 300, 120);

  scrolled_window = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), list_view);
  gtk_box_append (GTK_BOX (vbox), scrolled_window);

  button = gtk_button_new_with_mnemonic (_("_End Process"));
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  g_signal_connect (button, "clicked", G_CALLBACK (on_end_process_activated), op);
  gtk_box_append (GTK_BOX (vbox), button);

  priv->process_list_store = store;
  priv->process_list_view = list_view;
  /* set pointers to NULL when dialog goes away */
  g_object_add_weak_pointer (G_OBJECT (priv->process_list_store), (gpointer *) &priv->process_list_store);
  g_object_add_weak_pointer (G_OBJECT (priv->process_list_view), (gpointer *) &priv->process_list_view);

  g_object_ref (op);

  return dialog;
}

static void
call_processes_proxy_cb (GObject     *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  _GtkMountOperationHandler *proxy = _GTK_MOUNT_OPERATION_HANDLER (source);
  GMountOperation *op = user_data;
  GMountOperationResult result;
  GVariant *result_details;
  GVariantIter iter;
  const char *key;
  GVariant *value;
  GError *error = NULL;

  if (!_gtk_mount_operation_handler_call_show_processes_finish (proxy,
                                                                &result,
                                                                &result_details,
                                                                res,
                                                                &error))
    {
      result = G_MOUNT_OPERATION_ABORTED;
      g_warning ("Shell mount operation error: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* If the request was unhandled it means we called the method again;
   * in this case, just return and wait for the next response.
   */
  if (result == G_MOUNT_OPERATION_UNHANDLED)
    return;

  g_variant_iter_init (&iter, result_details);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      if (strcmp (key, "choice") == 0)
        g_mount_operation_set_choice (op, g_variant_get_int32 (value));
    }

 out:
  gtk_mount_operation_proxy_finish (GTK_MOUNT_OPERATION (op), result);
}

static void
gtk_mount_operation_show_processes_do_proxy (GtkMountOperation *operation,
                                             const char        *message,
                                             GArray            *processes,
                                             const char        *choices[])
{
  char id[255];
  g_sprintf(id, "GtkMountOperation%p", operation);

  operation->priv->handler_showing = TRUE;
  g_object_notify (G_OBJECT (operation), "is-showing");

  /* keep a ref to the operation while the handler is showing */
  g_object_ref (operation);

  _gtk_mount_operation_handler_call_show_processes (operation->priv->handler, id,
                                                    message, "drive-harddisk",
                                                    g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
                                                                               processes->data, processes->len,
                                                                               sizeof (GPid)),
                                                    choices, NULL,
                                                    call_processes_proxy_cb, operation);
}

static void
gtk_mount_operation_show_processes_do_gtk (GtkMountOperation *op,
                                           const char        *message,
                                           GArray            *processes,
                                           const char        *choices[])
{
  GtkMountOperationPrivate *priv;
  GtkWidget *dialog = NULL;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (message != NULL);
  g_return_if_fail (processes != NULL);
  g_return_if_fail (choices != NULL);

  priv = op->priv;

  if (priv->process_list_store == NULL)
    {
      /* need to create the dialog */
      dialog = create_show_processes_dialog (op, message, choices);
    }

  /* otherwise, we're showing the dialog, assume messages+choices hasn't changed */

  update_process_list_store (op,
                             priv->process_list_store,
                             processes);

  if (dialog != NULL)
    gtk_window_present (GTK_WINDOW (dialog));
}


static void
gtk_mount_operation_show_processes (GMountOperation *op,
                                    const char      *message,
                                    GArray          *processes,
                                    const char      *choices[])
{

  GtkMountOperation *operation;
  gboolean use_gtk;

  operation = GTK_MOUNT_OPERATION (op);
  use_gtk = (operation->priv->handler == NULL);

  if (use_gtk)
    gtk_mount_operation_show_processes_do_gtk (operation, message, processes, choices);
  else
    gtk_mount_operation_show_processes_do_proxy (operation, message, processes, choices);
}

static void
gtk_mount_operation_aborted (GMountOperation *op)
{
  GtkMountOperationPrivate *priv;

  priv = GTK_MOUNT_OPERATION (op)->priv;

  if (priv->dialog != NULL)
    {
      gtk_window_destroy (GTK_WINDOW (priv->dialog));
      priv->dialog = NULL;
      g_object_notify (G_OBJECT (op), "is-showing");
      g_object_unref (op);
    }

  if (priv->handler != NULL)
    {
      _gtk_mount_operation_handler_call_close (priv->handler, NULL, NULL, NULL);

      priv->handler_showing = FALSE;
      g_object_notify (G_OBJECT (op), "is-showing");
    }
}

/**
 * gtk_mount_operation_new:
 * @parent: (nullable): transient parent of the window
 *
 * Creates a new `GtkMountOperation`.
 *
 * Returns: a new `GtkMountOperation`
 */
GMountOperation *
gtk_mount_operation_new (GtkWindow *parent)
{
  GMountOperation *mount_operation;

  mount_operation = g_object_new (GTK_TYPE_MOUNT_OPERATION,
                                  "parent", parent, NULL);

  return mount_operation;
}

/**
 * gtk_mount_operation_is_showing: (attributes org.gtk.Method.get_property=is-showing)
 * @op: a `GtkMountOperation`
 *
 * Returns whether the `GtkMountOperation` is currently displaying
 * a window.
 *
 * Returns: %TRUE if @op is currently displaying a window
 */
gboolean
gtk_mount_operation_is_showing (GtkMountOperation *op)
{
  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), FALSE);

  return op->priv->dialog != NULL;
}

/**
 * gtk_mount_operation_set_parent: (attributes org.gtk.Method.set_property=parent)
 * @op: a `GtkMountOperation`
 * @parent: (nullable): transient parent of the window
 *
 * Sets the transient parent for windows shown by the
 * `GtkMountOperation`.
 */
void
gtk_mount_operation_set_parent (GtkMountOperation *op,
                                GtkWindow         *parent)
{
  GtkMountOperationPrivate *priv;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  priv = op->priv;

  if (priv->parent_window == parent)
    return;

  if (priv->parent_window)
    {
      g_signal_handlers_disconnect_by_func (priv->parent_window,
                                            parent_destroyed,
                                            &priv->parent_window);
      g_object_unref (priv->parent_window);
    }
  priv->parent_window = parent;
  if (priv->parent_window)
    {
      g_object_ref (priv->parent_window);
      g_signal_connect (priv->parent_window, "destroy",
                        G_CALLBACK (parent_destroyed), &priv->parent_window);
    }

  if (priv->dialog)
    gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), priv->parent_window);

  g_object_notify (G_OBJECT (op), "parent");
}

/**
 * gtk_mount_operation_get_parent: (attributes org.gtk.Method.get_property=parent)
 * @op: a `GtkMountOperation`
 *
 * Gets the transient parent used by the `GtkMountOperation`.
 *
 * Returns: (transfer none) (nullable): the transient parent for windows shown by @op
 */
GtkWindow *
gtk_mount_operation_get_parent (GtkMountOperation *op)
{
  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), NULL);

  return op->priv->parent_window;
}

/**
 * gtk_mount_operation_set_display: (attributes org.gtk.Method.set_property=display)
 * @op: a `GtkMountOperation`
 * @display: a `GdkDisplay`
 *
 * Sets the display to show windows of the `GtkMountOperation` on.
 */
void
gtk_mount_operation_set_display (GtkMountOperation *op,
                                 GdkDisplay        *display)
{
  GtkMountOperationPrivate *priv;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  priv = op->priv;

  if (priv->display == display)
    return;

  if (priv->display)
    g_object_unref (priv->display);

  priv->display = g_object_ref (display);

  if (priv->dialog)
    gtk_window_set_display (GTK_WINDOW (priv->dialog), display);

  g_object_notify (G_OBJECT (op), "display");
}

/**
 * gtk_mount_operation_get_display: (attributes org.gtk.Method.get_property=display)
 * @op: a `GtkMountOperation`
 *
 * Gets the display on which windows of the `GtkMountOperation`
 * will be shown.
 *
 * Returns: (transfer none): the display on which windows of @op are shown
 */
GdkDisplay *
gtk_mount_operation_get_display (GtkMountOperation *op)
{
  GtkMountOperationPrivate *priv;

  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), NULL);

  priv = op->priv;

  if (priv->dialog)
    return gtk_widget_get_display (GTK_WIDGET (priv->dialog));
  else if (priv->parent_window)
    return gtk_widget_get_display (GTK_WIDGET (priv->parent_window));
  else if (priv->display)
    return priv->display;
  else
    return gdk_display_get_default ();
}
