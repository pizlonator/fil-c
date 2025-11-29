/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-win32.c
 *
 * Copyright 2010 Hans Breuer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkwin32display.h"
#include "gdkwin32displaymanager.h"
#include "gdkprivate-win32.h"

#include "gdkdisplaymanagerprivate.h"

struct _GdkWin32DisplayManager
{
  GdkDisplayManager parent_instance;
};

struct _GdkWin32DisplayManagerClass
{
  GdkDisplayManagerClass parent_instance;
};

G_DEFINE_TYPE (GdkWin32DisplayManager, gdk_win32_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static void
gdk_win32_display_manager_init (GdkWin32DisplayManager *manager)
{
}

static void
gdk_win32_display_manager_finalize (GObject *object)
{
  g_error ("A GdkWin32DisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_win32_display_manager_parent_class)->finalize (object);
}

static void
gdk_win32_display_manager_class_init (GdkWin32DisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_win32_display_manager_finalize;
}
