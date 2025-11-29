/*
 * Copyright © 2020 Red Hat, Inc.
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

#include "gdkmacosdrag-private.h"

#include "gdkmacosdevice-private.h"
#include "gdkmacoscursor-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdragsurface-private.h"
#include "gdkmacospasteboard-private.h"

#include "gdk/gdkdeviceprivate.h"
#include "gdk/gdkeventsprivate.h"
#include <glib/gi18n-lib.h>
#include "gdk/gdkseatprivate.h"
#include "gdk/gdkprivate.h"

#define BIG_STEP 20
#define SMALL_STEP 1
#define ANIM_TIME 500000 /* .5 seconds */

typedef struct
{
  GdkMacosDrag  *drag;
  GdkFrameClock *frame_clock;
  gint64         start_time;
} GdkMacosZoomback;

G_DEFINE_TYPE (GdkMacosDrag, gdk_macos_drag, GDK_TYPE_DRAG)

enum {
  PROP_0,
  PROP_DRAG_SURFACE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static void
gdk_macos_zoomback_destroy (GdkMacosZoomback *zb)
{
  gdk_surface_hide (GDK_SURFACE (zb->drag->drag_surface));
  g_clear_object (&zb->drag);
  g_free (zb);
}

static gboolean
gdk_macos_zoomback_timeout (gpointer data)
{
  GdkMacosZoomback *zb = data;
  GdkFrameClock *frame_clock;
  GdkMacosDrag *drag;
  gint64 current_time;
  double f;
  double t;

  g_assert (zb != NULL);
  g_assert (GDK_IS_MACOS_DRAG (zb->drag));

  drag = zb->drag;
  frame_clock = zb->frame_clock;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);
  f = (current_time - zb->start_time) / (double) ANIM_TIME;
  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  _gdk_macos_surface_move (GDK_MACOS_SURFACE (drag->drag_surface),
                           (drag->last_x - drag->hot_x) +
                           (drag->start_x - drag->last_x) * t,
                           (drag->last_y - drag->hot_y) +
                           (drag->start_y - drag->last_y) * t);
  _gdk_macos_surface_set_opacity (GDK_MACOS_SURFACE (drag->drag_surface), 1.0 - f);

  /* Make sure we're topmost */
  _gdk_macos_surface_show (GDK_MACOS_SURFACE (drag->drag_surface));

  return G_SOURCE_CONTINUE;
}

static GdkSurface *
gdk_macos_drag_get_drag_surface (GdkDrag *drag)
{
  return GDK_SURFACE (GDK_MACOS_DRAG (drag)->drag_surface);
}

static void
gdk_macos_drag_set_hotspot (GdkDrag *drag,
                            int      hot_x,
                            int      hot_y)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  int change_x;
  int change_y;

  g_assert (GDK_IS_MACOS_DRAG (self));

  change_x = hot_x - self->hot_x;
  change_y = hot_y - self->hot_y;

  self->hot_x = hot_x;
  self->hot_y = hot_y;

  if (change_x || change_y)
    _gdk_macos_surface_move (GDK_MACOS_SURFACE (self->drag_surface),
                             GDK_SURFACE (self->drag_surface)->x + change_x,
                             GDK_SURFACE (self->drag_surface)->y + change_y);
}

static void
gdk_macos_drag_drop_done (GdkDrag  *drag,
                          gboolean  success)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  GdkMacosZoomback *zb;
  guint id;

  g_assert (GDK_IS_MACOS_DRAG (self));

  if (success)
    {
      gdk_surface_hide (GDK_SURFACE (self->drag_surface));
      g_object_unref (drag);
      return;
    }

  /* Apple HIG suggests doing a "zoomback" animation of the surface back
   * towards the original position.
   */
  zb = g_new0 (GdkMacosZoomback, 1);
  zb->drag = g_object_ref (self);
  zb->frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self->drag_surface));
  zb->start_time = gdk_frame_clock_get_frame_time (zb->frame_clock);

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_macos_zoomback_timeout,
                           zb,
                           (GDestroyNotify) gdk_macos_zoomback_destroy);
  gdk_source_set_static_name_by_id (id, "[gtk] gdk_macos_zoomback_timeout");
  g_object_unref (drag);
}

