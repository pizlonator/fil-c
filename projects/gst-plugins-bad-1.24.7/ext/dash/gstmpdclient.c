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

#include "gstmpdclient.h"
#include "gstmpdparser.h"

GST_DEBUG_CATEGORY_STATIC (gst_dash_mpd_client_debug);
#undef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT gst_dash_mpd_client_debug

G_DEFINE_TYPE (GstMPDClient, gst_mpd_client, GST_TYPE_OBJECT);

static GstMPDSegmentBaseNode *gst_mpd_client_get_segment_base (GstMPDPeriodNode
    * Period, GstMPDAdaptationSetNode * AdaptationSet,
    GstMPDRepresentationNode * Representation);
static GstMPDSegmentListNode *gst_mpd_client_get_segment_list (GstMPDClient *
    client, GstMPDPeriodNode * Period, GstMPDAdaptationSetNode * AdaptationSet,
    GstMPDRepresentationNode * Representation);
/* Segments */
static guint gst_mpd_client_get_segments_counts (GstMPDClient * client,
    GstActiveStream * stream);

static GList *gst_mpd_client_fetch_external_periods (GstMPDClient * client,
    GstMPDPeriodNode * period_node);
static GList *gst_mpd_client_fetch_external_adaptation_set (GstMPDClient *
    client, GstMPDPeriodNode * period, GstMPDAdaptationSetNode * adapt_set);

static GstMPDRepresentationNode *gst_mpd_client_get_lowest_representation (GList
    * Representations);
static GstStreamPeriod *gst_mpd_client_get_stream_period (GstMPDClient *
    client);

typedef GstMPDNode *(*MpdClientStringIDFilter) (GList * list, gchar * data);
typedef GstMPDNode *(*MpdClientIDFilter) (GList * list, guint data);

static GstMPDNode *
gst_mpd_client_get_period_with_id (GList * periods, gchar * period_id)
{
  GstMPDPeriodNode *period;
  GList *list = NULL;

  for (list = g_list_first (periods); list; list = g_list_next (list)) {
    period = (GstMPDPeriodNode *) list->data;
    if (!g_strcmp0 (period->id, period_id))
      return GST_MPD_NODE (period);
  }
  return NULL;
}

static GstMPDNode *
gst_mpd_client_get_adaptation_set_with_id (GList * adaptation_sets, guint id)
{
  GstMPDAdaptationSetNode *adaptation_set;
  GList *list = NULL;

  for (list = g_list_first (adaptation_sets); list; list = g_list_next (list)) {
    adaptation_set = (GstMPDAdaptationSetNode *) list->data;
    if (adaptation_set->id == id)
      return GST_MPD_NODE (adaptation_set);
  }
  return NULL;
}

GstMPDRepresentationNode *
gst_mpd_client_get_representation_with_id (GList * representations,
    gchar * rep_id)
{
  GstMPDRepresentationNode *representation;
  GList *list = NULL;

  for (list = g_list_first (representations); list; list = g_list_next (list)) {
    representation = (GstMPDRepresentationNode *) list->data;
    if (!g_strcmp0 (representation->id, rep_id))
      return GST_MPD_REPRESENTATION_NODE (representation);
  }
  return NULL;
}

static GstMPDNode *
gst_mpd_client_get_representation_with_id_filter (GList * representations,
    gchar * rep_id)
{
  GstMPDRepresentationNode *representation =
      gst_mpd_client_get_representation_with_id (representations, rep_id);

  if (representation != NULL)
    return GST_MPD_NODE (representation);

  return NULL;
}

static gchar *
_generate_new_string_id (GList * list, const gchar * tuple,
    MpdClientStringIDFilter filter)
{
  guint i = 0;
  gchar *id = NULL;
  GstMPDNode *node;
  do {
    g_free (id);
    id = g_strdup_printf (tuple, i);
    node = filter (list, id);
    i++;
  } while (node);

  return id;
}

static guint
_generate_new_id (GList * list, MpdClientIDFilter filter)
{
  guint id = 0;
  GstMPDNode *node;
  do {
    node = filter (list, id);
    id++;
  } while (node);

  return id;
}

static GstMPDRepresentationNode *
gst_mpd_client_get_lowest_representation (GList * Representations)
{
  GList *list = NULL;
  GstMPDRepresentationNode *rep = NULL;
  GstMPDRepresentationNode *lowest = NULL;

  if (Representations == NULL)
    return NULL;

  for (list = g_list_first (Representations); list; list = g_list_next (list)) {
    rep = (GstMPDRepresentationNode *) list->data;
    if (rep && (!lowest || rep->bandwidth < lowest->bandwidth)) {
      lowest = rep;
    }
  }

  return lowest;
}

#if 0
static GstMPDRepresentationNode *
gst_mpdparser_get_highest_representation (GList * Representations)
{
  GList *list = NULL;

  if (Representations == NULL)
    return NULL;

  list = g_list_last (Representations);

  return list ? (GstMPDRepresentationNode *) list->data : NULL;
}

static GstMPDRepresentationNode *
gst_mpdparser_get_representation_with_max_bandwidth (GList * Representations,
    gint max_bandwidth)
{
  GList *list = NULL;
  GstMPDRepresentationNode *representation, *best_rep = NULL;

  if (Representations == NULL)
    return NULL;

  if (max_bandwidth <= 0)       /* 0 => get highest representation available */
    return gst_mpdparser_get_highest_representation (Representations);

  for (list = g_list_first (Representations); list; list = g_list_next (list)) {
    representation = (GstMPDRepresentationNode *) list->data;
    if (representation && representation->bandwidth <= max_bandwidth) {
      best_rep = representation;
    }
  }

  return best_rep;
}
#endif

static GstMPDSegmentListNode *
gst_mpd_client_fetch_external_segment_list (GstMPDClient * client,
    GstMPDPeriodNode * Period,
    GstMPDAdaptationSetNode * AdaptationSet,
    GstMPDRepresentationNode * Representation,
    GstMPDSegmentListNode * parent, GstMPDSegmentListNode * segment_list)
{
  GstFragment *download;
  GstBuffer *segment_list_buffer = NULL;
  GstMapInfo map;
  GError *err = NULL;

  GstUri *base_uri, *uri;
  gchar *query = NULL;
  gchar *uri_string;
  GstMPDSegmentListNode *new_segment_list = NULL;

  /* ISO/IEC 23009-1:2014 5.5.3 4)
   * Remove nodes that resolve to nothing when resolving
   */
  if (strcmp (segment_list->xlink_href,
          "urn:mpeg:dash:resolve-to-zero:2013") == 0) {
    return NULL;
  }

  if (!client->downloader) {
    return NULL;
  }

  /* Build absolute URI */

  /* Get base URI at the MPD level */
  base_uri =
      gst_uri_from_string (client->mpd_base_uri ? client->
      mpd_base_uri : client->mpd_uri);

  /* combine a BaseURL at the MPD level with the current base url */
  base_uri =
      gst_mpd_helper_combine_urls (base_uri, client->mpd_root_node->BaseURLs,
      &query, 0);

  /* combine a BaseURL at the Period level with the current base url */
  base_uri =
      gst_mpd_helper_combine_urls (base_uri, Period->BaseURLs, &query, 0);

  if (AdaptationSet) {
    /* combine a BaseURL at the AdaptationSet level with the current base url */
    base_uri =
        gst_mpd_helper_combine_urls (base_uri, AdaptationSet->BaseURLs, &query,
        0);

    if (Representation) {
      /* combine a BaseURL at the Representation level with the current base url */
      base_uri =
          gst_mpd_helper_combine_urls (base_uri, Representation->BaseURLs,
          &query, 0);
    }
  }

  uri = gst_uri_from_string_with_base (base_uri, segment_list->xlink_href);
  if (query)
    gst_uri_set_query_string (uri, query);
  g_free (query);
  uri_string = gst_uri_to_string (uri);
  gst_uri_unref (base_uri);
  gst_uri_unref (uri);

  download =
      gst_uri_downloader_fetch_uri (client->downloader,
      uri_string, client->mpd_uri, TRUE, FALSE, TRUE, &err);
  g_free (uri_string);

  if (!download) {
    GST_ERROR ("Failed to download external SegmentList node at '%s': %s",
        segment_list->xlink_href, err->message);
    g_clear_error (&err);
    return NULL;
  }

  segment_list_buffer = gst_fragment_get_buffer (download);
  g_object_unref (download);

  gst_buffer_map (segment_list_buffer, &map, GST_MAP_READ);

  new_segment_list =
      gst_mpdparser_get_external_segment_list ((const gchar *) map.data,
      map.size, parent);

  if (segment_list_buffer) {
    gst_buffer_unmap (segment_list_buffer, &map);
    gst_buffer_unref (segment_list_buffer);
  }

  return new_segment_list;
}

static GstMPDSegmentBaseNode *
gst_mpd_client_get_segment_base (GstMPDPeriodNode * Period,
    GstMPDAdaptationSetNode * AdaptationSet,
    GstMPDRepresentationNode * Representation)
{
  GstMPDSegmentBaseNode *SegmentBase = NULL;

  if (Representation && Representation->SegmentBase) {
    SegmentBase = Representation->SegmentBase;
  } else if (AdaptationSet && AdaptationSet->SegmentBase) {
    SegmentBase = AdaptationSet->SegmentBase;
  } else if (Period && Period->SegmentBase) {
    SegmentBase = Period->SegmentBase;
  }
  /* the SegmentBase element could be encoded also inside a SegmentList element */
  if (SegmentBase == NULL) {
    if (Representation && Representation->SegmentList
        && GST_MPD_MULT_SEGMENT_BASE_NODE (Representation->SegmentList)
        && GST_MPD_MULT_SEGMENT_BASE_NODE (Representation->
            SegmentList)->SegmentBase) {
      SegmentBase =
          GST_MPD_MULT_SEGMENT_BASE_NODE (Representation->
          SegmentList)->SegmentBase;
    } else if (AdaptationSet && AdaptationSet->SegmentList
        && GST_MPD_MULT_SEGMENT_BASE_NODE (AdaptationSet->SegmentList)
        && GST_MPD_MULT_SEGMENT_BASE_NODE (AdaptationSet->
            SegmentList)->SegmentBase) {
      SegmentBase =
          GST_MPD_MULT_SEGMENT_BASE_NODE (AdaptationSet->
          SegmentList)->SegmentBase;
    } else if (Period && Period->SegmentList
        && GST_MPD_MULT_SEGMENT_BASE_NODE (Period->SegmentList)
        && GST_MPD_MULT_SEGMENT_BASE_NODE (Period->SegmentList)->SegmentBase) {
      SegmentBase =
          GST_MPD_MULT_SEGMENT_BASE_NODE (Period->SegmentList)->SegmentBase;
    }
  }

  return SegmentBase;
}

static GstMPDSegmentListNode *
gst_mpd_client_get_segment_list (GstMPDClient * client,
    GstMPDPeriodNode * Period, GstMPDAdaptationSetNode * AdaptationSet,
    GstMPDRepresentationNode * Representation)
{
  GstMPDSegmentListNode **SegmentList;
  GstMPDSegmentListNode *ParentSegmentList = NULL;

  if (Representation && Representation->SegmentList) {
    SegmentList = &Representation->SegmentList;
    ParentSegmentList = AdaptationSet->SegmentList;
  } else if (AdaptationSet && AdaptationSet->SegmentList) {
    SegmentList = &AdaptationSet->SegmentList;
    ParentSegmentList = Period->SegmentList;
    Representation = NULL;
  } else {
    Representation = NULL;
    AdaptationSet = NULL;
    SegmentList = &Period->SegmentList;
  }

  /* Resolve external segment list here. */
  if (*SegmentList && (*SegmentList)->xlink_href) {
    GstMPDSegmentListNode *new_segment_list;

    /* TODO: Use SegmentList of parent if
     * - Parent has its own SegmentList
     * - Fail to get SegmentList from external xml
     */
    new_segment_list =
        gst_mpd_client_fetch_external_segment_list (client, Period,
        AdaptationSet, Representation, ParentSegmentList, *SegmentList);

    gst_mpd_segment_list_node_free (*SegmentList);
    *SegmentList = new_segment_list;
  }

  return *SegmentList;
}

static GstClockTime
gst_mpd_client_get_segment_duration (GstMPDClient * client,
    GstActiveStream * stream, guint64 * scale_dur)
{
  GstStreamPeriod *stream_period;
  GstMPDMultSegmentBaseNode *base = NULL;
  GstClockTime duration = 0;

  g_return_val_if_fail (stream != NULL, GST_CLOCK_TIME_NONE);
  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, GST_CLOCK_TIME_NONE);

  if (stream->cur_segment_list) {
    base = GST_MPD_MULT_SEGMENT_BASE_NODE (stream->cur_segment_list);
  } else if (stream->cur_seg_template) {
    base = GST_MPD_MULT_SEGMENT_BASE_NODE (stream->cur_seg_template);
  }

  if (base == NULL || base->SegmentBase == NULL) {
    /* this may happen when we have a single segment */
    duration = stream_period->duration;
    if (scale_dur)
      *scale_dur = duration;
  } else {
    /* duration is guint so this cannot overflow */
    duration = base->duration * GST_SECOND;
    if (scale_dur)
      *scale_dur = duration;
    duration /= base->SegmentBase->timescale;
  }

  return duration;
}

void
gst_mpd_client_active_streams_free (GstMPDClient * client)
{
  if (client->active_streams) {
    g_list_foreach (client->active_streams,
        (GFunc) gst_mpdparser_free_active_stream, NULL);
    g_list_free (client->active_streams);
    client->active_streams = NULL;
  }
}

