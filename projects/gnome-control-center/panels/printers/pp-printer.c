/*
 * Copyright (C) 2016 Red Hat, Inc
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
 * Authors: Martin Hatina <mhatina@redhat.com>
 *          Marek Kasik <mkasik@redhat.com>
 */

#include "pp-printer.h"

#include "pp-job.h"

#if (CUPS_VERSION_MAJOR == 1) && (CUPS_VERSION_MINOR <= 6)
#define IPP_STATE_IDLE IPP_IDLE
#endif

struct _PpPrinter
{
  GObject  parent_instance;
  gchar   *printer_name;
};

G_DEFINE_TYPE (PpPrinter, pp_printer, G_TYPE_OBJECT)

static void
pp_printer_dispose (GObject *object)
{
  PpPrinter *self = PP_PRINTER (object);

  g_clear_pointer (&self->printer_name, g_free);

  G_OBJECT_CLASS (pp_printer_parent_class)->dispose (object);
}

static void
pp_printer_class_init (PpPrinterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = pp_printer_dispose;
}

static void
pp_printer_init (PpPrinter *self)
{
}

PpPrinter *
pp_printer_new (const gchar *name)
{
  PpPrinter *self = g_object_new (PP_TYPE_PRINTER, NULL);

  self->printer_name = g_strdup (name);

  return self;
}

const gchar *
pp_printer_get_name (PpPrinter *self)
{
  g_return_val_if_fail (PP_IS_PRINTER (self), NULL);
  return self->printer_name;
}

static void
printer_rename_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  PpPrinter        *self = PP_PRINTER (source_object);
  gboolean          result;
  const gchar      *new_printer_name = task_data;

  result = printer_rename (self->printer_name, new_printer_name);

  if (result)
    {
      g_free (self->printer_name);
      self->printer_name = g_strdup (new_printer_name);
    }

  g_task_return_boolean (task, result);
}

static void
printer_rename_dbus_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  PpPrinter          *self;
  g_autoptr(GVariant) output = NULL;
  gboolean            result = FALSE;
  g_autoptr(GError)   error = NULL;
  g_autoptr(GTask)    task = user_data;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output != NULL)
    {
      const gchar *ret_error;

      self = g_task_get_source_object (task);

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: renaming of printer %s failed: %s", self->printer_name, ret_error);
        }
      else
        {
          result = TRUE;
          g_free (self->printer_name);
          self->printer_name = g_strdup (g_task_get_task_data (task));
        }

      g_task_return_boolean (task, result);
    }
  else
    {
      if (error->domain == G_DBUS_ERROR &&
          (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
           error->code == G_DBUS_ERROR_UNKNOWN_METHOD))
        {
          g_warning ("Update cups-pk-helper to at least 0.2.6 please to be able to use PrinterRename method.");

          g_task_run_in_thread (task, printer_rename_thread);
        }
      else
        {
          g_task_return_boolean (task, FALSE);
        }
    }
}

static void
get_bus_cb (GObject      *source_object,
            GAsyncResult *res,
            gpointer      user_data)
{
  PpPrinter *self;
  GDBusConnection  *bus;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask)  task = user_data;

  bus = g_bus_get_finish (res, &error);
  if (bus != NULL)
    {
      self = g_task_get_source_object (task);
      g_dbus_connection_call (bus,
                              MECHANISM_BUS,
                              "/",
                              MECHANISM_BUS,
                              "PrinterRename",
                              g_variant_new ("(ss)",
                                             self->printer_name,
                                             g_task_get_task_data (task)),
                              G_VARIANT_TYPE ("(s)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              g_task_get_cancellable (task),
                              printer_rename_dbus_cb,
                              task);
      g_steal_pointer (&task);
    }
  else
    {
      g_warning ("Failed to get system bus: %s", error->message);
      g_task_return_boolean (task, FALSE);
    }
}

void
pp_printer_rename_async (PpPrinter           *self,
                         const gchar         *new_printer_name,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (new_printer_name != NULL);

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (new_printer_name), g_free);

  g_bus_get (G_BUS_TYPE_SYSTEM,
             cancellable,
             get_bus_cb,
             task);
}

