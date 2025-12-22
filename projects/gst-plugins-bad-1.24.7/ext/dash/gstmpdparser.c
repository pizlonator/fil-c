/*
 * DASH MPD parsing library
 *
 * gstmpdparser.c
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * Authors:
 *   Gianluca Gennari <gennarone@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include "gstmpdparser.h"
#include "gstdash_debug.h"

#define GST_CAT_DEFAULT gst_dash_demux_debug


/* XML node parsing */
static void gst_mpdparser_parse_baseURL_node (GList ** list, xmlNode * a_node);
static void gst_mpdparser_parse_descriptor_type (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_content_component_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_location_node (GList ** list, xmlNode * a_node);
static void gst_mpdparser_parse_subrepresentation_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_segment_url_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_url_type_node (GstMPDURLTypeNode ** pointer,
    xmlNode * a_node);
static void gst_mpdparser_parse_seg_base_type_ext (GstMPDSegmentBaseNode **
    pointer, xmlNode * a_node, GstMPDSegmentBaseNode * parent);
static void gst_mpdparser_parse_s_node (GQueue * queue, xmlNode * a_node);
static void gst_mpdparser_parse_segment_timeline_node (GstMPDSegmentTimelineNode
    ** pointer, xmlNode * a_node);
static gboolean
gst_mpdparser_parse_mult_seg_base_node (GstMPDMultSegmentBaseNode *
    pointer, xmlNode * a_node, GstMPDMultSegmentBaseNode * parent);
static gboolean gst_mpdparser_parse_segment_list_node (GstMPDSegmentListNode **
    pointer, xmlNode * a_node, GstMPDSegmentListNode * parent);
static void
gst_mpdparser_parse_representation_base (GstMPDRepresentationBaseNode *
    pointer, xmlNode * a_node);
static gboolean gst_mpdparser_parse_representation_node (GList ** list,
    xmlNode * a_node, GstMPDAdaptationSetNode * parent,
    GstMPDPeriodNode * period_node);
static gboolean gst_mpdparser_parse_adaptation_set_node (GList ** list,
    xmlNode * a_node, GstMPDPeriodNode * parent);
static void gst_mpdparser_parse_subset_node (GList ** list, xmlNode * a_node);
static gboolean
gst_mpdparser_parse_segment_template_node (GstMPDSegmentTemplateNode ** pointer,
    xmlNode * a_node, GstMPDSegmentTemplateNode * parent);
static gboolean gst_mpdparser_parse_period_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_program_info_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_metrics_range_node (GList ** list,
    xmlNode * a_node);
static void gst_mpdparser_parse_metrics_node (GList ** list, xmlNode * a_node);
static gboolean gst_mpdparser_parse_root_node (GstMPDRootNode ** pointer,
    xmlNode * a_node);
static void gst_mpdparser_parse_utctiming_node (GList ** list,
    xmlNode * a_node);

/*
  Duration Data Type

  The duration data type is used to specify a time interval.

  The time interval is specified in the following form "-PnYnMnDTnHnMnS" where:

    * - indicates the negative sign (optional)
    * P indicates the period (required)
    * nY indicates the number of years
    * nM indicates the number of months
    * nD indicates the number of days
    * T indicates the start of a time section (required if you are going to specify hours, minutes, or seconds)
    * nH indicates the number of hours
    * nM indicates the number of minutes
    * nS indicates the number of seconds
*/




static void
gst_mpdparser_parse_baseURL_node (GList ** list, xmlNode * a_node)
{
  GstMPDBaseURLNode *new_base_url;

  new_base_url = gst_mpd_baseurl_node_new ();
  *list = g_list_append (*list, new_base_url);

  GST_LOG ("content of BaseURL node:");
  gst_xml_helper_get_node_content (a_node, &new_base_url->baseURL);

  GST_LOG ("attributes of BaseURL node:");
  gst_xml_helper_get_prop_string (a_node, "serviceLocation",
      &new_base_url->serviceLocation);
  gst_xml_helper_get_prop_string (a_node, "byteRange",
      &new_base_url->byteRange);
}

static void
gst_mpdparser_parse_descriptor_type (GList ** list, xmlNode * a_node)
{
  GstMPDDescriptorTypeNode *new_descriptor;

  new_descriptor =
      gst_mpd_descriptor_type_node_new ((const gchar *) a_node->name);
  *list = g_list_append (*list, new_descriptor);

  GST_LOG ("attributes of %s node:", a_node->name);
  gst_xml_helper_get_prop_string_stripped (a_node, "schemeIdUri",
      &new_descriptor->schemeIdUri);
  if (!gst_xml_helper_get_prop_string (a_node, "value", &new_descriptor->value)) {
    /* if no value attribute, use XML string representation of the node */
    gst_xml_helper_get_node_as_string (a_node, &new_descriptor->value);
  }
}

static void
gst_mpdparser_parse_content_component_node (GList ** list, xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDContentComponentNode *new_content_component;

  new_content_component = gst_mpd_content_component_node_new ();
  *list = g_list_append (*list, new_content_component);

  GST_LOG ("attributes of ContentComponent node:");
  gst_xml_helper_get_prop_unsigned_integer (a_node, "id", 0,
      &new_content_component->id);
  gst_xml_helper_get_prop_string (a_node, "lang", &new_content_component->lang);
  gst_xml_helper_get_prop_string (a_node, "contentType",
      &new_content_component->contentType);
  gst_xml_helper_get_prop_ratio (a_node, "par", &new_content_component->par);

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Accessibility") == 0) {
        gst_mpdparser_parse_descriptor_type
            (&new_content_component->Accessibility, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Role") == 0) {
        gst_mpdparser_parse_descriptor_type (&new_content_component->Role,
            cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Rating") == 0) {
        gst_mpdparser_parse_descriptor_type
            (&new_content_component->Rating, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Viewpoint") == 0) {
        gst_mpdparser_parse_descriptor_type
            (&new_content_component->Viewpoint, cur_node);
      }
    }
  }
}

static void
gst_mpdparser_parse_location_node (GList ** list, xmlNode * a_node)
{
  gchar *location = NULL;
  GstMPDLocationNode *locationNode;

  GST_LOG ("content of Location node:");
  if (gst_xml_helper_get_node_content (a_node, &location)) {
    locationNode = gst_mpd_location_node_new ();
    locationNode->location = location;
    *list = g_list_append (*list, locationNode);
  }
}

static void
gst_mpdparser_parse_subrepresentation_node (GList ** list, xmlNode * a_node)
{
  GstMPDSubRepresentationNode *new_subrep;

  new_subrep = gst_mpd_sub_representation_node_new ();
  *list = g_list_append (*list, new_subrep);

  GST_LOG ("attributes of SubRepresentation node:");
  gst_xml_helper_get_prop_unsigned_integer (a_node, "level", 0,
      &new_subrep->level);
  gst_xml_helper_get_prop_uint_vector_type (a_node, "dependencyLevel",
      &new_subrep->dependencyLevel, &new_subrep->dependencyLevel_size);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "bandwidth", 0,
      &new_subrep->bandwidth);
  gst_xml_helper_get_prop_string_vector_type (a_node,
      "contentComponent", &new_subrep->contentComponent);

  /* RepresentationBase extension */
  gst_mpdparser_parse_representation_base (GST_MPD_REPRESENTATION_BASE_NODE
      (new_subrep), a_node);
}