static void
gst_mpd_client_finalize (GObject * object)
{
  GstMPDClient *client = GST_MPD_CLIENT (object);

  if (client->mpd_root_node)
    gst_mpd_root_node_free (client->mpd_root_node);

  if (client->periods) {
    g_list_free_full (client->periods,
        (GDestroyNotify) gst_mpdparser_free_stream_period);
  }

  gst_mpd_client_active_streams_free (client);

  g_free (client->mpd_uri);
  client->mpd_uri = NULL;
  g_free (client->mpd_base_uri);
  client->mpd_base_uri = NULL;

  if (client->downloader)
    gst_object_unref (client->downloader);
  client->downloader = NULL;

  G_OBJECT_CLASS (gst_mpd_client_parent_class)->finalize (object);
}

static void
gst_mpd_client_class_init (GstMPDClientClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gst_mpd_client_finalize;
}

static void
gst_mpd_client_init (GstMPDClient * client)
{
}

GstMPDClient *
gst_mpd_client_new (void)
{
  GstMPDClient *ret;

  GST_DEBUG_CATEGORY_INIT (gst_dash_mpd_client_debug, "dashmpdclient", 0,
      "DashmMpdClient");
  ret = g_object_new (GST_TYPE_MPD_CLIENT, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

GstMPDClient *
gst_mpd_client_new_static (void)
{
  GstMPDClient *client = gst_mpd_client_new ();

  client->mpd_root_node = gst_mpd_root_node_new ();
  client->mpd_root_node->default_namespace =
      g_strdup ("urn:mpeg:dash:schema:mpd:2011");
  client->mpd_root_node->profiles =
      g_strdup ("urn:mpeg:dash:profile:isoff-main:2011");
  client->mpd_root_node->type = GST_MPD_FILE_TYPE_STATIC;
  client->mpd_root_node->minBufferTime = 1500;

  return client;
}

void
gst_mpd_client_free (GstMPDClient * client)
{
  if (client)
    gst_object_unref (client);
}

gboolean
gst_mpd_client_parse (GstMPDClient * client, const gchar * data, gint size)
{
  gboolean ret = FALSE;


  ret = gst_mpdparser_get_mpd_root_node (&client->mpd_root_node, data, size);

  if (ret) {
    gst_mpd_client_check_profiles (client);
    gst_mpd_client_fetch_on_load_external_resources (client);
  }

  return ret;
}


gboolean
gst_mpd_client_get_xml_content (GstMPDClient * client, gchar ** data,
    gint * size)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (client != NULL, ret);
  g_return_val_if_fail (client->mpd_root_node != NULL, ret);

  ret = gst_mpd_node_get_xml_buffer (GST_MPD_NODE (client->mpd_root_node),
      data, (int *) size);

  return ret;
}

GstDateTime *
gst_mpd_client_get_availability_start_time (GstMPDClient * client)
{
  GstDateTime *start_time;

  if (client == NULL)
    return (GstDateTime *) NULL;

  start_time = client->mpd_root_node->availabilityStartTime;
  if (start_time)
    gst_date_time_ref (start_time);
  return start_time;
}

void
gst_mpd_client_set_uri_downloader (GstMPDClient * client,
    GstUriDownloader * downloader)
{
  if (client->downloader)
    gst_object_unref (client->downloader);
  client->downloader = gst_object_ref (downloader);
}

void
gst_mpd_client_check_profiles (GstMPDClient * client)
{
  GST_DEBUG ("Profiles: %s",
      client->mpd_root_node->profiles ? client->mpd_root_node->
      profiles : "<none>");

  if (!client->mpd_root_node->profiles)
    return;

  if (g_strstr_len (client->mpd_root_node->profiles, -1,
          "urn:mpeg:dash:profile:isoff-on-demand:2011")) {
    client->profile_isoff_ondemand = TRUE;
    GST_DEBUG ("Found ISOFF on demand profile (2011)");
  }
}

void
gst_mpd_client_fetch_on_load_external_resources (GstMPDClient * client)
{
  GList *l;

  for (l = client->mpd_root_node->Periods; l; /* explicitly advanced below */ ) {
    GstMPDPeriodNode *period = l->data;
    GList *m;

    if (period->xlink_href && period->actuate == GST_MPD_XLINK_ACTUATE_ON_LOAD) {
      GList *new_periods, *prev, *next;

      new_periods = gst_mpd_client_fetch_external_periods (client, period);

      prev = l->prev;
      client->mpd_root_node->Periods =
          g_list_delete_link (client->mpd_root_node->Periods, l);
      gst_mpd_period_node_free (period);
      period = NULL;

      /* Get new next node, we will insert before this */
      if (prev)
        next = prev->next;
      else
        next = client->mpd_root_node->Periods;

      while (new_periods) {
        client->mpd_root_node->Periods =
            g_list_insert_before (client->mpd_root_node->Periods, next,
            new_periods->data);
        new_periods = g_list_delete_link (new_periods, new_periods);
      }
      next = NULL;

      /* Update our iterator to the first new period if any, or the next */
      if (prev)
        l = prev->next;
      else
        l = client->mpd_root_node->Periods;

      continue;
    }

    if (period->SegmentList && period->SegmentList->xlink_href
        && period->SegmentList->actuate == GST_MPD_XLINK_ACTUATE_ON_LOAD) {
      GstMPDSegmentListNode *new_segment_list;

      new_segment_list =
          gst_mpd_client_fetch_external_segment_list (client, period, NULL,
          NULL, NULL, period->SegmentList);

      gst_mpd_segment_list_node_free (period->SegmentList);
      period->SegmentList = new_segment_list;
    }

    for (m = period->AdaptationSets; m; /* explicitly advanced below */ ) {
      GstMPDAdaptationSetNode *adapt_set = m->data;
      GList *n;

      if (adapt_set->xlink_href
          && adapt_set->actuate == GST_MPD_XLINK_ACTUATE_ON_LOAD) {
        GList *new_adapt_sets, *prev, *next;

        new_adapt_sets =
            gst_mpd_client_fetch_external_adaptation_set (client, period,
            adapt_set);

        prev = m->prev;
        period->AdaptationSets = g_list_delete_link (period->AdaptationSets, m);
        gst_mpd_adaptation_set_node_free (adapt_set);
        adapt_set = NULL;

        /* Get new next node, we will insert before this */
        if (prev)
          next = prev->next;
        else
          next = period->AdaptationSets;

        while (new_adapt_sets) {
          period->AdaptationSets =
              g_list_insert_before (period->AdaptationSets, next,
              new_adapt_sets->data);
          new_adapt_sets = g_list_delete_link (new_adapt_sets, new_adapt_sets);
        }
        next = NULL;

        /* Update our iterator to the first new adapt_set if any, or the next */
        if (prev)
          m = prev->next;
        else
          m = period->AdaptationSets;

        continue;
      }

      if (adapt_set->SegmentList && adapt_set->SegmentList->xlink_href
          && adapt_set->SegmentList->actuate == GST_MPD_XLINK_ACTUATE_ON_LOAD) {
        GstMPDSegmentListNode *new_segment_list;

        new_segment_list =
            gst_mpd_client_fetch_external_segment_list (client, period,
            adapt_set, NULL, period->SegmentList, adapt_set->SegmentList);

        gst_mpd_segment_list_node_free (adapt_set->SegmentList);
        adapt_set->SegmentList = new_segment_list;
      }

      for (n = adapt_set->Representations; n; n = n->next) {
        GstMPDRepresentationNode *representation = n->data;

        if (representation->SegmentList
            && representation->SegmentList->xlink_href
            && representation->SegmentList->actuate ==
            GST_MPD_XLINK_ACTUATE_ON_LOAD) {

          GstMPDSegmentListNode *new_segment_list;

          new_segment_list =
              gst_mpd_client_fetch_external_segment_list (client, period,
              adapt_set, representation, adapt_set->SegmentList,
              representation->SegmentList);

          gst_mpd_segment_list_node_free (representation->SegmentList);
          representation->SegmentList = new_segment_list;

        }
      }

      m = m->next;
    }

    l = l->next;
  }
}


static GstStreamPeriod *
gst_mpd_client_get_stream_period (GstMPDClient * client)
{
  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->periods != NULL, NULL);

  return g_list_nth_data (client->periods, client->period_idx);
}

const gchar *
gst_mpd_client_get_baseURL (GstMPDClient * client, guint indexStream)
{
  GstActiveStream *stream;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->active_streams != NULL, NULL);
  stream = g_list_nth_data (client->active_streams, indexStream);
  g_return_val_if_fail (stream != NULL, NULL);

  return stream->baseURL;
}

/* select a stream and extract the baseURL (if present) */
gchar *
gst_mpd_client_parse_baseURL (GstMPDClient * client, GstActiveStream * stream,
    gchar ** query)
{
  GstStreamPeriod *stream_period;
  static const gchar empty[] = "";
  gchar *ret = NULL;
  GstUri *abs_url;

  g_return_val_if_fail (stream != NULL, g_strdup (empty));
  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, g_strdup (empty));
  g_return_val_if_fail (stream_period->period != NULL, g_strdup (empty));

  /* NULLify query return before we start */
  if (query)
    *query = NULL;

  /* initialise base url */
  abs_url =
      gst_uri_from_string (client->mpd_base_uri ? client->
      mpd_base_uri : client->mpd_uri);

  /* combine a BaseURL at the MPD level with the current base url */
  abs_url =
      gst_mpd_helper_combine_urls (abs_url, client->mpd_root_node->BaseURLs,
      query, stream->baseURL_idx);

  /* combine a BaseURL at the Period level with the current base url */
  abs_url =
      gst_mpd_helper_combine_urls (abs_url, stream_period->period->BaseURLs,
      query, stream->baseURL_idx);

  GST_DEBUG ("Current adaptation set id %i (%s)", stream->cur_adapt_set->id,
      stream->cur_adapt_set->contentType);
  /* combine a BaseURL at the AdaptationSet level with the current base url */
  abs_url =
      gst_mpd_helper_combine_urls (abs_url, stream->cur_adapt_set->BaseURLs,
      query, stream->baseURL_idx);

  /* combine a BaseURL at the Representation level with the current base url */
  abs_url =
      gst_mpd_helper_combine_urls (abs_url,
      stream->cur_representation->BaseURLs, query, stream->baseURL_idx);

  ret = gst_uri_to_string (abs_url);
  gst_uri_unref (abs_url);

  return ret;
}

static GstClockTime
gst_mpd_client_get_segment_end_time (GstMPDClient * client,
    GPtrArray * segments, const GstMediaSegment * segment, gint index)
{
  const GstStreamPeriod *stream_period;
  GstClockTime end;

  if (segment->repeat >= 0)
    return segment->start + (segment->repeat + 1) * segment->duration;

  if (index < segments->len - 1) {
    const GstMediaSegment *next_segment =
        g_ptr_array_index (segments, index + 1);
    end = next_segment->start;
  } else {
    stream_period = gst_mpd_client_get_stream_period (client);
    end = stream_period->start + stream_period->duration;
  }
  return end;
}

static gboolean
gst_mpd_client_add_media_segment (GstActiveStream * stream,
    GstMPDSegmentURLNode * url_node, guint number, gint repeat,
    guint64 scale_start, guint64 scale_duration,
    GstClockTime start, GstClockTime duration)
{
  GstMediaSegment *media_segment;

  g_return_val_if_fail (stream->segments != NULL, FALSE);

  media_segment = g_slice_new0 (GstMediaSegment);

  media_segment->SegmentURL = url_node;
  media_segment->number = number;
  media_segment->scale_start = scale_start;
  media_segment->scale_duration = scale_duration;
  media_segment->start = start;
  media_segment->duration = duration;
  media_segment->repeat = repeat;

  g_ptr_array_add (stream->segments, media_segment);
  GST_LOG ("Added new segment: number %d, repeat %d, "
      "ts: %" GST_TIME_FORMAT ", dur: %"
      GST_TIME_FORMAT, number, repeat,
      GST_TIME_ARGS (start), GST_TIME_ARGS (duration));

  return TRUE;
}

