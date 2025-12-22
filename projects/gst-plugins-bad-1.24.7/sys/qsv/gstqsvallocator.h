/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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
#include <mfx.h>
#include "gstqsvutils.h"

G_BEGIN_DECLS

#define GST_TYPE_QSV_FRAME                (gst_qsv_frame_get_type())
#define GST_IS_QSV_FRAME(obj)             (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_QSV_FRAME))
#define GST_QSV_FRAME_CAST(obj)           ((GstQsvFrame *) obj)

#define GST_TYPE_QSV_ALLOCATOR            (gst_qsv_allocator_get_type())
#define GST_QSV_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_QSV_ALLOCATOR, GstQsvAllocator))
#define GST_QSV_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_QSV_ALLOCATOR, GstQsvAllocatorClass))
#define GST_IS_QSV_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_QSV_ALLOCATOR))
#define GST_IS_QSV_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_QSV_ALLOCATOR))
#define GST_QSV_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_QSV_ALLOCATOR, GstQsvAllocatorClass))
#define GST_QSV_ALLOCATOR_CAST(obj)       ((GstQsvAllocator *)obj)

typedef struct _GstQsvFrame GstQsvFrame;
typedef struct _GstQsvAllocator GstQsvAllocator;
typedef struct _GstQsvAllocatorClass GstQsvAllocatorClass;
typedef struct _GstQsvAllocatorPrivate GstQsvAllocatorPrivate;

GType       gst_qsv_frame_get_type (void);

GstBuffer * gst_qsv_frame_peek_buffer (GstQsvFrame * frame);

gboolean    gst_qsv_frame_set_buffer  (GstQsvFrame * frame,
                                       GstBuffer * buffer);

static inline GstQsvFrame *
gst_qsv_frame_ref (GstQsvFrame * frame)
{
  return (GstQsvFrame *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (frame));
}

static inline void
gst_qsv_frame_unref (GstQsvFrame * frame)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (frame));
}

static inline void
gst_clear_qsv_frame (GstQsvFrame ** frame)
{
  gst_clear_mini_object ((GstMiniObject **) frame);
}

typedef enum
{
  GST_QSV_SYSTEM_MEMORY = (1 << 0),
  GST_QSV_VIDEO_MEMORY = (1 << 1),
  GST_QSV_ENCODER_IN_MEMORY = (1 << 2),
  GST_QSV_DECODER_OUT_MEMORY = (1 << 3),
  GST_QSV_PROCESS_TARGET = (1 << 4),
} GstQsvMemoryType;

#define GST_QSV_MEM_TYPE_IS_SYSTEM(type) ((type & GST_QSV_SYSTEM_MEMORY) != 0)
#define GST_QSV_MEM_TYPE_IS_VIDEO(type) ((type & GST_QSV_VIDEO_MEMORY) != 0)

struct _GstQsvAllocator
{
  GstObject parent;
  gboolean is_gbr;

  GstQsvAllocatorPrivate *priv;
};

struct _GstQsvAllocatorClass
{
  GstObjectClass parent_class;

  mfxStatus   (*alloc)      (GstQsvAllocator * allocator,
                             gboolean dummy_alloc,
                             mfxFrameAllocRequest * request,
                             mfxFrameAllocResponse * response);

  GstBuffer * (*upload)     (GstQsvAllocator * allocator,
                             const GstVideoInfo * info,
                             GstBuffer * buffer,
                             GstBufferPool * pool);

  GstBuffer * (*download)   (GstQsvAllocator * allocator,
                             const GstVideoInfo * info,
                             gboolean force_copy,
                             GstQsvFrame * frame,
                             GstBufferPool * pool);
};

GType               gst_qsv_allocator_get_type        (void);

GstQsvFrame *       gst_qsv_allocator_acquire_frame   (GstQsvAllocator * allocator,
                                                       GstQsvMemoryType mem_type,
                                                       const GstVideoInfo * info,
                                                       GstBuffer * buffer,
                                                       GstBufferPool * pool);

GstBuffer *         gst_qsv_allocator_download_frame  (GstQsvAllocator * allocator,
                                                       gboolean force_copy,
                                                       GstQsvFrame * frame,
                                                       const GstVideoInfo *pool_info,
                                                       GstBufferPool * pool);

mfxFrameAllocator * gst_qsv_allocator_get_allocator_handle (GstQsvAllocator * allocator);

gboolean            gst_qsv_allocator_get_cached_response  (GstQsvAllocator * allocator,
                                                            mfxFrameAllocResponse * response);

void                gst_qsv_allocator_set_options          (GstQsvAllocator * allocator,
                                                            guint16 extra_alloc_size,
                                                            gboolean dummy_alloc);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstQsvAllocator, gst_object_unref)

G_END_DECLS

#ifdef __cplusplus
inline GstQsvMemoryType
operator | (const GstQsvMemoryType & lhs, const GstQsvMemoryType & rhs)
{
  return static_cast<GstQsvMemoryType> (static_cast<guint>(lhs) |
      static_cast<guint> (rhs));
}

inline GstQsvMemoryType &
operator |= (GstQsvMemoryType & lhs, const GstQsvMemoryType & rhs)
{
  return lhs = lhs | rhs;
}
#endif
