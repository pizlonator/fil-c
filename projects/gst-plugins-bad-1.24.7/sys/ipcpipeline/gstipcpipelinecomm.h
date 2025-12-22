/* GStreamer
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerch <vincent.penquerch@collabora.co.uk>
 *
 * gstipcpipelinecomm.h:
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


#ifndef __GST_IPC_PIPELINE_COMM_H__
#define __GST_IPC_PIPELINE_COMM_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_FLOW_COMM_ERROR GST_FLOW_CUSTOM_ERROR_1

extern GQuark QUARK_ID;

typedef enum {
  GST_IPC_PIPELINE_COMM_STATE_TYPE = 0,
  /* for the rest of the states we use directly the data type enums below */
} GstIpcPipelineCommState;

typedef enum {
  /* reply types */
  GST_IPC_PIPELINE_COMM_DATA_TYPE_ACK = 1,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY_RESULT,
  /* data send types */
  GST_IPC_PIPELINE_COMM_DATA_TYPE_BUFFER,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_EVENT,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_SINK_MESSAGE_EVENT,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_CHANGE,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_LOST,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_MESSAGE,
  GST_IPC_PIPELINE_COMM_DATA_TYPE_GERROR_MESSAGE,
} GstIpcPipelineCommDataType;

typedef struct
{
  GstElement *element;

  GMutex mutex;
  int fdin;
  int fdout;
  GHashTable *waiting_ids;

  GThread *reader_thread;
  GstPoll *poll;
  GstPollFD pollFDin;

  GstAdapter *adapter;
  guint8 state;
  guint32 send_id;

  guint32 payload_length;
  guint32 id;

  guint read_chunk_size;
  GstClockTime ack_time;

  void (*on_buffer) (guint32, GstBuffer *, gpointer);
  void (*on_event) (guint32, GstEvent *, gboolean, gpointer);
  void (*on_query) (guint32, GstQuery *, gboolean, gpointer);
  void (*on_state_change) (guint32, GstStateChange, gpointer);
  void (*on_state_lost) (gpointer);
  void (*on_message) (guint32, GstMessage *, gpointer);
  gpointer user_data;

} GstIpcPipelineComm;

void gst_ipc_pipeline_comm_plugin_init (void);

void gst_ipc_pipeline_comm_init (GstIpcPipelineComm *comm, GstElement *e);
void gst_ipc_pipeline_comm_clear (GstIpcPipelineComm *comm);
void gst_ipc_pipeline_comm_cancel (GstIpcPipelineComm * comm,
    gboolean flushing);

void gst_ipc_pipeline_comm_write_flow_ack_to_fd (GstIpcPipelineComm * comm,
    guint32 id, GstFlowReturn ret);
void gst_ipc_pipeline_comm_write_boolean_ack_to_fd (GstIpcPipelineComm * comm,
    guint32 id, gboolean ret);
void gst_ipc_pipeline_comm_write_state_change_ack_to_fd (
    GstIpcPipelineComm * comm, guint32 id, GstStateChangeReturn ret);

void gst_ipc_pipeline_comm_write_query_result_to_fd (GstIpcPipelineComm * comm,
    guint32 id, gboolean result, GstQuery *query);

GstFlowReturn gst_ipc_pipeline_comm_write_buffer_to_fd (
    GstIpcPipelineComm * comm, GstBuffer * buffer);
gboolean gst_ipc_pipeline_comm_write_event_to_fd (GstIpcPipelineComm * comm,
    gboolean upstream, GstEvent * event);
gboolean gst_ipc_pipeline_comm_write_query_to_fd (GstIpcPipelineComm * comm,
    gboolean upstream, GstQuery * query);
GstStateChangeReturn gst_ipc_pipeline_comm_write_state_change_to_fd (
    GstIpcPipelineComm * comm, GstStateChange transition);
void gst_ipc_pipeline_comm_write_state_lost_to_fd (GstIpcPipelineComm * comm);
gboolean gst_ipc_pipeline_comm_write_message_to_fd (GstIpcPipelineComm * comm,
    GstMessage *message);

gboolean gst_ipc_pipeline_comm_start_reader_thread (GstIpcPipelineComm * comm,
    void (*on_buffer) (guint32, GstBuffer *, gpointer),
    void (*on_event) (guint32, GstEvent *, gboolean, gpointer),
    void (*on_query) (guint32, GstQuery *, gboolean, gpointer),
    void (*on_state_change) (guint32, GstStateChange, gpointer),
    void (*on_state_lost) (gpointer),
    void (*on_message) (guint32, GstMessage *, gpointer),
    gpointer user_data);
void gst_ipc_pipeline_comm_stop_reader_thread (GstIpcPipelineComm * comm);

G_END_DECLS

#endif

