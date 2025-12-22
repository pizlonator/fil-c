/* GStreamer Round Robin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-roundrobin
 * @title: roundrobin
 *
 * This is a generic element that will distribute equally incoming
 * buffers over multiple src pads. This is the opposite of tee
 * element, which duplicates buffers over all pads. This element 
 * can be used to distrute load across multiple branches when the buffer
 * can be processed independently.
 */

#include "gstroundrobin.h"

GST_DEBUG_CATEGORY_STATIC (gst_round_robin_debug);
#define GST_CAT_DEFAULT gst_round_robin_debug

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src_%d",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("ANY"));

struct _GstRoundRobin
{
  GstElement parent;
  gint index;
};

G_DEFINE_TYPE_WITH_CODE (GstRoundRobin, gst_round_robin,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_round_robin_debug,
        "roundrobin", 0, "Round Robin"));
GST_ELEMENT_REGISTER_DEFINE (roundrobin, "roundrobin", GST_RANK_NONE,
    GST_TYPE_ROUND_ROBIN);

static GstFlowReturn
gst_round_robin_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstRoundRobin *disp = (GstRoundRobin *) parent;
  GstElement *elem = (GstElement *) parent;
  GstPad *src_pad = NULL;
  GstFlowReturn ret;

  GST_OBJECT_LOCK (disp);
  if (disp->index >= elem->numsrcpads)
    disp->index = 0;

  src_pad = g_list_nth_data (elem->srcpads, disp->index);

  if (src_pad) {
    gst_object_ref (src_pad);
    disp->index += 1;
  }
  GST_OBJECT_UNLOCK (disp);

  if (!src_pad)
    /* no pad, that's fine */
    return GST_FLOW_OK;

  ret = gst_pad_push (src_pad, buffer);
  gst_object_unref (src_pad);

  return ret;
}

static GstPad *
gst_round_robin_request_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstPad *pad;

  pad = gst_element_get_static_pad (element, name);
  if (pad) {
    gst_object_unref (pad);
    return NULL;
  }

  pad = gst_pad_new_from_static_template (&src_templ, name);
  gst_element_add_pad (element, pad);

  return pad;
}

static void
gst_round_robin_init (GstRoundRobin * disp)
{
  GstPad *pad;

  gst_element_create_all_pads (GST_ELEMENT (disp));
  pad = GST_PAD (GST_ELEMENT (disp)->sinkpads->data);

  GST_PAD_SET_PROXY_CAPS (pad);
  GST_PAD_SET_PROXY_SCHEDULING (pad);
  /* do not proxy allocation, it requires special handling like tee does */

  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (gst_round_robin_chain));
}

static void
gst_round_robin_class_init (GstRoundRobinClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;

  gst_element_class_set_metadata (element_class,
      "Round Robin", "Source/Network",
      "A round robin dispatcher element.",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com");

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_round_robin_request_pad);
}
