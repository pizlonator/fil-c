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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-utils.h"

#define DBUS_TIMEOUT      120000
#define DBUS_TIMEOUT_LONG 600000

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetCount(attr)     attr->num_values
#define ippGetGroupTag(attr)  attr->group_tag
#define ippGetValueTag(attr)  attr->value_tag
#define ippGetName(attr)      attr->name
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#define ippGetInteger(attr, element) attr->values[element].integer
#define ippGetString(attr, element, language) attr->values[element].string.text
#define ippGetBoolean(attr, element) attr->values[element].boolean

static int
ippGetRange (ipp_attribute_t *attr,
             int element,
             int *upper)
{
  *upper = attr->values[element].range.upper;
  return (attr->values[element].range.lower);
}

static ipp_attribute_t *
ippFirstAttribute (ipp_t *ipp)
{
  if (!ipp)
    return (NULL);
  return (ipp->current = ipp->attrs);
}

static ipp_attribute_t *
ippNextAttribute (ipp_t *ipp)
{
  if (!ipp || !ipp->current)
    return (NULL);
  return (ipp->current = ipp->current->next);
}
#endif

#if (CUPS_VERSION_MAJOR == 1) && (CUPS_VERSION_MINOR <= 6)
#define HTTP_URI_STATUS_OK HTTP_URI_OK
#endif

gchar *
get_tag_value (const gchar *tag_string, const gchar *tag_name)
{
  gchar **tag_string_splitted = NULL;
  gchar  *tag_value = NULL;
  gint    tag_name_length;
  gint    i;

  if (tag_string && tag_name)
    {
      tag_name_length = strlen (tag_name);
      tag_string_splitted = g_strsplit (tag_string, ";", 0);
      if (tag_string_splitted)
        {
          for (i = 0; i < g_strv_length (tag_string_splitted); i++)
            if (g_ascii_strncasecmp (tag_string_splitted[i], tag_name, tag_name_length) == 0)
              if (strlen (tag_string_splitted[i]) > tag_name_length + 1)
                tag_value = g_strdup (tag_string_splitted[i] + tag_name_length + 1);

          g_strfreev (tag_string_splitted);
        }
    }

  return tag_value;
}


/*
 * Normalize given string so that it is lowercase, doesn't
 * have trailing or leading whitespaces and digits doesn't
 * neighbour with alphabetic.
 * (see cupshelpers/ppds.py from system-config-printer)
 */
static gchar *
normalize (const gchar *input_string)
{
  gchar *result = NULL;
  gint   i, j = 0, k = -1;

  if (input_string)
    {
      g_autofree gchar *tmp = g_strstrip (g_ascii_strdown (input_string, -1));
      if (tmp)
        {
          g_autofree gchar *res = g_new (gchar, 2 * strlen (tmp) + 1);

          for (i = 0; i < strlen (tmp); i++)
            {
              if ((g_ascii_isalpha (tmp[i]) && k >= 0 && g_ascii_isdigit (res[k])) ||
                  (g_ascii_isdigit (tmp[i]) && k >= 0 && g_ascii_isalpha (res[k])))
                {
                  res[j] = ' ';
                  k = j++;
                  res[j] = tmp[i];
                  k = j++;
                }
              else
                {
                  if (g_ascii_isspace (tmp[i]) || !g_ascii_isalnum (tmp[i]))
                    {
                      if (!(k >= 0 && res[k] == ' '))
                        {
                          res[j] = ' ';
                          k = j++;
                        }
                    }
                  else
                    {
                      res[j] = tmp[i];
                      k = j++;
                    }
                }
            }

          res[j] = '\0';

          result = g_strdup (res);
        }
    }

  return result;
}


char *
get_dest_attr (const char *dest_name,
               const char *attr)
{
  cups_dest_t *dests;
  int          num_dests;
  cups_dest_t *dest;
  const char  *value;
  char        *ret;

  if (dest_name == NULL)
          return NULL;

  ret = NULL;

  num_dests = cupsGetDests (&dests);
  if (num_dests < 1) {
          g_debug ("Unable to get printer destinations");
          return NULL;
  }

  dest = cupsGetDest (dest_name, NULL, num_dests, dests);
  if (dest == NULL) {
          g_debug ("Unable to find a printer named '%s'", dest_name);
          goto out;
  }

  value = cupsGetOption (attr, dest->num_options, dest->options);
  if (value == NULL) {
          g_debug ("Unable to get %s for '%s'", attr, dest_name);
          goto out;
  }
  ret = g_strdup (value);
out:
  cupsFreeDests (num_dests, dests);

  return ret;
}

gchar *
get_ppd_attribute (const gchar *ppd_file_name,
                   const gchar *attribute_name)
{
  ppd_file_t *ppd_file = NULL;
  ppd_attr_t *ppd_attr = NULL;
  gchar *result = NULL;

  if (ppd_file_name)
    {
      ppd_file = ppdOpenFile (ppd_file_name);

      if (ppd_file)
        {
          ppd_attr = ppdFindAttr (ppd_file, attribute_name, NULL);
          if (ppd_attr != NULL)
            result = g_strdup (ppd_attr->value);
          ppdClose (ppd_file);
        }
    }

  return result;
}

/*  Set default destination in ~/.cups/lpoptions.
 *  Unset default destination if "dest" is NULL.
 */
void
set_local_default_printer (const gchar *printer_name)
{
  cups_dest_t *dests = NULL;
  int          num_dests = 0;
  int          i;

  num_dests = cupsGetDests (&dests);

  for (i = 0; i < num_dests; i ++)
    {
      if (printer_name && g_strcmp0 (dests[i].name, printer_name) == 0)
        dests[i].is_default = 1;
      else
        dests[i].is_default = 0;
    }

  cupsSetDests (num_dests, dests);
}

/*
 * This function does something which should be provided by CUPS...
 * It returns FALSE if the renaming fails.
 */
