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

#include "gdkmemorytexture.h"

#include "gdktextureprivate.h"

G_BEGIN_DECLS

#define GDK_MEMORY_GDK_PIXBUF_OPAQUE GDK_MEMORY_R8G8B8
#define GDK_MEMORY_GDK_PIXBUF_ALPHA GDK_MEMORY_R8G8B8A8

GdkMemoryTexture *      gdk_memory_texture_from_texture     (GdkTexture        *texture,
                                                             GdkMemoryFormat    format);
GdkTexture *            gdk_memory_texture_new_subtexture   (GdkMemoryTexture  *texture,
                                                             int                x,
                                                             int                y,
                                                             int                width,
                                                             int                height);

GBytes *                gdk_memory_texture_get_bytes        (GdkMemoryTexture  *self,
                                                             gsize             *out_stride);


G_END_DECLS

