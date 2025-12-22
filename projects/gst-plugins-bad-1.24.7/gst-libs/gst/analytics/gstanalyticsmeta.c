/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * gstanalyticsmeta.c
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

#include "gstanalyticsmeta.h"

#include <gst/video/video.h>

/**
 * SECTION:gstanalyticsmeta
 * @title: GstAnalyticsRelationMeta
 * @short_description: A GstMeta for analytics metadata
 * @symbols:
 * - GstAnalyticsRelationMeta
 * - GstAnalyticsMtd
 * @see_also: #GstAnalyticsODMtd, #GstAnalyticsClsMtd, #GstAnalyticsTrackingMtd
 *
 * The #GstAnalyticsRelationMeta is a #GstMeta that can contain a large number
 * of results from the analysis of a meta. Each result can be accessed by
 * using its id, or more conviently, by using a #GstAnalyticsMtd. A matrix
 * of relationships between the various metadata is also defined and can be
 * filled by the analysis processes.
 *
 * Since: 1.24
 */

GST_DEBUG_CATEGORY_STATIC (an_relation_meta_debug);
#define GST_CAT_AN_RELATION an_relation_meta_debug

/*
 * GstAnalyticsRelatableMtdData:
 * @analysis_type: Identify the type of analysis-metadata
 * @id: Instance identifier.
 * @size: Size in bytes of the instance
 *
 * Base structure for analysis-metadata that can be placed in relation. Only
 * other analysis-metadata based on GstAnalyticsRelatableMtdDatax should
 * directly use this structure.
 */
typedef struct
{
  const GstAnalyticsMtdImpl *impl;
  guint id;
  gsize size;
  gpointer data[];
} GstAnalyticsRelatableMtdData;


/*
 * GstAnalyticsRelationMeta:
 * Meta storing analysis-metadata relation information.
 *
 */
typedef struct _GstAnalyticsRelationMeta
{
  GstMeta parent_meta;

  /*< Next id > */
  guint next_id;

  /* Adjacency-matrix */
  guint8 **adj_mat;

  /* Lookup (offset relative to analysis_results) of relatable metadata */
  gsize *mtd_data_lookup;

  /* Relation order */
  gsize rel_order;

  /* Increment used when relation order grow */
  gsize rel_order_increment;

  /* Analysis metadata based on GstAnalyticsRelatableMtdData */
  gint8 *analysis_results;

  /* Current writing location in analysis_results buffer */
  gsize offset;

  /* Size of analysis_results buffer */
  gsize max_size;

  /* Increment of analysis_results */
  gsize max_size_increment;

  /* Number of analytic metadata (GstAnalyticsRelatableMtdData) in
   * analysis_results */
  gsize length;

} GstAnalyticsRelationMeta;

static guint
gst_analytics_relation_meta_get_next_id (GstAnalyticsRelationMeta * meta);


static GstAnalyticsRelatableMtdData *
gst_analytics_relation_meta_get_mtd_data_internal (GstAnalyticsRelationMeta *
    meta, guint an_meta_id)
{
  GstAnalyticsRelatableMtdData *rv;
  g_return_val_if_fail (meta, NULL);
  if (an_meta_id >= meta->rel_order) {
    GST_CAT_ERROR (GST_CAT_AN_RELATION, "Invalid parameter");
    return NULL;
  }
  rv = (GstAnalyticsRelatableMtdData *)
      (meta->mtd_data_lookup[an_meta_id] + meta->analysis_results);

  return rv;
}

/**
 * gst_analytics_mtd_get_mtd_type:
 *
 * @instance: Instance of #GstAnalyticsMtd
 * Get analysis result type.
 *
 * Returns: opaque id of the type
 * Since: 1.24
 */
GstAnalyticsMtdType
gst_analytics_mtd_get_mtd_type (GstAnalyticsMtd * handle)
{
  GstAnalyticsRelatableMtdData *rlt;
  rlt = gst_analytics_relation_meta_get_mtd_data_internal (handle->meta,
      handle->id);

  g_return_val_if_fail (rlt != NULL, 0);

  return (GstAnalyticsMtdType) rlt->impl;
}

/**
 * gst_analytics_mtd_get_id:
 * @instance: Instance of #GstAnalyticsMtd
 *
 * Get instance id
 *
 * Returns: Id of @instance
 * Since: 1.24
 */
guint
gst_analytics_mtd_get_id (GstAnalyticsMtd * handle)
{
  return handle->id;
}

/**
 * gst_analytics_mtd_get_size:
 * @instance: Instance of #GstAnalyticsMtd
 *
 * Get instance size
 *
 * Returns: Size (in bytes) of this instance or 0 on failure.
 * Since: 1.24
 */
gsize
gst_analytics_mtd_get_size (GstAnalyticsMtd * handle)
{
  GstAnalyticsRelatableMtdData *rlt;
  rlt = gst_analytics_relation_meta_get_mtd_data_internal (handle->meta,
      handle->id);
  if (rlt == NULL) {
    GST_CAT_ERROR (GST_CAT_AN_RELATION, "Invalid parameter");
    return 0;
  }

  return rlt->size;
}

