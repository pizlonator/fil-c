/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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

#include "pp-job.h"

#include <gio/gio.h>
#include <cups/cups.h>

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetBoolean(attr, element) attr->values[element].boolean
#define ippGetCount(attr)     attr->num_values
#define ippGetInteger(attr, element) attr->values[element].integer
#define ippGetString(attr, element, language) attr->values[element].string.text
#define ippGetValueTag(attr)  attr->value_tag
static int
ippGetRange (ipp_attribute_t *attr,
             int element,
             int *upper)
{
  *upper = attr->values[element].range.upper;
  return (attr->values[element].range.lower);
}
#endif

struct _PpJob
{
  GObject parent_instance;

  gint     id;
  gchar   *title;
  gint     state;
  gint     priority;
  gboolean sensitive;

  GStrv   auth_info_required;
};

G_DEFINE_TYPE (PpJob, pp_job, G_TYPE_OBJECT)

static void
pp_job_cancel_purge_async_dbus_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          NULL);
}

PpJob *
pp_job_new (gint id, const gchar *title, gint state, gint priority, GStrv auth_info_required)
{
   PpJob *job = g_object_new (pp_job_get_type (), NULL);

   job->id = id;
   job->title = g_strdup (title);
   job->state = state;
   job->priority = priority;
   job->sensitive = FALSE;
   job->auth_info_required = g_strdupv (auth_info_required);

   return job;
}

const gchar *
pp_job_get_title (PpJob *self)
{
   g_return_val_if_fail (PP_IS_JOB(self), NULL);
   return self->title;
}

gint
pp_job_get_state (PpJob *self)
{
   g_return_val_if_fail (PP_IS_JOB(self), -1);
   return self->state;
}

void
pp_job_priority_set_sensitive (PpJob    *self,
                               gboolean  sensitive)
{
   self->sensitive = sensitive;
}

gboolean
pp_job_priority_get_sensitive (PpJob *self)
{
   g_return_val_if_fail (PP_IS_JOB (self), FALSE);
   return self->sensitive;
}

gint
pp_job_get_priority (PpJob *self)
{
   g_return_val_if_fail (PP_IS_JOB (self), -1);
   return self->priority;
}

void
pp_job_set_priority (PpJob *self,
                     gint   priority)
{
   self->priority = priority;
}

GStrv
pp_job_get_auth_info_required (PpJob *self)
{
   g_return_val_if_fail (PP_IS_JOB(self), NULL);
   return self->auth_info_required;
}

void
pp_job_cancel_purge_async (PpJob        *self,
                           gboolean      job_purge)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GError) error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get session bus: %s", error->message);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "JobCancelPurge",
                          g_variant_new ("(ib)",
                                         self->id,
                                         job_purge),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          pp_job_cancel_purge_async_dbus_cb,
                          NULL);
}

static void
pp_job_set_hold_until_async_dbus_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          NULL);
}

void
pp_job_set_hold_until_async (PpJob        *self,
                             const gchar  *job_hold_until)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GError) error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get session bus: %s", error->message);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "JobSetHoldUntil",
                          g_variant_new ("(is)",
                                         self->id,
                                         job_hold_until),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          pp_job_set_hold_until_async_dbus_cb,
                          NULL);
}

static void
pp_job_init (PpJob *obj)
{
}

static void
pp_job_finalize (GObject *object)
{
  PpJob *self = PP_JOB (object);

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->auth_info_required, g_strfreev);

  G_OBJECT_CLASS (pp_job_parent_class)->finalize (object);
}

static void
pp_job_class_init (PpJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = pp_job_finalize;
}

