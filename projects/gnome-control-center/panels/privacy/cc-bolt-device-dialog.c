/* Copyright (C) 2018 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Christian J. Kellner <ckellner@redhat.com>
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "cc-list-row.h"

#include "bolt-device.h"
#include "bolt-error.h"
#include "bolt-time.h"

#include "cc-bolt-device-dialog.h"
#include "cc-bolt-device-entry.h"

struct _CcBoltDeviceDialog
{
  AdwWindow     parent;

  BoltClient   *client;
  BoltDevice   *device;
  GCancellable *cancel;

  AdwToastOverlay *toast_overlay;

  /* device details */
  CcListRow *status_row;
  CcListRow *uuid_row;
  CcListRow *time_row;

  /* parents */
  AdwPreferencesGroup *parents_group;
  GtkListBox *parents_devices;

  /* actions */
  GtkWidget  *button_box;
  GtkSpinner *spinner;
  GtkButton  *connect_button;
  GtkButton  *forget_button;
};

static void     on_forget_button_clicked_cb (CcBoltDeviceDialog *dialog);
static void     on_connect_button_clicked_cb (CcBoltDeviceDialog *dialog);

G_DEFINE_TYPE (CcBoltDeviceDialog, cc_bolt_device_dialog, ADW_TYPE_WINDOW);

#define RESOURCE_UI "/org/gnome/control-center/privacy/cc-bolt-device-dialog.ui"

static const char *
status_to_string_for_ui (BoltDevice *dev)
{
  BoltStatus status;
  BoltAuthFlags aflags;
  gboolean nopcie;

  status = bolt_device_get_status (dev);
  aflags = bolt_device_get_authflags(dev);
  nopcie = bolt_flag_isset (aflags, BOLT_AUTH_NOPCIE);

  switch (status)
    {
    case BOLT_STATUS_DISCONNECTED:
      return C_("Thunderbolt Device Status", "Disconnected");

    case BOLT_STATUS_CONNECTING:
      return C_("Thunderbolt Device Status", "Connecting");

    case BOLT_STATUS_CONNECTED:
      return C_("Thunderbolt Device Status", "Connected");

    case BOLT_STATUS_AUTH_ERROR:
      return C_("Thunderbolt Device Status", "Authorization Error");

    case BOLT_STATUS_AUTHORIZING:
      return C_("Thunderbolt Device Status", "Authorizing");

    case BOLT_STATUS_AUTHORIZED:
    case BOLT_STATUS_AUTHORIZED_NEWKEY:
    case BOLT_STATUS_AUTHORIZED_SECURE:
    case BOLT_STATUS_AUTHORIZED_DPONLY:
      if (nopcie)
        return C_("Thunderbolt Device Status", "Reduced Functionality");
      else
        return C_("Thunderbolt Device Status", "Connected & Authorized");

    case BOLT_STATUS_UNKNOWN:
      break; /* use default return value, i.e. Unknown */
    }

  return C_("Thunderbolt Device Status", "Unknown");
}

static void
dialog_update_from_device (CcBoltDeviceDialog *dialog)
{
  g_autofree char *generated = NULL;
  g_autofree char *timestr = NULL;
  const char *label;
  const char *uuid;
  const char *status_brief;
  BoltStatus status;
  gboolean stored;
  BoltDevice *dev;
  guint timestamp;

  if (gtk_widget_in_destruction (GTK_WIDGET (dialog)))
    return;

  dev = dialog->device;

  uuid = bolt_device_get_uid (dev);
  label = bolt_device_get_label (dev);

  stored = bolt_device_is_stored (dev);
  status = bolt_device_get_status (dev);

  if (label == NULL)
    {
      const char *name = bolt_device_get_name (dev);
      const char *vendor = bolt_device_get_vendor (dev);

      generated = g_strdup_printf ("%s %s", name, vendor);
      label = generated;
    }

  gtk_window_set_title (GTK_WINDOW (dialog), label);

  status_brief = status_to_string_for_ui (dev);
  cc_list_row_set_secondary_label (dialog->status_row, status_brief);
  gtk_widget_set_visible (GTK_WIDGET (dialog->forget_button), stored);

  /* while we are having an ongoing operation we are setting the buttons
   * to be in-sensitive. In that case, if the button was visible
   * before it will be hidden when the operation is finished by the
   * dialog_operation_done() function */
  if (gtk_widget_is_sensitive (GTK_WIDGET (dialog->connect_button)))
    gtk_widget_set_visible (GTK_WIDGET (dialog->connect_button),
                            status == BOLT_STATUS_CONNECTED);

  cc_list_row_set_secondary_label (dialog->uuid_row, uuid);

  if (bolt_status_is_authorized (status))
    {
      /* Translators: The time point the device was authorized. */
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dialog->time_row), _("Authorized at"));
      timestamp = bolt_device_get_authtime (dev);
    }
  else if (bolt_status_is_connected (status))
    {
      /* Translators: The time point the device was connected. */
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dialog->time_row), _("Connected at"));
      timestamp = bolt_device_get_conntime (dev);
    }
  else
    {
      /* Translators: The time point the device was enrolled,
       * i.e. authorized and stored in the device database. */
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dialog->time_row), _("Enrolled at"));
      timestamp = bolt_device_get_storetime (dev);
    }

  timestr = bolt_epoch_format (timestamp, "%c");
  cc_list_row_set_secondary_label (dialog->time_row, timestr);

}

