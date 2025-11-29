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
 

#include <gtkprintercpdb.h>

enum {
  PROP_0,
  PROP_PRINTER_OBJ
};

static void gtk_printer_cpdb_init       (GtkPrinterCpdb      *printer);
static void gtk_printer_cpdb_class_init (GtkPrinterCpdbClass *class);

static GType gtk_printer_cpdb_type = 0;

static void gtk_printer_cpdb_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec);
static void gtk_printer_cpdb_get_property (GObject      *object,
                                           guint         prop_id,
                                           GValue       *value,
                                           GParamSpec   *pspec);

void 
gtk_printer_cpdb_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkPrinterCpdbClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_printer_cpdb_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkPrinterCpdb),
    0,              /* n_preallocs */
    (GInstanceInitFunc) gtk_printer_cpdb_init,
  };

 gtk_printer_cpdb_type = g_type_module_register_type (module,
                                                      GTK_TYPE_PRINTER,
                                                      "GtkPrinterCpdb",
                                                      &object_info, 0);
}

GType
gtk_printer_cpdb_get_type (void)
{
  return gtk_printer_cpdb_type;
}

static void
gtk_printer_cpdb_class_init (GtkPrinterCpdbClass *klass)
{

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_printer_cpdb_set_property;
  object_class->get_property = gtk_printer_cpdb_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
                                   PROP_PRINTER_OBJ,
                                   g_param_spec_pointer ("printer-obj", 
                                                         NULL, 
                                                         NULL,
                                                         G_PARAM_READWRITE));
}

static void
gtk_printer_cpdb_init (GtkPrinterCpdb *self)
{
}

static void
gtk_printer_cpdb_set_property (GObject         *object,
                               guint            prop_id,
                               const GValue    *value,
                               GParamSpec      *pspec)
{
  GtkPrinterCpdb *printer_cpdb = GTK_PRINTER_CPDB (object);
  switch (prop_id)
    {
    case PROP_PRINTER_OBJ:
      printer_cpdb->printer_obj = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_cpdb_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkPrinterCpdb *printer_cpdb = GTK_PRINTER_CPDB (object);
  switch (prop_id)
    {
    case PROP_PRINTER_OBJ:
      g_value_set_pointer (value, printer_cpdb->printer_obj);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

cpdb_printer_obj_t *
gtk_printer_cpdb_get_printer_obj (GtkPrinterCpdb *cpdb_printer)
{
  return cpdb_printer->printer_obj;
}

void
gtk_printer_cpdb_set_printer_obj (GtkPrinterCpdb *cpdb_printer,
                                  cpdb_printer_obj_t *printer_obj)
{
  cpdb_printer->printer_obj = printer_obj;
}
