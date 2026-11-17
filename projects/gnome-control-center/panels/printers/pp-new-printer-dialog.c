/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>

#include <adwaita.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

#include "pp-new-printer-dialog.h"
#include "pp-cups.h"
#include "pp-host.h"
#include "pp-new-printer.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-samba.h"
#include "pp-utils.h"

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetState(ipp) ipp->state
#endif

/*
 * Additional delay to the default 150ms delay in GtkSearchEntry
 * resulting in total delay of 500ms.
 */
#define HOST_SEARCH_DELAY (500 - 150)

#define AUTHENTICATION_PAGE "authentication-page"
#define ADDPRINTER_PAGE "addprinter-page"

static void     set_device (PpNewPrinterDialog *self,
                            PpPrintDevice      *device,
                            GtkTreeIter        *iter);
static void     replace_device (PpNewPrinterDialog *self,
                                PpPrintDevice      *old_device,
                                PpPrintDevice      *new_device);
static void     populate_devices_list (PpNewPrinterDialog *self);
static void     search_entry_activated_cb (PpNewPrinterDialog *self);
static void     search_entry_changed_cb (PpNewPrinterDialog *self);
static void     add_cb (PpNewPrinterDialog *self);
static void     cancel_cb (PpNewPrinterDialog *self);
static void     update_dialog_state (PpNewPrinterDialog *self);
static void     add_devices_to_list (PpNewPrinterDialog  *self,
                                     GPtrArray           *devices);
static void     remove_device_from_list (PpNewPrinterDialog *self,
                                         const gchar        *device_name);

enum
{
  DEVICE_GICON_COLUMN = 0,
  DEVICE_NAME_COLUMN,
  DEVICE_DISPLAY_NAME_COLUMN,
  DEVICE_DESCRIPTION_COLUMN,
  SERVER_NEEDS_AUTHENTICATION_COLUMN,
  DEVICE_VISIBLE_COLUMN,
  DEVICE_COLUMN,
  DEVICE_N_COLUMNS
};

struct _PpNewPrinterDialog
{
  AdwWindow parent_instance;

  GPtrArray *local_cups_devices;

  GtkListStore       *devices_liststore;
  GtkTreeModelFilter *devices_model_filter;

  /* headerbar */
  AdwWindowTitle       *header_title;

  /* headerbar topleft buttons */
  GtkStack           *headerbar_topleft_buttons;
  GtkButton          *go_back_button;

  /* headerbar topright buttons */
  GtkStack           *headerbar_topright_buttons;
  GtkButton          *new_printer_add_button;
  GtkButton          *unlock_button;
  GtkButton          *authenticate_button;
  /* end headerbar */

  /* dialogstack */
  GtkStack           *dialog_stack;
  GtkStack           *stack;

  /* scrolledwindow1 */
  GtkScrolledWindow  *scrolledwindow1;
  GtkTreeView        *devices_treeview;

  GtkEntry           *search_entry;

  /* authentication page */
  GtkLabel           *authentication_title;
  GtkLabel           *authentication_text;
  GtkEntry           *username_entry;
  GtkEntry           *password_entry;
  /* end dialog stack */

  UserResponseCallback user_callback;
  gpointer             user_data;

  cups_dest_t *dests;
  gint         num_of_dests;

  GCancellable *cancellable;
  GCancellable *remote_host_cancellable;

  gboolean  cups_searching;
  gboolean  samba_authenticated_searching;
  gboolean  samba_searching;

  PpPPDSelectionDialog *ppd_selection_dialog;

  PpPrintDevice *new_device;

  PPDList *list;

  GIcon *local_printer_icon;
  GIcon *remote_printer_icon;
  GIcon *authenticated_server_icon;

  PpHost  *snmp_host;
  PpHost  *socket_host;
  PpHost  *lpd_host;
  PpHost  *remote_cups_host;
  PpSamba *samba_host;
  guint    host_search_timeout_id;
};

G_DEFINE_TYPE (PpNewPrinterDialog, pp_new_printer_dialog, ADW_TYPE_WINDOW)

typedef struct
{
  gchar    *server_name;
  gpointer  dialog;
} AuthSMBData;

static void
get_authenticated_samba_devices_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  AuthSMBData               *data = user_data;
  PpNewPrinterDialog        *self = PP_NEW_PRINTER_DIALOG (data->dialog);
  g_autoptr(GPtrArray)       devices = NULL;
  gboolean                   cancelled = FALSE;
  g_autoptr(GError)          error = NULL;

  devices = pp_samba_get_devices_finish (PP_SAMBA (source_object), res, &error);

  if (devices != NULL)
    {
      self->samba_authenticated_searching = FALSE;

      for (guint i = 0; i < devices->len; i++)
        {
          PpPrintDevice *device = g_ptr_array_index (devices, i);

          if (pp_print_device_is_authenticated_server (device))
            {
              cancelled = TRUE;
              break;
            }
        }

      if (!cancelled)
        {
          if (devices != NULL)
            {
              add_devices_to_list (self, devices);

              if (devices->len > 0)
                {
                  gtk_editable_set_text (GTK_EDITABLE (self->search_entry),
                                         pp_print_device_get_device_location (g_ptr_array_index (devices, 0)));
                  search_entry_activated_cb (self);
                }
            }
        }

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          self->samba_authenticated_searching = FALSE;
          update_dialog_state (self);
        }
    }

  g_free (data->server_name);
  g_free (data);
}

static void
go_to_page (PpNewPrinterDialog *self,
            const gchar        *page)
{
  gtk_stack_set_visible_child_name (self->dialog_stack, page);
  gtk_stack_set_visible_child_name (self->headerbar_topright_buttons, page);
  gtk_stack_set_visible_child_name (self->headerbar_topleft_buttons, page);
}

static void
on_authenticate (PpNewPrinterDialog *self)
{
  gchar                     *hostname = NULL;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  username = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->username_entry)));
  password = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->password_entry)));

  if ((username == NULL) || (username[0] == '\0') ||
      (password == NULL) || (password[0] == '\0'))
    {
      g_clear_pointer (&username, g_free);
      g_clear_pointer (&password, g_free);
      return;
    }

  pp_samba_set_auth_info (PP_SAMBA (self->samba_host), username, password);

  adw_window_title_set_title (self->header_title, _("Add Printer"));
  go_to_page (self, ADDPRINTER_PAGE);

  g_object_get (PP_HOST (self->samba_host), "hostname", &hostname, NULL);
  remove_device_from_list (self, hostname);
}

