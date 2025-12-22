/* GStreamer
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *
 * gstipcslavepipeline.c:
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
/**
 * SECTION:element-ipcslavepipeline
 * @see_also: #GstIpcPipelineSink, #GstIpcPipelineSrc
 *
 * This is a GstPipeline subclass meant to embed one ore more ipcpipelinesrc
 * elements, and be slaved transparently to the master pipeline, using one ore
 * more ipcpipelinesink elements on the master.
 *
 * The actual pipeline slaving logic happens in ipcpipelinesrc. The only thing
 * that this class actually does is that it watches the pipeline bus for
 * messages and forwards them to the master pipeline through the ipcpipelinesrc
 * elements that it contains.
 *
 * For more details about this mechanism and its uses, see the documentation
 * of the ipcpipelinesink element.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstipcpipelineelements.h"
#include "gstipcpipelinesrc.h"
#include "gstipcslavepipeline.h"

GST_DEBUG_CATEGORY_STATIC (gst_ipcslavepipeline_debug);
#define GST_CAT_DEFAULT gst_ipcslavepipeline_debug

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_ipcslavepipeline_debug, "ipcslavepipeline", 0, "ipcslavepipeline element");
#define gst_ipc_slave_pipeline_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstIpcSlavePipeline, gst_ipc_slave_pipeline,
    GST_TYPE_PIPELINE, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ipcslavepipeline, "ipcslavepipeline",
    GST_RANK_NONE, GST_TYPE_IPC_SLAVE_PIPELINE,
    icepipeline_element_init (plugin));

static gboolean gst_ipc_slave_pipeline_post_message (GstElement * element,
    GstMessage * message);

static void
gst_ipc_slave_pipeline_class_init (GstIpcSlavePipelineClass * klass)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (klass);

  element_class->post_message = gst_ipc_slave_pipeline_post_message;

  gst_element_class_set_static_metadata (element_class,
      "Inter-process slave pipeline",
      "Generic/Bin/Slave",
      "Contains the slave part of an inter-process pipeline",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk");
}

static void
gst_ipc_slave_pipeline_init (GstIpcSlavePipeline * isp)
{
}

static gboolean
send_message_if_ipcpipelinesrc (const GValue * v, GValue * r,
    gpointer user_data)
{
  GstElement *e;
  GType et;
  gboolean ret;
  GstMessage *message = user_data;

  e = g_value_get_object (v);
  et = gst_element_factory_get_element_type (gst_element_get_factory (e));
  if (et == GST_TYPE_IPC_PIPELINE_SRC) {
    g_signal_emit_by_name (G_OBJECT (e), "forward-message", message, &ret);

    /* if we successfully sent this to the master and it's not ASYNC_DONE or EOS,
     * we can skip sending it again through the other ipcpipelinesrcs */
    if (ret && GST_MESSAGE_TYPE (message) != GST_MESSAGE_ASYNC_DONE &&
        GST_MESSAGE_TYPE (message) != GST_MESSAGE_EOS)
      return FALSE;
  }
  return TRUE;
}

static void
gst_ipc_slave_pipeline_forward_message (GstIpcSlavePipeline * pipeline,
    GstMessage * message)
{
  GstIterator *it;

  it = gst_bin_iterate_sources (GST_BIN (pipeline));
  gst_iterator_fold (it, send_message_if_ipcpipelinesrc, NULL, message);
  gst_iterator_free (it);
}

static gboolean
gst_ipc_slave_pipeline_post_message (GstElement * element, GstMessage * message)
{
  gst_ipc_slave_pipeline_forward_message (GST_IPC_SLAVE_PIPELINE
      (element), message);

  return GST_ELEMENT_CLASS (parent_class)->post_message (element, message);
}
