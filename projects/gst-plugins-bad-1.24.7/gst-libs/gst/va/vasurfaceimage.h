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
#include <gst/video/video.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

G_BEGIN_DECLS

/* surfaces */
gboolean              va_create_surfaces                  (GstVaDisplay * display,
                                                           guint rt_format, guint fourcc,
                                                           guint width, guint height,
                                                           gint usage_hint,
                                                           guint64 * modifiers,
                                                           guint num_modifiers,
                                                           VADRMPRIMESurfaceDescriptor * desc,
                                                           VASurfaceID * surfaces,
                                                           guint num_surfaces);
gboolean              va_destroy_surfaces                 (GstVaDisplay * display,
                                                           VASurfaceID * surfaces,
                                                           gint num_surfaces);
gboolean              va_export_surface_to_dmabuf         (GstVaDisplay * display,
                                                           VASurfaceID surface,
                                                           guint32 flags,
                                                           VADRMPRIMESurfaceDescriptor * desc);

GST_VA_API
gboolean              va_sync_surface                     (GstVaDisplay * display,
                                                           VASurfaceID surface);
GST_VA_API
gboolean              va_check_surface                    (GstVaDisplay * display,
                                                           VASurfaceID surface);

gboolean              va_copy_surface                     (GstVaDisplay * display,
                                                           VASurfaceID dst,
                                                           VASurfaceID src);

GST_VA_API
guint                 va_get_surface_usage_hint           (GstVaDisplay * display,
                                                           VAEntrypoint entrypoint,
                                                           GstPadDirection dir,
                                                           gboolean is_dma);

/* images */
gboolean              va_create_image                     (GstVaDisplay * display,
                                                           GstVideoFormat format,
                                                           gint width, gint height,
                                                           VAImage * image);
gboolean              va_destroy_image                    (GstVaDisplay * display,
                                                           VAImageID image_id);
gboolean              va_get_image                        (GstVaDisplay * display,
                                                           VASurfaceID surface,
                                                           VAImage * image);
gboolean              va_get_derive_image                 (GstVaDisplay * display,
                                                           VASurfaceID surface,
                                                           VAImage * image);
gboolean              va_put_image                        (GstVaDisplay * display,
                                                           VASurfaceID surface,
                                                           VAImage * image);
gboolean              va_ensure_image                     (GstVaDisplay * display,
                                                           VASurfaceID surface,
                                                           GstVideoInfo * info,
                                                           VAImage * image,
                                                           gboolean derived);

/* mapping */
GST_VA_API
gboolean              va_map_buffer                       (GstVaDisplay * display,
                                                           VABufferID buffer,
                                                           GstMapFlags flags,
                                                           gpointer * data);
GST_VA_API
gboolean              va_unmap_buffer                     (GstVaDisplay * display,
                                                           VABufferID buffer);

G_END_DECLS
