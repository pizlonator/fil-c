/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <glib/gi18n.h>
#include <cairo/cairo.h>

#include "cc-fingerprint-dialog.h"

#include "cc-fingerprint-manager.h"
#include "cc-fprintd-generated.h"
#include "cc-list-row.h"

#include "config.h"

#define CC_FPRINTD_NAME "net.reactivated.Fprint"

/* Translate fprintd strings */
#define TR(s) dgettext ("fprintd", s)
#include "fingerprint-strings.h"

typedef enum {
  DIALOG_STATE_NONE                   = 0,
  DIALOG_STATE_DEVICES_LISTING        = (1 << 0),
  DIALOG_STATE_DEVICE_CLAIMING        = (1 << 1),
  DIALOG_STATE_DEVICE_CLAIMED         = (1 << 2),
  DIALOG_STATE_DEVICE_PRINTS_LISTING  = (1 << 3),
  DIALOG_STATE_DEVICE_RELEASING       = (1 << 4),
  DIALOG_STATE_DEVICE_ENROLL_STARTING = (1 << 5),
  DIALOG_STATE_DEVICE_ENROLLING       = (1 << 6),
  DIALOG_STATE_DEVICE_ENROLL_STOPPING = (1 << 7),
  DIALOG_STATE_DEVICE_DELETING        = (1 << 8),

  DIALOG_STATE_IDLE = DIALOG_STATE_DEVICE_CLAIMED | DIALOG_STATE_DEVICE_ENROLLING,
} DialogState;

struct _CcFingerprintDialog
{
  GtkWindow parent_instance;

  GtkButton      *back_button;
  GtkButton      *cancel_button;
  GtkButton      *delete_prints_button;
  GtkButton      *done_button;
  GtkBox         *add_print_popover_box;
  GtkEntry       *enroll_print_entry;
  GtkFlowBox     *prints_gallery;
  GtkHeaderBar   *titlebar;
  GtkImage       *enroll_result_image;
  GtkLabel       *enroll_message;
  GtkLabel       *enroll_result_message;
  GtkLabel       *infobar_error;
  GtkLabel       *title;
  GtkListBox     *devices_list;
  GtkPopover     *add_print_popover;
  GtkSpinner     *spinner;
  GtkStack       *stack;
  GtkWidget      *add_print_icon;
  GtkWidget      *delete_confirmation_infobar;
  GtkWidget      *device_selector;
  GtkWidget      *enroll_print_bin;
  GtkWidget      *enroll_result_icon;
  GtkWidget      *enrollment_view;
  GtkWidget      *error_infobar;
  GtkWidget      *no_devices_found;
  GtkWidget      *prints_manager;

  CcFingerprintManager *manager;
  DialogState           dialog_state;
  CcFprintdDevice      *device;
  gulong                device_signal_id;
  gulong                device_name_owner_id;
  GCancellable         *cancellable;
  GStrv                 enrolled_fingers;
  guint                 enroll_stages_passed;
  guint                 enroll_stage_passed_id;
  gdouble               enroll_progress;
};

/* TODO - fprintd and API changes required:
  - Identify the finger when the enroll dialog is visible
    + Only if device supports identification
      · And only in such case support enrolling more than one finger
  - Delete a single fingerprint | and remove the "Delete all" button
  - Highlight the finger when the sensor is touched during enrollment
  - Add customized labels to fingerprints
  - Devices hotplug (object manager)
 */

