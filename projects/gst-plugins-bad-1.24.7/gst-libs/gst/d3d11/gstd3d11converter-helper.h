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
#include <gst/d3d11/gstd3d11_fwd.h>

G_BEGIN_DECLS

typedef struct _GstD3D11ConverterHelper GstD3D11ConverterHelper;

GstD3D11ConverterHelper * gst_d3d11_converter_helper_new (GstD3D11Device * device,
                                                          GstVideoFormat in_format,
                                                          GstVideoFormat out_format,
                                                          guint width,
                                                          guint height);

void                      gst_d3d11_converter_helper_free (GstD3D11ConverterHelper * helper);

void                      gst_d3d11_converter_helper_update_size (GstD3D11ConverterHelper * helper,
                                                                  guint width,
                                                                  guint height);

GstBuffer *               gst_d3d11_converter_helper_preproc (GstD3D11ConverterHelper * helper,
                                                              GstBuffer * buffer);

gboolean                  gst_d3d11_converter_helper_postproc (GstD3D11ConverterHelper * helper,
                                                               GstBuffer * in_buf,
                                                               GstBuffer * out_buf);

G_END_DECLS