gboolean
printer_rename (const gchar *old_name,
                const gchar *new_name)
{
  ipp_attribute_t  *attr = NULL;
  cups_ptype_t      printer_type = 0;
  cups_dest_t      *dests = NULL;
  cups_dest_t      *dest = NULL;
  cups_job_t       *jobs = NULL;
  g_autoptr(GDBusConnection) bus = NULL;
  const gchar      *printer_location = NULL;
  const gchar      *printer_info = NULL;
  const gchar      *printer_uri = NULL;
  const gchar      *device_uri = NULL;
  const gchar      *job_sheets = NULL;
  gboolean          result = FALSE;
  gboolean          accepting = TRUE;
  gboolean          printer_paused = FALSE;
  gboolean          default_printer = FALSE;
  gboolean          printer_shared = FALSE;
  g_autoptr(GError) error = NULL;
  http_t           *http;
  g_autofree gchar *ppd_link = NULL;
  g_autofree gchar *ppd_filename = NULL;
  gchar           **sheets = NULL;
  gchar           **users_allowed = NULL;
  gchar           **users_denied = NULL;
  gchar           **member_names = NULL;
  const gchar      *start_sheet = NULL;
  const gchar      *end_sheet = NULL;
  g_autofree gchar *error_policy = NULL;
  g_autofree gchar *op_policy = NULL;
  ipp_t            *request;
  ipp_t            *response;
  gint              i;
  int               num_dests = 0;
  int               num_jobs = 0;
  static const char * const requested_attrs[] = {
    "printer-error-policy",
    "printer-op-policy",
    "requesting-user-name-allowed",
    "requesting-user-name-denied",
    "member-names"};

  if (old_name == NULL ||
      old_name[0] == '\0' ||
      new_name == NULL ||
      new_name[0] == '\0' ||
      g_strcmp0 (old_name, new_name) == 0)
    return FALSE;

  num_dests = cupsGetDests (&dests);

  dest = cupsGetDest (new_name, NULL, num_dests, dests);
  if (dest)
    {
      cupsFreeDests (num_dests, dests);
      return FALSE;
    }

  num_jobs = cupsGetJobs (&jobs, old_name, 0, CUPS_WHICHJOBS_ACTIVE);
  cupsFreeJobs (num_jobs, jobs);
  if (num_jobs > 1)
    {
      g_warning ("There are queued jobs on printer %s!", old_name);
      cupsFreeDests (num_dests, dests);
      return FALSE;
    }

  /*
   * Gather some informations about the original printer
   */
  dest = cupsGetDest (old_name, NULL, num_dests, dests);
  if (dest)
    {
      for (i = 0; i < dest->num_options; i++)
        {
          if (g_strcmp0 (dest->options[i].name, "printer-is-accepting-jobs") == 0)
            accepting = g_strcmp0 (dest->options[i].value, "true") == 0;
          else if (g_strcmp0 (dest->options[i].name, "printer-is-shared") == 0)
            printer_shared = g_strcmp0 (dest->options[i].value, "true") == 0;
          else if (g_strcmp0 (dest->options[i].name, "device-uri") == 0)
            device_uri = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-uri-supported") == 0)
            printer_uri = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-info") == 0)
            printer_info = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-location") == 0)
            printer_location = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-state") == 0)
            printer_paused = g_strcmp0 (dest->options[i].value, "5") == 0;
          else if (g_strcmp0 (dest->options[i].name, "job-sheets") == 0)
            job_sheets = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-type") == 0)
            printer_type = atoi (dest->options[i].value);
        }
      default_printer = dest->is_default;
    }
  cupsFreeDests (num_dests, dests);

  if (accepting)
    {
      printer_set_accepting_jobs (old_name, FALSE, NULL);

      num_jobs = cupsGetJobs (&jobs, old_name, 0, CUPS_WHICHJOBS_ACTIVE);
      cupsFreeJobs (num_jobs, jobs);
      if (num_jobs > 1)
        {
          printer_set_accepting_jobs (old_name, accepting, NULL);
          g_warning ("There are queued jobs on printer %s!", old_name);
          return FALSE;
        }
    }


  /*
   * Gather additional informations about the original printer
   */
#ifdef HAVE_CUPS_HTTPCONNECT2
  http = httpConnect2 (cupsServer (), ippPort (), NULL, AF_UNSPEC,
                       cupsEncryption (), 1, 30000, NULL);
#else
  http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
#endif
  if (http != NULL)
    {
      request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, printer_uri);
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", G_N_ELEMENTS (requested_attrs), NULL, requested_attrs);
      response = cupsDoRequest (http, request, "/");

      if (response)
        {
          if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
            {
              attr = ippFindAttribute (response, "printer-error-policy", IPP_TAG_NAME);
              if (attr)
                error_policy = g_strdup (ippGetString (attr, 0, NULL));

              attr = ippFindAttribute (response, "printer-op-policy", IPP_TAG_NAME);
              if (attr)
                op_policy = g_strdup (ippGetString (attr, 0, NULL));

              attr = ippFindAttribute (response, "requesting-user-name-allowed", IPP_TAG_NAME);
              if (attr && ippGetCount (attr) > 0)
                {
                  users_allowed = g_new0 (gchar *, ippGetCount (attr) + 1);
                  for (i = 0; i < ippGetCount (attr); i++)
                    users_allowed[i] = g_strdup (ippGetString (attr, i, NULL));
                }

              attr = ippFindAttribute (response, "requesting-user-name-denied", IPP_TAG_NAME);
              if (attr && ippGetCount (attr) > 0)
                {
                  users_denied = g_new0 (gchar *, ippGetCount (attr) + 1);
                  for (i = 0; i < ippGetCount (attr); i++)
                    users_denied[i] = g_strdup (ippGetString (attr, i, NULL));
                }

              attr = ippFindAttribute (response, "member-names", IPP_TAG_NAME);
              if (attr && ippGetCount (attr) > 0)
                {
                  member_names = g_new0 (gchar *, ippGetCount (attr) + 1);
                  for (i = 0; i < ippGetCount (attr); i++)
                    member_names[i] = g_strdup (ippGetString (attr, i, NULL));
                }
            }
          ippDelete (response);
        }
      httpClose (http);
    }

  if (job_sheets)
    {
      sheets = g_strsplit (job_sheets, ",", 0);
      if (g_strv_length (sheets) > 1)
        {
          start_sheet = sheets[0];
          end_sheet = sheets[1];
        }
    }

  ppd_link = g_strdup (cupsGetPPD (old_name));
  if (ppd_link)
    {
      ppd_filename = g_file_read_link (ppd_link, NULL);

      if (!ppd_filename)
        ppd_filename = g_strdup (ppd_link);
    }


  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
   }
  else
    {
      if (printer_type & CUPS_PRINTER_CLASS)
        {
          if (member_names)
            for (i = 0; i < g_strv_length (member_names); i++)
              class_add_printer (new_name, member_names[i]);
        }
      else
        {
          g_autoptr(GVariant) output = NULL;
          g_autoptr(GError) add_error = NULL;

          output = g_dbus_connection_call_sync (bus,
                                                MECHANISM_BUS,
                                                "/",
                                                MECHANISM_BUS,
                                                "PrinterAddWithPpdFile",
                                                g_variant_new ("(sssss)",
                                                               new_name,
                                                               device_uri ? device_uri : "",
                                                               ppd_filename ? ppd_filename : "",
                                                               printer_info ? printer_info : "",
                                                               printer_location ? printer_location : ""),
                                                G_VARIANT_TYPE ("(s)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                &add_error);

          if (output)
            {
              const gchar *ret_error;

              g_variant_get (output, "(&s)", &ret_error);
              if (ret_error[0] != '\0')
                g_warning ("cups-pk-helper: rename of printer %s to %s failed: %s", old_name, new_name, ret_error);
            }
          else
            {
              g_warning ("%s", add_error->message);
            }
        }
    }

  if (ppd_link)
    {
      g_unlink (ppd_link);
    }

  num_dests = cupsGetDests (&dests);
  dest = cupsGetDest (new_name, NULL, num_dests, dests);
  if (dest)
    {
      printer_set_accepting_jobs (new_name, accepting, NULL);
      printer_set_enabled (new_name, !printer_paused);
      printer_set_shared (new_name, printer_shared);
      printer_set_job_sheets (new_name, start_sheet, end_sheet);
      printer_set_policy (new_name, op_policy, FALSE);
      printer_set_policy (new_name, error_policy, TRUE);
      printer_set_users (new_name, users_allowed, TRUE);
      printer_set_users (new_name, users_denied, FALSE);
      if (default_printer)
        printer_set_default (new_name);

      printer_delete (old_name);

      result = TRUE;
    }
  else
    printer_set_accepting_jobs (old_name, accepting, NULL);

  cupsFreeDests (num_dests, dests);
  if (sheets)
    g_strfreev (sheets);
  if (users_allowed)
    g_strfreev (users_allowed);
  if (users_denied)
    g_strfreev (users_denied);

  return result;
}

gboolean
printer_set_location (const gchar *printer_name,
                      const gchar *location)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name || !location)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetLocation",
                                        g_variant_new ("(ss)", printer_name, location),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of location for printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_accepting_jobs (const gchar *printer_name,
                            gboolean     accepting_jobs,
                            const gchar *reason)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetAcceptJobs",
                                        g_variant_new ("(sbs)",
                                                       printer_name,
                                                       accepting_jobs,
                                                       reason ? reason : ""),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of acceptance of jobs for printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_enabled (const gchar *printer_name,
                     gboolean     enabled)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetEnabled",
                                        g_variant_new ("(sb)", printer_name, enabled),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of enablement of printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_delete (const gchar *printer_name)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterDelete",
                                        g_variant_new ("(s)", printer_name),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: removing of printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_default (const gchar *printer_name)
{
  const char *cups_server;
  g_autoptr(GError) error = NULL;

  if (!printer_name)
    return TRUE;

  cups_server = cupsServer ();
  if (g_ascii_strncasecmp (cups_server, "localhost", 9) == 0 ||
      g_ascii_strncasecmp (cups_server, "127.0.0.1", 9) == 0 ||
      g_ascii_strncasecmp (cups_server, "::1", 3) == 0 ||
      cups_server[0] == '/')
    {
      g_autoptr(GDBusConnection) bus = NULL;
      g_autoptr(GVariant) output = NULL;
      const gchar *ret_error;

      /* Clean .cups/lpoptions before setting
       * default printer on local CUPS server.
       */
      set_local_default_printer (NULL);

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
      if (!bus)
        {
          g_warning ("Failed to get system bus: %s", error->message);
          return FALSE;
        }

      output = g_dbus_connection_call_sync (bus,
                                            MECHANISM_BUS,
                                            "/",
                                            MECHANISM_BUS,
                                            "PrinterSetDefault",
                                            g_variant_new ("(s)", printer_name),
                                            G_VARIANT_TYPE ("(s)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            NULL,
                                            &error);

      if (output == NULL)
        {
          g_warning ("%s", error->message);
          return FALSE;
        }

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: setting default printer to %s failed: %s", printer_name, ret_error);
          return FALSE;
        }

      return TRUE;
    }
  else
    /* Store default printer to .cups/lpoptions
     * if we are connected to a remote CUPS server.
     */
    {
      set_local_default_printer (printer_name);
      return TRUE;
    }
}

