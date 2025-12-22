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
#include "gstmpdrepresentationbasenode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDRepresentationBaseNode, gst_mpd_representation_base_node,
    GST_TYPE_MPD_NODE);

enum
{
  PROP_MPD_REPRESENTATION_BASE_0 = 100,
  PROP_MPD_REPRESENTATION_BASE_PROFILES,
  PROP_MPD_REPRESENTATION_BASE_WIDTH,
  PROP_MPD_REPRESENTATION_BASE_HEIGHT,
  PROP_MPD_REPRESENTATION_BASE_SAR,
  PROP_MPD_REPRESENTATION_BASE_MIN_FRAME_RATE,
  PROP_MPD_REPRESENTATION_BASE_MAX_FRAME_RATE,
  PROP_MPD_REPRESENTATION_BASE_FRAME_RATE,
  PROP_MPD_REPRESENTATION_BASE_AUDIO_SAMPLING_RATE,
  PROP_MPD_REPRESENTATION_BASE_MIMETYPE,
  PROP_MPD_REPRESENTATION_BASE_SEGMENT_PROFILES,
  PROP_MPD_REPRESENTATION_BASE_CODECS,
  PROP_MPD_REPRESENTATION_BASE_MAX_SAP_PERIOD,
  PROP_MPD_REPRESENTATION_BASE_START_WITH_SAP,
  PROP_MPD_REPRESENTATION_BASE_MAX_PLAYOUT_RATE,
  PROP_MPD_REPRESENTATION_BASE_CODING_DEPENDENCY,
  PROP_MPD_REPRESENTATION_BASE_SCAN_TYPE,
};

/* GObject VMethods */

