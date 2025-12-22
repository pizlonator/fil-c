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
#include "gstmpddescriptortypenode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDDescriptorTypeNode, gst_mpd_descriptor_type_node,
    GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_descriptor_type_node_finalize (GObject * object)
{
  GstMPDDescriptorTypeNode *self = GST_MPD_DESCRIPTOR_TYPE_NODE (object);

  if (self->schemeIdUri)
    xmlFree (self->schemeIdUri);
  if (self->value)
    xmlFree (self->value);
  g_free (self->node_name);

  G_OBJECT_CLASS (gst_mpd_descriptor_type_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_descriptor_type_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr descriptor_type_xml_node = NULL;
  GstMPDDescriptorTypeNode *self = GST_MPD_DESCRIPTOR_TYPE_NODE (node);

  descriptor_type_xml_node = xmlNewNode (NULL, (xmlChar *) self->node_name);

  gst_xml_helper_set_prop_string (descriptor_type_xml_node, "schemeIdUri",
      self->schemeIdUri);

  gst_xml_helper_set_prop_string (descriptor_type_xml_node, "value",
      self->value);

  return descriptor_type_xml_node;
}

static void
gst_mpd_descriptor_type_node_class_init (GstMPDDescriptorTypeNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_descriptor_type_node_finalize;

  m_klass->get_xml_node = gst_mpd_descriptor_type_get_xml_node;
}

static void
gst_mpd_descriptor_type_node_init (GstMPDDescriptorTypeNode * self)
{
  if (self->schemeIdUri)
    xmlFree (self->schemeIdUri);
  if (self->value)
    xmlFree (self->value);
}

GstMPDDescriptorTypeNode *
gst_mpd_descriptor_type_node_new (const gchar * name)
{
  GstMPDDescriptorTypeNode *self =
      g_object_new (GST_TYPE_MPD_DESCRIPTOR_TYPE_NODE, NULL);
  self->node_name = g_strdup (name);
  gst_object_ref_sink (self);
  return self;
}

void
gst_mpd_descriptor_type_node_free (GstMPDDescriptorTypeNode * self)
{
  if (self)
    gst_object_unref (self);
}