static void
gst_mpdparser_parse_segment_url_node (GList ** list, xmlNode * a_node)
{
  GstMPDSegmentURLNode *new_segment_url;

  new_segment_url = gst_mpd_segment_url_node_new ();
  *list = g_list_append (*list, new_segment_url);

  GST_LOG ("attributes of SegmentURL node:");
  gst_xml_helper_get_prop_string (a_node, "media", &new_segment_url->media);
  gst_xml_helper_get_prop_range (a_node, "mediaRange",
      &new_segment_url->mediaRange);
  gst_xml_helper_get_prop_string (a_node, "index", &new_segment_url->index);
  gst_xml_helper_get_prop_range (a_node, "indexRange",
      &new_segment_url->indexRange);
}

static void
gst_mpdparser_parse_url_type_node (GstMPDURLTypeNode ** pointer,
    xmlNode * a_node)
{
  GstMPDURLTypeNode *new_url_type;

  gst_mpd_url_type_node_free (*pointer);
  *pointer = new_url_type =
      gst_mpd_url_type_node_new ((const gchar *) a_node->name);

  GST_LOG ("attributes of URLType node:");
  gst_xml_helper_get_prop_string (a_node, "sourceURL",
      &new_url_type->sourceURL);
  gst_xml_helper_get_prop_range (a_node, "range", &new_url_type->range);
}

static void
gst_mpdparser_parse_seg_base_type_ext (GstMPDSegmentBaseNode ** pointer,
    xmlNode * a_node, GstMPDSegmentBaseNode * parent)
{
  xmlNode *cur_node;
  GstMPDSegmentBaseNode *seg_base_type;
  guint intval;
  guint64 int64val;
  gboolean boolval;
  GstXMLRange *rangeval;

  gst_mpd_segment_base_node_free (*pointer);
  *pointer = seg_base_type = gst_mpd_segment_base_node_new ();

  /* Initialize values that have defaults */
  seg_base_type->indexRangeExact = FALSE;
  seg_base_type->timescale = 1;

  /* Inherit attribute values from parent */
  if (parent) {
    seg_base_type->timescale = parent->timescale;
    seg_base_type->presentationTimeOffset = parent->presentationTimeOffset;
    seg_base_type->indexRange = gst_xml_helper_clone_range (parent->indexRange);
    seg_base_type->indexRangeExact = parent->indexRangeExact;
    seg_base_type->Initialization =
        gst_mpd_url_type_node_clone (parent->Initialization);
    seg_base_type->RepresentationIndex =
        gst_mpd_url_type_node_clone (parent->RepresentationIndex);
  }

  /* We must retrieve each value first to see if it exists.  If it does not
   * exist, we do not want to overwrite an inherited value */
  GST_LOG ("attributes of SegmentBaseType extension:");
  if (gst_xml_helper_get_prop_unsigned_integer (a_node, "timescale", 1,
          &intval)) {
    seg_base_type->timescale = intval;
  }
  if (gst_xml_helper_get_prop_unsigned_integer_64 (a_node,
          "presentationTimeOffset", 0, &int64val)) {
    seg_base_type->presentationTimeOffset = int64val;
  }
  if (gst_xml_helper_get_prop_range (a_node, "indexRange", &rangeval)) {
    if (seg_base_type->indexRange) {
      g_slice_free (GstXMLRange, seg_base_type->indexRange);
    }
    seg_base_type->indexRange = rangeval;
  }
  if (gst_xml_helper_get_prop_boolean (a_node, "indexRangeExact",
          FALSE, &boolval)) {
    seg_base_type->indexRangeExact = boolval;
  }

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Initialization") == 0 ||
          xmlStrcmp (cur_node->name, (xmlChar *) "Initialisation") == 0) {
        /* parse will free the previous pointer to create a new one */
        gst_mpdparser_parse_url_type_node (&seg_base_type->Initialization,
            cur_node);
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "RepresentationIndex") == 0) {
        /* parse will free the previous pointer to create a new one */
        gst_mpdparser_parse_url_type_node (&seg_base_type->RepresentationIndex,
            cur_node);
      }
    }
  }
}



static void
gst_mpdparser_parse_s_node (GQueue * queue, xmlNode * a_node)
{
  GstMPDSNode *new_s_node;

  new_s_node = gst_mpd_s_node_new ();
  g_queue_push_tail (queue, new_s_node);

  GST_LOG ("attributes of S node:");
  gst_xml_helper_get_prop_unsigned_integer_64 (a_node, "t", 0, &new_s_node->t);
  gst_xml_helper_get_prop_unsigned_integer_64 (a_node, "d", 0, &new_s_node->d);
  gst_xml_helper_get_prop_signed_integer (a_node, "r", 0, &new_s_node->r);
}



static void
gst_mpdparser_parse_segment_timeline_node (GstMPDSegmentTimelineNode ** pointer,
    xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDSegmentTimelineNode *new_seg_timeline;

  gst_mpd_segment_timeline_node_free (*pointer);
  *pointer = new_seg_timeline = gst_mpd_segment_timeline_node_new ();
  if (new_seg_timeline == NULL) {
    GST_WARNING ("Allocation of SegmentTimeline node failed!");
    return;
  }

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "S") == 0) {
        gst_mpdparser_parse_s_node (&new_seg_timeline->S, cur_node);
      }
    }
  }
}

