/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdesktop-enums.h>

#include <cups/cups.h>

#include "pp-jobs-dialog.h"
#include "pp-utils.h"
#include "pp-job.h"
#include "pp-job-row.h"
#include "pp-cups.h"
#include "pp-printer.h"

#define EMPTY_TEXT "\xe2\x80\x94"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

struct _PpJobsDialog {
  AdwWindow          parent_instance;

  GtkButton         *authenticate_button;
  GtkMenuButton     *authenticate_jobs_button;
  GtkLabel          *authenticate_jobs_label;
  GtkInfoBar        *authentication_infobar;
  GtkLabel          *authentication_label;
  GtkEntry          *domain_entry;
  GtkLabel          *domain_label;
  GtkButton         *jobs_clear_all_button;
  GtkListBox        *jobs_listbox;
  GtkScrolledWindow *list_jobs_page;
  GtkBox            *no_jobs_page;
  GtkEntry          *password_entry;
  GtkLabel          *password_label;
  GtkStack          *stack;
  GListStore        *store;
  GtkEntry          *username_entry;
  GtkLabel          *username_label;

  gchar *printer_name;

  gchar    **actual_auth_info_required;
  gboolean   jobs_filled;
  gboolean   pop_up_authentication_popup;
  gint       max_priority;

  GCancellable *get_jobs_cancellable;
};

G_DEFINE_TYPE (PpJobsDialog, pp_jobs_dialog, ADW_TYPE_WINDOW)

static gboolean
is_info_required (PpJobsDialog *self,
                  const gchar  *info)
{
  gint i;

  if (self->actual_auth_info_required == NULL)
    return FALSE;

  for (i = 0; self->actual_auth_info_required[i] != NULL; i++)
    if (g_strcmp0 (self->actual_auth_info_required[i], info) == 0)
      return TRUE;

  return FALSE;
}

static gboolean
is_domain_required (PpJobsDialog *self)
{
  return is_info_required (self, "domain");
}

static gboolean
is_username_required (PpJobsDialog *self)
{
  return is_info_required (self, "username");
}

static gboolean
is_password_required (PpJobsDialog *self)
{
  return is_info_required (self, "password");
}

static gboolean
auth_popup_filled (PpJobsDialog *self)
{
  gboolean domain_required;
  gboolean username_required;
  gboolean password_required;
  guint16  domain_length;
  guint16  username_length;
  guint16  password_length;

  domain_required = is_domain_required (self);
  username_required = is_username_required (self);
  password_required = is_password_required (self);

  domain_length = gtk_entry_get_text_length (self->domain_entry);
  username_length = gtk_entry_get_text_length (self->username_entry);
  password_length = gtk_entry_get_text_length (self->password_entry);

  return (!domain_required || domain_length > 0) &&
         (!username_required || username_length > 0) &&
         (!password_required || password_length > 0);
}

static void
auth_entries_changed (PpJobsDialog *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->authenticate_button), auth_popup_filled (self));
}

static void
auth_entries_activated (PpJobsDialog *self)
{
  if (auth_popup_filled (self))
    g_signal_emit_by_name (self->authenticate_button, "activate");
}

static void
authenticate_popover_update (PpJobsDialog *self)
{
  gboolean   domain_required;
  gboolean   username_required;
  gboolean   password_required;

  domain_required = is_domain_required (self);
  username_required = is_username_required (self);
  password_required = is_password_required (self);

  gtk_widget_set_visible (GTK_WIDGET (self->domain_label), domain_required);
  gtk_widget_set_visible (GTK_WIDGET (self->domain_entry), domain_required);
  if (domain_required)
    gtk_editable_set_text (GTK_EDITABLE (self->domain_entry), "");

  gtk_widget_set_visible (GTK_WIDGET (self->username_label), username_required);
  gtk_widget_set_visible (GTK_WIDGET (self->username_entry), username_required);
  if (username_required)
    gtk_editable_set_text (GTK_EDITABLE (self->username_entry), cupsUser ());

  gtk_widget_set_visible (GTK_WIDGET (self->password_label), password_required);
  gtk_widget_set_visible (GTK_WIDGET (self->password_entry), password_required);
  if (password_required)
    gtk_editable_set_text (GTK_EDITABLE (self->password_entry), "");

  gtk_widget_set_sensitive (GTK_WIDGET (self->authenticate_button), FALSE);
}

static void
pp_job_update_cb (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  PpJobsDialog     *self = user_data;
  gboolean          result;
  g_autoptr(GError) error = NULL;
  PpJob            *job = PP_JOB (source_object);

  result = pp_job_set_priority_finish (job, res, &error);
  if (result)
    {
      pp_jobs_dialog_update (self);
    }
  else if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not set job priority: %s", error->message);
        }
    }
}

