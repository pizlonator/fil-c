/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * gstanalyticsclassificationmtd.c
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

#include "gstanalyticsclassificationmtd.h"

/**
 * SECTION:gstanalyticsclassificationtd
 * @title: GstAnalyticsClsMtd
 * @short_description: An analytics metadata for classification inside a #GstAnalyticsRelationMeta
 * @symbols:
 * - GstAnalyticsClsMtd
 * @see_also: #GstAnalyticsMtd, #GstAnalyticsRelationMeta
 *
 * This type of metadata holds classification, it is generally used in
 * relationship with another metadata type to enhance its content. For example,
 * it can enhance the classifcation of an object detection held by the
 * #GstAnalyticsODMtd metadata type.
 *
 * Since: 1.24
 */

static const GstAnalyticsMtdImpl cls_impl = {
  "classification",
  NULL
};

typedef struct _GstAnalyticsClsConfLvlAndClass GstAnalyticsClsConfLvlAndClass;
typedef struct _GstAnalyticsClsMtdData GstAnalyticsClsMtdData;

struct _GstAnalyticsClsConfLvlAndClass
{
  GQuark class;
  gfloat confidence_levels;
};

/*
 * GstAnalyticsClsMtd:
 * @length: classes and confidence levels count
 * @class_quarks: (array length=length): Array of quark representing a class
 * @confidence_levels: (array length=length): Array of confidence levels for
 * each class in @class_quarks.
 *
 * Information on results of a classification of buffer content.
 */
struct _GstAnalyticsClsMtdData
{
  gsize length;
  GstAnalyticsClsConfLvlAndClass confidence_levels_and_classes[];       // Must be last
};

/**
 * gst_analytics_cls_mtd_get_mtd_type:
 *
 * Get an id identifying #GstAnalyticsMtd type.
 *
 * Returns: opaque id of #GstAnalyticsMtd type
 *
 * Since: 1.24
 */
GstAnalyticsMtdType
gst_analytics_cls_mtd_get_mtd_type (void)
{
  return (GstAnalyticsMtdType) & cls_impl;
}

/**
 * gst_analytics_cls_mtd_get_level:
 * @handle: instance handle
 * @index: Object class index
 *
 * Get confidence level for class at @index
 * Returns: confidence level for @index, <0.0 if the call failed.
 *
 * Since: 1.24
 */
gfloat
gst_analytics_cls_mtd_get_level (GstAnalyticsClsMtd * handle, gsize index)
{
  g_return_val_if_fail (handle, -1.0);
  g_return_val_if_fail (handle->meta != NULL, -1.0);
  GstAnalyticsClsMtdData *cls_mtd_data;
  cls_mtd_data = gst_analytics_relation_meta_get_mtd_data (handle->meta,
      handle->id);
  g_return_val_if_fail (cls_mtd_data != NULL, -1.0);
  g_return_val_if_fail (cls_mtd_data->length > index, -1.0);
  return cls_mtd_data->confidence_levels_and_classes[index].confidence_levels;
}

/**
 * gst_analytics_cls_mtd_get_index_by_quark:
 * @handle: Instance handle
 * @quark: Quark of the class
 * Get index of class represented by @quark
 * Returns: index of the class associated with @quarks ( and label) or
 *     a negative value on failure.
 *
 * Since: 1.24
 */
gint
gst_analytics_cls_mtd_get_index_by_quark (GstAnalyticsClsMtd * handle,
    GQuark quark)
{
  g_return_val_if_fail (handle, -1);

  GstAnalyticsClsMtdData *cls_mtd_data;
  cls_mtd_data = gst_analytics_relation_meta_get_mtd_data (handle->meta,
      handle->id);
  g_return_val_if_fail (cls_mtd_data != NULL, -1);

  for (gint i = 0; i < cls_mtd_data->length; i++) {
    if (quark == cls_mtd_data->confidence_levels_and_classes[i].class) {
      return i;
    }
  }
  return -1;
}

/**
 * gst_analytics_cls_mtd_get_length:
 * @handle: Instance handle
 * Get number of classes
 * Returns: Number of classes in this classification instance
 *
 * Since: 1.24
 */