/**
 * gst_analytics_mtd_type_get_name:
 * @type: The type of analytics data
 *
 * Gets the string version of the name of this type of analytics data
 *
 * Returns: the name
 * Since: 1.24
 */
const gchar *
gst_analytics_mtd_type_get_name (GstAnalyticsMtdType type)
{
  GstAnalyticsMtdImpl *impl = (GstAnalyticsMtdImpl *) type;

  g_return_val_if_fail (impl != NULL, NULL);

  if (type == GST_ANALYTICS_MTD_TYPE_ANY)
    return "ANY";
  else
    return impl->name;
}

/**
 * gst_analytics_relation_get_length:
 * @instance: Instance of #GstAnalyticsRelationMeta
 *
 * Get number of relatable meta attached to instance
 *
 * Returns: Number of analysis-meta attached to this
 *  instance.
 * Since: 1.24
 */
gsize
gst_analytics_relation_get_length (GstAnalyticsRelationMeta * instance)
{
  gsize rv;
  g_return_val_if_fail (instance, 0);

  rv = instance->length;
  return rv;
}

/*
 * gst_analytics_relation_adj_mat_create:
 * @order: Order or the adjacency-matrix to create.
 * Create a new adjacency-matrix (array of MxN where M and N are equal
 * to order).
 *
 * Returns: new adjacency-matrix
 *
 * Since: 1.24
 */
static guint8 **
gst_analytics_relation_adj_mat_create (gsize order)
{
  guint8 **adj_mat, *data;
  gsize sz = sizeof (guint8 *) * order + sizeof (guint8) * order * order;
  adj_mat = (guint8 **) g_malloc0 (sz);
  data = (guint8 *) (adj_mat + order);
  for (gsize r = 0; r < order; r++) {
    adj_mat[r] = (data + order * r);
  }
  return adj_mat;
}

/**
 * gst_analytics_relation_adj_mat_dup:
 * @adj_mat: Adjcency-matrix (array or MxN)
 * @order: Order of the existing matrix
 * @new_order: Order of the matrix to create
 *
 * Duplicate adj_mat to a newly allocated array new_order x new_order dimension
 * while keep values of adj_mat at the same indexes in the new array.
 *
 * Returns: New adjacency matrix with maintained values.
 * Since: 1.24
 */
static guint8 **
gst_analytics_relation_adj_mat_dup (guint8 ** adj_mat, gsize order,
    gsize new_order)
{
  guint8 **new_adj_mat = gst_analytics_relation_adj_mat_create (new_order);
  for (gsize r = 0; r < order; r++) {
    memcpy (new_adj_mat[r], adj_mat[r], sizeof (guint8) * order);
  }
  return new_adj_mat;
}

/**
 * gst_analytics_relation_meta_api_get_type:
 *
 * Returns: GType of GstAnalyticsRelationMeta
 * Since: 1.24
 */
GType
gst_analytics_relation_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = {
    GST_META_TAG_VIDEO_SIZE_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_STR,
    NULL
  };
  if (g_once_init_enter (&type)) {
    GType newType =
        gst_meta_api_type_register ("GstAnalyticsRelationMetaAPI", tags);
    GST_DEBUG_CATEGORY_INIT (an_relation_meta_debug, "anrelmeta",
        GST_DEBUG_FG_BLACK, "Content analysis meta relations meta");
    g_once_init_leave (&type, newType);
  }
  return type;
}

static gboolean
gst_analytics_relation_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *) meta;
  GstAnalyticsRelationMetaInitParams *rel_params = params;
  rmeta->next_id = 0;

  g_return_val_if_fail (params != NULL, FALSE);

  GST_CAT_TRACE (GST_CAT_AN_RELATION, "Relation order:%" G_GSIZE_FORMAT,
      *((gsize *) params));

  rmeta->rel_order_increment = rel_params->initial_relation_order;
  rmeta->rel_order = rmeta->rel_order_increment;
  if (rmeta->rel_order > 0) {
    rmeta->adj_mat = gst_analytics_relation_adj_mat_create (rmeta->rel_order);
    rmeta->mtd_data_lookup = g_malloc0 (sizeof (gpointer) * rmeta->rel_order);
  }
  rmeta->offset = 0;
  rmeta->max_size = rmeta->max_size_increment = rel_params->initial_buf_size;
  rmeta->analysis_results = g_malloc (rel_params->initial_buf_size);
  rmeta->length = 0;

  if (buffer->pool)
    GST_META_FLAG_SET (meta, GST_META_FLAG_POOLED);

  GST_CAT_DEBUG (GST_CAT_AN_RELATION,
      "Content analysis meta-relation meta(%p, order=%" G_GSIZE_FORMAT
      ") created for buffer(%p)", (gpointer) rmeta, *(gsize *) params,
      (gpointer) buffer);
  return TRUE;
}

static void
gst_analytics_relation_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *) meta;

  GST_CAT_TRACE (GST_CAT_AN_RELATION,
      "Content analysis meta-data(%p) freed for buffer(%p)",
      (gpointer) rmeta, (gpointer) buffer);

  g_free (rmeta->analysis_results);
  g_free (rmeta->adj_mat);
  g_free (rmeta->mtd_data_lookup);
}

