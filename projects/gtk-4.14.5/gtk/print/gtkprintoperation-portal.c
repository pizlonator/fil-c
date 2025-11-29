/* GTK - The GIMP Toolkit
 * gtkprintoperation-portal.c: Print Operation Details for sandboxed apps
 * Copyright (C) 2016, Red Hat, Inc.
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

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <gio/gunixfdlist.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkwindowprivate.h"

#include "gtkprintoperation-private.h"
#include "gtkprintoperation-portal.h"
#include "gtkprintsettings.h"
#include "gtkpagesetup.h"
#include "gtkprintbackendprivate.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define PORTAL_PRINT_INTERFACE "org.freedesktop.portal.Print"

static char *
get_portal_request_path (GDBusConnection  *connection,
                         char            **token)
{
  char *sender;
  int i;
  char *path;

  *token = g_strdup_printf ("gtk%d", g_random_int_range (0, G_MAXINT));
  /* +1 to skip the leading : */
  sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
  for (i = 0; sender[i]; i++)
    if (sender[i] == '.')
      sender[i] = '_';

  path = g_strconcat (PORTAL_OBJECT_PATH, "/request/", sender, "/", *token, NULL);

  g_free (sender);

  return path;
}

typedef struct {
  GtkPrintOperation *op;
  GDBusProxy *proxy;
  guint response_signal_id;
  gboolean do_print;
  GtkPrintOperationResult result;
  GtkPrintOperationPrintFunc print_cb;
  GtkWindow *parent;
  char *handle;
  GMainLoop *loop;
  guint32 token;
  GDestroyNotify destroy;
  GVariant *settings;
  GVariant *setup;
  GVariant *options;
  char *prepare_print_handle;
} PortalData;

static void
portal_data_free (gpointer data)
{
  PortalData *portal = data;

  if (portal->parent && portal->handle)
    gtk_window_unexport_handle (portal->parent, portal->handle);
  g_free (portal->handle);
  g_object_unref (portal->op);
  g_object_unref (portal->proxy);
  if (portal->loop)
    g_main_loop_unref (portal->loop);
  if (portal->settings)
    g_variant_unref (portal->settings);
  if (portal->setup)
    g_variant_unref (portal->setup);
  if (portal->options)
    g_variant_unref (portal->options);
  g_free (portal->prepare_print_handle);
  g_free (portal);
}

typedef struct {
  GDBusProxy *proxy;
  GtkPrintJob *job;
  guint32 token;
  cairo_surface_t *surface;
  GMainLoop *loop;
  gboolean file_written;
} GtkPrintOperationPortal;

static void
op_portal_free (GtkPrintOperationPortal *op_portal)
{
  g_clear_object (&op_portal->proxy);
  g_clear_object (&op_portal->job);
  if (op_portal->loop)
    g_main_loop_unref (op_portal->loop);
  g_free (op_portal);
}

static void
portal_start_page (GtkPrintOperation *op,
                   GtkPrintContext   *print_context,
                   GtkPageSetup      *page_setup)
{
  GtkPrintOperationPortal *op_portal = op->priv->platform_data;
  GtkPaperSize *paper_size;
  cairo_surface_type_t type;
  double w, h;

  paper_size = gtk_page_setup_get_paper_size (page_setup);

  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);

  type = cairo_surface_get_type (op_portal->surface);

  if ((op->priv->manual_number_up < 2) ||
      (op->priv->page_position % op->priv->manual_number_up == 0))
    {
      if (type == CAIRO_SURFACE_TYPE_PS)
        {
          cairo_ps_surface_set_size (op_portal->surface, w, h);
          cairo_ps_surface_dsc_begin_page_setup (op_portal->surface);
          switch (gtk_page_setup_get_orientation (page_setup))
            {
              case GTK_PAGE_ORIENTATION_PORTRAIT:
              case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
                cairo_ps_surface_dsc_comment (op_portal->surface, "%%PageOrientation: Portrait");
                break;

              case GTK_PAGE_ORIENTATION_LANDSCAPE:
              case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
                cairo_ps_surface_dsc_comment (op_portal->surface, "%%PageOrientation: Landscape");
                break;

              default:
                break;
            }
         }
      else if (type == CAIRO_SURFACE_TYPE_PDF)
        {
          if (!op->priv->manual_orientation)
            {
              w = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
              h = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
            }
          cairo_pdf_surface_set_size (op_portal->surface, w, h);
        }
    }
}

