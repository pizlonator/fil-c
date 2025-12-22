/*
 * GStreamer gstreamer-tensormeta
 * Copyright (C) 2023 Collabora Ltd
 *
 * gsttensormeta.h
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
#ifndef __GST_TENSOR_META_H__
#define __GST_TENSOR_META_H__

#include <gst/gst.h>

/**
 * GstTensorDataType:
 *
 * @GST_TENSOR_TYPE_INT4 signed 4 bit integer tensor data
 * @GST_TENSOR_TYPE_INT8 signed 8 bit integer tensor data
 * @GST_TENSOR_TYPE_INT16 signed 16 bit integer tensor data
 * @GST_TENSOR_TYPE_INT32 signed 32 bit integer tensor data
 * @GST_TENSOR_TYPE_INT64 signed 64 bit integer tensor data
 * @GST_TENSOR_TYPE_UINT4 unsigned 4 bit integer tensor data
 * @GST_TENSOR_TYPE_UINT8 unsigned 8 bit integer tensor data
 * @GST_TENSOR_TYPE_UINT16 unsigned 16 bit integer tensor data
 * @GST_TENSOR_TYPE_UINT32 unsigned 32 bit integer tensor data
 * @GST_TENSOR_TYPE_UINT64 unsigned 64 bit integer tensor data
 * @GST_TENSOR_TYPE_FLOAT16 16 bit floating point tensor data
 * @GST_TENSOR_TYPE_FLOAT32 32 bit floating point tensor data
 * @GST_TENSOR_TYPE_FLOAT64 64 bit floating point tensor data
 * @GST_TENSOR_TYPE_BFLOAT16 "brain" 16 bit floating point tensor data
 *
 * Describe the type of data contain in the tensor.
 *
 * Since: 1.24
 */
typedef enum _GstTensorDataType
{
  GST_TENSOR_TYPE_INT4,
  GST_TENSOR_TYPE_INT8,
  GST_TENSOR_TYPE_INT16,
  GST_TENSOR_TYPE_INT32,
  GST_TENSOR_TYPE_INT64,
  GST_TENSOR_TYPE_UINT4,
  GST_TENSOR_TYPE_UINT8,
  GST_TENSOR_TYPE_UINT16,
  GST_TENSOR_TYPE_UINT32,
  GST_TENSOR_TYPE_UINT64,
  GST_TENSOR_TYPE_FLOAT16,
  GST_TENSOR_TYPE_FLOAT32,
  GST_TENSOR_TYPE_FLOAT64,
  GST_TENSOR_TYPE_BFLOAT16,
} GstTensorDataType;


/**
 * GstTensor:
 *
 * @id: semantically identify the contents of the tensor
 * @num_dims: number of tensor dimensions
 * @dims: tensor dimensions
 * @type: #GstTensorDataType of tensor data
 * @data: #GstBuffer holding tensor data
 *
 * Since: 1.24
 */
typedef struct _GstTensor
{
  GQuark id;
  gint num_dims;
  int64_t *dims;
  GstTensorDataType data_type;
  GstBuffer *data;
} GstTensor;

#define GST_TENSOR_MISSING_ID -1

/**
 * GstTensorMeta:
 *
 * @meta base GstMeta
 * @num_tensors number of tensors
 * @tensor @ref GstTensor for each tensor
 * @batch_size model batch size
 *
 * Since: 1.24
 */
typedef struct _GstTensorMeta
{
  GstMeta meta;

  gint num_tensors;
  GstTensor *tensor;
  int batch_size;
} GstTensorMeta;

G_BEGIN_DECLS

#define GST_TENSOR_META_API_TYPE \
  (gst_tensor_meta_api_get_type())

#define GST_TENSOR_META_INFO \
  (gst_tensor_meta_get_info())


GType gst_tensor_meta_api_get_type (void);
const GstMetaInfo *gst_tensor_meta_get_info (void);
gint gst_tensor_meta_get_index_from_id(GstTensorMeta *meta, GQuark id);

G_END_DECLS

#endif