static gboolean
gst_analytics_relation_meta_transform (GstBuffer * transbuf,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data)
{

  GST_CAT_TRACE (GST_CAT_AN_RELATION, "meta transform %s",
      g_quark_to_string (type));

  if (GST_META_TRANSFORM_IS_COPY (type) ||
      GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *) meta;
    GstAnalyticsRelationMeta *new = (GstAnalyticsRelationMeta *)
        gst_buffer_get_meta (transbuf, GST_ANALYTICS_RELATION_META_API_TYPE);

    if (new == NULL) {
      GstAnalyticsRelationMetaInitParams init_params = {
        rmeta->rel_order, rmeta->max_size
      };

      GST_CAT_TRACE (GST_CAT_AN_RELATION,
          "meta transform creating new meta rel_order:%" G_GSIZE_FORMAT
          " max_size:%" G_GSIZE_FORMAT,
          init_params.initial_relation_order, init_params.initial_buf_size);

      new =
          gst_buffer_add_analytics_relation_meta_full (transbuf, &init_params);
    }

    if (new->offset == 0) {
      guint i;

      if (new->rel_order < rmeta->rel_order) {
        g_free (new->adj_mat);
        g_free (new->mtd_data_lookup);
        new->adj_mat = gst_analytics_relation_adj_mat_create (rmeta->rel_order);
        new->mtd_data_lookup = g_malloc0 (sizeof (gpointer) * rmeta->rel_order);
        new->rel_order = rmeta->rel_order;
      }

      if (new->max_size < rmeta->max_size) {
        g_free (new->analysis_results);
        new->analysis_results = g_malloc (rmeta->max_size);
        new->max_size = rmeta->max_size;
      }

      if (rmeta->rel_order == new->rel_order) {
        memcpy (new->adj_mat + new->rel_order, rmeta->adj_mat +
            rmeta->rel_order, rmeta->rel_order * rmeta->rel_order);
      } else {
        /* When destination adj_mat has a higher order than source we need
         * to copy by row to have the correct alignment */
        for (gsize r = 0; r < rmeta->rel_order; r++) {
          memcpy (new->adj_mat[r], rmeta->adj_mat[r], rmeta->rel_order);
        }
      }
      memcpy (new->mtd_data_lookup, rmeta->mtd_data_lookup,
          sizeof (gpointer) * rmeta->rel_order);
      memcpy (new->analysis_results, rmeta->analysis_results, rmeta->offset);

      new->length = rmeta->length;
      new->next_id = rmeta->next_id;
      new->offset = rmeta->offset;

      for (i = 0; i < new->length; i++) {
        GstAnalyticsRelatableMtdData *rlt_mtd_data =
            (GstAnalyticsRelatableMtdData *) (new->mtd_data_lookup[i] +
            new->analysis_results);
        if (rlt_mtd_data->impl && rlt_mtd_data->impl->mtd_meta_transform) {
          GstAnalyticsMtd transmtd;
          transmtd.id = rlt_mtd_data->id;
          transmtd.meta = new;
          rlt_mtd_data->impl->mtd_meta_transform (transbuf, &transmtd, buffer,
              type, data);
        }
      }
      return TRUE;
    } else {
      g_warning ("Trying to copy GstAnalyticsRelationMeta into non-empty meta");
      g_debug ("ofs:%" G_GSIZE_FORMAT, new->offset);

      return FALSE;
    }
  }

  return FALSE;
}

static void
gst_analytics_relation_meta_clear (GstBuffer * buffer, GstMeta * meta)
{
  GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *) meta;
  gsize adj_mat_data_size =
      (sizeof (guint8) * rmeta->rel_order * rmeta->rel_order);

  rmeta->next_id = 0;
  rmeta->offset = 0;
  rmeta->length = 0;
  if (adj_mat_data_size) {
    /* Only clear data and not lines addresses occupying begining of this
     * array. */
    memset (rmeta->adj_mat + rmeta->rel_order, 0, adj_mat_data_size);
  }
}

/**
 * gst_analytics_relation_meta_get_info: (skip)
 *
 * Get the meta info
 *
 * Since: 1.24
 */
const GstMetaInfo *
gst_analytics_relation_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;
  if (g_once_init_enter ((GstMetaInfo **) & info)) {
    GstMetaInfo *meta = gst_meta_info_new (GST_ANALYTICS_RELATION_META_API_TYPE,
        "GstAnalyticsRelationMeta",
        sizeof (GstAnalyticsRelationMeta));
    meta->init_func = gst_analytics_relation_meta_init;
    meta->free_func = gst_analytics_relation_meta_free;
    meta->transform_func = gst_analytics_relation_meta_transform;
    meta->clear_func = gst_analytics_relation_meta_clear;
    meta = (GstMetaInfo *) gst_meta_info_register (meta);
    g_once_init_leave ((GstMetaInfo **) & info, (GstMetaInfo *) meta);
  }
  return info;
}

