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

#pragma once

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#define GDK_BEGIN_MACOS_ALLOC_POOL NSAutoreleasePool *_pool = [[NSAutoreleasePool alloc] init]
#define GDK_END_MACOS_ALLOC_POOL   [_pool release]

static inline gboolean
queue_contains (GQueue *queue,
                GList  *link_)
{
  return queue->head == link_ || link_->prev || link_->next;
}

struct _GdkPoint
{
  int x;
  int y;
};
typedef struct _GdkPoint GdkPoint;

static inline NSPoint
convert_nspoint_from_screen (NSWindow *window,
                             NSPoint   point)
{
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_15_AND_LATER
  return [window convertPointFromScreen:point];
#else
  /* Apple documentation claims that convertPointFromScreen is available
   * on 10.12+. However, that doesn't seem to be the case when using it.
   * Instead, we'll just use it on modern 10.15 systems and fallback to
   * converting using rects on older systems.
   */
  return [window convertRectFromScreen:NSMakeRect (point.x, point.y, 0, 0)].origin;
#endif
}

static inline NSPoint
convert_nspoint_to_screen (NSWindow *window,
                           NSPoint   point)
{
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_15_AND_LATER
  return [window convertPointToScreen:point];
#else
  /* Apple documentation claims that convertPointToScreen is available
   * on 10.12+. However, that doesn't seem to be the case when using it.
   * Instead, we'll just use it on modern 10.15 systems and fallback to
   * converting using rects on older systems.
   */
  return [window convertRectToScreen:NSMakeRect (point.x, point.y, 0, 0)].origin;
#endif
}

