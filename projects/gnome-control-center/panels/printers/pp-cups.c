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

#include "pp-cups.h"

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetInteger(attr, element) attr->values[element].integer
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#endif

struct _PpCups
{
  GObject parent_instance;
};

G_DEFINE_TYPE (PpCups, pp_cups, G_TYPE_OBJECT);

static void
pp_cups_class_init (PpCupsClass *klass)
{
}

static void
pp_cups_init (PpCups *self)
{
}

PpCups *
pp_cups_new ()
{
  return g_object_new (PP_TYPE_CUPS, NULL);
}

static void
pp_cups_dests_free (PpCupsDests *dests)
{
  cupsFreeDests (dests->num_of_dests, dests->dests);
}

static void
_pp_cups_get_dests_thread (GTask        *task,
                           gpointer     *object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  PpCupsDests *dests;

  dests = g_new0 (PpCupsDests, 1);
  dests->num_of_dests = cupsGetDests (&dests->dests);

  if (g_task_set_return_on_cancel (task, FALSE))
    {
      g_task_return_pointer (task, dests, (GDestroyNotify) pp_cups_dests_free);
    }
  else
    {
      pp_cups_dests_free (dests);
    }
}

void
pp_cups_get_dests_async (PpCups              *self,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, (GTaskThreadFunc) _pp_cups_get_dests_thread);
}

PpCupsDests *
pp_cups_get_dests_finish (PpCups        *self,
                          GAsyncResult  *res,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
connection_test_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  http_t *http;

#ifdef HAVE_CUPS_HTTPCONNECT2
  http = httpConnect2 (cupsServer (), ippPort (), NULL, AF_UNSPEC,
                       cupsEncryption (), 1, 30000, NULL);
#else
  http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
#endif
  httpClose (http);

  if (g_task_set_return_on_cancel (task, FALSE))
    {
      g_task_return_boolean (task, http != NULL);
    }
}

void
pp_cups_connection_test_async (PpCups              *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, connection_test_thread);
}

gboolean
pp_cups_connection_test_finish (PpCups         *self,
                                GAsyncResult   *result,
                                GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/* Cancels subscription of given id */
static void
cancel_subscription_thread (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  ipp_t *request;
  ipp_t *response = NULL;
  gint   id = GPOINTER_TO_INT (task_data);

  if (id >= 0)
    {
      request = ippNewRequest (IPP_CANCEL_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddInteger (request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                     "notify-subscription-id", id);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
    }

  g_task_return_boolean (task, response != NULL && ippGetStatusCode (response) <= IPP_OK);

  ippDelete (response);
}

void
pp_cups_cancel_subscription_async (PpCups              *self,
                                   gint                 subscription_id,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, NULL, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (subscription_id), NULL);
  g_task_run_in_thread (task, cancel_subscription_thread);
}

gboolean
pp_cups_cancel_subscription_finish (PpCups       *self,
                                    GAsyncResult *result)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), NULL);
}

typedef struct {
  gint id;
  gchar **events;
  int lease_duration;
} CRSData;

static void
crs_data_free (CRSData *data)
{
  g_strfreev (data->events);
  g_slice_free (CRSData, data);
}

static void
renew_subscription_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  ipp_attribute_t *attr = NULL;
  CRSData         *subscription_data = task_data;
  ipp_t           *request;
  ipp_t           *response = NULL;
  gint             result = -1;

  if (g_cancellable_is_cancelled (cancellable))
    return;

  if (subscription_data->id > 0)
    {
      request = ippNewRequest (IPP_RENEW_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                   "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser ());
      ippAddInteger (request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                    "notify-subscription-id", subscription_data->id);
      ippAddInteger (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                    "notify-lease-duration", subscription_data->lease_duration);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
      if (response != NULL && ippGetStatusCode (response) <= IPP_OK_CONFLICT)
        {
          if ((attr = ippFindAttribute (response, "notify-lease-duration", IPP_TAG_INTEGER)) == NULL)
            g_debug ("No notify-lease-duration in response!\n");
          else if (ippGetInteger (attr, 0) == subscription_data->lease_duration)
            result = subscription_data->id;
        }
    }

  if (result < 0)
    {
      request = ippNewRequest (IPP_CREATE_PRINTER_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                   "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
                    "notify-events", g_strv_length (subscription_data->events), NULL,
                     (const char * const *) subscription_data->events);
      ippAddString (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
                   "notify-pull-method", NULL, "ippget");
      ippAddString (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
                   "notify-recipient-uri", NULL, "dbus://");
      ippAddInteger (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                    "notify-lease-duration", subscription_data->lease_duration);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");

      if (response != NULL && ippGetStatusCode (response) <= IPP_OK_CONFLICT)
        {
          if ((attr = ippFindAttribute (response, "notify-subscription-id", IPP_TAG_INTEGER)) == NULL)
            g_debug ("No notify-subscription-id in response!\n");
          else
            result = ippGetInteger (attr, 0);
        }
    }

  ippDelete (response);

  g_task_return_int (task, result);
}

void
pp_cups_renew_subscription_async  (PpCups               *self,
                                   gint                  subscription_id,
                                   gchar               **events,
                                   gint                  lease_duration,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  CRSData *subscription_data;
  g_autoptr(GTask) task = NULL;

  subscription_data = g_slice_new (CRSData);
  subscription_data->id = subscription_id;
  subscription_data->events = g_strdupv (events);
  subscription_data->lease_duration = lease_duration;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, subscription_data, (GDestroyNotify) crs_data_free);
  g_task_run_in_thread (task, renew_subscription_thread);
}

/* Returns id of renewed subscription or new id */
gint
pp_cups_renew_subscription_finish (PpCups       *self,
                                   GAsyncResult *result)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_int (G_TASK (result), NULL);
}
