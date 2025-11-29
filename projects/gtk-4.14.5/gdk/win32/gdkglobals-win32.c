/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gdktypes.h"
#include "gdkprivate-win32.h"

GdkDisplay	 *_gdk_display = NULL;
GdkDeviceManagerWin32 *_gdk_device_manager = NULL;

int		  _gdk_input_ignore_core;

HKL		  _gdk_input_locale;
gboolean	  _gdk_input_locale_is_ime = FALSE;

GdkWin32ModalOpKind	  _modal_operation_in_progress = GDK_WIN32_MODAL_OP_NONE;
HWND              _modal_move_resize_window = NULL;

/* The singleton clipdrop object pointer */
GdkWin32Clipdrop *_win32_clipdrop = NULL;

GThread          *_win32_main_thread = NULL;
