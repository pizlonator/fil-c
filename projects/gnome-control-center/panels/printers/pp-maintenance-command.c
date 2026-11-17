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

#include <glib/gstdio.h>

#include "pp-maintenance-command.h"

#include "pp-utils.h"

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetCount(attr)     attr->num_values
#define ippGetValueTag(attr)  attr->value_tag
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#define ippGetString(attr, element, language) attr->values[element].string.text
#endif

struct _PpMaintenanceCommand
{
  GObject parent_instance;

  gchar *printer_name;
  gchar *command;
  gchar *parameters;
  gchar *title;
};

G_DEFINE_TYPE (PpMaintenanceCommand, pp_maintenance_command, G_TYPE_OBJECT);

enum {
  PROP_0 = 0,
  PROP_PRINTER_NAME,
  PROP_COMMAND,
  PROP_PARAMETERS,
  PROP_TITLE
};

static void
pp_maintenance_command_finalize (GObject *object)
{
  PpMaintenanceCommand *self = PP_MAINTENANCE_COMMAND (object);

  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->parameters, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (pp_maintenance_command_parent_class)->finalize (object);
}

static void
pp_maintenance_command_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *param_spec)
{
  PpMaintenanceCommand *self = PP_MAINTENANCE_COMMAND (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        g_value_set_string (value, self->printer_name);
        break;
      case PROP_COMMAND:
        g_value_set_string (value, self->command);
        break;
      case PROP_PARAMETERS:
        g_value_set_string (value, self->parameters);
        break;
      case PROP_TITLE:
        g_value_set_string (value, self->title);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_maintenance_command_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *param_spec)
{
  PpMaintenanceCommand *self = PP_MAINTENANCE_COMMAND (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        g_free (self->printer_name);
        self->printer_name = g_value_dup_string (value);
        break;
      case PROP_COMMAND:
        g_free (self->command);
        self->command = g_value_dup_string (value);
        break;
      case PROP_PARAMETERS:
        g_free (self->parameters);
        self->parameters = g_value_dup_string (value);
        break;
      case PROP_TITLE:
        g_free (self->title);
        self->title = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_maintenance_command_class_init (PpMaintenanceCommandClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = pp_maintenance_command_set_property;
  gobject_class->get_property = pp_maintenance_command_get_property;
  gobject_class->finalize = pp_maintenance_command_finalize;

  g_object_class_install_property (gobject_class, PROP_PRINTER_NAME,
    g_param_spec_string ("printer-name",
                         "Printer name",
                         "Name of the printer",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_COMMAND,
    g_param_spec_string ("command",
                         "Maintenance command",
                         "Command to execute",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PARAMETERS,
    g_param_spec_string ("parameters",
                         "Optional parameters",
                         "Optional parameters for the maintenance command",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TITLE,
    g_param_spec_string ("title",
                         "Command title",
                         "Title of the job by which the command will be executed",
                         NULL,
                         G_PARAM_READWRITE));
}

static void
pp_maintenance_command_init (PpMaintenanceCommand *self)
{
}

PpMaintenanceCommand *
pp_maintenance_command_new (const gchar *printer_name,
                            const gchar *command,
                            const gchar *parameters,
                            const gchar *title)
{
  return g_object_new (PP_TYPE_MAINTENANCE_COMMAND,
                       "printer-name", printer_name,
                       "command", command,
                       "parameters", parameters,
                       "title", title,
                       NULL);
}

static gboolean _pp_maintenance_command_is_supported (const gchar *printer_name,
                                                      const gchar *command);

static void
_pp_maintenance_command_execute_thread (GTask        *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  PpMaintenanceCommand        *self = PP_MAINTENANCE_COMMAND (source_object);
  gboolean                     success = FALSE;
  GError                      *error = NULL;

  if (_pp_maintenance_command_is_supported (self->printer_name, self->command))
    {
      ipp_t            *request;
      ipp_t            *response = NULL;
      g_autofree gchar *printer_uri = NULL;
      g_autofree gchar *file_name = NULL;
      int               fd = -1;

      printer_uri = g_strdup_printf ("ipp://localhost/printers/%s",
                                     self->printer_name);

      request = ippNewRequest (IPP_PRINT_JOB);

      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, printer_uri);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "job-name", NULL, self->title);
      ippAddString (request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
                    "document-format", NULL, "application/vnd.cups-command");

      fd = g_file_open_tmp ("ccXXXXXX", &file_name, &error);

      if (fd != -1)
        {
          FILE *file;

          file = fdopen (fd, "w");
          fprintf (file, "#CUPS-COMMAND\n");
          fprintf (file, "%s", self->command);
          if (self->parameters)
            fprintf (file, " %s", self->parameters);
          fprintf (file, "\n");
          fclose (file);

          response = cupsDoFileRequest (CUPS_HTTP_DEFAULT, request, "/", file_name);
          g_unlink (file_name);

          if (response != NULL)
            {
              if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
                {
                  success = TRUE;
                }

              ippDelete (response);
            }
        }
    }
  else
    {
      success = TRUE;
    }

  if (!success)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Execution of maintenance command failed.");
    }

  g_task_return_boolean (task, success);
}

void
pp_maintenance_command_execute_async (PpMaintenanceCommand *self,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, TRUE);
  g_task_run_in_thread (task, _pp_maintenance_command_execute_thread);
}

gboolean
pp_maintenance_command_execute_finish (PpMaintenanceCommand  *self,
                                       GAsyncResult          *result,
                                       GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
_pp_maintenance_command_is_supported (const gchar *printer_name,
                                      const gchar *command)
{
  ipp_attribute_t   *attr = NULL;
  gboolean           is_supported = FALSE;
  ipp_t             *request;
  ipp_t             *response = NULL;
  g_autofree gchar  *printer_uri = NULL;
  GPtrArray         *available_commands = NULL;
  int                i;

  printer_uri = g_strdup_printf ("ipp://localhost/printers/%s",
                                 printer_name);

  request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                "printer-uri", NULL, printer_uri);
  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", NULL, "printer-commands");
  response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
  if (response != NULL)
    {
      if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
        {
          int commands_count;

          attr = ippFindAttribute (response, "printer-commands", IPP_TAG_ZERO);
          commands_count = attr != NULL ? ippGetCount (attr) : 0;
          if (commands_count > 0 &&
              ippGetValueTag (attr) != IPP_TAG_NOVALUE &&
              (ippGetValueTag (attr) == IPP_TAG_KEYWORD))
            {
              available_commands = g_ptr_array_new_full (commands_count, g_free);
              for (i = 0; i < commands_count; ++i)
                {
                  /* Array gains ownership of the lower-cased string */
                  g_ptr_array_add (available_commands, g_ascii_strdown (ippGetString (attr, i, NULL), -1));
                }
            }
        }

      ippDelete (response);
    }

  if (available_commands != NULL)
    {
      g_autofree gchar *command_lowercase = g_ascii_strdown (command, -1);
      for (i = 0; i < available_commands->len; ++i)
        {
          const gchar *available_command = g_ptr_array_index (available_commands, i);
          if (g_strcmp0 (available_command, command_lowercase) == 0)
            {
              is_supported = TRUE;
              break;
            }
        }

      g_ptr_array_free (available_commands, TRUE);
    }

  return is_supported;
}

static void
_pp_maintenance_command_is_supported_thread (GTask        *task,
                                             gpointer      source_object,
                                             gpointer      task_data,
                                             GCancellable *cancellable)
{
  PpMaintenanceCommand        *self = PP_MAINTENANCE_COMMAND (source_object);
  gboolean                     success = FALSE;

  success = _pp_maintenance_command_is_supported (self->printer_name, self->command);
  g_task_return_boolean (task, success);
}

void
pp_maintenance_command_is_supported_async  (PpMaintenanceCommand *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, TRUE);
  g_task_run_in_thread (task, _pp_maintenance_command_is_supported_thread);
}

gboolean
pp_maintenance_command_is_supported_finish (PpMaintenanceCommand  *self,
                                            GAsyncResult          *result,
                                            GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
