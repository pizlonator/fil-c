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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvdecobject.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>
#include <string.h>
#include <algorithm>
#include <gst/cuda/gstcuda-private.h>

extern "C"
{
  GST_DEBUG_CATEGORY_EXTERN (gst_nv_decoder_debug);
}

#define GST_CAT_DEFAULT gst_nv_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstNvDecSurface, gst_nv_dec_surface);
static GstNvDecSurface *gst_nv_dec_surface_new (guint seq_num);

/* *INDENT-OFF* */
struct GstNvDecOutput
{
  GstNvDecObject *self = nullptr;
  CUdeviceptr devptr = 0;
  guint seq_num = 0;
};

struct GstNvDecObjectPrivate
{
  std::vector < GstNvDecSurface * >surface_queue;
  std::map < CUdeviceptr, GstMemory *> output_map;
  std::map < CUdeviceptr, GstMemory *> free_output_map;

  std::mutex lock;
  std::condition_variable cond;
};
/* *INDENT-ON* */

struct _GstNvDecObject
{
  GstObject parent;

  GstNvDecObjectPrivate *priv;

  CUvideodecoder handle;
  CUVIDDECODECREATEINFO create_info;

  GstVideoInfo video_info;

  GstCudaContext *context;

  gboolean flushing;

  guint pool_size;
  guint num_mapped;
  gboolean alloc_aux_frame;
  guint plane_height;
  guint seq_num;
};

static void gst_nv_dec_object_finalize (GObject * object);

#define gst_nv_dec_object_parent_class parent_class
G_DEFINE_TYPE (GstNvDecObject, gst_nv_dec_object, GST_TYPE_OBJECT);

static void
gst_nv_dec_object_class_init (GstNvDecObjectClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_nv_dec_object_finalize;
}

static void
gst_nv_dec_object_init (GstNvDecObject * self)
{
  self->priv = new GstNvDecObjectPrivate ();
}

static void
gst_nv_dec_object_finalize (GObject * object)
{
  GstNvDecObject *self = GST_NV_DEC_OBJECT (object);
  GstNvDecObjectPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Finalize");

  gst_cuda_context_push (self->context);
  /* *INDENT-OFF* */
  for (auto it : priv->surface_queue)
    gst_nv_dec_surface_unref (it);

  /* *INDENT-OFF* */
  for (auto it : priv->free_output_map)
    gst_memory_unref (it.second);
  /* *INDENT-ON* */

  delete self->priv;

  CuvidDestroyDecoder (self->handle);
  gst_cuda_context_pop (nullptr);

  gst_object_unref (self->context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstNvDecObject *
gst_nv_dec_object_new (GstCudaContext * context,
    CUVIDDECODECREATEINFO * create_info, const GstVideoInfo * video_info,
    gboolean alloc_aux_frame)
{
  GstNvDecObject *self;
  CUresult ret;
  CUvideodecoder handle = nullptr;
  guint pool_size;

  if (!gst_cuda_context_push (context)) {
    GST_ERROR_OBJECT (context, "Failed to push context");
    return nullptr;
  }

  ret = CuvidCreateDecoder (&handle, create_info);
  gst_cuda_context_pop (nullptr);

  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (context, "Could not create decoder instance");
    return nullptr;
  }

  pool_size = create_info->ulNumDecodeSurfaces;
  if (alloc_aux_frame)
    pool_size /= 2;

  self = (GstNvDecObject *)
      g_object_new (GST_TYPE_NV_DEC_OBJECT, nullptr);
  gst_object_ref_sink (self);
  self->context = (GstCudaContext *) gst_object_ref (context);
  self->handle = handle;
  self->create_info = *create_info;
  self->video_info = *video_info;
  self->pool_size = pool_size;
  self->plane_height = create_info->ulTargetHeight;

  for (guint i = 0; i < pool_size; i++) {
    GstNvDecSurface *surf = gst_nv_dec_surface_new (0);

    surf->index = i;

    /* [0, pool_size - 1]: output picture
     * [pool_size, pool_size * 2 - 1]: decoder output without film-grain,
     * used for reference picture */
    if (alloc_aux_frame)
      surf->decode_frame_index = i + pool_size;
    else
      surf->decode_frame_index = i;

    self->priv->surface_queue.push_back (surf);
  }

  return self;
}

gboolean
gst_nv_dec_object_reconfigure (GstNvDecObject * object,
    CUVIDRECONFIGUREDECODERINFO * reconfigure_info,
    const GstVideoInfo * video_info, gboolean alloc_aux_frame)
{
  GstNvDecObjectPrivate *priv = object->priv;
  CUresult ret;
  guint pool_size;

  if (!gst_cuvid_can_reconfigure ())
    return FALSE;

  pool_size = reconfigure_info->ulNumDecodeSurfaces;
  if (alloc_aux_frame)
    pool_size /= 2;

  std::lock_guard < std::mutex > lk (priv->lock);
  if (!gst_cuda_context_push (object->context)) {
    GST_ERROR_OBJECT (object, "Couldn't push context");
    return FALSE;
  }

  ret = CuvidReconfigureDecoder (object->handle, reconfigure_info);
  gst_cuda_context_pop (nullptr);

  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (object, "Couldn't reconfigure decoder");
    return FALSE;
  }

  if ((guint) priv->surface_queue.size () != object->pool_size) {
    GST_WARNING_OBJECT (object, "Unused surfaces %u != pool size %u",
        (guint) priv->surface_queue.size (), object->pool_size);
  }

  /* Release old surfaces and create new ones */
  /* *INDENT-OFF* */
  for (auto it : priv->surface_queue)
    gst_nv_dec_surface_unref (it);
  /* *INDENT-ON* */

  priv->surface_queue.clear ();

  object->pool_size = pool_size;
  object->video_info = *video_info;
  object->seq_num++;
  object->plane_height = reconfigure_info->ulTargetHeight;

  for (guint i = 0; i < pool_size; i++) {
    GstNvDecSurface *surf = gst_nv_dec_surface_new (object->seq_num);

    surf->index = i;

    /* [0, pool_size - 1]: output picture
     * [pool_size, pool_size * 2 - 1]: decoder output without film-grain,
     * used for reference picture */
    if (alloc_aux_frame)
      surf->decode_frame_index = i + pool_size;
    else
      surf->decode_frame_index = i;

    object->priv->surface_queue.push_back (surf);
  }

  return TRUE;
}

