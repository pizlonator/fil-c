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

#include <gst/d3d11/gstd3d11.h>
#include <gst/video/video.h>
#include <gst/base/base.h>
#include "gstdwrite-utils.h"
#include "gstdwrite-enums.h"

G_BEGIN_DECLS

enum class GstDWriteBlendMode
{
  NOT_SUPPORTED,

  /* attach meta with d3d11 texture buffer. */
  ATTACH_TEXTURE,

  /* attach meta with bitmap buffer */
  ATTACH_BITMAP,

  /* software blending */
  SW_BLEND,

  /* 1) renders text on BGRA
   * 2) blends */
  BLEND,

  /* 1) convert texture to BGRA
   * 2) render text on another BGRA texture
   * 3) blends two textures
   * 3) converts back to original format */
  CONVERT,

  /* 1) converts texture to RGBA64_LE
   * 2) renders text on BGRA texture
   * 3) blends two textures
   * 3) converts back original format */
  CONVERT_64,
};

#define GST_TYPE_DWRITE_OVERLAY_OBJECT (gst_dwrite_overlay_object_get_type())
G_DECLARE_FINAL_TYPE (GstDWriteOverlayObject,
    gst_dwrite_overlay_object, GST, DWRITE_OVERLAY_OBJECT, GstObject);

GstDWriteOverlayObject * gst_dwrite_overlay_object_new (void);

gboolean  gst_dwrite_overlay_object_start (GstDWriteOverlayObject * object,
                                           IDWriteFactory * dwrite_factory);

gboolean  gst_dwrite_overlay_object_stop  (GstDWriteOverlayObject * object);

void      gst_dwrite_overlay_object_set_context (GstDWriteOverlayObject * object,
                                                 GstElement * elem,
                                                 GstContext * context);

gboolean  gst_dwrite_overlay_object_handle_query (GstDWriteOverlayObject * object,
                                                  GstElement * elem,
                                                  GstQuery * query);

gboolean  gst_dwrite_overlay_object_decide_allocation (GstDWriteOverlayObject * object,
                                                       GstElement * elem,
                                                       GstQuery * query);

gboolean  gst_dwrite_overlay_object_propose_allocation (GstDWriteOverlayObject * object,
                                                        GstElement * elem,
                                                        GstQuery * query);

gboolean  gst_dwrite_overlay_object_set_caps (GstDWriteOverlayObject * object,
                                              GstElement * elem,
                                              GstCaps * in_caps,
                                              GstCaps * out_caps,
                                              GstVideoInfo * info,
                                              GstDWriteBlendMode * selected_mode);

gboolean  gst_dwrite_overlay_object_update_device (GstDWriteOverlayObject * object,
                                                   GstBuffer * buffer);

GstFlowReturn gst_dwrite_overlay_object_prepare_output (GstDWriteOverlayObject * object,
                                                        GstBaseTransform * trans,
                                                        gpointer trans_class,
                                                        GstBuffer * inbuf,
                                                        GstBuffer ** outbuf);

gboolean  gst_dwrite_overlay_object_draw (GstDWriteOverlayObject * object,
                                          GstBuffer * buffer,
                                          IDWriteTextLayout * layout,
                                          gint x,
                                          gint y);

G_END_DECLS
