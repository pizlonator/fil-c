/* edgesnapping.h
 *
 * Copyright © 2020 Red Hat, Inc.
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gdktypes.h"
#include "gdkmacosutils-private.h"

G_BEGIN_DECLS

typedef struct
{
  GdkRectangle geometry;
  GdkRectangle workarea;
  GdkPoint     last_pointer_position;
  GdkPoint     pointer_offset_in_window;
} EdgeSnapping;

void _edge_snapping_init        (EdgeSnapping       *self,
                                 const GdkRectangle *geometry,
                                 const GdkRectangle *workarea,
                                 const GdkPoint     *pointer_position,
                                 const GdkRectangle *window);
void _edge_snapping_motion      (EdgeSnapping       *self,
                                 const GdkPoint     *pointer_position,
                                 GdkRectangle       *window);
void _edge_snapping_set_monitor (EdgeSnapping       *self,
                                 const GdkRectangle *geometry,
                                 const GdkRectangle *workarea);

G_END_DECLS