gsize
gst_analytics_cls_mtd_get_length (GstAnalyticsClsMtd * handle)
{
  GstAnalyticsClsMtdData *cls_mtd_data;
  cls_mtd_data = gst_analytics_relation_meta_get_mtd_data (handle->meta,
      handle->id);
  g_return_val_if_fail (cls_mtd_data != NULL, 0);
  return cls_mtd_data->length;
}

/**
 * gst_analytics_cls_mtd_get_quark:
 * @handle: Instance handle
 * @index: index of the class
 * Get quark of the class at @index
 * Returns: Quark of this class (label) associated with @index
 *
 * Since: 1.24
 */
GQuark
gst_analytics_cls_mtd_get_quark (GstAnalyticsClsMtd * handle, gsize index)
{
  GstAnalyticsClsMtdData *cls_mtd_data;
  g_return_val_if_fail (handle, 0);
  cls_mtd_data = gst_analytics_relation_meta_get_mtd_data (handle->meta,
      handle->id);
  g_return_val_if_fail (cls_mtd_data != NULL, 0);
  g_return_val_if_fail (cls_mtd_data->length > index, 0);

  return cls_mtd_data->confidence_levels_and_classes[index].class;
}

/**
 * gst_analytics_relation_meta_add_cls_mtd:
 * @instance: Instance of #GstAnalyticsRelationMeta where to add classification instance
 * @length: length of @confidence_levels
 * @confidence_levels: (array length=length): confidence levels
 * @class_quarks: (array length=length): labels of this
 *    classification. Order define index, quark, labels relation. This array
 *    need to exist as long has this classification meta exist.
 * @cls_mtd: (out) (not nullable): Handle updated to newly added classification meta.
 *
 * Add analytic classification metadata to @instance.
 * Returns: Added successfully
 *
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_add_cls_mtd (GstAnalyticsRelationMeta *
    instance, gsize length, gfloat * confidence_levels, GQuark * class_quarks,
    GstAnalyticsClsMtd * cls_mtd)
{
  g_return_val_if_fail (instance, FALSE);
  gsize confidence_levels_size =
      (sizeof (GstAnalyticsClsConfLvlAndClass) * length);
  gsize size = sizeof (GstAnalyticsClsMtdData) + confidence_levels_size;
  GstAnalyticsClsConfLvlAndClass *conf_lvls_and_classes;

  GstAnalyticsClsMtdData *cls_mtd_data = (GstAnalyticsClsMtdData *)
      gst_analytics_relation_meta_add_mtd (instance, &cls_impl, size, cls_mtd);
  if (cls_mtd_data) {
    cls_mtd_data->length = length;
    for (gsize i = 0; i < length; i++) {
      conf_lvls_and_classes = &(cls_mtd_data->confidence_levels_and_classes[i]);
      conf_lvls_and_classes->class = class_quarks[i];
      conf_lvls_and_classes->confidence_levels = confidence_levels[i];
    }
  }
  return cls_mtd_data != NULL;
}

/**
 * gst_analytics_relation_meta_add_one_cls_mtd:
 * @instance: Instance of #GstAnalyticsRelationMeta where to add classification instance
 * @confidence_level: confidence levels
 * @class_quark: labels of this
 *    classification. Order define index, quark, labels relation. This array
 *    need to exist as long has this classification meta exist.
 * @cls_mtd: (out) (not nullable): Handle updated to newly added classification meta.
 *
 * Add analytic classification metadata to @instance.
 * Returns: Added successfully
 *
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_add_one_cls_mtd (GstAnalyticsRelationMeta *
    instance, gfloat confidence_level, GQuark class_quark,
    GstAnalyticsClsMtd * cls_mtd)
{
  static const gsize len = 1;

  return gst_analytics_relation_meta_add_cls_mtd (instance, len,
      &confidence_level, &class_quark, cls_mtd);
}

/**
 * gst_analytics_relation_meta_get_cls_mtd:
 * @meta: Instance of #GstAnalyticsRelationMeta
 * @an_meta_id: Id of #GstAnalyticsClsMtd instance to retrieve
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
gst_analytics_relation_meta_get_cls_mtd (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsClsMtd * rlt)
{
  return gst_analytics_relation_meta_get_mtd (meta, an_meta_id,
      gst_analytics_cls_mtd_get_mtd_type (), (GstAnalyticsClsMtd *) rlt);
}