static void
portal_end_page (GtkPrintOperation *op,
                 GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo_context (print_context);

  if ((op->priv->manual_number_up < 2) ||
      ((op->priv->page_position + 1) % op->priv->manual_number_up == 0) ||
      (op->priv->page_position == op->priv->nr_of_pages_to_print - 1))
    cairo_show_page (cr);
}

static void
print_file_done (GObject *source,
                 GAsyncResult *result,
                 gpointer data)
{
  GtkPrintOperation *op = data;
  GtkPrintOperationPortal *op_portal = op->priv->platform_data;
  GError *error = NULL;
  GVariant *ret;

  ret = g_dbus_proxy_call_finish (op_portal->proxy,
                                  result,
                                  &error);
  if (ret == NULL)
    {
      if (op->priv->error == NULL)
        op->priv->error = g_error_copy (error);
      g_warning ("Print file failed: %s", error->message);
      g_error_free (error);
    }
  else
    g_variant_unref (ret);

  if (op_portal->loop)
    g_main_loop_quit (op_portal->loop);

  g_object_unref (op);
}

static void
portal_job_complete (GtkPrintJob  *job,
                     gpointer      data,
                     const GError *error)
{
  GtkPrintOperation *op = data;
  GtkPrintOperationPortal *op_portal = op->priv->platform_data;
  GtkPrintSettings *settings;
  const char *uri;
  char *filename;
  int fd, idx;
  GVariantBuilder opt_builder;
  GUnixFDList *fd_list;

  if (error != NULL && op->priv->error == NULL)
    {
      g_warning ("Print job failed: %s", error->message);
      op->priv->error = g_error_copy (error);
      return;
    }

  op_portal->file_written = TRUE;

  settings = gtk_print_job_get_settings (job);
  uri = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_URI);
  filename = g_filename_from_uri (uri, NULL, NULL);

  fd = open (filename, O_RDONLY|O_CLOEXEC);
  fd_list = g_unix_fd_list_new ();
  idx = g_unix_fd_list_append (fd_list, fd, NULL);
  close (fd);

  g_free (filename);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}",  "token", g_variant_new_uint32 (op_portal->token));

  g_dbus_proxy_call_with_unix_fd_list (op_portal->proxy,
                                       "Print",
                                       g_variant_new ("(ssh@a{sv})",
                                                      "", /* window */
                                                      _("Print"), /* title */
                                                      idx,
                                                      g_variant_builder_end (&opt_builder)),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       fd_list,
                                       NULL,
                                       print_file_done,
                                       op);
  g_object_unref (fd_list);
}

static void
portal_end_run (GtkPrintOperation *op,
                gboolean           wait,
                gboolean           cancelled)
{
  GtkPrintOperationPortal *op_portal = op->priv->platform_data;

  cairo_surface_finish (op_portal->surface);

  if (cancelled)
    return;

  if (wait)
    op_portal->loop = g_main_loop_new (NULL, FALSE);

  /* TODO: Check for error */
  if (op_portal->job != NULL)
    {
      g_object_ref (op);
      gtk_print_job_send (op_portal->job, portal_job_complete, op, NULL);
    }

  if (wait)
    {
      g_object_ref (op);
      if (!op_portal->file_written)
        g_main_loop_run (op_portal->loop);
      g_object_unref (op);
    }
}