G_DEFINE_TYPE (CcFingerprintDialog, cc_fingerprint_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

#define N_VALID_FINGERS G_N_ELEMENTS (FINGER_IDS) - 1
/* The order of the fingers here will affect the UI order */
const char * FINGER_IDS[] = {
  "right-index-finger",
  "left-index-finger",
  "right-thumb",
  "right-middle-finger",
  "right-ring-finger",
  "right-little-finger",
  "left-thumb",
  "left-middle-finger",
  "left-ring-finger",
  "left-little-finger",
  "any",
};

typedef enum {
  ENROLL_STATE_NORMAL,
  ENROLL_STATE_RETRY,
  ENROLL_STATE_SUCCESS,
  ENROLL_STATE_WARNING,
  ENROLL_STATE_ERROR,
  ENROLL_STATE_COMPLETED,
  N_ENROLL_STATES,
} EnrollState;

const char * ENROLL_STATE_CLASSES[N_ENROLL_STATES] = {
  "",
  "retry",
  "success",
  "warning",
  "error",
  "completed",
};

static GParamSpec *properties[N_PROPS];

CcFingerprintDialog *
cc_fingerprint_dialog_new (CcFingerprintManager *manager)
{
  return g_object_new (CC_TYPE_FINGERPRINT_DIALOG,
                       "fingerprint-manager", manager,
                       NULL);
}

static gboolean
update_dialog_state (CcFingerprintDialog *self,
                     DialogState         state)
{
  if (self->dialog_state == state)
    return FALSE;

  self->dialog_state = state;

  if (self->dialog_state == DIALOG_STATE_NONE ||
      self->dialog_state == (self->dialog_state & DIALOG_STATE_IDLE))
    {
      gtk_spinner_stop (self->spinner);
    }
  else
    {
      gtk_spinner_start (self->spinner);
    }

  return TRUE;
}

static gboolean
add_dialog_state (CcFingerprintDialog *self,
                  DialogState          state)
{
  return update_dialog_state (self, (self->dialog_state | state));
}

static gboolean
remove_dialog_state (CcFingerprintDialog *self,
                     DialogState          state)
{
  return update_dialog_state (self, (self->dialog_state & ~state));
}

typedef struct
{
  CcFingerprintDialog *dialog;
  DialogState          state;
} DialogStateRemover;

static DialogStateRemover *
auto_state_remover (CcFingerprintDialog *self,
                    DialogState          state)
{
  DialogStateRemover *state_remover;

  state_remover = g_new0 (DialogStateRemover, 1);
  state_remover->dialog = g_object_ref (self);
  state_remover->state = state;

  return state_remover;
}

static void
auto_state_remover_cleanup (DialogStateRemover *state_remover)
{
  remove_dialog_state (state_remover->dialog, state_remover->state);
  g_clear_object (&state_remover->dialog);
  g_free (state_remover);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DialogStateRemover, auto_state_remover_cleanup);

static const char *
dbus_error_to_human (CcFingerprintDialog *self,
                     GError              *error)
{
  g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

  if (dbus_error == NULL)
    { /* Fallback to generic */ }
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.ClaimDevice"))
    return _("the device needs to be claimed to perform this action");
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.AlreadyInUse"))
    return _("the device is already claimed by another process");
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.PermissionDenied"))
    return _("you do not have permission to perform the action");
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoEnrolledPrints"))
    return _("no prints have been enrolled");
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoActionInProgress"))
    { /* Fallback to generic */ }
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.InvalidFingername"))
    { /* Fallback to generic */ }
  else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.Internal"))
    { /* Fallback to generic */ }

  if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING)
    return _("Failed to communicate with the device during enrollment");

  if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED ||
      self->dialog_state & DIALOG_STATE_DEVICE_CLAIMING)
    return _("Failed to communicate with the fingerprint reader");

  return _("Failed to communicate with the fingerprint daemon");
}

static void
disconnect_device_signals (CcFingerprintDialog *self)
{
  if (!self->device)
    return;

  if (self->device_signal_id)
    {
      g_signal_handler_disconnect (self->device, self->device_signal_id);
      self->device_signal_id = 0;
    }

  if (self->device_name_owner_id)
    {
      g_signal_handler_disconnect (self->device, self->device_name_owner_id);
      self->device_name_owner_id = 0;
    }
}