static gboolean
gst_mpdparser_parse_mult_seg_base_node (GstMPDMultSegmentBaseNode *
    mult_seg_base_node, xmlNode * a_node, GstMPDMultSegmentBaseNode * parent)
{
  xmlNode *cur_node;

  guint intval;
  gboolean has_timeline = FALSE, has_duration = FALSE;

  mult_seg_base_node->duration = 0;
  mult_seg_base_node->startNumber = 1;

  /* Inherit attribute values from parent */
  if (parent) {
    mult_seg_base_node->duration = parent->duration;
    mult_seg_base_node->startNumber = parent->startNumber;
    mult_seg_base_node->SegmentTimeline =
        gst_mpd_segment_timeline_node_clone (parent->SegmentTimeline);
    mult_seg_base_node->BitstreamSwitching =
        gst_mpd_url_type_node_clone (parent->BitstreamSwitching);
  }
  GST_LOG ("attributes of MultipleSegmentBaseType extension:");
  if (gst_xml_helper_get_prop_unsigned_integer (a_node, "duration", 0, &intval)) {
    mult_seg_base_node->duration = intval;
  }

  /* duration might be specified from parent */
  if (mult_seg_base_node->duration)
    has_duration = TRUE;

  if (gst_xml_helper_get_prop_unsigned_integer (a_node, "startNumber", 1,
          &intval)) {
    mult_seg_base_node->startNumber = intval;
  }

  GST_LOG ("extension of MultipleSegmentBaseType extension:");
  gst_mpdparser_parse_seg_base_type_ext (&mult_seg_base_node->SegmentBase,
      a_node, (parent ? parent->SegmentBase : NULL));

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentTimeline") == 0) {
        /* parse frees the segmenttimeline if any */
        gst_mpdparser_parse_segment_timeline_node
            (&mult_seg_base_node->SegmentTimeline, cur_node);
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "BitstreamSwitching") == 0) {
        /* parse frees the old url before setting the new one */
        gst_mpdparser_parse_url_type_node
            (&mult_seg_base_node->BitstreamSwitching, cur_node);
      }
    }
  }

  has_timeline = mult_seg_base_node->SegmentTimeline != NULL;

  /* Checking duration and timeline only at Representation's child level */
  if (xmlStrcmp (a_node->parent->name, (xmlChar *) "Representation") == 0
      && !has_duration && !has_timeline) {
    GST_ERROR ("segment has neither duration nor timeline");
  }

  return TRUE;
}

static gboolean
gst_mpdparser_parse_segment_list_node (GstMPDSegmentListNode ** pointer,
    xmlNode * a_node, GstMPDSegmentListNode * parent)
{
  xmlNode *cur_node;
  GstMPDSegmentListNode *new_segment_list;
  gchar *actuate;
  gboolean segment_urls_inherited_from_parent = FALSE;

  gst_mpd_segment_list_node_free (*pointer);
  new_segment_list = gst_mpd_segment_list_node_new ();

  /* Inherit attribute values from parent */
  if (parent) {
    GList *list;
    GstMPDSegmentURLNode *seg_url;
    for (list = g_list_first (parent->SegmentURL); list;
        list = g_list_next (list)) {
      seg_url = (GstMPDSegmentURLNode *) list->data;
      new_segment_list->SegmentURL =
          g_list_append (new_segment_list->SegmentURL,
          gst_mpd_segment_url_node_clone (seg_url));
      segment_urls_inherited_from_parent = TRUE;
    }
  }

  new_segment_list->actuate = GST_MPD_XLINK_ACTUATE_ON_REQUEST;
  if (gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "href", &new_segment_list->xlink_href)
      && gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "actuate", &actuate)) {
    if (strcmp (actuate, GST_MPD_XLINK_ACTUATE_ON_LOAD_STR) == 0)
      new_segment_list->actuate = GST_MPD_XLINK_ACTUATE_ON_LOAD;
    xmlFree (actuate);
  }

  GST_LOG ("extension of SegmentList node:");
  if (!gst_mpdparser_parse_mult_seg_base_node
      (GST_MPD_MULT_SEGMENT_BASE_NODE (new_segment_list), a_node,
          (parent ? GST_MPD_MULT_SEGMENT_BASE_NODE (parent) : NULL)))
    goto error;

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentURL") == 0) {
        if (segment_urls_inherited_from_parent) {
          /*
           * SegmentBase, SegmentTemplate and SegmentList shall inherit
           * attributes and elements from the same element on a higher level.
           * If the same attribute or element is present on both levels,
           * the one on the lower level shall take precedence over the one
           * on the higher level.
           */

          /* Clear the list of inherited segment URLs */
          g_list_free_full (new_segment_list->SegmentURL,
              (GDestroyNotify) gst_mpd_segment_url_node_free);
          new_segment_list->SegmentURL = NULL;

          /* mark the fact that we cleared the list, so that it is not tried again */
          segment_urls_inherited_from_parent = FALSE;
        }
        gst_mpdparser_parse_segment_url_node (&new_segment_list->SegmentURL,
            cur_node);
      }
    }
  }

  *pointer = new_segment_list;
  return TRUE;

error:
  gst_mpd_segment_list_node_free (new_segment_list);
  return FALSE;
}

static void
gst_mpdparser_parse_content_protection_node (GList ** list, xmlNode * a_node)
{
  GstMPDDescriptorTypeNode *new_descriptor;
  new_descriptor = gst_mpd_descriptor_type_node_new ((const gchar *)
      a_node->name);
  *list = g_list_append (*list, new_descriptor);

  gst_xml_helper_get_prop_string_stripped (a_node, "schemeIdUri",
      &new_descriptor->schemeIdUri);
  gst_xml_helper_get_node_as_string (a_node, &new_descriptor->value);
}

static void
gst_mpdparser_parse_representation_base (GstMPDRepresentationBaseNode *
    representation_base, xmlNode * a_node)
{
  xmlNode *cur_node;

  GST_LOG ("attributes of RepresentationBaseType extension:");
  gst_xml_helper_get_prop_string (a_node, "profiles",
      &representation_base->profiles);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "width", 0,
      &representation_base->width);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "height", 0,
      &representation_base->height);
  gst_xml_helper_get_prop_ratio (a_node, "sar", &representation_base->sar);
  gst_xml_helper_get_prop_framerate (a_node, "frameRate",
      &representation_base->frameRate);
  gst_xml_helper_get_prop_framerate (a_node, "minFrameRate",
      &representation_base->minFrameRate);
  gst_xml_helper_get_prop_framerate (a_node, "maxFrameRate",
      &representation_base->maxFrameRate);
  gst_xml_helper_get_prop_string (a_node, "audioSamplingRate",
      &representation_base->audioSamplingRate);
  gst_xml_helper_get_prop_string (a_node, "mimeType",
      &representation_base->mimeType);
  gst_xml_helper_get_prop_string (a_node, "segmentProfiles",
      &representation_base->segmentProfiles);
  gst_xml_helper_get_prop_string (a_node, "codecs",
      &representation_base->codecs);
  gst_xml_helper_get_prop_double (a_node, "maximumSAPPeriod",
      &representation_base->maximumSAPPeriod);
  gst_mpd_helper_get_SAP_type (a_node, "startWithSAP",
      &representation_base->startWithSAP);
  gst_xml_helper_get_prop_double (a_node, "maxPlayoutRate",
      &representation_base->maxPlayoutRate);
  gst_xml_helper_get_prop_boolean (a_node, "codingDependency",
      FALSE, &representation_base->codingDependency);
  gst_xml_helper_get_prop_string (a_node, "scanType",
      &representation_base->scanType);

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "FramePacking") == 0) {
        gst_mpdparser_parse_descriptor_type
            (&representation_base->FramePacking, cur_node);
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "AudioChannelConfiguration") == 0) {
        gst_mpdparser_parse_descriptor_type
            (&representation_base->AudioChannelConfiguration, cur_node);
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "ContentProtection") == 0) {
        gst_mpdparser_parse_content_protection_node
            (&representation_base->ContentProtection, cur_node);
      }
    }
  }
}