gboolean
printer_set_shared (const gchar *printer_name,
                    gboolean     shared)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetShared",
                                        g_variant_new ("(sb)", printer_name, shared),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of sharing of printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_job_sheets (const gchar *printer_name,
                        const gchar *start_sheet,
                        const gchar *end_sheet)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name || !start_sheet || !end_sheet)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetJobSheets",
                                        g_variant_new ("(sss)", printer_name, start_sheet, end_sheet),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of job sheets for printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_policy (const gchar *printer_name,
                    const gchar *policy,
                    gboolean     error_policy)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!printer_name || !policy)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  if (error_policy)
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetErrorPolicy",
                                          g_variant_new ("(ss)", printer_name, policy),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  else
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetOpPolicy",
                                          g_variant_new ("(ss)", printer_name, policy),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of a policy for printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_set_users (const gchar  *printer_name,
                   gchar       **users,
                   gboolean      allowed)
{
  g_autoptr(GDBusConnection) bus = NULL;
  GVariantBuilder     array_builder;
  gint                i;
  g_autoptr(GVariant) output = NULL;
  const gchar        *ret_error;
  g_autoptr(GError)   error = NULL;

  if (!printer_name || !users)
    return TRUE;
  
  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
  for (i = 0; users[i]; i++)
    g_variant_builder_add (&array_builder, "s", users[i]);

  if (allowed)
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetUsersAllowed",
                                          g_variant_new ("(sas)", printer_name, &array_builder),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  else
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetUsersDenied",
                                          g_variant_new ("(sas)", printer_name, &array_builder),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: setting of access list for printer %s failed: %s", printer_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
class_add_printer (const gchar *class_name,
                   const gchar *printer_name)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) output = NULL;
  const gchar *ret_error;
  g_autoptr(GError) error = NULL;

  if (!class_name || !printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "ClassAddPrinter",
                                        g_variant_new ("(ss)", class_name, printer_name),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  if (output == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  g_variant_get (output, "(&s)", &ret_error);
  if (ret_error[0] != '\0')
    {
      g_warning ("cups-pk-helper: adding of printer %s to class %s failed: %s", printer_name, class_name, ret_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
printer_is_local (cups_ptype_t  printer_type,
                  const gchar  *device_uri)
{
  gboolean result = TRUE;
  char     scheme[HTTP_MAX_URI];
  char     username[HTTP_MAX_URI];
  char     hostname[HTTP_MAX_URI];
  char     resource[HTTP_MAX_URI];
  int      port;

  if (printer_type &
      (CUPS_PRINTER_DISCOVERED |
       CUPS_PRINTER_REMOTE |
       CUPS_PRINTER_IMPLICIT))
    result = FALSE;

  if (device_uri == NULL || !result)
    return result;

  httpSeparateURI (HTTP_URI_CODING_ALL, device_uri,
		   scheme, sizeof (scheme), 
		   username, sizeof (username),
		   hostname, sizeof (hostname),
		   &port,
		   resource, sizeof (resource));

  if (g_str_equal (scheme, "ipp") ||
      g_str_equal (scheme, "smb") ||
      g_str_equal (scheme, "socket") ||
      g_str_equal (scheme, "lpd"))
    result = FALSE;

  return result;
}

gchar*
printer_get_hostname (cups_ptype_t  printer_type,
                      const gchar  *device_uri,
                      const gchar  *printer_uri)
{
  gboolean  local = TRUE;
  gchar    *result = NULL;
  char      scheme[HTTP_MAX_URI];
  char      username[HTTP_MAX_URI];
  char      hostname[HTTP_MAX_URI];
  char      resource[HTTP_MAX_URI];
  int       port;

  if (device_uri == NULL)
    return result;

  if (printer_type & (CUPS_PRINTER_DISCOVERED |
                      CUPS_PRINTER_REMOTE |
                      CUPS_PRINTER_IMPLICIT))
    {
      if (printer_uri)
        {
          httpSeparateURI (HTTP_URI_CODING_ALL, printer_uri,
                           scheme, sizeof (scheme),
                           username, sizeof (username),
                           hostname, sizeof (hostname),
                           &port,
                           resource, sizeof (resource));

          if (hostname[0] != '\0')
            result = g_strdup (hostname);
        }

      local = FALSE;
    }

  if (result == NULL && device_uri)
    {
      httpSeparateURI (HTTP_URI_CODING_ALL, device_uri,
                       scheme, sizeof (scheme),
                       username, sizeof (username),
                       hostname, sizeof (hostname),
                       &port,
                       resource, sizeof (resource));

      if (g_str_equal (scheme, "ipp") ||
          g_str_equal (scheme, "smb") ||
          g_str_equal (scheme, "socket") ||
          g_str_equal (scheme, "lpd"))
        {
          if (hostname[0] != '\0')
            result = g_strdup (hostname);

          local = FALSE;
        }
    }

  if (local)
    result = g_strdup ("localhost");

  return result;
}

/* Returns default page size for current locale */
const gchar *
get_page_size_from_locale (void)
{
  if (g_str_equal (gtk_paper_size_get_default (), GTK_PAPER_NAME_LETTER))
    return "Letter";
  else
    return "A4";
}

typedef struct
{
  gchar        *printer_name;
  gchar       **attributes_names;
  GHashTable   *result;
  GIACallback   callback;
  gpointer      user_data;
  GMainContext *context;
} GIAData;

static GIAData *
gia_data_new (const gchar *printer_name, gchar **attributes_names, GIACallback callback, gpointer user_data)
{
  GIAData *data;

  data = g_new0 (GIAData, 1);
  data->printer_name = g_strdup (printer_name);
  data->attributes_names = g_strdupv (attributes_names);
  data->callback = callback;
  data->user_data = user_data;
  data->context = g_main_context_ref_thread_default ();

  return data;
}

static void
gia_data_free (GIAData *data)
{
  g_free (data->printer_name);
  if (data->attributes_names)
    g_strfreev (data->attributes_names);
  if (data->result)
    g_hash_table_unref (data->result);
  if (data->context)
    g_main_context_unref (data->context);
  g_free (data);
}

static gboolean
get_ipp_attributes_idle_cb (gpointer user_data)
{
  GIAData *data = (GIAData *) user_data;

  data->callback (data->result, data->user_data);
  data->result = NULL;

  return FALSE;
}

static void
get_ipp_attributes_cb (gpointer user_data)
{
  GIAData *data = user_data;
  g_autoptr(GSource) idle_source = NULL;

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
                         get_ipp_attributes_idle_cb,
                         data,
                         (GDestroyNotify) gia_data_free);
  g_source_attach (idle_source, data->context);
}

static void
ipp_attribute_free2 (gpointer attr)
{
  IPPAttribute *attribute = (IPPAttribute *) attr;
  ipp_attribute_free (attribute);
}

static gpointer
get_ipp_attributes_func (gpointer user_data)
{
  ipp_attribute_t  *attr = NULL;
  GIAData          *data = user_data;
  ipp_t            *request;
  ipp_t            *response = NULL;
  g_autofree gchar *printer_uri = NULL;
  char            **requested_attrs = NULL;
  gint              i, j, length = 0;

  printer_uri = g_strdup_printf ("ipp://localhost/printers/%s", data->printer_name);

  if (data->attributes_names)
    {
      length = g_strv_length (data->attributes_names);

      requested_attrs = g_new0 (char *, length);
      for (i = 0; data->attributes_names[i]; i++)
        requested_attrs[i] = g_strdup (data->attributes_names[i]);

      request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, printer_uri);
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", length, NULL, (const char **) requested_attrs);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
    }

  if (response)
    {
      if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
        {
          for (j = 0; j < length; j++)
            {
              attr = ippFindAttribute (response, requested_attrs[j], IPP_TAG_ZERO);
              if (attr && ippGetCount (attr) > 0 && ippGetValueTag (attr) != IPP_TAG_NOVALUE)
                {
                  IPPAttribute *attribute;

                  attribute = g_new0 (IPPAttribute, 1);
                  attribute->attribute_name = g_strdup (requested_attrs[j]);
                  attribute->attribute_values = g_new0 (IPPAttributeValue, ippGetCount (attr));
                  attribute->num_of_values = ippGetCount (attr);

                  if (ippGetValueTag (attr) == IPP_TAG_INTEGER ||
                      ippGetValueTag (attr) == IPP_TAG_ENUM)
                    {
                      attribute->attribute_type = IPP_ATTRIBUTE_TYPE_INTEGER;

                      for (i = 0; i < ippGetCount (attr); i++)
                        attribute->attribute_values[i].integer_value = ippGetInteger (attr, i);
                    }
                  else if (ippGetValueTag (attr) == IPP_TAG_NAME ||
                           ippGetValueTag (attr) == IPP_TAG_STRING ||
                           ippGetValueTag (attr) == IPP_TAG_TEXT ||
                           ippGetValueTag (attr) == IPP_TAG_URI ||
                           ippGetValueTag (attr) == IPP_TAG_KEYWORD ||
                           ippGetValueTag (attr) == IPP_TAG_URISCHEME)
                    {
                      attribute->attribute_type = IPP_ATTRIBUTE_TYPE_STRING;

                      for (i = 0; i < ippGetCount (attr); i++)
                        attribute->attribute_values[i].string_value = g_strdup (ippGetString (attr, i, NULL));
                    }
                  else if (ippGetValueTag (attr) == IPP_TAG_RANGE)
                    {
                      attribute->attribute_type = IPP_ATTRIBUTE_TYPE_RANGE;

                      for (i = 0; i < ippGetCount (attr); i++)
                        {
                          attribute->attribute_values[i].lower_range =
                            ippGetRange (attr, i, &(attribute->attribute_values[i].upper_range));
                        }
                    }
                  else if (ippGetValueTag (attr) == IPP_TAG_BOOLEAN)
                    {
                      attribute->attribute_type = IPP_ATTRIBUTE_TYPE_BOOLEAN;

                      for (i = 0; i < ippGetCount (attr); i++)
                        attribute->attribute_values[i].boolean_value = ippGetBoolean (attr, i);
                    }

                  if (!data->result)
                    data->result = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, ipp_attribute_free2);

                  g_hash_table_insert (data->result, g_strdup (requested_attrs[j]), attribute);
                }
            }
        }

      ippDelete (response);
    }


  for (i = 0; i < length; i++)
    g_free (requested_attrs[i]);
  g_free (requested_attrs);

  get_ipp_attributes_cb (data);

  return NULL;
}

void
get_ipp_attributes_async (const gchar  *printer_name,
                          gchar       **attributes_names,
                          GIACallback   callback,
                          gpointer      user_data)
{
  GIAData          *data;
  g_autoptr(GThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  data = gia_data_new (printer_name, attributes_names, callback, user_data);

  thread = g_thread_try_new ("get-ipp-attributes",
                             get_ipp_attributes_func,
                             data,
                             &error);

  if (!thread)
    {
      g_warning ("%s", error->message);
      callback (NULL, user_data);

      gia_data_free (data);
    }
}

IPPAttribute *
ipp_attribute_copy (IPPAttribute *attr)
{
  IPPAttribute *result = NULL;
  gint          i;

  if (attr)
    {
      result = g_new0 (IPPAttribute, 1);

      *result = *attr;
      result->attribute_name = g_strdup (attr->attribute_name);
      result->attribute_values = g_new0 (IPPAttributeValue, attr->num_of_values);
      for (i = 0; i < attr->num_of_values; i++)
        {
          result->attribute_values[i] = attr->attribute_values[i];
          if (attr->attribute_values[i].string_value)
            result->attribute_values[i].string_value = g_strdup (attr->attribute_values[i].string_value);
        }
    }

  return result;
}

void
ipp_attribute_free (IPPAttribute *attr)
{
  gint i;

  if (attr)
    {
      for (i = 0; i < attr->num_of_values; i++)
        g_free (attr->attribute_values[i].string_value);

      g_free (attr->attribute_values);
      g_free (attr->attribute_name);
      g_free (attr);
    }
}

typedef struct
{
  gchar        *printer_name;
  gchar        *ppd_copy;
  GCancellable *cancellable;
  PSPCallback   callback;
  gpointer      user_data;
} PSPData;

static PSPData *
psp_data_new (const gchar *printer_name, const gchar *ppd_copy, GCancellable *cancellable, PSPCallback callback, gpointer user_data)
{
  PSPData *data;

  data = g_new0 (PSPData, 1);
  data->printer_name = g_strdup (printer_name);
  data->ppd_copy = g_strdup (ppd_copy);
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;
  return data;
}

static void
psp_data_free (PSPData *data)
{
  g_free (data->printer_name);
  if (data->ppd_copy != NULL)
    {
      g_unlink (data->ppd_copy);
      g_free (data->ppd_copy);
    }
  g_clear_object (&data->cancellable);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PSPData, psp_data_free)

static void
printer_set_ppd_async_dbus_cb (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;
  gboolean            result = FALSE;
  g_autoptr(PSPData)  data = user_data;
  g_autoptr(GError)   error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("cups-pk-helper: setting of driver for printer %s failed: %s", data->printer_name, ret_error);
      else
        result = TRUE;
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  /* Don't call callback if cancelled */
  if (!data->cancellable ||
      !g_cancellable_is_cancelled (data->cancellable))
    data->callback (data->printer_name,
                    result,
                    data->user_data);
}

/*
 * Set ppd for given printer.
 * Don't use this for classes, just for printers.
 */
void
printer_set_ppd_async (const gchar  *printer_name,
                       const gchar  *ppd_name,
                       GCancellable *cancellable,
                       PSPCallback   callback,
                       gpointer      user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GError) error = NULL;

  if (printer_name == NULL ||
      printer_name[0] == '\0')
    {
      callback (printer_name, FALSE, user_data);
      return;
    }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get system bus: %s", error->message);
      callback (printer_name, FALSE, user_data);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "PrinterAdd",
                          g_variant_new ("(sssss)",
                                         printer_name,
                                         "",
                                         ppd_name,
                                         "",
                                         ""),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          printer_set_ppd_async_dbus_cb,
                          psp_data_new (printer_name, NULL, cancellable, callback, user_data));
}

static void
printer_set_ppd_file_async_scb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  gboolean          success;
  g_autoptr(PSPData) data = user_data;
  g_autoptr(GError) error = NULL;

  success = g_file_copy_finish (G_FILE (source_object),
                                res,
                                &error);

  if (!success)
    {
      g_warning ("%s", error->message);
      data->callback (data->printer_name, FALSE, data->user_data);
      return;
    }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get system bus: %s", error->message);
      data->callback (data->printer_name, FALSE, data->user_data);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "PrinterAddWithPpdFile",
                          g_variant_new ("(sssss)",
                                         data->printer_name,
                                         "",
                                         data->ppd_copy,
                                         "",
                                         ""),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          data->cancellable,
                          printer_set_ppd_async_dbus_cb,
                          data);
   g_steal_pointer (&data);
}