static void
cc_fingerprint_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_fingerprint_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_set_object (&self->manager, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
notify_error (CcFingerprintDialog *self,
              const char          *error_message)
{
  if (error_message)
    gtk_label_set_label (self->infobar_error, error_message);

  gtk_widget_set_visible (self->error_infobar, error_message != NULL);
}

static GtkWidget *
fingerprint_icon_new (const char *icon_name,
                      const char *label_text,
                      GType       icon_widget_type,
                      gpointer    progress_data,
                      GtkWidget **out_icon,
                      GtkWidget **out_label)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *icon_widget;

  g_return_val_if_fail (g_type_is_a (icon_widget_type, GTK_TYPE_WIDGET), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_name (box, "fingerprint-box");
  gtk_widget_set_hexpand (box, TRUE);

  image = gtk_image_new_from_icon_name (icon_name);

  if (icon_widget_type == GTK_TYPE_IMAGE)
    icon_widget = image;
  else
    icon_widget = g_object_new (icon_widget_type, NULL);

  if (g_type_is_a (icon_widget_type, GTK_TYPE_MENU_BUTTON))
    {
      gtk_menu_button_set_child (GTK_MENU_BUTTON (icon_widget), image);
      gtk_widget_set_can_focus (icon_widget, FALSE);
    }

  gtk_widget_set_halign (icon_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (icon_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_name (icon_widget, "fingerprint-image");

  gtk_box_append (GTK_BOX (box), icon_widget);

  gtk_widget_add_css_class (icon_widget, "circular");

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_box_append (GTK_BOX (box), label);

  gtk_widget_add_css_class (box, "fingerprint-icon");

  if (out_icon)
    *out_icon = icon_widget;

  if (out_label)
    *out_label = label;

  return box;
}

static GtkWidget *
fingerprint_menu_button (const char *icon_name,
                         const char *label_text)
{
  GtkWidget *flowbox_child;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *box;

  box = fingerprint_icon_new (icon_name, label_text, GTK_TYPE_MENU_BUTTON, NULL,
                              &button, &label);

  flowbox_child = gtk_flow_box_child_new ();
  gtk_widget_set_focus_on_click (flowbox_child, FALSE);
  gtk_widget_set_name (flowbox_child, "fingerprint-flowbox");

  gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (flowbox_child), box);

  g_object_set_data (G_OBJECT (flowbox_child), "button", button);
  g_object_set_data (G_OBJECT (flowbox_child), "icon",
                     GTK_IMAGE (gtk_menu_button_get_child (GTK_MENU_BUTTON (button))));
  g_object_set_data (G_OBJECT (flowbox_child), "label", label);
  g_object_set_data (G_OBJECT (button), "flowbox-child", flowbox_child);

  return flowbox_child;
}

static gboolean
prints_visibility_filter (GtkFlowBoxChild *child,
                          gpointer         user_data)
{
  CcFingerprintDialog *self = user_data;
  const char *finger_id;

  if (gtk_stack_get_visible_child (self->stack) != self->prints_manager)
    return FALSE;

  finger_id = g_object_get_data (G_OBJECT (child), "finger-id");

  if (!finger_id)
    return TRUE;

  if (!self->enrolled_fingers)
    return FALSE;

  return g_strv_contains ((const gchar **) self->enrolled_fingers, finger_id);
}

static GList *
get_container_children (GtkWidget *container)
{
  GtkWidget *child;
  GList *list = NULL;
    
  child = gtk_widget_get_first_child (container);
  while (child) {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      list = g_list_append (list, child);

      child = next;
  }

  return list;
}

static void
update_prints_to_add_visibility (CcFingerprintDialog *self)
{
  g_autoptr(GList) print_buttons = NULL;
  GList *l;

  print_buttons = get_container_children (GTK_WIDGET (self->add_print_popover_box));

  for (l = print_buttons; l != NULL; l = l->next)
    {
      GtkWidget *button = l->data;
      gboolean enrolled;
      const char *finger_id = g_object_get_data (G_OBJECT (button), "finger-id");

      enrolled = self->enrolled_fingers &&
                 g_strv_contains ((const gchar **) self->enrolled_fingers,
                                  finger_id);

      gtk_widget_set_visible (button, !enrolled);
    }
}

static void
update_prints_visibility (CcFingerprintDialog *self)
{
  update_prints_to_add_visibility (self);

  gtk_flow_box_invalidate_filter (self->prints_gallery);
}

static void
list_enrolled_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_auto(GStrv) enrolled_fingers = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(DialogStateRemover) state_remover = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;
  guint n_enrolled_fingers = 0;

  cc_fprintd_device_call_list_enrolled_fingers_finish (fprintd_device,
                                                       &enrolled_fingers,
                                                       res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_PRINTS_LISTING);

  gtk_widget_set_sensitive (GTK_WIDGET (self->add_print_icon), TRUE);

  if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED)
    gtk_widget_set_sensitive (GTK_WIDGET (self->prints_manager), TRUE);

  if (error)
    {
      g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

      if (!dbus_error || !g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoEnrolledPrints"))
        {
          g_autofree char *error_message = NULL;

          error_message = g_strdup_printf (_("Failed to list fingerprints: %s"),
                                           dbus_error_to_human (self, error));
          g_warning ("Listing of fingerprints on device %s failed: %s",
                     cc_fprintd_device_get_name (self->device), error->message);
          notify_error (self, error_message);
          return;
        }
    }
  else
    {
      n_enrolled_fingers = g_strv_length (enrolled_fingers);
    }

  self->enrolled_fingers = g_steal_pointer (&enrolled_fingers);
  gtk_flow_box_set_max_children_per_line (self->prints_gallery,
                                          MIN (3, n_enrolled_fingers + 1));

  update_prints_visibility (self);

  if (n_enrolled_fingers == N_VALID_FINGERS)
    gtk_widget_set_sensitive (self->add_print_icon, FALSE);

  if (n_enrolled_fingers > 0)
    gtk_widget_set_visible (GTK_WIDGET (self->delete_prints_button), TRUE);
}

static void
update_prints_store (CcFingerprintDialog *self)
{
  ActUser *user;

  g_assert_true (CC_FPRINTD_IS_DEVICE (self->device));

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_PRINTS_LISTING))
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (self->add_print_icon), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->delete_prints_button), FALSE);

  g_clear_pointer (&self->enrolled_fingers, g_strfreev);

  user = cc_fingerprint_manager_get_user (self->manager);
  cc_fprintd_device_call_list_enrolled_fingers (self->device,
                                                act_user_get_user_name (user),
                                                self->cancellable,
                                                list_enrolled_cb,
                                                self);
}

static void
delete_prints_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_delete_enrolled_fingers2_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    {
      g_autofree char *error_message = NULL;

      error_message = g_strdup_printf (_("Failed to delete saved fingerprints: %s"),
                                       dbus_error_to_human (self, error));
      g_warning ("Deletion of fingerprints on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);
    }

  update_prints_store (self);
  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
delete_enrolled_prints (CcFingerprintDialog *self)
{
  g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED);

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_DELETING))
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (self->prints_manager), FALSE);

  cc_fprintd_device_call_delete_enrolled_fingers2 (self->device,
                                                   self->cancellable,
                                                   delete_prints_cb,
                                                   self);
}

