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

#pragma once

#include <gtk/gtk.h>
#include <cups/cups.h>

#include "pp-print-device.h"

#define ALLOWED_CHARACTERS "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"

#define MECHANISM_BUS "org.opensuse.CupsPkHelper.Mechanism"

#define SCP_BUS   "org.fedoraproject.Config.Printing"
#define SCP_PATH  "/org/fedoraproject/Config/Printing"
#define SCP_IFACE "org.fedoraproject.Config.Printing"

G_BEGIN_DECLS

typedef void (*UserResponseCallback) (GtkWindow *dialog, gint response_id, gpointer user_data);

/*
 * Match level of PPD driver.
 */
enum
{
  PPD_NO_MATCH = 0,
  PPD_GENERIC_MATCH,
  PPD_CLOSE_MATCH,
  PPD_EXACT_MATCH,
  PPD_EXACT_CMD_MATCH
};

enum
{
  ACQUISITION_METHOD_DEFAULT_CUPS_SERVER = 0,
  ACQUISITION_METHOD_REMOTE_CUPS_SERVER,
  ACQUISITION_METHOD_SNMP,
  ACQUISITION_METHOD_SAMBA,
  ACQUISITION_METHOD_SAMBA_HOST,
  ACQUISITION_METHOD_JETDIRECT,
  ACQUISITION_METHOD_LPD
};

typedef struct
{
  gchar *ppd_name;
  gchar *ppd_display_name;
  gint   ppd_match_level;
} PPDName;

typedef struct
{
  gchar    *manufacturer_name;
  gchar    *manufacturer_display_name;
  PPDName **ppds;
  gsize     num_of_ppds;
} PPDManufacturerItem;

typedef struct
{
  PPDManufacturerItem **manufacturers;
  gsize                 num_of_manufacturers;
} PPDList;

gchar      *get_tag_value (const gchar *tag_string,
                           const gchar *tag_name);

char       *get_dest_attr (const char *dest_name,
                           const char *attr);

gchar      *get_ppd_attribute (const gchar *ppd_file_name,
                               const gchar *attribute_name);

void        set_local_default_printer (const gchar *printer_name);

gboolean    printer_set_location (const gchar *printer_name,
                                  const gchar *location);

gboolean    printer_set_accepting_jobs (const gchar *printer_name,
                                        gboolean     accepting_jobs,
                                        const gchar *reason);

gboolean    printer_set_enabled (const gchar *printer_name,
                                 gboolean     enabled);

gboolean    printer_rename (const gchar *old_name,
                            const gchar *new_name);

gboolean    printer_delete (const gchar *printer_name);

gboolean    printer_set_default (const gchar *printer_name);

gboolean    printer_set_shared (const gchar *printer_name,
                                gboolean     shared);

gboolean    printer_set_job_sheets (const gchar *printer_name,
                                    const gchar *start_sheet,
                                    const gchar *end_sheet);

gboolean    printer_set_policy (const gchar *printer_name,
                                const gchar *policy,
                                gboolean     error_policy);

gboolean    printer_set_users (const gchar  *printer_name,
                               gchar       **users,
                               gboolean      allowed);

gboolean    class_add_printer (const gchar *class_name,
                               const gchar *printer_name);

gboolean    printer_is_local (cups_ptype_t  printer_type,
                              const gchar  *device_uri);

gchar      *printer_get_hostname (cups_ptype_t  printer_type,
                                  const gchar  *device_uri,
                                  const gchar  *printer_uri);

typedef void (*PSPCallback) (const gchar *printer_name,
                             gboolean  success,
                             gpointer  user_data);

void        printer_set_ppd_async (const gchar  *printer_name,
                                   const gchar  *ppd_name,
                                   GCancellable *cancellable,
                                   PSPCallback   callback,
                                   gpointer      user_data);

void        printer_set_ppd_file_async (const gchar *printer_name,
                                        const gchar *ppd_filename,
                                        GCancellable *cancellable,
                                        PSPCallback   callback,
                                        gpointer      user_data);

typedef void (*GPNCallback) (PPDName     **names,
                             const gchar  *printer_name,
                             gboolean      cancelled,
                             gpointer      user_data);

void        get_ppd_names_async (gchar        *printer_name,
                                 gint          count,
                                 GCancellable *cancellable,
                                 GPNCallback   callback,
                                 gpointer      user_data);

typedef void (*GAPCallback) (PPDList  *ppds,
                             gpointer  user_data);

void        get_all_ppds_async (GCancellable *cancellable,
                                GAPCallback   callback,
                                gpointer      user_data);

PPDList    *ppd_list_copy (PPDList *list);
void        ppd_list_free (PPDList *list);

enum
{
  IPP_ATTRIBUTE_TYPE_INTEGER = 0,
  IPP_ATTRIBUTE_TYPE_STRING,
  IPP_ATTRIBUTE_TYPE_RANGE,
  IPP_ATTRIBUTE_TYPE_BOOLEAN
};

typedef struct
{
  gboolean  boolean_value;
  gchar    *string_value;
  gint      integer_value;
  gint      lower_range;
  gint      upper_range;
} IPPAttributeValue;

typedef struct
{
  gchar             *attribute_name;
  IPPAttributeValue *attribute_values;
  gint               num_of_values;
  gint               attribute_type;
} IPPAttribute;

typedef void (*GIACallback) (GHashTable *table,
                             gpointer    user_data);

void        get_ipp_attributes_async (const gchar  *printer_name,
                                      gchar       **attributes_names,
                                      GIACallback   callback,
                                      gpointer      user_data);

IPPAttribute *ipp_attribute_copy (IPPAttribute *attr);

void        ipp_attribute_free (IPPAttribute *attr);

gchar      *get_standard_manufacturers_name (const gchar *name);

typedef void (*PGPCallback) (const gchar *ppd_filename,
                             gpointer     user_data);

void        printer_get_ppd_async (const gchar *printer_name,
                                   const gchar *host_name,
                                   gint         port,
                                   PGPCallback  callback,
                                   gpointer     user_data);

/* NOTE: 'destination' is passed with ownership as cupsCopyDest doesn't seem to work as expected */
typedef void (*GNDCallback) (cups_dest_t *destination,
                             gpointer     user_data);

void        get_named_dest_async (const gchar *printer_name,
                                  GNDCallback  callback,
                                  gpointer     user_data);

typedef void (*PAOCallback) (gboolean success,
                             gpointer user_data);

void        printer_add_option_async (const gchar   *printer_name,
                                      const gchar   *option_name,
                                      gchar        **values,
                                      gboolean       set_default,
                                      GCancellable  *cancellable,
                                      PAOCallback    callback,
                                      gpointer       user_data);

const gchar *get_page_size_from_locale (void);

typedef void (*GCDCallback) (GPtrArray      *devices,
                             gboolean        finished,
                             gboolean        cancelled,
                             gpointer        user_data);

void        get_cups_devices_async (GCancellable *cancellable,
                                    GCDCallback   callback,
                                    gpointer      user_data);

gchar      *guess_device_hostname (PpPrintDevice *device);

gchar      *canonicalize_device_name (GList         *device_names,
                                      GPtrArray     *local_cups_devices,
                                      cups_dest_t   *dests,
                                      gint           num_of_dests,
                                      PpPrintDevice *device);

void        shift_string_left (gchar *str);

gboolean    printer_name_is_valid (const gchar *str);

G_END_DECLS
