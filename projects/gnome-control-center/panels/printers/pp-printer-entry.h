/*
 * Copyright 2017 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>
#include <cups/cups.h>

#define PP_PRINTER_ENTRY_TYPE (pp_printer_entry_get_type ())
G_DECLARE_FINAL_TYPE (PpPrinterEntry, pp_printer_entry, PP, PRINTER_ENTRY, GtkListBoxRow)

PpPrinterEntry *pp_printer_entry_new  (cups_dest_t printer,
                                       gboolean    is_authorized);

const gchar    *pp_printer_entry_get_name (PpPrinterEntry *self);

const gchar    *pp_printer_entry_get_location (PpPrinterEntry *self);

void            pp_printer_entry_update_jobs_count (PpPrinterEntry *self);

GSList         *pp_printer_entry_get_size_group_widgets (PpPrinterEntry *self);

void            pp_printer_entry_show_jobs_dialog (PpPrinterEntry *self);

void            pp_printer_entry_authenticate_jobs (PpPrinterEntry *self);

void            pp_printer_entry_update (PpPrinterEntry *self,
                                         cups_dest_t     printer,
                                         gboolean        is_authorized);