static const char *
get_finger_name (const char *finger_id)
{
  if (g_str_equal (finger_id, "left-thumb"))
    return _("Left thumb");
  if (g_str_equal (finger_id, "left-middle-finger"))
    return _("Left middle finger");
  if (g_str_equal (finger_id, "left-index-finger"))
    return _("_Left index finger");
  if (g_str_equal (finger_id, "left-ring-finger"))
    return _("Left ring finger");
  if (g_str_equal (finger_id, "left-little-finger"))
    return _("Left little finger");
  if (g_str_equal (finger_id, "right-thumb"))
    return _("Right thumb");
  if (g_str_equal (finger_id, "right-middle-finger"))
    return _("Right middle finger");
  if (g_str_equal (finger_id, "right-index-finger"))
    return _("_Right index finger");
  if (g_str_equal (finger_id, "right-ring-finger"))
    return _("Right ring finger");
  if (g_str_equal (finger_id, "right-little-finger"))
    return _("Right little finger");

  g_return_val_if_reached (_("Unknown Finger"));
}

static gboolean
have_multiple_devices (CcFingerprintDialog *self)
{
  g_autoptr(GList) devices_rows = NULL;

  devices_rows = get_container_children (GTK_WIDGET (self->devices_list));

  return devices_rows && devices_rows->next;
}

static void
set_enroll_result_message (CcFingerprintDialog *self,
                           EnrollState          enroll_state,
                           const char          *message)
{
  const char *icon_name;
  guint i;

  g_return_if_fail (enroll_state >= 0 && enroll_state < N_ENROLL_STATES);

  switch (enroll_state)
    {
      case ENROLL_STATE_WARNING:
      case ENROLL_STATE_ERROR:
        icon_name = "fingerprint-detection-warning-symbolic";
        break;
      case ENROLL_STATE_COMPLETED:
        icon_name = "fingerprint-detection-complete-symbolic";
        break;
      default:
        icon_name = "fingerprint-detection-symbolic";
    }

  for (i = 0; i < N_ENROLL_STATES; ++i)
    gtk_widget_remove_css_class (self->enroll_result_icon, ENROLL_STATE_CLASSES[i]);

  gtk_widget_add_css_class (self->enroll_result_icon, ENROLL_STATE_CLASSES[enroll_state]);

  gtk_image_set_from_icon_name (self->enroll_result_image, icon_name);
  gtk_label_set_label (self->enroll_result_message, message);
}

static gboolean
stage_passed_timeout_cb (gpointer user_data)
{
  CcFingerprintDialog *self = user_data;
  const char *current_message;

  current_message = gtk_label_get_label (self->enroll_result_message);
  set_enroll_result_message (self, ENROLL_STATE_NORMAL, current_message);
  self->enroll_stage_passed_id = 0;

  return FALSE;
}

static void
handle_enroll_signal (CcFingerprintDialog *self,
                      const char          *result,
                      gboolean             done)
{
  gboolean completed;

  g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

  g_debug ("Device enroll result message: %s, done: %d", result, done);

  completed = g_str_equal (result, "enroll-completed");
  g_clear_handle_id (&self->enroll_stage_passed_id, g_source_remove);

  if (g_str_equal (result, "enroll-stage-passed") || completed)
    {
      guint enroll_stages;

      enroll_stages = cc_fprintd_device_get_num_enroll_stages (self->device);

      self->enroll_stages_passed++;

      if (enroll_stages > 0)
        self->enroll_progress =
          MIN (1.0f, self->enroll_stages_passed / (double) enroll_stages);
      else
        g_warning ("The device %s requires an invalid number of enroll stages (%u)",
                   cc_fprintd_device_get_name (self->device), enroll_stages);

      g_debug ("Enroll state passed, %u/%u (%.2f%%)",
               self->enroll_stages_passed, (guint) enroll_stages,
               self->enroll_progress);

      if (!completed)
        {
          set_enroll_result_message (self, ENROLL_STATE_SUCCESS, NULL);

          self->enroll_stage_passed_id =
            g_timeout_add (750, stage_passed_timeout_cb, self);
        }
      else
        {
          if (!G_APPROX_VALUE (self->enroll_progress, 1.0f, FLT_EPSILON))
            {
              g_warning ("Device marked enroll as completed, but progress is at %.2f",
                         self->enroll_progress);
              self->enroll_progress = 1.0f;
            }
        }
    }
  else if (!done)
    {
      const char *scan_type;
      const char *message;
      gboolean is_swipe;

      scan_type = cc_fprintd_device_get_scan_type (self->device);
      is_swipe = g_str_equal (scan_type, "swipe");

      message = enroll_result_str_to_msg (result, is_swipe);
      set_enroll_result_message (self, ENROLL_STATE_RETRY, message);

      self->enroll_stage_passed_id =
        g_timeout_add (850, stage_passed_timeout_cb, self);
    }

  if (done)
    {
      if (completed)
        {
          /* TRANSLATORS: This is the message shown when the fingerprint
           * enrollment has been completed successfully */
          set_enroll_result_message (self, ENROLL_STATE_COMPLETED,
                                     C_("Fingerprint enroll state", "Complete"));
          gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), FALSE);
          gtk_widget_set_sensitive (GTK_WIDGET (self->done_button), TRUE);
          gtk_widget_grab_focus (GTK_WIDGET (self->done_button));
        }
      else
        {
          const char *message;

          if (g_str_equal (result, "enroll-disconnected"))
            {
              message = _("Fingerprint device disconnected");
              remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED |
                                         DIALOG_STATE_DEVICE_ENROLLING);
            }
          else if (g_str_equal (result, "enroll-data-full"))
            {
              message = _("Fingerprint device storage is full");
            }
          else if (g_str_equal (result, "enroll-duplicate"))
            {
              message = _("Fingerprint is duplicate");
            }
          else
            {
              message = _("Failed to enroll new fingerprint");
            }

          set_enroll_result_message (self, ENROLL_STATE_WARNING, message);
        }
    }
}

