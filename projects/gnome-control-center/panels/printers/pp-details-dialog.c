/*
 *
 * Copyright 2016  Red Hat, Inc,
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
 * Author: Felipe Borges <feborges@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-details-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-printer.h"
#include "pp-utils.h"

struct _PpDetailsDialog {
  AdwWindow     parent_instance;

  GtkBox       *driver_buttons;
  GtkBox       *loading_box;
  GtkLabel     *printer_address_label;
  GtkRevealer  *print_name_hint_revealer;
  GtkEntry     *printer_location_entry;
  GtkLabel     *printer_model_label;
  GtkStack     *printer_model_stack;
  GtkEntry     *printer_name_entry;
  GtkButton    *search_for_drivers_button;

  gchar        *printer_name;
  gchar        *ppd_file_name;
  PPDList      *all_ppds_list;
  GCancellable *cancellable;

  /* Dialogs */
  PpPPDSelectionDialog *pp_ppd_selection_dialog;
};

G_DEFINE_TYPE (PpDetailsDialog, pp_details_dialog, ADW_TYPE_WINDOW)

static void
printer_name_changed (PpDetailsDialog *self)
{
  const gchar *name;
  g_autofree gchar *title = NULL;

  name = pp_details_dialog_get_printer_name (self);

  if (printer_name_is_valid (name)){
    /* Translators: This is the title of the dialog. %s is the printer name. */
    title = g_strdup_printf (_("%s Details"), name);
    gtk_revealer_set_reveal_child (self->print_name_hint_revealer, FALSE);
    gtk_window_set_title (GTK_WINDOW (self), title);
  } else {
    gtk_revealer_set_reveal_child (self->print_name_hint_revealer, TRUE);
  }
}

static void set_ppd_cb (const gchar *printer_name, gboolean success, gpointer user_data);

static void
get_ppd_names_cb (PPDName     **names,
                  const gchar  *printer_name,
                  gboolean      cancelled,
                  gpointer      user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  if (!cancelled)
    {
      if (names != NULL)
        {
          gtk_label_set_text (self->printer_model_label, names[0]->ppd_display_name);
          printer_set_ppd_async (printer_name,
                                 names[0]->ppd_name,
                                 self->cancellable,
                                 set_ppd_cb,
                                 self);
        }
      else
        {
          gtk_label_set_text (self->printer_model_label, _("No suitable driver found"));
        }

      gtk_stack_set_visible_child (self->printer_model_stack, GTK_WIDGET (self->printer_model_label));
    }
}

static void
search_for_drivers (PpDetailsDialog *self)
{
  gtk_stack_set_visible_child (self->printer_model_stack, GTK_WIDGET (self->loading_box));
  gtk_widget_set_sensitive (GTK_WIDGET (self->search_for_drivers_button), FALSE);

  get_ppd_names_async (self->printer_name,
                       1,
                       self->cancellable,
                       get_ppd_names_cb,
                       self);
}

static void
set_ppd_cb (const gchar *printer_name,
            gboolean  success,
            gpointer  user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  gtk_label_set_text (GTK_LABEL (self->printer_model_label), self->ppd_file_name);
}

static void
ppd_selection_dialog_response_cb (GtkWindow *dialog,
                                  gint       response_id,
                                  gpointer   user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  if (response_id == GTK_RESPONSE_OK)
    {
      g_autofree gchar *ppd_name = NULL;

      ppd_name = pp_ppd_selection_dialog_get_ppd_name (self->pp_ppd_selection_dialog);

      if (self->printer_name && ppd_name)
        {
          printer_set_ppd_async (self->printer_name,
                                 ppd_name,
                                 self->cancellable,
                                 set_ppd_cb,
                                 self);

          g_clear_pointer (&self->ppd_file_name, g_free);
          self->ppd_file_name = g_strdup (ppd_name);
        }
    }

  gtk_window_destroy (GTK_WINDOW (self->pp_ppd_selection_dialog));
  self->pp_ppd_selection_dialog = NULL;
}

