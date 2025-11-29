/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include "gdkdrop.h"

G_BEGIN_DECLS


#define GDK_DROP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DROP, GdkDropClass))
#define GDK_IS_DROP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DROP))
#define GDK_DROP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DROP, GdkDropClass))

typedef struct _GdkDropClass GdkDropClass;


struct _GdkDrop {
  GObject parent_instance;
};

struct _GdkDropClass {
  GObjectClass parent_class;

  void                  (* status)                              (GdkDrop                *self,
                                                                 GdkDragAction           actions,
                                                                 GdkDragAction           preferred);
  void                  (* finish)                              (GdkDrop                *self,
                                                                 GdkDragAction           action);

  void                  (* read_async)                          (GdkDrop                *self,
                                                                 GdkContentFormats      *formats,
                                                                 int                     io_priority,
                                                                 GCancellable           *cancellable,
                                                                 GAsyncReadyCallback     callback,
                                                                 gpointer                user_data);
  GInputStream *        (* read_finish)                         (GdkDrop                *self,
                                                                 GAsyncResult           *result,
                                                                 const char            **out_mime_type,
                                                                 GError                **error);
};

void                    gdk_drop_set_actions                    (GdkDrop                *self,
                                                                 GdkDragAction           actions);

void                    gdk_drop_emit_enter_event               (GdkDrop                *self,
                                                                 gboolean                dont_queue,
                                                                 double                  x,
                                                                 double                  y,
                                                                 guint32                 time);
void                    gdk_drop_emit_motion_event              (GdkDrop                *self,
                                                                 gboolean                dont_queue,
                                                                 double                  x,
                                                                 double                  y,
                                                                 guint32                 time);
void                    gdk_drop_emit_leave_event               (GdkDrop                *self,
                                                                 gboolean                dont_queue,
                                                                 guint32                 time);
void                    gdk_drop_emit_drop_event                (GdkDrop                *self,
                                                                 gboolean                dont_queue,
                                                                 double                  x,
                                                                 double                  y,
                                                                 guint32                 time);

G_END_DECLS