static void
on_priority_changed (PpJobsDialog *self,
                     PpJobRow     *job_row)
{
  PpJob *job;

  job = pp_job_row_get_job (job_row);
  pp_job_set_priority_async (job, ++self->max_priority, NULL, pp_job_update_cb, self);
}

static GtkWidget *
create_listbox_row (gpointer item,
                    gpointer user_data)
{
  PpJobsDialog *self = user_data;
  PpJobRow *job_row;

  job_row = pp_job_row_new (PP_JOB (item));

  g_signal_connect_swapped (job_row,
                            "priority-changed",
                            G_CALLBACK (on_priority_changed),
                            self);

  return GTK_WIDGET (job_row);
}

static void
pop_up_authentication_popup (PpJobsDialog *self)
{
  if (self->actual_auth_info_required != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->authenticate_jobs_button), TRUE);
}

static void
update_jobs_list_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  PpJobsDialog        *self = user_data;
  PpPrinter           *printer = PP_PRINTER (source_object);
  g_autoptr(GError)    error = NULL;
  g_autoptr(GPtrArray) jobs;
  PpJob               *job;
  gint                 num_of_auth_jobs = 0;
  gint                 job_priority;
  guint                state;
  guint                i;
  gint                 current_max_value = 1;
  gint                 first_unprocessed_job = -1;

  g_list_store_remove_all (self->store);

  jobs = pp_printer_get_jobs_finish (printer, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get jobs: %s", error->message);
        }

      return;
    }

  if (jobs->len > 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->jobs_clear_all_button), TRUE);
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->list_jobs_page));
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->jobs_clear_all_button), FALSE);
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->no_jobs_page));
    }

  for (i = 0; i < jobs->len; i++)
  {
    job = PP_JOB (g_ptr_array_index (jobs, i));
    state = pp_job_get_state (job);

    if (state == IPP_JOB_PENDING || state == IPP_JOB_HELD)
      {
        if (first_unprocessed_job == -1)
          {
            first_unprocessed_job = i;
            break;
          }
      }
  }

  for (i = 0; i < jobs->len; i++)
    {
      job = PP_JOB (g_ptr_array_index (jobs, i));
      job_priority = pp_job_get_priority (job);
      pp_job_priority_set_sensitive (job, (pp_job_get_state (job) == IPP_JOB_PENDING ||
                                     pp_job_get_state (job) == IPP_JOB_HELD) &&
                                     i > first_unprocessed_job);

      if (job_priority >= current_max_value && job_priority != 100)
        current_max_value = job_priority;

      g_list_store_append (self->store, g_object_ref (job));

      if (pp_job_get_auth_info_required (job) != NULL)
        {
          num_of_auth_jobs++;

          if (self->actual_auth_info_required == NULL)
            self->actual_auth_info_required = g_strdupv (pp_job_get_auth_info_required (job));
        }
    }
  self->max_priority = current_max_value;
  if (num_of_auth_jobs > 0)
    {
      g_autofree gchar *text = NULL;

      /* Translators: This label shows how many jobs of this printer needs to be authenticated to be printed. */
      text = g_strdup_printf (ngettext ("%u Job Requires Authentication", "%u Jobs Require Authentication", num_of_auth_jobs), num_of_auth_jobs);
      gtk_label_set_text (self->authenticate_jobs_label, text);

      gtk_widget_set_visible (GTK_WIDGET (self->authentication_infobar), TRUE);
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->authentication_infobar), FALSE);
    }

  authenticate_popover_update (self);

  g_clear_object (&self->get_jobs_cancellable);

  if (!self->jobs_filled)
    {
      if (self->pop_up_authentication_popup)
        {
          pop_up_authentication_popup (self);
          self->pop_up_authentication_popup = FALSE;
        }

      self->jobs_filled = TRUE;
    }
}

static void
update_jobs_list (PpJobsDialog *self)
{
  g_autoptr(PpPrinter) printer = NULL;

  if (self->printer_name != NULL)
    {
      g_cancellable_cancel (self->get_jobs_cancellable);
      g_clear_object (&self->get_jobs_cancellable);

      self->get_jobs_cancellable = g_cancellable_new ();

      printer = pp_printer_new (self->printer_name);
      pp_printer_get_jobs_async (printer,
                                 TRUE,
                                 CUPS_WHICHJOBS_ACTIVE,
                                 self->get_jobs_cancellable,
                                 update_jobs_list_cb,
                                 self);
    }
}

