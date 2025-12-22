/*
 * GStreamer
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

#ifndef __GST_KMS_BUFFER_POOL_H__
#define __GST_KMS_BUFFER_POOL_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstkmssink.h"

G_BEGIN_DECLS

/**
 * GST_BUFFER_POOL_OPTION_KMS_BUFFER:
 *
 * An option that can be activated on buffer pool to request KMS
 * buffers. NOT IMPLEMENTED
 */
#define GST_BUFFER_POOL_OPTION_KMS_BUFFER "GstBufferPoolOptionKMSBuffer"

/**
 * GST_BUFFER_POOL_OPTION_KMS_PRIME_EXPORT:
 *
 * An option that can be activated on buffer pool to request DMABuf
 * buffers. Callers are responsible to check if this is supported. Dumb buffers
 * will be returned if not supported.
 */
#define GST_BUFFER_POOL_OPTION_KMS_PRIME_EXPORT "GstBufferPoolOptionKMSPrimeExport"

/* video bufferpool */
typedef struct _GstKMSBufferPool GstKMSBufferPool;
typedef struct _GstKMSBufferPoolClass GstKMSBufferPoolClass;
typedef struct _GstKMSBufferPoolPrivate GstKMSBufferPoolPrivate;

#define GST_TYPE_KMS_BUFFER_POOL \
  (gst_kms_buffer_pool_get_type())
#define GST_IS_KMS_BUFFER_POOL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KMS_BUFFER_POOL))
#define GST_KMS_BUFFER_POOL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KMS_BUFFER_POOL, GstKMSBufferPool))
#define GST_KMS_BUFFER_POOL_CAST(obj) \
  ((GstKMSBufferPool*)(obj))

struct _GstKMSBufferPool
{
  GstVideoBufferPool parent;
  GstKMSBufferPoolPrivate *priv;
};

struct _GstKMSBufferPoolClass
{
  GstVideoBufferPoolClass parent_class;
};

GType gst_kms_buffer_pool_get_type (void) G_GNUC_CONST;

GstBufferPool *gst_kms_buffer_pool_new (void);

G_END_DECLS

#endif /* __GST_KMS_BUFFER_POOL_H__ */