static void
finish_print (PortalData        *portal,
              GtkPrinter        *printer,
              GtkPageSetup      *page_setup,
              GtkPrintSettings  *settings)
{
  GtkPrintOperation *op = portal->op;
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPrintJob *job;
  GtkPrintOperationPortal *op_portal;
  cairo_t *cr;

  if (portal->do_print)
    {
      gtk_print_operation_set_print_settings (op, settings);
      priv->print_context = _gtk_print_context_new (op);

      _gtk_print_context_set_hard_margins (priv->print_context, 0, 0, 0, 0);

      gtk_print_operation_set_default_page_setup (op, page_setup);
      _gtk_print_context_set_page_setup (priv->print_context, page_setup);

      op_portal = g_new0 (GtkPrintOperationPortal, 1);
      priv->platform_data = op_portal;
      priv->free_platform_data = (GDestroyNotify) op_portal_free;

      priv->start_page = portal_start_page;
      priv->end_page = portal_end_page;
      priv->end_run = portal_end_run;

      job = gtk_print_job_new (priv->job_name, printer, settings, page_setup);
      op_portal->job = job;

      op_portal->proxy = g_object_ref (portal->proxy);
      op_portal->token = portal->token;

      op_portal->surface = gtk_print_job_get_surface (job, &priv->error);
      if (op_portal->surface == NULL)
        {
          portal->result = GTK_PRINT_OPERATION_RESULT_ERROR;
          portal->do_print = FALSE;
          goto out;
        }

      cr = cairo_create (op_portal->surface);
      gtk_print_context_set_cairo_context (priv->print_context, cr, 72, 72);
      cairo_destroy (cr);

      priv->print_pages = gtk_print_job_get_pages (job);
      priv->page_ranges = gtk_print_job_get_page_ranges (job, &priv->num_page_ranges);
      priv->manual_num_copies = gtk_print_job_get_num_copies (job);
      priv->manual_collation = gtk_print_job_get_collate (job);
      priv->manual_reverse = gtk_print_job_get_reverse (job);
      priv->manual_page_set = gtk_print_job_get_page_set (job);
      priv->manual_scale = gtk_print_job_get_scale (job);
      priv->manual_orientation = gtk_print_job_get_rotate (job);
      priv->manual_number_up = gtk_print_job_get_n_up (job);
      priv->manual_number_up_layout = gtk_print_job_get_n_up_layout (job);
    }

out:
  if (portal->print_cb)
    portal->print_cb (op, portal->parent, portal->do_print, portal->result);

  if (portal->destroy)
    portal->destroy (portal);
}

static GtkPrinter *
find_file_printer (void)
{
  GList *backends, *l, *printers;
  GtkPrinter *printer;

  printer = NULL;

  backends = gtk_print_backend_load_modules ();
  for (l = backends; l; l = l->next)
    {
      GtkPrintBackend *backend = l->data;

      /* FIXME: this needs changes for cpdb */
      if (strcmp (G_OBJECT_TYPE_NAME (backend), "GtkPrintBackendFile") == 0)
        {
          printers = gtk_print_backend_get_printer_list (backend);
          printer = printers->data;
          g_list_free (printers);
          break;
        }
    }
  g_list_free (backends);

  return printer;
}