/*
 * gst_analytics_relation_meta_bfs:
 * @start: start vertex
 * @adj_mat: graph's adjacency matrix
 * @adj_mat_order: order of the adjacency matrix (number of vertex in the graph)
 * @edge_mask: allow to select edge type we are interested by.
 * @max_span: Maximum number of edge to traverse from start vertex while
 * exploring graph.
 * @level: (out)(not nullable)(array length=adj_mat_order): Array that will be
 *    filled with number of edge to traverse to reach @start from the vertex
 *    identified by the array index. (Ex: start=1 and level[3]=2, mean we need
 *    to traverse 2 edges from vertex 2 to vertex 3. A value of -1 in @level
 *    mean this vertex is un-reachable considering @edge_mask, @max_span and
 *    @adj_mat.
 * @parent: (out)(array length=adj_mat_order): Array that will be filled with
 *    shortest path information. The shortest path from vertex X and vertex
 *    @start, where X is the index of @parent array. To find each node on the
 *    path we need to recursively do ...parent[parent[parent[X]]] until value
 *    is @start. Value at index Y equal to -1 mean there's no path from
 *    vertex Y to vertex @start.
 *
 * This function can be used to find the existence of relation paths that
 * emerge from a analysis-metadata (@start). The existence verification can
 * also be parametrize by only considering certain types of relation
 * (@edge_mask), a maximum intermediates analysis-metadata (@max_span). A
 * usage exemple can be found in @gst_analytics_relation_meta_exist.
 */
static void
gst_analytics_relation_meta_bfs (guint start, const guint8 ** adj_mat,
    gsize adj_mat_order, guint8 edge_mask, gsize max_span, gint * level,
    gint * parent)
{
  GSList *frontier = NULL;
  GSList *iter;
  GSList *next_frontier;
  gsize i = 1;
  memset (level, -1, sizeof (gint) * adj_mat_order);
  memset (parent, -1, sizeof (gint) * adj_mat_order);

  GST_CAT_TRACE (GST_CAT_AN_RELATION,
      "Performing bfs to find relation(%x) starting from %d with less than %"
      G_GSIZE_FORMAT " edges from start", edge_mask, start, max_span);

  // vertex that has a relation with itself
  if (adj_mat[start][start] & edge_mask) {
    level[start] = 0;
  }

  frontier = g_slist_prepend (frontier, GINT_TO_POINTER (start));

  while (frontier && i <= max_span) {
    next_frontier = NULL;
    for (iter = frontier; iter; iter = iter->next) {
      for (gsize j = 0; j < adj_mat_order; j++) {
        if (adj_mat[(gsize) GPOINTER_TO_INT (iter->data)][j] & edge_mask) {
          if (level[j] == -1) {
            level[j] = i;
            parent[j] = GPOINTER_TO_INT (iter->data);
            GST_CAT_TRACE (GST_CAT_AN_RELATION, "Parent of %" G_GSIZE_FORMAT
                " is %d", j, parent[j]);
            next_frontier =
                g_slist_prepend (next_frontier, GINT_TO_POINTER ((gint) j));
          }
        }
      }
    }
    g_slist_free (frontier);
    frontier = next_frontier;
    i++;
  }
  g_slist_free (frontier);
}

/*
 * gst_analytics_relation_meta_get_next_id:
 * @meta a #GstAnalyticsRelationMeta from which we want to get next id.
 *
 * Get next id and prepare for future request.
 *
 * Returns: next id
 *
 */
static guint
gst_analytics_relation_meta_get_next_id (GstAnalyticsRelationMeta * meta)
{
  g_return_val_if_fail (meta != NULL, -1);
  return g_atomic_int_add (&meta->next_id, 1);
}

/**
 * gst_analytics_relation_meta_get_relation:
 * @meta: (transfer none): a #GstAnalyticsRelationMeta
 * @an_meta_first_id: Id of first analysis-meta
 * @an_meta_second_id: Id of second  analysis-meta
 *
 * Get relations between first and second analysis-meta.
 * Ids (@an_meta_first_id and @an_meta_second_id) must be from a call to
 * @gst_analytics_mtd_get_id (handle).
 *
 * Returns: relation description between first and second analysis-meta.
 * Since: 1.24
 */
GstAnalyticsRelTypes
gst_analytics_relation_meta_get_relation (GstAnalyticsRelationMeta * meta,
    guint an_meta_first_id, guint an_meta_second_id)
{
  GstAnalyticsRelTypes types = GST_ANALYTICS_REL_TYPE_NONE;
  g_return_val_if_fail (meta, GST_ANALYTICS_REL_TYPE_NONE);

  g_return_val_if_fail (meta->adj_mat != NULL, GST_ANALYTICS_REL_TYPE_NONE);
  if (meta->rel_order > an_meta_first_id && meta->rel_order > an_meta_second_id) {
    types = meta->adj_mat[an_meta_first_id][an_meta_second_id];
  } else {
    GST_CAT_DEBUG (GST_CAT_AN_RELATION,
        "an_meta_first(%u) and an_meta_second(%u) must be inferior to %"
        G_GSIZE_FORMAT, an_meta_first_id, an_meta_second_id, meta->rel_order);

    if (an_meta_first_id >= meta->rel_order) {
      GST_CAT_ERROR (GST_CAT_AN_RELATION,
          "an_meta_first(%u) must be from a call to "
          "gst_analytics_mtd_get_id(...)", an_meta_first_id);
    }

    if (an_meta_second_id >= meta->rel_order) {
      GST_CAT_ERROR (GST_CAT_AN_RELATION,
          "an_meta_second(%u) must be from a call to "
          "gst_analytics_mtd_get_id(...)", an_meta_second_id);
    }
  }
  return types;
}