static gboolean
gst_mpdparser_parse_representation_node (GList ** list, xmlNode * a_node,
    GstMPDAdaptationSetNode * parent, GstMPDPeriodNode * period_node)
{
  xmlNode *cur_node;
  GstMPDRepresentationNode *new_representation;

  new_representation = gst_mpd_representation_node_new ();

  GST_LOG ("attributes of Representation node:");
  if (!gst_xml_helper_get_prop_string_no_whitespace (a_node, "id",
          &new_representation->id)) {
    GST_ERROR ("Cannot parse Representation id, invalid manifest");
    goto error;
  }
  if (!gst_xml_helper_get_prop_unsigned_integer (a_node, "bandwidth", 0,
          &new_representation->bandwidth)) {
    GST_ERROR ("Cannot parse Representation bandwidth, invalid manifest");
    goto error;
  }
  gst_xml_helper_get_prop_unsigned_integer (a_node, "qualityRanking", 0,
      &new_representation->qualityRanking);
  gst_xml_helper_get_prop_string_vector_type (a_node, "dependencyId",
      &new_representation->dependencyId);
  gst_xml_helper_get_prop_string_vector_type (a_node,
      "mediaStreamStructureId", &new_representation->mediaStreamStructureId);
  /* RepresentationBase extension */
  gst_mpdparser_parse_representation_base
      (GST_MPD_REPRESENTATION_BASE_NODE (new_representation), a_node);

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentBase") == 0) {
        gst_mpdparser_parse_seg_base_type_ext (&new_representation->SegmentBase,
            cur_node, parent->SegmentBase ?
            parent->SegmentBase : period_node->SegmentBase);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentTemplate") == 0) {
        if (!gst_mpdparser_parse_segment_template_node
            (&new_representation->SegmentTemplate, cur_node,
                parent->SegmentTemplate ?
                parent->SegmentTemplate : period_node->SegmentTemplate))
          goto error;
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentList") == 0) {
        if (!gst_mpdparser_parse_segment_list_node
            (&new_representation->SegmentList, cur_node,
                parent->SegmentList ? parent->SegmentList : period_node->
                SegmentList))
          goto error;
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "BaseURL") == 0) {
        gst_mpdparser_parse_baseURL_node (&new_representation->BaseURLs,
            cur_node);
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "SubRepresentation") == 0) {
        gst_mpdparser_parse_subrepresentation_node
            (&new_representation->SubRepresentations, cur_node);
      }
    }
  }

  /* some sanity checking */

  *list = g_list_append (*list, new_representation);
  return TRUE;

error:
  gst_mpd_representation_node_free (new_representation);
  return FALSE;
}

static gboolean
gst_mpdparser_parse_adaptation_set_node (GList ** list, xmlNode * a_node,
    GstMPDPeriodNode * parent)
{
  xmlNode *cur_node;
  GstMPDAdaptationSetNode *new_adap_set;
  gchar *actuate;

  new_adap_set = gst_mpd_adaptation_set_node_new ();

  GST_LOG ("attributes of AdaptationSet node:");

  new_adap_set->actuate = GST_MPD_XLINK_ACTUATE_ON_REQUEST;
  if (gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "href", &new_adap_set->xlink_href)
      && gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "actuate", &actuate)) {
    if (strcmp (actuate, "onLoad") == 0)
      new_adap_set->actuate = GST_MPD_XLINK_ACTUATE_ON_LOAD;
    xmlFree (actuate);
  }

  gst_xml_helper_get_prop_unsigned_integer (a_node, "id", 0, &new_adap_set->id);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "group", 0,
      &new_adap_set->group);
  gst_xml_helper_get_prop_string (a_node, "lang", &new_adap_set->lang);
  gst_xml_helper_get_prop_string (a_node, "contentType",
      &new_adap_set->contentType);
  gst_xml_helper_get_prop_ratio (a_node, "par", &new_adap_set->par);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "minBandwidth", 0,
      &new_adap_set->minBandwidth);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "maxBandwidth", 0,
      &new_adap_set->maxBandwidth);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "minWidth", 0,
      &new_adap_set->minWidth);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "maxWidth", 0,
      &new_adap_set->maxWidth);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "minHeight", 0,
      &new_adap_set->minHeight);
  gst_xml_helper_get_prop_unsigned_integer (a_node, "maxHeight", 0,
      &new_adap_set->maxHeight);
  gst_xml_helper_get_prop_cond_uint (a_node, "segmentAlignment",
      &new_adap_set->segmentAlignment);
  gst_xml_helper_get_prop_boolean (a_node, "bitstreamSwitching",
      parent->bitstreamSwitching, &new_adap_set->bitstreamSwitching);
  if (parent->bitstreamSwitching && !new_adap_set->bitstreamSwitching) {
    /* according to the standard, if the Period's bitstreamSwitching attribute
     * is true, the AdaptationSet should not have the bitstreamSwitching
     * attribute set to false.
     * We should return a parsing error, but we are generous and ignore the
     * standard violation.
     */
    new_adap_set->bitstreamSwitching = parent->bitstreamSwitching;
  }
  gst_xml_helper_get_prop_cond_uint (a_node, "subsegmentAlignment",
      &new_adap_set->subsegmentAlignment);
  gst_mpd_helper_get_SAP_type (a_node, "subsegmentStartsWithSAP",
      &new_adap_set->subsegmentStartsWithSAP);

  /* RepresentationBase extension */
  gst_mpdparser_parse_representation_base
      (GST_MPD_REPRESENTATION_BASE_NODE (new_adap_set), a_node);

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Accessibility") == 0) {
        gst_mpdparser_parse_descriptor_type (&new_adap_set->Accessibility,
            cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Role") == 0) {
        gst_mpdparser_parse_descriptor_type (&new_adap_set->Role, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Rating") == 0) {
        gst_mpdparser_parse_descriptor_type (&new_adap_set->Rating, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Viewpoint") == 0) {
        gst_mpdparser_parse_descriptor_type (&new_adap_set->Viewpoint,
            cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "BaseURL") == 0) {
        gst_mpdparser_parse_baseURL_node (&new_adap_set->BaseURLs, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentBase") == 0) {
        gst_mpdparser_parse_seg_base_type_ext (&new_adap_set->SegmentBase,
            cur_node, parent->SegmentBase);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentList") == 0) {
        if (!gst_mpdparser_parse_segment_list_node (&new_adap_set->SegmentList,
                cur_node, parent->SegmentList))
          goto error;
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "ContentComponent") == 0) {
        gst_mpdparser_parse_content_component_node
            (&new_adap_set->ContentComponents, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentTemplate") == 0) {
        if (!gst_mpdparser_parse_segment_template_node
            (&new_adap_set->SegmentTemplate, cur_node, parent->SegmentTemplate))
          goto error;
      }
    }
  }

  /* We must parse Representation after everything else in the AdaptationSet
   * has been parsed because certain Representation child elements can inherit
   * attributes specified by the same element in the AdaptationSet
   */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Representation") == 0) {
        if (!gst_mpdparser_parse_representation_node
            (&new_adap_set->Representations, cur_node, new_adap_set, parent))
          goto error;
      }
    }
  }

  *list = g_list_append (*list, new_adap_set);
  return TRUE;

