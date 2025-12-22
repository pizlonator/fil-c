/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * gstanalyticsobjectdetectionmtd.c
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

#include "gstanalyticsobjectdetectionmtd.h"

#include <gst/video/video.h>

/**
 * SECTION:gstanalyticsobjectdetectionmtd
 * @title: GstAnalyticsODMtd
 * @short_description: An analytics metadata for object dection inside a #GstAnalyticsRelationMeta
 * @symbols:
 * - GstAnalyticsODMtd
 * @see_also: #GstAnalyticsMtd, #GstAnalyticsRelationMeta
 *
 * This type of metadata holds the position of detected object inside the
 * image, along with the probabily of each detection.
 *
 * Since: 1.24
 */

typedef struct _GstAnalyticsODMtdData GstAnalyticsODMtdData;

/**
 * GstAnalyticsODMtdData:
 * @object_type: Type of object
 * @x: x component of upper-left corner
 * @y: y component of upper-left corner
 * @w: bounding box width
 * @h: bounding box height
 * @location_confidence_lvl: Confidence on object location
 *
 * Store information on results of object detection
 *
 * Since: 1.24
 */
struct _GstAnalyticsODMtdData
{
  GQuark object_type;
  gint x;
  gint y;
  gint w;
  gint h;
  gfloat location_confidence_lvl;
};

static gboolean
gst_analytics_od_mtd_meta_transform (GstBuffer * transbuf,
    GstAnalyticsMtd * transmtd, GstBuffer * buffer, GQuark type, gpointer data)
{
  if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstVideoMetaTransform *trans = data;
    gint ow, oh, nw, nh;
    GstAnalyticsODMtdData *oddata;

    ow = GST_VIDEO_INFO_WIDTH (trans->in_info);
    nw = GST_VIDEO_INFO_WIDTH (trans->out_info);
    oh = GST_VIDEO_INFO_HEIGHT (trans->in_info);
    nh = GST_VIDEO_INFO_HEIGHT (trans->out_info);

    oddata = gst_analytics_relation_meta_get_mtd_data (transmtd->meta,
        transmtd->id);

    oddata->x *= nw;
    oddata->x /= ow;

    oddata->w *= nw;
    oddata->w /= ow;

    oddata->y *= nh;
    oddata->y /= oh;

    oddata->h *= nh;
    oddata->h /= oh;
  }

  return TRUE;
}

static const GstAnalyticsMtdImpl od_impl = {
  "object-detection",
  gst_analytics_od_mtd_meta_transform
};

/**
 * gst_analytics_od_mtd_get_mtd_type:
 *
 * Get an id that represent object-detection metadata type
 *
 * Returns: Opaque id of the #GstAnalyticsMtd type
 *
 * Since: 1.24
 */
GstAnalyticsMtdType
gst_analytics_od_mtd_get_mtd_type (void)
{
  return (GstAnalyticsMtdType) & od_impl;
}

/**
 * gst_analytics_od_mtd_get_location:
 * @instance: instance
 * @x: (out): x component of upper-left corner of the object location
 * @y: (out): y component of upper-left corner of the object location
 * @w: (out): bounding box width of the object location
 * @h: (out): bounding box height of the object location
 * @loc_conf_lvl: (out)(optional): Confidence on object location

 *
 * Retrieve location and location confidence level.
 *
 * Returns: TRUE on success, otherwise FALSE.
 *
 * Since: 1.24
 */
gboolean
gst_analytics_od_mtd_get_location (GstAnalyticsODMtd * instance,
    gint * x, gint * y, gint * w, gint * h, gfloat * loc_conf_lvl)
{
  GstAnalyticsODMtdData *data;

  g_return_val_if_fail (instance && x && y && w && h, FALSE);
  data = gst_analytics_relation_meta_get_mtd_data (instance->meta,
      instance->id);
  g_return_val_if_fail (data != NULL, FALSE);

  *x = data->x;
  *y = data->y;
  *w = data->w;
  *h = data->h;

  if (loc_conf_lvl)
    *loc_conf_lvl = data->location_confidence_lvl;

  return TRUE;
}

/**
 * gst_analytics_od_mtd_get_confidence_lvl:
 * @instance: instance
 * @loc_conf_lvl: (out): Confidence on object location
 *
 * Retrieve location confidence level.
 *
 * Returns: TRUE on success, otherwise FALSE.
 *
 * Since: 1.24
 */
gboolean
gst_analytics_od_mtd_get_confidence_lvl (GstAnalyticsODMtd * instance,
    gfloat * loc_conf_lvl)
{
  GstAnalyticsODMtdData *data;

  g_return_val_if_fail (instance && loc_conf_lvl, FALSE);
  data = gst_analytics_relation_meta_get_mtd_data (instance->meta,
      instance->id);
  g_return_val_if_fail (data != NULL, FALSE);

  *loc_conf_lvl = data->location_confidence_lvl;

  return TRUE;
}

/**
 * gst_analytics_od_mtd_get_obj_type:
 * @handle: Instance handle
 *
 * Quark of the class of object associated with this location.
 *
 * Returns: Quark different from on success and 0 on failure.
 *
 * Since: 1.24
 */
GQuark
gst_analytics_od_mtd_get_obj_type (GstAnalyticsODMtd * handle)
{
  GstAnalyticsODMtdData *data;
  g_return_val_if_fail (handle != NULL, 0);
  data = gst_analytics_relation_meta_get_mtd_data (handle->meta, handle->id);
  g_return_val_if_fail (data != NULL, 0);
  return data->object_type;
}

/**
 * gst_analytics_relation_meta_add_od_mtd:
 * @instance: Instance of #GstAnalyticsRelationMeta where to add classification instance
 * @type: Quark of the object type
 * @x: x component of bounding box upper-left corner
 * @y: y component of bounding box upper-left corner
 * @w: bounding box width
 * @h: bounding box height
 * @loc_conf_lvl: confidence level on the object location
 * @od_mtd: (out)(nullable): Handle updated with newly added object detection
 *    meta. Add an object-detetion metadata to @instance.
 *
 * Returns: Added successfully
 *
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_add_od_mtd (GstAnalyticsRelationMeta *
    instance, GQuark type, gint x, gint y, gint w, gint h,
    gfloat loc_conf_lvl, GstAnalyticsODMtd * od_mtd)
{
  g_return_val_if_fail (instance != NULL, FALSE);
  gsize size = sizeof (GstAnalyticsODMtdData);
  GstAnalyticsODMtdData *od_mtd_data = (GstAnalyticsODMtdData *)
      gst_analytics_relation_meta_add_mtd (instance, &od_impl, size, od_mtd);
  if (od_mtd_data) {
    od_mtd_data->x = x;
    od_mtd_data->y = y;
    od_mtd_data->w = w;
    od_mtd_data->h = h;
    od_mtd_data->location_confidence_lvl = loc_conf_lvl;
    od_mtd_data->object_type = type;
  }
  return od_mtd_data != NULL;
}

/**
 * gst_analytics_relation_meta_get_od_mtd:
 * @meta: Instance of #GstAnalyticsRelationMeta
 * @an_meta_id: Id of #GstAnalyticsODMtd instance to retrieve
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
gst_analytics_relation_meta_get_od_mtd (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsODMtd * rlt)
{
  return gst_analytics_relation_meta_get_mtd (meta, an_meta_id,
      gst_analytics_od_mtd_get_mtd_type (), (GstAnalyticsODMtd *) rlt);
}