void
gst_nv_dec_object_set_flushing (GstNvDecObject * object, gboolean flushing)
{
  GstNvDecObjectPrivate *priv = object->priv;
  std::lock_guard < std::mutex > lk (priv->lock);
  object->flushing = flushing;
  priv->cond.notify_all ();
}

static gboolean
gst_nv_dec_object_unmap_surface_unlocked (GstNvDecObject * self,
    GstNvDecSurface * surface)
{
  gboolean ret = TRUE;

  if (!gst_cuda_result (CuvidUnmapVideoFrame (self->handle, surface->devptr))) {
    GST_ERROR_OBJECT (self, "Couldn't unmap surface %d", surface->index);
    ret = FALSE;
  } else {
    surface->devptr = 0;
    self->num_mapped--;

    GST_LOG_OBJECT (self, "Surface %d is unmapped, num-mapped %d",
        surface->index, self->num_mapped);
  }
  self->priv->cond.notify_all ();

  return ret;
}

GstFlowReturn
gst_nv_dec_object_acquire_surface (GstNvDecObject * object,
    GstNvDecSurface ** surface)
{
  GstNvDecObjectPrivate *priv = object->priv;
  GstNvDecSurface *surf = nullptr;
  std::unique_lock < std::mutex > lk (priv->lock);

  do {
    if (object->flushing) {
      GST_DEBUG_OBJECT (object, "We are flushing");
      return GST_FLOW_FLUSHING;
    }

    if (!priv->surface_queue.empty ()) {
      surf = priv->surface_queue[0];
      priv->surface_queue.erase (priv->surface_queue.begin ());
      break;
    }

    GST_LOG_OBJECT (object, "No available surface, waiting for release");
    priv->cond.wait (lk);
  } while (true);

  g_assert (surf);
  g_assert (!surf->object);

  surf->object = (GstNvDecObject *) gst_object_ref (object);

  *surface = surf;

  return GST_FLOW_OK;
}

gboolean
gst_nv_dec_object_decode (GstNvDecObject * object, CUVIDPICPARAMS * params)
{
  gboolean ret = TRUE;

  GST_LOG_OBJECT (object, "picture index: %u", params->CurrPicIdx);

  if (!gst_cuda_context_push (object->context)) {
    GST_ERROR_OBJECT (object, "Failed to push CUDA context");
    return FALSE;
  }

  if (!gst_cuda_result (CuvidDecodePicture (object->handle, params))) {
    GST_ERROR_OBJECT (object, "Failed to decode picture");
    ret = FALSE;
  }

  if (!gst_cuda_context_pop (nullptr))
    GST_WARNING_OBJECT (object, "Failed to pop CUDA context");

  return ret;
}