static void
gdk_macos_drag_set_cursor (GdkDrag   *drag,
                           GdkCursor *cursor)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;
  NSCursor *nscursor;

  g_assert (GDK_IS_MACOS_DRAG (self));
  g_assert (!cursor || GDK_IS_CURSOR (cursor));

  g_set_object (&self->cursor, cursor);

  nscursor = _gdk_macos_cursor_get_ns_cursor (cursor);

  if (nscursor != NULL)
    [nscursor set];
}

static void
gdk_macos_drag_cancel (GdkDrag             *drag,
                       GdkDragCancelReason  reason)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  if (self->cancelled)
    return;

  self->cancelled = TRUE;
  gdk_drag_drop_done (drag, FALSE);
}

static void
gdk_macos_drag_drop_performed (GdkDrag *drag,
                               guint32  time)
{
  GdkMacosDrag *self = (GdkMacosDrag *)drag;

  g_assert (GDK_IS_MACOS_DRAG (self));

  g_object_ref (self);
  g_signal_emit_by_name (drag, "dnd-finished");
  gdk_drag_drop_done (drag, TRUE);
  g_object_unref (self);
}

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              int              button,
                              GdkDragAction    actions,
                              GdkDragAction   *suggested_action,
                              GdkDragAction   *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
    {
      *suggested_action = GDK_ACTION_ASK;
      *possible_actions = actions;
    }
  else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    {
      if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
        {
          if (actions & GDK_ACTION_LINK)
            {
              *suggested_action = GDK_ACTION_LINK;
              *possible_actions = GDK_ACTION_LINK;
            }
        }
      else if (state & GDK_CONTROL_MASK)
        {
          if (actions & GDK_ACTION_COPY)
            {
              *suggested_action = GDK_ACTION_COPY;
              *possible_actions = GDK_ACTION_COPY;
            }
        }
      else
        {
          if (actions & GDK_ACTION_MOVE)
            {
              *suggested_action = GDK_ACTION_MOVE;
              *possible_actions = GDK_ACTION_MOVE;
            }
        }
    }
  else
    {
      *possible_actions = actions;

      if ((state & (GDK_ALT_MASK)) && (actions & GDK_ACTION_ASK))
        *suggested_action = GDK_ACTION_ASK;
      else if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static void
gdk_macos_drag_finalize (GObject *object)
{
  GdkMacosDrag *self = (GdkMacosDrag *)object;
  GdkMacosDragSurface *drag_surface = g_steal_pointer (&self->drag_surface);

  g_clear_object (&self->cursor);

  G_OBJECT_CLASS (gdk_macos_drag_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (GDK_SURFACE (drag_surface));
}

static void
gdk_macos_drag_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (object);

  switch (prop_id)
    {
    case PROP_DRAG_SURFACE:
      g_value_set_object (value, self->drag_surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_drag_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdkMacosDrag *self = GDK_MACOS_DRAG (object);

  switch (prop_id)
    {
    case PROP_DRAG_SURFACE:
      self->drag_surface = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_drag_class_init (GdkMacosDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_macos_drag_finalize;
  object_class->get_property = gdk_macos_drag_get_property;
  object_class->set_property = gdk_macos_drag_set_property;

  drag_class->get_drag_surface = gdk_macos_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_macos_drag_set_hotspot;
  drag_class->drop_done = gdk_macos_drag_drop_done;
  drag_class->set_cursor = gdk_macos_drag_set_cursor;
  drag_class->cancel = gdk_macos_drag_cancel;
  drag_class->drop_performed = gdk_macos_drag_drop_performed;

  properties [PROP_DRAG_SURFACE] =
    g_param_spec_object ("drag-surface", NULL, NULL,
                         GDK_TYPE_MACOS_DRAG_SURFACE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gdk_macos_drag_init (GdkMacosDrag *self)
{
}

gboolean
_gdk_macos_drag_begin (GdkMacosDrag       *self,
                       GdkContentProvider *content,
                       GdkMacosWindow     *window)
{
  NSArray<NSDraggingItem *> *items;
  NSDraggingSession *session;
  NSPasteboardItem *item;
  NSEvent *nsevent;

  g_return_val_if_fail (GDK_IS_MACOS_DRAG (self), FALSE);
  g_return_val_if_fail (GDK_IS_MACOS_WINDOW (window), FALSE);

  GDK_BEGIN_MACOS_ALLOC_POOL;

  item = [[GdkMacosPasteboardItem alloc] initForDrag:GDK_DRAG (self) withContentProvider:content];
  items = [NSArray arrayWithObject:item];
  nsevent = _gdk_macos_display_get_last_nsevent ();

  session = [[window contentView] beginDraggingSessionWithItems:items
                                                          event:nsevent
                                                         source:window];

  GDK_END_MACOS_ALLOC_POOL;

  _gdk_macos_display_set_drag (GDK_MACOS_DISPLAY (gdk_drag_get_display (GDK_DRAG (self))),
                               [session draggingSequenceNumber],
                               GDK_DRAG (self));

  return TRUE;
}

NSDragOperation
_gdk_macos_drag_operation (GdkMacosDrag *self)
{
  NSDragOperation operation = NSDragOperationNone;
  GdkDragAction actions;

  g_return_val_if_fail (GDK_IS_MACOS_DRAG (self), NSDragOperationNone);

  actions = gdk_drag_get_actions (GDK_DRAG (self));

  if (actions & GDK_ACTION_LINK)
    operation |= NSDragOperationLink;

  if (actions & GDK_ACTION_MOVE)
    operation |= NSDragOperationMove;

  if (actions & GDK_ACTION_COPY)
    operation |= NSDragOperationCopy;

  return operation;
}

GdkDragAction
_gdk_macos_drag_ns_operation_to_action (NSDragOperation operation)
{
  if (operation & NSDragOperationCopy)
    return GDK_ACTION_COPY;
  if (operation & NSDragOperationMove)
    return GDK_ACTION_MOVE;
  if (operation & NSDragOperationLink)
    return GDK_ACTION_LINK;
  return 0;
}

void
_gdk_macos_drag_surface_move (GdkMacosDrag *self,
                              int           x_root,
                              int           y_root)
{
  g_return_if_fail (GDK_IS_MACOS_DRAG (self));

  self->last_x = x_root;
  self->last_y = y_root;

  if (GDK_IS_MACOS_SURFACE (self->drag_surface))
    _gdk_macos_surface_move (GDK_MACOS_SURFACE (self->drag_surface),
                             x_root - self->hot_x,
                             y_root - self->hot_y);
}

void
_gdk_macos_drag_set_start_position (GdkMacosDrag *self,
                                    int           start_x,
                                    int           start_y)
{
  g_return_if_fail (GDK_IS_MACOS_DRAG (self));

  self->start_x = start_x;
  self->start_y = start_y;
}

void
_gdk_macos_drag_set_actions (GdkMacosDrag    *self,
                             GdkModifierType  mods)
{
  GdkDragAction suggested_action;
  GdkDragAction possible_actions;

  g_assert (GDK_IS_MACOS_DRAG (self));

  gdk_drag_get_current_actions (mods,
                                GDK_BUTTON_PRIMARY,
                                gdk_drag_get_actions (GDK_DRAG (self)),
                                &suggested_action,
                                &possible_actions);

  gdk_drag_set_selected_action (GDK_DRAG (self), suggested_action);
  gdk_drag_set_actions (GDK_DRAG (self), possible_actions);
}