static void
prepare_print_response (GDBusConnection *connection,
                        const char      *sender_name,
                        const char      *object_path,
                        const char      *interface_name,
                        const char      *signal_name,
                        GVariant        *parameters,
                        gpointer         data)
{
  PortalData *portal = data;
  guint32 response;
  GVariant *options = NULL;

  if (portal->response_signal_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (connection,
                                            portal->response_signal_id);
      portal->response_signal_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &options);

  portal->do_print = (response == 0);

  if (portal->do_print)
    {
      GVariant *v;
      GtkPrintSettings *settings;
      GtkPageSetup *page_setup;
      GtkPrinter *printer;

      v = g_variant_lookup_value (options, "settings", G_VARIANT_TYPE_VARDICT);
      settings = gtk_print_settings_new_from_gvariant (v);
      g_variant_unref (v);

      v = g_variant_lookup_value (options, "page-setup", G_VARIANT_TYPE_VARDICT);
      page_setup = gtk_page_setup_new_from_gvariant (v);
      g_variant_unref (v);

      g_variant_lookup (options, "token", "u", &portal->token);

      printer = find_file_printer ();
      if (printer)
        {
          char *filename;
          int fd;
          char *uri;

          fd = g_file_open_tmp ("gtkprintXXXXXX", &filename, NULL);
          uri = g_filename_to_uri (filename, NULL, NULL);
          gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_URI, uri);
          g_free (uri);
          close (fd);

          finish_print (portal, printer, page_setup, settings);
          g_free (filename);

          portal->result = GTK_PRINT_OPERATION_RESULT_APPLY;
        }
      else
        {
          portal->do_print = FALSE;
          portal->result = GTK_PRINT_OPERATION_RESULT_ERROR;
        }
    }
  else
    {
      portal->result = GTK_PRINT_OPERATION_RESULT_CANCEL;

      if (portal->print_cb)
        portal->print_cb (portal->op, portal->parent, portal->do_print, portal->result);

      if (portal->destroy)
        portal->destroy (portal);
    }

  if (options)
    g_variant_unref (options);

  if (portal->loop)
    g_main_loop_quit (portal->loop);
}

static void
prepare_print_called (GObject      *source,
                      GAsyncResult *result,
                      gpointer      data)
{
  PortalData *portal = data;
  GError *error = NULL;
  const char *handle = NULL;
  GVariant *ret;

  ret = g_dbus_proxy_call_finish (portal->proxy, result, &error);
  if (ret == NULL)
    {
      if (portal->op->priv->error == NULL)
        portal->op->priv->error = g_error_copy (error);
      g_error_free (error);
      if (portal->loop)
        g_main_loop_quit (portal->loop);
      return;
    }
  else
    g_variant_get (ret, "(&o)", &handle);

  if (strcmp (portal->prepare_print_handle, handle) != 0)
    {
      g_free (portal->prepare_print_handle);
      portal->prepare_print_handle = g_strdup (handle);
      g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (portal->proxy)),
                                            portal->response_signal_id);
      portal->response_signal_id =
        g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (portal->proxy)),
                                            PORTAL_BUS_NAME,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Response",
                                            handle,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            prepare_print_response,
                                            portal, NULL);
     }

  g_variant_unref (ret);
}

static PortalData *
create_portal_data (GtkPrintOperation          *op,
                    GtkWindow                  *parent,
                    GtkPrintOperationPrintFunc  print_cb)
{
  GDBusProxy *proxy;
  PortalData *portal;
  guint signal_id;
  GError *error = NULL;

  signal_id = g_signal_lookup ("create-custom-widget", GTK_TYPE_PRINT_OPERATION);
  if (g_signal_has_handler_pending (op, signal_id, 0, TRUE))
    g_warning ("GtkPrintOperation::create-custom-widget not supported with portal");

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         PORTAL_BUS_NAME,
                                         PORTAL_OBJECT_PATH,
                                         PORTAL_PRINT_INTERFACE,
                                         NULL,
                                         &error);

  if (proxy == NULL)
    {
      if (op->priv->error == NULL)
        op->priv->error = g_error_copy (error);
      g_error_free (error);
      return NULL;
    }

  portal = g_new0 (PortalData, 1);
  portal->proxy = proxy;
  portal->op = g_object_ref (op);
  portal->parent = parent;
  portal->result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  portal->print_cb = print_cb;

  if (print_cb) /* async case */
    {
      portal->loop = NULL;
      portal->destroy = portal_data_free;
    }
  else
    {
      portal->loop = g_main_loop_new (NULL, FALSE);
      portal->destroy = NULL;
    }

  return portal;
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    user_data)
{
  PortalData *portal = user_data;

  portal->handle = g_strdup (handle_str);

  g_dbus_proxy_call (portal->proxy,
                     "PreparePrint",
                     g_variant_new ("(ss@a{sv}@a{sv}@a{sv})",
                                    handle_str,
                                    _("Print"), /* title */
                                    portal->settings,
                                    portal->setup,
                                    portal->options),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     prepare_print_called,
                     portal);
}

