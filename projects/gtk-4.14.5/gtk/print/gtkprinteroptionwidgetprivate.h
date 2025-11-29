/* GtkPrinterOptionWidget 
 * Copyright (C) 2006 John (J5) Palmieri <johnp@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <gtk/gtk.h>
#include "gtkprinteroptionprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_OPTION_WIDGET                  (gtk_printer_option_widget_get_type ())
#define GTK_PRINTER_OPTION_WIDGET(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_OPTION_WIDGET, GtkPrinterOptionWidget))
#define GTK_PRINTER_OPTION_WIDGET_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER_OPTION_WIDGET, GtkPrinterOptionWidgetClass))
#define GTK_IS_PRINTER_OPTION_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_OPTION_WIDGET))
#define GTK_IS_PRINTER_OPTION_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER_OPTION_WIDGET))
#define GTK_PRINTER_OPTION_WIDGET_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER_OPTION_WIDGET, GtkPrinterOptionWidgetClass))


typedef struct _GtkPrinterOptionWidget         GtkPrinterOptionWidget;
typedef struct _GtkPrinterOptionWidgetClass    GtkPrinterOptionWidgetClass;
typedef struct GtkPrinterOptionWidgetPrivate   GtkPrinterOptionWidgetPrivate;

struct _GtkPrinterOptionWidget
{
  GtkBox parent_instance;

  GtkPrinterOptionWidgetPrivate *priv;
};

struct _GtkPrinterOptionWidgetClass
{
  GtkBoxClass parent_class;

  void (*changed) (GtkPrinterOptionWidget *widget);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_ALL
GType	     gtk_printer_option_widget_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget   *gtk_printer_option_widget_new                (GtkPrinterOption       *source);
GDK_AVAILABLE_IN_ALL
void         gtk_printer_option_widget_set_source         (GtkPrinterOptionWidget *setting,
		 					   GtkPrinterOption       *source);
GDK_AVAILABLE_IN_ALL
gboolean     gtk_printer_option_widget_has_external_label (GtkPrinterOptionWidget *setting);
GDK_AVAILABLE_IN_ALL
GtkWidget   *gtk_printer_option_widget_get_external_label (GtkPrinterOptionWidget *setting);
GDK_AVAILABLE_IN_ALL
const char *gtk_printer_option_widget_get_value          (GtkPrinterOptionWidget *setting);

G_END_DECLS

