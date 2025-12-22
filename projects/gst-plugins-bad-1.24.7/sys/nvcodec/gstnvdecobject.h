/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
#include <gst/video/video.h>
#include <gst/cuda/gstcuda.h>
#include "gstcuvidloader.h"

G_BEGIN_DECLS

#define GST_TYPE_NV_DEC_OBJECT (gst_nv_dec_object_get_type())
G_DECLARE_FINAL_TYPE (GstNvDecObject,
    gst_nv_dec_object, GST, NV_DEC_OBJECT, GstObject);

#define GST_TYPE_NV_DEC_SURFACE (gst_nv_dec_surface_get_type())
typedef struct _GstNvDecSurface GstNvDecSurface;

struct _GstNvDecSurface
{
  GstMiniObject parent;

  GstNvDecObject *object;

  gint index;
  gint decode_frame_index;

  CUdeviceptr devptr;
  guint pitch;

  guint seq_num;
};

GstNvDecObject * gst_nv_dec_object_new (GstCudaContext * context,
                                        CUVIDDECODECREATEINFO * create_info,
                                        const GstVideoInfo * video_info,
                                        gboolean alloc_aux_frame);

gboolean         gst_nv_dec_object_reconfigure (GstNvDecObject * object,
                                                CUVIDRECONFIGUREDECODERINFO * reconfigure_info,
                                                const GstVideoInfo * video_info,
                                                gboolean alloc_aux_frame);

void             gst_nv_dec_object_set_flushing (GstNvDecObject * object,
                                                 gboolean flushing);

GstFlowReturn    gst_nv_dec_object_acquire_surface (GstNvDecObject * object,
                                                    GstNvDecSurface ** surface);

gboolean         gst_nv_dec_object_decode (GstNvDecObject * object,
                                           CUVIDPICPARAMS * params);

GstFlowReturn    gst_nv_dec_object_map_surface (GstNvDecObject * object,
                                                GstNvDecSurface * surface,
                                                GstCudaStream * stream);

gboolean         gst_nv_dec_object_unmap_surface (GstNvDecObject * object,
                                                  GstNvDecSurface * surface);

GstFlowReturn    gst_nv_dec_object_export_surface (GstNvDecObject * object,
                                                   GstNvDecSurface * surface,
                                                   GstCudaStream * stream,
                                                   GstMemory ** memory);

guint            gst_nv_dec_object_get_num_free_surfaces (GstNvDecObject * object);

GType gst_nv_dec_surface_get_type (void);

static inline GstNvDecSurface *
gst_nv_dec_surface_ref (GstNvDecSurface * surface)
{
  return (GstNvDecSurface *)
      gst_mini_object_ref (GST_MINI_OBJECT_CAST (surface));
}

static inline void
gst_nv_dec_surface_unref (GstNvDecSurface * surface)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (surface));
}

G_END_DECLS