error:
  gst_mpd_adaptation_set_node_free (new_adap_set);
  return FALSE;
}

static void
gst_mpdparser_parse_subset_node (GList ** list, xmlNode * a_node)
{
  GstMPDSubsetNode *new_subset;

  new_subset = gst_mpd_subset_node_new ();
  *list = g_list_append (*list, new_subset);

  GST_LOG ("attributes of Subset node:");
  gst_xml_helper_get_prop_uint_vector_type (a_node, "contains",
      &new_subset->contains, &new_subset->contains_size);
}

static gboolean
gst_mpdparser_parse_segment_template_node (GstMPDSegmentTemplateNode ** pointer,
    xmlNode * a_node, GstMPDSegmentTemplateNode * parent)
{
  GstMPDSegmentTemplateNode *new_segment_template;
  gchar *strval;

  gst_mpd_segment_template_node_free (*pointer);
  new_segment_template = gst_mpd_segment_template_node_new ();

  GST_LOG ("extension of SegmentTemplate node:");
  if (!gst_mpdparser_parse_mult_seg_base_node
      (GST_MPD_MULT_SEGMENT_BASE_NODE (new_segment_template), a_node,
          (parent ? GST_MPD_MULT_SEGMENT_BASE_NODE (parent) : NULL)))
    goto error;

  /* Inherit attribute values from parent when the value isn't found */
  GST_LOG ("attributes of SegmentTemplate node:");
  if (gst_xml_helper_get_prop_string (a_node, "media", &strval)) {
    new_segment_template->media = strval;
  } else if (parent) {
    new_segment_template->media = xmlMemStrdup (parent->media);
  }

  if (gst_xml_helper_get_prop_string (a_node, "index", &strval)) {
    new_segment_template->index = strval;
  } else if (parent) {
    new_segment_template->index = xmlMemStrdup (parent->index);
  }

  if (gst_xml_helper_get_prop_string (a_node, "initialization", &strval)) {
    new_segment_template->initialization = strval;
  } else if (parent) {
    new_segment_template->initialization =
        xmlMemStrdup (parent->initialization);
  }

  if (gst_xml_helper_get_prop_string (a_node, "bitstreamSwitching", &strval)) {
    new_segment_template->bitstreamSwitching = strval;
  } else if (parent) {
    new_segment_template->bitstreamSwitching =
        xmlMemStrdup (parent->bitstreamSwitching);
  }

  *pointer = new_segment_template;
  return TRUE;

error:
  gst_mpd_segment_template_node_free (new_segment_template);
  return FALSE;
}

static gboolean
gst_mpdparser_parse_period_node (GList ** list, xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDPeriodNode *new_period;
  gchar *actuate;

  new_period = gst_mpd_period_node_new ();

  GST_LOG ("attributes of Period node:");

  new_period->actuate = GST_MPD_XLINK_ACTUATE_ON_REQUEST;
  if (gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "href", &new_period->xlink_href)
      && gst_xml_helper_get_ns_prop_string (a_node,
          "http://www.w3.org/1999/xlink", "actuate", &actuate)) {
    if (strcmp (actuate, "onLoad") == 0)
      new_period->actuate = GST_MPD_XLINK_ACTUATE_ON_LOAD;
    xmlFree (actuate);
  }

  gst_xml_helper_get_prop_string (a_node, "id", &new_period->id);
  gst_xml_helper_get_prop_duration (a_node, "start", GST_MPD_DURATION_NONE,
      &new_period->start);
  gst_xml_helper_get_prop_duration (a_node, "duration",
      GST_MPD_DURATION_NONE, &new_period->duration);
  gst_xml_helper_get_prop_boolean (a_node, "bitstreamSwitching", FALSE,
      &new_period->bitstreamSwitching);

  /* explore children nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentBase") == 0) {
        gst_mpdparser_parse_seg_base_type_ext (&new_period->SegmentBase,
            cur_node, NULL);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentList") == 0) {
        if (!gst_mpdparser_parse_segment_list_node (&new_period->SegmentList,
                cur_node, NULL))
          goto error;
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "SegmentTemplate") == 0) {
        if (!gst_mpdparser_parse_segment_template_node
            (&new_period->SegmentTemplate, cur_node, NULL))
          goto error;
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Subset") == 0) {
        gst_mpdparser_parse_subset_node (&new_period->Subsets, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "BaseURL") == 0) {
        gst_mpdparser_parse_baseURL_node (&new_period->BaseURLs, cur_node);
      }
    }
  }

  /* We must parse AdaptationSet after everything else in the Period has been
   * parsed because certain AdaptationSet child elements can inherit attributes
   * specified by the same element in the Period
   */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "AdaptationSet") == 0) {
        if (!gst_mpdparser_parse_adaptation_set_node
            (&new_period->AdaptationSets, cur_node, new_period))
          goto error;
      }
    }
  }

  *list = g_list_append (*list, new_period);
  return TRUE;

error:
  gst_mpd_period_node_free (new_period);
  return FALSE;
}

static void
gst_mpdparser_parse_program_info_node (GList ** list, xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDProgramInformationNode *new_prog_info;

  new_prog_info = gst_mpd_program_information_node_new ();
  *list = g_list_append (*list, new_prog_info);

  GST_LOG ("attributes of ProgramInformation node:");
  gst_xml_helper_get_prop_string (a_node, "lang", &new_prog_info->lang);
  gst_xml_helper_get_prop_string (a_node, "moreInformationURL",
      &new_prog_info->moreInformationURL);

  /* explore children nodes */
  GST_LOG ("children of ProgramInformation node:");
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Title") == 0) {
        gst_xml_helper_get_node_content (cur_node, &new_prog_info->Title);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Source") == 0) {
        gst_xml_helper_get_node_content (cur_node, &new_prog_info->Source);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Copyright") == 0) {
        gst_xml_helper_get_node_content (cur_node, &new_prog_info->Copyright);
      }
    }
  }
}