static void
on_authentication_required (PpNewPrinterDialog *self)
{
  g_autofree gchar          *hostname = NULL;
  g_autofree gchar          *title = NULL;
  g_autofree gchar          *text = NULL;

  adw_window_title_set_subtitle (self->header_title, NULL);
  adw_window_title_set_title (self->header_title, _("Unlock Print Server"));

  g_object_get (self->samba_host, "hostname", &hostname, NULL);
  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  title = g_strdup_printf (_("Unlock %s."), hostname);
  gtk_label_set_text (self->authentication_title, title);

  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  text = g_strdup_printf (_("Enter username and password to view printers on %s."), hostname);
  gtk_label_set_text (self->authentication_text, text);

  go_to_page (self, AUTHENTICATION_PAGE);

  g_signal_connect_object (self->authenticate_button, "clicked", G_CALLBACK (on_authenticate), self, G_CONNECT_SWAPPED);
}

static void
auth_entries_changed (PpNewPrinterDialog *self)
{
  gboolean                   can_authenticate = FALSE;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  username = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->username_entry)));
  password = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->password_entry)));

  can_authenticate = (username != NULL && username[0] != '\0' &&
                      password != NULL && password[0] != '\0');

  gtk_widget_set_sensitive (GTK_WIDGET (self->authenticate_button), can_authenticate);

  g_clear_pointer (&username, g_free);
  g_clear_pointer (&password, g_free);
}

static void
on_go_back_button_clicked (PpNewPrinterDialog *self)
{
  pp_samba_set_auth_info (self->samba_host, NULL, NULL);
  g_clear_object (&self->samba_host);

  go_to_page (self, ADDPRINTER_PAGE);
  adw_window_title_set_title (self->header_title, _("Add Printer"));
  gtk_widget_set_sensitive (GTK_WIDGET (self->new_printer_add_button), FALSE);

  gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (self->devices_treeview));
}

static void
authenticate_samba_server (PpNewPrinterDialog *self)
{
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  AuthSMBData               *data;
  gchar                     *server_name = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (self->unlock_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->authenticate_button), FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->username_entry));

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->devices_treeview), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DEVICE_NAME_COLUMN, &server_name,
                          -1);

      if (server_name != NULL)
        {
          g_clear_object (&self->samba_host);

          self->samba_host = pp_samba_new (server_name);
          g_signal_connect_object (self->samba_host,
                                   "authentication-required",
                                   G_CALLBACK (on_authentication_required),
                                   self, G_CONNECT_SWAPPED);

          self->samba_authenticated_searching = TRUE;
          update_dialog_state (self);

          data = g_new (AuthSMBData, 1);
          data->server_name = server_name;
          data->dialog = self;

          pp_samba_get_devices_async (self->samba_host,
                                      TRUE,
                                      self->cancellable,
                                      get_authenticated_samba_devices_cb,
                                      data);
        }
    }
}

static void
device_selection_changed_cb (PpNewPrinterDialog *self)
{
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->devices_treeview),
                                              &model,
                                              &iter);

  if (selected)
    {
      gtk_tree_model_get (model, &iter,
                          SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed,
                          -1);

      gtk_widget_set_sensitive (GTK_WIDGET (self->new_printer_add_button), selected);
      gtk_widget_set_sensitive (GTK_WIDGET (self->unlock_button), authentication_needed);

      if (authentication_needed)
        gtk_stack_set_visible_child_name (self->headerbar_topright_buttons, "unlock_button");
      else
        gtk_stack_set_visible_child_name (self->headerbar_topright_buttons, ADDPRINTER_PAGE);
    }
}

static void
remove_device_from_list (PpNewPrinterDialog *self,
                         const gchar        *device_name)
{
  GtkTreeIter                iter;
  gboolean                   cont;

  cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter);
  while (cont)
    {
      g_autoptr(PpPrintDevice) device = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (self->devices_liststore), &iter,
                          DEVICE_COLUMN, &device,
                          -1);

      if (g_strcmp0 (pp_print_device_get_device_name (device), device_name) == 0)
        {
          gtk_list_store_remove (self->devices_liststore, &iter);
          break;
        }

      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->devices_liststore), &iter);
    }

  update_dialog_state (self);
}

static gboolean
prepend_original_name (GtkTreeModel *model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      data)
{
  g_autoptr(PpPrintDevice) device = NULL;
  GList         **list = data;

  gtk_tree_model_get (model, iter,
                      DEVICE_COLUMN, &device,
                      -1);

  *list = g_list_prepend (*list, g_strdup (pp_print_device_get_device_original_name (device)));

  return FALSE;
}

