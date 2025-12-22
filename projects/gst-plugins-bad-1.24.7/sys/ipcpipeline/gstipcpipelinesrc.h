/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *               2015-2017 YouView TV Ltd, Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *
 * gstipcpipelinesrc.h:
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


#ifndef __GST_IPC_PIPELINE_SRC_H__
#define __GST_IPC_PIPELINE_SRC_H__

#include <gst/gst.h>
#include "gstipcpipelinecomm.h"

G_BEGIN_DECLS

#define GST_TYPE_IPC_PIPELINE_SRC \
  (gst_ipc_pipeline_src_get_type())
#define GST_IPC_PIPELINE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IPC_PIPELINE_SRC,GstIpcPipelineSrc))
#define GST_IPC_PIPELINE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IPC_PIPELINE_SRC,GstIpcPipelineSrcClass))
#define GST_IS_IPC_PIPELINE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IPC_PIPELINE_SRC))
#define GST_IS_IPC_PIPELINE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IPC_PIPELINE_SRC))

typedef struct _GstIpcPipelineSrc GstIpcPipelineSrc;
typedef struct _GstIpcPipelineSrcClass GstIpcPipelineSrcClass;

/**
 * GstIpcPipelineSrc:
 *
 * Opaque #GstIpcPipelineSrc data structure.
 */
struct _GstIpcPipelineSrc {
  GstElement element;

  GstIpcPipelineComm comm;
  GstPad *srcpad;

  gboolean flushing;
  GList *queued;
  GstFlowReturn last_ret;

  GCond create_cond;
};

struct _GstIpcPipelineSrcClass {
  GstElementClass parent_class;

  gboolean (*forward_message) (GstIpcPipelineSrc *, GstMessage *);
  void (*disconnect) (GstIpcPipelineSrc * src);
};

G_GNUC_INTERNAL GType gst_ipc_pipeline_src_get_type (void);

G_END_DECLS

#endif /* __GST_IPC_PIPELINE_SRC_H__ */