static void
gst_mpd_client_stream_update_presentation_time_offset (GstMPDClient * client,
    GstActiveStream * stream)
{
  GstMPDSegmentBaseNode *segbase = NULL;

  /* Find the used segbase */
  if (stream->cur_segment_list) {
    segbase =
        GST_MPD_MULT_SEGMENT_BASE_NODE (stream->cur_segment_list)->SegmentBase;
  } else if (stream->cur_seg_template) {
    segbase =
        GST_MPD_MULT_SEGMENT_BASE_NODE (stream->cur_seg_template)->SegmentBase;
  } else if (stream->cur_segment_base) {
    segbase = stream->cur_segment_base;
  }

  if (segbase) {
    /* Avoid overflows */
    stream->presentationTimeOffset =
        gst_util_uint64_scale (segbase->presentationTimeOffset, GST_SECOND,
        segbase->timescale);
  } else {
    stream->presentationTimeOffset = 0;
  }

  GST_LOG ("Setting stream's presentation time offset to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (stream->presentationTimeOffset));
}

gboolean
gst_mpd_client_setup_representation (GstMPDClient * client,
    GstActiveStream * stream, GstMPDRepresentationNode * representation)
{
  GstStreamPeriod *stream_period;
  GList *rep_list;
  GstClockTime PeriodStart, PeriodEnd, start_time, duration;
  guint i;
  guint64 start;

  if (stream->cur_adapt_set == NULL) {
    GST_WARNING ("No valid AdaptationSet node in the MPD file, aborting...");
    return FALSE;
  }

  rep_list = stream->cur_adapt_set->Representations;
  stream->cur_representation = representation;
  stream->representation_idx = g_list_index (rep_list, representation);

  /* clean the old segment list, if any */
  if (stream->segments) {
    g_ptr_array_unref (stream->segments);
    stream->segments = NULL;
  }

  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, FALSE);
  g_return_val_if_fail (stream_period->period != NULL, FALSE);

  PeriodStart = stream_period->start;
  if (GST_CLOCK_TIME_IS_VALID (stream_period->duration))
    PeriodEnd = stream_period->start + stream_period->duration;
  else
    PeriodEnd = GST_CLOCK_TIME_NONE;

  GST_LOG ("Building segment list for Period from %" GST_TIME_FORMAT " to %"
      GST_TIME_FORMAT, GST_TIME_ARGS (PeriodStart), GST_TIME_ARGS (PeriodEnd));

  if (representation->SegmentBase != NULL
      || representation->SegmentList != NULL) {
    GList *SegmentURL;

    /* We have a fixed list of segments for any of the cases here,
     * init the segments list */
    gst_mpdparser_init_active_stream_segments (stream);

    /* get the first segment_base of the selected representation */
    if ((stream->cur_segment_base =
            gst_mpd_client_get_segment_base (stream_period->period,
                stream->cur_adapt_set, representation)) == NULL) {
      GST_DEBUG ("No useful SegmentBase node for the current Representation");
    }

    /* get the first segment_list of the selected representation */
    if ((stream->cur_segment_list =
            gst_mpd_client_get_segment_list (client, stream_period->period,
                stream->cur_adapt_set, representation)) == NULL) {
      GST_DEBUG ("No useful SegmentList node for the current Representation");
      /* here we should have a single segment for each representation, whose URL is encoded in the baseURL element */
      if (!gst_mpd_client_add_media_segment (stream, NULL, 1, 0, 0,
              PeriodEnd - PeriodStart, PeriodStart, PeriodEnd - PeriodStart)) {
        return FALSE;
      }
    } else {
      /* build the list of GstMediaSegment nodes from the SegmentList node */
      SegmentURL = stream->cur_segment_list->SegmentURL;
      if (SegmentURL == NULL) {
        GST_WARNING
            ("No valid list of SegmentURL nodes in the MPD file, aborting...");
        return FALSE;
      }

      /* build segment list */
      i = GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
          cur_segment_list)->startNumber;
      start = 0;
      start_time = PeriodStart;

      GST_LOG ("Building media segment list using a SegmentList node");
      if (GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
              cur_segment_list)->SegmentTimeline) {
        GstMPDSegmentTimelineNode *timeline;
        GstMPDSNode *S;
        GList *list;
        GstClockTime presentationTimeOffset;
        GstMPDSegmentBaseNode *segbase;

        segbase =
            GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
            cur_segment_list)->SegmentBase;
        presentationTimeOffset =
            gst_util_uint64_scale (segbase->presentationTimeOffset, GST_SECOND,
            segbase->timescale);
        GST_LOG ("presentationTimeOffset = %" G_GUINT64_FORMAT,
            presentationTimeOffset);

        timeline =
            GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
            cur_segment_list)->SegmentTimeline;
        for (list = g_queue_peek_head_link (&timeline->S); list;
            list = g_list_next (list)) {
          guint timescale;

          S = (GstMPDSNode *) list->data;
          GST_LOG ("Processing S node: d=%" G_GUINT64_FORMAT " r=%d t=%"
              G_GUINT64_FORMAT, S->d, S->r, S->t);
          timescale =
              GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
              cur_segment_list)->SegmentBase->timescale;
          duration = gst_util_uint64_scale (S->d, GST_SECOND, timescale);

          if (S->t > 0) {
            start = S->t;
            start_time = gst_util_uint64_scale (S->t, GST_SECOND, timescale)
                + PeriodStart - presentationTimeOffset;
          }

          if (!SegmentURL) {
            GST_WARNING
                ("SegmentTimeline does not have a matching SegmentURL, aborting...");
            return FALSE;
          }

          if (!gst_mpd_client_add_media_segment (stream, SegmentURL->data, i,
                  S->r, start, S->d, start_time, duration)) {
            return FALSE;
          }
          i += S->r + 1;
          start_time += duration * (S->r + 1);
          start += S->d * (S->r + 1);
          SegmentURL = g_list_next (SegmentURL);
        }
      } else {
        guint64 scale_dur;

        duration =
            gst_mpd_client_get_segment_duration (client, stream, &scale_dur);
        if (!GST_CLOCK_TIME_IS_VALID (duration))
          return FALSE;

        while (SegmentURL) {
          if (!gst_mpd_client_add_media_segment (stream, SegmentURL->data, i,
                  0, start, scale_dur, start_time, duration)) {
            return FALSE;
          }
          i++;
          start += scale_dur;
          start_time += duration;
          SegmentURL = g_list_next (SegmentURL);
        }
      }
    }
  } else {
    if (representation->SegmentTemplate != NULL) {
      stream->cur_seg_template = representation->SegmentTemplate;
    } else if (stream->cur_adapt_set->SegmentTemplate != NULL) {
      stream->cur_seg_template = stream->cur_adapt_set->SegmentTemplate;
    } else if (stream_period->period->SegmentTemplate != NULL) {
      stream->cur_seg_template = stream_period->period->SegmentTemplate;
    }

    if (stream->cur_seg_template == NULL) {

      gst_mpdparser_init_active_stream_segments (stream);
      /* here we should have a single segment for each representation, whose URL is encoded in the baseURL element */
      if (!gst_mpd_client_add_media_segment (stream, NULL, 1, 0, 0,
              PeriodEnd - PeriodStart, 0, PeriodEnd - PeriodStart)) {
        return FALSE;
      }
    } else {
      GstClockTime presentationTimeOffset;
      GstMPDMultSegmentBaseNode *mult_seg =
          GST_MPD_MULT_SEGMENT_BASE_NODE (stream->cur_seg_template);
      presentationTimeOffset =
          gst_util_uint64_scale (mult_seg->SegmentBase->presentationTimeOffset,
          GST_SECOND, mult_seg->SegmentBase->timescale);
      GST_LOG ("presentationTimeOffset = %" GST_TIME_FORMAT,
          GST_TIME_ARGS (presentationTimeOffset));
      /* build segment list */
      i = mult_seg->startNumber;
      start = 0;
      start_time = 0;

      GST_LOG ("Building media segment list using this template: %s",
          stream->cur_seg_template->media);

      if (mult_seg->SegmentTimeline) {
        GstMPDSegmentTimelineNode *timeline;
        GstMPDSNode *S;
        GList *list;

        timeline = mult_seg->SegmentTimeline;
        gst_mpdparser_init_active_stream_segments (stream);
        for (list = g_queue_peek_head_link (&timeline->S); list;
            list = g_list_next (list)) {
          guint timescale;

          S = (GstMPDSNode *) list->data;
          GST_LOG ("Processing S node: d=%" G_GUINT64_FORMAT " r=%u t=%"
              G_GUINT64_FORMAT, S->d, S->r, S->t);
          timescale = mult_seg->SegmentBase->timescale;
          duration = gst_util_uint64_scale (S->d, GST_SECOND, timescale);
          if (S->t > 0) {
            start = S->t;
            start_time = gst_util_uint64_scale (S->t, GST_SECOND, timescale)
                + PeriodStart - presentationTimeOffset;
          }

          if (!gst_mpd_client_add_media_segment (stream, NULL, i, S->r, start,
                  S->d, start_time, duration)) {
            return FALSE;
          }
          i += S->r + 1;
          start += S->d * (S->r + 1);
          start_time += duration * (S->r + 1);
        }
      } else {
        /* NOP - The segment is created on demand with the template, no need
         * to build a list */
      }
    }
  }

  /* clip duration of segments to stop at period end */
  if (stream->segments && stream->segments->len) {
    if (GST_CLOCK_TIME_IS_VALID (PeriodEnd)) {
      guint n;

      for (n = 0; n < stream->segments->len; ++n) {
        GstMediaSegment *media_segment =
            g_ptr_array_index (stream->segments, n);
        if (media_segment) {
          if (media_segment->start + media_segment->duration > PeriodEnd) {
            GstClockTime stop = PeriodEnd;
            if (n < stream->segments->len - 1) {
              GstMediaSegment *next_segment =
                  g_ptr_array_index (stream->segments, n + 1);
              if (next_segment && next_segment->start < PeriodEnd)
                stop = next_segment->start;
            }
            media_segment->duration =
                media_segment->start > stop ? 0 : stop - media_segment->start;
            GST_LOG ("Fixed duration of segment %u: %" GST_TIME_FORMAT, n,
                GST_TIME_ARGS (media_segment->duration));

            /* If the segment was clipped entirely, we discard it and all
             * subsequent ones */
            if (media_segment->duration == 0) {
              GST_WARNING ("Discarding %u segments outside period",
                  stream->segments->len - n);
              /* _set_size should properly unref elements */
              g_ptr_array_set_size (stream->segments, n);
              break;
            }
          }
        }
      }
    }
#ifndef GST_DISABLE_GST_DEBUG
    if (stream->segments->len > 0) {
      GstMediaSegment *last_media_segment =
          g_ptr_array_index (stream->segments, stream->segments->len - 1);
      GST_LOG ("Built a list of %d segments", last_media_segment->number);
    } else {
      GST_LOG ("All media segments were clipped");
    }
#endif
  }

  g_free (stream->baseURL);
  g_free (stream->queryURL);
  stream->baseURL =
      gst_mpd_client_parse_baseURL (client, stream, &stream->queryURL);

  gst_mpd_client_stream_update_presentation_time_offset (client, stream);

  return TRUE;
}

#define CUSTOM_WRAPPER_START "<custom_wrapper>"
#define CUSTOM_WRAPPER_END "</custom_wrapper>"

static GList *
gst_mpd_client_fetch_external_periods (GstMPDClient * client,
    GstMPDPeriodNode * period_node)
{
  GstFragment *download;
  GstAdapter *adapter;
  GstBuffer *period_buffer;
  GError *err = NULL;

  GstUri *base_uri, *uri;
  gchar *query = NULL;
  gchar *uri_string, *wrapper;
  GList *new_periods = NULL;
  const gchar *data;

  /* ISO/IEC 23009-1:2014 5.5.3 4)
   * Remove nodes that resolve to nothing when resolving
   */
  if (strcmp (period_node->xlink_href,
          "urn:mpeg:dash:resolve-to-zero:2013") == 0) {
    return NULL;
  }

  if (!client->downloader) {
    return NULL;
  }

  /* Build absolute URI */

  /* Get base URI at the MPD level */
  base_uri =
      gst_uri_from_string (client->mpd_base_uri ? client->
      mpd_base_uri : client->mpd_uri);

  /* combine a BaseURL at the MPD level with the current base url */
  base_uri =
      gst_mpd_helper_combine_urls (base_uri, client->mpd_root_node->BaseURLs,
      &query, 0);
  uri = gst_uri_from_string_with_base (base_uri, period_node->xlink_href);
  if (query)
    gst_uri_set_query_string (uri, query);
  g_free (query);
  uri_string = gst_uri_to_string (uri);
  gst_uri_unref (base_uri);
  gst_uri_unref (uri);

  download =
      gst_uri_downloader_fetch_uri (client->downloader,
      uri_string, client->mpd_uri, TRUE, FALSE, TRUE, &err);
  g_free (uri_string);

  if (!download) {
    GST_ERROR ("Failed to download external Period node at '%s': %s",
        period_node->xlink_href, err->message);
    g_clear_error (&err);
    return NULL;
  }

  period_buffer = gst_fragment_get_buffer (download);
  g_object_unref (download);

  /* external xml could have multiple period without root xmlNode.
   * To avoid xml parsing error caused by no root node, wrapping it with
   * custom root node */
  adapter = gst_adapter_new ();

  wrapper = g_new (gchar, strlen (CUSTOM_WRAPPER_START));
  memcpy (wrapper, CUSTOM_WRAPPER_START, strlen (CUSTOM_WRAPPER_START));
  gst_adapter_push (adapter,
      gst_buffer_new_wrapped (wrapper, strlen (CUSTOM_WRAPPER_START)));

  gst_adapter_push (adapter, period_buffer);

  wrapper = g_strdup (CUSTOM_WRAPPER_END);
  gst_adapter_push (adapter,
      gst_buffer_new_wrapped (wrapper, strlen (CUSTOM_WRAPPER_END) + 1));

  data = gst_adapter_map (adapter, gst_adapter_available (adapter));

  new_periods =
      gst_mpdparser_get_external_periods (data,
      gst_adapter_available (adapter));

  gst_adapter_unmap (adapter);
  gst_adapter_clear (adapter);
  gst_object_unref (adapter);

  return new_periods;
}