/**
 * gst_analytics_relation_meta_set_relation:
 * @meta: (transfer none): Parameter to receive new maximum number of
 *    analysis-meta described by relation.
 * @type: a #GstAnalyticsRelTypes defining relation between two analysis-meta
 * @an_meta_first_id: first meta id
 * @an_meta_second_id: second meta id
 *
 * Sets the relation (#GstAnalyticsRelTypes) between @an_meta_first and
 *    @an_meta_second.
 * Ids must have been obtained a call to
 *    @gst_analytics_mtd_get_id(handle).
 *
 * Returns: TRUE on success and FALSE on failure.
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_set_relation (GstAnalyticsRelationMeta * meta,
    GstAnalyticsRelTypes type, guint an_meta_first_id, guint an_meta_second_id)
{
  g_return_val_if_fail (type < GST_ANALYTICS_REL_TYPE_LAST, FALSE);
  g_return_val_if_fail (meta, FALSE);
  if (an_meta_first_id >= meta->rel_order
      || an_meta_second_id >= meta->rel_order) {
    GST_CAT_ERROR (GST_CAT_AN_RELATION, "Invalid parameter");
    return FALSE;
  }
  meta->adj_mat[an_meta_first_id][an_meta_second_id] = type;
  GST_CAT_TRACE (GST_CAT_AN_RELATION,
      "Relation %x set between %u and %u",
      type, an_meta_first_id, an_meta_second_id);
  return TRUE;
}

/**
 * gst_analytics_relation_meta_exist:
 * @rmeta: (transfer none): a #GstAnalyticsRelationMeta describing analysis-meta
 *    relation
 * @an_meta_first_id: First analysis-meta
 * @an_meta_second_id: Second analysis-meta
 * @max_relation_span: Maximum number of relation between @an_meta_first_id and
 *    @an_meta_second_id.
 *    A value of 1 mean only only consider direct relation.
 * @cond_types: condition on relation types.
 * @relations_path: (transfer full)(nullable)(out caller-allocates)(array)
 *    (element-type gint):
 *    If not NULL this list will be filled with relation path between
 *    @an_meta_first_id and
 *    @an_meta_second_id. List value should be access with GSList API. Use
 *    GPOINTER_TO_INT(iter->data) where iter is a GSList element to get
 *    analysis-meta id on the relation path. Free this list with g_slist_free
 *    (@relations_path) after using.
 *
 * Verify existence of relation(s) between @an_meta_first_d and
 * @an_meta_second_id according to relation condition @cond_types. It optionally
 * also return a shortest path of relations ( compliant with @cond_types)
 * between @an_meta_first_id and @an_meta_second_id.
 *
 * Returns: TRUE if a relation between exit between @an_meta_first_id and
 *  @an_meta_second_id, otherwise FALSE.
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_exist (GstAnalyticsRelationMeta * rmeta,
    guint an_meta_first_id,
    guint an_meta_second_id,
    gint max_relation_span,
    GstAnalyticsRelTypes cond_types, GArray ** relations_path)
{
  gboolean rv = FALSE;
  guint8 **adj_mat;
  gsize adj_mat_order, span;
  GArray *path = NULL;
  gint *level, i, path_left;
  gint *parent;
  g_return_val_if_fail (rmeta, FALSE);

  if (!rmeta) {
    GST_CAT_ERROR (GST_CAT_AN_RELATION, "Invalid parameter");
    return EINVAL;
  }
  adj_mat_order = rmeta->rel_order;

  if (adj_mat_order < (an_meta_first_id + 1)
      || adj_mat_order < (an_meta_second_id + 1)) {

    GST_CAT_DEBUG (GST_CAT_AN_RELATION,
        "Testing relation existence for analysis-meta that have no index in "
        "adj-mat.");

    return FALSE;
  }

  adj_mat = rmeta->adj_mat;
  if (max_relation_span < 0) {
    span = G_MAXSIZE;
  }
  // If we're only considering the direct relation (@max_relation_span <= 1) we can directly read the
  // adjacency-matrix,
  if (max_relation_span == 0 || max_relation_span == 1) {
    rv = (adj_mat[an_meta_first_id][an_meta_second_id] & cond_types) != 0;
    if (rv && relations_path) {
      path = *relations_path;

      /* re-use array if possible */
      if (path != NULL && (g_array_get_element_size (path) != sizeof (gint)
              || path->len < 2)) {
        g_array_free (path, TRUE);
        path = NULL;
      }

      if (path == NULL) {
        path = g_array_sized_new (FALSE, FALSE, sizeof (gint), 2);
      }
      g_array_index (path, gint, 0) = an_meta_first_id;
      g_array_index (path, gint, 1) = an_meta_second_id;
      path->len = 2;
      *relations_path = path;
    }
  } else {

    level = g_malloc (sizeof (gint) * adj_mat_order);
    parent = g_malloc (sizeof (gint) * adj_mat_order);
    gst_analytics_relation_meta_bfs (an_meta_first_id,
        (const guint8 **) adj_mat, adj_mat_order, cond_types, span, level,
        parent);

    GST_CAT_TRACE (GST_CAT_AN_RELATION, "Adj order:%" G_GSIZE_FORMAT,
        adj_mat_order);

    rv = level[an_meta_second_id] != -1;
    if (rv && relations_path) {
      path_left = level[an_meta_second_id] + 1;
      i = parent[an_meta_second_id];
      if (i != -1) {
        path = *relations_path;

        /* re-use array if possible */
        if (path != NULL && (g_array_get_element_size (path) != sizeof (gint)
                || path->len < path_left)) {
          g_array_free (path, TRUE);
          path = NULL;
        }

        if (path == NULL) {
          path = g_array_sized_new (FALSE, FALSE, sizeof (gint), path_left);
        }

        path->len = path_left;
        g_array_index (path, gint, --path_left) = an_meta_second_id;
        //path = g_slist_prepend (path, GINT_TO_POINTER (an_meta_second_id));
        while (i != -1 && i != an_meta_second_id) {
          GST_CAT_TRACE (GST_CAT_AN_RELATION, "Relation parent of %d", i);
          g_array_index (path, gint, --path_left) = i;
          //path = g_slist_prepend (path, GINT_TO_POINTER (i));
          i = parent[i];
        }

        while (path_left > 0) {
          g_array_index (path, gint, --path_left) = -1;
        }
      }
      *relations_path = path;
    }

    g_free (level);
    g_free (parent);
  }

  GST_CAT_TRACE (GST_CAT_AN_RELATION,
      "Relation %x between %d and %d %s",
      cond_types, an_meta_first_id, an_meta_second_id,
      rv ? "exist" : "does not exist");
  return rv;
}