static void
gst_mpd_representation_base_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDRepresentationBaseNode *self =
      GST_MPD_REPRESENTATION_BASE_NODE (object);
  switch (prop_id) {
    case PROP_MPD_REPRESENTATION_BASE_PROFILES:
      g_free (self->profiles);
      self->profiles = g_value_dup_string (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_WIDTH:
      self->width = g_value_get_uint (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_HEIGHT:
      self->height = g_value_get_uint (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SAR:
      g_slice_free (GstXMLRatio, self->sar);
      self->sar = gst_xml_helper_clone_ratio (g_value_get_pointer (value));
      break;
    case PROP_MPD_REPRESENTATION_BASE_MIN_FRAME_RATE:
      g_slice_free (GstXMLFrameRate, self->minFrameRate);
      self->minFrameRate =
          gst_xml_helper_clone_frame_rate (g_value_get_pointer (value));
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_FRAME_RATE:
      g_slice_free (GstXMLFrameRate, self->maxFrameRate);
      self->maxFrameRate =
          gst_xml_helper_clone_frame_rate (g_value_get_pointer (value));
      break;
    case PROP_MPD_REPRESENTATION_BASE_FRAME_RATE:
      g_slice_free (GstXMLFrameRate, self->frameRate);
      self->frameRate =
          gst_xml_helper_clone_frame_rate (g_value_get_pointer (value));
      break;
    case PROP_MPD_REPRESENTATION_BASE_AUDIO_SAMPLING_RATE:
      g_free (self->audioSamplingRate);
      self->audioSamplingRate =
          g_strdup_printf ("%u", g_value_get_uint (value));
      break;
    case PROP_MPD_REPRESENTATION_BASE_MIMETYPE:
      g_free (self->mimeType);
      self->mimeType = g_value_dup_string (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SEGMENT_PROFILES:
      g_free (self->segmentProfiles);
      self->segmentProfiles = g_value_dup_string (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_CODECS:
      g_free (self->codecs);
      self->codecs = g_value_dup_string (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_SAP_PERIOD:
      self->maximumSAPPeriod = g_value_get_double (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_START_WITH_SAP:
      self->startWithSAP = g_value_get_int (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_PLAYOUT_RATE:
      self->maxPlayoutRate = g_value_get_double (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_CODING_DEPENDENCY:
      self->codingDependency = g_value_get_boolean (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SCAN_TYPE:
      g_free (self->scanType);
      self->scanType = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_representation_base_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDRepresentationBaseNode *self =
      GST_MPD_REPRESENTATION_BASE_NODE (object);
  switch (prop_id) {
    case PROP_MPD_REPRESENTATION_BASE_PROFILES:
      g_value_set_string (value, self->profiles);
      break;
    case PROP_MPD_REPRESENTATION_BASE_WIDTH:
      g_value_set_uint (value, self->width);
      break;
    case PROP_MPD_REPRESENTATION_BASE_HEIGHT:
      g_value_set_uint (value, self->height);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SAR:
      g_value_set_pointer (value, self->sar);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MIN_FRAME_RATE:
      g_value_set_pointer (value, self->minFrameRate);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_FRAME_RATE:
      g_value_set_pointer (value, self->maxFrameRate);
      break;
    case PROP_MPD_REPRESENTATION_BASE_FRAME_RATE:
      g_value_set_pointer (value, self->frameRate);
      break;
    case PROP_MPD_REPRESENTATION_BASE_AUDIO_SAMPLING_RATE:
      g_value_set_uint (value, atoi (self->audioSamplingRate));
      break;
    case PROP_MPD_REPRESENTATION_BASE_MIMETYPE:
      g_value_set_string (value, self->mimeType);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SEGMENT_PROFILES:
      g_value_set_string (value, self->segmentProfiles);
      break;
    case PROP_MPD_REPRESENTATION_BASE_CODECS:
      g_value_set_string (value, self->codecs);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_SAP_PERIOD:
      g_value_set_double (value, self->maximumSAPPeriod);
      break;
    case PROP_MPD_REPRESENTATION_BASE_START_WITH_SAP:
      g_value_set_int (value, self->startWithSAP);
      break;
    case PROP_MPD_REPRESENTATION_BASE_MAX_PLAYOUT_RATE:
      g_value_set_double (value, self->maxPlayoutRate);
      break;
    case PROP_MPD_REPRESENTATION_BASE_CODING_DEPENDENCY:
      g_value_set_boolean (value, self->codingDependency);
      self->codingDependency = g_value_get_boolean (value);
      break;
    case PROP_MPD_REPRESENTATION_BASE_SCAN_TYPE:
      g_value_set_string (value, self->scanType);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_representation_base_node_finalize (GObject * object)
{
  GstMPDRepresentationBaseNode *self =
      GST_MPD_REPRESENTATION_BASE_NODE (object);

  if (self->profiles)
    xmlFree (self->profiles);
  g_slice_free (GstXMLRatio, self->sar);
  g_slice_free (GstXMLFrameRate, self->frameRate);
  g_slice_free (GstXMLFrameRate, self->minFrameRate);
  g_slice_free (GstXMLFrameRate, self->maxFrameRate);
  if (self->audioSamplingRate)
    xmlFree (self->audioSamplingRate);
  if (self->mimeType)
    xmlFree (self->mimeType);
  if (self->segmentProfiles)
    xmlFree (self->segmentProfiles);
  if (self->codecs)
    xmlFree (self->codecs);
  if (self->scanType)
    xmlFree (self->scanType);
  g_list_free_full (self->FramePacking,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->AudioChannelConfiguration,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->ContentProtection,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);

  G_OBJECT_CLASS (gst_mpd_representation_base_node_parent_class)->finalize
      (object);
}

/* Base class */

static void
gst_mpd_representation_base_get_xml_node (GstMPDNode * node,
    xmlNodePtr representation_base_node)
{
  GstMPDRepresentationBaseNode *self = GST_MPD_REPRESENTATION_BASE_NODE (node);

  if (self->profiles)
    gst_xml_helper_set_prop_string (representation_base_node, "profiles",
        self->profiles);
  if (self->width)
    gst_xml_helper_set_prop_uint (representation_base_node, "width",
        self->width);
  if (self->height)
    gst_xml_helper_set_prop_uint (representation_base_node, "height",
        self->height);

  gst_xml_helper_set_prop_ratio (representation_base_node, "sar", self->sar);
  gst_xml_helper_set_prop_framerate (representation_base_node, "minFrameRate",
      self->minFrameRate);
  gst_xml_helper_set_prop_framerate (representation_base_node, "maxFrameRate",
      self->maxFrameRate);
  gst_xml_helper_set_prop_framerate (representation_base_node, "frameRate",
      self->frameRate);

  gst_xml_helper_set_prop_string (representation_base_node,
      "audioSamplingRate", self->audioSamplingRate);
  gst_xml_helper_set_prop_string (representation_base_node, "mimeType",
      self->mimeType);
  gst_xml_helper_set_prop_string (representation_base_node, "segmentProfiles",
      self->segmentProfiles);
  gst_xml_helper_set_prop_string (representation_base_node, "codecs",
      self->codecs);
  if (self->maximumSAPPeriod)
    gst_xml_helper_set_prop_double (representation_base_node,
        "maximumSAPPeriod", self->maximumSAPPeriod);
  if (self->startWithSAP)
    gst_xml_helper_set_prop_int (representation_base_node, "startWithSAP",
        self->startWithSAP);
  if (self->maxPlayoutRate)
    gst_xml_helper_set_prop_double (representation_base_node, "maxPlayoutRate",
        self->maxPlayoutRate);
  if (self->codingDependency)
    gst_xml_helper_set_prop_boolean (representation_base_node,
        "codingDependency", self->codingDependency);

  gst_xml_helper_set_prop_string (representation_base_node, "scanType",
      self->scanType);

  g_list_foreach (self->FramePacking,
      gst_mpd_node_get_list_item, representation_base_node);
  g_list_foreach (self->AudioChannelConfiguration,
      gst_mpd_node_get_list_item, representation_base_node);
  g_list_foreach (self->ContentProtection,
      gst_mpd_node_get_list_item, representation_base_node);
}

static void
gst_mpd_representation_base_node_class_init (GstMPDRepresentationBaseNodeClass *
    klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_mpd_representation_base_node_finalize;
  object_class->set_property = gst_mpd_representation_base_node_set_property;
  object_class->get_property = gst_mpd_representation_base_node_get_property;


  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BASE_WIDTH, g_param_spec_uint ("width",
          "width", "representation width", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BASE_HEIGHT, g_param_spec_uint ("height",
          "height", "representation height", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BASE_MIMETYPE, g_param_spec_string ("mime-type",
          "mimetype", "representation mimetype", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BASE_CODECS, g_param_spec_string ("codecs",
          "codecs", "representation codec", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BASE_AUDIO_SAMPLING_RATE,
      g_param_spec_uint ("audio-sampling-rate", "audio sampling rate",
          "representation audio sampling rate", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mpd_representation_base_node_init (GstMPDRepresentationBaseNode * self)
{
  self->profiles = NULL;
  self->width = 0;
  self->height = 0;
  self->sar = NULL;
  self->minFrameRate = NULL;
  self->maxFrameRate = NULL;
  self->frameRate = NULL;
  self->audioSamplingRate = NULL;
  self->mimeType = NULL;
  self->segmentProfiles = NULL;
  self->codecs = NULL;
  self->maximumSAPPeriod = 0;
  self->startWithSAP = GST_SAP_TYPE_0;
  self->maxPlayoutRate = 0.0;
  self->codingDependency = FALSE;
  self->scanType = NULL;
  self->FramePacking = NULL;
  self->AudioChannelConfiguration = NULL;
  self->ContentProtection = NULL;
}

void
gst_mpd_representation_base_node_get_list_item (gpointer data,
    gpointer user_data)
{
  GstMPDNode *node = (GstMPDNode *) data;
  xmlNodePtr parent_xml_node = (xmlNodePtr) user_data;
  xmlNodePtr new_xml_node = gst_mpd_node_get_xml_pointer (node);

  gst_mpd_representation_base_get_xml_node (node, new_xml_node);
  xmlAddChild (parent_xml_node, new_xml_node);
}
