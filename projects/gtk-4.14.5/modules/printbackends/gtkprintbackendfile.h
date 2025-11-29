/* GTK - The GIMP Toolkit
 * gtkprintbackendpdf.h: Default implementation of GtkPrintBackend
 * for printing to a file
 * Copyright (C) 2003, Red Hat, Inc.
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
#include "gtk/print/gtkprintbackendprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINT_BACKEND_FILE    (gtk_print_backend_file_get_type ())
#define GTK_PRINT_BACKEND_FILE(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_BACKEND_FILE, GtkPrintBackendFile))
#define GTK_IS_PRINT_BACKEND_FILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_BACKEND_FILE))

typedef struct _GtkPrintBackendFile    GtkPrintBackendFile;

GtkPrintBackend *gtk_print_backend_file_new      (void);
GType            gtk_print_backend_file_get_type (void) G_GNUC_CONST;

G_END_DECLS