/**
 * gst_buffer_add_analytics_relation_meta:
 * @buffer: (transfer none): a #GstBuffer
 *
 * Attach a analysis-results-meta-relation  meta (#GstAnalyticsRelationMeta)to @buffer.
 *
 * A #GstAnalyticsRelationMeta is a metadata describing relation between other
 * analysis meta. It's more efficient to use #gst_buffer_add_analytics_relation_meta_full
 * and providing the maximum number of analysis meta that will attached to a buffer.
 *
 * Returns: (transfer none) (nullable) : Newly attached #GstAnalyticsRelationMeta
 * Since: 1.24
 */
GstAnalyticsRelationMeta *
gst_buffer_add_analytics_relation_meta (GstBuffer * buffer)
{
  GstAnalyticsRelationMetaInitParams init_params = { 5, 1024 };
  return gst_buffer_add_analytics_relation_meta_full (buffer, &init_params);
}

/**
 * gst_buffer_add_analytics_relation_meta_full:
 * @buffer: (transfer none): a #GstBuffer
 * @init_params: Initialization parameters
 *
 * Attache a analysis-results relation-meta (#GstAnalyticsRelationMeta) to @buffer.
 *
 * A #GstAnalyticsRelationMeta is a metadata describing relation between other
 * analysis meta.
 *
 * Returns: (transfer none) (nullable) : Newly attached #GstAnalyticsRelationMeta
 * Since: 1.24
 */
GstAnalyticsRelationMeta *
gst_buffer_add_analytics_relation_meta_full (GstBuffer * buffer,
    GstAnalyticsRelationMetaInitParams * init_params)
{
  GstAnalyticsRelationMeta *meta = NULL;
  g_return_val_if_fail (init_params != NULL, NULL);
  g_return_val_if_fail (buffer != NULL, NULL);

  // We only want one relation-meta on a buffer, will check if one already
  // exist.
  meta = (GstAnalyticsRelationMeta *) gst_buffer_get_meta (buffer,
      GST_ANALYTICS_RELATION_META_API_TYPE);

  if (!meta) {
    meta =
        (GstAnalyticsRelationMeta *) gst_buffer_add_meta (buffer,
        GST_ANALYTICS_RELATION_META_INFO, init_params);
  }

  return meta;
}

/**
 * gst_buffer_get_analytics_relation_meta:
 * @buffer: a #GstBuffer
 *
 * Retrives the meta or %NULL if it doesn't exist
 *
 * Returns: (transfer none) (nullable) :The #GstAnalyticsRelationMeta if there is one
 * Since: 1.24:
 */
GstAnalyticsRelationMeta *
gst_buffer_get_analytics_relation_meta (GstBuffer * buffer)
{
  return (GstAnalyticsRelationMeta *)
      gst_buffer_get_meta (buffer, GST_ANALYTICS_RELATION_META_API_TYPE);
}

