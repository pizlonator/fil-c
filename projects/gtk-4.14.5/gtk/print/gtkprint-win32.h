/* GTK - The GIMP Toolkit
 * gtkprint-win32.h: Win32 Print utils
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

#include "win32/gdkwin32.h"

G_BEGIN_DECLS

#ifndef START_PAGE_GENERAL
#define START_PAGE_GENERAL 0xffffffff
#endif

#ifndef PD_RESULT_CANCEL
#define PD_RESULT_CANCEL  0
#define PD_RESULT_PRINT  1
#define PD_RESULT_APPLY  2
#endif

#ifndef PD_NOCURRENTPAGE
#define PD_NOCURRENTPAGE  0x00800000
#endif

#ifndef PD_CURRENTPAGE
#define PD_CURRENTPAGE  0x00400000
#endif

typedef struct {
  char *driver;
  char *device;
  char *output;
  int flags;
} GtkPrintWin32Devnames;

GDK_AVAILABLE_IN_ALL
void gtk_print_win32_devnames_free (GtkPrintWin32Devnames *devnames);
GDK_AVAILABLE_IN_ALL
GtkPrintWin32Devnames *gtk_print_win32_devnames_from_win32 (HGLOBAL global);
GDK_AVAILABLE_IN_ALL
GtkPrintWin32Devnames *gtk_print_win32_devnames_from_printer_name (const char *printer);
GDK_AVAILABLE_IN_ALL
HGLOBAL gtk_print_win32_devnames_to_win32 (const GtkPrintWin32Devnames *devnames);
GDK_AVAILABLE_IN_ALL
HGLOBAL gtk_print_win32_devnames_to_win32_from_printer_name (const char *printer);

G_END_DECLS