static void
enroll_start_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(DialogStateRemover) state_remover = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_enroll_start_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_ENROLL_STARTING);

  if (error)
    {
      g_autofree char *error_message = NULL;

      remove_dialog_state (self, DIALOG_STATE_DEVICE_ENROLLING);

      error_message = g_strdup_printf (_("Failed to start enrollment: %s"),
                                       dbus_error_to_human (self, error));
      g_warning ("Enrollment on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);

      set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                 C_("Fingerprint enroll state",
                                    "Failed to enroll new fingerprint"));
      gtk_widget_set_sensitive (self->enrollment_view, FALSE);

      return;
    }
}

static void
enroll_stop_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(DialogStateRemover) state_remover = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_enroll_stop_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_ENROLLING |
                                            DIALOG_STATE_DEVICE_ENROLL_STOPPING);
  gtk_widget_set_sensitive (self->enrollment_view, TRUE);
  gtk_stack_set_visible_child (self->stack, self->prints_manager);

  if (error)
    {
      g_autofree char *error_message = NULL;

      error_message = g_strdup_printf (_("Failed to stop enrollment: %s"),
                                       dbus_error_to_human (self, error));
      g_warning ("Stopping enrollment on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);

      return;
    }

  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
enroll_stop (CcFingerprintDialog *self)
{
  g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_ENROLL_STOPPING))
    return;

  gtk_widget_set_sensitive (self->enrollment_view, FALSE);
  cc_fprintd_device_call_enroll_stop (self->device, self->cancellable,
                                      enroll_stop_cb, self);
}

static char *
get_enrollment_string (CcFingerprintDialog *self,
                       const char          *finger_id)
{
  char *ret;
  const char *scan_type;
  const char *device_name;
  gboolean is_swipe;

  device_name = NULL;
  scan_type = cc_fprintd_device_get_scan_type (self->device);
  is_swipe = g_str_equal (scan_type, "swipe");

  if (have_multiple_devices (self))
    device_name = cc_fprintd_device_get_name (self->device);

  ret = finger_str_to_msg (finger_id, device_name, is_swipe);

  if (ret)
    return ret;

  return g_strdup (_("Repeatedly lift and place your finger on the reader to enroll your fingerprint"));
}

static void
enroll_finger (CcFingerprintDialog *self,
               const char          *finger_id)
{
  g_auto(GStrv) tmp_finger_name = NULL;
  g_autofree char *finger_name = NULL;
  g_autofree char *enroll_message = NULL;

  g_return_if_fail (finger_id);

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_ENROLLING |
                               DIALOG_STATE_DEVICE_ENROLL_STARTING))
    return;

  self->enroll_progress = 0;
  self->enroll_stages_passed = 0;

  g_debug ("Enrolling finger %s", finger_id);

  enroll_message = get_enrollment_string (self, finger_id);
  tmp_finger_name = g_strsplit (get_finger_name (finger_id), "_", -1);
  finger_name = g_strjoinv ("", tmp_finger_name);

  set_enroll_result_message (self, ENROLL_STATE_NORMAL, NULL);
  gtk_stack_set_visible_child (self->stack, self->enrollment_view);
  gtk_label_set_label (self->enroll_message, enroll_message);
  gtk_editable_set_text (GTK_EDITABLE (self->enroll_print_entry), finger_name);

  cc_fprintd_device_call_enroll_start (self->device, finger_id, self->cancellable,
                                       enroll_start_cb, self);
}