static void
add_device_to_list (PpNewPrinterDialog *self,
                    PpPrintDevice      *device)
{
  GList                     *original_names_list = NULL;
  gint                       acquisition_method;

  if (device)
    {
      if (pp_print_device_get_host_name (device) == NULL)
        {
          g_autofree gchar *host_name = guess_device_hostname (device);
          g_object_set (device, "host-name", host_name, NULL);
        }

      acquisition_method = pp_print_device_get_acquisition_method (device);
      if (pp_print_device_get_device_id (device) ||
          pp_print_device_get_device_ppd (device) ||
          (pp_print_device_get_host_name (device) &&
           acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
           acquisition_method == ACQUISITION_METHOD_SAMBA ||
          (pp_print_device_get_device_uri (device) &&
           (acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
            acquisition_method == ACQUISITION_METHOD_LPD)))
        {
          g_autofree gchar *canonicalized_name = NULL;

          g_object_set (device,
                        "device-original-name", pp_print_device_get_device_name (device),
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (self->devices_liststore),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          canonicalized_name = canonicalize_device_name (original_names_list,
                                                         self->local_cups_devices,
                                                         self->dests,
                                                         self->num_of_dests,
                                                         device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (device,
                        "display-name", canonicalized_name,
                        "device-name", canonicalized_name,
                        NULL);

          if (pp_print_device_get_acquisition_method (device) == ACQUISITION_METHOD_DEFAULT_CUPS_SERVER)
            g_ptr_array_add (self->local_cups_devices, g_object_ref (device));
          else
            set_device (self, device, NULL);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          g_autoptr(PpPrintDevice) store_device = NULL;

          store_device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                       "device-name", pp_print_device_get_host_name (device),
                                       "host-name", pp_print_device_get_host_name (device),
                                       "is-authenticated-server", pp_print_device_is_authenticated_server (device),
                                       NULL);

          set_device (self, store_device, NULL);
        }
    }
}

static void
add_devices_to_list (PpNewPrinterDialog  *self,
                     GPtrArray           *devices)
{
  for (guint i = 0; i < devices->len; i++)
    add_device_to_list (self, g_ptr_array_index (devices, i));
}

static PpPrintDevice *
device_in_list (gchar *device_uri,
                GPtrArray *device_list)
{
  for (guint i = 0; i < device_list->len; i++)
    {
      PpPrintDevice *device = g_ptr_array_index (device_list, i);
      /* GroupPhysicalDevices returns uris without port numbers */
      if (pp_print_device_get_device_uri (device) != NULL &&
          g_str_has_prefix (pp_print_device_get_device_uri (device), device_uri))
        return g_object_ref (device);
    }

  return NULL;
}

static PpPrintDevice *
device_in_liststore (gchar        *device_uri,
                     GtkListStore *device_liststore)
{
  GtkTreeIter    iter;
  gboolean       cont;

  cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (device_liststore), &iter);
  while (cont)
    {
      g_autoptr(PpPrintDevice) device = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (device_liststore), &iter,
                          DEVICE_COLUMN, &device,
                          -1);

      /* GroupPhysicalDevices returns uris without port numbers */
      if (pp_print_device_get_device_uri (device) != NULL &&
          g_str_has_prefix (pp_print_device_get_device_uri (device), device_uri))
        {
          return g_steal_pointer(&device);
        }

      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (device_liststore), &iter);
    }

  return NULL;
}

static void
update_dialog_state (PpNewPrinterDialog *self)
{
  GtkTreeIter                iter;
  gboolean                   searching;

  searching = self->cups_searching ||
              self->remote_cups_host != NULL ||
              self->snmp_host != NULL ||
              self->socket_host != NULL ||
              self->lpd_host != NULL ||
              self->samba_host != NULL ||
              self->samba_authenticated_searching ||
              self->samba_searching;

  if (searching)
    {
      adw_window_title_set_subtitle (self->header_title, _("Searching for Printers"));
    }
  else
    {
      adw_window_title_set_subtitle (self->header_title, NULL);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter))
      gtk_stack_set_visible_child_name (self->stack, "standard-page");
  else
      gtk_stack_set_visible_child_name (self->stack, searching ? "loading-page" : "no-printers-page");
}

static void
group_physical_devices_cb (gchar    ***device_uris,
                           gpointer    user_data)
{
  PpNewPrinterDialog        *self = user_data;
  gint                       i, j;

  if (device_uris != NULL)
    {
      for (i = 0; device_uris[i] != NULL; i++)
        {
          /* Is there any device in this sublist? */
          if (device_uris[i][0] != NULL)
            {
              g_autoptr(PpPrintDevice) device = NULL;

              for (j = 0; device_uris[i][j] != NULL; j++)
                {
                  device = device_in_liststore (device_uris[i][j], self->devices_liststore);
                  if (device != NULL)
                    break;
                }

              /* Is this sublist represented in the current list of devices? */
              if (device != NULL)
                {
                  /* Is there better device in the sublist? */
                  if (j != 0)
                    {
                      g_autoptr(PpPrintDevice) better_device = NULL;

                      better_device = device_in_list (device_uris[i][0], self->local_cups_devices);
                      replace_device (self, device, better_device);
                    }
                }
              else
                {
                  device = device_in_list (device_uris[i][0], self->local_cups_devices);
                  if (device != NULL)
                    set_device (self, device, NULL);
                }
            }
        }

      for (i = 0; device_uris[i] != NULL; i++)
        g_strfreev (device_uris[i]);

      g_free (device_uris);
    }
  else
    {
      for (i = 0; i < self->local_cups_devices->len; i++)
        set_device (self, g_ptr_array_index (self->local_cups_devices, i), NULL);
      g_ptr_array_set_size (self->local_cups_devices, 0);
    }

  update_dialog_state (self);
}

static void
group_physical_devices_dbus_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;
  g_autoptr(GError)   error = NULL;
  gchar            ***result = NULL;
  gint                i;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      g_autoptr(GVariant) array = NULL;

      g_variant_get (output, "(@aas)", &array);

      if (array)
        {
          g_autoptr(GVariantIter) iter = NULL;
          GStrv device_uris;

          result = g_new0 (gchar **, g_variant_n_children (array) + 1);
          g_variant_get (array, "aas", &iter);
          i = 0;
          while (g_variant_iter_next (iter, "^as", &device_uris))
            {
              result[i] = device_uris;
              i++;
            }
        }
    }
  else if (error &&
           error->domain == G_DBUS_ERROR &&
           (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
            error->code == G_DBUS_ERROR_UNKNOWN_METHOD))
    {
      g_warning ("Install system-config-printer which provides \
DBus method \"GroupPhysicalDevices\" to group duplicates in device list.");
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    group_physical_devices_cb (result, user_data);
}