/**
 * gst_analytics_relation_meta_add_mtd: (skip)
 * @meta: Instance
 * @impl: Implementation of relatable (#GstAnalyticsRelatableMtd)
 * @size: Size required
 * @rlt_mtd: Updated handle
 *
 * Add a relatable metadata to @meta. This method is meant to be used by
 * new struct sub-classing GstAnalyticsRelatableMtd.
 *
 * Returns: A pointer to a memory area of size @size where to put the data
 * Since: 1.24
 */
gpointer
gst_analytics_relation_meta_add_mtd (GstAnalyticsRelationMeta * meta,
    const GstAnalyticsMtdImpl * impl, gsize size, GstAnalyticsMtd * rlt_mtd)
{
  gsize object_size;
  gsize new_size;
  GstAnalyticsRelatableMtdData *dest = NULL;
  gpointer mem;
  guint8 **new_adj_mat;
  gsize new_mem_cap, new_rel_order;
  GST_CAT_TRACE (GST_CAT_AN_RELATION, "Adding relatable metadata to rmeta %p",
      meta);

  object_size = sizeof (GstAnalyticsRelatableMtdData);
  object_size += sizeof (gpointer) * (size / sizeof (gpointer));
  if (size % sizeof (gpointer))
    object_size += sizeof (gpointer);
  new_size = meta->offset + object_size;

  if (new_size > meta->max_size) {

    if (new_size > meta->max_size_increment + meta->offset) {
      new_mem_cap = new_size;
    } else {
      new_mem_cap = meta->max_size + meta->max_size_increment;
    }

    mem = g_realloc (meta->analysis_results, new_mem_cap);
    meta->max_size = new_mem_cap;
    meta->analysis_results = mem;
  }

  if (meta->length >= meta->rel_order) {
    new_rel_order = meta->rel_order + meta->rel_order_increment;
    new_adj_mat = gst_analytics_relation_adj_mat_dup (meta->adj_mat,
        meta->rel_order, new_rel_order);
    g_free (meta->adj_mat);
    meta->adj_mat = new_adj_mat;

    mem = g_realloc (meta->mtd_data_lookup, sizeof (gpointer) * new_rel_order);
    meta->mtd_data_lookup = mem;
    meta->rel_order = new_rel_order;
  }

  if (new_size <= meta->max_size && (meta->length < meta->rel_order)) {
    dest =
        (GstAnalyticsRelatableMtdData *) (meta->analysis_results +
        meta->offset);
    dest->impl = impl;
    dest->id = gst_analytics_relation_meta_get_next_id (meta);
    dest->size = size;
    meta->mtd_data_lookup[dest->id] = meta->offset;
    meta->offset += object_size;
    meta->length++;
    rlt_mtd->id = dest->id;
    rlt_mtd->meta = meta;
    GST_CAT_TRACE (GST_CAT_AN_RELATION, "Add %p relatable type=%s (%"
        G_GSIZE_FORMAT " / %" G_GSIZE_FORMAT ").", dest,
        impl->name, new_size, meta->max_size);
  } else {
    GST_CAT_ERROR (GST_CAT_AN_RELATION,
        "Failed to add relatable, out-of-space (%" G_GSIZE_FORMAT " / %"
        G_GSIZE_FORMAT ").", new_size, meta->max_size);
  }
  return &dest->data[0];
}

/**
 * gst_analytics_relation_meta_get_mtd:
 * @meta: Instance of GstAnalyticsRelationMeta
 * @an_meta_id: Id of GstAnalyticsMtd instance to retrieve
 * @type: Filter on a specific type of analysis, use
 *  %GST_ANALYTICS_MTD_TYPE_ANY to match any type
 * @rlt: (out caller-allocates)(not nullable): Will be filled with relatable
 *    meta
 *
 * Fill @rlt if a analytics-meta with id == @an_meta_id exist in @meta instance,
 * otherwise this method return FALSE and @rlt is invalid.
 *
 * Returns: TRUE if successful.
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_get_mtd (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsMtdType type, GstAnalyticsMtd * rlt)
{
  GstAnalyticsRelatableMtdData *d;

  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (rlt, FALSE);

  rlt->meta = NULL;

  if (an_meta_id >= meta->length) {
    GST_CAT_ERROR (GST_CAT_AN_RELATION, "Invalid parameter");
    return FALSE;
  }

  d = gst_analytics_relation_meta_get_mtd_data_internal (meta, an_meta_id);
  if (d == NULL)
    return FALSE;

  if (d->impl == NULL) {
    return FALSE;
  }

  if (type != GST_ANALYTICS_MTD_TYPE_ANY &&
      d->impl != (GstAnalyticsMtdImpl *) type) {
    return FALSE;
  }

  rlt->meta = meta;
  rlt->id = an_meta_id;

  return TRUE;
}

/**
 * gst_analytics_relation_meta_get_mtd_data: (skip)
 * @meta: Instance of GstAnalyticsRelationMeta
 * @an_meta_id: Id of GstAnalyticsMtd instance to retrieve
 *
 * Returns:(nullable): Analytics data pointer
 * Since: 1.24
 */
