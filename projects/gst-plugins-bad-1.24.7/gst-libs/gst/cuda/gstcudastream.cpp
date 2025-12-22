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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cuda-gst.h"
#include "gstcudastream.h"
#include "gstcudautils.h"
#include "gstcuda-private.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static GstDebugCategory *cat = nullptr;

  GST_CUDA_CALL_ONCE_BEGIN {
    cat = _gst_debug_category_new ("cudastream", 0, "cudastream");
  } GST_CUDA_CALL_ONCE_END;

  return cat;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static gint
gst_cuda_stream_compare_func (const GstCudaStream * a, const GstCudaStream * b)
{
  if (a == b)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

static void
gst_cuda_stream_init_once (GType type)
{
  static GstValueTable table = {
    0, (GstValueCompareFunc) gst_cuda_stream_compare_func,
    nullptr, nullptr
  };

  table.type = type;
  gst_value_register (&table);
}

G_DEFINE_BOXED_TYPE_WITH_CODE (GstCudaStream, gst_cuda_stream,
    (GBoxedCopyFunc) gst_mini_object_ref,
    (GBoxedFreeFunc) gst_mini_object_unref,
    gst_cuda_stream_init_once (g_define_type_id));

struct _GstCudaStreamPrivate
{
  CUstream handle;
};

static void
_gst_cuda_stream_free (GstCudaStream * stream)
{
  GstCudaStreamPrivate *priv = stream->priv;

  if (stream->context) {
    if (priv->handle) {
      gst_cuda_context_push (stream->context);
      CuStreamDestroy (priv->handle);
      gst_cuda_context_pop (nullptr);
    }

    gst_object_unref (stream->context);
  }

  g_free (priv);
  g_free (stream);
}

/**
 * gst_cuda_stream_new:
 * @context: a #GstCudaContext
 *
 * Creates a new #GstCudaStream
 *
 * Returns: (transfer full) (nullable): a new #GstCudaStream or %NULL on
 * failure
 *
 * Since: 1.24
 */
GstCudaStream *
gst_cuda_stream_new (GstCudaContext * context)
{
  GstCudaStream *self;
  CUresult cuda_ret;
  CUstream stream;

  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  if (!gst_cuda_context_push (context)) {
    GST_ERROR_OBJECT (context, "Couldn't push context");
    return nullptr;
  }

  cuda_ret = CuStreamCreate (&stream, CU_STREAM_DEFAULT);
  gst_cuda_context_pop (nullptr);

  if (!gst_cuda_result (cuda_ret)) {
    GST_ERROR_OBJECT (context, "Couldn't create stream");
    return nullptr;
  }

  self = g_new0 (GstCudaStream, 1);
  self->context = (GstCudaContext *) gst_object_ref (context);
  self->priv = g_new0 (GstCudaStreamPrivate, 1);
  self->priv->handle = stream;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (self), 0,
      GST_TYPE_CUDA_STREAM, nullptr, nullptr,
      (GstMiniObjectFreeFunction) _gst_cuda_stream_free);

  return self;
}

/**
 * gst_cuda_stream_get_handle:
 * @stream: (allow-none): a #GstCudaStream
 *
 * Get CUDA stream handle
 *
 * Returns: a `CUstream` handle of @stream or %NULL if @stream is %NULL
 *
 * Since: 1.24
 */
CUstream
gst_cuda_stream_get_handle (GstCudaStream * stream)
{
  g_return_val_if_fail (!stream || GST_IS_CUDA_STREAM (stream), nullptr);

  if (!stream)
    return nullptr;

  return stream->priv->handle;
}

/**
 * gst_cuda_stream_ref:
 * @stream: a #GstCudaStream
 *
 * Increase the reference count of @stream.
 *
 * Returns: (transfer full): @stream
 *
 * Since: 1.24
 */
GstCudaStream *
gst_cuda_stream_ref (GstCudaStream * stream)
{
  return (GstCudaStream *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (stream));
}

/**
 * gst_cuda_stream_unref:
 * @stream: a #GstCudaStream
 *
 * Decrease the reference count of @stream.
 *
 * Since: 1.24
 */
void
gst_cuda_stream_unref (GstCudaStream * stream)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (stream));
}

/**
 * gst_clear_cuda_stream: (skip)
 * @stream: a pointer to a #GstCudaStream reference
 *
 * Clears a reference to a #GstCudaStream.
 *
 * Since: 1.24
 */
void
gst_clear_cuda_stream (GstCudaStream ** stream)
{
  if (stream && *stream) {
    gst_cuda_stream_unref (*stream);
    *stream = nullptr;
  }
}