static void
gst_mpdparser_parse_metrics_range_node (GList ** list, xmlNode * a_node)
{
  GstMPDMetricsRangeNode *new_metrics_range;

  new_metrics_range = gst_mpd_metrics_range_node_new ();
  *list = g_list_append (*list, new_metrics_range);

  GST_LOG ("attributes of Metrics Range node:");
  gst_xml_helper_get_prop_duration (a_node, "starttime",
      GST_MPD_DURATION_NONE, &new_metrics_range->starttime);
  gst_xml_helper_get_prop_duration (a_node, "duration",
      GST_MPD_DURATION_NONE, &new_metrics_range->duration);
}

static void
gst_mpdparser_parse_metrics_node (GList ** list, xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDMetricsNode *new_metrics;

  new_metrics = gst_mpd_metrics_node_new ();
  *list = g_list_append (*list, new_metrics);

  GST_LOG ("attributes of Metrics node:");
  gst_xml_helper_get_prop_string (a_node, "metrics", &new_metrics->metrics);

  /* explore children nodes */
  GST_LOG ("children of Metrics node:");
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Range") == 0) {
        gst_mpdparser_parse_metrics_range_node (&new_metrics->MetricsRanges,
            cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Reporting") == 0) {
        /* No reporting scheme is specified in this part of ISO/IEC 23009.
         * It is expected that external specifications may define formats
         * and delivery for the reporting data. */
        GST_LOG (" - Reporting node found (unknown structure)");
      }
    }
  }
}

/* The UTCTiming element is defined in
 * ISO/IEC 23009-1:2014/PDAM 1 "Information technology — Dynamic adaptive streaming over HTTP (DASH) — Part 1: Media presentation description and segment formats / Amendment 1: High Profile and Availability Time Synchronization"
 */
static void
gst_mpdparser_parse_utctiming_node (GList ** list, xmlNode * a_node)
{
  GstMPDUTCTimingNode *new_timing;
  gchar *method = NULL;
  gchar *value = NULL;

  new_timing = gst_mpd_utctiming_node_new ();

  GST_LOG ("attributes of UTCTiming node:");
  if (gst_xml_helper_get_prop_string (a_node, "schemeIdUri", &method)) {
    new_timing->method = gst_mpd_utctiming_get_method (method);
    xmlFree (method);
  }

  if (gst_xml_helper_get_prop_string (a_node, "value", &value)) {
    int max_tokens = 0;
    if (GST_MPD_UTCTIMING_TYPE_DIRECT == new_timing->method) {
      /* The GST_MPD_UTCTIMING_TYPE_DIRECT method is a special case
       * that is not a space separated list.
       */
      max_tokens = 1;
    }
    new_timing->urls = g_strsplit (value, " ", max_tokens);
    xmlFree (value);
  }

  /* append to list only if both method and urls were set */
  if (new_timing->method != 0 && new_timing->urls != NULL &&
      g_strv_length (new_timing->urls) != 0) {
    *list = g_list_append (*list, new_timing);
  } else {
    gst_mpd_utctiming_node_free (new_timing);
  }
}

static gboolean
gst_mpdparser_parse_root_node (GstMPDRootNode ** pointer, xmlNode * a_node)
{
  xmlNode *cur_node;
  GstMPDRootNode *new_mpd_root;

  gst_mpd_root_node_free (*pointer);
  *pointer = NULL;
  new_mpd_root = gst_mpd_root_node_new ();

  GST_LOG ("namespaces of root MPD node:");
  new_mpd_root->default_namespace =
      gst_xml_helper_get_node_namespace (a_node, NULL);
  new_mpd_root->namespace_xsi =
      gst_xml_helper_get_node_namespace (a_node, "xsi");
  new_mpd_root->namespace_ext =
      gst_xml_helper_get_node_namespace (a_node, "ext");

  GST_LOG ("attributes of root MPD node:");
  gst_xml_helper_get_prop_string (a_node, "schemaLocation",
      &new_mpd_root->schemaLocation);
  gst_xml_helper_get_prop_string (a_node, "id", &new_mpd_root->id);
  gst_xml_helper_get_prop_string (a_node, "profiles", &new_mpd_root->profiles);
  gst_mpd_helper_get_mpd_type (a_node, "type", &new_mpd_root->type);
  gst_xml_helper_get_prop_dateTime (a_node, "availabilityStartTime",
      &new_mpd_root->availabilityStartTime);
  gst_xml_helper_get_prop_dateTime (a_node, "availabilityEndTime",
      &new_mpd_root->availabilityEndTime);
  gst_xml_helper_get_prop_duration (a_node, "mediaPresentationDuration",
      GST_MPD_DURATION_NONE, &new_mpd_root->mediaPresentationDuration);
  gst_xml_helper_get_prop_duration (a_node, "minimumUpdatePeriod",
      GST_MPD_DURATION_NONE, &new_mpd_root->minimumUpdatePeriod);
  gst_xml_helper_get_prop_duration (a_node, "minBufferTime",
      GST_MPD_DURATION_NONE, &new_mpd_root->minBufferTime);
  gst_xml_helper_get_prop_duration (a_node, "timeShiftBufferDepth",
      GST_MPD_DURATION_NONE, &new_mpd_root->timeShiftBufferDepth);
  gst_xml_helper_get_prop_duration (a_node, "suggestedPresentationDelay",
      GST_MPD_DURATION_NONE, &new_mpd_root->suggestedPresentationDelay);
  gst_xml_helper_get_prop_duration (a_node, "maxSegmentDuration",
      GST_MPD_DURATION_NONE, &new_mpd_root->maxSegmentDuration);
  gst_xml_helper_get_prop_duration (a_node, "maxSubsegmentDuration",
      GST_MPD_DURATION_NONE, &new_mpd_root->maxSubsegmentDuration);

  /* explore children Period nodes */
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "Period") == 0) {
        if (!gst_mpdparser_parse_period_node (&new_mpd_root->Periods, cur_node))
          goto error;
      } else if (xmlStrcmp (cur_node->name,
              (xmlChar *) "ProgramInformation") == 0) {
        gst_mpdparser_parse_program_info_node (&new_mpd_root->ProgramInfos,
            cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "BaseURL") == 0) {
        gst_mpdparser_parse_baseURL_node (&new_mpd_root->BaseURLs, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Location") == 0) {
        gst_mpdparser_parse_location_node (&new_mpd_root->Locations, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "Metrics") == 0) {
        gst_mpdparser_parse_metrics_node (&new_mpd_root->Metrics, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "UTCTiming") == 0) {
        gst_mpdparser_parse_utctiming_node (&new_mpd_root->UTCTimings,
            cur_node);
      }
    }
  }

  *pointer = new_mpd_root;
  return TRUE;

error:
  gst_mpd_root_node_free (new_mpd_root);
  return FALSE;
}

/* internal memory management functions */

