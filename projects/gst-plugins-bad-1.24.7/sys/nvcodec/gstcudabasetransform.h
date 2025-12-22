/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_CUDA_BASE_TRANSFORM_H__
#define __GST_CUDA_BASE_TRANSFORM_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <gst/cuda/gstcuda.h>

G_BEGIN_DECLS

#define GST_TYPE_CUDA_BASE_TRANSFORM             (gst_cuda_base_transform_get_type())
#define GST_CUDA_BASE_TRANSFORM(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_BASE_TRANSFORM,GstCudaBaseTransform))
#define GST_CUDA_BASE_TRANSFORM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_BASE_TRANSFORM,GstCudaBaseTransformClass))
#define GST_CUDA_BASE_TRANSFORM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_BASE_TRANSFORM,GstCudaBaseTransformClass))
#define GST_IS_CUDA_BASE_TRANSFORM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_BASE_TRANSFORM))
#define GST_IS_CUDA_BASE_TRANSFORM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_BASE_TRANSFORM))

typedef struct _GstCudaBaseTransform GstCudaBaseTransform;
typedef struct _GstCudaBaseTransformClass GstCudaBaseTransformClass;

struct _GstCudaBaseTransform
{
  GstBaseTransform parent;

  GstCudaContext *context;
  GstCudaStream *stream;

  GstVideoInfo in_info;
  GstVideoInfo out_info;

  gint device_id;
};

struct _GstCudaBaseTransformClass
{
  GstBaseTransformClass parent_class;

  gboolean  (*set_info) (GstCudaBaseTransform *filter,
                         GstCaps *incaps,
                         GstVideoInfo *in_info,
                         GstCaps *outcaps,
                         GstVideoInfo *out_info);
};

GType gst_cuda_base_transform_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstCudaBaseTransform, gst_object_unref)

G_END_DECLS

#endif /* __GST_CUDA_BASE_TRANSFORM_H__ */
