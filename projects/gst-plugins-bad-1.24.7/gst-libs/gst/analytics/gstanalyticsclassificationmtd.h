/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * gstanalyticsclassificationmtd.h
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

#ifndef __GST_ANALYTICS_CLASSIFICATION_H__
#define __GST_ANALYTICS_CLASSIFICATION_H__

#include <gst/gst.h>
#include <gst/analytics/analytics-meta-prelude.h>
#include <gst/analytics/gstanalyticsmeta.h>

G_BEGIN_DECLS

/**
 * GstAnalyticsClsMtd:
 * @id: Instance identifier.
 * @meta: Instance of #GstAnalyticsRelationMeta where the analysis-metadata
 * identified by @id is stored.
 *
 * Handle containing data required to use gst_analytics_cls_mtd APIs. This type
 * is generally expected to be allocated on the stack.
 *
 * Since: 1.24
 */
typedef struct _GstAnalyticsMtd GstAnalyticsClsMtd;

GST_ANALYTICS_META_API
GstAnalyticsMtdType gst_analytics_cls_mtd_get_mtd_type (void);

GST_ANALYTICS_META_API
gfloat gst_analytics_cls_mtd_get_level (GstAnalyticsClsMtd * handle,
    gsize index);

GST_ANALYTICS_META_API
gint gst_analytics_cls_mtd_get_index_by_quark (GstAnalyticsClsMtd * handle,
    GQuark quark);

GST_ANALYTICS_META_API
gsize gst_analytics_cls_mtd_get_length (GstAnalyticsClsMtd * handle);

GST_ANALYTICS_META_API
GQuark gst_analytics_cls_mtd_get_quark (GstAnalyticsClsMtd * handle,
    gsize index);

GST_ANALYTICS_META_API
gboolean
gst_analytics_relation_meta_add_cls_mtd (GstAnalyticsRelationMeta *
    instance, gsize length, gfloat * confidence_levels, GQuark * class_quarks,
    GstAnalyticsClsMtd * cls_mtd);

GST_ANALYTICS_META_API
gboolean
gst_analytics_relation_meta_add_one_cls_mtd (GstAnalyticsRelationMeta *
    instance, gfloat confidence_level, GQuark class_quark,
    GstAnalyticsClsMtd * cls_mtd);

GST_ANALYTICS_META_API
gboolean
gst_analytics_relation_meta_get_cls_mtd (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsClsMtd * rlt);

G_END_DECLS
#endif // __GST_ANALYTICS_CLASSIFICATION_H__
