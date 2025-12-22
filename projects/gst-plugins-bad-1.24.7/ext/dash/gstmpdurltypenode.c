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
#include "gstmpdurltypenode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDURLTypeNode, gst_mpd_url_type_node, GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_url_type_node_finalize (GObject * object)
{
  GstMPDURLTypeNode *self = GST_MPD_URL_TYPE_NODE (object);

  if (self->sourceURL)
    xmlFree (self->sourceURL);
  g_slice_free (GstXMLRange, self->range);
  g_free (self->node_name);

  G_OBJECT_CLASS (gst_mpd_url_type_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_url_type_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr url_type_xml_node = NULL;
  GstMPDURLTypeNode *self = GST_MPD_URL_TYPE_NODE (node);

  url_type_xml_node = xmlNewNode (NULL, (xmlChar *) self->node_name);

  gst_xml_helper_set_prop_string (url_type_xml_node, "sourceURL",
      self->sourceURL);
  gst_xml_helper_set_prop_range (url_type_xml_node, "range", self->range);

  return url_type_xml_node;
}

static void
gst_mpd_url_type_node_class_init (GstMPDURLTypeNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_url_type_node_finalize;

  m_klass->get_xml_node = gst_mpd_url_type_get_xml_node;
}

static void
gst_mpd_url_type_node_init (GstMPDURLTypeNode * self)
{
  self->node_name = NULL;
  self->sourceURL = NULL;
  self->range = NULL;
}

GstMPDURLTypeNode *
gst_mpd_url_type_node_new (const gchar * name)
{
  GstMPDURLTypeNode *self = g_object_new (GST_TYPE_MPD_URL_TYPE_NODE, NULL);
  self->node_name = g_strdup (name);
  gst_object_ref_sink (self);
  return self;
}

void
gst_mpd_url_type_node_free (GstMPDURLTypeNode * self)
{
  if (self)
    gst_object_unref (self);
}

GstMPDURLTypeNode *
gst_mpd_url_type_node_clone (GstMPDURLTypeNode * url)
{

  GstMPDURLTypeNode *clone = NULL;

  if (url) {
    clone = gst_mpd_url_type_node_new (url->node_name);
    if (url->sourceURL) {
      clone->sourceURL = xmlMemStrdup (url->sourceURL);
    }
    clone->range = gst_xml_helper_clone_range (url->range);
  }

  return clone;
}