static void
call_prepare_print (GtkPrintOperation *op,
                    PortalData        *portal)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GVariantBuilder opt_builder;
  char *token;

  portal->prepare_print_handle =
      get_portal_request_path (g_dbus_proxy_get_connection (portal->proxy), &token);

  portal->response_signal_id =
    g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (portal->proxy)),
                                        PORTAL_BUS_NAME,
                                        PORTAL_REQUEST_INTERFACE,
                                        "Response",
                                        portal->prepare_print_handle,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        prepare_print_response,
                                        portal, NULL);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (token));
  g_free (token);
  portal->options = g_variant_builder_end (&opt_builder);

  if (priv->print_settings)
    portal->settings = gtk_print_settings_to_gvariant (priv->print_settings);
  else
    {
      GVariantBuilder builder;
      g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
      portal->settings = g_variant_builder_end (&builder);
    }

  if (priv->default_page_setup)
    portal->setup = gtk_page_setup_to_gvariant (priv->default_page_setup);
  else
    {
      GtkPageSetup *page_setup = gtk_page_setup_new ();
      portal->setup = gtk_page_setup_to_gvariant (page_setup);
      g_object_unref (page_setup);
    }

  g_variant_ref_sink (portal->options);
  g_variant_ref_sink (portal->settings);
  g_variant_ref_sink (portal->setup);

  if (portal->parent != NULL &&
      gtk_widget_is_visible (GTK_WIDGET (portal->parent)) &&
      gtk_window_export_handle (portal->parent, window_handle_exported, portal))
    return;

  g_dbus_proxy_call (portal->proxy,
                     "PreparePrint",
                     g_variant_new ("(ss@a{sv}@a{sv}@a{sv})",
                                    "",
                                    _("Print"), /* title */
                                    portal->settings,
                                    portal->setup,
                                    portal->options),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     prepare_print_called,
                     portal);
}

GtkPrintOperationResult
gtk_print_operation_portal_run_dialog (GtkPrintOperation *op,
                                       gboolean           show_dialog,
                                       GtkWindow         *parent,
                                       gboolean          *do_print)
{
  PortalData *portal;
  GtkPrintOperationResult result;

  portal = create_portal_data (op, parent, NULL);
  if (portal == NULL)
    return GTK_PRINT_OPERATION_RESULT_ERROR;

  call_prepare_print (op, portal);

  g_main_loop_run (portal->loop);

  *do_print = portal->do_print;
  result = portal->result;

  portal_data_free (portal);

  return result;
}

void
gtk_print_operation_portal_run_dialog_async (GtkPrintOperation          *op,
                                             gboolean                    show_dialog,
                                             GtkWindow                  *parent,
                                             GtkPrintOperationPrintFunc  print_cb)
{
  PortalData *portal;

  portal = create_portal_data (op, parent, print_cb);
  if (portal == NULL)
    return;

  call_prepare_print (op, portal);
}

void
gtk_print_operation_portal_launch_preview (GtkPrintOperation *op,
                                           cairo_surface_t   *surface,
                                           GtkWindow         *parent,
                                           const char        *filename)
{
  GFile *file;
  GtkFileLauncher *launcher;

  file = g_file_new_for_path (filename);
  launcher = gtk_file_launcher_new (file);
  gtk_file_launcher_launch (launcher, parent, NULL, NULL, NULL);
  g_object_unref (launcher);
  g_object_unref (file);
}
