/* GTK - The GIMP Toolkit
 * gtkprintoperation.h: Print Operation
 * Copyright (C) 2006, Red Hat, Inc.
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

#include <gtk/gtk.h>
#include "gtkunixprint.h"
#include "gtkprinteroptionsetprivate.h"

G_BEGIN_DECLS

#define GTK_PRINTER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER, GtkPrinterClass))
#define GTK_IS_PRINTER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER))
#define GTK_PRINTER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER, GtkPrinterClass))

typedef struct _GtkPrinterClass     GtkPrinterClass;
typedef struct _GtkPrinterPrivate   GtkPrinterPrivate;

struct _GtkPrinter
{
  GObject parent_instance;
};

struct _GtkPrinterClass
{
  GObjectClass parent_class;

  void (*details_acquired) (GtkPrinter *printer,
                            gboolean    success);
};

GtkPrinterOptionSet *_gtk_printer_get_options               (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup,
							     GtkPrintCapabilities capabilities);
gboolean             _gtk_printer_mark_conflicts            (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options);
void                 _gtk_printer_get_settings_from_options (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options,
							     GtkPrintSettings    *settings);
void                 _gtk_printer_prepare_for_print         (GtkPrinter          *printer,
							     GtkPrintJob         *print_job,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup);
cairo_surface_t *    _gtk_printer_create_cairo_surface      (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     double               width,
							     double               height,
							     GIOChannel          *cache_io);
GHashTable *         _gtk_printer_get_custom_widgets        (GtkPrinter          *printer);

/* GtkPrintJob private methods: */
GDK_AVAILABLE_IN_ALL
void gtk_print_job_set_status (GtkPrintJob   *job,
			       GtkPrintStatus status);

GtkPrinter *         gtk_printer_find (const char *name);

G_END_DECLS
