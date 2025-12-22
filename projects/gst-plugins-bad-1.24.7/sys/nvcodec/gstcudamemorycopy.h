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

#define GST_TYPE_CUDA_MEMORY_COPY             (gst_cuda_memory_copy_get_type())
#define GST_CUDA_MEMORY_COPY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_MEMORY_COPY,GstCudaMemoryCopy))
#define GST_CUDA_MEMORY_COPY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_MEMORY_COPY,GstCudaMemoryCopyClass))
#define GST_CUDA_MEMORY_COPY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_MEMORY_COPY,GstCudaMemoryCopyClass))
#define GST_IS_CUDA_MEMORY_COPY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_MEMORY_COPY))
#define GST_IS_CUDA_MEMORY_COPY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_MEMORY_COPY))

typedef struct _GstCudaMemoryCopy GstCudaMemoryCopy;
typedef struct _GstCudaMemoryCopyClass GstCudaMemoryCopyClass;

struct _GstCudaMemoryCopyClass
{
  GstCudaBaseTransform parent_class;

  gboolean uploader;
};

GType gst_cuda_memory_copy_get_type (void);

void gst_cuda_memory_copy_register  (GstPlugin * plugin,
                                     guint rank);

G_END_DECLS