static void
get_cups_devices_cb (GPtrArray *devices,
                     gboolean  finished,
                     gboolean  cancelled,
                     gpointer  user_data)
{
  PpNewPrinterDialog         *self = user_data;
  g_autoptr(GDBusConnection)  bus = NULL;
  GVariantBuilder             device_list;
  GVariantBuilder             device_hash;
  PpPrintDevice             **all_devices;
  const gchar                *device_class;
  GtkTreeIter                 iter;
  gboolean                    cont;
  g_autoptr(GError)           error = NULL;
  gint                        length, i;


  if (!cancelled)
    {
      if (finished)
        {
          self->cups_searching = FALSE;
        }

      if (devices != NULL)
        {
          add_devices_to_list (self, devices);

          length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->devices_liststore), NULL) + self->local_cups_devices->len;
          if (length > 0)
            {
              all_devices = g_new0 (PpPrintDevice *, length);

              i = 0;
              cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter);
              while (cont)
                {
                  g_autoptr(PpPrintDevice) device = NULL;

                  gtk_tree_model_get (GTK_TREE_MODEL (self->devices_liststore), &iter,
                                      DEVICE_COLUMN, &device,
                                      -1);

                  all_devices[i] = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                 "device-id", pp_print_device_get_device_id (device),
                                                 "device-make-and-model", pp_print_device_get_device_make_and_model (device),
                                                 "is-network-device", pp_print_device_is_network_device (device),
                                                 "device-uri", pp_print_device_get_device_uri (device),
                                                 NULL);
                  i++;

                  cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->devices_liststore), &iter);
                }

              for (guint j = 0; j < self->local_cups_devices->len; j++)
                {
                  PpPrintDevice *pp_device = g_ptr_array_index (self->local_cups_devices, j);
                  all_devices[i] = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                 "device-id", pp_print_device_get_device_id (pp_device),
                                                 "device-make-and-model", pp_print_device_get_device_make_and_model (pp_device),
                                                 "is-network-device", pp_print_device_is_network_device (pp_device),
                                                 "device-uri", pp_print_device_get_device_uri (pp_device),
                                                 NULL);
                   i++;
                }

              bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
              if (bus)
                {
                  g_variant_builder_init (&device_list, G_VARIANT_TYPE ("a{sv}"));

                  for (i = 0; i < length; i++)
                    {
                      if (pp_print_device_get_device_uri (all_devices[i]))
                        {
                          g_variant_builder_init (&device_hash, G_VARIANT_TYPE ("a{ss}"));

                          if (pp_print_device_get_device_id (all_devices[i]))
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-id",
                                                   pp_print_device_get_device_id (all_devices[i]));

                          if (pp_print_device_get_device_make_and_model (all_devices[i]))
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-make-and-model",
                                                   pp_print_device_get_device_make_and_model (all_devices[i]));

                          if (pp_print_device_is_network_device (all_devices[i]))
                            device_class = "network";
                          else
                            device_class = "direct";

                          g_variant_builder_add (&device_hash,
                                                 "{ss}",
                                                 "device-class",
                                                 device_class);

                          g_variant_builder_add (&device_list,
                                                 "{sv}",
                                                 pp_print_device_get_device_uri (all_devices[i]),
                                                 g_variant_builder_end (&device_hash));
                        }
                    }

                  g_dbus_connection_call (bus,
                                          SCP_BUS,
                                          SCP_PATH,
                                          SCP_IFACE,
                                          "GroupPhysicalDevices",
                                          g_variant_new ("(v)", g_variant_builder_end (&device_list)),
                                          G_VARIANT_TYPE ("(aas)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          self->cancellable,
                                          group_physical_devices_dbus_cb,
                                          self);
                }
              else
                {
                  g_warning ("Failed to get system bus: %s", error->message);
                  group_physical_devices_cb (NULL, user_data);
                }

              for (i = 0; i < length; i++)
                g_object_unref (all_devices[i]);
              g_free (all_devices);
            }
          else
            {
              update_dialog_state (self);
            }
        }
      else
        {
          update_dialog_state (self);
        }
    }
}

static void
get_snmp_devices_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GError)          error = NULL;
  g_autoptr(GPtrArray)       devices = NULL;

  devices = pp_host_get_snmp_devices_finish (PP_HOST (source_object), res, &error);

  if (devices != NULL)
    {
      g_clear_object(&self->snmp_host);

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          g_clear_object(&self->snmp_host);

          update_dialog_state (self);
        }
    }
}

static void
get_remote_cups_devices_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GError)          error = NULL;
  g_autoptr(GPtrArray)       devices = NULL;

  devices = pp_host_get_remote_cups_devices_finish (PP_HOST (source_object), res, &error);

  if (devices != NULL)
    {
      g_clear_object(&self->remote_cups_host);

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          g_clear_object(&self->remote_cups_host);

          update_dialog_state (self);
        }
    }
}

static void
get_samba_host_devices_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GPtrArray)       devices = NULL;
  g_autoptr(GError)          error = NULL;

  devices = pp_samba_get_devices_finish (PP_SAMBA (source_object), res, &error);

  if (devices != NULL)
    {
      g_clear_object(&self->samba_host);

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          g_clear_object(&self->samba_host);

          update_dialog_state (self);
        }
    }
}

static void
get_samba_devices_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GPtrArray)       devices = NULL;
  g_autoptr(GError)          error = NULL;

  devices = pp_samba_get_devices_finish (PP_SAMBA (source_object), res, &error);

  if (devices != NULL)
    {
      self->samba_searching = FALSE;

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          self->samba_searching = FALSE;

          update_dialog_state (self);
        }
    }
}

static void
get_jetdirect_devices_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GError)          error = NULL;
  g_autoptr(GPtrArray)       devices = NULL;

  devices = pp_host_get_jetdirect_devices_finish (PP_HOST (source_object), res, &error);

  if (devices != NULL)
    {
      g_clear_object(&self->socket_host);

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          g_clear_object(&self->socket_host);

          update_dialog_state (self);
        }
    }
}

static void
get_lpd_devices_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autoptr(GError)          error = NULL;
  g_autoptr(GPtrArray)       devices = NULL;

  devices = pp_host_get_lpd_devices_finish (PP_HOST (source_object), res, &error);

  if (devices != NULL)
    {
      g_clear_object(&self->lpd_host);

      add_devices_to_list (self, devices);

      update_dialog_state (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          g_clear_object(&self->lpd_host);

          update_dialog_state (self);
        }
    }
}

static void
get_cups_devices (PpNewPrinterDialog *self)
{
  self->cups_searching = TRUE;
  update_dialog_state (self);

  get_cups_devices_async (self->cancellable,
                          get_cups_devices_cb,
                          self);
}

