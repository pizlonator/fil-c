/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
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

G_BEGIN_DECLS

#define GST_TYPE_VA_DISPLAY_DRM            (gst_va_display_drm_get_type())
#define GST_VA_DISPLAY_DRM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_DISPLAY_DRM, GstVaDisplayDrm))
#define GST_VA_DISPLAY_DRM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VA_DISPLAY_DRM, GstVaDisplayDrmClass))
#define GST_IS_VA_DISPLAY_DRM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_DISPLAY_DRM))
#define GST_IS_VA_DISPLAY_DRM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VA_DISPLAY_DRM))
#define GST_VA_DISPLAY_DRM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VA_DISPLAY_DRM, GstVaDisplayDrmClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVaDisplayDrm, gst_object_unref)

GST_VA_API
GType                 gst_va_display_drm_get_type         (void);
GST_VA_API
GstVaDisplay *        gst_va_display_drm_new_from_path    (const gchar * path);

G_END_DECLS