GstFlowReturn
gst_nv_dec_object_map_surface (GstNvDecObject * object,
    GstNvDecSurface * surface, GstCudaStream * stream)
{
  GstNvDecObjectPrivate *priv = object->priv;

  if (surface->devptr) {
    GST_ERROR_OBJECT (object, "Mapped Surface %d was not cleared",
        surface->index);
    return GST_FLOW_ERROR;
  }

  std::unique_lock < std::mutex > lk (priv->lock);
  do {
    if (object->flushing) {
      GST_DEBUG_OBJECT (object, "We are flushing");
      return GST_FLOW_FLUSHING;
    }

    if (object->num_mapped < (guint) object->create_info.ulNumOutputSurfaces) {
      CUVIDPROCPARAMS params = { 0 };

      params.progressive_frame = 1;
      params.output_stream = gst_cuda_stream_get_handle (stream);

      if (!gst_cuda_result (CuvidMapVideoFrame (object->handle, surface->index,
                  &surface->devptr, &surface->pitch, &params))) {
        GST_ERROR_OBJECT (object, "Couldn't map picture");
        return GST_FLOW_ERROR;
      }

      object->num_mapped++;
      GST_LOG_OBJECT (object, "Surface %d is mapped, num-mapped %d",
          surface->index, object->num_mapped);
      break;
    }

    GST_LOG_OBJECT (object, "No available output surface, waiting for release");
    priv->cond.wait (lk);
  } while (true);

  return GST_FLOW_OK;
}

gboolean
gst_nv_dec_object_unmap_surface (GstNvDecObject * object,
    GstNvDecSurface * surface)
{
  GstNvDecObjectPrivate *priv = object->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  return gst_nv_dec_object_unmap_surface_unlocked (object, surface);
}

static gboolean
gst_nv_dec_output_release (GstCudaMemory * mem)
{
  GstNvDecOutput *output = (GstNvDecOutput *)
      gst_cuda_memory_get_user_data (mem);
  GstNvDecObject *self = output->self;
  GstNvDecObjectPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "Release memory %p", mem);

  gst_memory_ref (GST_MEMORY_CAST (mem));
  GST_MINI_OBJECT_CAST (mem)->dispose = nullptr;

  output->self = nullptr;

  {
    std::lock_guard < std::mutex > lk (priv->lock);

    self->num_mapped--;
    gst_cuda_context_push (self->context);
    if (!gst_cuda_result (CuvidUnmapVideoFrame (self->handle, output->devptr))) {
      GST_ERROR_OBJECT (self, "Couldn't unmap frame");
    } else {
      GST_LOG_OBJECT (self, "Exported surface is freed, num-mapped %d",
          self->num_mapped);
    }
    gst_cuda_context_pop (nullptr);

    priv->free_output_map[output->devptr] = GST_MEMORY_CAST (mem);
    priv->cond.notify_all ();
  }

  gst_object_unref (self);

  return FALSE;
}

static void
gst_nv_dec_output_free (GstNvDecOutput * output)
{
  delete output;
}