static gboolean
parse_uri (const gchar  *uri,
           gchar       **scheme,
           gchar       **host,
           gint         *port)
{
  const gchar      *tmp = NULL;
  g_autofree gchar *resulting_host = NULL;
  gchar            *position;

  *port = PP_HOST_UNSET_PORT;

  position = g_strrstr (uri, "://");
  if (position != NULL)
    {
      *scheme = g_strndup (uri, position - uri);
      tmp = position + 3;
    }
  else
    {
      tmp = uri;
    }

  if (g_strrstr (tmp, "@"))
    tmp = g_strrstr (tmp, "@") + 1;

  if ((position = g_strrstr (tmp, "/")))
    {
      *position = '\0';
      resulting_host = g_strdup (tmp);
      *position = '/';
    }
  else
    {
      resulting_host = g_strdup (tmp);
    }

  if ((position = g_strrstr (resulting_host, ":")))
    {
      *position = '\0';
      *port = atoi (position + 1);
    }

  *host = g_uri_unescape_string (resulting_host,
                                 G_URI_RESERVED_CHARS_GENERIC_DELIMITERS
                                 G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS);

  return TRUE;
}

typedef struct
{
  PpNewPrinterDialog *dialog;
  gchar              *host_scheme;
  gchar              *host_name;
  gint                host_port;
} THostSearchData;

static void
search_for_remote_printers_free (THostSearchData *data)
{
  g_free (data->host_scheme);
  g_free (data->host_name);
  g_free (data);
}

static gboolean
search_for_remote_printers (THostSearchData *data)
{
  PpNewPrinterDialog *self = data->dialog;

  g_cancellable_cancel (self->remote_host_cancellable);
  g_clear_object (&self->remote_host_cancellable);

  self->remote_host_cancellable = g_cancellable_new ();

  self->remote_cups_host = pp_host_new (data->host_name);
  self->snmp_host = pp_host_new (data->host_name);
  self->socket_host = pp_host_new (data->host_name);
  self->lpd_host = pp_host_new (data->host_name);

  if (data->host_port != PP_HOST_UNSET_PORT)
    {
      g_object_set (self->remote_cups_host, "port", data->host_port, NULL);
      g_object_set (self->snmp_host, "port", data->host_port, NULL);

      /* Accept port different from the default one only if user specifies
       * scheme (for socket and lpd printers).
       */
      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "socket") == 0)
        g_object_set (self->socket_host, "port", data->host_port, NULL);

      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "lpd") == 0)
        g_object_set (self->lpd_host, "port", data->host_port, NULL);
    }

  self->samba_host = pp_samba_new (data->host_name);

  update_dialog_state (data->dialog);

  pp_host_get_remote_cups_devices_async (self->remote_cups_host,
                                         self->remote_host_cancellable,
                                         get_remote_cups_devices_cb,
                                         data->dialog);

  pp_host_get_snmp_devices_async (self->snmp_host,
                                  self->remote_host_cancellable,
                                  get_snmp_devices_cb,
                                  data->dialog);

  pp_host_get_jetdirect_devices_async (self->socket_host,
                                       self->remote_host_cancellable,
                                       get_jetdirect_devices_cb,
                                       data->dialog);

  pp_host_get_lpd_devices_async (self->lpd_host,
                                 self->remote_host_cancellable,
                                 get_lpd_devices_cb,
                                 data->dialog);

  pp_samba_get_devices_async (self->samba_host,
                              FALSE,
                              self->remote_host_cancellable,
                              get_samba_host_devices_cb,
                              data->dialog);

  self->host_search_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
search_address (const gchar        *text,
                PpNewPrinterDialog *self,
                gboolean            delay_search)
{
  GtkTreeIter                 iter;
  gboolean                    found = FALSE;
  gboolean                    subfound;
  gboolean                    next_set;
  gboolean                    cont;
  g_autofree gchar           *lowercase_text = NULL;
  gchar                     **words;
  gint                        words_length = 0;
  gint                        i;
  gint                        acquisition_method;

  lowercase_text = g_ascii_strdown (text, -1);
  words = g_strsplit_set (lowercase_text, " ", -1);

  if (words)
    {
      words_length = g_strv_length (words);

      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter);
      while (cont)
        {
          g_autoptr(PpPrintDevice) device = NULL;
          g_autofree gchar *lowercase_name = NULL;
          g_autofree gchar *lowercase_location = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->devices_liststore), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          lowercase_name = g_ascii_strdown (pp_print_device_get_device_name (device), -1);
          if (pp_print_device_get_device_location (device))
            lowercase_location = g_ascii_strdown (pp_print_device_get_device_location (device), -1);
          else
            lowercase_location = NULL;

          subfound = TRUE;
          for (i = 0; words[i]; i++)
            {
              if (!g_strrstr (lowercase_name, words[i]) &&
                  (!lowercase_location || !g_strrstr (lowercase_location, words[i])))
                subfound = FALSE;
            }

          if (subfound)
            found = TRUE;

          gtk_list_store_set (GTK_LIST_STORE (self->devices_liststore), &iter,
                              DEVICE_VISIBLE_COLUMN, subfound,
                              -1);

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->devices_liststore), &iter);
        }

      g_strfreev (words);
  }

  /*
   * The given word is probably an address since it was not found among
   * already present devices.
   */
  if (!found && words_length == 1)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter);
      while (cont)
        {
          g_autoptr(PpPrintDevice) device = NULL;

          next_set = FALSE;
          gtk_tree_model_get (GTK_TREE_MODEL (self->devices_liststore), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          gtk_list_store_set (GTK_LIST_STORE (self->devices_liststore), &iter,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              -1);

          acquisition_method = pp_print_device_get_acquisition_method (device);
          if (acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER ||
              acquisition_method == ACQUISITION_METHOD_SNMP ||
              acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
              acquisition_method == ACQUISITION_METHOD_LPD ||
              acquisition_method == ACQUISITION_METHOD_SAMBA_HOST)
            {
              if (!gtk_list_store_remove (self->devices_liststore, &iter))
                break;
              else
                next_set = TRUE;
            }

          if (!next_set)
            cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->devices_liststore), &iter);
        }

      if (text && text[0] != '\0')
        {
          g_autoptr(GSocketConnectable) conn = NULL;
          g_autofree gchar *test_uri = NULL;
          g_autofree gchar *test_port = NULL;
          gchar *scheme = NULL;
          gchar *host = NULL;
          gint   port;

          parse_uri (text, &scheme, &host, &port);

          if (host != NULL)
            {
              if (port >= 0)
                test_port = g_strdup_printf (":%d", port);
              else
                test_port = g_strdup ("");

              test_uri = g_strdup_printf ("%s://%s%s",
                                          scheme != NULL && scheme[0] != '\0' ? scheme : "none",
                                          host,
                                          test_port);

              conn = g_network_address_parse_uri (test_uri, 0, NULL);
              if (conn != NULL)
                {
                  THostSearchData *search_data;

                  search_data = g_new (THostSearchData, 1);
                  search_data->host_scheme = scheme;
                  search_data->host_name = host;
                  search_data->host_port = port;
                  search_data->dialog = self;

                  g_clear_handle_id (&self->host_search_timeout_id, g_source_remove);

                  if (delay_search)
                    {
                      self->host_search_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                                         HOST_SEARCH_DELAY,
                                                                         (GSourceFunc) search_for_remote_printers,
                                                                         search_data,
                                                                         (GDestroyNotify) search_for_remote_printers_free);
                    }
                  else
                    {
                      search_for_remote_printers (search_data);
                      search_for_remote_printers_free (search_data);
                    }
                }
            }
        }
    }
}