/*
 * Set ppd for given printer.
 * Don't use this for classes, just for printers.
 */
void
printer_set_ppd_file_async (const gchar  *printer_name,
                            const gchar  *ppd_filename,
                            GCancellable *cancellable,
                            PSPCallback   callback,
                            gpointer      user_data)
{
  g_autoptr(GFileIOStream) stream = NULL;
  g_autoptr(GFile) source_ppd_file = NULL;
  g_autoptr(GFile) destination_ppd_file = NULL;

  if (printer_name == NULL ||
      printer_name[0] == '\0')
    {
      callback (printer_name, FALSE, user_data);
      return;
    }

  /*
   * We need to copy the PPD to temp directory at first.
   * This is needed because of SELinux.
   */
  source_ppd_file = g_file_new_for_path (ppd_filename);
  destination_ppd_file = g_file_new_tmp ("g-c-c-XXXXXX.ppd", &stream, NULL);

  g_file_copy_async (source_ppd_file,
                     destination_ppd_file,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     NULL,
                     NULL,
                     printer_set_ppd_file_async_scb,
                     psp_data_new (printer_name, g_file_get_path (destination_ppd_file), cancellable, callback, user_data));
}

typedef void (*GPACallback) (gchar    **attribute_values,
                             gpointer   user_data);

typedef struct
{
  gchar        **ppds_names;
  gchar         *attribute_name;
  gchar        **result;
  GPACallback    callback;
  gpointer       user_data;
  GMainContext  *context;
} GPAData;

static GPAData *
gpa_data_new (gchar **ppds_names, gchar *attribute_name, GPACallback callback, gpointer user_data)
{
  GPAData *data;

  data = g_new0 (GPAData, 1);
  data->ppds_names = g_strdupv (ppds_names);
  data->attribute_name = g_strdup (attribute_name);
  data->callback = callback;
  data->user_data = user_data;
  data->context = g_main_context_ref_thread_default ();

  return data;
}

static void
gpa_data_free (GPAData *data)
{
  g_free (data->attribute_name);
  g_strfreev (data->ppds_names);
  if (data->result != NULL)
    g_strfreev (data->result);
  if (data->context)
    g_main_context_unref (data->context);
  g_free (data);
}

static gboolean
get_ppds_attribute_idle_cb (gpointer user_data)
{
  GPAData *data = (GPAData *) user_data;

  data->callback (data->result, data->user_data);

  return FALSE;
}

static void
get_ppds_attribute_cb (gpointer user_data)
{
  GPAData *data = (GPAData *) user_data;
  g_autoptr(GSource) idle_source = NULL;

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
                         get_ppds_attribute_idle_cb,
                         data,
                         (GDestroyNotify) gpa_data_free);
  g_source_attach (idle_source, data->context);
}

static gpointer
get_ppds_attribute_func (gpointer user_data)
{
  ppd_file_t  *ppd_file;
  ppd_attr_t  *ppd_attr;
  GPAData     *data = user_data;
  gint         i;

  data->result = g_new0 (gchar *, g_strv_length (data->ppds_names) + 1);
  for (i = 0; data->ppds_names[i]; i++)
    {
      g_autofree gchar *ppd_filename = g_strdup (cupsGetServerPPD (CUPS_HTTP_DEFAULT, data->ppds_names[i]));
      if (ppd_filename)
        {
          ppd_file = ppdOpenFile (ppd_filename);
          if (ppd_file)
            {
              ppd_attr = ppdFindAttr (ppd_file, data->attribute_name, NULL);
              if (ppd_attr != NULL)
                data->result[i] = g_strdup (ppd_attr->value);

              ppdClose (ppd_file);
            }

          g_unlink (ppd_filename);
        }
    }

  get_ppds_attribute_cb (data);

  return NULL;
}

/*
 * Get values of requested PPD attribute for given PPDs.
 */
static void
get_ppds_attribute_async (gchar       **ppds_names,
                          gchar        *attribute_name,
                          GPACallback   callback,
                          gpointer      user_data)
{
  GPAData          *data;
  g_autoptr(GThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  if (!ppds_names || !attribute_name)
    {
      callback (NULL, user_data);
      return;
    }

  data = gpa_data_new (ppds_names, attribute_name, callback, user_data);

  thread = g_thread_try_new ("get-ppds-attribute",
                             get_ppds_attribute_func,
                             data,
                             &error);

  if (!thread)
    {
      g_warning ("%s", error->message);
      callback (NULL, user_data);

      gpa_data_free (data);
    }
}



typedef void (*GDACallback) (gchar    *device_id,
                             gchar    *device_make_and_model,
                             gchar    *device_uri,
                             gpointer  user_data);

typedef struct
{
  gchar        *printer_name;
  gchar        *device_uri;
  GList        *backend_list;
  GCancellable *cancellable;
  GDACallback   callback;
  gpointer      user_data;
} GDAData;

static GDAData *
gda_data_new (const gchar *printer_name, GCancellable *cancellable, GDACallback callback, gpointer user_data)
{
  GDAData *data;

  data = g_new0 (GDAData, 1);
  data->printer_name = g_strdup (printer_name);
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
gda_data_free (GDAData *data)
{
  g_free (data->printer_name);
  g_free (data->device_uri);
  g_list_free_full(data->backend_list, g_free);
  g_clear_object (&data->cancellable);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GDAData, gda_data_free)

typedef struct
{
  gchar         *printer_name;
  gint           count;
  PPDName      **result;
  GCancellable  *cancellable;
  GPNCallback    callback;
  gpointer       user_data;
} GPNData;

static GPNData *
gpn_data_new (const gchar *printer_name, gint count, GCancellable *cancellable, GPNCallback callback, gpointer user_data)
{
  GPNData *data;

  data = g_new0 (GPNData, 1);
  data->printer_name = g_strdup (printer_name);
  data->count = count;
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
gpn_data_free (GPNData *data)
{
  g_free (data->printer_name);
  if (data->result != NULL)
    {
      for (int i = 0; data->result[i]; i++)
        {
           g_free (data->result[i]->ppd_name);
           g_free (data->result[i]->ppd_display_name);
           g_free (data->result[i]);
        }
      g_free (data->result);
    }
  g_clear_object (&data->cancellable);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GPNData, gpn_data_free)

static void
get_ppd_names_async_cb (gchar    **attribute_values,
                        gpointer   user_data)
{
  g_autoptr(GPNData) data = user_data;
  gint     i;

  if (g_cancellable_is_cancelled (data->cancellable))
    {
      data->callback (NULL,
                      data->printer_name,
                      TRUE,
                      data->user_data);
      return;
    }

  if (attribute_values)
    {
      for (i = 0; attribute_values[i]; i++)
        data->result[i]->ppd_display_name = g_strdup (attribute_values[i]);
    }

  data->callback (data->result,
                  data->printer_name,
                  FALSE,
                  data->user_data);
}

static void
get_ppd_names_async_dbus_scb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;
  PPDName            *ppd_item;
  PPDName           **result = NULL;
  g_autoptr(GPNData)  data = user_data;
  g_autoptr(GError)   error = NULL;
  GList              *driver_list = NULL;
  GList              *iter;
  gint                i, j, n = 0;
  static const char * const match_levels[] = {
             "exact-cmd",
             "exact",
             "close",
             "generic",
             "none"};

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      g_autoptr(GVariant) array = NULL;

      g_variant_get (output, "(@a(ss))",
                     &array);

      for (j = 0; j < G_N_ELEMENTS (match_levels) && n < data->count; j++)
        {
          g_autoptr(GVariantIter) iter = NULL;
          const gchar *driver, *match;

          g_variant_get (array,
                         "a(ss)",
                         &iter);

          while (g_variant_iter_next (iter, "(&s&s)", &driver, &match))
            {
              if (g_str_equal (match, match_levels[j]) && n < data->count)
                {
                  ppd_item = g_new0 (PPDName, 1);
                  ppd_item->ppd_name = g_strdup (driver);

                  if (g_strcmp0 (match, "exact-cmd") == 0)
                    ppd_item->ppd_match_level = PPD_EXACT_CMD_MATCH;
                  else if (g_strcmp0 (match, "exact") == 0)
                    ppd_item->ppd_match_level = PPD_EXACT_MATCH;
                  else if (g_strcmp0 (match, "close") == 0)
                    ppd_item->ppd_match_level = PPD_CLOSE_MATCH;
                  else if (g_strcmp0 (match, "generic") == 0)
                    ppd_item->ppd_match_level = PPD_GENERIC_MATCH;
                  else if (g_strcmp0 (match, "none") == 0)
                    ppd_item->ppd_match_level = PPD_NO_MATCH;

                  driver_list = g_list_append (driver_list, ppd_item);

                  n++;
                }
            }
        }
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  if (n > 0)
    {
      result = g_new0 (PPDName *, n + 1);
      i = 0;
      for (iter = driver_list; iter; iter = iter->next)
        {
          result[i] = iter->data;
          i++;
        }
    }

  if (result)
    {
      g_auto(GStrv) ppds_names = NULL;

      data->result = result;

      ppds_names = g_new0 (gchar *, n + 1);
      for (i = 0; i < n; i++)
        ppds_names[i] = g_strdup (result[i]->ppd_name);

      get_ppds_attribute_async (ppds_names,
                                "NickName",
                                get_ppd_names_async_cb,
                                data);
      g_steal_pointer (&data);
    }
  else
    {
      data->callback (NULL,
                      data->printer_name,
                      g_cancellable_is_cancelled (data->cancellable),
                      data->user_data);
    }
}

static void
get_device_attributes_cb (gchar    *device_id,
                          gchar    *device_make_and_model,
                          gchar    *device_uri,
                          gpointer  user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPNData) data = user_data;

  if (g_cancellable_is_cancelled (data->cancellable))
    {
      data->callback (NULL,
                      data->printer_name,
                      TRUE,
                      data->user_data);
      return;
    }

  if (!device_id || !device_make_and_model || !device_uri)
    {
      data->callback (NULL,
                      data->printer_name,
                      FALSE,
                      data->user_data);
      return;
    }

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get system bus: %s", error->message);
      data->callback (NULL,
                      data->printer_name,
                      FALSE,
                      data->user_data);
      return;
    }

  g_dbus_connection_call (bus,
                          SCP_BUS,
                          SCP_PATH,
                          SCP_IFACE,
                          "GetBestDrivers",
                          g_variant_new ("(sss)",
                                         device_id,
                                         device_make_and_model,
                                         device_uri),
                          G_VARIANT_TYPE ("(a(ss))"),
                          G_DBUS_CALL_FLAGS_NONE,
                          DBUS_TIMEOUT_LONG,
                          data->cancellable,
                          get_ppd_names_async_dbus_scb,
                          data);
  g_steal_pointer (&data);
}

