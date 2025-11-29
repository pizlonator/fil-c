/* gskoffloadprivate.h
 *
 * Copyright 2023 Red Hat, Inc.
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
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gdk/gdk.h>
#include "gdk/gdksubsurfaceprivate.h"
#include "gskrendernode.h"

typedef struct _GskOffload GskOffload;

typedef struct
{
  GdkSubsurface *subsurface;
  GdkTexture *texture;
  GdkSubsurface *place_above;
  graphene_rect_t dest;
  graphene_rect_t source;

  guint was_offloaded : 1;
  guint can_offload   : 1;
  guint is_offloaded  : 1;

  guint was_above     : 1;
  guint can_raise     : 1;
  guint is_above      : 1;
} GskOffloadInfo;

GskOffload *        gsk_offload_new                      (GdkSurface       *surface,
                                                          GskRenderNode    *root,
                                                          cairo_region_t   *diff);
void                gsk_offload_free                     (GskOffload       *self);

GskOffloadInfo    * gsk_offload_get_subsurface_info      (GskOffload       *self,
                                                          GdkSubsurface    *subsurface);