static void
populate_enrollment_view (CcFingerprintDialog *self)
{
  self->enroll_result_icon =
    fingerprint_icon_new ("fingerprint-detection-symbolic",
                          NULL,
                          GTK_TYPE_IMAGE,
                          &self->enroll_progress,
                          (GtkWidget **) &self->enroll_result_image,
                          (GtkWidget **) &self->enroll_result_message);

  gtk_box_prepend (GTK_BOX (self->enroll_print_bin), self->enroll_result_icon);

  gtk_widget_add_css_class (self->enroll_result_icon,  "enroll-status");
}

static void
on_print_activated_cb (CcFingerprintDialog *self,
                       GtkFlowBoxChild     *child)
{
  GtkWidget *selected_button;

  selected_button = g_object_get_data (G_OBJECT (child), "button");
  g_signal_emit_by_name (GTK_MENU_BUTTON (selected_button), "activate");
}

static void
on_enroll_cb (CcFingerprintDialog *self,
              GtkMenuButton       *button)
{
  const char *finger_id;

  finger_id = g_object_get_data (G_OBJECT (button), "finger-id");
  enroll_finger (self, finger_id);
}

static void
populate_add_print_popover (CcFingerprintDialog *self)
{
  guint i;

  for (i = 0; i < N_VALID_FINGERS; ++i)
    {
      GtkWidget *finger_item;

      finger_item = gtk_button_new ();
      gtk_button_set_label (GTK_BUTTON (finger_item), get_finger_name (FINGER_IDS[i]));
      gtk_button_set_use_underline (GTK_BUTTON (finger_item), TRUE);
      g_object_set_data (G_OBJECT (finger_item), "finger-id", (gpointer) FINGER_IDS[i]);
      gtk_box_prepend (GTK_BOX (self->add_print_popover_box), finger_item);

      g_signal_connect_object (finger_item, "clicked", G_CALLBACK (on_enroll_cb),
                               self, G_CONNECT_SWAPPED);
    }
}

static void
populate_prints_gallery (CcFingerprintDialog *self)
{
  const char *add_print_label;
  GtkWidget *button;
  guint i;

  g_return_if_fail (!GTK_IS_WIDGET (self->add_print_icon));

  for (i = 0; i < N_VALID_FINGERS; ++i)
    {
      GtkWidget *flowbox_child;
      GtkWidget *popover;
      GtkWidget *reenroll_button;

      flowbox_child = fingerprint_menu_button ("fingerprint-detection-symbolic",
                                               get_finger_name (FINGER_IDS[i]));

      button = g_object_get_data (G_OBJECT (flowbox_child), "button");

      popover = gtk_popover_new ();
      reenroll_button = gtk_button_new ();
      gtk_button_set_use_underline (GTK_BUTTON (reenroll_button), TRUE);
      gtk_button_set_label (GTK_BUTTON (reenroll_button), _("_Re-enroll this finger…"));
      g_object_set_data (G_OBJECT (reenroll_button), "finger-id",
                         (gpointer) FINGER_IDS[i]);
      g_signal_connect_object (reenroll_button, "clicked", G_CALLBACK (on_enroll_cb), self, G_CONNECT_SWAPPED);
      gtk_popover_set_child (GTK_POPOVER (popover), reenroll_button);

      gtk_menu_button_set_popover (GTK_MENU_BUTTON (button),
                                   popover);
      g_object_set_data (G_OBJECT (flowbox_child), "finger-id",
                         (gpointer) FINGER_IDS[i]);

      gtk_flow_box_insert (self->prints_gallery, flowbox_child, i);
    }

  /* TRANSLATORS: This is the label for the button to enroll a new finger */
  add_print_label = _("Scan new fingerprint");
  self->add_print_icon = fingerprint_menu_button ("list-add-symbolic",
                                                  add_print_label);
  gtk_widget_add_css_class (self->add_print_icon, "fingerprint-print-add");

  populate_add_print_popover (self);
  button = g_object_get_data (G_OBJECT (self->add_print_icon), "button");
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button),
                               GTK_WIDGET (self->add_print_popover));

  gtk_flow_box_insert (self->prints_gallery, self->add_print_icon, -1);
  gtk_flow_box_set_max_children_per_line (self->prints_gallery, 1);

  gtk_flow_box_set_filter_func (self->prints_gallery, prints_visibility_filter,
                                self, NULL);

  update_prints_visibility (self);
}

static void
release_device_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_release_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    {
      g_autofree char *error_message = NULL;

      error_message = g_strdup_printf (_("Failed to release fingerprint device %s: %s"),
                                       cc_fprintd_device_get_name (fprintd_device),
                                       dbus_error_to_human (self, error));
      g_warning ("Releasing device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);

      notify_error (self, error_message);
      return;
    }

  remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED);
}