gpointer
gst_analytics_relation_meta_get_mtd_data (GstAnalyticsRelationMeta *
    meta, guint an_meta_id)
{
  GstAnalyticsRelatableMtdData *rv =
      gst_analytics_relation_meta_get_mtd_data_internal (meta, an_meta_id);
  return &rv->data[0];
}


/**
 * gst_analytics_relation_meta_get_direct_related:
 * @meta: GstAnalyticsRelationMeta instance where to query for
 *    GstAnalyticsRelatableMtd.
 * @an_meta_id: Id of GstAnalyticsMtd involved in relation to query
 * @relation_type: Type of relation to filter on.
 * @type: Type of GstAnalyticsMtd to filter on
 * @state: (inout) (not nullable): Opaque data to store state of the query.
 *    If @state point to NULL, the first analytics-metadata directly related
 *    to @an_meta_id will be set in @rlt_mtd. Doesn't need to be free.
 * @rlt_mtd: Handle updated to directly related relatable meta.
 *
 * Returns: TRUE if @rlt_mtd was updated, other wise FALSE
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_get_direct_related (GstAnalyticsRelationMeta * meta,
    guint an_meta_id, GstAnalyticsRelTypes relation_type,
    GstAnalyticsMtdType type, gpointer * state, GstAnalyticsMtd * rlt_mtd)
{
  guint8 **adj_mat;
  gsize adj_mat_order;
  GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *) meta;
  GstAnalyticsRelatableMtdData *rlt_mtd_data = NULL;
  gsize i;

  GST_CAT_TRACE (GST_CAT_AN_RELATION,
      "Looking for %s related to %u by %d",
      gst_analytics_mtd_type_get_name (type), an_meta_id, relation_type);

  g_return_val_if_fail (rmeta != NULL, FALSE);

  if (state) {
    if (*state) {
      i = ~G_MINSSIZE & (GPOINTER_TO_SIZE (*state) + 1);
    } else {
      i = 0;
      *state = GSIZE_TO_POINTER (G_MINSSIZE | i);
    }
  } else {
    i = 0;
  }

  adj_mat_order = meta->rel_order;

  if (adj_mat_order < (an_meta_id + 1)) {
    GST_CAT_DEBUG (GST_CAT_AN_RELATION,
        "Testing relation existence for analysis-meta that have no index in "
        "adj-mat.");
    return FALSE;
  }

  rlt_mtd->meta = rmeta;
  adj_mat = meta->adj_mat;
  for (; i < adj_mat_order; i++) {
    if (adj_mat[an_meta_id][i] & relation_type) {
      rlt_mtd_data = (GstAnalyticsRelatableMtdData *)
          (meta->mtd_data_lookup[i] + meta->analysis_results);
      rlt_mtd->id = rlt_mtd_data->id;
      if (type == GST_ANALYTICS_MTD_TYPE_ANY
          || gst_analytics_mtd_get_mtd_type (rlt_mtd) == type) {
        if (state) {
          *state = GSIZE_TO_POINTER (G_MINSSIZE | i);
        }
        GST_CAT_TRACE (GST_CAT_AN_RELATION, "Found match at %" G_GSIZE_FORMAT,
            i);
        break;
      }
      rlt_mtd_data = NULL;
    }
  }

  return rlt_mtd_data != NULL;
}

/**
 * gst_analytics_relation_meta_iterate:
 * @meta: Instance of GstAnalyticsRelationMeta
 * @state: Opaque data to store iteration state, initialize to NULL, no need to
 *    free it.
 * @type: Type of GstAnalyticsMtd to iterate on or use
 *  %GST_ANALYTICS_MTD_TYPE_ANY for any.
 * @rlt_mtd: Handle updated to iterated GstAnalyticsRelatableMtd.
 *
 * Returns: FALSE if end was reached and iteration is completed.
 * Since: 1.24
 */
gboolean
gst_analytics_relation_meta_iterate (GstAnalyticsRelationMeta * meta,
    gpointer * state, GstAnalyticsMtdType type, GstAnalyticsMtd * rlt_mtd)
{
  gsize index;
  gsize len = gst_analytics_relation_get_length (meta);
  GstAnalyticsRelatableMtdData *rlt_mtd_data = NULL;

  g_return_val_if_fail (rlt_mtd != NULL, FALSE);

  if (*state) {
    index = ~G_MINSSIZE & (GPOINTER_TO_SIZE (*state) + 1);
  } else {
    index = 0;
    *state = GSIZE_TO_POINTER (G_MINSSIZE | index);
  }
  for (; index < len; index++) {
    rlt_mtd_data = (GstAnalyticsRelatableMtdData *)
        (meta->mtd_data_lookup[index] + meta->analysis_results);
    rlt_mtd->id = rlt_mtd_data->id;
    rlt_mtd->meta = meta;
    if (type == GST_ANALYTICS_MTD_TYPE_ANY ||
        gst_analytics_mtd_get_mtd_type (rlt_mtd) == type) {
      *state = GSIZE_TO_POINTER (G_MINSSIZE | index);
      return TRUE;
    }
  }

  return FALSE;
}