gboolean
pp_printer_rename_finish (PpPrinter     *self,
                          GAsyncResult  *res,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

typedef struct
{
  gboolean  myjobs;
  gint      which_jobs;
} GetJobsData;

static void
get_jobs_thread (GTask        *task,
                 gpointer      source_object,
                 gpointer      task_data,
                 GCancellable *cancellable)
{
  ipp_attribute_t  *attr = NULL;
  static gchar     *printer_attributes[] = { "auth-info-required" };
  GetJobsData      *get_jobs_data = task_data;
  cups_job_t       *jobs = NULL;
  PpPrinter        *self = PP_PRINTER (source_object);
  gboolean          auth_info_is_required;
  PpJob            *job;
  ipp_t            *job_request;
  ipp_t            *job_response;
  ipp_t            *printer_request;
  ipp_t            *printer_response;
  gchar           **auth_info_required = NULL;
  g_autofree gchar *printer_name = NULL;
  g_autoptr(GPtrArray) array = NULL;
  gint              num_jobs;
  gint              i, j;

  num_jobs = cupsGetJobs (&jobs,
                          self->printer_name,
                          get_jobs_data->myjobs ? 1 : 0,
                          get_jobs_data->which_jobs);

  array = g_ptr_array_new_with_free_func (g_object_unref);
  for (i = 0; i < num_jobs; i++)
    {
      auth_info_is_required = FALSE;
      if (jobs[i].state == IPP_JOB_HELD)
        {
          g_autofree gchar *job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", jobs[i].id);

          job_request = ippNewRequest (IPP_GET_JOB_ATTRIBUTES);
          ippAddString (job_request, IPP_TAG_OPERATION, IPP_TAG_URI,
                        "job-uri", NULL, job_uri);
          ippAddString (job_request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                        "requesting-user-name", NULL, cupsUser ());
          ippAddString (job_request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                        "requested-attributes", NULL, "job-hold-until");
          job_response = cupsDoRequest (CUPS_HTTP_DEFAULT, job_request, "/");

          if (job_response != NULL)
            {
              attr = ippFindAttribute (job_response, "job-hold-until", IPP_TAG_ZERO);
              if (attr != NULL && g_strcmp0 (ippGetString (attr, 0, NULL), "auth-info-required") == 0)
                {
                  auth_info_is_required = TRUE;

                  if (auth_info_required == NULL)
                    {
                      g_autofree gchar *printer_uri = g_strdup_printf ("ipp://localhost/printers/%s", self->printer_name);

                      printer_request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
                      ippAddString (printer_request, IPP_TAG_OPERATION, IPP_TAG_URI,
                                    "printer-uri", NULL, printer_uri);
                      ippAddString (printer_request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                                    "requesting-user-name", NULL, cupsUser ());
                      ippAddStrings (printer_request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                                     "requested-attributes", 1, NULL, (const char **) printer_attributes);
                      printer_response = cupsDoRequest (CUPS_HTTP_DEFAULT, printer_request, "/");

                      if (printer_response != NULL)
                        {
                          attr = ippFindAttribute (printer_response, "auth-info-required", IPP_TAG_ZERO);
                          if (attr != NULL)
                            {
                              auth_info_required = g_new0 (gchar *, ippGetCount (attr) + 1);
                              for (j = 0; j < ippGetCount (attr); j++)
                                auth_info_required[j] = g_strdup (ippGetString (attr, j, NULL));
                            }

                          ippDelete (printer_response);
                        }
                    }
                }

              ippDelete (job_response);
            }
        }

      job = pp_job_new (jobs[i].id, jobs[i].title, jobs[i].state, jobs[i].priority, auth_info_is_required ? auth_info_required : NULL);
      g_ptr_array_add (array, job);
    }

  g_strfreev (auth_info_required);
  cupsFreeJobs (num_jobs, jobs);

  if (g_task_set_return_on_cancel (task, FALSE))
    {
      g_task_return_pointer (task, g_steal_pointer (&array), (GDestroyNotify) g_ptr_array_unref);
    }
}

void
pp_printer_get_jobs_async (PpPrinter           *self,
                           gboolean             myjobs,
                           gint                 which_jobs,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GetJobsData *get_jobs_data;
  g_autoptr(GTask) task = NULL;

  get_jobs_data = g_new (GetJobsData, 1);
  get_jobs_data->myjobs = myjobs;
  get_jobs_data->which_jobs = which_jobs;

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
  g_task_set_task_data (task, get_jobs_data, g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, get_jobs_thread);
}

GPtrArray *
pp_printer_get_jobs_finish (PpPrinter          *self,
                            GAsyncResult       *res,
                            GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
pp_printer_delete_dbus_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  PpPrinter *self;
  g_autoptr(GVariant) output = NULL;
  gboolean            result = FALSE;
  g_autoptr(GError)   error = NULL;
  GTask              *task = user_data;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output != NULL)
    {
      const gchar      *ret_error;

      self = g_task_get_source_object (task);

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("cups-pk-helper: removing of printer %s failed: %s", self->printer_name, ret_error);
      else
        result = TRUE;

      g_task_return_boolean (task, result);
    }
  else
    {
      g_warning ("%s", error->message);

      g_task_return_boolean (task, FALSE);
    }
}