static void
release_device (CcFingerprintDialog *self)
{
  if (!self->device || !(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
    return;

  disconnect_device_signals (self);

  cc_fprintd_device_call_release (self->device,
                                  self->cancellable,
                                  release_device_cb,
                                  self);
}

static void
on_device_signal (CcFingerprintDialog *self,
                  gchar               *sender_name,
                  gchar               *signal_name,
                  GVariant            *parameters,
                  gpointer             user_data)
{
  if (g_str_equal (signal_name, "EnrollStatus"))
    {
      const char *result;
      gboolean done;

      if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sb)")))
        {
          g_warning ("Unexpected enroll parameters type %s",
                     g_variant_get_type_string (parameters));
          return;
        }

      g_variant_get (parameters, "(&sb)", &result, &done);
      handle_enroll_signal (self, result, done);
    }
}

static void claim_device (CcFingerprintDialog *self);

static void
on_device_owner_changed (CcFprintdDevice     *device,
                         GParamSpec          *spec,
                         CcFingerprintDialog *self)
{
  g_autofree char *name_owner = NULL;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (device));

  if (!name_owner)
    {
      if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED)
        {
          disconnect_device_signals (self);

          if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING)
            {
              set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                         C_("Fingerprint enroll state",
                                            "Problem Reading Device"));
            }

          remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED);
          claim_device (self);
        }
    }
}

static void
claim_device_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(DialogStateRemover) state_remover = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_claim_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_CLAIMING);

  if (error)
    {
      g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);
      g_autofree char *error_message = NULL;

      if (dbus_error && g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.AlreadyInUse") &&
          (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
         return;

      error_message = g_strdup_printf (_("Failed to claim fingerprint device %s: %s"),
                                       cc_fprintd_device_get_name (self->device),
                                       dbus_error_to_human (self, error));
      g_warning ("Claiming device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);
      return;
    }

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED))
    return;

  gtk_widget_set_sensitive (self->prints_manager, TRUE);
  self->device_signal_id = g_signal_connect_object (self->device, "g-signal",
                                                    G_CALLBACK (on_device_signal),
                                                    self, G_CONNECT_SWAPPED);
  self->device_name_owner_id = g_signal_connect_object (self->device, "notify::g-name-owner",
                                                        G_CALLBACK (on_device_owner_changed),
                                                        self, 0);
}

static void
claim_device (CcFingerprintDialog *self)
{
  ActUser *user;

  g_return_if_fail (!(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED));

  if (!add_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMING))
    return;

  user = cc_fingerprint_manager_get_user (self->manager);
  gtk_widget_set_sensitive (self->prints_manager, FALSE);

  cc_fprintd_device_call_claim (self->device,
                                act_user_get_user_name (user),
                                self->cancellable,
                                claim_device_cb,
                                self);
}

static void
on_stack_child_changed (CcFingerprintDialog *self)
{
  GtkWidget *visible_child = gtk_stack_get_visible_child (self->stack);

  g_debug ("Fingerprint dialog child changed: %s",
           gtk_stack_get_visible_child_name (self->stack));

  gtk_widget_set_visible (GTK_WIDGET (self->back_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->done_button), FALSE);

  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (self->titlebar), TRUE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->titlebar), TRUE);
  gtk_flow_box_invalidate_filter (self->prints_gallery);

  if (visible_child == self->prints_manager)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->back_button),
                              have_multiple_devices (self));
      notify_error (self, NULL);
      update_prints_store (self);

      if (!(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
        claim_device (self);
    }
  else if (visible_child == self->enrollment_view)
    {
      adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (self->titlebar), FALSE);
      adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->titlebar), FALSE);

      gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), TRUE);

      gtk_widget_set_visible (GTK_WIDGET (self->done_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->done_button), FALSE);
    }
  else
    {
      release_device (self);
      g_clear_object (&self->device);
    }
}