/*
 * Special item for the list of backends. It represents
 * backends not present in the list itself.
 */
#define OTHER_BACKENDS "other-backends"

/*
 * List of CUPS backends sorted according to their speed,
 * the fastest is the first one. The last item represents
 * backends not present in the list.
 */
const gchar *cups_backends[] = {
  "usb",
  "socket",
  "serial",
  "parallel",
  "lpd",
  "ipp",
  "hp",
  "dnssd",
  "snmp",
  "bluetooth",
  "beh",
  "ncp",
  "hpfax",
  OTHER_BACKENDS
};

static GList *
create_backends_list ()
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < G_N_ELEMENTS (cups_backends); i++)
    list = g_list_prepend (list, g_strdup (cups_backends[i]));
  list = g_list_reverse (list);

  return list;
}

static GVariantBuilder *
create_other_backends_array ()
{
  GVariantBuilder *builder;
  gint             i;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  for (i = 0; i < G_N_ELEMENTS (cups_backends) - 1; i++)
    g_variant_builder_add (builder, "s", cups_backends[i]);

  return builder;
}

static void
get_device_attributes_async_dbus_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)

{
  g_autoptr(GVariant) output = NULL;
  g_autoptr(GDAData)  data = user_data;
  g_autoptr(GError)   error = NULL;
  gchar              *device_id = NULL;
  gchar              *device_make_and_model = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      const gchar *ret_error;
      g_autoptr(GVariant) devices_variant = NULL;
      gint index = -1;

      g_variant_get (output, "(&s@a{ss})",
                     &ret_error,
                     &devices_variant);

      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: getting of attributes for printer %s failed: %s", data->printer_name, ret_error);
        }

      if (data->device_uri)
        {
          g_autoptr(GVariantIter) iter = NULL;
          const gchar *key, *value;
          g_autofree gchar *suffix = NULL;

          g_variant_get (devices_variant,
                         "a{ss}",
                         &iter);

          while (g_variant_iter_next (iter, "{&s&s}", &key, &value))
            {
              if (g_str_equal (value, data->device_uri))
                {
                  gchar *number = g_strrstr (key, ":");
                  if (number != NULL)
                    {
                      gchar *endptr;

                      number++;
                      index = g_ascii_strtoll (number, &endptr, 10);
                      if (index == 0 && endptr == (number))
                        index = -1;
                    }
                }
            }

          suffix = g_strdup_printf (":%d", index);

          g_variant_get (devices_variant,
                         "a{ss}",
                         &iter);

          while (g_variant_iter_next (iter, "{&s&s}", &key, &value))
            {
              if (g_str_has_suffix (key, suffix))
                {
                  if (g_str_has_prefix (key, "device-id"))
                    {
                      device_id = g_strdup (value);
                    }

                  if (g_str_has_prefix (key, "device-make-and-model"))
                    {
                      device_make_and_model = g_strdup (value);
                    }
                }
            }
        }
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  if (!device_id || !device_make_and_model)
    {
      GVariantBuilder *include_scheme_builder = NULL;
      GVariantBuilder *exclude_scheme_builder = NULL;

      g_free (device_id);
      g_free (device_make_and_model);

      device_id = NULL;
      device_make_and_model = NULL;

      if (data->backend_list && !g_cancellable_is_cancelled (data->cancellable))
        {
          const gchar *backend_name;

          backend_name = data->backend_list->data;

          if (g_strcmp0 (backend_name, OTHER_BACKENDS) != 0)
            {
              include_scheme_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
              g_variant_builder_add (include_scheme_builder, "s", backend_name);
            }
          else
            {
              exclude_scheme_builder = create_other_backends_array ();
            }

          data->backend_list = g_list_delete_link (data->backend_list, data->backend_list);

          g_dbus_connection_call (G_DBUS_CONNECTION (g_object_ref (source_object)),
                                  MECHANISM_BUS,
                                  "/",
                                  MECHANISM_BUS,
                                  "DevicesGet",
                                  g_variant_new ("(iiasas)",
                                                 0,
                                                 0,
                                                 include_scheme_builder,
                                                 exclude_scheme_builder),
                                  G_VARIANT_TYPE ("(sa{ss})"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT,
                                  data->cancellable,
                                  get_device_attributes_async_dbus_cb,
                                  data);
          g_steal_pointer (&data);

          if (include_scheme_builder)
            g_variant_builder_unref (include_scheme_builder);

          if (exclude_scheme_builder)
            g_variant_builder_unref (exclude_scheme_builder);

          return;
        }
    }

  data->callback (device_id,
                  device_make_and_model,
                  data->device_uri,
                  data->user_data);
}

static void
get_device_attributes_async_scb (GHashTable *result,
                                 gpointer    user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  GVariantBuilder   include_scheme_builder;
  IPPAttribute     *attr;
  g_autoptr(GDAData) data = user_data;
  g_autoptr(GError) error = NULL;

  if (result)
    {
      attr = g_hash_table_lookup (result, "device-uri");
      if (attr && attr->attribute_type == IPP_ATTRIBUTE_TYPE_STRING &&
          attr->num_of_values > 0)
      data->device_uri = g_strdup (attr->attribute_values[0].string_value);
      g_hash_table_unref (result);
    }

  if (g_cancellable_is_cancelled (data->cancellable))
    {
      data->callback (NULL, NULL, NULL, data->user_data);
      return;
    }

  if (!data->device_uri)
    {
      data->callback (NULL, NULL, NULL, data->user_data);
      return;
    }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get system bus: %s", error->message);
      data->callback (NULL, NULL, NULL, data->user_data);
      return;
    }

  data->backend_list = create_backends_list ();

  g_variant_builder_init (&include_scheme_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&include_scheme_builder, "s", data->backend_list->data);

  data->backend_list = g_list_delete_link (data->backend_list, data->backend_list);

  g_dbus_connection_call (g_object_ref (bus),
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "DevicesGet",
                          g_variant_new ("(iiasas)",
                                         0,
                                         0,
                                         &include_scheme_builder,
                                         NULL),
                          G_VARIANT_TYPE ("(sa{ss})"),
                          G_DBUS_CALL_FLAGS_NONE,
                          DBUS_TIMEOUT,
                          data->cancellable,
                          get_device_attributes_async_dbus_cb,
                          data);
   g_steal_pointer (&data);
}

/*
 * Get device-id, device-make-and-model and device-uri for given printer.
 */
static void
get_device_attributes_async (const gchar  *printer_name,
                             GCancellable *cancellable,
                             GDACallback   callback,
                             gpointer      user_data)
{
  g_auto(GStrv) attributes = NULL;

  if (!printer_name)
   {
     callback (NULL, NULL, NULL, user_data);
     return;
   }

  attributes = g_new0 (gchar *, 2);
  attributes[0] = g_strdup ("device-uri");

  get_ipp_attributes_async (printer_name,
                            attributes,
                            get_device_attributes_async_scb,
                            gda_data_new (printer_name, cancellable, callback, user_data));
}

/*
 * Return "count" best matching driver names for given printer.
 */
void
get_ppd_names_async (gchar        *printer_name,
                     gint          count,
                     GCancellable *cancellable,
                     GPNCallback   callback,
                     gpointer      user_data)
{
  if (!printer_name)
    {
      callback (NULL, NULL, TRUE, user_data);
      return;
    }

  /*
   * We have to find out device-id for this printer at first.
   */
  get_device_attributes_async (printer_name,
                               cancellable,
                               get_device_attributes_cb,
                               gpn_data_new (printer_name, count, cancellable, callback, user_data));
}

typedef struct
{
  PPDList      *result;
  GCancellable *cancellable;
  GAPCallback   callback;
  gpointer      user_data;
  GMainContext *context;
} GAPData;

static GAPData *
gap_data_new (GCancellable *cancellable, GAPCallback callback, gpointer user_data)
{
  GAPData *data;

  data = g_new0 (GAPData, 1);
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;
  data->context = g_main_context_ref_thread_default ();

  return data;
}

static void
gap_data_free (GAPData *data)
{
  if (data->result != NULL)
    ppd_list_free (data->result);
  g_clear_object (&data->cancellable);
  if (data->context)
    g_main_context_unref (data->context);
  g_free (data);
}

static gboolean
get_all_ppds_idle_cb (gpointer user_data)
{
  GAPData *data = user_data;

  if (!g_cancellable_is_cancelled (data->cancellable))
    data->callback (data->result, data->user_data);

  return FALSE;
}

static void
get_all_ppds_cb (gpointer user_data)
{
  GAPData *data = user_data;
  g_autoptr(GSource) idle_source = NULL;

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
                         get_all_ppds_idle_cb,
                         data,
                         (GDestroyNotify) gap_data_free);
  g_source_attach (idle_source, data->context);
}

static const struct {
  const char *normalized_name;
  const char *display_name;
} manufacturers_names[] = {
  { "alps", "Alps" },
  { "anitech", "Anitech" },
  { "apple", "Apple" },
  { "apollo", "Apollo" },
  { "brother", "Brother" },
  { "canon", "Canon" },
  { "citizen", "Citizen" },
  { "citoh", "Citoh" },
  { "compaq", "Compaq" },
  { "dec", "DEC" },
  { "dell", "Dell" },
  { "dnp", "DNP" },
  { "dymo", "Dymo" },
  { "epson", "Epson" },
  { "fujifilm", "Fujifilm" },
  { "fujitsu", "Fujitsu" },
  { "gelsprinter", "Ricoh" },
  { "generic", "Generic" },
  { "genicom", "Genicom" },
  { "gestetner", "Gestetner" },
  { "hewlett packard", "Hewlett-Packard" },
  { "heidelberg", "Heidelberg" },
  { "hitachi", "Hitachi" },
  { "hp", "Hewlett-Packard" },
  { "ibm", "IBM" },
  { "imagen", "Imagen" },
  { "imagistics", "Imagistics" },
  { "infoprint", "InfoPrint" },
  { "infotec", "Infotec" },
  { "intellitech", "Intellitech" },
  { "kodak", "Kodak" },
  { "konica minolta", "Minolta" },
  { "kyocera", "Kyocera" },
  { "kyocera mita", "Kyocera" },
  { "lanier", "Lanier" },
  { "lexmark international", "Lexmark" },
  { "lexmark", "Lexmark" },
  { "minolta", "Minolta" },
  { "minolta qms", "Minolta" },
  { "mitsubishi", "Mitsubishi" },
  { "nec", "NEC" },
  { "nrg", "NRG" },
  { "oce", "Oce" },
  { "oki", "Oki" },
  { "oki data corp", "Oki" },
  { "olivetti", "Olivetti" },
  { "olympus", "Olympus" },
  { "panasonic", "Panasonic" },
  { "pcpi", "PCPI" },
  { "pentax", "Pentax" },
  { "qms", "QMS" },
  { "raven", "Raven" },
  { "raw", "Raw" },
  { "ricoh", "Ricoh" },
  { "samsung", "Samsung" },
  { "savin", "Savin" },
  { "seiko", "Seiko" },
  { "sharp", "Sharp" },
  { "shinko", "Shinko" },
  { "sipix", "SiPix" },
  { "sony", "Sony" },
  { "star", "Star" },
  { "tally", "Tally" },
  { "tektronix", "Tektronix" },
  { "texas instruments", "Texas Instruments" },
  { "toshiba", "Toshiba" },
  { "toshiba tec corp.", "Toshiba" },
  { "xante", "Xante" },
  { "xerox", "Xerox" },
  { "zebra", "Zebra" },
};

