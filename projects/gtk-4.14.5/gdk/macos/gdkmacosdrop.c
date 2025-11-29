/*
 * Copyright © 2021 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkmacosclipboard-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdrag-private.h"
#include "gdkmacosdrop-private.h"

G_DEFINE_TYPE (GdkMacosDrop, gdk_macos_drop, GDK_TYPE_DROP)

static void
gdk_macos_drop_status (GdkDrop       *drop,
                       GdkDragAction  actions,
                       GdkDragAction  preferred)
{
  GdkMacosDrop *self = (GdkMacosDrop *)drop;

  g_assert (GDK_IS_MACOS_DROP (self));

  self->all_actions = actions;
  self->preferred_action = preferred;
}

static void
gdk_macos_drop_read_async (GdkDrop             *drop,
                           GdkContentFormats   *content_formats,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  _gdk_macos_pasteboard_read_async (G_OBJECT (drop),
                                    GDK_MACOS_DROP (drop)->pasteboard,
                                    content_formats,
                                    io_priority,
                                    cancellable,
                                    callback,
                                    user_data);
}

static GInputStream *
gdk_macos_drop_read_finish (GdkDrop       *drop,
                            GAsyncResult  *result,
                            const char   **out_mime_type,
                            GError       **error)
{
  return _gdk_macos_pasteboard_read_finish (G_OBJECT (drop), result, out_mime_type, error);
}

static void
gdk_macos_drop_finish (GdkDrop       *drop,
                       GdkDragAction  action)
{
  g_assert (GDK_IS_MACOS_DROP (drop));

  GDK_MACOS_DROP (drop)->finish_action = action;
}

static void
gdk_macos_drop_finalize (GObject *object)
{
  GdkMacosDrop *self = (GdkMacosDrop *)object;

  if (self->pasteboard)
    {
      [self->pasteboard release];
      self->pasteboard = NULL;
    }

  G_OBJECT_CLASS (gdk_macos_drop_parent_class)->finalize (object);
}

static void
gdk_macos_drop_class_init (GdkMacosDropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);

  object_class->finalize = gdk_macos_drop_finalize;

  drop_class->status = gdk_macos_drop_status;
  drop_class->read_async = gdk_macos_drop_read_async;
  drop_class->read_finish = gdk_macos_drop_read_finish;
  drop_class->finish = gdk_macos_drop_finish;
}

static void
gdk_macos_drop_init (GdkMacosDrop *self)
{
}

void
_gdk_macos_drop_update_actions (GdkMacosDrop       *self,
                                id<NSDraggingInfo>  info)
{
  NSDragOperation op;
  GdkDragAction actions = 0;

  g_assert (GDK_IS_MACOS_DROP (self));

  op = [info draggingSourceOperationMask];

  if (op & NSDragOperationCopy)
    actions |= GDK_ACTION_COPY;

  if (op & NSDragOperationLink)
    actions |= GDK_ACTION_LINK;

  if (op & NSDragOperationMove)
    actions |= GDK_ACTION_MOVE;

  gdk_drop_set_actions (GDK_DROP (self), actions);
}

GdkMacosDrop *
_gdk_macos_drop_new (GdkMacosSurface    *surface,
                     id<NSDraggingInfo>  info)
{
  GdkDrag *drag = NULL;
  GdkContentFormats *content_formats;
  GdkMacosDrop *self;
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (surface), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  display = gdk_surface_get_display (GDK_SURFACE (surface));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  drag = _gdk_macos_display_find_drag (GDK_MACOS_DISPLAY (display), [info draggingSequenceNumber]);

  content_formats = _gdk_macos_pasteboard_load_formats ([info draggingPasteboard]);

  self = g_object_new (GDK_TYPE_MACOS_DROP,
                       "device", device,
                       "drag", drag,
                       "formats", content_formats,
                       "surface", surface,
                       NULL);

  self->pasteboard = [[info draggingPasteboard] retain];

  _gdk_macos_drop_update_actions (self, info);

  gdk_content_formats_unref (content_formats);

  return g_steal_pointer (&self);
}

NSDragOperation
_gdk_macos_drop_operation (GdkMacosDrop *self)
{
  if (self->preferred_action & GDK_ACTION_LINK)
    return NSDragOperationLink;

  if (self->preferred_action & GDK_ACTION_MOVE)
    return NSDragOperationMove;

  if (self->preferred_action & GDK_ACTION_COPY)
    return NSDragOperationCopy;
  
  return NSDragOperationNone;
}