static void
search_entry_activated_cb (PpNewPrinterDialog *self)
{
  search_address (gtk_editable_get_text (GTK_EDITABLE (self->search_entry)),
                  self,
                  FALSE);
}

static void
search_entry_changed_cb (PpNewPrinterDialog *self)
{
  search_address (gtk_editable_get_text (GTK_EDITABLE (self->search_entry)),
                  self,
                  TRUE);
}

static gchar *
get_local_scheme_description_from_uri (gchar *device_uri)
{
  gchar *description = NULL;

  if (device_uri != NULL)
    {
      if (g_str_has_prefix (device_uri, "usb") ||
          g_str_has_prefix (device_uri, "hp:/usb/") ||
          g_str_has_prefix (device_uri, "hpfax:/usb/"))
        {
          /* Translators: The found device is a printer connected via USB */
          description = g_strdup (_("USB"));
        }
      else if (g_str_has_prefix (device_uri, "serial"))
        {
          /* Translators: The found device is a printer connected via serial port */
          description = g_strdup (_("Serial Port"));
        }
      else if (g_str_has_prefix (device_uri, "parallel") ||
               g_str_has_prefix (device_uri, "hp:/par/") ||
               g_str_has_prefix (device_uri, "hpfax:/par/"))
        {
          /* Translators: The found device is a printer connected via parallel port */
          description = g_strdup (_("Parallel Port"));
        }
      else if (g_str_has_prefix (device_uri, "bluetooth"))
        {
          /* Translators: The found device is a printer connected via Bluetooth */
          description = g_strdup (_("Bluetooth"));
        }
    }

  return description;
}

static void
set_device (PpNewPrinterDialog *self,
            PpPrintDevice      *device,
            GtkTreeIter        *iter)
{
  GtkTreeIter                titer;
  gint                       acquisition_method;

  if (device != NULL)
    {
      acquisition_method = pp_print_device_get_acquisition_method (device);
      if (pp_print_device_get_display_name (device) &&
          (pp_print_device_get_device_id (device) ||
           pp_print_device_get_device_ppd (device) ||
           (pp_print_device_get_host_name (device) &&
            acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           (pp_print_device_get_device_uri (device) &&
            (acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
             acquisition_method == ACQUISITION_METHOD_LPD)) ||
           acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
           acquisition_method == ACQUISITION_METHOD_SAMBA))
        {
          g_autofree gchar *description = NULL;

          description = get_local_scheme_description_from_uri (pp_print_device_get_device_uri (device));
          if (description == NULL)
            {
              if (pp_print_device_get_device_location (device) != NULL && pp_print_device_get_device_location (device)[0] != '\0')
                {
                  /* Translators: Location of found network printer (e.g. Kitchen, Reception) */
                  description = g_strdup_printf (_("Location: %s"), pp_print_device_get_device_location (device));
                }
              else if (pp_print_device_get_host_name (device) != NULL && pp_print_device_get_host_name (device)[0] != '\0')
                {
                  /* Translators: Network address of found printer */
                  description = g_strdup_printf (_("Address: %s"), pp_print_device_get_host_name (device));
                }
            }

          if (iter == NULL)
            gtk_list_store_append (self->devices_liststore, &titer);

          gtk_list_store_set (self->devices_liststore, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, pp_print_device_is_network_device (device) ? self->remote_printer_icon : self->local_printer_icon,
                              DEVICE_NAME_COLUMN, pp_print_device_get_device_name (device),
                              DEVICE_DISPLAY_NAME_COLUMN, pp_print_device_get_display_name (device),
                              DEVICE_DESCRIPTION_COLUMN, description,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              DEVICE_COLUMN, device,
                              -1);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          if (iter == NULL)
            gtk_list_store_append (self->devices_liststore, &titer);

          gtk_list_store_set (self->devices_liststore, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, self->authenticated_server_icon,
                              DEVICE_NAME_COLUMN, pp_print_device_get_host_name (device),
                              DEVICE_DISPLAY_NAME_COLUMN, pp_print_device_get_host_name (device),
                              /* Translators: This item is a server which needs authentication to show its printers */
                              DEVICE_DESCRIPTION_COLUMN, _("Server requires authentication"),
                              SERVER_NEEDS_AUTHENTICATION_COLUMN, TRUE,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              DEVICE_COLUMN, device,
                              -1);
        }
    }
}

static void
replace_device (PpNewPrinterDialog *self,
                PpPrintDevice      *old_device,
                PpPrintDevice      *new_device)
{
  GtkTreeIter                iter;
  gboolean                   cont;

  if (old_device != NULL && new_device != NULL)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->devices_liststore), &iter);
      while (cont)
        {
          g_autoptr(PpPrintDevice) device = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->devices_liststore), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          if (old_device == device)
            {
              set_device (self, new_device, &iter);
              break;
            }

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->devices_liststore), &iter);
        }
    }
}

static void
cups_get_dests_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpCupsDests               *dests;
  g_autoptr(GError)          error = NULL;

  dests = pp_cups_get_dests_finish (PP_CUPS (source_object), res, &error);

  if (dests)
    {
      self->dests = dests->dests;
      self->num_of_dests = dests->num_of_dests;

      get_cups_devices (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          get_cups_devices (self);
        }
    }
}

