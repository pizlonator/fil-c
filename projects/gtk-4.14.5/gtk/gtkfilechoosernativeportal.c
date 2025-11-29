/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativeportal.c: Portal File selector dialog
 * Copyright (C) 2015, Red Hat, Inc.
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

#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include "gtkprivate.h"
#include "deprecated/gtkdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkheaderbar.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkfilefilterprivate.h"
#include "gtkwindowprivate.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct {
  GtkFileChooserNative *self;

  GtkWidget *grab_widget;

  GDBusConnection *connection;
  char *portal_handle;
  guint portal_response_signal_id;
  gboolean modal;

  gboolean hidden;

  const char *method_name;

  char *exported_handle;
  GtkWindow *exported_window;
  PortalErrorHandler error_handler;
} FilechooserPortalData;


static void
filechooser_portal_data_clear (FilechooserPortalData *data)
{
  if (data->portal_response_signal_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (data->connection,
                                            data->portal_response_signal_id);
      data->portal_response_signal_id = 0;
    }

  g_clear_object (&data->connection);

  if (data->grab_widget)
    {
      gtk_grab_remove (data->grab_widget);
      g_clear_object (&data->grab_widget);
    }

  g_clear_object (&data->self);

  if (data->exported_window)
    {
      if (data->exported_handle)
        {
          gtk_window_unexport_handle (data->exported_window, data->exported_handle);
          g_clear_pointer (&data->exported_handle, g_free);
        }
      g_clear_object (&data->exported_window);
    }

  g_clear_pointer (&data->portal_handle, g_free);
}

static void
filechooser_portal_data_free (FilechooserPortalData *data)
{
  if (data != NULL)
    {
      filechooser_portal_data_clear (data);
      g_free (data);
    }
}

static void
response_cb (GDBusConnection  *connection,
             const char       *sender_name,
             const char       *object_path,
             const char       *interface_name,
             const char       *signal_name,
             GVariant         *parameters,
             gpointer          user_data)
{
  GtkFileChooserNative *self = user_data;
  FilechooserPortalData *data = self->mode_data;
  guint32 portal_response;
  int gtk_response;
  const char **uris;
  int i;
  GVariant *response_data;
  GVariant *choices = NULL;
  GVariant *current_filter = NULL;

  g_variant_get (parameters, "(u@a{sv})", &portal_response, &response_data);
  g_variant_lookup (response_data, "uris", "^a&s", &uris);

  choices = g_variant_lookup_value (response_data, "choices", G_VARIANT_TYPE ("a(ss)"));
  if (choices)
    {
      for (i = 0; i < g_variant_n_children (choices); i++)
        {
          const char *id;
          const char *selected;
          g_variant_get_child (choices, i, "(&s&s)", &id, &selected);
          gtk_file_chooser_set_choice (GTK_FILE_CHOOSER (self), id, selected);
        }
      g_variant_unref (choices);
    }

  current_filter = g_variant_lookup_value (response_data, "current_filter", G_VARIANT_TYPE ("(sa(us))"));
  if (current_filter)
    {
      GtkFileFilter *filter = gtk_file_filter_new_from_gvariant (current_filter);
      const char *current_filter_name = gtk_file_filter_get_name (filter);

      /* Try to find  the given filter in the list of filters.
       * Since filters are compared by pointer value, using the passed
       * filter would otherwise not match in a comparison, even if
       * a filter in the list of filters has been selected.
       * We'll use the heuristic that if two filters have the same name,
       * they must be the same.
       * If there is no match, just set the filter as it was retrieved.
       */
      GtkFileFilter *filter_to_select = filter;
      GListModel *filters;
      guint j, n;

      filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (self));
      n = g_list_model_get_n_items (filters);
      for (j = 0; j < n; j++)
        {
          GtkFileFilter *f = g_list_model_get_item (filters, j);
          if (g_strcmp0 (gtk_file_filter_get_name (f), current_filter_name) == 0)
            {
              filter_to_select = f;
              break;
            }
          g_object_unref (f);
        }
      g_object_unref (filters);
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self), filter_to_select);
      g_object_unref (filter_to_select);

      g_variant_unref (current_filter);
    }

  g_slist_free_full (self->custom_files, g_object_unref);
  self->custom_files = NULL;
  for (i = 0; uris[i]; i++)
    self->custom_files = g_slist_prepend (self->custom_files, g_file_new_for_uri (uris[i]));
  self->custom_files = g_slist_reverse (self->custom_files);

  g_free (uris);
  g_variant_unref (response_data);

  switch (portal_response)
    {
    case 0:
      gtk_response = GTK_RESPONSE_ACCEPT;
      break;
    case 1:
      gtk_response = GTK_RESPONSE_CANCEL;
      break;
    case 2:
    default:
      gtk_response = GTK_RESPONSE_DELETE_EVENT;
      break;
    }

  /* Keep a reference on the native dialog until we can emit the response
   * signal; filechooser_portal_data_free() will drop a reference on the
   * dialog as well
   */
  g_object_ref (self);

  filechooser_portal_data_free (data);
  self->mode_data = NULL;

  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), gtk_response);

  g_object_unref (self);
}