static void
_pp_job_get_attributes_thread (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  PpJob *self = PP_JOB (source_object);
  ipp_attribute_t  *attr = NULL;
  GVariantBuilder   builder;
  GVariant         *attributes = NULL;
  gchar           **attributes_names = task_data;
  ipp_t            *request;
  ipp_t            *response = NULL;
  g_autofree gchar *job_uri = NULL;
  gint              i, j, length = 0, n_attrs = 0;

  job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", self->id);

  if (attributes_names != NULL)
    {
      length = g_strv_length (attributes_names);

      request = ippNewRequest (IPP_GET_JOB_ATTRIBUTES);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "job-uri", NULL, job_uri);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", length, NULL, (const char **) attributes_names);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
    }

  if (response != NULL)
    {
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

      for (j = 0; j < length; j++)
        {
          attr = ippFindAttribute (response, attributes_names[j], IPP_TAG_ZERO);
          n_attrs = ippGetCount (attr);
          if (attr != NULL && n_attrs > 0 && ippGetValueTag (attr) != IPP_TAG_NOVALUE)
            {
              const GVariantType  *type = NULL;
              GVariant           **values;
              GVariant            *range[2];
              gint                 range_uppervalue;

              values = g_new (GVariant*, n_attrs);

              switch (ippGetValueTag (attr))
                {
                  case IPP_TAG_INTEGER:
                  case IPP_TAG_ENUM:
                    type = G_VARIANT_TYPE_INT32;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_int32 (ippGetInteger (attr, i));
                    break;

                  case IPP_TAG_NAME:
                  case IPP_TAG_STRING:
                  case IPP_TAG_TEXT:
                  case IPP_TAG_URI:
                  case IPP_TAG_KEYWORD:
                  case IPP_TAG_URISCHEME:
                    type = G_VARIANT_TYPE_STRING;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_string (ippGetString (attr, i, NULL));
                    break;

                  case IPP_TAG_RANGE:
                    type = G_VARIANT_TYPE_TUPLE;

                    for (i = 0; i < n_attrs; i++)
                      {
                        range[0] = g_variant_new_int32 (ippGetRange (attr, i, &(range_uppervalue)));
                        range[1] = g_variant_new_int32 (range_uppervalue);

                        values[i] = g_variant_new_tuple (range, 2);
                      }
                    break;

                  case IPP_TAG_BOOLEAN:
                    type = G_VARIANT_TYPE_BOOLEAN;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_boolean (ippGetBoolean (attr, i));
                    break;

                  default:
                    /* do nothing (switch w/ enumeration type) */
                    break;
                }

              if (type != NULL)
                {
                  g_variant_builder_add (&builder, "{sv}",
                                         attributes_names[j],
                                         g_variant_new_array (type, values, n_attrs));
                }

              g_free (values);
            }
        }

      attributes = g_variant_builder_end (&builder);
    }

  g_task_return_pointer (task, attributes, (GDestroyNotify) g_variant_unref);
}

void
pp_job_get_attributes_async (PpJob                *self,
                             gchar               **attributes_names,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdupv (attributes_names), (GDestroyNotify) g_strfreev);
  g_task_run_in_thread (task, _pp_job_get_attributes_thread);
}

GVariant *
pp_job_get_attributes_finish (PpJob         *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_pp_job_authenticate_thread (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  PpJob         *self = source_object;
  gboolean       result = FALSE;
  gchar        **auth_info = task_data;
  ipp_t         *request;
  ipp_t         *response = NULL;
  gint           length;

  if (auth_info != NULL)
    {
      g_autofree gchar *job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", self->id);

      length = g_strv_length (auth_info);

      request = ippNewRequest (IPP_OP_CUPS_AUTHENTICATE_JOB);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "job-uri", NULL, job_uri);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                     "auth-info", length, NULL, (const char **) auth_info);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");

      result = response != NULL && ippGetStatusCode (response) <= IPP_OK;

      if (response != NULL)
        ippDelete (response);
    }

  g_task_return_boolean (task, result);
}

void
pp_job_authenticate_async (PpJob                *self,
                           gchar               **auth_info,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdupv (auth_info), (GDestroyNotify) g_strfreev);
  g_task_run_in_thread (task, _pp_job_authenticate_thread);
}

gboolean
pp_job_authenticate_finish (PpJob         *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
pp_job_set_priority_thread (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  PpJob            *self = source_object;
  gint              priority = GPOINTER_TO_INT (task_data);
  ipp_t            *request;
  gboolean          result = TRUE;
  g_autofree gchar *uri = NULL;

  request = ippNewRequest (IPP_SET_JOB_ATTRIBUTES);
  uri = g_strdup_printf ("ipp://localhost/jobs/%d", self->id);
  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "job-uri", NULL, uri);
  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser ());

  ippAddInteger (request, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "job-priority", priority);

  ippDelete (cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/"));

  if (cupsLastError () > IPP_OK_CONFLICT)
    {
      g_warning ("Failed to set job priority: %s", cupsLastErrorString ());
      result = FALSE;
    }

  g_task_return_boolean (task, result);
}

void
pp_job_set_priority_async (PpJob                *self,
                           gint                  priority,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (priority), NULL);
  g_task_run_in_thread (task, pp_job_set_priority_thread);
}

gboolean
pp_job_set_priority_finish (PpJob         *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