static void
row_activated_cb (PpNewPrinterDialog *self)
{
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->devices_treeview),
                                              &model,
                                              &iter);

  if (selected)
    {
      gtk_tree_model_get (model, &iter, SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed, -1);

      if (authentication_needed)
        {
          authenticate_samba_server (self);
        }
      else
        {
          add_cb (self);
        }
    }
}

static void
cell_data_func (GtkTreeViewColumn  *tree_column,
                GtkCellRenderer    *cell,
                GtkTreeModel       *tree_model,
                GtkTreeIter        *iter,
                gpointer            user_data)
{
  PpNewPrinterDialog        *self = user_data;
  gboolean                   selected = FALSE;
  g_autofree gchar          *name = NULL;
  g_autofree gchar          *description = NULL;

  selected = gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (self->devices_treeview), iter);

  gtk_tree_model_get (tree_model, iter,
                      DEVICE_DISPLAY_NAME_COLUMN, &name,
                      DEVICE_DESCRIPTION_COLUMN, &description,
                      -1);

  if (name != NULL)
    {
      g_autofree gchar *text = NULL;

      if (description != NULL)
        {
          if (selected)
            text = g_markup_printf_escaped ("<b>%s</b>\n<small>%s</small>",
                                            name,
                                            description);
          else
            text = g_markup_printf_escaped ("<b>%s</b>\n<small><span foreground=\"#555555\">%s</span></small>",
                                            name,
                                            description);
        }
      else
        {
          text = g_markup_printf_escaped ("<b>%s</b>\n ",
                                          name);
        }

      g_object_set (G_OBJECT (cell),
                    "markup", text,
                    NULL);
    }
}

static void
populate_devices_list (PpNewPrinterDialog *self)
{
  GtkTreeViewColumn         *column;
  g_autoptr(PpSamba)         samba = NULL;
  g_autoptr(GEmblem)         emblem = NULL;
  g_autoptr(PpCups)          cups = NULL;
  g_autoptr(GIcon)           icon = NULL;
  g_autoptr(GIcon)           emblem_icon = NULL;
  GtkCellRenderer           *text_renderer;
  GtkCellRenderer           *icon_renderer;

  g_signal_connect_object (gtk_tree_view_get_selection (self->devices_treeview),
                           "changed", G_CALLBACK (device_selection_changed_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->devices_treeview,
                           "row-activated", G_CALLBACK (row_activated_cb), self, G_CONNECT_SWAPPED);

  self->local_printer_icon = g_themed_icon_new ("printer");
  self->remote_printer_icon = g_themed_icon_new ("printer-network");

  icon = g_themed_icon_new ("network-server");
  emblem_icon = g_themed_icon_new ("changes-prevent");
  emblem = g_emblem_new (emblem_icon);

  self->authenticated_server_icon = g_emblemed_icon_new (icon, emblem);

  icon_renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (icon_renderer, "icon-size", GTK_ICON_SIZE_LARGE, NULL);
  gtk_cell_renderer_set_alignment (icon_renderer, 1.0, 0.5);
  gtk_cell_renderer_set_padding (icon_renderer, 4, 4);
  column = gtk_tree_view_column_new_with_attributes ("Icon", icon_renderer,
                                                     "gicon", DEVICE_GICON_COLUMN, NULL);
  gtk_tree_view_column_set_max_width (column, -1);
  gtk_tree_view_column_set_min_width (column, 80);
  gtk_tree_view_append_column (self->devices_treeview, column);


  text_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Devices", text_renderer,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, text_renderer, cell_data_func,
                                           self, NULL);
  gtk_tree_view_append_column (self->devices_treeview, column);

  gtk_tree_model_filter_set_visible_column (self->devices_model_filter, DEVICE_VISIBLE_COLUMN);

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups, self->cancellable, cups_get_dests_cb, self);

  self->samba_searching = TRUE;
  update_dialog_state (self);

  samba = pp_samba_new (NULL);
  pp_samba_get_devices_async (samba, FALSE, self->cancellable, get_samba_devices_cb, self);
}

static void
ppd_selection_cb (GtkWindow *_dialog,
                  gint       response_id,
                  gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;
  GList                     *original_names_list = NULL;
  g_autofree gchar          *ppd_name = NULL;
  g_autofree gchar          *ppd_display_name = NULL;
  gint                       acquisition_method;

  if (response_id == GTK_RESPONSE_OK) {
      ppd_name = pp_ppd_selection_dialog_get_ppd_name (self->ppd_selection_dialog);
      ppd_display_name = pp_ppd_selection_dialog_get_ppd_display_name (self->ppd_selection_dialog);
  }

  if (ppd_name)
    {
      g_object_set (self->new_device, "device-ppd", ppd_name, NULL);

      acquisition_method = pp_print_device_get_acquisition_method (self->new_device);
      if ((acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
           acquisition_method == ACQUISITION_METHOD_LPD) &&
          ppd_display_name != NULL)
        {
          g_autofree gchar *printer_name = NULL;

          g_object_set (self->new_device,
                        "device-name", ppd_display_name,
                        "device-original-name", ppd_display_name,
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (self->devices_liststore),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          printer_name = canonicalize_device_name (original_names_list,
                                                   self->local_cups_devices,
                                                   self->dests,
                                                   self->num_of_dests,
                                                   self->new_device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (self->new_device,
                        "device-name", printer_name,
                        "device-original-name", printer_name,
                        NULL);
        }
    }

  /* This is needed here since parent dialog is destroyed first. */
  gtk_window_set_transient_for (GTK_WINDOW (self->ppd_selection_dialog), NULL);

  self->user_callback (GTK_WINDOW (self), response_id, self->user_data);
}

static void
cancel_cb (PpNewPrinterDialog *self)
{
  self->user_callback (GTK_WINDOW (self), GTK_RESPONSE_CANCEL, self->user_data);
}

static void
add_cb (PpNewPrinterDialog *self)
{
  g_autoptr(PpPrintDevice)   device = NULL;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  gint                       acquisition_method;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->devices_treeview), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DEVICE_COLUMN, &device,
                          -1);
    }

  if (device)
    {
      acquisition_method = pp_print_device_get_acquisition_method (device);
      if (acquisition_method == ACQUISITION_METHOD_SAMBA ||
          acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
          acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
          acquisition_method == ACQUISITION_METHOD_LPD)
        {
          self->new_device = pp_print_device_copy (device);
          self->ppd_selection_dialog =
            pp_ppd_selection_dialog_new (self->list,
                                         NULL,
                                         ppd_selection_cb,
                                         self);

          gtk_window_set_transient_for (GTK_WINDOW (self->ppd_selection_dialog),
                                        GTK_WINDOW (self));

          /* New device will be set at return from ppd selection */
          gtk_widget_set_visible (GTK_WIDGET (self->ppd_selection_dialog), TRUE);
        }
      else
        {
          self->new_device = pp_print_device_copy (device);
          self->user_callback (GTK_WINDOW (self), GTK_RESPONSE_OK, self->user_data);
        }
    }
}

