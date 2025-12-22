/* GStreamer
 * Copyright (C) <2018-2019> Seungha Yang <seungha.yang@navercorp.com>
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
#include <gst/cuda/cuda-prelude.h>
#include <gst/cuda/cuda-gst.h>

G_BEGIN_DECLS

#define GST_TYPE_CUDA_CONTEXT             (gst_cuda_context_get_type())
#define GST_CUDA_CONTEXT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CUDA_CONTEXT,GstCudaContext))
#define GST_CUDA_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),  GST_TYPE_CUDA_CONTEXT,GstCudaContextClass))
#define GST_CUDA_CONTEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),  GST_TYPE_CUDA_CONTEXT,GstCudaContextClass))
#define GST_IS_CUDA_CONTEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj),GST_TYPE_CUDA_CONTEXT))
#define GST_IS_CUDA_CONTEXT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CUDA_CONTEXT))

/**
 * GST_CUDA_CONTEXT_CAST:
 *
 * Since: 1.22
 */
#define GST_CUDA_CONTEXT_CAST(obj)        ((GstCudaContext*)(obj))

/**
 * GST_CUDA_CONTEXT_TYPE:
 *
 * Since: 1.22
 */
#define GST_CUDA_CONTEXT_TYPE "gst.cuda.context"

typedef struct _GstCudaContext GstCudaContext;
typedef struct _GstCudaContextClass GstCudaContextClass;
typedef struct _GstCudaContextPrivate GstCudaContextPrivate;

/**
 * GstCudaContext:
 *
 * Since: 1.22
 */
struct _GstCudaContext
{
  GstObject object;

  /*< private >*/
  GstCudaContextPrivate *priv;
};

struct _GstCudaContextClass
{
  GstObjectClass parent_class;
};

GST_CUDA_API
GType            gst_cuda_context_get_type    (void);

GST_CUDA_API
GstCudaContext * gst_cuda_context_new         (guint device_id);

GST_CUDA_API
GstCudaContext * gst_cuda_context_new_wrapped (CUcontext handler, CUdevice device);

GST_CUDA_API
gboolean         gst_cuda_context_push        (GstCudaContext * ctx);

GST_CUDA_API
gboolean         gst_cuda_context_pop         (CUcontext * cuda_ctx);

GST_CUDA_API
gpointer         gst_cuda_context_get_handle  (GstCudaContext * ctx);

GST_CUDA_API
gint             gst_cuda_context_get_texture_alignment (GstCudaContext * ctx);

GST_CUDA_API
gboolean         gst_cuda_context_can_access_peer (GstCudaContext * ctx,
                                                   GstCudaContext * peer);

G_END_DECLS