static void
on_device_notify_cb (GObject    *gobject,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);

  dialog_update_from_device (dialog);
}

static void
dialog_operation_start (CcBoltDeviceDialog *dialog)
{
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->connect_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->forget_button), FALSE);
  gtk_spinner_start (dialog->spinner);
}

static void
dialog_operation_done (CcBoltDeviceDialog *dialog,
                       GtkWidget          *sender,
                       GError             *error)
{
  GtkWidget *cb = GTK_WIDGET (dialog->connect_button);
  GtkWidget *fb = GTK_WIDGET (dialog->forget_button);

  /* don' do anything if we are being destroyed */
  if (gtk_widget_in_destruction (GTK_WIDGET (dialog)))
    return;

  /* also don't do anything if the op was canceled */
  if (error != NULL && bolt_err_cancelled (error))
    return;

  gtk_spinner_stop (dialog->spinner);

  if (error != NULL)
    {
      AdwToast *toast = adw_toast_new (error->message);
      adw_toast_overlay_add_toast (dialog->toast_overlay, toast);

      /* set the *other* button to sensitive */
      gtk_widget_set_sensitive (cb, cb != sender);
      gtk_widget_set_sensitive (fb, fb != sender);
    }
  else
    {
      gtk_widget_set_visible (sender, FALSE);
      gtk_widget_set_sensitive (cb, TRUE);
      gtk_widget_set_sensitive (fb, TRUE);
    }
}

static void
on_connect_all_done (GObject      *source_object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);
  gboolean ok;

  ok = bolt_client_connect_all_finish (dialog->client, res, &err);

  if (!ok)
    g_prefix_error (&err, _("Failed to authorize device: "));

  dialog_operation_done (dialog, GTK_WIDGET (dialog->connect_button), err);
}

static void
on_connect_button_clicked_cb (CcBoltDeviceDialog *dialog)
{
  g_autoptr(GPtrArray) devices = NULL;
  BoltDevice *device = dialog->device;
  GtkWidget *child;

  g_return_if_fail (device != NULL);

  dialog_operation_start (dialog);

  devices = g_ptr_array_new ();

  /* Iter from the last child to the first one */
  for (child = gtk_widget_get_last_child (GTK_WIDGET (dialog->parents_devices));
       child;
       child = gtk_widget_get_prev_sibling (child))
    {
      CcBoltDeviceEntry *entry;
      BoltDevice *dev;
      BoltStatus status;

      entry = CC_BOLT_DEVICE_ENTRY (child);
      dev = cc_bolt_device_entry_get_device (entry);
      status = bolt_device_get_status (dev);

      /* skip any devices down in the chain that are already authorized
       * NB: it is not possible to have gaps of non-authorized devices
       * in the chain, i.e. once we encounter a non-authorized device,
       * all following device (down the chain, towards the target) will
       * also be not authorized. */
      if (!bolt_status_is_pending (status))
        continue;

      /* device is now either !stored || pending */
      g_ptr_array_add (devices, dev);
    }

  /* finally the actual device of the dialog */
  g_ptr_array_add (devices, device);

  bolt_client_connect_all_async (dialog->client,
				 devices,
				 BOLT_POLICY_DEFAULT,
				 BOLT_AUTHCTRL_NONE,
				 dialog->cancel,
				 on_connect_all_done,
				 dialog);
}