/* ISO/IEC 23009-1:2004 5.3.9.4.4 */
static gboolean
validate_format (const gchar * format)
{
  const gchar *p = format;

  /* Check if it starts with % */
  if (!p || p[0] != '%')
    return FALSE;
  p++;

  /* the spec mandates a format like %0[width]d */
  /* Following the %, we must have a 0 */
  if (p[0] != '0')
    return FALSE;

  /* Following the % must be a number starting with 0
   */
  while (g_ascii_isdigit (*p))
    p++;

  /* After any 0 and alphanumeric values, there must be a d.
   */
  if (p[0] != 'd')
    return FALSE;
  p++;

  /* And then potentially more characters without any
   * further %, even if the spec does not mention this
   */
  p = strchr (p, '%');
  if (p)
    return FALSE;

  return TRUE;
}

static gchar *
promote_format_to_uint64 (const gchar * format)
{
  const gchar *p = format;
  gchar *promoted_format;

  /* Must be called with a validated format! */
  g_return_val_if_fail (validate_format (format), NULL);

  /* it starts with % */
  p++;

  /* Following the % must be a 0, or any of d, x or u.
   * x and u are not part of the spec, but don't hurt us
   */
  if (p[0] == '0') {
    p++;

    while (g_ascii_isdigit (*p))
      p++;
  }

  /* After any 0 and alphanumeric values, there must be a d.
   * Otherwise validation would have failed
   */
  g_assert (p[0] == 'd');

  promoted_format =
      g_strdup_printf ("%.*s" G_GINT64_MODIFIER "%s", (gint) (p - format),
      format, p);

  return promoted_format;
}

static gboolean
gst_mpdparser_validate_rfc1738_url (const char *s)
{
  while (*s) {
    if (!strchr
        (";:@&=aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789$-_.+!*'(),%/",
            *s))
      return FALSE;
    if (*s == '%') {
      /* g_ascii_isdigit returns FALSE for NUL, and || is a short circuiting
         operator, so this is safe for strings ending before two hex digits */
      if (!g_ascii_isxdigit (s[1]) || !g_ascii_isxdigit (s[2]))
        return FALSE;
      s += 2;
    }
    s++;
  }
  return TRUE;
}

void
gst_mpdparser_media_fragment_info_clear (GstMediaFragmentInfo * fragment)
{
  g_free (fragment->uri);
  g_free (fragment->index_uri);
}

/* API */
gboolean
gst_mpdparser_get_mpd_root_node (GstMPDRootNode ** mpd_root_node,
    const gchar * data, gint size)
{
  gboolean ret = FALSE;

  if (data) {
    xmlDocPtr doc;
    xmlNode *root_element = NULL;

    GST_DEBUG ("MPD file fully buffered, start parsing...");

    /* parse the complete MPD file into a tree (using the libxml2 default parser API) */

    /* this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used
     */
    LIBXML_TEST_VERSION;

    /* parse "data" into a document (which is a libxml2 tree structure xmlDoc) */
    doc = xmlReadMemory (data, size, "noname.xml", NULL, XML_PARSE_NONET);
    if (doc == NULL) {
      GST_ERROR ("failed to parse the MPD file");
      ret = FALSE;
    } else {
      /* get the root element node */
      root_element = xmlDocGetRootElement (doc);

      if (root_element->type != XML_ELEMENT_NODE
          || xmlStrcmp (root_element->name, (xmlChar *) "MPD") != 0) {
        GST_ERROR
            ("can not find the root element MPD, failed to parse the MPD file");
        ret = FALSE;            /* used to return TRUE before, but this seems wrong */
      } else {
        /* now we can parse the MPD root node and all children nodes, recursively */
        ret = gst_mpdparser_parse_root_node (mpd_root_node, root_element);
      }
      /* free the document */
      xmlFreeDoc (doc);
    }
  }

  return ret;
}

GstMPDSegmentListNode *
gst_mpdparser_get_external_segment_list (const gchar * data, gint size,
    GstMPDSegmentListNode * parent)
{
  xmlDocPtr doc = NULL;
  GstMPDSegmentListNode *new_segment_list = NULL;

  doc = xmlReadMemory (data, size, "noname.xml", NULL, XML_PARSE_NONET);


  /* NOTE: ISO/IEC 23009-1:2014 5.3.9.3.2 is saying that one or multiple SegmentList
   * in external xml is allowed, however, multiple SegmentList does not make sense
   * because Period/AdaptationSet/Representation allow only one SegmentList */
  if (doc) {
    xmlNode *root_element = xmlDocGetRootElement (doc);


    if (root_element->type == XML_ELEMENT_NODE &&
        xmlStrcmp (root_element->name, (xmlChar *) "SegmentList") == 0) {
      gst_mpdparser_parse_segment_list_node (&new_segment_list, root_element,
          parent);
    }
  }

  if (doc)
    xmlFreeDoc (doc);

  return new_segment_list;
}

GList *
gst_mpdparser_get_external_periods (const gchar * data, gint size)
{
  xmlDocPtr doc = NULL;
  GList *new_periods = NULL;

  doc = xmlReadMemory (data, size, "noname.xml", NULL, XML_PARSE_NONET);


  if (doc) {
    xmlNode *root_element = xmlDocGetRootElement (doc);
    xmlNode *iter;

    for (iter = root_element->children; iter; iter = iter->next) {
      if (iter->type == XML_ELEMENT_NODE) {
        if (xmlStrcmp (iter->name, (xmlChar *) "Period") == 0) {
          gst_mpdparser_parse_period_node (&new_periods, iter);
        } else {
          goto error;
        }
      }
    }
  }

done:
  if (doc)
    xmlFreeDoc (doc);

  return new_periods;

error:
  GST_ERROR ("Failed to parse period node XML");

  if (new_periods) {
    g_list_free_full (new_periods, (GDestroyNotify) gst_mpd_period_node_free);
    new_periods = NULL;
  }
  goto done;
}

GList *
gst_mpdparser_get_external_adaptation_sets (const gchar * data, gint size,
    GstMPDPeriodNode * period)
{
  xmlDocPtr doc = NULL;
  GList *new_adaptation_sets = NULL;

  doc = xmlReadMemory (data, size, "noname.xml", NULL, XML_PARSE_NONET);

  /* NOTE: ISO/IEC 23009-1:2014 5.3.3.2 is saying that exactly one AdaptationSet
   * in external xml is allowed */
  if (doc) {
    xmlNode *root_element = xmlDocGetRootElement (doc);
    if (root_element->type == XML_ELEMENT_NODE &&
        xmlStrcmp (root_element->name, (xmlChar *) "AdaptationSet") == 0) {
      gst_mpdparser_parse_adaptation_set_node (&new_adaptation_sets,
          root_element, period);
    }
  }

  if (doc)
    xmlFreeDoc (doc);

  return new_adaptation_sets;
}

void
gst_mpdparser_free_stream_period (GstStreamPeriod * stream_period)
{
  if (stream_period) {
    g_slice_free (GstStreamPeriod, stream_period);
  }
}