gboolean
gst_mpd_client_setup_media_presentation (GstMPDClient * client,
    GstClockTime time, gint period_idx, const gchar * period_id)
{
  GstStreamPeriod *stream_period;
  GstClockTime start, duration;
  GList *list, *next;
  guint idx;
  gboolean ret = FALSE;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  /* Check if we set up the media presentation far enough already */
  for (list = client->periods; list; list = list->next) {
    GstStreamPeriod *stream_period = list->data;

    if ((time != GST_CLOCK_TIME_NONE
            && stream_period->duration != GST_CLOCK_TIME_NONE
            && stream_period->start + stream_period->duration >= time)
        || (time != GST_CLOCK_TIME_NONE && stream_period->start >= time))
      return TRUE;

    if (period_idx != -1 && stream_period->number >= period_idx)
      return TRUE;

    if (period_id != NULL && stream_period->period->id != NULL
        && strcmp (stream_period->period->id, period_id) == 0)
      return TRUE;

  }

  GST_DEBUG ("Building the list of Periods in the Media Presentation");
  /* clean the old period list, if any */
  /* TODO: In theory we could reuse the ones we have so far but that
   * seems more complicated than the overhead caused here
   */
  if (client->periods) {
    g_list_foreach (client->periods,
        (GFunc) gst_mpdparser_free_stream_period, NULL);
    g_list_free (client->periods);
    client->periods = NULL;
  }

  idx = 0;
  start = 0;
  duration = GST_CLOCK_TIME_NONE;

  if (client->mpd_root_node->mediaPresentationDuration <= 0 &&
      client->mpd_root_node->mediaPresentationDuration != -1) {
    /* Invalid MPD file: MPD duration is negative or zero */
    goto syntax_error;
  }

  for (list = client->mpd_root_node->Periods; list;
      /* explicitly advanced below */ ) {
    GstMPDPeriodNode *period_node = list->data;
    GstMPDPeriodNode *next_period_node = NULL;

    /* Download external period */
    if (period_node->xlink_href) {
      GList *new_periods;
      GList *prev;

      new_periods = gst_mpd_client_fetch_external_periods (client, period_node);

      prev = list->prev;
      client->mpd_root_node->Periods =
          g_list_delete_link (client->mpd_root_node->Periods, list);
      gst_mpd_period_node_free (period_node);
      period_node = NULL;

      /* Get new next node, we will insert before this */
      if (prev)
        next = prev->next;
      else
        next = client->mpd_root_node->Periods;

      while (new_periods) {
        client->mpd_root_node->Periods =
            g_list_insert_before (client->mpd_root_node->Periods, next,
            new_periods->data);
        new_periods = g_list_delete_link (new_periods, new_periods);
      }
      next = NULL;

      /* Update our iterator to the first new period if any, or the next */
      if (prev)
        list = prev->next;
      else
        list = client->mpd_root_node->Periods;

      /* And try again */
      continue;
    }

    if (period_node->start != -1) {
      /* we have a regular period */
      /* start cannot be smaller than previous start */
      if (list != g_list_first (client->mpd_root_node->Periods)
          && start >= period_node->start * GST_MSECOND) {
        /* Invalid MPD file: duration would be negative or zero */
        goto syntax_error;
      }
      start = period_node->start * GST_MSECOND;
    } else if (duration != GST_CLOCK_TIME_NONE) {
      /* start time inferred from previous period, this is still a regular period */
      start += duration;
    } else if (idx == 0
        && client->mpd_root_node->type == GST_MPD_FILE_TYPE_STATIC) {
      /* first period of a static MPD file, start time is 0 */
      start = 0;
    } else if (client->mpd_root_node->type == GST_MPD_FILE_TYPE_DYNAMIC) {
      /* this should be a live stream, let this pass */
    } else {
      /* this is an 'Early Available Period' */
      goto early;
    }

    /* compute duration.
       If there is a start time for the next period, or this is the last period
       and mediaPresentationDuration was set, those values will take precedence
       over a configured period duration in computing this period's duration

       ISO/IEC 23009-1:2014(E), chapter 5.3.2.1
       "The Period extends until the PeriodStart of the next Period, or until
       the end of the Media Presentation in the case of the last Period."
     */

    while ((next = g_list_next (list)) != NULL) {
      /* try to infer this period duration from the start time of the next period */
      next_period_node = next->data;

      if (next_period_node->xlink_href) {
        GList *new_periods;

        new_periods =
            gst_mpd_client_fetch_external_periods (client, next_period_node);

        client->mpd_root_node->Periods =
            g_list_delete_link (client->mpd_root_node->Periods, next);
        gst_mpd_period_node_free (next_period_node);
        next_period_node = NULL;
        /* Get new next node, we will insert before this */
        next = g_list_next (list);
        while (new_periods) {
          client->mpd_root_node->Periods =
              g_list_insert_before (client->mpd_root_node->Periods, next,
              new_periods->data);
          new_periods = g_list_delete_link (new_periods, new_periods);
        }

        /* And try again, getting the next list element which is now our newly
         * inserted nodes. If any */
      } else {
        /* Got the next period and it doesn't have to be downloaded first */
        break;
      }
    }

    if (next_period_node) {
      if (next_period_node->start != -1) {
        if (start >= next_period_node->start * GST_MSECOND) {
          /* Invalid MPD file: duration would be negative or zero */
          goto syntax_error;
        }
        duration = next_period_node->start * GST_MSECOND - start;
      } else if (period_node->duration != -1) {
        if (period_node->duration <= 0) {
          /* Invalid MPD file: duration would be negative or zero */
          goto syntax_error;
        }
        duration = period_node->duration * GST_MSECOND;
      } else if (client->mpd_root_node->type == GST_MPD_FILE_TYPE_DYNAMIC) {
        /* might be a live file, ignore unspecified duration */
      } else {
        /* Invalid MPD file! */
        goto syntax_error;
      }
    } else if (client->mpd_root_node->mediaPresentationDuration != -1) {
      /* last Period of the Media Presentation */
      if (client->mpd_root_node->mediaPresentationDuration * GST_MSECOND <=
          start) {
        /* Invalid MPD file: duration would be negative or zero */
        goto syntax_error;
      }
      duration =
          client->mpd_root_node->mediaPresentationDuration * GST_MSECOND -
          start;
    } else if (period_node->duration != -1) {
      duration = period_node->duration * GST_MSECOND;
    } else if (client->mpd_root_node->type == GST_MPD_FILE_TYPE_DYNAMIC) {
      /* might be a live file, ignore unspecified duration */
    } else {
      /* Invalid MPD file! */
      GST_ERROR
          ("Invalid MPD file. The MPD is static without a valid duration");
      goto syntax_error;
    }

    stream_period = g_slice_new0 (GstStreamPeriod);
    client->periods = g_list_append (client->periods, stream_period);
    stream_period->period = period_node;
    stream_period->number = idx++;
    stream_period->start = start;
    stream_period->duration = duration;
    ret = TRUE;
    GST_LOG (" - added Period %d start=%" GST_TIME_FORMAT " duration=%"
        GST_TIME_FORMAT, idx, GST_TIME_ARGS (start), GST_TIME_ARGS (duration));

    if ((time != GST_CLOCK_TIME_NONE
            && stream_period->duration != GST_CLOCK_TIME_NONE
            && stream_period->start + stream_period->duration >= time)
        || (time != GST_CLOCK_TIME_NONE && stream_period->start >= time))
      break;

    if (period_idx != -1 && stream_period->number >= period_idx)
      break;

    if (period_id != NULL && stream_period->period->id != NULL
        && strcmp (stream_period->period->id, period_id) == 0)
      break;

    list = list->next;
  }

  GST_DEBUG
      ("Found a total of %d valid Periods in the Media Presentation up to this point",
      idx);
  return ret;

early:
  GST_WARNING
      ("Found an Early Available Period, skipping the rest of the Media Presentation");
  return ret;

syntax_error:
  GST_WARNING
      ("Cannot get the duration of the Period %d, skipping the rest of the Media Presentation",
      idx);
  return ret;
}

static GList *
gst_mpd_client_fetch_external_adaptation_set (GstMPDClient * client,
    GstMPDPeriodNode * period, GstMPDAdaptationSetNode * adapt_set)
{
  GstFragment *download;
  GstBuffer *adapt_set_buffer;
  GstMapInfo map;
  GError *err = NULL;
  GstUri *base_uri, *uri;
  gchar *query = NULL;
  gchar *uri_string;
  GList *new_adapt_sets = NULL;

  /* ISO/IEC 23009-1:2014 5.5.3 4)
   * Remove nodes that resolve to nothing when resolving
   */
  if (strcmp (adapt_set->xlink_href, "urn:mpeg:dash:resolve-to-zero:2013") == 0) {
    return NULL;
  }

  if (!client->downloader) {
    return NULL;
  }

  /* Build absolute URI */

  /* Get base URI at the MPD level */
  base_uri =
      gst_uri_from_string (client->mpd_base_uri ? client->
      mpd_base_uri : client->mpd_uri);

  /* combine a BaseURL at the MPD level with the current base url */
  base_uri =
      gst_mpd_helper_combine_urls (base_uri, client->mpd_root_node->BaseURLs,
      &query, 0);

  /* combine a BaseURL at the Period level with the current base url */
  base_uri =
      gst_mpd_helper_combine_urls (base_uri, period->BaseURLs, &query, 0);

  uri = gst_uri_from_string_with_base (base_uri, adapt_set->xlink_href);
  if (query)
    gst_uri_set_query_string (uri, query);
  g_free (query);
  uri_string = gst_uri_to_string (uri);
  gst_uri_unref (base_uri);
  gst_uri_unref (uri);

  download =
      gst_uri_downloader_fetch_uri (client->downloader,
      uri_string, client->mpd_uri, TRUE, FALSE, TRUE, &err);
  g_free (uri_string);

  if (!download) {
    GST_ERROR ("Failed to download external AdaptationSet node at '%s': %s",
        adapt_set->xlink_href, err->message);
    g_clear_error (&err);
    return NULL;
  }

  adapt_set_buffer = gst_fragment_get_buffer (download);
  g_object_unref (download);

  gst_buffer_map (adapt_set_buffer, &map, GST_MAP_READ);

  new_adapt_sets =
      gst_mpdparser_get_external_adaptation_sets ((const gchar *) map.data,
      map.size, period);

  gst_buffer_unmap (adapt_set_buffer, &map);
  gst_buffer_unref (adapt_set_buffer);

  return new_adapt_sets;
}

static GList *
gst_mpd_client_get_adaptation_sets_for_period (GstMPDClient * client,
    GstStreamPeriod * period)
{
  GList *list;

  g_return_val_if_fail (period != NULL, NULL);

  /* Resolve all external adaptation sets of this period. Every user of
   * the adaptation sets would need to know the content of all adaptation sets
   * to decide which one to use, so we have to resolve them all here
   */
  for (list = period->period->AdaptationSets; list;
      /* advanced explicitly below */ ) {
    GstMPDAdaptationSetNode *adapt_set = (GstMPDAdaptationSetNode *) list->data;
    GList *new_adapt_sets = NULL, *prev, *next;

    if (!adapt_set->xlink_href) {
      list = list->next;
      continue;
    }

    new_adapt_sets =
        gst_mpd_client_fetch_external_adaptation_set (client, period->period,
        adapt_set);

    prev = list->prev;
    period->period->AdaptationSets =
        g_list_delete_link (period->period->AdaptationSets, list);
    gst_mpd_adaptation_set_node_free (adapt_set);
    adapt_set = NULL;

    /* Get new next node, we will insert before this */
    if (prev)
      next = prev->next;
    else
      next = period->period->AdaptationSets;

    while (new_adapt_sets) {
      period->period->AdaptationSets =
          g_list_insert_before (period->period->AdaptationSets, next,
          new_adapt_sets->data);
      new_adapt_sets = g_list_delete_link (new_adapt_sets, new_adapt_sets);
    }

    /* Update our iterator to the first new adaptation set if any, or the next */
    if (prev)
      list = prev->next;
    else
      list = period->period->AdaptationSets;
  }

  return period->period->AdaptationSets;
}

GList *
gst_mpd_client_get_adaptation_sets (GstMPDClient * client)
{
  GstStreamPeriod *stream_period;

  stream_period = gst_mpd_client_get_stream_period (client);
  if (stream_period == NULL || stream_period->period == NULL) {
    GST_DEBUG ("No more Period nodes in the MPD file, terminating...");
    return NULL;
  }

  return gst_mpd_client_get_adaptation_sets_for_period (client, stream_period);
}

gboolean
gst_mpd_client_setup_streaming (GstMPDClient * client,
    GstMPDAdaptationSetNode * adapt_set)
{
  GstMPDRepresentationNode *representation;
  GList *rep_list = NULL;
  GstActiveStream *stream;

  rep_list = adapt_set->Representations;
  if (!rep_list) {
    GST_WARNING ("Can not retrieve any representation, aborting...");
    return FALSE;
  }

  stream = g_slice_new0 (GstActiveStream);
  gst_mpdparser_init_active_stream_segments (stream);

  stream->baseURL_idx = 0;
  stream->cur_adapt_set = adapt_set;

  GST_DEBUG ("0. Current stream %p", stream);

#if 0
  /* fast start */
  representation =
      gst_mpdparser_get_representation_with_max_bandwidth (rep_list,
      stream->max_bandwidth);

  if (!representation) {
    GST_WARNING
        ("Can not retrieve a representation with the requested bandwidth");
    representation = gst_mpd_client_get_lowest_representation (rep_list);
  }
#else
  /* slow start */
  representation = gst_mpd_client_get_lowest_representation (rep_list);
#endif

  if (!representation) {
    GST_WARNING ("No valid representation in the MPD file, aborting...");
    gst_mpdparser_free_active_stream (stream);
    return FALSE;
  }
  stream->mimeType =
      gst_mpdparser_representation_get_mimetype (adapt_set, representation);
  if (stream->mimeType == GST_STREAM_UNKNOWN) {
    GST_WARNING ("Unknown mime type in the representation, aborting...");
    gst_mpdparser_free_active_stream (stream);
    return FALSE;
  }

  client->active_streams = g_list_append (client->active_streams, stream);
  if (!gst_mpd_client_setup_representation (client, stream, representation)) {
    GST_WARNING ("Failed to setup the representation, aborting...");
    return FALSE;
  }

  GST_INFO ("Successfully setup the download pipeline for mimeType %d",
      stream->mimeType);

  return TRUE;
}

