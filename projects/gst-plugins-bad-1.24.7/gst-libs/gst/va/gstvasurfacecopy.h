/* GStreamer
 * Copyright (C) 2021 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gst/va/gstva.h>
#include <gst/va/va_fwd.h>
#include <gst/video/video.h>
#include <va/va.h>

G_BEGIN_DECLS

/**
 * GstVaSurfaceCopy:
 *
 * Opaque object helper for copying surfaces.
 *
 * It's purpose is to avoid circular dependencies.
 */
typedef struct _GstVaSurfaceCopy GstVaSurfaceCopy;

GstVaSurfaceCopy *    gst_va_surface_copy_new             (GstVaDisplay * display,
                                                           GstVideoInfo * vinfo);
void                  gst_va_surface_copy_free            (GstVaSurfaceCopy * self);
gboolean              gst_va_surface_copy                 (GstVaSurfaceCopy * self,
                                                           VASurfaceID dst,
                                                           VASurfaceID src);

G_END_DECLS
