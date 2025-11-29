/* GtkPrinterCpdb
 * Copyright (C) 2022, 2023 TinyTrebuchet <tinytrebuchet@protonmail.com>
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

#pragma once

#include <glib-object.h>
#include <cpdb/cpdb-frontend.h>
#include <gtk/print/gtkprinterprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_CPDB           (gtk_printer_cpdb_get_type ())
G_DECLARE_FINAL_TYPE                    (GtkPrinterCpdb, gtk_printer_cpdb, GTK, PRINTER_CPDB, GtkPrinter)

struct _GtkPrinterCpdb
{
  GtkPrinter parent_instance;
  cpdb_printer_obj_t *printer_obj;
};

void                  gtk_printer_cpdb_register_type          (GTypeModule          *module);

cpdb_printer_obj_t   *gtk_printer_cpdb_get_printer_obj        (GtkPrinterCpdb       *cpdb_printer);

void                  gtk_printer_cpdb_set_printer_obj        (GtkPrinterCpdb       *cpdb_printer,
                                                               cpdb_printer_obj_t   *printer_obj);

G_END_DECLS