static void
get_all_ppds_async_cb (PPDList  *ppds,
                       gpointer  user_data)
{
  PpDetailsDialog *self = user_data;

  self->all_ppds_list = ppd_list_copy (ppds);

  if (self->pp_ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (self->pp_ppd_selection_dialog,
                                          self->all_ppds_list);
}

static void
select_ppd_in_dialog (PpDetailsDialog *self)
{
  g_autofree gchar *device_id = NULL;
  g_autofree gchar *manufacturer = NULL;

  g_clear_pointer (&self->ppd_file_name, g_free);
  self->ppd_file_name = g_strdup (cupsGetPPD (self->printer_name));

  if (!self->pp_ppd_selection_dialog)
    {
      device_id =
        get_ppd_attribute (self->ppd_file_name,
                           "1284DeviceID");

      if (device_id)
        {
          manufacturer = get_tag_value (device_id, "mfg");
          if (!manufacturer)
            manufacturer = get_tag_value (device_id, "manufacturer");
          }

      if (manufacturer == NULL)
        {
          manufacturer =
            get_ppd_attribute (self->ppd_file_name,
                               "Manufacturer");
          }

      if (manufacturer == NULL)
        {
          manufacturer = g_strdup ("Raw");
        }

       if (self->all_ppds_list == NULL)
         {
           get_all_ppds_async (self->cancellable, get_all_ppds_async_cb, self);
         }

        self->pp_ppd_selection_dialog = pp_ppd_selection_dialog_new (
          self->all_ppds_list,
          manufacturer,
          ppd_selection_dialog_response_cb,
          self);

        gtk_window_set_transient_for (GTK_WINDOW (self->pp_ppd_selection_dialog),
                                      GTK_WINDOW (self));

        gtk_widget_set_visible (GTK_WIDGET (self->pp_ppd_selection_dialog), TRUE);
    }
}

static void
ppd_file_select_response_cb (PpDetailsDialog *self,
                             gint             response_id,
                             GtkDialog       *dialog)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = NULL;
      g_autofree gchar *ppd_filename = NULL;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      ppd_filename = g_file_get_path (file);

      if (self->printer_name && ppd_filename)
        {
          printer_set_ppd_file_async (self->printer_name,
                                      ppd_filename,
                                      self->cancellable,
                                      set_ppd_cb,
                                      self);
        }
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
select_ppd_manually (PpDetailsDialog *self)
{
  GtkFileFilter *filter;
  GtkWidget     *dialog;

  dialog = gtk_file_chooser_dialog_new (_("Select PPD File"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter,
    _("PostScript Printer Description files (*.ppd, *.PPD, *.ppd.gz, *.PPD.gz, *.PPD.GZ)"));
  gtk_file_filter_add_pattern (filter, "*.ppd");
  gtk_file_filter_add_pattern (filter, "*.PPD");
  gtk_file_filter_add_pattern (filter, "*.ppd.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.GZ");

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  gtk_widget_set_visible (dialog, TRUE);

  g_signal_connect_swapped (dialog, "response", G_CALLBACK (ppd_file_select_response_cb), self);
}

static void
update_sensitivity (PpDetailsDialog *self,
                    gboolean         sensitive)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->printer_name_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->printer_location_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->driver_buttons), sensitive);
}

static void
pp_details_dialog_init (PpDetailsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
}

static void
pp_details_dialog_dispose (GObject *object)
{
  PpDetailsDialog *self = PP_DETAILS_DIALOG (object);

  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->ppd_file_name, g_free);

  if (self->all_ppds_list != NULL)
    {
      ppd_list_free (self->all_ppds_list);
      self->all_ppds_list = NULL;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (pp_details_dialog_parent_class)->dispose (object);
}

static void
pp_details_dialog_class_init (PpDetailsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pp_details_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/pp-details-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, print_name_hint_revealer);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, driver_buttons);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, loading_box);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_location_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_stack);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_name_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, search_for_drivers_button);

  gtk_widget_class_bind_template_callback (widget_class, printer_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_for_drivers);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_in_dialog);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_manually);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

PpDetailsDialog *
pp_details_dialog_new (gchar   *printer_name,
                       gchar   *printer_location,
                       gchar   *printer_address,
                       gchar   *printer_make_and_model,
                       gboolean sensitive)
{
  PpDetailsDialog *self;
  g_autofree gchar *title = NULL;
  g_autofree gchar *printer_url = NULL;

  self = g_object_new (PP_DETAILS_DIALOG_TYPE, NULL);

  self->printer_name = g_strdup (printer_name);
  self->ppd_file_name = NULL;

  /* Translators: This is the title of the dialog. %s is the printer name. */
  title = g_strdup_printf (_("%s Details"), printer_name);
  gtk_window_set_title (GTK_WINDOW (self), title);

  printer_url = g_strdup_printf ("<a href=\"http://%s:%d\">%s</a>", printer_address, ippPort (), printer_address);
  gtk_label_set_markup (GTK_LABEL (self->printer_address_label), printer_url);

  gtk_editable_set_text (GTK_EDITABLE (self->printer_name_entry), printer_name);
  gtk_editable_set_text (GTK_EDITABLE (self->printer_location_entry), printer_location);
  gtk_label_set_text (GTK_LABEL (self->printer_model_label), printer_make_and_model);

  update_sensitivity (self, sensitive);

  return self;
}

const gchar *
pp_details_dialog_get_printer_name (PpDetailsDialog *self)
{
  g_return_val_if_fail (PP_IS_DETAILS_DIALOG (self), NULL);
  return gtk_editable_get_text (GTK_EDITABLE (self->printer_name_entry));
}

const gchar *
pp_details_dialog_get_printer_location (PpDetailsDialog *self)
{
  g_return_val_if_fail (PP_IS_DETAILS_DIALOG (self), NULL);
  return gtk_editable_get_text (GTK_EDITABLE (self->printer_location_entry));
}
