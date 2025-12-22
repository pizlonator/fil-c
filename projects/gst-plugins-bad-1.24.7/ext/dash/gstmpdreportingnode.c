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
#include "gstmpdreportingnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDReportingNode, gst_mpd_reporting_node, GST_TYPE_MPD_NODE);

/* Base class */

static xmlNodePtr
gst_mpd_reporting_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr reporting_xml_node = NULL;

  reporting_xml_node = xmlNewNode (NULL, (xmlChar *) "Reporting");

  return reporting_xml_node;
}

static void
gst_mpd_reporting_node_class_init (GstMPDReportingNodeClass * klass)
{
  GstMPDNodeClass *m_klass;

  m_klass = GST_MPD_NODE_CLASS (klass);

  m_klass->get_xml_node = gst_mpd_reporting_get_xml_node;
}

static void
gst_mpd_reporting_node_init (GstMPDReportingNode * self)
{
}

GstMPDReportingNode *
gst_mpd_reporting_node_new (void)
{
  GstMPDReportingNode *ret;

  ret = g_object_new (GST_TYPE_MPD_REPORTING_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_reporting_node_free (GstMPDReportingNode * self)
{
  if (self)
    gst_object_unref (self);
}