gboolean
gst_mpd_client_stream_seek (GstMPDClient * client, GstActiveStream * stream,
    gboolean forward, GstSeekFlags flags, GstClockTime ts,
    GstClockTime * final_ts)
{
  gint index = 0;
  gint repeat_index = 0;
  GstMediaSegment *selectedChunk = NULL;

  g_return_val_if_fail (stream != NULL, 0);

  if (stream->segments) {
    for (index = 0; index < stream->segments->len; index++) {
      gboolean in_segment = FALSE;
      GstMediaSegment *segment = g_ptr_array_index (stream->segments, index);
      GstClockTime end_time;

      GST_DEBUG ("Looking at fragment sequence chunk %d / %d", index,
          stream->segments->len);

      end_time =
          gst_mpd_client_get_segment_end_time (client, stream->segments,
          segment, index);

      /* avoid downloading another fragment just for 1ns in reverse mode */
      if (forward)
        in_segment = ts < end_time;
      else
        in_segment = ts <= end_time;

      if (in_segment) {
        GstClockTime chunk_time;

        selectedChunk = segment;
        repeat_index =
            ((ts - segment->start) +
            ((GstMediaSegment *) stream->segments->pdata[0])->start) /
            segment->duration;

        chunk_time = segment->start + segment->duration * repeat_index;

        /* At the end of a segment in reverse mode, start from the previous fragment */
        if (!forward && repeat_index > 0
            && ((ts - segment->start) % segment->duration == 0))
          repeat_index--;

        if ((flags & GST_SEEK_FLAG_SNAP_NEAREST) == GST_SEEK_FLAG_SNAP_NEAREST) {
          if (repeat_index + 1 < segment->repeat) {
            if (ts - chunk_time > chunk_time + segment->duration - ts)
              repeat_index++;
          } else if (index + 1 < stream->segments->len) {
            GstMediaSegment *next_segment =
                g_ptr_array_index (stream->segments, index + 1);

            if (ts - chunk_time > next_segment->start - ts) {
              repeat_index = 0;
              selectedChunk = next_segment;
              index++;
            }
          }
        } else if (((forward && flags & GST_SEEK_FLAG_SNAP_AFTER) ||
                (!forward && flags & GST_SEEK_FLAG_SNAP_BEFORE)) &&
            ts != chunk_time) {

          if (repeat_index + 1 < segment->repeat) {
            repeat_index++;
          } else {
            repeat_index = 0;
            if (index + 1 >= stream->segments->len) {
              selectedChunk = NULL;
            } else {
              selectedChunk = g_ptr_array_index (stream->segments, ++index);
            }
          }
        }
        break;
      }
    }

    if (selectedChunk == NULL) {
      stream->segment_index = stream->segments->len;
      stream->segment_repeat_index = 0;
      GST_DEBUG ("Seek to after last segment");
      return FALSE;
    }

    if (final_ts)
      *final_ts = selectedChunk->start + selectedChunk->duration * repeat_index;
  } else {
    GstClockTime duration =
        gst_mpd_client_get_segment_duration (client, stream, NULL);
    GstStreamPeriod *stream_period = gst_mpd_client_get_stream_period (client);
    guint segments_count = gst_mpd_client_get_segments_counts (client, stream);
    GstClockTime index_time;

    g_return_val_if_fail (GST_MPD_MULT_SEGMENT_BASE_NODE
        (stream->cur_seg_template)->SegmentTimeline == NULL, FALSE);
    if (!GST_CLOCK_TIME_IS_VALID (duration) || duration == 0) {
      return FALSE;
    }

    if (ts > stream_period->start)
      ts -= stream_period->start;
    else
      ts = 0;

    index = ts / duration;

    /* At the end of a segment in reverse mode, start from the previous fragment */
    if (!forward && index > 0 && ts % duration == 0)
      index--;

    index_time = index * duration;

    if ((flags & GST_SEEK_FLAG_SNAP_NEAREST) == GST_SEEK_FLAG_SNAP_NEAREST) {
      if (ts - index_time > index_time + duration - ts)
        index++;
    } else if (((forward && flags & GST_SEEK_FLAG_SNAP_AFTER) ||
            (!forward && flags & GST_SEEK_FLAG_SNAP_BEFORE))
        && ts != index_time) {
      index++;
    }

    if (segments_count > 0 && index >= segments_count) {
      stream->segment_index = segments_count;
      stream->segment_repeat_index = 0;
      GST_DEBUG ("Seek to after last segment");
      return FALSE;
    }
    if (final_ts)
      *final_ts = index * duration;
  }

  stream->segment_repeat_index = repeat_index;
  stream->segment_index = index;

  return TRUE;
}

gint64
gst_mpd_client_calculate_time_difference (const GstDateTime * t1,
    const GstDateTime * t2)
{
  GDateTime *gdt1, *gdt2;
  GTimeSpan diff;

  g_assert (t1 != NULL && t2 != NULL);
  gdt1 = gst_date_time_to_g_date_time ((GstDateTime *) t1);
  gdt2 = gst_date_time_to_g_date_time ((GstDateTime *) t2);
  diff = g_date_time_difference (gdt2, gdt1);
  g_date_time_unref (gdt1);
  g_date_time_unref (gdt2);
  return diff * GST_USECOND;
}

GstDateTime *
gst_mpd_client_add_time_difference (GstDateTime * t1, gint64 usecs)
{
  GDateTime *gdt;
  GDateTime *gdt2;
  GstDateTime *rv;

  g_assert (t1 != NULL);
  gdt = gst_date_time_to_g_date_time (t1);
  g_assert (gdt != NULL);
  gdt2 = g_date_time_add (gdt, usecs);
  g_assert (gdt2 != NULL);
  g_date_time_unref (gdt);
  rv = gst_date_time_new_from_g_date_time (gdt2);

  /* Don't g_date_time_unref(gdt2) because gst_date_time_new_from_g_date_time takes
   * ownership of the GDateTime pointer.
   */

  return rv;
}

gboolean
gst_mpd_client_get_last_fragment_timestamp_end (GstMPDClient * client,
    guint stream_idx, GstClockTime * ts)
{
  GstActiveStream *stream;
  gint segment_idx;
  GstMediaSegment *currentChunk;
  GstStreamPeriod *stream_period;

  GST_DEBUG ("Stream index: %i", stream_idx);
  stream = g_list_nth_data (client->active_streams, stream_idx);
  g_return_val_if_fail (stream != NULL, 0);

  if (!stream->segments) {
    stream_period = gst_mpd_client_get_stream_period (client);
    *ts = stream_period->start + stream_period->duration;
  } else {
    segment_idx = gst_mpd_client_get_segments_counts (client, stream) - 1;
    if (segment_idx >= stream->segments->len) {
      GST_WARNING ("Segment index %d is outside of segment list of length %d",
          segment_idx, stream->segments->len);
      return FALSE;
    }
    currentChunk = g_ptr_array_index (stream->segments, segment_idx);

    if (currentChunk->repeat >= 0) {
      *ts =
          currentChunk->start + (currentChunk->duration * (1 +
              currentChunk->repeat));
    } else {
      /* 5.3.9.6.1: negative repeat means repeat till the end of the
       * period, or the next update of the MPD (which I think is
       * implicit, as this will all get deleted/recreated), or the
       * start of the next segment, if any. */
      stream_period = gst_mpd_client_get_stream_period (client);
      *ts = stream_period->start + stream_period->duration;
    }
  }

  return TRUE;
}

gboolean
gst_mpd_client_get_next_fragment_timestamp (GstMPDClient * client,
    guint stream_idx, GstClockTime * ts)
{
  GstActiveStream *stream;
  GstMediaSegment *currentChunk;

  GST_DEBUG ("Stream index: %i", stream_idx);
  stream = g_list_nth_data (client->active_streams, stream_idx);
  g_return_val_if_fail (stream != NULL, 0);

  if (stream->segments) {
    GST_DEBUG ("Looking for fragment sequence chunk %d / %d",
        stream->segment_index, stream->segments->len);
    if (stream->segment_index >= stream->segments->len)
      return FALSE;
    currentChunk = g_ptr_array_index (stream->segments, stream->segment_index);

    *ts =
        currentChunk->start +
        (currentChunk->duration * stream->segment_repeat_index);
  } else {
    GstClockTime duration =
        gst_mpd_client_get_segment_duration (client, stream, NULL);
    guint segments_count = gst_mpd_client_get_segments_counts (client, stream);

    g_return_val_if_fail (GST_MPD_MULT_SEGMENT_BASE_NODE
        (stream->cur_seg_template)->SegmentTimeline == NULL, FALSE);
    if (!GST_CLOCK_TIME_IS_VALID (duration) || (segments_count > 0
            && stream->segment_index >= segments_count)) {
      return FALSE;
    }
    *ts = stream->segment_index * duration;
  }

  return TRUE;
}

GstClockTime
gst_mpd_client_get_stream_presentation_offset (GstMPDClient * client,
    guint stream_idx)
{
  GstActiveStream *stream = NULL;

  g_return_val_if_fail (client != NULL, 0);
  g_return_val_if_fail (client->active_streams != NULL, 0);
  stream = g_list_nth_data (client->active_streams, stream_idx);
  g_return_val_if_fail (stream != NULL, 0);

  return stream->presentationTimeOffset;
}

GstClockTime
gst_mpd_client_get_period_start_time (GstMPDClient * client)
{
  GstStreamPeriod *stream_period = NULL;

  g_return_val_if_fail (client != NULL, 0);
  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, 0);

  return stream_period->start;
}

/**
 * gst_mpd_client_get_utc_timing_sources:
 * @client: #GstMPDClient to check for UTCTiming elements
 * @methods: A bit mask of #GstMPDUTCTimingType that specifies the methods
 *     to search for.
 * @selected_method: (nullable): The selected method
 * Returns: (transfer none): A NULL terminated array of URLs of servers
 *     that use @selected_method to provide a realtime clock.
 *
 * Searches the UTCTiming elements found in the manifest for an element
 * that uses one of the UTC timing methods specified in @selected_method.
 * If multiple UTCTiming elements are present that support one of the
 * methods specified in @selected_method, the first one is returned.
 *
 * Since: 1.6
 */
gchar **
gst_mpd_client_get_utc_timing_sources (GstMPDClient * client,
    guint methods, GstMPDUTCTimingType * selected_method)
{
  GList *list;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->mpd_root_node != NULL, NULL);
  for (list = g_list_first (client->mpd_root_node->UTCTimings); list;
      list = g_list_next (list)) {
    const GstMPDUTCTimingNode *node = (const GstMPDUTCTimingNode *) list->data;
    if (node->method & methods) {
      if (selected_method) {
        *selected_method = node->method;
      }
      return node->urls;
    }
  }
  return NULL;
}