static void
send_close (FilechooserPortalData *data)
{
  GDBusMessage *message;
  GError *error = NULL;

  message = g_dbus_message_new_method_call (PORTAL_BUS_NAME,
                                            data->portal_handle,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Close");

  if (!g_dbus_connection_send_message (data->connection,
                                       message,
                                       G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                       NULL, &error))
    {
      g_warning ("unable to send FileChooser Close message: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (message);
}

static void
open_file_msg_cb (GObject *source_object,
                  GAsyncResult *res,
                  gpointer user_data)
{
  FilechooserPortalData *data = user_data;
  GtkFileChooserNative *self = data->self;
  GDBusMessage *reply;
  GError *error = NULL;
  char *handle = NULL;

  reply = g_dbus_connection_send_message_with_reply_finish (data->connection, res, &error);

  if (reply && g_dbus_message_to_gerror (reply, &error))
    g_clear_object (&reply);

  if (reply == NULL)
    {
      if (!data->hidden && data->error_handler)
        {
          data->error_handler (self);
          filechooser_portal_data_free (data);
          self->mode_data = NULL;
        }
      g_error_free (error);
      return;
    }

  g_variant_get_child (g_dbus_message_get_body (reply), 0, "o", &handle);

  if (data->hidden)
    {
      /* The dialog was hidden before we got the handle, close it now */
      filechooser_portal_data_free (data);
      self->mode_data = NULL;
    }
  else if (strcmp (handle, data->portal_handle) != 0)
    {
      g_free (data->portal_handle);
      data->portal_handle = g_steal_pointer (&handle);
      g_dbus_connection_signal_unsubscribe (data->connection,
                                            data->portal_response_signal_id);

      data->portal_response_signal_id =
        g_dbus_connection_signal_subscribe (data->connection,
                                            PORTAL_BUS_NAME,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Response",
                                            data->portal_handle,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            response_cb,
                                            self, NULL);
    }

  g_object_unref (reply);
  g_free (handle);
}

static GVariant *
get_filters (GtkFileChooser *self)
{
  GListModel *filters;
  guint n, i;
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sa(us))"));
  filters = gtk_file_chooser_get_filters (self);
  n = g_list_model_get_n_items (filters);
  for (i = 0; i < n; i++)
    {
      GtkFileFilter *filter = g_list_model_get_item (filters, i);
      g_variant_builder_add (&builder, "@(sa(us))", gtk_file_filter_to_gvariant (filter));
      g_object_unref (filter);
    }
  g_object_unref (filters);

  return g_variant_builder_end (&builder);
}

static GVariant *
gtk_file_chooser_native_choice_to_variant (GtkFileChooserNativeChoice *choice)
{
  GVariantBuilder choices;
  int i;

  g_variant_builder_init (&choices, G_VARIANT_TYPE ("a(ss)"));
  if (choice->options)
    {
      for (i = 0; choice->options[i]; i++)
        g_variant_builder_add (&choices, "(&s&s)", choice->options[i], choice->option_labels[i]);
    }

  return g_variant_new ("(&s&s@a(ss)&s)",
                        choice->id,
                        choice->label,
                        g_variant_builder_end (&choices),
                        choice->selected ? choice->selected : "");
}

static GVariant *
serialize_choices (GtkFileChooserNative *self)
{
  GVariantBuilder builder;
  GSList *l;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssa(ss)s)"));
  for (l = self->choices; l; l = l->next)
    {
      GtkFileChooserNativeChoice *choice = l->data;

      g_variant_builder_add (&builder, "@(ssa(ss)s)",
                             gtk_file_chooser_native_choice_to_variant (choice));
    }

  return g_variant_builder_end (&builder);
}

static void
show_portal_file_chooser (GtkFileChooserNative *self,
                          const char           *parent_window_str)
{
  FilechooserPortalData *data = self->mode_data;
  GDBusMessage *message;
  GVariantBuilder opt_builder;
  gboolean multiple;
  gboolean directory;
  const char *title;
  char *token;

  message = g_dbus_message_new_method_call (PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            PORTAL_FILECHOOSER_INTERFACE,
                                            data->method_name);

  data->portal_handle = gtk_get_portal_request_path (data->connection, &token);
  data->portal_response_signal_id =
    g_dbus_connection_signal_subscribe (data->connection,
                                        PORTAL_BUS_NAME,
                                        PORTAL_REQUEST_INTERFACE,
                                        "Response",
                                        data->portal_handle,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        response_cb,
                                        self, NULL);

  multiple = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self));
  directory = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self)) == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);

  g_variant_builder_add (&opt_builder, "{sv}", "handle_token",
                         g_variant_new_string (token));
  g_free (token);

  g_variant_builder_add (&opt_builder, "{sv}", "multiple",
                         g_variant_new_boolean (multiple));
  g_variant_builder_add (&opt_builder, "{sv}", "directory",
                         g_variant_new_boolean (directory));
  if (self->accept_label)
    g_variant_builder_add (&opt_builder, "{sv}", "accept_label",
                           g_variant_new_string (self->accept_label));
  if (self->cancel_label)
    g_variant_builder_add (&opt_builder, "{sv}", "cancel_label",
                           g_variant_new_string (self->cancel_label));
  g_variant_builder_add (&opt_builder, "{sv}", "modal",
                         g_variant_new_boolean (data->modal));
  g_variant_builder_add (&opt_builder, "{sv}", "filters", get_filters (GTK_FILE_CHOOSER (self)));
  if (self->current_filter)
    g_variant_builder_add (&opt_builder, "{sv}", "current_filter",
                           gtk_file_filter_to_gvariant (self->current_filter));
  if (self->current_name)
    g_variant_builder_add (&opt_builder, "{sv}", "current_name",
                           g_variant_new_string (GTK_FILE_CHOOSER_NATIVE (self)->current_name));
  if (self->current_folder)
    {
      char *path;

      path = g_file_get_path (GTK_FILE_CHOOSER_NATIVE (self)->current_folder);
      g_variant_builder_add (&opt_builder, "{sv}", "current_folder",
                             g_variant_new_bytestring (path));
      g_free (path);
    }
  if (self->current_file)
    {
      char *path;

      path = g_file_get_path (GTK_FILE_CHOOSER_NATIVE (self)->current_file);
      g_variant_builder_add (&opt_builder, "{sv}", "current_file",
                             g_variant_new_bytestring (path));
      g_free (path);
    }

  if (self->choices)
    g_variant_builder_add (&opt_builder, "{sv}", "choices",
                           serialize_choices (GTK_FILE_CHOOSER_NATIVE (self)));

  title = gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self));

  g_dbus_message_set_body (message,
                           g_variant_new ("(ss@a{sv})",
                                          parent_window_str ? parent_window_str : "",
                                          title ? title : "",
                                          g_variant_builder_end (&opt_builder)));

  g_dbus_connection_send_message_with_reply (data->connection,
                                             message,
                                             G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                             G_MAXINT,
                                             NULL,
                                             NULL,
                                             open_file_msg_cb,
                                             data);

  g_object_unref (message);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    user_data)
{
  GtkFileChooserNative *self = user_data;
  FilechooserPortalData *data = self->mode_data;

  if (data->modal)
    {
      data->grab_widget = g_object_ref_sink (gtk_label_new (""));
      gtk_grab_add (GTK_WIDGET (data->grab_widget));
    }

  data->exported_handle = g_strdup (handle_str);
  show_portal_file_chooser (self, handle_str);
}