void
gst_mpdparser_free_media_segment (GstMediaSegment * media_segment)
{
  if (media_segment) {
    g_slice_free (GstMediaSegment, media_segment);
  }
}

void
gst_mpdparser_init_active_stream_segments (GstActiveStream * stream)
{
  g_assert (stream->segments == NULL);
  stream->segments = g_ptr_array_new ();
  g_ptr_array_set_free_func (stream->segments,
      (GDestroyNotify) gst_mpdparser_free_media_segment);
}

void
gst_mpdparser_free_active_stream (GstActiveStream * active_stream)
{
  if (active_stream) {
    g_free (active_stream->baseURL);
    active_stream->baseURL = NULL;
    g_free (active_stream->queryURL);
    active_stream->queryURL = NULL;
    if (active_stream->segments)
      g_ptr_array_unref (active_stream->segments);
    g_slice_free (GstActiveStream, active_stream);
  }
}

static gchar *
get_base_url_with_query (GstActiveStream * stream)
{
  GstUri *uri;
  gchar *uri_str;

  if (!stream->queryURL)
    return g_strdup (stream->baseURL);

  uri = gst_uri_from_string (stream->baseURL);
  gst_uri_set_query_string (uri, stream->queryURL);
  uri_str = gst_uri_to_string (uri);

  gst_uri_unref (uri);
  return uri_str;
}

/*
 * gst_mpdparser_get_initializationURL:
 *
 * Returns: (transfer full): stream initializationURL if available,
 *   baseURL combined with queryURL otherwise.
 */
gchar *
gst_mpdparser_get_initializationURL (GstActiveStream * stream,
    GstMPDURLTypeNode * InitializationURL)
{
  g_return_val_if_fail (stream != NULL, NULL);

  return (InitializationURL && InitializationURL->sourceURL)
      ? g_strdup (InitializationURL->sourceURL)
      : get_base_url_with_query (stream);
}

/*
 * gst_mpdparser_get_mediaURL:
 *
 * Returns: (transfer full): stream mediaURL if available,
 *   baseURL combined with queryURL otherwise.
 */
gchar *
gst_mpdparser_get_mediaURL (GstActiveStream * stream,
    GstMPDSegmentURLNode * segmentURL)
{
  g_return_val_if_fail (stream != NULL, NULL);
  g_return_val_if_fail (segmentURL != NULL, NULL);

  return (segmentURL->media)
      ? g_strdup (segmentURL->media)
      : get_base_url_with_query (stream);
}

/* navigation functions */
GstStreamMimeType
gst_mpdparser_representation_get_mimetype (GstMPDAdaptationSetNode * adapt_set,
    GstMPDRepresentationNode * rep)
{
  gchar *mime = NULL;
  if (rep)
    mime = GST_MPD_REPRESENTATION_BASE_NODE (rep)->mimeType;
  if (mime == NULL) {
    mime = GST_MPD_REPRESENTATION_BASE_NODE (adapt_set)->mimeType;
  }

  if (gst_mpd_helper_strncmp_ext (mime, "audio") == 0)
    return GST_STREAM_AUDIO;
  if (gst_mpd_helper_strncmp_ext (mime, "video") == 0)
    return GST_STREAM_VIDEO;
  if (gst_mpd_helper_strncmp_ext (mime, "application") == 0
      || gst_mpd_helper_strncmp_ext (mime, "text") == 0)
    return GST_STREAM_APPLICATION;

  return GST_STREAM_UNKNOWN;
}

/* Helper methods */
gchar *
gst_mpdparser_build_URL_from_template (const gchar * url_template,
    const gchar * id, guint number, guint bandwidth, guint64 time)
{
  static const gchar default_format[] = "%01d";
  gchar **tokens, *token, *ret;
  const gchar *format;
  gint i, num_tokens;

  g_return_val_if_fail (url_template != NULL, NULL);
  tokens = g_strsplit_set (url_template, "$", -1);
  if (!tokens) {
    GST_WARNING ("Scan of URL template failed!");
    return NULL;
  }
  num_tokens = g_strv_length (tokens);

  /*
   * each identifier is guarded by 2 $, which means that we must have an odd number of tokens
   * An even number of tokens means the string is not valid.
   */
  if ((num_tokens & 1) == 0) {
    GST_ERROR ("Invalid number of tokens (%d). url_template is '%s'",
        num_tokens, url_template);
    g_strfreev (tokens);
    return NULL;
  }

  for (i = 0; i < num_tokens; i++) {
    token = tokens[i];
    format = default_format;

    /* the tokens to replace must be provided between $ characters, eg $token$
     * For a string like token0$token1$token2$token3$token4, only the odd number
     * tokens (1,3,...) must be parsed.
     *
     * Skip even tokens
     */
    if ((i & 1) == 0)
      continue;

    if (!g_strcmp0 (token, "RepresentationID")) {
      if (!gst_mpdparser_validate_rfc1738_url (id))
        goto invalid_representation_id;

      tokens[i] = g_strdup_printf ("%s", id);
      g_free (token);
    } else if (!strncmp (token, "Number", 6)) {
      if (strlen (token) > 6) {
        format = token + 6;     /* format tag */
      }
      if (!validate_format (format))
        goto invalid_format;

      tokens[i] = g_strdup_printf (format, number);
      g_free (token);
    } else if (!strncmp (token, "Bandwidth", 9)) {
      if (strlen (token) > 9) {
        format = token + 9;     /* format tag */
      }
      if (!validate_format (format))
        goto invalid_format;

      tokens[i] = g_strdup_printf (format, bandwidth);
      g_free (token);
    } else if (!strncmp (token, "Time", 4)) {
      gchar *promoted_format;

      if (strlen (token) > 4) {
        format = token + 4;     /* format tag */
      }
      if (!validate_format (format))
        goto invalid_format;

      promoted_format = promote_format_to_uint64 (format);
      tokens[i] = g_strdup_printf (promoted_format, time);
      g_free (promoted_format);
      g_free (token);
    } else if (!g_strcmp0 (token, "")) {
      tokens[i] = g_strdup_printf ("%s", "$");
      g_free (token);
    } else {
      /* unexpected identifier found between $ signs
       *
       * "If the URL contains unescaped $ symbols which do not enclose a valid
       * identifier then the result of URL formation is undefined"
       */
      goto invalid_format;
    }
  }

  ret = g_strjoinv (NULL, tokens);

  g_strfreev (tokens);

  return ret;

invalid_format:
  {
    GST_ERROR ("Invalid format '%s' in '%s'", format, token);

    g_strfreev (tokens);

    return NULL;
  }
invalid_representation_id:
  {
    GST_ERROR
        ("Representation ID string '%s' has characters invalid in an RFC 1738 URL",
        id);

    g_strfreev (tokens);

    return NULL;
  }
}
