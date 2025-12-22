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
#include <va/va.h>

G_BEGIN_DECLS

gboolean              gst_va_caps_from_profiles           (GstVaDisplay * display,
                                                           GArray * profiles,
                                                           VAEntrypoint entrypoint,
                                                           GstCaps ** codedcaps,
                                                           GstCaps ** rawcaps);

VASurfaceAttrib *     gst_va_get_surface_attribs          (GstVaDisplay * display,
                                                           VAConfigID config,
                                                           guint * attrib_count);

GstCaps *             gst_va_create_dma_caps              (GstVaDisplay * display,
                                                           VAEntrypoint entrypoint,
                                                           GArray * formats,
                                                           gint min_width,
                                                           gint max_width,
                                                           gint min_height,
                                                           gint max_height);
GstCaps *             gst_va_create_raw_caps_from_config  (GstVaDisplay * display,
                                                           VAConfigID config);
GstCaps *             gst_va_create_coded_caps            (GstVaDisplay * display,
                                                           VAProfile profile,
                                                           VAEntrypoint entrypoint,
                                                           guint32 * rt_formats_ptr);
gboolean              gst_va_video_info_from_caps         (GstVideoInfo * info,
                                                           guint64 * modifier,
                                                           GstCaps * caps);
GstCaps *             gst_va_video_info_to_dma_caps       (GstVideoInfo * info,
                                                           guint64 modifier);

gboolean              gst_caps_set_format_array           (GstCaps * caps,
                                                           GArray * formats);

gboolean              gst_caps_is_dmabuf                  (GstCaps * caps);
gboolean              gst_caps_is_vamemory                (GstCaps * caps);
gboolean              gst_caps_is_raw                     (GstCaps * caps);

G_END_DECLS