gboolean
gst_mpd_client_get_next_fragment (GstMPDClient * client,
    guint indexStream, GstMediaFragmentInfo * fragment)
{
  GstActiveStream *stream = NULL;
  GstMediaSegment *currentChunk;
  gchar *mediaURL = NULL;
  gchar *indexURL = NULL;
  GstUri *base_url, *frag_url;

  /* select stream */
  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->active_streams != NULL, FALSE);
  stream = g_list_nth_data (client->active_streams, indexStream);
  g_return_val_if_fail (stream != NULL, FALSE);
  g_return_val_if_fail (stream->cur_representation != NULL, FALSE);

  if (stream->segments) {
    GST_DEBUG ("Looking for fragment sequence chunk %d / %d",
        stream->segment_index, stream->segments->len);
    if (stream->segment_index >= stream->segments->len)
      return FALSE;
  } else {
    GstClockTime duration = gst_mpd_client_get_segment_duration (client,
        stream, NULL);
    guint segments_count = gst_mpd_client_get_segments_counts (client, stream);

    g_return_val_if_fail (GST_MPD_MULT_SEGMENT_BASE_NODE
        (stream->cur_seg_template)->SegmentTimeline == NULL, FALSE);
    if (!GST_CLOCK_TIME_IS_VALID (duration) || (segments_count > 0
            && stream->segment_index >= segments_count)) {
      return FALSE;
    }
    fragment->duration = duration;
  }

  /* FIXME rework discont checking */
  /* fragment->discontinuity = segment_idx != currentChunk.number; */
  fragment->range_start = 0;
  fragment->range_end = -1;
  fragment->index_uri = NULL;
  fragment->index_range_start = 0;
  fragment->index_range_end = -1;

  if (stream->segments) {
    currentChunk = g_ptr_array_index (stream->segments, stream->segment_index);

    GST_DEBUG ("currentChunk->SegmentURL = %p", currentChunk->SegmentURL);
    if (currentChunk->SegmentURL != NULL) {
      mediaURL = gst_mpdparser_get_mediaURL (stream, currentChunk->SegmentURL);
      indexURL = g_strdup (currentChunk->SegmentURL->index);
    } else if (stream->cur_seg_template != NULL) {
      mediaURL =
          gst_mpdparser_build_URL_from_template (stream->cur_seg_template->
          media, stream->cur_representation->id,
          currentChunk->number + stream->segment_repeat_index,
          stream->cur_representation->bandwidth,
          currentChunk->scale_start +
          stream->segment_repeat_index * currentChunk->scale_duration);
      if (stream->cur_seg_template->index) {
        indexURL =
            gst_mpdparser_build_URL_from_template (stream->cur_seg_template->
            index, stream->cur_representation->id,
            currentChunk->number + stream->segment_repeat_index,
            stream->cur_representation->bandwidth,
            currentChunk->scale_start +
            stream->segment_repeat_index * currentChunk->scale_duration);
      }
    }
    GST_DEBUG ("mediaURL = %s", mediaURL);
    GST_DEBUG ("indexURL = %s", indexURL);

    fragment->timestamp =
        currentChunk->start +
        stream->segment_repeat_index * currentChunk->duration;
    fragment->duration = currentChunk->duration;
    if (currentChunk->SegmentURL) {
      if (currentChunk->SegmentURL->mediaRange) {
        fragment->range_start =
            currentChunk->SegmentURL->mediaRange->first_byte_pos;
        fragment->range_end =
            currentChunk->SegmentURL->mediaRange->last_byte_pos;
      }
      if (currentChunk->SegmentURL->indexRange) {
        fragment->index_range_start =
            currentChunk->SegmentURL->indexRange->first_byte_pos;
        fragment->index_range_end =
            currentChunk->SegmentURL->indexRange->last_byte_pos;
      }
    }
  } else {
    if (stream->cur_seg_template != NULL) {
      mediaURL =
          gst_mpdparser_build_URL_from_template (stream->cur_seg_template->
          media, stream->cur_representation->id,
          stream->segment_index +
          GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
              cur_seg_template)->startNumber,
          stream->cur_representation->bandwidth,
          stream->segment_index * fragment->duration);
      if (stream->cur_seg_template->index) {
        indexURL =
            gst_mpdparser_build_URL_from_template (stream->cur_seg_template->
            index, stream->cur_representation->id,
            stream->segment_index +
            GST_MPD_MULT_SEGMENT_BASE_NODE (stream->
                cur_seg_template)->startNumber,
            stream->cur_representation->bandwidth,
            stream->segment_index * fragment->duration);
      }
    } else {
      return FALSE;
    }

    GST_DEBUG ("mediaURL = %s", mediaURL);
    GST_DEBUG ("indexURL = %s", indexURL);

    fragment->timestamp = stream->segment_index * fragment->duration;
  }

  base_url = gst_uri_from_string (stream->baseURL);
  frag_url = gst_uri_from_string_with_base (base_url, mediaURL);
  g_free (mediaURL);
  if (stream->queryURL) {
    frag_url = gst_uri_make_writable (frag_url);
    gst_uri_set_query_string (frag_url, stream->queryURL);
  }
  fragment->uri = gst_uri_to_string (frag_url);
  gst_uri_unref (frag_url);

  if (indexURL != NULL) {
    frag_url = gst_uri_make_writable (gst_uri_from_string_with_base (base_url,
            indexURL));
    gst_uri_set_query_string (frag_url, stream->queryURL);
    fragment->index_uri = gst_uri_to_string (frag_url);
    gst_uri_unref (frag_url);
    g_free (indexURL);
  } else if (indexURL == NULL && (fragment->index_range_start
          || fragment->index_range_end != -1)) {
    /* index has no specific URL but has a range, we should only use this if
     * the media also has a range, otherwise we are serving some data twice
     * (in the media fragment and again in the index) */
    if (!(fragment->range_start || fragment->range_end != -1)) {
      GST_WARNING ("Ignoring index ranges because there isn't a media range "
          "and URIs would be the same");
      /* removing index information */
      fragment->index_range_start = 0;
      fragment->index_range_end = -1;
    }
  }

  gst_uri_unref (base_url);

  GST_DEBUG ("Loading chunk with URL %s", fragment->uri);

  return TRUE;
}

gboolean
gst_mpd_client_has_next_segment (GstMPDClient * client,
    GstActiveStream * stream, gboolean forward)
{
  if (forward) {
    guint segments_count = gst_mpd_client_get_segments_counts (client, stream);

    if (segments_count > 0 && stream->segments
        && stream->segment_index + 1 == segments_count) {
      GstMediaSegment *segment;

      segment = g_ptr_array_index (stream->segments, stream->segment_index);
      if (segment->repeat >= 0
          && stream->segment_repeat_index >= segment->repeat)
        return FALSE;
    } else if (segments_count > 0
        && stream->segment_index + 1 >= segments_count) {
      return FALSE;
    }
  } else {
    if (stream->segment_index < 0)
      return FALSE;
  }

  return TRUE;
}

GstFlowReturn
gst_mpd_client_advance_segment (GstMPDClient * client, GstActiveStream * stream,
    gboolean forward)
{
  GstMediaSegment *segment;
  GstFlowReturn ret = GST_FLOW_OK;
  guint segments_count = gst_mpd_client_get_segments_counts (client, stream);

  GST_DEBUG ("Advancing segment. Current: %d / %d r:%d", stream->segment_index,
      segments_count, stream->segment_repeat_index);

  /* handle special cases first */
  if (forward) {
    if (segments_count > 0 && stream->segment_index >= segments_count) {
      ret = GST_FLOW_EOS;
      goto done;
    }

    if (stream->segments == NULL) {
      if (stream->segment_index < 0) {
        stream->segment_index = 0;
      } else {
        stream->segment_index++;
        if (segments_count > 0 && stream->segment_index >= segments_count) {
          ret = GST_FLOW_EOS;
        }
      }
      goto done;
    }

    /* special case for when playback direction is reverted right at *
     * the end of the segment list */
    if (stream->segment_index < 0) {
      stream->segment_index = 0;
      goto done;
    }
  } else {
    if (stream->segments == NULL)
      stream->segment_index--;
    if (stream->segment_index < 0) {
      stream->segment_index = -1;
      ret = GST_FLOW_EOS;
      goto done;
    }
    if (stream->segments == NULL)
      goto done;

    /* special case for when playback direction is reverted right at *
     * the end of the segment list */
    if (stream->segment_index >= segments_count) {
      stream->segment_index = segments_count - 1;
      segment = g_ptr_array_index (stream->segments, stream->segment_index);
      if (segment->repeat >= 0) {
        stream->segment_repeat_index = segment->repeat;
      } else {
        GstClockTime start = segment->start;
        GstClockTime end =
            gst_mpd_client_get_segment_end_time (client, stream->segments,
            segment,
            stream->segment_index);
        stream->segment_repeat_index =
            (guint) (end - start) / segment->duration;
      }
      goto done;
    }
  }

  /* for the normal cases we can get the segment safely here */
  segment = g_ptr_array_index (stream->segments, stream->segment_index);
  if (forward) {
    if (segment->repeat >= 0 && stream->segment_repeat_index >= segment->repeat) {
      stream->segment_repeat_index = 0;
      stream->segment_index++;
      if (segments_count > 0 && stream->segment_index >= segments_count) {
        ret = GST_FLOW_EOS;
        goto done;
      }
    } else {
      stream->segment_repeat_index++;
    }
  } else {
    if (stream->segment_repeat_index == 0) {
      stream->segment_index--;
      if (stream->segment_index < 0) {
        ret = GST_FLOW_EOS;
        goto done;
      }

      segment = g_ptr_array_index (stream->segments, stream->segment_index);
      /* negative repeats only seem to make sense at the end of a list,
       * so this one will probably not be. Needs some sanity checking
       * when loading the XML data. */
      if (segment->repeat >= 0) {
        stream->segment_repeat_index = segment->repeat;
      } else {
        GstClockTime start = segment->start;
        GstClockTime end =
            gst_mpd_client_get_segment_end_time (client, stream->segments,
            segment,
            stream->segment_index);
        stream->segment_repeat_index =
            (guint) (end - start) / segment->duration;
      }
    } else {
      stream->segment_repeat_index--;
    }
  }

done:
  GST_DEBUG ("Advanced to segment: %d / %d r:%d (ret: %s)",
      stream->segment_index, segments_count,
      stream->segment_repeat_index, gst_flow_get_name (ret));
  return ret;
}

gboolean
gst_mpd_client_get_next_header (GstMPDClient * client, gchar ** uri,
    guint stream_idx, gint64 * range_start, gint64 * range_end)
{
  GstActiveStream *stream;
  GstStreamPeriod *stream_period;

  stream = gst_mpd_client_get_active_stream_by_index (client, stream_idx);
  g_return_val_if_fail (stream != NULL, FALSE);
  g_return_val_if_fail (stream->cur_representation != NULL, FALSE);
  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, FALSE);
  g_return_val_if_fail (stream_period->period != NULL, FALSE);

  *range_start = 0;
  *range_end = -1;

  GST_DEBUG ("Looking for current representation header");
  *uri = NULL;
  if (stream->cur_segment_base) {
    if (stream->cur_segment_base->Initialization) {
      *uri = gst_mpdparser_get_initializationURL (stream,
          stream->cur_segment_base->Initialization);
      if (stream->cur_segment_base->Initialization->range) {
        *range_start =
            stream->cur_segment_base->Initialization->range->first_byte_pos;
        *range_end =
            stream->cur_segment_base->Initialization->range->last_byte_pos;
      }
    } else if (stream->cur_segment_base->indexRange) {
      *uri = gst_mpdparser_get_initializationURL (stream,
          stream->cur_segment_base->Initialization);
      *range_start = 0;
      *range_end = stream->cur_segment_base->indexRange->first_byte_pos - 1;
    }
  } else if (stream->cur_seg_template
      && stream->cur_seg_template->initialization) {
    *uri =
        gst_mpdparser_build_URL_from_template (stream->cur_seg_template->
        initialization, stream->cur_representation->id, 0,
        stream->cur_representation->bandwidth, 0);
  }

  return *uri == NULL ? FALSE : TRUE;
}

gboolean
gst_mpd_client_get_next_header_index (GstMPDClient * client, gchar ** uri,
    guint stream_idx, gint64 * range_start, gint64 * range_end)
{
  GstActiveStream *stream;
  GstStreamPeriod *stream_period;

  stream = gst_mpd_client_get_active_stream_by_index (client, stream_idx);
  g_return_val_if_fail (stream != NULL, FALSE);
  g_return_val_if_fail (stream->cur_representation != NULL, FALSE);
  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, FALSE);
  g_return_val_if_fail (stream_period->period != NULL, FALSE);

  *range_start = 0;
  *range_end = -1;

  GST_DEBUG ("Looking for current representation index");
  *uri = NULL;
  if (stream->cur_segment_base && stream->cur_segment_base->indexRange) {
    *uri = gst_mpdparser_get_initializationURL (stream,
        stream->cur_segment_base->RepresentationIndex);
    *range_start = stream->cur_segment_base->indexRange->first_byte_pos;
    *range_end = stream->cur_segment_base->indexRange->last_byte_pos;
  } else if (stream->cur_seg_template && stream->cur_seg_template->index) {
    *uri =
        gst_mpdparser_build_URL_from_template (stream->cur_seg_template->index,
        stream->cur_representation->id, 0,
        stream->cur_representation->bandwidth, 0);
  }

  return *uri == NULL ? FALSE : TRUE;
}

GstClockTime
gst_mpd_client_get_next_fragment_duration (GstMPDClient * client,
    GstActiveStream * stream)
{
  GstMediaSegment *media_segment = NULL;
  gint seg_idx;

  g_return_val_if_fail (stream != NULL, 0);

  seg_idx = stream->segment_index;

  if (stream->segments) {
    if (seg_idx < stream->segments->len && seg_idx >= 0)
      media_segment = g_ptr_array_index (stream->segments, seg_idx);

    return media_segment == NULL ? 0 : media_segment->duration;
  } else {
    GstClockTime duration =
        gst_mpd_client_get_segment_duration (client, stream, NULL);
    guint segments_count = gst_mpd_client_get_segments_counts (client, stream);

    g_return_val_if_fail (GST_MPD_MULT_SEGMENT_BASE_NODE
        (stream->cur_seg_template)->SegmentTimeline == NULL, 0);

    if (!GST_CLOCK_TIME_IS_VALID (duration) || (segments_count > 0
            && seg_idx >= segments_count)) {
      return 0;
    }
    return duration;
  }
}

GstClockTime
gst_mpd_client_get_media_presentation_duration (GstMPDClient * client)
{
  GstClockTime duration;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);

  if (client->mpd_root_node->mediaPresentationDuration != -1) {
    duration = client->mpd_root_node->mediaPresentationDuration * GST_MSECOND;
  } else {
    /* We can only get the duration for on-demand streams */
    duration = GST_CLOCK_TIME_NONE;
  }

  return duration;
}

gboolean
gst_mpd_client_set_period_id (GstMPDClient * client, const gchar * period_id)
{
  GstStreamPeriod *next_stream_period;
  gboolean ret = FALSE;
  GList *iter;
  guint period_idx;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->periods != NULL, FALSE);
  g_return_val_if_fail (period_id != NULL, FALSE);

  if (!gst_mpd_client_setup_media_presentation (client, GST_CLOCK_TIME_NONE, -1,
          period_id))
    return FALSE;

  for (period_idx = 0, iter = client->periods; iter;
      period_idx++, iter = g_list_next (iter)) {
    next_stream_period = iter->data;

    if (next_stream_period->period->id
        && strcmp (next_stream_period->period->id, period_id) == 0) {
      ret = TRUE;
      client->period_idx = period_idx;
      break;
    }
  }

  return ret;
}

gboolean
gst_mpd_client_set_period_index (GstMPDClient * client, guint period_idx)
{
  GstStreamPeriod *next_stream_period;
  gboolean ret = FALSE;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->periods != NULL, FALSE);

  if (!gst_mpd_client_setup_media_presentation (client, -1, period_idx, NULL))
    return FALSE;

  next_stream_period = g_list_nth_data (client->periods, period_idx);
  if (next_stream_period != NULL) {
    client->period_idx = period_idx;
    ret = TRUE;
  }

  return ret;
}

