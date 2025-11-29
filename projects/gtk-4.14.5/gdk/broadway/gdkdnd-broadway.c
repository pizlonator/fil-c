/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdkdragprivate.h"

#include "gdksurfaceprivate.h"
#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

#include <string.h>

#define GDK_TYPE_BROADWAY_DRAG              (gdk_broadway_drag_get_type ())
#define GDK_BROADWAY_DRAG(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_DRAG, GdkBroadwayDrag))
#define GDK_BROADWAY_DRAG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_DRAG, GdkBroadwayDragClass))
#define GDK_IS_BROADWAY_DRAG(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_DRAG))
#define GDK_IS_BROADWAY_DRAG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_DRAG))
#define GDK_BROADWAY_DRAG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_DRAG, GdkBroadwayDragClass))

#ifdef GTK_COMPILATION
typedef struct _GdkBroadwayDrag GdkBroadwayDrag;
#else
typedef GdkDrag GdkBroadwayDrag;
#endif
typedef struct _GdkBroadwayDragClass GdkBroadwayDragClass;

GType     gdk_broadway_drag_get_type (void);

struct _GdkBroadwayDrag {
  GdkDrag context;
};

struct _GdkBroadwayDragClass
{
  GdkDragClass parent_class;
};

static void gdk_broadway_drag_finalize (GObject *object);

static GList *contexts;

G_DEFINE_TYPE (GdkBroadwayDrag, gdk_broadway_drag, GDK_TYPE_DRAG)

static void
gdk_broadway_drag_init (GdkBroadwayDrag *dragcontext)
{
  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_broadway_drag_finalize (GObject *object)
{
  GdkDrag *context = GDK_DRAG (object);

  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (gdk_broadway_drag_parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDrag *
_gdk_broadway_surface_drag_begin (GdkSurface         *surface,
                                  GdkDevice          *device,
                                  GdkContentProvider *content,
                                  GdkDragAction       actions,
                                  double              dx,
                                  double              dy)
{
  GdkDrag *new_context;

  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (GDK_IS_BROADWAY_SURFACE (surface), NULL);

  new_context = g_object_new (GDK_TYPE_BROADWAY_DRAG,
                              "device", device,
                              "content", content,
                              NULL);

  return new_context;
}

static void
gdk_broadway_drag_class_init (GdkBroadwayDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_broadway_drag_finalize;
}
