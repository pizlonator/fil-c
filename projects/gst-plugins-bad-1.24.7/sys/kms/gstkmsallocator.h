/* GStreamer
 *
 * Copyright (C) 2016 Igalia
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
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
 *
 */

#ifndef __GST_KMS_ALLOCATOR_H__
#define __GST_KMS_ALLOCATOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_KMS_ALLOCATOR	\
   (gst_kms_allocator_get_type())
#define GST_IS_KMS_ALLOCATOR(obj)				\
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KMS_ALLOCATOR))
#define GST_IS_KMS_ALLOCATOR_CLASS(klass)			\
   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KMS_ALLOCATOR))
#define GST_KMS_ALLOCATOR_GET_CLASS(obj)			\
   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_KMS_ALLOCATOR, GstKMSAllocatorClass))
#define GST_KMS_ALLOCATOR(obj)				\
   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KMS_ALLOCATOR, GstKMSAllocator))
#define GST_KMS_ALLOCATOR_CLASS(klass)			\
   (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_KMS_ALLOCATOR, GstKMSAllocatorClass))

typedef struct _GstKMSAllocator GstKMSAllocator;
typedef struct _GstKMSAllocatorClass GstKMSAllocatorClass;
typedef struct _GstKMSAllocatorPrivate GstKMSAllocatorPrivate;
typedef struct _GstKMSMemory GstKMSMemory;

struct kms_bo;

struct _GstKMSMemory
{
  GstMemory parent;

  guint32 fb_id;
  GstMemory *bo;
  GstMapInfo bo_map;
  gint bo_map_refs;
};

struct _GstKMSAllocator
{
  GstAllocator parent;
  GstKMSAllocatorPrivate *priv;
};

struct _GstKMSAllocatorClass {
  GstAllocatorClass parent_class;
};

GType gst_kms_allocator_get_type (void) G_GNUC_CONST;

gboolean gst_is_kms_memory (GstMemory *mem);
guint32 gst_kms_memory_get_fb_id (GstMemory *mem);

GstAllocator* gst_kms_allocator_new (gint fd);

GstMemory*    gst_kms_allocator_bo_alloc (GstAllocator *allocator,
					  GstVideoInfo *vinfo);

GstKMSMemory* gst_kms_allocator_dmabuf_import (GstAllocator *allocator,
					       gint *prime_fds,
					       gint n_planes,
					       gsize offsets[GST_VIDEO_MAX_PLANES],
					       GstVideoInfo *vinfo);

GstMemory*    gst_kms_allocator_dmabuf_export (GstAllocator *allocator,
                                               GstMemory *kmsmem);

GstMemory *   gst_kms_allocator_get_cached  (GstMemory * mem);

void          gst_kms_allocator_clear_cache (GstAllocator * allocator);

void          gst_kms_allocator_cache       (GstAllocator * allocator,
                                             GstMemory * mem,
                                             GstMemory * kmsmem);

G_END_DECLS


#endif /* __GST_KMS_ALLOCATOR_H__ */