GstFlowReturn
gst_nv_dec_object_export_surface (GstNvDecObject * object,
    GstNvDecSurface * surface, GstCudaStream * stream, GstMemory ** memory)
{
  GstNvDecObjectPrivate *priv = object->priv;
  GstVideoInfo info;
  gsize offset;
  GstMemory *mem = nullptr;
  GstNvDecOutput *output;

  if (!surface->devptr) {
    GST_ERROR_OBJECT (object, "Surface %d is not mapped", surface->index);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (object, "Exporting surface %d", surface->index);

  offset = surface->pitch * object->plane_height;

  info = object->video_info;
  switch (GST_VIDEO_INFO_FORMAT (&info)) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      info.stride[0] = surface->pitch;
      info.stride[1] = surface->pitch;
      info.offset[0] = 0;
      info.offset[1] = offset;
      info.size = offset + offset / 2;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_16LE:
      info.stride[0] = surface->pitch;
      info.stride[1] = surface->pitch;
      info.stride[2] = surface->pitch;
      info.offset[0] = 0;
      info.offset[1] = offset;
      info.offset[2] = offset * 2;
      info.size = offset * 3;
      break;
    default:
      GST_ERROR_OBJECT (object, "Unexpected format %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&info)));
      return GST_FLOW_ERROR;
  }

  std::unique_lock < std::mutex > lk (priv->lock);
  auto output_iter = priv->output_map.find (surface->devptr);
  if (output_iter != priv->output_map.end ())
    mem = output_iter->second;

  if (mem) {
    do {
      if (object->flushing) {
        GST_DEBUG_OBJECT (object, "We are flushing");
        return GST_FLOW_FLUSHING;
      }

      auto iter = priv->free_output_map.find (surface->devptr);
      if (iter != priv->free_output_map.end ()) {
        priv->free_output_map.erase (iter);
        break;
      }

      GST_LOG_OBJECT (object, "Waiting for output release");
      priv->cond.wait (lk);
    } while (true);

    output = (GstNvDecOutput *)
        gst_cuda_memory_get_user_data (GST_CUDA_MEMORY_CAST (mem));
    if (output->seq_num != object->seq_num) {
      GST_DEBUG_OBJECT (object,
          "output belongs to previous sequence, need new memory");
      gst_memory_unref (mem);
      mem = nullptr;
    }
  }

  if (!mem) {
    output = new GstNvDecOutput ();
    output->devptr = surface->devptr;
    output->seq_num = object->seq_num;

    GST_LOG_OBJECT (object, "New output, allocating memory");

    mem = gst_cuda_allocator_alloc_wrapped (nullptr, object->context,
        stream, &info, output->devptr, output,
        (GDestroyNotify) gst_nv_dec_output_free);
    gst_cuda_memory_set_from_fixed_pool (mem);

    priv->output_map[output->devptr] = mem;
  } else {
    GST_LOG_OBJECT (object, "Reuse memory");
  }

  GST_MINI_OBJECT_CAST (mem)->dispose =
      (GstMiniObjectDisposeFunction) gst_nv_dec_output_release;

  output = (GstNvDecOutput *)
      gst_cuda_memory_get_user_data (GST_CUDA_MEMORY_CAST (mem));

  g_assert (!output->self);

  output->self = (GstNvDecObject *) gst_object_ref (object);
  surface->devptr = 0;

  *memory = mem;

  return GST_FLOW_OK;
}

guint
gst_nv_dec_object_get_num_free_surfaces (GstNvDecObject * object)
{
  GstNvDecObjectPrivate *priv = object->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  if (object->num_mapped >= object->create_info.ulNumOutputSurfaces)
    return 0;

  return object->create_info.ulNumOutputSurfaces - object->num_mapped;
}

static gboolean
gst_nv_dec_surface_dispose (GstNvDecSurface * surf)
{
  GstNvDecObject *object;
  GstNvDecObjectPrivate *priv;
  gboolean ret = FALSE;

  if (!surf->object)
    return TRUE;

  object = (GstNvDecObject *) g_steal_pointer (&surf->object);
  priv = object->priv;

  /* *INDENT-OFF* */
  {
    std::lock_guard < std::mutex > lk (priv->lock);

    if (surf->seq_num == object->seq_num) {
      /* Back to surface queue */
      gst_nv_dec_surface_ref (surf);

      /* Keep sorted order */
      priv->surface_queue.insert (
          std::upper_bound (priv->surface_queue.begin (),
          priv->surface_queue.end(), surf,
              [] (const GstNvDecSurface * a, const GstNvDecSurface * b)
              {
                return a->index < b->index;
              }), surf);
      priv->cond.notify_all ();
    } else {
      GST_WARNING_OBJECT (object, "Releasing surface %p of previous sequence",
          surf);
      /* Shouldn't happen (e.g., surfaces were not flushed before reconfigure) */
      ret = TRUE;
    }
  }
  /* *INDENT-ON* */

  gst_object_unref (object);

  return ret;
}

static GstNvDecSurface *
gst_nv_dec_surface_new (guint seq_num)
{
  GstNvDecSurface *surf = g_new0 (GstNvDecSurface, 1);

  surf->seq_num = seq_num;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (surf),
      0, GST_TYPE_NV_DEC_SURFACE, nullptr,
      (GstMiniObjectDisposeFunction) gst_nv_dec_surface_dispose,
      (GstMiniObjectFreeFunction) g_free);

  return surf;
}