static void
pp_printer_delete_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpPrinter *self;
  GDBusConnection  *bus;
  g_autoptr(GError) error = NULL;
  GTask            *task = user_data;

  bus = g_bus_get_finish (res, &error);
  if (bus != NULL)
    {
      self = g_task_get_source_object (task);

      g_dbus_connection_call (bus,
                              MECHANISM_BUS,
                              "/",
                              MECHANISM_BUS,
                              "PrinterDelete",
                              g_variant_new ("(s)", self->printer_name),
                              G_VARIANT_TYPE ("(s)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              g_task_get_cancellable (task),
                              pp_printer_delete_dbus_cb,
                              task);
    }
  else
    {
      g_warning ("Failed to get system bus: %s", error->message);
      g_task_return_boolean (task, FALSE);
    }
}

void
pp_printer_delete_async (PpPrinter           *self,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask *task;

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);

  g_bus_get (G_BUS_TYPE_SYSTEM,
             cancellable,
             pp_printer_delete_cb,
             task);
}

gboolean
pp_printer_delete_finish (PpPrinter     *self,
                          GAsyncResult  *res,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

gboolean
pp_printer_delete_sync (PpPrinter     *self,
                        GCancellable  *cancellable,
                        GError       **error)
{
  g_autoptr(GDBusConnection)  bus = NULL;
  g_autoptr(GVariant)         output = NULL;
  g_autoptr(GError)           tmp_error = NULL;
  const gchar                *ret_error;
  gboolean                    result = FALSE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, cancellable, &tmp_error);
  if (bus == NULL)
    {
      g_warning ("Failed to get system bus: %s", tmp_error->message);
      *error = g_error_copy (tmp_error);
      return result;
    }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterDelete",
                                        g_variant_new ("(s)", self->printer_name),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        cancellable,
                                        &tmp_error);

  if (output == NULL)
    {
      g_warning ("%s", tmp_error->message);
      *error = g_error_copy (tmp_error);
      return result;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    g_warning ("cups-pk-helper: removing of printer %s failed: %s", self->printer_name, ret_error);
  else
    result = TRUE;

  return result;
}

typedef struct
{
  gchar *filename;
  gchar *job_name;
} PrintFileData;

static void
print_file_data_free (PrintFileData *print_file_data)
{
  g_free (print_file_data->filename);
  g_free (print_file_data->job_name);

  g_slice_free (PrintFileData, print_file_data);
}

static void
print_file_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  PpPrinter        *self = PP_PRINTER (source_object);
  PrintFileData    *print_file_data;
  cups_dest_t      *dest = NULL;
  gboolean          ret = FALSE;
  gint              job_id = 0;

  dest = cupsGetNamedDest (CUPS_HTTP_DEFAULT, self->printer_name, NULL);

  if (dest == NULL)
    {
      g_warning ("Failed to get the destination %s - %s.", self->printer_name, cupsLastErrorString ());
      g_task_return_boolean (task, ret);
    }

  print_file_data = g_task_get_task_data (task);

  job_id = cupsPrintFile2 (CUPS_HTTP_DEFAULT, dest->name, print_file_data->filename, print_file_data->job_name, dest->num_options, dest->options);

  if (job_id < 1)
    g_warning ("An error has occurred during printing of test page - %s", cupsLastErrorString ());
  else
    ret = TRUE;

  cupsFreeDests (1, dest);

  if (g_task_set_return_on_cancel (task, FALSE))
    {
      g_task_return_boolean (task, ret);
    }
}

void
pp_printer_print_file_async (PpPrinter           *self,
                             const gchar         *filename,
                             const gchar         *job_name,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  PrintFileData *print_file_data;
  g_autoptr(GTask) task = NULL;

  print_file_data = g_new (PrintFileData, 1);
  print_file_data->filename = g_strdup (filename);
  print_file_data->job_name = g_strdup (job_name);

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);

  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_task_data (task, print_file_data, (GDestroyNotify) print_file_data_free);

  g_task_run_in_thread (task, print_file_thread);
}

gboolean
pp_printer_print_file_finish (PpPrinter     *self,
                              GAsyncResult  *res,
                              GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}
