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
#include "gstmpdprograminformationnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDProgramInformationNode, gst_mpd_program_information_node,
    GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_program_information_node_finalize (GObject * object)
{
  GstMPDProgramInformationNode *self =
      GST_MPD_PROGRAM_INFORMATION_NODE (object);

  if (self->lang)
    xmlFree (self->lang);
  if (self->moreInformationURL)
    xmlFree (self->moreInformationURL);
  if (self->Title)
    xmlFree (self->Title);
  if (self->Source)
    xmlFree (self->Source);
  if (self->Copyright)
    xmlFree (self->Copyright);

  G_OBJECT_CLASS (gst_mpd_program_information_node_parent_class)->finalize
      (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_program_information_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr program_info_xml_node = NULL;
  xmlNodePtr child_node = NULL;
  GstMPDProgramInformationNode *self = GST_MPD_PROGRAM_INFORMATION_NODE (node);

  program_info_xml_node = xmlNewNode (NULL, (xmlChar *) "ProgramInformation");

  if (self->lang)
    gst_xml_helper_set_prop_string (program_info_xml_node, "lang", self->lang);

  if (self->moreInformationURL)
    gst_xml_helper_set_prop_string (program_info_xml_node, "moreInformationURL",
        self->moreInformationURL);

  if (self->Title) {
    child_node = xmlNewNode (NULL, (xmlChar *) "Title");
    gst_xml_helper_set_content (child_node, self->Title);
    xmlAddChild (program_info_xml_node, child_node);
  }

  if (self->Source) {
    child_node = xmlNewNode (NULL, (xmlChar *) "Source");
    gst_xml_helper_set_content (child_node, self->Source);
    xmlAddChild (program_info_xml_node, child_node);
  }

  if (self->Copyright) {
    child_node = xmlNewNode (NULL, (xmlChar *) "Copyright");
    gst_xml_helper_set_content (child_node, self->Copyright);
    xmlAddChild (program_info_xml_node, child_node);
  }

  return program_info_xml_node;
}

static void
gst_mpd_program_information_node_class_init (GstMPDProgramInformationNodeClass *
    klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_program_information_node_finalize;

  m_klass->get_xml_node = gst_mpd_program_information_get_xml_node;
}

static void
gst_mpd_program_information_node_init (GstMPDProgramInformationNode * self)
{
  self->lang = NULL;
  self->moreInformationURL = NULL;
  self->Title = NULL;
  self->Source = NULL;
  self->Copyright = NULL;
}

GstMPDProgramInformationNode *
gst_mpd_program_information_node_new (void)
{
  GstMPDProgramInformationNode *ret;

  ret = g_object_new (GST_TYPE_MPD_PROGRAM_INFORMATION_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_program_information_node_free (GstMPDProgramInformationNode * self)
{
  if (self)
    gst_object_unref (self);
}