static gpointer
get_all_ppds_func (gpointer user_data)
{
  ipp_attribute_t *attr;
  GHashTable      *ppds_hash = NULL;
  GHashTable      *manufacturers_hash = NULL;
  GAPData         *data = user_data;
  PPDName         *item;
  ipp_t           *request;
  ipp_t           *response;
  GList           *list;
  gchar           *manufacturer_display_name;
  gint             i, j;

  request = ippNewRequest (CUPS_GET_PPDS);
  response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");

  if (response &&
      ippGetStatusCode (response) <= IPP_OK_CONFLICT)
    {
      /*
       * This hash contains names of manufacturers as keys and
       * values are GLists of PPD names.
       */
      ppds_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

      /*
       * This hash contains all possible names of manufacturers as keys
       * and values are just first occurrences of their equivalents.
       * This is for mapping of e.g. "Hewlett Packard" and "HP" to the same name
       * (the one which comes first).
       */
      manufacturers_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      for (i = 0; i < G_N_ELEMENTS (manufacturers_names); i++)
        {
          g_hash_table_insert (manufacturers_hash,
                               g_strdup (manufacturers_names[i].normalized_name),
                               g_strdup (manufacturers_names[i].display_name));
        }

      for (attr = ippFirstAttribute (response); attr != NULL; attr = ippNextAttribute (response))
        {
          const gchar      *ppd_device_id = NULL;
          const gchar      *ppd_make_and_model = NULL;
          const gchar      *ppd_name = NULL;
          const gchar      *ppd_product = NULL;
          const gchar      *ppd_make = NULL;
          g_autofree gchar *mdl = NULL;
          g_autofree gchar *mfg = NULL;
          g_autofree gchar *mfg_normalized = NULL;

          while (attr != NULL && ippGetGroupTag (attr) != IPP_TAG_PRINTER)
            attr = ippNextAttribute (response);

          if (attr == NULL)
            break;

          while (attr != NULL && ippGetGroupTag (attr) == IPP_TAG_PRINTER)
            {
              if (g_strcmp0 (ippGetName (attr), "ppd-device-id") == 0 &&
                  ippGetValueTag (attr) == IPP_TAG_TEXT)
                ppd_device_id = ippGetString (attr, 0, NULL);
              else if (g_strcmp0 (ippGetName (attr), "ppd-make-and-model") == 0 &&
                       ippGetValueTag (attr) == IPP_TAG_TEXT)
                ppd_make_and_model = ippGetString (attr, 0, NULL);
              else if (g_strcmp0 (ippGetName (attr), "ppd-name") == 0 &&
                       ippGetValueTag (attr) == IPP_TAG_NAME)
                ppd_name = ippGetString (attr, 0, NULL);
              else if (g_strcmp0 (ippGetName (attr), "ppd-product") == 0 &&
                       ippGetValueTag (attr) == IPP_TAG_TEXT)
                ppd_product = ippGetString (attr, 0, NULL);
              else if (g_strcmp0 (ippGetName (attr), "ppd-make") == 0 &&
                       ippGetValueTag (attr) == IPP_TAG_TEXT)
                ppd_make = ippGetString (attr, 0, NULL);

              attr = ippNextAttribute (response);
            }

          /* Get manufacturer's name */
          if (ppd_device_id && ppd_device_id[0] != '\0')
            {
              mfg = get_tag_value (ppd_device_id, "mfg");
              if (!mfg)
                mfg = get_tag_value (ppd_device_id, "manufacturer");
              mfg_normalized = normalize (mfg);
            }

          if (!mfg &&
              ppd_make &&
              ppd_make[0] != '\0')
            {
              mfg = g_strdup (ppd_make);
              mfg_normalized = normalize (ppd_make);
            }

          /* Get model */
          if (ppd_make_and_model &&
              ppd_make_and_model[0] != '\0')
            {
              mdl = g_strdup (ppd_make_and_model);
            }

          if (!mdl &&
              ppd_product &&
              ppd_product[0] != '\0')
            {
              mdl = g_strdup (ppd_product);
            }

          if (!mdl &&
              ppd_device_id &&
              ppd_device_id[0] != '\0')
            {
              mdl = get_tag_value (ppd_device_id, "mdl");
              if (!mdl)
                mdl = get_tag_value (ppd_device_id, "model");
            }

          if (ppd_name && ppd_name[0] != '\0' &&
              mdl && mdl[0] != '\0' &&
              mfg && mfg[0] != '\0')
            {
              manufacturer_display_name = g_hash_table_lookup (manufacturers_hash, mfg_normalized);
              if (!manufacturer_display_name)
                {
                  g_hash_table_insert (manufacturers_hash, g_strdup (mfg_normalized), g_strdup (mfg));
                }
              else
                {
                  g_free (mfg_normalized);
                  mfg_normalized = normalize (manufacturer_display_name);
                }

              item = g_new0 (PPDName, 1);
              item->ppd_name = g_strdup (ppd_name);
              item->ppd_display_name = g_strdup (mdl);
              item->ppd_match_level = -1;

              list = g_hash_table_lookup (ppds_hash, mfg_normalized);
              if (list)
                {
                  list = g_list_append (list, item);
                }
              else
                {
                  list = g_list_append (list, item);
                  g_hash_table_insert (ppds_hash, g_strdup (mfg_normalized), list);
                }
            }

          if (attr == NULL)
            break;
        }
    }

  if (response)
    ippDelete(response);

  if (ppds_hash &&
      manufacturers_hash)
    {
      GHashTableIter  iter;
      gpointer        key;
      gpointer        value;
      GList          *ppd_item;
      GList          *sort_list = NULL;
      GList          *list_iter;
      gchar          *name;

      data->result = g_new0 (PPDList, 1);
      data->result->num_of_manufacturers = g_hash_table_size (ppds_hash);
      data->result->manufacturers = g_new0 (PPDManufacturerItem *, data->result->num_of_manufacturers);

      g_hash_table_iter_init (&iter, ppds_hash);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          sort_list = g_list_append (sort_list, g_strdup (key));
        }

      /* Sort list of manufacturers */
      sort_list = g_list_sort (sort_list, (GCompareFunc) g_strcmp0);

      /*
       * Fill resulting list of lists (list of manufacturers where
       * each item contains list of PPD names)
       */
      i = 0;
      for (list_iter = sort_list; list_iter; list_iter = list_iter->next)
        {
          name = (gchar *) list_iter->data;
          value = g_hash_table_lookup (ppds_hash, name);

          data->result->manufacturers[i] = g_new0 (PPDManufacturerItem, 1);
          data->result->manufacturers[i]->manufacturer_name = g_strdup (name);
          data->result->manufacturers[i]->manufacturer_display_name = g_strdup (g_hash_table_lookup (manufacturers_hash, name));
          data->result->manufacturers[i]->num_of_ppds = g_list_length ((GList *) value);
          data->result->manufacturers[i]->ppds = g_new0 (PPDName *, data->result->manufacturers[i]->num_of_ppds);

          for (ppd_item = (GList *) value, j = 0; ppd_item; ppd_item = ppd_item->next, j++)
            {
              data->result->manufacturers[i]->ppds[j] = ppd_item->data;
            }

          g_list_free ((GList *) value);

          i++;
        }

      g_list_free_full (sort_list, g_free);
      g_hash_table_destroy (ppds_hash);
      g_hash_table_destroy (manufacturers_hash);
    }

  get_all_ppds_cb (data);

  return NULL;
}

/*
 * Get names of all installed PPDs sorted by manufacturers names.
 */
void
get_all_ppds_async (GCancellable *cancellable,
                    GAPCallback   callback,
                    gpointer      user_data)
{
  GAPData          *data;
  g_autoptr(GThread) thread = NULL;
  g_autoptr(GError)  error = NULL;

  data = gap_data_new (cancellable, callback, user_data);

  thread = g_thread_try_new ("get-all-ppds",
                             get_all_ppds_func,
                             data,
                             &error);

  if (!thread)
    {
      g_warning ("%s", error->message);
      callback (NULL, user_data);

      gap_data_free (data);
    }
}

PPDList *
ppd_list_copy (PPDList *list)
{
  PPDList *result = NULL;
  gint     i, j;

  if (list)
    {
      result = g_new0 (PPDList, 1);
      result->num_of_manufacturers = list->num_of_manufacturers;
      result->manufacturers = g_new0 (PPDManufacturerItem *, list->num_of_manufacturers);

      for (i = 0; i < result->num_of_manufacturers; i++)
        {
          result->manufacturers[i] = g_new0 (PPDManufacturerItem, 1);
          result->manufacturers[i]->num_of_ppds = list->manufacturers[i]->num_of_ppds;
          result->manufacturers[i]->ppds = g_new0 (PPDName *, result->manufacturers[i]->num_of_ppds);

          result->manufacturers[i]->manufacturer_display_name =
            g_strdup (list->manufacturers[i]->manufacturer_display_name);

          result->manufacturers[i]->manufacturer_name =
            g_strdup (list->manufacturers[i]->manufacturer_name);

          for (j = 0; j < result->manufacturers[i]->num_of_ppds; j++)
            {
              result->manufacturers[i]->ppds[j] = g_new0 (PPDName, 1);

              result->manufacturers[i]->ppds[j]->ppd_display_name =
                g_strdup (list->manufacturers[i]->ppds[j]->ppd_display_name);

              result->manufacturers[i]->ppds[j]->ppd_name =
                g_strdup (list->manufacturers[i]->ppds[j]->ppd_name);

              result->manufacturers[i]->ppds[j]->ppd_match_level =
                list->manufacturers[i]->ppds[j]->ppd_match_level;
            }
        }
    }

  return result;
}