static void
cc_fingerprint_dialog_init (CcFingerprintDialog *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/system/users/cc-fingerprint-dialog.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  on_stack_child_changed (self);
  g_signal_connect_object (self->stack, "notify::visible-child",
                           G_CALLBACK (on_stack_child_changed), self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (self->stack, "visible-child-name",
                          self->title, "label", G_BINDING_SYNC_CREATE);

  populate_prints_gallery (self);
  populate_enrollment_view (self);
}

static void
select_device_row (CcFingerprintDialog *self,
                   GtkListBoxRow       *row,
                   GtkListBox          *listbox)
{
  CcFprintdDevice *device = g_object_get_data (G_OBJECT (row), "device");

  g_return_if_fail (CC_FPRINTD_DEVICE (device));

  g_set_object (&self->device, device);
  gtk_stack_set_visible_child (self->stack, self->prints_manager);
}

static void
on_devices_list (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autolist (CcFprintdDevice) fprintd_devices = NULL;
  g_autoptr(DialogStateRemover) state_remover = NULL;
  g_autoptr(GError) error = NULL;
  CcFingerprintManager *fingerprint_manager = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);

  fprintd_devices = cc_fingerprint_manager_get_devices_finish (fingerprint_manager,
                                                               res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  state_remover = auto_state_remover (self, DIALOG_STATE_DEVICES_LISTING);

  if (fprintd_devices == NULL)
    {
      if (error)
        {
          g_autofree char *error_message = NULL;

          error_message = g_strdup_printf (_("Failed to get fingerprint devices: %s"),
                                           dbus_error_to_human (self, error));
          g_warning ("Retrieving fingerprint devices failed: %s", error->message);
          notify_error (self, error_message);
        }

      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->no_devices_found));
    }
  else if (fprintd_devices->next == NULL)
    {
      /* We have just one device... Skip devices selection */
      self->device = g_object_ref (fprintd_devices->data);
      gtk_stack_set_visible_child (self->stack, self->prints_manager);
    }
  else
    {
      GList *l;

      for (l = fprintd_devices; l; l = l->next)
        {
          CcFprintdDevice *device = l->data;
          CcListRow *device_row;

          device_row = g_object_new (CC_TYPE_LIST_ROW,
                                     "visible", TRUE,
                                     "icon-name", "go-next-symbolic",
                                     "title", cc_fprintd_device_get_name (device),
                                     NULL);

          gtk_list_box_insert (self->devices_list, GTK_WIDGET (device_row), -1);
          g_object_set_data_full (G_OBJECT (device_row), "device",
                                  g_object_ref (device), g_object_unref);
        }

      gtk_stack_set_visible_child (self->stack, self->device_selector);
    }
}

static void
cc_fingerprint_dialog_constructed (GObject *object)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  bindtextdomain ("fprintd", GNOMELOCALEDIR);
  bind_textdomain_codeset ("fprintd", "UTF-8");

  add_dialog_state (self, DIALOG_STATE_DEVICES_LISTING);
  cc_fingerprint_manager_get_devices (self->manager, self->cancellable,
                                      on_devices_list, self);
}

static void
back_button_clicked_cb (CcFingerprintDialog *self)
{
  if (gtk_stack_get_visible_child (self->stack) == self->prints_manager)
    {
      notify_error (self, NULL);
      gtk_stack_set_visible_child (self->stack, self->device_selector);
      return;
    }

  g_return_if_reached ();
}

static void
confirm_deletion_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_set_visible (self->delete_confirmation_infobar, FALSE);
  delete_enrolled_prints (self);
}

static void
cancel_deletion_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_set_sensitive (self->prints_manager, TRUE);
  gtk_widget_set_visible (self->delete_confirmation_infobar, FALSE);
}

static void
delete_prints_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_set_sensitive (self->prints_manager, FALSE);
  gtk_widget_set_visible (self->delete_confirmation_infobar, TRUE);
}

static void
cancel_button_clicked_cb (CcFingerprintDialog *self)
{
  if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING)
    {
      g_cancellable_cancel (self->cancellable);
      g_set_object (&self->cancellable, g_cancellable_new ());

      g_debug ("Cancelling enroll operation");
      enroll_stop (self);
    }
  else
    {
      gtk_stack_set_visible_child (self->stack, self->prints_manager);
    }
}

static void
done_button_clicked_cb (CcFingerprintDialog *self)
{
  g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

  g_debug ("Completing enroll operation");
  enroll_stop (self);
}

static gboolean
cc_fingerprint_dialog_close_request (GtkWindow *window)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (window);

  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);

  g_clear_handle_id (&self->enroll_stage_passed_id, g_source_remove);

  if (self->device && (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
    {
      disconnect_device_signals (self);

      if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING)
        cc_fprintd_device_call_enroll_stop_sync (self->device, NULL, NULL);
      cc_fprintd_device_call_release (self->device, NULL, NULL, NULL);
    }

  g_clear_object (&self->manager);
  g_clear_object (&self->device);
  g_clear_pointer (&self->enrolled_fingers, g_strfreev);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  return GTK_WINDOW_CLASS (cc_fingerprint_dialog_parent_class)->close_request (window);
}

static void
cc_fingerprint_dialog_class_init (CcFingerprintDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class,
    "/org/gnome/control-center/system/users/cc-fingerprint-dialog.ui");

  object_class->constructed = cc_fingerprint_dialog_constructed;
  object_class->get_property = cc_fingerprint_dialog_get_property;
  object_class->set_property = cc_fingerprint_dialog_set_property;

  window_class->close_request = cc_fingerprint_dialog_close_request;

  properties[PROP_MANAGER] =
    g_param_spec_object ("fingerprint-manager",
                         "FingerprintManager",
                         "The CC fingerprint manager",
                         CC_TYPE_FINGERPRINT_MANAGER,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, add_print_popover);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, add_print_popover_box);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, delete_confirmation_infobar);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, delete_prints_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, device_selector);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, devices_list);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_message);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_print_bin);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_print_entry);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enrollment_view);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, error_infobar);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, infobar_error);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, no_devices_found);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_gallery);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_manager);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, title);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, titlebar);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_deletion_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, confirm_deletion_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, delete_prints_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, done_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_print_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, select_device_row);
}
