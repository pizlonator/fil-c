/* GTK - The GIMP Toolkit
 * Copyright (C) 2016, Red Hat, Inc.
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

#include "gtkprintoperation.h"

G_BEGIN_DECLS

GtkPrintOperationResult gtk_print_operation_portal_run_dialog             (GtkPrintOperation           *op,
                                                                           gboolean                     show_dialog,
                                                                           GtkWindow                   *parent,
                                                                           gboolean                    *do_print);
void                    gtk_print_operation_portal_run_dialog_async       (GtkPrintOperation           *op,
                                                                           gboolean                     show_dialog,
                                                                           GtkWindow                   *parent,
                                                                           GtkPrintOperationPrintFunc   print_cb);
void                    gtk_print_operation_portal_launch_preview         (GtkPrintOperation           *op,
                                                                           cairo_surface_t             *surface,
                                                                           GtkWindow                   *parent,
                                                                           const char                  *filename);

G_END_DECLS