void
ppd_list_free (PPDList *list)
{
  gint i, j;

  if (list)
    {
      for (i = 0; i < list->num_of_manufacturers; i++)
        {
          for (j = 0; j < list->manufacturers[i]->num_of_ppds; j++)
            {
              g_free (list->manufacturers[i]->ppds[j]->ppd_name);
              g_free (list->manufacturers[i]->ppds[j]->ppd_display_name);
              g_free (list->manufacturers[i]->ppds[j]);
            }

          g_free (list->manufacturers[i]->manufacturer_name);
          g_free (list->manufacturers[i]->manufacturer_display_name);
          g_free (list->manufacturers[i]->ppds);
          g_free (list->manufacturers[i]);
        }

      g_free (list->manufacturers);
      g_free (list);
    }
}

gchar *
get_standard_manufacturers_name (const gchar *name)
{
  g_autofree gchar *normalized_name = NULL;
  gint   i;

  if (name == NULL)
    return NULL;

  normalized_name = normalize (name);

  for (i = 0; i < G_N_ELEMENTS (manufacturers_names); i++)
    {
      if (g_strcmp0 (manufacturers_names[i].normalized_name, normalized_name) == 0)
        {
          return g_strdup (manufacturers_names[i].display_name);
        }
    }

  return NULL;
}

typedef struct
{
  gchar        *printer_name;
  gchar        *host_name;
  gint          port;
  gchar        *result;
  PGPCallback   callback;
  gpointer      user_data;
  GMainContext *context;
} PGPData;

static PGPData *
pgp_data_new (const gchar *printer_name, const gchar *host_name, gint port, PGPCallback callback, gpointer user_data)
{
  PGPData *data;

  data = g_new0 (PGPData, 1);
  data->printer_name = g_strdup (printer_name);
  data->host_name = g_strdup (host_name);
  data->port = port;
  data->callback = callback;
  data->user_data = user_data;
  data->context = g_main_context_ref_thread_default ();

  return data;
}

static void
pgp_data_free (PGPData *data)
{
  g_free (data->printer_name);
  g_free (data->host_name);
  g_free (data->result);
  if (data->context)
    g_main_context_unref (data->context);
  g_free (data);
}

static gboolean
printer_get_ppd_idle_cb (gpointer user_data)
{
  PGPData *data = user_data;

  data->callback (data->result, data->user_data);

  return FALSE;
}

static void
printer_get_ppd_cb (gpointer user_data)
{
  PGPData *data = user_data;
  g_autoptr(GSource) idle_source = NULL;

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
                         printer_get_ppd_idle_cb,
                         data,
                         (GDestroyNotify) pgp_data_free);
  g_source_attach (idle_source, data->context);
}

static gpointer
printer_get_ppd_func (gpointer user_data)
{
  PGPData *data = user_data;

  if (data->host_name)
    {
      http_t *http;

#ifdef HAVE_CUPS_HTTPCONNECT2
      http = httpConnect2 (data->host_name, data->port, NULL, AF_UNSPEC,
                           HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
#else
      http = httpConnect (data->host_name, data->port);
#endif
      if (http)
        {
          data->result = g_strdup (cupsGetPPD2 (http, data->printer_name));
          httpClose (http);
        }
    }
  else
    {
      data->result = g_strdup (cupsGetPPD (data->printer_name));
    }

  printer_get_ppd_cb (data);

  return NULL;
}

void
printer_get_ppd_async (const gchar *printer_name,
                       const gchar *host_name,
                       gint         port,
                       PGPCallback  callback,
                       gpointer     user_data)
{
  PGPData          *data;
  g_autoptr(GThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  data = pgp_data_new (printer_name, host_name, port, callback, user_data);

  thread = g_thread_try_new ("printer-get-ppd",
                             printer_get_ppd_func,
                             data,
                             &error);

  if (!thread)
    {
      g_warning ("%s", error->message);
      callback (NULL, user_data);

      pgp_data_free (data);
    }
}

typedef struct
{
  gchar        *printer_name;
  cups_dest_t  *result;
  GNDCallback   callback;
  gpointer      user_data;
  GMainContext *context;
} GNDData;

static GNDData *
gnd_data_new (const gchar *printer_name, GNDCallback callback, gpointer user_data)
{
  GNDData *data;

  data = g_new0 (GNDData, 1);
  data->printer_name = g_strdup (printer_name);
  data->callback = callback;
  data->user_data = user_data;
  data->context = g_main_context_ref_thread_default ();

  return data;
}

static void
gnd_data_free (GNDData *data)
{
  g_free (data->printer_name);
  if (data->context)
    g_main_context_unref (data->context);
  g_free (data);
}

static gboolean
get_named_dest_idle_cb (gpointer user_data)
{
  GNDData *data = user_data;

  data->callback (data->result, data->user_data);

  return FALSE;
}

static void
get_named_dest_cb (gpointer user_data)
{
  GNDData *data = user_data;
  g_autoptr(GSource) idle_source = NULL;

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
                         get_named_dest_idle_cb,
                         data,
                         (GDestroyNotify) gnd_data_free);
  g_source_attach (idle_source, data->context);
}

static gpointer
get_named_dest_func (gpointer user_data)
{
  GNDData *data = user_data;

  data->result = cupsGetNamedDest (CUPS_HTTP_DEFAULT, data->printer_name, NULL);

  get_named_dest_cb (data);

  return NULL;
}

void
get_named_dest_async (const gchar *printer_name,
                      GNDCallback  callback,
                      gpointer     user_data)
{
  GNDData          *data;
  g_autoptr(GThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  data = gnd_data_new (printer_name, callback, user_data);

  thread = g_thread_try_new ("get-named-dest",
                             get_named_dest_func,
                             data,
                             &error);

  if (!thread)
    {
      g_warning ("%s", error->message);
      callback (NULL, user_data);

      gnd_data_free (data);
    }
}

typedef struct
{
  GCancellable *cancellable;
  PAOCallback   callback;
  gpointer      user_data;
} PAOData;

static PAOData *
pao_data_new (GCancellable *cancellable, PAOCallback callback, gpointer user_data)
{
  PAOData *data;

  data = g_new0 (PAOData, 1);
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;
  return data;
}

static void
pao_data_free (PAOData *data)
{
  g_clear_object (&data->cancellable);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PAOData, pao_data_free)

static void
printer_add_option_async_dbus_cb (GObject      *source_object,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  g_autoptr(GVariant) output = NULL;
  gboolean            success = FALSE;
  g_autoptr(PAOData)  data = user_data;
  g_autoptr(GError)   error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("cups-pk-helper: setting of an option failed: %s", ret_error);
      else
        success = TRUE;
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  if (!g_cancellable_is_cancelled (data->cancellable))
    data->callback (success, data->user_data);
}

void
printer_add_option_async (const gchar   *printer_name,
                          const gchar   *option_name,
                          gchar        **values,
                          gboolean       set_default,
                          GCancellable  *cancellable,
                          PAOCallback    callback,
                          gpointer       user_data)
{
  GVariantBuilder   array_builder;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GError) error = NULL;
  gint              i;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     callback (FALSE, user_data);
     return;
   }

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
  if (values)
    {
      for (i = 0; values[i]; i++)
        g_variant_builder_add (&array_builder, "s", values[i]);
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          set_default ? "PrinterAddOptionDefault" :
                                        "PrinterAddOption",
                          g_variant_new ("(ssas)",
                                         printer_name,
                                         option_name,
                                         &array_builder),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          DBUS_TIMEOUT,
                          cancellable,
                          printer_add_option_async_dbus_cb,
                          pao_data_new (cancellable, callback, user_data));
}

typedef struct
{
  GList        *backend_list;
  GCancellable *cancellable;
  GCDCallback   callback;
  gpointer      user_data;
} GCDData;

static GCDData *
gcd_data_new (GList *backend_list, GCancellable *cancellable, GCDCallback callback, gpointer user_data)
{
  GCDData *data;

  data = g_new0 (GCDData, 1);
  data->backend_list = backend_list;
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
gcd_data_free (GCDData *data)
{
  g_list_free_full (data->backend_list, g_free);
  g_clear_object (&data->cancellable);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GCDData, gcd_data_free)

static gint
get_suffix_index (const gchar *string)
{
  gchar *number;
  gchar *endptr;
  gint   index = -1;

  number = g_strrstr (string, ":");
  if (number)
    {
      number++;
      index = g_ascii_strtoll (number, &endptr, 10);
      if (index == 0 && endptr == number)
        index = -1;
    }

  return index;
}

static void
get_cups_devices_async_dbus_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)

