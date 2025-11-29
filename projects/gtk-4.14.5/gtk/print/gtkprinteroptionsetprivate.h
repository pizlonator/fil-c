/* GTK - The GIMP Toolkit
 * gtkprinteroptionset.h: printer option set
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

/* This is a "semi-private" header; it is meant only for
 * alternate GtkPrintDialog backend modules; no stability guarantees
 * are made at this point
 */
#ifndef GTK_PRINT_BACKEND_ENABLE_UNSUPPORTED
#error "GtkPrintBackend is not supported API for general use"
#endif

#include <glib-object.h>
#include <gdk/gdk.h>
#include "gtkprinteroptionprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_OPTION_SET             (gtk_printer_option_set_get_type ())
#define GTK_PRINTER_OPTION_SET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_OPTION_SET, GtkPrinterOptionSet))
#define GTK_IS_PRINTER_OPTION_SET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_OPTION_SET))

typedef struct _GtkPrinterOptionSet       GtkPrinterOptionSet;
typedef struct _GtkPrinterOptionSetClass  GtkPrinterOptionSetClass;

struct _GtkPrinterOptionSet
{
  GObject parent_instance;

  /*< private >*/
  GPtrArray *array;
  GHashTable *hash;
};

struct _GtkPrinterOptionSetClass
{
  GObjectClass parent_class;

  void (*changed) (GtkPrinterOptionSet *option);


  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

typedef void (*GtkPrinterOptionSetFunc) (GtkPrinterOption  *option,
					 gpointer           user_data);


GDK_AVAILABLE_IN_ALL
GType   gtk_printer_option_set_get_type       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkPrinterOptionSet *gtk_printer_option_set_new              (void);
GDK_AVAILABLE_IN_ALL
void                 gtk_printer_option_set_add              (GtkPrinterOptionSet     *set,
							      GtkPrinterOption        *option);
GDK_AVAILABLE_IN_ALL
void                 gtk_printer_option_set_remove           (GtkPrinterOptionSet     *set,
							      GtkPrinterOption        *option);
GDK_AVAILABLE_IN_ALL
GtkPrinterOption *   gtk_printer_option_set_lookup           (GtkPrinterOptionSet     *set,
							      const char              *name);
GDK_AVAILABLE_IN_ALL
void                 gtk_printer_option_set_foreach          (GtkPrinterOptionSet     *set,
							      GtkPrinterOptionSetFunc  func,
							      gpointer                 user_data);
GDK_AVAILABLE_IN_ALL
void                 gtk_printer_option_set_clear_conflicts  (GtkPrinterOptionSet     *set);
GDK_AVAILABLE_IN_ALL
GList *              gtk_printer_option_set_get_groups       (GtkPrinterOptionSet     *set);
GDK_AVAILABLE_IN_ALL
void                 gtk_printer_option_set_foreach_in_group (GtkPrinterOptionSet     *set,
							      const char              *group,
							      GtkPrinterOptionSetFunc  func,
							      gpointer                 user_data);

G_END_DECLS

