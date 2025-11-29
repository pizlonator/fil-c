/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010, Red Hat, Inc
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

#include "gdkdrag.h"

G_BEGIN_DECLS


#define GDK_DRAG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAG, GdkDragClass))
#define GDK_IS_DRAG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAG))
#define GDK_DRAG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAG, GdkDragClass))

typedef struct _GdkDragClass GdkDragClass;


struct _GdkDragClass {
  GObjectClass parent_class;

  GdkSurface*  (*get_drag_surface) (GdkDrag *drag);
  void        (*set_hotspot)   (GdkDrag  *drag,
                                int              hot_x,
                                int              hot_y);
  void        (*drop_done)     (GdkDrag   *drag,
                                gboolean          success);

  void        (*set_cursor)     (GdkDrag  *drag,
                                 GdkCursor       *cursor);
  void        (*cancel)         (GdkDrag      *drag,
                                 GdkDragCancelReason  reason);
  void        (*drop_performed) (GdkDrag  *drag,
                                 guint32          time);
  void        (*dnd_finished)   (GdkDrag  *drag);

  gboolean    (*handle_event)   (GdkDrag  *drag,
                                 GdkEvent  *event);
};

struct _GdkDrag {
  GObject parent_instance;
};

void     gdk_drag_set_cursor          (GdkDrag        *drag,
                                       GdkCursor      *cursor);
void     gdk_drag_set_actions         (GdkDrag        *drag,
                                       GdkDragAction   actions);
void     gdk_drag_set_selected_action (GdkDrag        *drag,
                                       GdkDragAction   action);

void     gdk_drag_cancel              (GdkDrag        *drag,
                                       GdkDragCancelReason  reason);
gboolean gdk_drag_handle_source_event (GdkEvent       *event);
GdkCursor * gdk_drag_get_cursor       (GdkDrag        *drag,
                                       GdkDragAction   action);

void     gdk_drag_write_async         (GdkDrag             *drag,
                                       const char          *mime_type,
                                       GOutputStream       *stream,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);
gboolean gdk_drag_write_finish        (GdkDrag             *drag,
                                       GAsyncResult        *result,
                                       GError             **error);


G_END_DECLS