{
  g_autoptr(GPtrArray) devices = NULL;
  g_autoptr(GVariant) output = NULL;
  g_autoptr(GCDData)  data = user_data;
  g_autoptr(GError)   error = NULL;
  gint                num_of_devices = 0;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      const gchar *ret_error;
      g_autoptr(GVariant) devices_variant = NULL;
      gboolean     is_network_device;
      g_autoptr(GVariantIter) iter = NULL;
      const gchar  *key, *value;
      gint          index = -1, max_index = -1, i;

      g_variant_get (output, "(&s@a{ss})",
                     &ret_error,
                     &devices_variant);

      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: getting of CUPS devices failed: %s", ret_error);
        }

      g_variant_get (devices_variant, "a{ss}", &iter);
      while (g_variant_iter_next (iter, "{&s&s}", &key, &value))
        {
          index = get_suffix_index (key);
          if (index > max_index)
            max_index = index;
        }

      if (max_index >= 0)
        {
          g_autoptr(GVariantIter) iter2 = NULL;

          num_of_devices = max_index + 1;
          devices = g_ptr_array_new_with_free_func (g_object_unref);
          for (i = 0; i < num_of_devices; i++)
             g_ptr_array_add (devices, pp_print_device_new ());

          g_variant_get (devices_variant, "a{ss}", &iter2);
          while (g_variant_iter_next (iter2, "{&s&s}", &key, &value))
            {
              PpPrintDevice *device;

              index = get_suffix_index (key);
              if (index >= 0)
                {
                  device = g_ptr_array_index (devices, index);
                  if (g_str_has_prefix (key, "device-class"))
                    {
                      is_network_device = g_strcmp0 (value, "network") == 0;
                      g_object_set (device, "is-network-device", is_network_device, NULL);
                    }
                  else if (g_str_has_prefix (key, "device-id"))
                    g_object_set (device, "device-id", value, NULL);
                  else if (g_str_has_prefix (key, "device-info"))
                    g_object_set (device, "device-info", value, NULL);
                  else if (g_str_has_prefix (key, "device-make-and-model"))
                    {
                      g_object_set (device,
                                    "device-make-and-model", value,
                                    "device-name", value,
                                    NULL);
                    }
                  else if (g_str_has_prefix (key, "device-uri"))
                    g_object_set (device, "device-uri", value, NULL);
                  else if (g_str_has_prefix (key, "device-location"))
                    g_object_set (device, "device-location", value, NULL);

                  g_object_set (device, "acquisition-method", ACQUISITION_METHOD_DEFAULT_CUPS_SERVER, NULL);
                }
            }
        }
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      data->callback (devices,
                      TRUE,
                      g_cancellable_is_cancelled (data->cancellable),
                      data->user_data);
      return;
    }

  if (data->backend_list)
    {
      if (!g_cancellable_is_cancelled (data->cancellable))
        {
          GVariantBuilder  *include_scheme_builder = NULL;
          GVariantBuilder  *exclude_scheme_builder = NULL;
          g_autofree gchar *backend_name = NULL;

          backend_name = data->backend_list->data;

          data->callback (devices,
                          FALSE,
                          FALSE,
                          data->user_data);

          if (g_strcmp0 (backend_name, OTHER_BACKENDS) != 0)
            {
              include_scheme_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
              g_variant_builder_add (include_scheme_builder, "s", backend_name);
            }
          else
            {
              exclude_scheme_builder = create_other_backends_array ();
            }

          data->backend_list = g_list_delete_link (data->backend_list, data->backend_list);

          g_dbus_connection_call (G_DBUS_CONNECTION (g_object_ref (source_object)),
                                  MECHANISM_BUS,
                                  "/",
                                  MECHANISM_BUS,
                                  "DevicesGet",
                                  g_variant_new ("(iiasas)",
                                                 0,
                                                 0,
                                                 include_scheme_builder,
                                                 exclude_scheme_builder),
                                  G_VARIANT_TYPE ("(sa{ss})"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT,
                                  data->cancellable,
                                  get_cups_devices_async_dbus_cb,
                                  data);
          g_steal_pointer (&data);

          if (include_scheme_builder)
            g_variant_builder_unref (include_scheme_builder);

          if (exclude_scheme_builder)
            g_variant_builder_unref (exclude_scheme_builder);

          return;
        }
      else
        {
          data->callback (devices,
                          TRUE,
                          TRUE,
                          data->user_data);
        }
    }
  else
    {
      data->callback (devices,
                      TRUE,
                      g_cancellable_is_cancelled (data->cancellable),
                      data->user_data);
    }
}

void
get_cups_devices_async (GCancellable *cancellable,
                        GCDCallback   callback,
                        gpointer      user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  GVariantBuilder   include_scheme_builder;
  GList *backend_list;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *backend_name = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     callback (NULL, TRUE, FALSE, user_data);
     return;
   }

  backend_list = create_backends_list ();

  backend_name = backend_list->data;

  g_variant_builder_init (&include_scheme_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&include_scheme_builder, "s", backend_name);

  backend_list = g_list_delete_link (backend_list, backend_list);

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "DevicesGet",
                          g_variant_new ("(iiasas)",
                                         0,
                                         0,
                                         &include_scheme_builder,
                                         NULL),
                          G_VARIANT_TYPE ("(sa{ss})"),
                          G_DBUS_CALL_FLAGS_NONE,
                          DBUS_TIMEOUT,
                          cancellable,
                          get_cups_devices_async_dbus_cb,
                          gcd_data_new (backend_list, cancellable, callback, user_data));
}

gchar *
guess_device_hostname (PpPrintDevice *device)
{
  http_uri_status_t  status;
  char               scheme[HTTP_MAX_URI];
  char               username[HTTP_MAX_URI];
  char               hostname[HTTP_MAX_URI];
  char               resource[HTTP_MAX_URI];
  int                port;
  gchar             *result = NULL;
  gchar             *hostname_begin;
  gchar             *hostname_end = NULL;

  if (device != NULL && pp_print_device_get_device_uri (device) != NULL)
    {
      if (g_str_has_prefix (pp_print_device_get_device_uri (device), "socket") ||
          g_str_has_prefix (pp_print_device_get_device_uri (device), "lpd") ||
          g_str_has_prefix (pp_print_device_get_device_uri (device), "ipp") ||
          g_str_has_prefix (pp_print_device_get_device_uri (device), "smb"))
        {
          status = httpSeparateURI (HTTP_URI_CODING_ALL,
                                    pp_print_device_get_device_uri (device),
                                    scheme, HTTP_MAX_URI,
                                    username, HTTP_MAX_URI,
                                    hostname, HTTP_MAX_URI,
                                    &port,
                                    resource, HTTP_MAX_URI);

          if (status >= HTTP_URI_STATUS_OK &&
              hostname[0] != '\0')
            result = g_strdup (hostname);
        }
      else if ((g_str_has_prefix (pp_print_device_get_device_uri (device), "dnssd") ||
                g_str_has_prefix (pp_print_device_get_device_uri (device), "mdns")) &&
               pp_print_device_get_device_info (device) != NULL)
        {
          /*
           * CUPS browses its printers as
           * "PrinterName @ ComputerName" or "PrinterInfo @ ComputerName"
           * through DNS-SD.
           */
          hostname_begin = g_strrstr (pp_print_device_get_device_info (device), " @ ");
          if (hostname_begin != NULL)
            result = g_strdup (hostname_begin + 3);
        }
      else if (g_str_has_prefix (pp_print_device_get_device_uri (device), "hp:/net/") ||
               g_str_has_prefix (pp_print_device_get_device_uri (device), "hpfax:/net/"))
        {
          /*
           * HPLIP printers have URI of form hp:/net/%s?ip=%s&port=%d
           * or hp:/net/%s?ip=%s.
           */
          hostname_begin = g_strrstr (pp_print_device_get_device_uri (device), "ip=");
          if (hostname_begin != NULL)
            {
              hostname_begin += 3;
              hostname_end = strstr (hostname_begin, "&");
            }

          if (hostname_end != NULL)
            result = g_strndup (hostname_begin, hostname_end - hostname_begin);
          else
            result = g_strdup (hostname_begin);
        }
    }

  return result;
}

gchar *
canonicalize_device_name (GList         *device_names,
                          GPtrArray     *local_cups_devices,
                          cups_dest_t   *dests,
                          gint           num_of_dests,
                          PpPrintDevice *device)
{
  PpPrintDevice             *item;
  gboolean                   already_present;
  GList                     *iter;
  gsize                      len;
  g_autofree gchar          *name = NULL;
  gchar                     *occurrence;
  gint                       name_index, j;
  static const char * const  residues[] = {
    "-foomatic",
    "-hpijs",
    "-hpcups",
    "-cups",
    "-gutenprint",
    "-series",
    "-label-printer",
    "-dot-matrix",
    "-ps3",
    "-ps2",
    "-br-script",
    "-kpdl",
    "-pcl3",
    "-pcl",
    "-zxs",
    "-pxl"};

  if (pp_print_device_get_device_id (device) != NULL)
    {
      name = get_tag_value (pp_print_device_get_device_id (device), "mdl");
      if (name == NULL)
        name = get_tag_value (pp_print_device_get_device_id (device), "model");
    }

  if (name == NULL &&
      pp_print_device_get_device_make_and_model (device) != NULL &&
      pp_print_device_get_device_make_and_model (device)[0] != '\0')
    {
      name = g_strdup (pp_print_device_get_device_make_and_model (device));
    }

  if (name == NULL &&
      pp_print_device_get_device_original_name (device) != NULL &&
      pp_print_device_get_device_original_name (device)[0] != '\0')
    {
      name = g_strdup (pp_print_device_get_device_original_name (device));
    }

  if (name == NULL &&
      pp_print_device_get_device_info (device) != NULL &&
      pp_print_device_get_device_info (device)[0] != '\0')
    {
      name = g_strdup (pp_print_device_get_device_info (device));
    }

  if (name == NULL)
    return NULL;

  g_strstrip (name);
  g_strcanon (name, ALLOWED_CHARACTERS, '-');

  /* Remove common strings found in driver names */
  for (j = 0; j < G_N_ELEMENTS (residues); j++)
    {
      g_autofree gchar *lower_name = g_ascii_strdown (name, -1);

      occurrence = g_strrstr (lower_name, residues[j]);
      if (occurrence != NULL)
        {
          occurrence[0] = '\0';
          name[strlen (lower_name)] = '\0';
        }
    }

  /* Remove trailing "-" */
  len = strlen (name);
  while (len-- && name[len] == '-')
    name[len] = '\0';

  /* Merge "--" to "-" */
  occurrence = g_strrstr (name, "--");
  while (occurrence != NULL)
    {
      shift_string_left (occurrence);
      occurrence = g_strrstr (name, "--");
    }

  /* Remove leading "-" */
  if (name[0] == '-')
    shift_string_left (name);

  name_index = 2;
  already_present = FALSE;
  while (TRUE)
    {
      g_autofree gchar *new_name = NULL;

      if (already_present)
        {
          new_name = g_strdup_printf ("%s-%d", name, name_index);
          name_index++;
        }
      else
        {
          new_name = g_strdup (name);
        }

      already_present = FALSE;
      for (j = 0; j < num_of_dests; j++)
        if (g_strcmp0 (dests[j].name, new_name) == 0)
          already_present = TRUE;

      for (iter = device_names; iter; iter = iter->next)
        {
          gchar *device_original_name = iter->data;
          if (g_strcmp0 (device_original_name, new_name) == 0)
            already_present = TRUE;
        }

      for (guint i = 0; i < local_cups_devices->len; i++)
        {
          item = g_ptr_array_index (local_cups_devices, i);
          if (g_strcmp0 (pp_print_device_get_device_original_name (item), new_name) == 0)
            already_present = TRUE;
        }

      if (!already_present)
        return g_steal_pointer (&new_name);
    }
}

void
shift_string_left (gchar *str)
{
  gchar *next;

  if (str != NULL && str[0] != '\0')
    {
      next = g_utf8_find_next_char (str, NULL);
      memmove (str, next, strlen (next) + 1);
    }
}

gboolean
printer_name_is_valid (const gchar *str)
{
  const gchar *invalid_chars = " \t#/";
  return strlen(str) == strcspn(str, invalid_chars);
}