guint
gst_mpd_client_get_period_index (GstMPDClient * client)
{
  guint period_idx;

  g_return_val_if_fail (client != NULL, 0);
  period_idx = client->period_idx;

  return period_idx;
}

const gchar *
gst_mpd_client_get_period_id (GstMPDClient * client)
{
  GstStreamPeriod *period;
  gchar *period_id = NULL;

  g_return_val_if_fail (client != NULL, 0);
  period = g_list_nth_data (client->periods, client->period_idx);
  if (period && period->period)
    period_id = period->period->id;

  return period_id;
}

gboolean
gst_mpd_client_has_next_period (GstMPDClient * client)
{
  GList *next_stream_period;
  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->periods != NULL, FALSE);

  if (!gst_mpd_client_setup_media_presentation (client, GST_CLOCK_TIME_NONE,
          client->period_idx + 1, NULL))
    return FALSE;

  next_stream_period =
      g_list_nth_data (client->periods, client->period_idx + 1);
  return next_stream_period != NULL;
}

gboolean
gst_mpd_client_has_previous_period (GstMPDClient * client)
{
  GList *next_stream_period;
  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->periods != NULL, FALSE);

  if (!gst_mpd_client_setup_media_presentation (client, GST_CLOCK_TIME_NONE,
          client->period_idx - 1, NULL))
    return FALSE;

  next_stream_period =
      g_list_nth_data (client->periods, client->period_idx - 1);

  return next_stream_period != NULL;
}

gint
gst_mpd_client_get_rep_idx_with_min_bandwidth (GList * Representations)
{
  GList *list = NULL, *lowest = NULL;
  GstMPDRepresentationNode *rep = NULL;
  gint lowest_bandwidth = -1;

  if (Representations == NULL)
    return -1;

  for (list = g_list_first (Representations); list; list = g_list_next (list)) {
    rep = (GstMPDRepresentationNode *) list->data;
    if (rep && (!lowest || rep->bandwidth < lowest_bandwidth)) {
      lowest = list;
      lowest_bandwidth = rep->bandwidth;
    }
  }

  return lowest ? g_list_position (Representations, lowest) : -1;
}

gint
gst_mpd_client_get_rep_idx_with_max_bandwidth (GList * Representations,
    gint64 max_bandwidth, gint max_video_width, gint max_video_height, gint
    max_video_framerate_n, gint max_video_framerate_d)
{
  GList *list = NULL, *best = NULL;
  GstMPDRepresentationNode *representation;
  gint best_bandwidth = 0;

  GST_DEBUG ("max_bandwidth = %" G_GINT64_FORMAT, max_bandwidth);

  if (Representations == NULL)
    return -1;

  if (max_bandwidth <= 0)       /* 0 => get lowest representation available */
    return gst_mpd_client_get_rep_idx_with_min_bandwidth (Representations);

  for (list = g_list_first (Representations); list; list = g_list_next (list)) {
    GstXMLFrameRate *framerate = NULL;

    representation = (GstMPDRepresentationNode *) list->data;

    /* FIXME: Really? */
    if (!representation)
      continue;

    framerate = GST_MPD_REPRESENTATION_BASE_NODE (representation)->frameRate;
    if (!framerate)
      framerate =
          GST_MPD_REPRESENTATION_BASE_NODE (representation)->maxFrameRate;

    if (framerate && max_video_framerate_n > 0) {
      if (gst_util_fraction_compare (framerate->num, framerate->den,
              max_video_framerate_n, max_video_framerate_d) > 0)
        continue;
    }

    if (max_video_width > 0
        && GST_MPD_REPRESENTATION_BASE_NODE (representation)->width >
        max_video_width)
      continue;
    if (max_video_height > 0
        && GST_MPD_REPRESENTATION_BASE_NODE (representation)->height >
        max_video_height)
      continue;

    if (representation->bandwidth <= max_bandwidth &&
        representation->bandwidth > best_bandwidth) {
      best = list;
      best_bandwidth = representation->bandwidth;
    }
  }

  return best ? g_list_position (Representations, best) : -1;
}

void
gst_mpd_client_seek_to_first_segment (GstMPDClient * client)
{
  GList *list;

  g_return_if_fail (client != NULL);
  g_return_if_fail (client->active_streams != NULL);

  for (list = g_list_first (client->active_streams); list;
      list = g_list_next (list)) {
    GstActiveStream *stream = (GstActiveStream *) list->data;
    if (stream) {
      stream->segment_index = 0;
      stream->segment_repeat_index = 0;
    }
  }
}

static guint
gst_mpd_client_get_segments_counts (GstMPDClient * client,
    GstActiveStream * stream)
{
  GstStreamPeriod *stream_period;

  g_return_val_if_fail (stream != NULL, 0);

  if (stream->segments)
    return stream->segments->len;
  g_return_val_if_fail (GST_MPD_MULT_SEGMENT_BASE_NODE
      (stream->cur_seg_template)->SegmentTimeline == NULL, 0);

  stream_period = gst_mpd_client_get_stream_period (client);
  if (stream_period->duration != -1)
    return gst_util_uint64_scale_ceil (stream_period->duration, 1,
        gst_mpd_client_get_segment_duration (client, stream, NULL));

  return 0;
}

gboolean
gst_mpd_client_is_live (GstMPDClient * client)
{
  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  return client->mpd_root_node->type == GST_MPD_FILE_TYPE_DYNAMIC;
}

guint
gst_mpd_client_get_nb_active_stream (GstMPDClient * client)
{
  g_return_val_if_fail (client != NULL, 0);

  return g_list_length (client->active_streams);
}

guint
gst_mpd_client_get_nb_adaptationSet (GstMPDClient * client)
{
  GstStreamPeriod *stream_period;

  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, 0);
  g_return_val_if_fail (stream_period->period != NULL, 0);

  return g_list_length (stream_period->period->AdaptationSets);
}

GstActiveStream *
gst_mpd_client_get_active_stream_by_index (GstMPDClient * client,
    guint stream_idx)
{
  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->active_streams != NULL, NULL);

  return g_list_nth_data (client->active_streams, stream_idx);
}

gboolean
gst_mpd_client_active_stream_contains_subtitles (GstActiveStream * stream)
{
  const gchar *mimeType;
  const gchar *adapt_set_codecs;
  const gchar *rep_codecs;

  mimeType =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_representation)->mimeType;
  if (!mimeType)
    mimeType =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->mimeType;

  if (g_strcmp0 (mimeType, "application/ttml+xml") == 0 ||
      g_strcmp0 (mimeType, "text/vtt") == 0)
    return TRUE;

  adapt_set_codecs =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->codecs;
  rep_codecs =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_representation)->codecs;

  return (adapt_set_codecs && g_str_has_prefix (adapt_set_codecs, "stpp"))
      || (rep_codecs && g_str_has_prefix (rep_codecs, "stpp"));
}

GstCaps *
gst_mpd_client_get_stream_caps (GstActiveStream * stream)
{
  const gchar *mimeType, *caps_string;
  GstCaps *ret = NULL;

  if (stream == NULL || stream->cur_adapt_set == NULL
      || stream->cur_representation == NULL)
    return NULL;

  mimeType =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_representation)->mimeType;
  if (mimeType == NULL) {
    mimeType =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->mimeType;
  }

  caps_string = gst_mpd_helper_mimetype_to_caps (mimeType);

  if ((g_strcmp0 (caps_string, "application/mp4") == 0)
      && gst_mpd_client_active_stream_contains_subtitles (stream))
    caps_string = "video/quicktime";

  if (caps_string)
    ret = gst_caps_from_string (caps_string);

  return ret;
}

gboolean
gst_mpd_client_get_bitstream_switching_flag (GstActiveStream * stream)
{
  if (stream == NULL || stream->cur_adapt_set == NULL)
    return FALSE;

  return stream->cur_adapt_set->bitstreamSwitching;
}

guint
gst_mpd_client_get_video_stream_width (GstActiveStream * stream)
{
  guint width;

  if (stream == NULL || stream->cur_adapt_set == NULL
      || stream->cur_representation == NULL)
    return 0;

  width = GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_representation)->width;
  if (width == 0) {
    width = GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->width;
  }

  return width;
}

guint
gst_mpd_client_get_video_stream_height (GstActiveStream * stream)
{
  guint height;

  if (stream == NULL || stream->cur_adapt_set == NULL
      || stream->cur_representation == NULL)
    return 0;

  height =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_representation)->height;
  if (height == 0) {
    height = GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->height;
  }

  return height;
}

gboolean
gst_mpd_client_get_video_stream_framerate (GstActiveStream * stream,
    gint * fps_num, gint * fps_den)
{
  if (stream == NULL)
    return FALSE;

  if (stream->cur_adapt_set &&
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->frameRate !=
      NULL) {
    *fps_num =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->
        frameRate->num;
    *fps_den =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->
        frameRate->den;
    return TRUE;
  }

  if (stream->cur_adapt_set &&
      GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->maxFrameRate !=
      NULL) {
    *fps_num =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->
        maxFrameRate->num;
    *fps_den =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->cur_adapt_set)->
        maxFrameRate->den;
    return TRUE;
  }

  if (stream->cur_representation &&
      GST_MPD_REPRESENTATION_BASE_NODE (stream->
          cur_representation)->frameRate != NULL) {
    *fps_num =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->
        cur_representation)->frameRate->num;
    *fps_den =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->
        cur_representation)->frameRate->den;
    return TRUE;
  }

  if (stream->cur_representation &&
      GST_MPD_REPRESENTATION_BASE_NODE (stream->
          cur_representation)->maxFrameRate != NULL) {
    *fps_num =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->
        cur_representation)->maxFrameRate->num;
    *fps_den =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->
        cur_representation)->maxFrameRate->den;
    return TRUE;
  }

  return FALSE;
}

guint
gst_mpd_client_get_audio_stream_rate (GstActiveStream * stream)
{
  const gchar *rate;

  if (stream == NULL || stream->cur_adapt_set == NULL
      || stream->cur_representation == NULL)
    return 0;

  rate =
      GST_MPD_REPRESENTATION_BASE_NODE (stream->
      cur_representation)->audioSamplingRate;
  if (rate == NULL) {
    rate =
        GST_MPD_REPRESENTATION_BASE_NODE (stream->
        cur_adapt_set)->audioSamplingRate;
  }

  return rate ? atoi (rate) : 0;
}

guint
gst_mpd_client_get_audio_stream_num_channels (GstActiveStream * stream)
{
  if (stream == NULL || stream->cur_adapt_set == NULL
      || stream->cur_representation == NULL)
    return 0;
  /* TODO: here we have to parse the AudioChannelConfiguration descriptors */
  return 0;
}

guint
gst_mpd_client_get_list_and_nb_of_audio_language (GstMPDClient * client,
    GList ** lang)
{
  GstStreamPeriod *stream_period;
  GstMPDAdaptationSetNode *adapt_set;
  GList *adaptation_sets, *list;
  const gchar *this_mimeType = "audio";
  gchar *mimeType = NULL;
  guint nb_adaptation_set = 0;

  stream_period = gst_mpd_client_get_stream_period (client);
  g_return_val_if_fail (stream_period != NULL, 0);
  g_return_val_if_fail (stream_period->period != NULL, 0);

  adaptation_sets =
      gst_mpd_client_get_adaptation_sets_for_period (client, stream_period);
  for (list = adaptation_sets; list; list = g_list_next (list)) {
    adapt_set = (GstMPDAdaptationSetNode *) list->data;
    if (adapt_set && adapt_set->lang) {
      gchar *this_lang = adapt_set->lang;
      GstMPDRepresentationNode *rep;
      rep =
          gst_mpd_client_get_lowest_representation (adapt_set->Representations);
      mimeType = NULL;
      if (GST_MPD_REPRESENTATION_BASE_NODE (rep))
        mimeType = GST_MPD_REPRESENTATION_BASE_NODE (rep)->mimeType;
      if (!mimeType && GST_MPD_REPRESENTATION_BASE_NODE (adapt_set)) {
        mimeType = GST_MPD_REPRESENTATION_BASE_NODE (adapt_set)->mimeType;
      }

      if (gst_mpd_helper_strncmp_ext (mimeType, this_mimeType) == 0) {
        nb_adaptation_set++;
        *lang = g_list_append (*lang, this_lang);
      }
    }
  }

  return nb_adaptation_set;
}


GstDateTime *
gst_mpd_client_get_next_segment_availability_start_time (GstMPDClient * client,
    GstActiveStream * stream)
{
  GstDateTime *availability_start_time, *rv;
  gint seg_idx;
  GstMediaSegment *segment;
  GstClockTime segmentEndTime;
  const GstStreamPeriod *stream_period;
  GstClockTime period_start = 0;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (stream != NULL, NULL);

  stream_period = gst_mpd_client_get_stream_period (client);
  if (stream_period && stream_period->period) {
    period_start = stream_period->start;
  }

  seg_idx = stream->segment_index;

  if (stream->segments) {
    segment = g_ptr_array_index (stream->segments, seg_idx);

    if (segment->repeat >= 0) {
      segmentEndTime = segment->start + (stream->segment_repeat_index + 1) *
          segment->duration;
    } else if (seg_idx < stream->segments->len - 1) {
      const GstMediaSegment *next_segment =
          g_ptr_array_index (stream->segments, seg_idx + 1);
      segmentEndTime = next_segment->start;
    } else {
      g_return_val_if_fail (stream_period != NULL, NULL);
      segmentEndTime = period_start + stream_period->duration;
    }
  } else {
    GstClockTime seg_duration;
    seg_duration = gst_mpd_client_get_segment_duration (client, stream, NULL);
    if (seg_duration == 0)
      return NULL;
    segmentEndTime = period_start + (1 + seg_idx) * seg_duration;
  }

  availability_start_time = gst_mpd_client_get_availability_start_time (client);
  if (availability_start_time == NULL) {
    GST_WARNING_OBJECT (client, "Failed to get availability_start_time");
    return NULL;
  }

  rv = gst_mpd_client_add_time_difference (availability_start_time,
      segmentEndTime / GST_USECOND);
  gst_date_time_unref (availability_start_time);
  if (rv == NULL) {
    GST_WARNING_OBJECT (client, "Failed to offset availability_start_time");
    return NULL;
  }

  return rv;
}

