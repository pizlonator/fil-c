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

#include <gst/gst.h>

#include <gst/va/gstva.h>

typedef struct _GstVaBufferImporter GstVaBufferImporter;

typedef GstBufferPool *(*GstVaBufferImporterGetSinkPool) (GstElement * element, gpointer data);

struct _GstVaBufferImporter
{
  GstElement *element;
  GstDebugCategory *debug_category;

  GstVaDisplay *display;
  VAEntrypoint entrypoint;

  union {
    GstVideoInfo *in_info;
    GstVideoInfoDmaDrm *in_drm_info;
  };
  GstVideoInfo *sinkpad_info;

  gpointer pool_data;
  GstVaBufferImporterGetSinkPool get_sinkpad_pool;
};

GstFlowReturn         gst_va_buffer_importer_import       (GstVaBufferImporter * base,
                                                           GstBuffer * inbuf,
                                                           GstBuffer ** outbuf);

gboolean              gst_va_base_convert_caps_to_va      (GstCaps * caps);
