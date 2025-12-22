/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * gstanalyticsobjecttrackingmtd.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstanalyticsobjecttrackingmtd.h"

/**
 * SECTION:gstanalyticsobjecttrackingmtd
 * @title: GstAnalyticsTrackingMtd
 * @short_description: An analytics metadata for tracking inside a #GstAnalyticsRelationMeta
 * @symbols:
 * - GstAnalyticsTrackingMtd
 * @see_also: #GstAnalyticsMtd, #GstAnalyticsRelationMeta
 *
 * This type of metadata holds tracking information. In many cases, it is
 * desired to track an object across many frames. This type of metadata holds
 * information about the tracking, for example, it can be used alongside a
 * #GstAnalyticsODMtd to track an object.
 *
 * Since: 1.24
 */

typedef struct _GstAnalyticsTrackingMtdData GstAnalyticsTrackingMtdData;

/**
 * GstAnalyticsTrackingMtd:
 * @tracking_id: Tracking identifier
 * @tracking_first_seen: Tracking creation time
 * @tracking_last_seen: Tracking last observation
 * @tracking_lost: Track lost
 *
 * Store information on results of object tracking
 *
 * Since: 1.24
 */
struct _GstAnalyticsTrackingMtdData
{
  guint64 tracking_id;
  GstClockTime tracking_first_seen;
  GstClockTime tracking_last_seen;
  gboolean tracking_lost;
};


static const GstAnalyticsMtdImpl tracking_impl = {
  "object-tracking",
  NULL
};

/**
 * gst_analytics_tracking_mtd_get_mtd_type:
 *
 * Returns: id representing the type of GstAnalyticsRelatableMtd
 *
 * Get the opaque id identifying the relatable type
 *
 * Since: 1.24
 */
GstAnalyticsMtdType
gst_analytics_tracking_mtd_get_mtd_type (void)
{
  return (GstAnalyticsMtdType) & tracking_impl;
}

/**
 * gst_analytics_tracking_mtd_update_last_seen:
 * @instance: GstAnalyticsTrackingMtd instance
 * @last_seen: Timestamp of last time this object was tracked
 *
 * Since: 1.24
 */
gboolean
gst_analytics_tracking_mtd_update_last_seen (GstAnalyticsTrackingMtd * instance,
    GstClockTime last_seen)
{
  GstAnalyticsTrackingMtdData *trk_mtd_data;
  g_return_val_if_fail (instance, FALSE);
  trk_mtd_data = gst_analytics_relation_meta_get_mtd_data (instance->meta,
      instance->id);
  g_return_val_if_fail (trk_mtd_data != NULL, FALSE);

  trk_mtd_data->tracking_last_seen = last_seen;
  return TRUE;
}

/**
 * gst_analytics_tracking_mtd_set_lost:
 * @instance: Instance of GstAnalyticsTrackingMtd.
 * Set tracking to lost
 *
 * Returns: Update successful
 *
 * Since: 1.24
 */
gboolean
gst_analytics_tracking_mtd_set_lost (GstAnalyticsTrackingMtd * instance)
{
  GstAnalyticsTrackingMtdData *trk_mtd_data;
  g_return_val_if_fail (instance, FALSE);
  trk_mtd_data = gst_analytics_relation_meta_get_mtd_data (instance->meta,
      instance->id);
  g_return_val_if_fail (trk_mtd_data != NULL, FALSE);
  trk_mtd_data->tracking_lost = TRUE;
  return TRUE;
}

/**
 * gst_analytics_tracking_mtd_get_info:
 * @instance: Instance of tracking metadata
 * @tracking_id: (out): Updated tracking id
 * @tracking_first_seen: (out): Updated timestamp of the tracking first observation.
 * @tracking_last_seen: (out): Updated timestamp of the tracking last observation.
 * @tracking_lost: (out): Has the tracking been lost
 *
 * Retrieve tracking information.
 *
 * Returns: Successfully retrieved info.
 * Since: 1.24
 */
gboolean
gst_analytics_tracking_mtd_get_info (GstAnalyticsTrackingMtd * instance,
    guint64 * tracking_id, GstClockTime * tracking_first_seen, GstClockTime *
    tracking_last_seen, gboolean * tracking_lost)
{
  GstAnalyticsTrackingMtdData *trk_mtd_data;
  g_return_val_if_fail (instance, FALSE);
  trk_mtd_data = gst_analytics_relation_meta_get_mtd_data (instance->meta,
      instance->id);

  g_return_val_if_fail (trk_mtd_data != NULL, FALSE);

  if (tracking_id)
    *tracking_id = trk_mtd_data->tracking_id;
  if (tracking_first_seen)
    *tracking_first_seen = trk_mtd_data->tracking_first_seen;
  if (tracking_last_seen)
    *tracking_last_seen = trk_mtd_data->tracking_last_seen;
  if (tracking_lost)
    *tracking_lost = trk_mtd_data->tracking_lost;

  return TRUE;
}


/**
 * gst_analytics_relation_meta_add_tracking_mtd:
 * @instance: Instance of GstAnalyticsRelationMeta where to add tracking mtd
 * @tracking_id: Tracking id
 * @tracking_first_seen: Timestamp of first time the object was observed.
 * @trk_mtd: (out) (not nullable): Handle updated with newly added tracking meta.
 * Add an analytic tracking metadata to @instance.
 * Returns: Added successfully
 *
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_add_tracking_mtd (GstAnalyticsRelationMeta *
    instance, guint64 tracking_id, GstClockTime tracking_first_seen,
    GstAnalyticsTrackingMtd * trk_mtd)
{
  g_return_val_if_fail (instance, FALSE);

  gsize size = sizeof (GstAnalyticsTrackingMtdData);
  GstAnalyticsTrackingMtdData *trk_mtd_data = (GstAnalyticsTrackingMtdData *)
      gst_analytics_relation_meta_add_mtd (instance, &tracking_impl, size,
      trk_mtd);

  if (trk_mtd_data) {
    trk_mtd_data->tracking_id = tracking_id;
    trk_mtd_data->tracking_first_seen = tracking_first_seen;
    trk_mtd_data->tracking_last_seen = tracking_first_seen;
    trk_mtd_data->tracking_lost = FALSE;
  }
  return trk_mtd_data != NULL;
}

/**
 * gst_analytics_relation_meta_get_tracking_mtd:
 * @meta: Instance of GstAnalyticsRelationMeta
 * @an_meta_id: Id of GstAnalyticsMtd instance to retrieve
 * @rlt: (out caller-allocates)(not nullable): Will be filled with relatable
 *    meta
 *
 * Fill @rlt if a analytics-meta with id == @an_meta_id exist in @meta instance,
 * otherwise this method return FALSE and @rlt is invalid.
 *
 * Returns: TRUE if successful.
 *
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_get_tracking_mtd (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsTrackingMtd * rlt)
{
  return gst_analytics_relation_meta_get_mtd (meta, an_meta_id,
      gst_analytics_tracking_mtd_get_mtd_type (),
      (GstAnalyticsTrackingMtd *) rlt);
}