gboolean
gst_mpd_client_seek_to_time (GstMPDClient * client, GDateTime * time)
{
  GDateTime *start;
  GTimeSpan ts_microseconds;
  GstClockTime ts;
  gboolean ret = TRUE;
  GList *stream;

  g_return_val_if_fail (gst_mpd_client_is_live (client), FALSE);
  g_return_val_if_fail (client->mpd_root_node->availabilityStartTime != NULL,
      FALSE);

  start =
      gst_date_time_to_g_date_time (client->mpd_root_node->
      availabilityStartTime);

  ts_microseconds = g_date_time_difference (time, start);
  g_date_time_unref (start);

  /* Clamp to availability start time, otherwise calculations wrap around */
  if (ts_microseconds < 0)
    ts_microseconds = 0;

  ts = ts_microseconds * GST_USECOND;
  for (stream = client->active_streams; stream; stream = g_list_next (stream)) {
    ret =
        ret & gst_mpd_client_stream_seek (client, stream->data, TRUE, 0, ts,
        NULL);
  }
  return ret;
}

gboolean
gst_mpd_client_has_isoff_ondemand_profile (GstMPDClient * client)
{
  return client->profile_isoff_ondemand;
}

/**
 * gst_mpd_client_parse_default_presentation_delay:
 * @client: #GstMPDClient that has a parsed manifest
 * @default_presentation_delay: A string that specifies a time period
 * in fragments (e.g. "5 f"), seconds ("12 s") or milliseconds
 * ("12000 ms")
 * Returns: the parsed string in milliseconds
 *
 * Since: 1.6
 */
gint64
gst_mpd_client_parse_default_presentation_delay (GstMPDClient * client,
    const gchar * default_presentation_delay)
{
  gint64 value;
  char *endptr = NULL;

  g_return_val_if_fail (client != NULL, 0);
  g_return_val_if_fail (default_presentation_delay != NULL, 0);
  value = strtol (default_presentation_delay, &endptr, 10);
  if (endptr == default_presentation_delay || value == 0) {
    return 0;
  }
  while (*endptr == ' ')
    endptr++;
  if (*endptr == 's' || *endptr == 'S') {
    value *= 1000;              /* convert to ms */
  } else if (*endptr == 'f' || *endptr == 'F') {
    gint64 segment_duration;
    g_assert (client->mpd_root_node != NULL);
    segment_duration = client->mpd_root_node->maxSegmentDuration;
    value *= segment_duration;
  } else if (*endptr != 'm' && *endptr != 'M') {
    GST_ERROR ("Unable to parse default presentation delay: %s",
        default_presentation_delay);
    value = 0;
  }
  return value;
}

GstClockTime
gst_mpd_client_get_maximum_segment_duration (GstMPDClient * client)
{
  GstClockTime ret = GST_CLOCK_TIME_NONE, dur;
  GList *stream;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);
  g_return_val_if_fail (client->mpd_root_node != NULL, GST_CLOCK_TIME_NONE);

  if (client->mpd_root_node->maxSegmentDuration != GST_MPD_DURATION_NONE) {
    return client->mpd_root_node->maxSegmentDuration * GST_MSECOND;
  }

  /* According to the DASH specification, if maxSegmentDuration is not present:
     "If not present, then the maximum Segment duration shall be the maximum
     duration of any Segment documented in this MPD"
   */
  for (stream = client->active_streams; stream; stream = g_list_next (stream)) {
    dur = gst_mpd_client_get_segment_duration (client, stream->data, NULL);
    if (dur != GST_CLOCK_TIME_NONE && (dur > ret || ret == GST_CLOCK_TIME_NONE)) {
      ret = dur;
    }
  }
  return ret;
}

guint
gst_mpd_client_get_period_index_at_time (GstMPDClient * client,
    GstDateTime * time)
{
  GList *iter;
  guint period_idx = G_MAXUINT;
  guint idx;
  gint64 time_offset;
  GstDateTime *avail_start =
      gst_mpd_client_get_availability_start_time (client);
  GstStreamPeriod *stream_period;

  if (avail_start == NULL)
    return 0;

  time_offset = gst_mpd_client_calculate_time_difference (avail_start, time);
  gst_date_time_unref (avail_start);

  if (time_offset < 0)
    return 0;

  if (!gst_mpd_client_setup_media_presentation (client, time_offset, -1, NULL))
    return 0;

  for (idx = 0, iter = client->periods; iter; idx++, iter = g_list_next (iter)) {
    stream_period = iter->data;
    if (stream_period->start <= time_offset
        && (!GST_CLOCK_TIME_IS_VALID (stream_period->duration)
            || stream_period->start + stream_period->duration > time_offset)) {
      period_idx = idx;
      break;
    }
  }

  return period_idx;
}

/* add or set node methods */

gboolean
gst_mpd_client_set_root_node (GstMPDClient * client,
    const gchar * property_name, ...)
{
  va_list myargs;
  g_return_val_if_fail (client != NULL, FALSE);

  if (!client->mpd_root_node)
    client->mpd_root_node = gst_mpd_root_node_new ();

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (client->mpd_root_node), property_name, myargs);
  va_end (myargs);

  return TRUE;
}

gboolean
gst_mpd_client_add_baseurl_node (GstMPDClient * client,
    const gchar * property_name, ...)
{
  GstMPDBaseURLNode *baseurl_node = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  va_start (myargs, property_name);

  baseurl_node = gst_mpd_baseurl_node_new ();
  g_object_set_valist (G_OBJECT (baseurl_node), property_name, myargs);
  client->mpd_root_node->BaseURLs =
      g_list_append (client->mpd_root_node->BaseURLs, baseurl_node);

  va_end (myargs);
  return TRUE;
}

/* returns a period id */
gchar *
gst_mpd_client_set_period_node (GstMPDClient * client,
    gchar * period_id, const gchar * property_name, ...)
{
  GstMPDPeriodNode *period_node = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->mpd_root_node != NULL, NULL);

  period_node =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  if (!period_node) {
    period_node = gst_mpd_period_node_new ();
    if (period_id)
      period_node->id = g_strdup (period_id);
    else
      period_node->id =
          _generate_new_string_id (client->mpd_root_node->Periods,
          "period_%.2d", gst_mpd_client_get_period_with_id);
    client->mpd_root_node->Periods =
        g_list_append (client->mpd_root_node->Periods, period_node);
  }

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (period_node), property_name, myargs);
  va_end (myargs);

  return period_node->id;
}

/* returns an adaptation set id */
guint
gst_mpd_client_set_adaptation_set_node (GstMPDClient * client,
    gchar * period_id, guint adaptation_set_id, const gchar * property_name,
    ...)
{
  GstMPDAdaptationSetNode *adap_node = NULL;
  GstMPDPeriodNode *period_node = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, 0);
  g_return_val_if_fail (client->mpd_root_node != NULL, 0);

  period_node =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  g_return_val_if_fail (period_node != NULL, 0);
  adap_node =
      GST_MPD_ADAPTATION_SET_NODE (gst_mpd_client_get_adaptation_set_with_id
      (period_node->AdaptationSets, adaptation_set_id));
  if (!adap_node) {
    adap_node = gst_mpd_adaptation_set_node_new ();
    if (adaptation_set_id)
      adap_node->id = adaptation_set_id;
    else
      adap_node->id =
          _generate_new_id (period_node->AdaptationSets,
          gst_mpd_client_get_adaptation_set_with_id);
    GST_DEBUG_OBJECT (client, "Add a new adaptation set with id %d",
        adap_node->id);
    period_node->AdaptationSets =
        g_list_append (period_node->AdaptationSets, adap_node);
  }

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (adap_node), property_name, myargs);
  va_end (myargs);

  return adap_node->id;
}

/* returns a representation id */
gchar *
gst_mpd_client_set_representation_node (GstMPDClient * client,
    gchar * period_id, guint adaptation_set_id, gchar * representation_id,
    const gchar * property_name, ...)
{
  GstMPDRepresentationNode *rep_node = NULL;
  GstMPDAdaptationSetNode *adap_set_node = NULL;
  GstMPDPeriodNode *period_node = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->mpd_root_node != NULL, NULL);

  period_node =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  adap_set_node =
      GST_MPD_ADAPTATION_SET_NODE (gst_mpd_client_get_adaptation_set_with_id
      (period_node->AdaptationSets, adaptation_set_id));
  g_return_val_if_fail (adap_set_node != NULL, NULL);
  rep_node =
      gst_mpd_client_get_representation_with_id (adap_set_node->Representations,
      representation_id);
  if (!rep_node) {
    rep_node = gst_mpd_representation_node_new ();
    if (representation_id)
      rep_node->id = g_strdup (representation_id);
    else
      rep_node->id =
          _generate_new_string_id (adap_set_node->Representations,
          "representation_%.2d",
          gst_mpd_client_get_representation_with_id_filter);
    GST_DEBUG_OBJECT (client, "Add a new representation with id %s",
        rep_node->id);
    adap_set_node->Representations =
        g_list_append (adap_set_node->Representations, rep_node);
  }

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (rep_node), property_name, myargs);
  va_end (myargs);

  return rep_node->id;
}

/* add/set a segment list node */
gboolean
gst_mpd_client_set_segment_list (GstMPDClient * client,
    gchar * period_id, guint adap_set_id, gchar * rep_id,
    const gchar * property_name, ...)
{
  GstMPDRepresentationNode *representation = NULL;
  GstMPDAdaptationSetNode *adaptation_set = NULL;
  GstMPDPeriodNode *period = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  period =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  adaptation_set =
      GST_MPD_ADAPTATION_SET_NODE (gst_mpd_client_get_adaptation_set_with_id
      (period->AdaptationSets, adap_set_id));
  g_return_val_if_fail (adaptation_set != NULL, FALSE);

  representation =
      gst_mpd_client_get_representation_with_id
      (adaptation_set->Representations, rep_id);
  if (!representation->SegmentList) {
    representation->SegmentList = gst_mpd_segment_list_node_new ();
  }

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (representation->SegmentList), property_name,
      myargs);
  va_end (myargs);

  return TRUE;
}

/* add/set a segment template node */
gboolean
gst_mpd_client_set_segment_template (GstMPDClient * client,
    gchar * period_id, guint adap_set_id, gchar * rep_id,
    const gchar * property_name, ...)
{
  GstMPDRepresentationNode *representation = NULL;
  GstMPDAdaptationSetNode *adaptation_set = NULL;
  GstMPDPeriodNode *period = NULL;
  va_list myargs;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  period =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  adaptation_set =
      GST_MPD_ADAPTATION_SET_NODE (gst_mpd_client_get_adaptation_set_with_id
      (period->AdaptationSets, adap_set_id));
  g_return_val_if_fail (adaptation_set != NULL, FALSE);

  representation =
      gst_mpd_client_get_representation_with_id
      (adaptation_set->Representations, rep_id);
  if (!representation->SegmentTemplate) {
    representation->SegmentTemplate = gst_mpd_segment_template_node_new ();
  }

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (representation->SegmentTemplate),
      property_name, myargs);
  va_end (myargs);

  return TRUE;
}

/* add a segmentURL node with to a SegmentList node */
gboolean
gst_mpd_client_add_segment_url (GstMPDClient * client,
    gchar * period_id, guint adap_set_id, gchar * rep_id,
    const gchar * property_name, ...)
{
  GstMPDRepresentationNode *representation = NULL;
  GstMPDAdaptationSetNode *adaptation_set = NULL;
  GstMPDPeriodNode *period = NULL;
  GstMPDSegmentURLNode *segment_url = NULL;
  guint64 media_presentation_duration = 0;
  va_list myargs;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->mpd_root_node != NULL, FALSE);

  period =
      GST_MPD_PERIOD_NODE (gst_mpd_client_get_period_with_id
      (client->mpd_root_node->Periods, period_id));
  adaptation_set =
      GST_MPD_ADAPTATION_SET_NODE (gst_mpd_client_get_adaptation_set_with_id
      (period->AdaptationSets, adap_set_id));
  g_return_val_if_fail (adaptation_set != NULL, FALSE);

  representation =
      gst_mpd_client_get_representation_with_id
      (adaptation_set->Representations, rep_id);

  if (!representation->SegmentList) {
    representation->SegmentList = gst_mpd_segment_list_node_new ();
  }

  segment_url = gst_mpd_segment_url_node_new ();

  va_start (myargs, property_name);
  g_object_set_valist (G_OBJECT (segment_url), property_name, myargs);
  va_end (myargs);

  gst_mpd_segment_list_node_add_segment (representation->SegmentList,
      segment_url);

  /* Set the media presentation time according to the new segment duration added */
  g_object_get (client->mpd_root_node, "media-presentation-duration",
      &media_presentation_duration, NULL);
  media_presentation_duration +=
      GST_MPD_MULT_SEGMENT_BASE_NODE (representation->SegmentList)->duration;
  g_object_set (client->mpd_root_node, "media-presentation-duration",
      media_presentation_duration, NULL);

  return TRUE;
}
