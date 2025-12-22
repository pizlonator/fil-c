/* GStreamer
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *
 * gstipcslavepipeline.h:
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
#ifndef _GST_IPC_SLAVE_PIPELINE_H_
#define _GST_IPC_SLAVE_PIPELINE_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_IPC_SLAVE_PIPELINE \
    (gst_ipc_slave_pipeline_get_type())
#define GST_IPC_SLAVE_PIPELINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IPC_SLAVE_PIPELINE,GstIpcSlavePipeline))
#define GST_IPC_SLAVE_PIPELINE_CAST(obj) \
    ((GstIpcSlavePipeline *) obj)
#define GST_IPC_SLAVE_PIPELINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IPC_SLAVE_PIPELINE,GstIpcSlavePipelineClass))
#define GST_IS_IPC_SLAVE_PIPELINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IPC_SLAVE_PIPELINE))
#define GST_IS_IPC_SLAVE_PIPELINE_CLASS(obj) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IPC_SLAVE_PIPELINE))

typedef struct _GstIpcSlavePipeline GstIpcSlavePipeline;
typedef struct _GstIpcSlavePipelineClass GstIpcSlavePipelineClass;

struct _GstIpcSlavePipeline
{
  GstPipeline pipeline;
};

struct _GstIpcSlavePipelineClass
{
  GstPipelineClass pipeline_class;
};

G_GNUC_INTERNAL GType gst_ipc_slave_pipeline_get_type (void);

G_END_DECLS

#endif
