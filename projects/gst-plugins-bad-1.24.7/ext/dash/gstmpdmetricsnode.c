/* GStreamer
 *
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */
#include "gstmpdmetricsnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDMetricsNode, gst_mpd_metrics_node, GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_metrics_node_finalize (GObject * object)
{
  GstMPDMetricsNode *self = GST_MPD_METRICS_NODE (object);

  g_free (self->metrics);
  g_list_free_full (self->MetricsRanges,
      (GDestroyNotify) gst_mpd_metrics_range_node_free);

  G_OBJECT_CLASS (gst_mpd_metrics_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_metrics_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr metrics_xml_node = NULL;
  GstMPDMetricsNode *self = GST_MPD_METRICS_NODE (node);

  metrics_xml_node = xmlNewNode (NULL, (xmlChar *) "Metrics");

  if (self->metrics)
    gst_xml_helper_set_prop_string (metrics_xml_node, "metrics", self->metrics);

  g_list_foreach (self->Reportings, gst_mpd_node_get_list_item,
      metrics_xml_node);
  g_list_foreach (self->MetricsRanges, gst_mpd_node_get_list_item,
      metrics_xml_node);

  return metrics_xml_node;
}

static void
gst_mpd_metrics_node_class_init (GstMPDMetricsNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_metrics_node_finalize;

  m_klass->get_xml_node = gst_mpd_metrics_get_xml_node;
}

static void
gst_mpd_metrics_node_init (GstMPDMetricsNode * self)
{
  self->metrics = NULL;
  self->MetricsRanges = NULL;
  self->Reportings = NULL;
}

GstMPDMetricsNode *
gst_mpd_metrics_node_new (void)
{
  GstMPDMetricsNode *ret;
  ret = g_object_new (GST_TYPE_MPD_METRICS_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_metrics_node_free (GstMPDMetricsNode * self)
{
  if (self)
    gst_object_unref (self);
}
