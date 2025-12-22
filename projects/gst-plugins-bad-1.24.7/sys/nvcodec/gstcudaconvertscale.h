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

#include <gst/gst.h>
#include "gstcudabasetransform.h"

G_BEGIN_DECLS

#define GST_TYPE_CUDA_BASE_CONVERT             (gst_cuda_base_convert_get_type())
#define GST_CUDA_BASE_CONVERT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_BASE_CONVERT,GstCudaBaseConvert))
#define GST_CUDA_BASE_CONVERT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_BASE_CONVERT,GstCudaBaseConvertClass))
#define GST_CUDA_BASE_CONVERT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_BASE_CONVERT,GstCudaBaseConvertClass))
#define GST_IS_CUDA_BASE_CONVERT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_BASE_CONVERT))
#define GST_IS_CUDA_BASE_CONVERT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_BASE_CONVERT))

typedef struct _GstCudaBaseConvert GstCudaBaseConvert;
typedef struct _GstCudaBaseConvertClass GstCudaBaseConvertClass;

struct _GstCudaBaseConvertClass
{
  GstCudaBaseTransform parent_class;
};

GType gst_cuda_base_convert_get_type (void);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstCudaBaseConvert, gst_object_unref)

#define GST_TYPE_CUDA_CONVERT_SCALE (gst_cuda_convert_scale_get_type())
G_DECLARE_FINAL_TYPE (GstCudaConvertScale, gst_cuda_convert_scale,
    GST, CUDA_CONVERT_SCALE, GstCudaBaseConvert)

#define GST_TYPE_CUDA_CONVERT (gst_cuda_convert_get_type())
G_DECLARE_FINAL_TYPE (GstCudaConvert, gst_cuda_convert,
    GST, CUDA_CONVERT, GstCudaBaseConvert)

#define GST_TYPE_CUDA_SCALE (gst_cuda_scale_get_type())
G_DECLARE_FINAL_TYPE (GstCudaScale, gst_cuda_scale,
    GST, CUDA_SCALE, GstCudaBaseConvert)

G_END_DECLS