static void
on_clear_all_button_clicked (PpJobsDialog *self)
{
  guint num_items;
  guint i;

  num_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));

  for (i = 0; i < num_items; i++)
    {
      PpJob *job = PP_JOB (g_list_model_get_item (G_LIST_MODEL (self->store), i));

      pp_job_cancel_purge_async (job, FALSE);
    }
}

static void
pp_job_authenticate_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  PpJobsDialog     *self = user_data;
  gboolean          result;
  g_autoptr(GError) error = NULL;
  PpJob            *job = PP_JOB (source_object);

  result = pp_job_authenticate_finish (job, res, &error);
  if (result)
    {
      pp_jobs_dialog_update (self);
    }
  else if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not authenticate job: %s", error->message);
        }
    }
}

static void
authenticate_button_clicked (PpJobsDialog *self)
{
  PpJob        *job;
  gchar       **auth_info;
  guint         num_items;
  gint          i;

  auth_info = g_new0 (gchar *, g_strv_length (self->actual_auth_info_required) + 1);
  for (i = 0; self->actual_auth_info_required[i] != NULL; i++)
    {
      if (g_strcmp0 (self->actual_auth_info_required[i], "domain") == 0)
        auth_info[i] = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->domain_entry)));
      else if (g_strcmp0 (self->actual_auth_info_required[i], "username") == 0)
        auth_info[i] = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->username_entry)));
      else if (g_strcmp0 (self->actual_auth_info_required[i], "password") == 0)
        auth_info[i] = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->password_entry)));
    }

  num_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));
  for (i = 0; i < num_items; i++)
    {
      job = PP_JOB (g_list_model_get_item (G_LIST_MODEL (self->store), i));

      if (pp_job_get_auth_info_required (job) != NULL)
        {
          pp_job_authenticate_async (job, auth_info, NULL, pp_job_authenticate_cb, self);
        }
    }

  g_strfreev (auth_info);
}

PpJobsDialog *
pp_jobs_dialog_new (const gchar *printer_name)
{
  PpJobsDialog *self;
  g_autofree gchar *text = NULL;
  g_autofree gchar *title = NULL;

  self = g_object_new (PP_TYPE_JOBS_DIALOG, NULL);

  self->printer_name = g_strdup (printer_name);
  self->actual_auth_info_required = NULL;
  self->jobs_filled = FALSE;
  self->pop_up_authentication_popup = FALSE;

  /* Translators: This is the printer name for which we are showing the active jobs */
  title = g_strdup_printf (C_("Printer jobs dialog title", "%s — Active Jobs"), printer_name);
  gtk_window_set_title (GTK_WINDOW (self), title);

  /* Translators: The printer needs authentication info to print. */
  text = g_strdup_printf (_("Enter credentials to print from %s"), printer_name);
  gtk_label_set_text (self->authentication_label, text);

  self->store = g_list_store_new (pp_job_get_type ());
  gtk_list_box_bind_model (self->jobs_listbox, G_LIST_MODEL (self->store),
                           create_listbox_row, self, NULL);

  update_jobs_list (self);

  return self;
}

void
pp_jobs_dialog_update (PpJobsDialog *self)
{
  update_jobs_list (self);
}

void
pp_jobs_dialog_authenticate_jobs (PpJobsDialog *self)
{
  if (self->jobs_filled)
    pop_up_authentication_popup (self);
  else
    self->pop_up_authentication_popup = TRUE;
}

static void
pp_jobs_dialog_init (PpJobsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

static void
pp_jobs_dialog_dispose (GObject *object)
{
  PpJobsDialog *self = PP_JOBS_DIALOG (object);

  g_cancellable_cancel (self->get_jobs_cancellable);
  g_clear_object (&self->get_jobs_cancellable);
  g_clear_pointer (&self->actual_auth_info_required, g_strfreev);
  g_clear_pointer (&self->printer_name, g_free);

  G_OBJECT_CLASS (pp_jobs_dialog_parent_class)->dispose (object);
}

static void
pp_jobs_dialog_class_init (PpJobsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/pp-jobs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, authenticate_button);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, authenticate_jobs_button);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, authenticate_jobs_label);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, authentication_infobar);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, authentication_label);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, domain_entry);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, domain_label);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, jobs_clear_all_button);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, jobs_listbox);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, list_jobs_page);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, no_jobs_page);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, password_label);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, username_entry);
  gtk_widget_class_bind_template_child (widget_class, PpJobsDialog, username_label);

  gtk_widget_class_bind_template_callback (widget_class, authenticate_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_all_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, auth_entries_activated);
  gtk_widget_class_bind_template_callback (widget_class, auth_entries_changed);

  object_class->dispose = pp_jobs_dialog_dispose;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}
