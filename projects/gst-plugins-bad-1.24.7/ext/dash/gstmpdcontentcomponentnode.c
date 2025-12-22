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
#include "gstmpdcontentcomponentnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDContentComponentNode, gst_mpd_content_component_node,
    GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_content_component_node_finalize (GObject * object)
{
  GstMPDContentComponentNode *self = GST_MPD_CONTENT_COMPONENT_NODE (object);

  if (self->lang)
    xmlFree (self->lang);
  if (self->contentType)
    xmlFree (self->contentType);
  g_slice_free (GstXMLRatio, self->par);
  g_list_free_full (self->Accessibility,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Role,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Rating,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Viewpoint,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);

  G_OBJECT_CLASS (gst_mpd_content_component_node_parent_class)->finalize
      (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_content_component_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr content_component_xml_node = NULL;
  GstMPDContentComponentNode *self = GST_MPD_CONTENT_COMPONENT_NODE (node);
  content_component_xml_node =
      xmlNewNode (NULL, (xmlChar *) "ContentComponent");

  gst_xml_helper_set_prop_uint (content_component_xml_node, "id", self->id);
  gst_xml_helper_set_prop_string (content_component_xml_node, "lang",
      self->lang);
  gst_xml_helper_set_prop_string (content_component_xml_node, "contentType",
      self->contentType);
  gst_xml_helper_set_prop_ratio (content_component_xml_node, "par", self->par);

  g_list_foreach (self->Accessibility, gst_mpd_node_get_list_item,
      content_component_xml_node);
  g_list_foreach (self->Role, gst_mpd_node_get_list_item,
      content_component_xml_node);
  g_list_foreach (self->Rating, gst_mpd_node_get_list_item,
      content_component_xml_node);
  g_list_foreach (self->Viewpoint, gst_mpd_node_get_list_item,
      content_component_xml_node);

  return content_component_xml_node;
}

static void
gst_mpd_content_component_node_class_init (GstMPDContentComponentNodeClass *
    klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_content_component_node_finalize;

  m_klass->get_xml_node = gst_mpd_content_component_get_xml_node;
}

static void
gst_mpd_content_component_node_init (GstMPDContentComponentNode * self)
{
  self->id = 0;
  self->lang = NULL;
  self->contentType = NULL;
  self->par = 0;
  self->Accessibility = 0;
  self->Role = NULL;
  self->Rating = NULL;
  self->Viewpoint = NULL;
}

GstMPDContentComponentNode *
gst_mpd_content_component_node_new (void)
{
  GstMPDContentComponentNode *ret;

  ret = g_object_new (GST_TYPE_MPD_CONTENT_COMPONENT_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_content_component_node_free (GstMPDContentComponentNode * self)
{
  if (self)
    gst_object_unref (self);
}