PpNewPrinterDialog *
pp_new_printer_dialog_new (PPDList              *ppd_list,
                           UserResponseCallback  user_callback,
                           gpointer              user_data)
{
  PpNewPrinterDialog *self;

  self = g_object_new (pp_new_printer_dialog_get_type (), NULL);

  self->user_callback = user_callback;
  self->user_data = user_data;

  self->list = ppd_list_copy (ppd_list);

  self->local_cups_devices = g_ptr_array_new_with_free_func (g_object_unref);

  /* GCancellable for cancelling of async operations */
  self->cancellable = g_cancellable_new ();

  g_signal_connect_object (self->search_entry, "activate", G_CALLBACK (search_entry_activated_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->search_entry, "search-changed", G_CALLBACK (search_entry_changed_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->unlock_button, "clicked", G_CALLBACK (authenticate_samba_server), self, G_CONNECT_SWAPPED);

  /* Authentication form widgets */
  g_signal_connect_object (self->username_entry, "changed", G_CALLBACK (auth_entries_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->password_entry, "changed", G_CALLBACK (auth_entries_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->go_back_button, "clicked", G_CALLBACK (on_go_back_button_clicked), self, G_CONNECT_SWAPPED);

  /* Fill with data */
  populate_devices_list (self);

  return self;
}

static gboolean
pp_new_printer_dialog_close_request (GtkWindow *window)
{
  PpNewPrinterDialog *self = PP_NEW_PRINTER_DIALOG (window);

  cancel_cb (self);

  return GDK_EVENT_STOP;
}

static void
pp_new_printer_dialog_dispose (GObject *object)
{
  PpNewPrinterDialog *self = PP_NEW_PRINTER_DIALOG (object);

  g_cancellable_cancel (self->remote_host_cancellable);
  g_cancellable_cancel (self->cancellable);

  g_clear_handle_id (&self->host_search_timeout_id, g_source_remove);
  g_clear_object (&self->remote_host_cancellable);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->list, ppd_list_free);
  g_clear_pointer (&self->local_cups_devices, g_ptr_array_unref);
  g_clear_object (&self->new_device);
  g_clear_object (&self->local_printer_icon);
  g_clear_object (&self->remote_printer_icon);
  g_clear_object (&self->authenticated_server_icon);
  g_clear_object (&self->snmp_host);
  g_clear_object (&self->socket_host);
  g_clear_object (&self->lpd_host);
  g_clear_object (&self->remote_cups_host);
  g_clear_object (&self->samba_host);

  if (self->ppd_selection_dialog != NULL)
    {
      gtk_window_destroy (GTK_WINDOW (self->ppd_selection_dialog));
      self->ppd_selection_dialog = NULL;
    }

  if (self->num_of_dests > 0)
    {
      cupsFreeDests (self->num_of_dests, self->dests);
      self->num_of_dests = 0;
      self->dests = NULL;
    }

  G_OBJECT_CLASS (pp_new_printer_dialog_parent_class)->dispose (object);
}

void
pp_new_printer_dialog_class_init (PpNewPrinterDialogClass *klass)
{
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (PP_TYPE_PRINT_DEVICE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/new-printer-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, devices_liststore);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, devices_model_filter);

  /* headerbar */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, header_title);

  /* headerbar topleft buttons */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, headerbar_topleft_buttons);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, go_back_button);

  /* headerbar topright buttons */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, headerbar_topright_buttons);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, new_printer_add_button);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, unlock_button);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, authenticate_button);

  /* dialogstack */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, dialog_stack);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, stack);

  /* scrolledwindow1 */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, scrolledwindow1);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, devices_treeview);

  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, search_entry);

  /* authentication page */
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, authentication_title);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, authentication_text);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, username_entry);
  gtk_widget_class_bind_template_child (widget_class, PpNewPrinterDialog, password_entry);

  gtk_widget_class_bind_template_callback (widget_class, add_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_cb);

  object_class->dispose = pp_new_printer_dialog_dispose;
  window_class->close_request = pp_new_printer_dialog_close_request;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}


void
pp_new_printer_dialog_init (PpNewPrinterDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
pp_new_printer_dialog_set_ppd_list (PpNewPrinterDialog *self,
                                    PPDList            *list)
{
  self->list = ppd_list_copy (list);

  if (self->ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (self->ppd_selection_dialog, self->list);
}

PpNewPrinter *
pp_new_printer_dialog_get_new_printer (PpNewPrinterDialog *self)
{
  PpNewPrinter *new_printer = NULL;

  new_printer = pp_new_printer_new ();
  g_object_set (new_printer,
                "name", pp_print_device_get_device_name (self->new_device),
                "original-name", pp_print_device_get_device_original_name (self->new_device),
                "device-uri", pp_print_device_get_device_uri (self->new_device),
                "device-id", pp_print_device_get_device_id (self->new_device),
                "ppd-name", pp_print_device_get_device_ppd (self->new_device),
                "ppd-file-name", pp_print_device_get_device_ppd (self->new_device),
                "info", pp_print_device_get_device_info (self->new_device),
                "location", pp_print_device_get_device_location (self->new_device),
                "make-and-model", pp_print_device_get_device_make_and_model (self->new_device),
                "host-name", pp_print_device_get_host_name (self->new_device),
                "host-port", pp_print_device_get_host_port (self->new_device),
                "is-network-device", pp_print_device_is_network_device (self->new_device),
                "window-id", 0,
                NULL);

  return new_printer;
}