static void
on_forget_device_done (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);
  gboolean ok;

  ok = bolt_client_forget_device_finish (dialog->client, res, &err);

  if (!ok)
    g_prefix_error (&err, _("Failed to forget device: "));

  dialog_operation_done (dialog, GTK_WIDGET (dialog->forget_button), err);
}

static void
on_forget_button_clicked_cb (CcBoltDeviceDialog *dialog)
{
  const char *uid = NULL;

  g_return_if_fail (dialog->device != NULL);

  uid = bolt_device_get_uid (dialog->device);
  dialog_operation_start (dialog);

  bolt_client_forget_device_async (dialog->client,
                                   uid,
                                   dialog->cancel,
                                   on_forget_device_done,
                                   dialog);
}

static void
cc_bolt_device_dialog_finalize (GObject *object)
{
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (object);

  g_clear_object (&dialog->device);
  g_cancellable_cancel (dialog->cancel);
  g_clear_object (&dialog->cancel);
  g_clear_object (&dialog->client);

  G_OBJECT_CLASS (cc_bolt_device_dialog_parent_class)->finalize (object);
}

static void
cc_bolt_device_dialog_class_init (CcBoltDeviceDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_bolt_device_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_UI);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, toast_overlay);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, status_row);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, uuid_row);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, time_row);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, parents_devices);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, parents_group);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, button_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, connect_button);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, forget_button);

  gtk_widget_class_bind_template_callback (widget_class, on_connect_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_forget_button_clicked_cb);
}

static void
cc_bolt_device_dialog_init (CcBoltDeviceDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

/* public functions */
CcBoltDeviceDialog *
cc_bolt_device_dialog_new (void)
{
  CcBoltDeviceDialog *dialog;

  dialog = g_object_new (CC_TYPE_BOLT_DEVICE_DIALOG,
                         NULL);
  return dialog;
}

void
cc_bolt_device_dialog_set_client (CcBoltDeviceDialog *dialog,
                                  BoltClient         *client)
{
  g_clear_object (&dialog->client);
  dialog->client = g_object_ref (client);
}

void
cc_bolt_device_dialog_set_device (CcBoltDeviceDialog *dialog,
                                  BoltDevice         *device,
				  GPtrArray          *parents)
{
  g_autofree char *msg = NULL;
  guint i;

  if (device == dialog->device)
    return;

  if (dialog->device)
    {
      GtkWidget *child;

      g_cancellable_cancel (dialog->cancel);
      g_clear_object (&dialog->cancel);
      dialog->cancel = g_cancellable_new ();

      g_signal_handlers_disconnect_by_func (dialog->device,
                                            G_CALLBACK (on_device_notify_cb),
                                            dialog);
      g_clear_object (&dialog->device);

      while ((child = gtk_widget_get_first_child (GTK_WIDGET (dialog->parents_devices))) != NULL)
        gtk_list_box_remove (dialog->parents_devices, child);

      gtk_widget_set_visible (GTK_WIDGET (dialog->parents_group), FALSE);
    }

  if (device == NULL)
    return;

  dialog->device = g_object_ref (device);
  g_signal_connect_object (dialog->device,
                           "notify",
                           G_CALLBACK (on_device_notify_cb),
                           dialog,
                           0);

  /* reset the sensitivity of the buttons, because
   * dialog_update_from_device, because it can't know */
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->connect_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->forget_button), TRUE);

  dialog_update_from_device (dialog);

  /* no parents, we are done here */
  if (!parents || parents->len == 0)
    return;

  msg = g_strdup_printf (ngettext ("Depends on %u other device",
				   "Depends on %u other devices",
				   parents->len), parents->len);

  adw_preferences_group_set_title (dialog->parents_group, msg);
  gtk_widget_set_visible (GTK_WIDGET (dialog->parents_group), TRUE);

  for (i = 0; i < parents->len; i++)
    {
      CcBoltDeviceEntry *entry;
      BoltDevice *parent;

      parent = g_ptr_array_index (parents, i);

      entry = cc_bolt_device_entry_new (parent, TRUE);
      gtk_list_box_append (dialog->parents_devices, GTK_WIDGET (entry));
    }
}

BoltDevice *
cc_bolt_device_dialog_peek_device (CcBoltDeviceDialog *dialog)
{
  return dialog->device;
}

gboolean
cc_bolt_device_dialog_device_equal (CcBoltDeviceDialog *dialog,
                                    BoltDevice         *device)
{
  return dialog->device != NULL && device == dialog->device;
}
