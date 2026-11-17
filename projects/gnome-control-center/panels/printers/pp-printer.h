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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_PRINTER (pp_printer_get_type ())
G_DECLARE_FINAL_TYPE (PpPrinter, pp_printer, PP, PRINTER, GObject)

GType        pp_printer_get_type      (void) G_GNUC_CONST;

PpPrinter   *pp_printer_new           (const gchar          *name);

const gchar *pp_printer_get_name      (PpPrinter            *printer);

void         pp_printer_rename_async  (PpPrinter            *printer,
                                       const gchar          *new_printer_name,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);

gboolean     pp_printer_rename_finish (PpPrinter            *printer,
                                       GAsyncResult         *res,
                                       GError              **error);

void         pp_printer_delete_async  (PpPrinter            *printer,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);

gboolean     pp_printer_delete_finish (PpPrinter            *printer,
                                       GAsyncResult         *res,
                                       GError              **error);

gboolean     pp_printer_delete_sync   (PpPrinter            *self,
                                       GCancellable         *cancellable,
                                       GError              **error);

void         pp_printer_get_jobs_async (PpPrinter           *printer,
                                        gboolean             myjobs,
                                        gint                 which_jobs,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

GPtrArray   *pp_printer_get_jobs_finish (PpPrinter          *printer,
                                         GAsyncResult       *res,
                                         GError            **error);

void         pp_printer_print_file_async (PpPrinter           *printer,
                                          const gchar         *filename,
                                          const gchar         *job_name,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);

gboolean     pp_printer_print_file_finish (PpPrinter         *printer,
                                           GAsyncResult      *res,
                                           GError           **error);

G_END_DECLS