gboolean
gtk_file_chooser_native_portal_show (GtkFileChooserNative *self,
                                     PortalErrorHandler    error_handler)
{
  FilechooserPortalData *data;
  GtkWindow *transient_for;
  GDBusConnection *connection;
  GtkFileChooserAction action;
  const char *method_name;

  if (!self->use_portal && !gdk_should_use_portal ())
    return FALSE;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (connection == NULL)
    return FALSE;

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self));

  if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
    method_name = "OpenFile";
  else if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    method_name = "SaveFile";
  else if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (gtk_get_portal_interface_version (connection, "org.freedesktop.portal.FileChooser") < 3)
        {
          g_warning ("GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER is not supported by GtkFileChooserNativePortal because portal is too old");
          return FALSE;
        }
      method_name = "OpenFile";
    }
  else
    {
      g_warning ("GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER is not supported by GtkFileChooserNativePortal");
      return FALSE;
    }

  data = g_new0 (FilechooserPortalData, 1);
  data->self = g_object_ref (self);
  data->connection = connection;
  data->error_handler = error_handler;

  data->method_name = method_name;

  if (gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)))
    data->modal = TRUE;

  self->mode_data = data;

  transient_for = gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self));
  if (transient_for != NULL && gtk_widget_is_visible (GTK_WIDGET (transient_for)))
    {
      if (!gtk_window_export_handle (transient_for,
                                     window_handle_exported,
                                     self))
        {
          g_warning ("Failed to export handle, could not set transient for");
          show_portal_file_chooser (self, NULL);
        }
      else
        {
          data->exported_window = g_object_ref (transient_for);
        }
    }
  else
    {
      show_portal_file_chooser (self, NULL);
    }

  return TRUE;
}

void
gtk_file_chooser_native_portal_hide (GtkFileChooserNative *self)
{
  FilechooserPortalData *data = self->mode_data;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  data->hidden = TRUE;

  if (data->portal_handle)
    send_close (data);

  /* We clear the data because we might have in-flight async
   * operations that can still access it
   */
  filechooser_portal_data_clear (data);
  self->mode_data = NULL;
}
