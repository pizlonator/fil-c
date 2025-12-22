/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstnvencoder.h"

G_BEGIN_DECLS

GstNvEncoderClassData * gst_nv_h265_encoder_register_cuda  (GstPlugin * plugin,
                                                            GstCudaContext * context,
                                                            guint rank);

#ifdef G_OS_WIN32
GstNvEncoderClassData * gst_nv_h265_encoder_register_d3d11 (GstPlugin * plugin,
                                                            GstD3D11Device * device,
                                                            guint rank);
#endif

void gst_nv_h265_encoder_register_auto_select (GstPlugin * plugin,
                                               GList * device_caps_list,
                                               guint rank);


G_END_DECLS
