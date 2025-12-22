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
#include "gstmpdperiodnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDPeriodNode, gst_mpd_period_node, GST_TYPE_MPD_NODE);

enum
{
  PROP_MPD_PERIOD_0,
  PROP_MPD_PERIOD_ID,
  PROP_MPD_PERIOD_START,
  PROP_MPD_PERIOD_DURATION,
  PROP_MPD_PERIOD_BITSTREAM_SWITCHING,
};

/* GObject VMethods */

static void
gst_mpd_period_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDPeriodNode *self = GST_MPD_PERIOD_NODE (object);
  switch (prop_id) {
    case PROP_MPD_PERIOD_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_MPD_PERIOD_START:
      self->start = g_value_get_uint64 (value);
      break;
    case PROP_MPD_PERIOD_DURATION:
      self->duration = g_value_get_uint64 (value);
      break;
    case PROP_MPD_PERIOD_BITSTREAM_SWITCHING:
      self->bitstreamSwitching = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_period_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDPeriodNode *self = GST_MPD_PERIOD_NODE (object);
  switch (prop_id) {
    case PROP_MPD_PERIOD_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_MPD_PERIOD_START:
      g_value_set_uint64 (value, self->start);
      break;
    case PROP_MPD_PERIOD_DURATION:
      g_value_set_uint64 (value, self->duration);
      break;
    case PROP_MPD_PERIOD_BITSTREAM_SWITCHING:
      g_value_set_boolean (value, self->bitstreamSwitching);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_period_node_finalize (GObject * object)
{
  GstMPDPeriodNode *self = GST_MPD_PERIOD_NODE (object);

  if (self->id)
    xmlFree (self->id);
  gst_mpd_segment_base_node_free (self->SegmentBase);
  gst_mpd_segment_list_node_free (self->SegmentList);
  gst_mpd_segment_template_node_free (self->SegmentTemplate);
  g_list_free_full (self->AdaptationSets,
      (GDestroyNotify) gst_mpd_adaptation_set_node_free);
  g_list_free_full (self->Subsets, (GDestroyNotify) gst_mpd_subset_node_free);
  g_list_free_full (self->BaseURLs, (GDestroyNotify) gst_mpd_baseurl_node_free);
  if (self->xlink_href)
    xmlFree (self->xlink_href);

  G_OBJECT_CLASS (gst_mpd_period_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_period_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr period_xml_node = NULL;
  GstMPDPeriodNode *self = GST_MPD_PERIOD_NODE (node);

  period_xml_node = xmlNewNode (NULL, (xmlChar *) "Period");

  if (self->id)
    gst_xml_helper_set_prop_string (period_xml_node, "id", self->id);

  gst_xml_helper_set_prop_duration (period_xml_node, "start", self->start);
  gst_xml_helper_set_prop_duration (period_xml_node, "duration",
      self->duration);
  gst_xml_helper_set_prop_boolean (period_xml_node, "bitstreamSwitching",
      self->bitstreamSwitching);

  if (self->SegmentBase)
    gst_mpd_node_add_child_node (GST_MPD_NODE (self->SegmentBase),
        period_xml_node);

  if (self->SegmentList)
    gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
        (self->SegmentList), period_xml_node);

  if (self->SegmentTemplate)
    gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
        (self->SegmentTemplate), period_xml_node);

  g_list_foreach (self->AdaptationSets,
      gst_mpd_representation_base_node_get_list_item, period_xml_node);
  g_list_foreach (self->Subsets, gst_mpd_node_get_list_item, period_xml_node);
  g_list_foreach (self->BaseURLs, gst_mpd_node_get_list_item, period_xml_node);


  return period_xml_node;
}

static void
gst_mpd_period_node_class_init (GstMPDPeriodNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;


  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_period_node_finalize;
  object_class->set_property = gst_mpd_period_node_set_property;
  object_class->get_property = gst_mpd_period_node_get_property;

  m_klass->get_xml_node = gst_mpd_period_get_xml_node;

  g_object_class_install_property (object_class, PROP_MPD_PERIOD_ID,
      g_param_spec_string ("id", "id",
          "unique id for period", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MPD_PERIOD_START,
      g_param_spec_uint64 ("start", "Period start",
          "Period start",
          0, G_MAXUINT64, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_MPD_PERIOD_DURATION,
      g_param_spec_uint64 ("duration", "period duration",
          "Period duration",
          0, G_MAXUINT64, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class,
      PROP_MPD_PERIOD_BITSTREAM_SWITCHING,
      g_param_spec_boolean ("bitstream-switching", "Bitstream switching",
          "Bitstream switching", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_mpd_period_node_init (GstMPDPeriodNode * self)
{
  self->id = NULL;
  self->start = 0;              /* [ms] */
  self->duration = 0;           /* [ms] */
  self->bitstreamSwitching = 0;
  self->SegmentBase = NULL;
  self->SegmentList = NULL;
  self->SegmentTemplate = NULL;
  self->AdaptationSets = NULL;
  self->Subsets = NULL;
  self->BaseURLs = NULL;
  self->xlink_href = NULL;
  self->actuate = 0;
}

GstMPDPeriodNode *
gst_mpd_period_node_new (void)
{
  GstMPDPeriodNode *ret;

  ret = g_object_new (GST_TYPE_MPD_PERIOD_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_period_node_free (GstMPDPeriodNode * self)
{
  if (self)
    gst_object_unref (self);
}
