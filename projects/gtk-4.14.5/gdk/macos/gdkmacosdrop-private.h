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

#pragma once

#include <AppKit/AppKit.h>

#include "gdkdropprivate.h"

#include "gdkmacossurface-private.h"

G_BEGIN_DECLS

#define GDK_TYPE_MACOS_DROP            (gdk_macos_drop_get_type ())
#define GDK_MACOS_DROP(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MACOS_DROP, GdkMacosDrop))
#define GDK_MACOS_DROP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MACOS_DROP, GdkMacosDropClass))
#define GDK_IS_MACOS_DROP(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MACOS_DROP))
#define GDK_IS_MACOS_DROP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MACOS_DROP))
#define GDK_MACOS_DROP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MACOS_DROP, GdkMacosDropClass))

typedef struct _GdkMacosDrop GdkMacosDrop;
typedef struct _GdkMacosDropClass GdkMacosDropClass;

struct _GdkMacosDrop
{
  GdkDrop parent_instance;

  NSPasteboard *pasteboard;

  GdkDragAction all_actions;
  GdkDragAction preferred_action;
  GdkDragAction finish_action;
};

struct _GdkMacosDropClass
{
  GdkDropClass parent_class;
};

GType             gdk_macos_drop_get_type       (void) G_GNUC_CONST;
GdkMacosDrop    *_gdk_macos_drop_new            (GdkMacosSurface    *surface,
                                                 id<NSDraggingInfo>  info);
NSDragOperation  _gdk_macos_drop_operation      (GdkMacosDrop       *self);
void             _gdk_macos_drop_update_actions (GdkMacosDrop       *self,
                                                 id<NSDraggingInfo>  info);

G_END_DECLS

