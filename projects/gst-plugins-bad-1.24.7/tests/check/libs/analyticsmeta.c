/* GStreamer
 * Copyright (C) 2022 Collabora Ltd
 *
 * analyticmeta.c
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

#include <gst/check/gstcheck.h>
#include <gst/analytics/analytics.h>

GST_START_TEST (test_add_classification_meta)
{
  /* Verify we can create a relation metadata
   * and attach it classification mtd
   */
  GstBuffer *buf;
  GstAnalyticsRelationMeta *rmeta;
  GstAnalyticsClsMtd cls_mtd;
  gfloat conf_lvl[] = { 0.5f, 0.5f };
  GQuark class_quarks[2];
  gboolean ret;

  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  buf = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta (buf);
  ret = gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, conf_lvl,
      class_quarks, &cls_mtd);
  fail_unless (ret == TRUE);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_meta_pooled)
{
  GstBufferPool *pool;
  GstStructure *config;
  GstBuffer *buf;
  GstAnalyticsRelationMeta *rmeta1, *rmeta2;

  pool = gst_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, NULL, 1, 1, 1);
  gst_buffer_pool_set_config (pool, config);
  gst_buffer_pool_set_active (pool, TRUE);

  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  rmeta1 = gst_buffer_add_analytics_relation_meta (buf);
  gst_buffer_unref (buf);

  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);

  rmeta2 = gst_buffer_add_analytics_relation_meta (buf);

  fail_unless (rmeta1 == rmeta2);
  gst_buffer_unref (buf);
  gst_object_unref (pool);
}

GST_END_TEST;

GST_START_TEST (test_classification_meta_classes)
{
  /* Verify we can retrieve classification data
   * from the relation metadata
   */
  GstBuffer *buf;
  GstAnalyticsRelationMeta *rmeta;
  gboolean ret;
  GQuark class_quarks[2];
  GstAnalyticsClsMtd cls_mtd, cls_mtd2;

  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  buf = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta (buf);
  gfloat conf_lvl[] = { 0.6f, 0.4f };
  ret = gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, conf_lvl,
      class_quarks, &cls_mtd);
  fail_unless (ret == TRUE);
  fail_unless (gst_analytics_relation_get_length (rmeta) == 1);

  gint dogIndex = gst_analytics_cls_mtd_get_index_by_quark (&cls_mtd,
      class_quarks[0]);
  fail_unless (dogIndex == 0);
  gfloat confLvl = gst_analytics_cls_mtd_get_level (&cls_mtd, dogIndex);
  GST_LOG ("dog:%f", confLvl);
  assert_equals_float (confLvl, 0.6f);

  gint catIndex = gst_analytics_cls_mtd_get_index_by_quark (&cls_mtd,
      g_quark_from_string ("cat"));
  confLvl = gst_analytics_cls_mtd_get_level (&cls_mtd, catIndex);
  GST_LOG ("Cat:%f", confLvl);
  assert_equals_float (confLvl, 0.4f);
  assert_equals_int (gst_analytics_mtd_get_id ((GstAnalyticsMtd *) & cls_mtd),
      0);

  conf_lvl[0] = 0.1f;
  conf_lvl[1] = 0.9f;
  ret =
      gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, conf_lvl,
      class_quarks, &cls_mtd2);
  fail_unless (ret == TRUE);
  fail_unless (gst_analytics_relation_get_length (rmeta) == 2);

  dogIndex = gst_analytics_cls_mtd_get_index_by_quark (&cls_mtd2,
      class_quarks[0]);
  confLvl = gst_analytics_cls_mtd_get_level (&cls_mtd2, dogIndex);
  assert_equals_float (confLvl, 0.1f);

  catIndex = gst_analytics_cls_mtd_get_index_by_quark (&cls_mtd2,
      class_quarks[0]);
  confLvl = gst_analytics_cls_mtd_get_level (&cls_mtd2, catIndex);
  assert_equals_float (confLvl, 0.1f);

  /* Verify the relation meta contain the correct number of relatable metadata */
  fail_unless (gst_analytics_relation_get_length (rmeta) == 2);

  /* Verify first relatable metadata has the correct id. */
  assert_equals_int (gst_analytics_mtd_get_id ((GstAnalyticsMtd *) & cls_mtd),
      0);

  /* Verify second relatable metadata has the correct id. */
  assert_equals_int (gst_analytics_mtd_get_id (
          (GstAnalyticsMtd *) & cls_mtd2), 1);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_add_relation_meta)
{
  /* Verify we set a relation between relatable metadata. */

  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *relations;
  GQuark class_quarks[2];
  gboolean ret;

  buf = gst_buffer_new ();
  relations = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  gfloat conf_lvl[] = { 0.6f, 0.4f };
  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[0]);
  fail_unless (ret == TRUE);
  gst_analytics_mtd_get_id ((GstAnalyticsMtd *)
      & cls_mtd[0]);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;

  class_quarks[0] = g_quark_from_string ("plant");
  class_quarks[1] = g_quark_from_string ("animal");


  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[1]);
  gst_analytics_mtd_get_id ((GstAnalyticsMtd *)
      & cls_mtd[1]);

  fail_unless (gst_analytics_relation_meta_set_relation (relations,
          GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id,
          cls_mtd[1].id) == TRUE);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_add_relation_inefficiency_reporting_cases)
{
  /*
   * Verify inefficiency of relation order is reported.
   */
  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsRelationMetaInitParams init_params = { 2, 10 };
  GstAnalyticsRelationMeta *relations;
  gboolean ret;
  GQuark class_quarks[2];


  buf = gst_buffer_new ();
  relations = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  gfloat conf_lvl[] = { 0.6f, 0.4f };

  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[0]);
  fail_unless (gst_analytics_relation_get_length (relations) == 1);
  fail_unless (ret == TRUE);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("plant");
  class_quarks[1] = g_quark_from_string ("animal");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[1]);
  fail_unless (gst_analytics_relation_get_length (relations) == 2);
  fail_unless (ret == TRUE);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("male");
  class_quarks[1] = g_quark_from_string ("female");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[2]);
  fail_unless (gst_analytics_relation_get_length (relations) == 3);
  fail_unless (ret == TRUE);

  fail_unless (gst_analytics_relation_meta_set_relation (relations,
          GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id, cls_mtd[1].id)
      == TRUE);
  fail_unless (gst_analytics_relation_meta_set_relation (relations,
          GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id, cls_mtd[2].id)
      == TRUE);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_query_relation_meta_cases)
{
  /* Verify we can query existence of direct and indirect relation */
  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsRelationMetaInitParams init_params = { 2, 150 };
  GstAnalyticsRelationMeta *relations;
  gboolean ret;
  GQuark class_quarks[2];

  buf = gst_buffer_new ();
  relations = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  gfloat conf_lvl[] = { 0.6f, 0.4f };

  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[0]);
  fail_unless (ret == TRUE);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("plant");
  class_quarks[1] = g_quark_from_string ("animal");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[1]);
  fail_unless (ret == TRUE);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("male");
  class_quarks[1] = g_quark_from_string ("female");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[2]);
  fail_unless (ret == TRUE);

  // Pet is part of kingdom
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id, cls_mtd[1].id);

  // Kingdom contain pet
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_CONTAIN, cls_mtd[1].id, cls_mtd[0].id);

  // Pet contain gender
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_CONTAIN, cls_mtd[0].id, cls_mtd[2].id);

  /* Query if pet relate kingdom through a IS_PART relation with a maximum
   * relation span of 1. Max relation span of 1 mean they directly related.*/
  gboolean exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[1].id, 1, GST_ANALYTICS_REL_TYPE_IS_PART_OF, NULL);
  fail_unless (exist == TRUE);

  /* Query if pet relate to gender through a IS_PART relation. */
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[2].id, 1, GST_ANALYTICS_REL_TYPE_IS_PART_OF, NULL);
  fail_unless (exist == FALSE);

  /* Query if pet relate to kingdom through a CONTAIN relation. */
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[1].id, 1, GST_ANALYTICS_REL_TYPE_CONTAIN, NULL);
  fail_unless (exist == FALSE);

  GstAnalyticsRelTypes cond =
      GST_ANALYTICS_REL_TYPE_IS_PART_OF | GST_ANALYTICS_REL_TYPE_CONTAIN |
      GST_ANALYTICS_REL_TYPE_RELATE_TO;

  /* Query if pet relate to gender through IS_PART or CONTAIN or
   * RELATE_TO relation. */
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[2].id, 1, cond, NULL);
  fail_unless (exist == TRUE);

  /* Query if pet relate to kindom through CONTAIN or RELATE_TO relation */
  cond = GST_ANALYTICS_REL_TYPE_CONTAIN | GST_ANALYTICS_REL_TYPE_RELATE_TO;
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[1].id, 1, cond, NULL);
  fail_unless (exist == FALSE);

  /* Query if kingdom relate to gender through a CONTAIN relation with a maximum
   * relation span of 1. */
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[1].id,
      cls_mtd[2].id, 1, GST_ANALYTICS_REL_TYPE_CONTAIN, NULL);

  /* We expect this to fail because kingdom relate to gender CONTAIN relations
   * but indirectly (via pet) and we set the max relation span to 1*/
  fail_unless (exist == FALSE);

  /* Same has previous check but using INFINIT relation span */
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[1].id,
      cls_mtd[2].id, GST_INF_RELATION_SPAN, GST_ANALYTICS_REL_TYPE_CONTAIN,
      NULL);
  fail_unless (exist == TRUE);

  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[2].id,
      cls_mtd[1].id, GST_INF_RELATION_SPAN, GST_ANALYTICS_REL_TYPE_CONTAIN,
      NULL);
  fail_unless (exist == FALSE);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_path_relation_meta)
{
  /* Verify we can retrieve relation path */
  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsRelationMetaInitParams init_params = { 2, 150 };
  GstAnalyticsRelationMeta *relations;
  gboolean ret;
  GQuark class_quarks[2];

  buf = gst_buffer_new ();
  relations = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  gfloat conf_lvl[] = { 0.6f, 0.4f };
  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[0]);
  fail_unless (ret == TRUE);
  fail_unless (cls_mtd[0].id == 0);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("plant");
  class_quarks[1] = g_quark_from_string ("animal");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[1]);
  fail_unless (ret == TRUE);
  fail_unless (cls_mtd[1].id == 1);

  conf_lvl[0] = 0.6f;
  conf_lvl[1] = 0.4f;
  class_quarks[0] = g_quark_from_string ("male");
  class_quarks[1] = g_quark_from_string ("female");

  ret = gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[2]);
  fail_unless (ret == TRUE);
  fail_unless (cls_mtd[2].id == 2);

  // Pet is part of kingdom
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id, cls_mtd[1].id);

  // Kingdom contain pet
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_CONTAIN, cls_mtd[1].id, cls_mtd[0].id);

  // Pet contain gender
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_CONTAIN, cls_mtd[0].id, cls_mtd[2].id);

  GArray *path = NULL;
  GstAnalyticsRelTypes cond = GST_ANALYTICS_REL_TYPE_CONTAIN;
  gboolean exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[2].id, GST_INF_RELATION_SPAN, cond, &path);
  if (exist) {
    fail_unless (path != NULL);
    gint i;
    guint path_check_ids[] = { 0, 2 };
    fail_unless (path->len == 2);
    for (i = 0; i < path->len; i++) {
      fail_unless (path_check_ids[i] == g_array_index (path, guint, i));
    }
    g_array_free (g_steal_pointer (&path), TRUE);
    fail_unless (i == 2);
  }

  cond = GST_ANALYTICS_REL_TYPE_CONTAIN;
  exist = gst_analytics_relation_meta_exist (relations, cls_mtd[1].id,
      cls_mtd[2].id, GST_INF_RELATION_SPAN, cond, &path);
  if (exist) {
    gint i;
    guint path_check_ids[] = { 1, 0, 2 };
    fail_unless (path != NULL);
    fail_unless (path->len == 3);
    for (i = 0; i < path->len; i++) {
      fail_unless (path_check_ids[i] == g_array_index (path, guint, i));
    }
    g_array_free (g_steal_pointer (&path), TRUE);
    fail_unless (i == 3);
  }
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_cyclic_relation_meta)
{
  /* Verify we can discover cycle in relation and not report multiple time
   * the same node and get into an infinit exploration */

  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsRelationMetaInitParams init_params = { 2, 150 };
  GstAnalyticsRelationMeta *relations;
  gfloat conf_lvl[2];
  GQuark class_quarks[2];

  class_quarks[0] = g_quark_from_string ("attr1");
  class_quarks[1] = g_quark_from_string ("attr2");

  buf = gst_buffer_new ();
  relations = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  conf_lvl[0] = 0.5f;
  conf_lvl[1] = 0.5f;
  gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[0]);

  gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[1]);

  gst_analytics_relation_meta_add_cls_mtd (relations, 2, conf_lvl,
      class_quarks, &cls_mtd[2]);

  // (0) -> (1)
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[0].id, cls_mtd[1].id);

  // (1)->(2)
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[1].id, cls_mtd[2].id);

  // (2) -> (0)
  gst_analytics_relation_meta_set_relation (relations,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd[2].id, cls_mtd[0].id);

  GArray *path = NULL;
  GstAnalyticsRelTypes cond = GST_ANALYTICS_REL_TYPE_CONTAIN;
  gboolean exist = gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[2].id, GST_INF_RELATION_SPAN, cond, &path);
  fail_unless (exist == FALSE);

  cond = GST_ANALYTICS_REL_TYPE_IS_PART_OF;
  exist =
      gst_analytics_relation_meta_exist (relations, cls_mtd[0].id,
      cls_mtd[2].id, GST_INF_RELATION_SPAN, cond, &path);
  fail_unless (exist == TRUE);
  if (exist) {
    gint i;
    guint path_ids[] = { 0, 1, 2 };
    fail_unless (path->len == 3);
    for (i = 0; i < path->len; i++) {
      fail_unless (path_ids[i] == g_array_index (path, guint, i));
    }
    g_array_free (g_steal_pointer (&path), TRUE);
    fail_unless (i == 3);
  }

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_add_od_meta)
{
  /* Verity we can add Object Detection relatable metadata to a relation
   * metadata */
  GstBuffer *buf;
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *rmeta;
  GstAnalyticsODMtd od_mtd;
  gboolean ret;
  buf = gst_buffer_new ();

  rmeta = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  GQuark type = g_quark_from_string ("dog");
  gint x = 20;
  gint y = 20;
  gint w = 10;
  gint h = 15;
  gfloat loc_conf_lvl = 0.6f;
  ret = gst_analytics_relation_meta_add_od_mtd (rmeta, type, x, y,
      w, h, loc_conf_lvl, &od_mtd);
  fail_unless (ret == TRUE);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_od_meta_fields)
{
  /* Verify we can readback fields of object detection metadata */
  GstBuffer *buf;
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *rmeta;
  GstAnalyticsODMtd od_mtd;
  gboolean ret;
  buf = gst_buffer_new ();

  rmeta = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  GQuark type = g_quark_from_string ("dog");
  gint x = 21;
  gint y = 20;
  gint w = 10;
  gint h = 15;
  gfloat loc_conf_lvl = 0.6f;
  ret = gst_analytics_relation_meta_add_od_mtd (rmeta, type, x, y,
      w, h, loc_conf_lvl, &od_mtd);

  fail_unless (ret == TRUE);

  gint _x, _y, _w, _h;
  gfloat _loc_conf_lvl;
  gst_analytics_od_mtd_get_location (&od_mtd, &_x, &_y, &_w, &_h,
      &_loc_conf_lvl);

  fail_unless (_x == x);
  fail_unless (_y == y);
  fail_unless (_w == w);
  fail_unless (_h == h);
  fail_unless (_loc_conf_lvl == loc_conf_lvl);

  _loc_conf_lvl = -200.0;       // dirty this var by setting invalid value.
  gst_analytics_od_mtd_get_confidence_lvl (&od_mtd, &_loc_conf_lvl);

  fail_unless (_loc_conf_lvl == loc_conf_lvl);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_od_cls_relation)
{
  /* Verify we can add a object detection and classification metadata to
   * a relation metadata */

  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd;
  GstAnalyticsODMtd od_mtd;
  /* We intentionally set buffer small than required to verify sanity
   * with re-allocation */
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *rmeta;
  gboolean ret;
  GArray *path = NULL;
  gboolean exist;
  gint _x, _y, _w, _h;
  gfloat _loc_conf_lvl;
  GQuark class_quarks[2];

  buf = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  gfloat conf_lvl[] = { 0.7f, 0.3f };
  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");

  gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, conf_lvl,
      class_quarks, &cls_mtd);

  GQuark type = g_quark_from_string ("dog");
  gint x = 21;
  gint y = 20;
  gint w = 10;
  gint h = 15;
  gfloat loc_conf_lvl = 0.6f;
  gst_analytics_relation_meta_add_od_mtd (rmeta, type, x, y, w, h,
      loc_conf_lvl, &od_mtd);

  ret = gst_analytics_relation_meta_set_relation (rmeta,
      GST_ANALYTICS_REL_TYPE_CONTAIN, od_mtd.id, cls_mtd.id);
  fail_unless (ret == TRUE);

  ret = gst_analytics_relation_meta_set_relation (rmeta,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, cls_mtd.id, od_mtd.id);
  fail_unless (ret == TRUE);

  /* Verify OD relate to CLS only through a CONTAIN relation */
  exist = gst_analytics_relation_meta_exist (rmeta,
      od_mtd.id, cls_mtd.id, GST_INF_RELATION_SPAN,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, NULL);
  fail_unless (exist == FALSE);

  exist = gst_analytics_relation_meta_exist (rmeta,
      od_mtd.id, cls_mtd.id, GST_INF_RELATION_SPAN,
      GST_ANALYTICS_REL_TYPE_CONTAIN, &path);
  fail_unless (exist == TRUE);

  /* Query the relation path and verify it is correct */
  guint ids[2];
  gint i;
  fail_unless (path->len == 2);
  for (i = 0; i < path->len; i++) {
    ids[i] = g_array_index (path, guint, i);
    GST_LOG ("id=%u", ids[i]);
  }
  g_array_free (g_steal_pointer (&path), TRUE);
  fail_unless (ids[0] == 1);
  fail_unless (ids[1] == 0);

  GstAnalyticsMtd rlt_mtd;
  exist = gst_analytics_relation_meta_get_mtd (rmeta, ids[0], 0, &rlt_mtd);
  fail_unless (exist == TRUE);

  GstAnalyticsMtdType mtd_type = gst_analytics_mtd_get_mtd_type (&rlt_mtd);

  /* Verify relatable meta with id == 1 is of type Object Detection */
  fail_unless (mtd_type == gst_analytics_od_mtd_get_mtd_type ());

  gst_analytics_od_mtd_get_location ((GstAnalyticsODMtd *) & rlt_mtd, &_x, &_y,
      &_w, &_h, &_loc_conf_lvl);
  fail_unless (_x == x);
  fail_unless (_y == y);
  fail_unless (_w == w);
  fail_unless (_h == h);
  fail_unless (_loc_conf_lvl == loc_conf_lvl);

  GST_LOG ("mtd_type:%s", gst_analytics_mtd_type_get_name (mtd_type));

  exist = gst_analytics_relation_meta_get_mtd (rmeta, ids[1], 0, &rlt_mtd);
  fail_unless (exist == TRUE);
  mtd_type = gst_analytics_mtd_get_mtd_type (&rlt_mtd);

  /* Verify relatable meta with id == 0 is of type classification */
  fail_unless (mtd_type == gst_analytics_cls_mtd_get_mtd_type ());
  gint index =
      gst_analytics_cls_mtd_get_index_by_quark ((GstAnalyticsClsMtd *) &
      rlt_mtd,
      g_quark_from_string ("dog"));
  gfloat lvl =
      gst_analytics_cls_mtd_get_level ((GstAnalyticsClsMtd *) & rlt_mtd, index);
  GST_LOG ("dog %f [%d, %d %d, %d", lvl, _x, _y, _w, _h);

  fail_unless (lvl == 0.7f);
  index =
      gst_analytics_cls_mtd_get_index_by_quark ((GstAnalyticsClsMtd *) &
      rlt_mtd, g_quark_from_string ("cat"));
  lvl =
      gst_analytics_cls_mtd_get_level ((GstAnalyticsClsMtd *) & rlt_mtd, index);
  fail_unless (lvl == 0.3f);

  GST_LOG ("mtd_type:%s", gst_analytics_mtd_type_get_name (mtd_type));
  GST_LOG ("cat %f [%d, %d %d, %d", lvl, _x, _y, _w, _h);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_multi_od_cls_relation)
{
  GstBuffer *buf;
  GstAnalyticsClsMtd cls_mtd[3];
  GstAnalyticsODMtd od_mtd[2];
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *rmeta;
  guint cls_id, ids[2], i;
  gboolean ret;
  const gint dog_cls_index = 0;
  const gint cat_cls_index = 1;
  gfloat cls_conf_lvl[2], lvl;
  GArray *path = NULL;
  gfloat _loc_conf_lvl;
  gint x, _x, y, _y, w, _w, h, _h;
  GstAnalyticsMtdType mtd_type;
  GQuark cls_type;
  GstAnalyticsMtd mtd;
  gpointer state = NULL;
  GQuark class_quarks[2];
  class_quarks[0] = g_quark_from_string ("dog");
  class_quarks[1] = g_quark_from_string ("cat");


  buf = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta_full (buf, &init_params);

  /* Define first relation ObjectDetection -contain-> Classification */
  cls_conf_lvl[dog_cls_index] = 0.7f;
  cls_conf_lvl[cat_cls_index] = 0.3f;

  gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, cls_conf_lvl,
      class_quarks, &cls_mtd[0]);

  cls_type = g_quark_from_string ("dog");
  x = 21;
  y = 20;
  w = 10;
  h = 15;
  gfloat loc_conf_lvl = 0.6f;
  gst_analytics_relation_meta_add_od_mtd (rmeta, cls_type, x, y, w,
      h, loc_conf_lvl, &od_mtd[0]);

  ret = gst_analytics_relation_meta_set_relation (rmeta,
      GST_ANALYTICS_REL_TYPE_CONTAIN, od_mtd[0].id, cls_mtd[0].id);
  fail_unless (ret == TRUE);
  GST_LOG ("Set rel Obj:%d -c-> Cls:%d", od_mtd[0].id, cls_mtd[0].id);

  /* Define second relation ObjectDetection -contain-> Classification */
  cls_conf_lvl[dog_cls_index] = 0.1f;
  cls_conf_lvl[cat_cls_index] = 0.9f;
  gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, cls_conf_lvl,
      class_quarks, &cls_mtd[1]);

  cls_type = g_quark_from_string ("cat");
  x = 50;
  y = 21;
  w = 11;
  h = 16;
  loc_conf_lvl = 0.7f;
  gst_analytics_relation_meta_add_od_mtd (rmeta, cls_type, x, y, w,
      h, loc_conf_lvl, &od_mtd[1]);

  ret = gst_analytics_relation_meta_set_relation (rmeta,
      GST_ANALYTICS_REL_TYPE_CONTAIN, od_mtd[1].id, cls_mtd[1].id);

  GST_LOG ("Set rel Obj:%d -c-> Cls:%d", od_mtd[1].id, cls_mtd[1].id);
  fail_unless (ret == TRUE);

  /* Query relations */

  /* Query relation between first object detection and first classification
   * and verify they are only related by CONTAIN relation OD relate to
   * CLASSIFICATION through a CONTAIN relation. */
  gboolean exist =
      gst_analytics_relation_meta_exist (rmeta, od_mtd[0].id, cls_mtd[0].id,
      GST_INF_RELATION_SPAN,
      GST_ANALYTICS_REL_TYPE_IS_PART_OF, NULL);
  fail_unless (exist == FALSE);

  exist =
      gst_analytics_relation_meta_exist (rmeta, od_mtd[0].id, cls_mtd[0].id,
      GST_INF_RELATION_SPAN, GST_ANALYTICS_REL_TYPE_CONTAIN, NULL);
  fail_unless (exist == TRUE);


  /* Query relation between second object detection and second classification
   * and verify they are only related by CONTAIN relation OD relate to
   * CLASSIFICATION through a CONTAIN relation.
   */
  exist =
      gst_analytics_relation_meta_exist (rmeta, od_mtd[1].id, cls_mtd[1].id,
      GST_INF_RELATION_SPAN, GST_ANALYTICS_REL_TYPE_CONTAIN, &path);
  fail_unless (exist == TRUE);

  /* Verify relation path between OD second (id=3) and Cls second (id=2)
   * is correct
   */
  fail_unless (path->len == 2);
  for (i = 0; i < path->len; i++) {
    ids[i] = g_array_index (path, guint, i);
    GST_LOG ("id=%u", ids[i]);
  }
  g_array_free (g_steal_pointer (&path), TRUE);
  fail_unless (ids[0] == 3);
  fail_unless (ids[1] == 2);

  /* Verify the relatable metadata 3 is of correct type
   * (ObjectDetection). Verify it describe the correct
   * the correct data.
   */
  gst_analytics_relation_meta_get_mtd (rmeta, ids[0], 0, &mtd);
  mtd_type = gst_analytics_mtd_get_mtd_type (&mtd);
  fail_unless (mtd_type == gst_analytics_od_mtd_get_mtd_type ());

  gst_analytics_od_mtd_get_location ((GstAnalyticsODMtd *) & mtd, &_x, &_y, &_w,
      &_h, &_loc_conf_lvl);
  fail_unless (_x == 50);
  fail_unless (_y == 21);
  fail_unless (_w == 11);
  fail_unless (_h == 16);
  fail_unless (_loc_conf_lvl == 0.7f);

  GST_LOG ("mtd_type:%s", gst_analytics_mtd_type_get_name (mtd_type));

  /* Verify the relatable metadata 2 is of correct type
   * (ObjectDetection).
   */
  gst_analytics_relation_meta_get_mtd (rmeta, ids[1], 0, &mtd);
  mtd_type = gst_analytics_mtd_get_mtd_type (&mtd);
  fail_unless (mtd_type == gst_analytics_cls_mtd_get_mtd_type ());

  /* Verify data of the CLASSIFICATION retrieved */
  gint index =
      gst_analytics_cls_mtd_get_index_by_quark ((GstAnalyticsClsMtd *) & mtd,
      g_quark_from_string ("dog"));
  lvl = gst_analytics_cls_mtd_get_level ((GstAnalyticsClsMtd *) & mtd, index);
  GST_LOG ("dog %f [%d, %d %d, %d", lvl, _x, _y, _w, _h);
  fail_unless (lvl == 0.1f);

  /* Verify data of the CLASSIFICATION retrieved */
  index =
      gst_analytics_cls_mtd_get_index_by_quark ((GstAnalyticsClsMtd *) & mtd,
      g_quark_from_string ("cat"));
  lvl = gst_analytics_cls_mtd_get_level ((GstAnalyticsClsMtd *) & mtd, index);
  GST_LOG ("mtd_type:%s", gst_analytics_mtd_type_get_name (mtd_type));
  GST_LOG ("cat %f [%d, %d %d, %d", lvl, _x, _y, _w, _h);
  fail_unless (lvl == 0.9f);

  /* Retrieve relatable metadata related to the first object detection
   * through a CONTAIN relation of type CLASSIFICATION
   * Verify it's the first classification metadata
   */
  gst_analytics_relation_meta_get_direct_related (rmeta, od_mtd[0].id,
      GST_ANALYTICS_REL_TYPE_CONTAIN, gst_analytics_cls_mtd_get_mtd_type (),
      &state, &mtd);

  cls_id = gst_analytics_mtd_get_id (&mtd);
  GST_LOG ("Obj:%d -> Cls:%d", od_mtd[0].id, cls_id);
  fail_unless (cls_id == cls_mtd[0].id);

  state = NULL;
  /* Retrieve relatable metadata related to the second object detection
   * through a CONTAIN relation of type CLASSIFICATION
   * Verify it's the first classification metadata
   */
  gst_analytics_relation_meta_get_direct_related (rmeta, od_mtd[1].id,
      GST_ANALYTICS_REL_TYPE_CONTAIN, gst_analytics_cls_mtd_get_mtd_type (),
      &state, &mtd);
  cls_id = gst_analytics_mtd_get_id (&mtd);

  GST_LOG ("Obj:%d -> Cls:%d", od_mtd[1].id, cls_id);
  fail_unless (cls_id == cls_mtd[1].id);

  cls_conf_lvl[dog_cls_index] = 0.2f;
  cls_conf_lvl[cat_cls_index] = 0.8f;
  class_quarks[0] = g_quark_from_string ("canine");
  class_quarks[1] = g_quark_from_string ("feline");
  gst_analytics_relation_meta_add_cls_mtd (rmeta, 2, cls_conf_lvl,
      class_quarks, &cls_mtd[2]);

  ret = gst_analytics_relation_meta_set_relation (rmeta,
      GST_ANALYTICS_REL_TYPE_CONTAIN, od_mtd[1].id, cls_mtd[2].id);

  state = NULL;
  ret = gst_analytics_relation_meta_get_direct_related (rmeta, od_mtd[1].id,
      GST_ANALYTICS_REL_TYPE_CONTAIN, gst_analytics_od_mtd_get_mtd_type (),
      &state, &mtd);

  fail_unless (ret == FALSE);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_add_tracking_meta)
{
  /* Verify we can add tracking relatable meta to relation meta */
  GstBuffer *buf1, *buf2;
  GstAnalyticsRelationMetaInitParams init_params = { 5, 150 };
  GstAnalyticsRelationMeta *rmeta;
  GstAnalyticsTrackingMtd tracking_mtd;
  guint tracking_id;
  GstClockTime tracking_observation_time_1;
  gboolean ret;

  /* Verify we can add multiple trackings to relation metadata
   */

  buf1 = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta_full (buf1, &init_params);
  tracking_id = 1;
  tracking_observation_time_1 = GST_BUFFER_TIMESTAMP (buf1);
  ret = gst_analytics_relation_meta_add_tracking_mtd (rmeta, tracking_id,
      tracking_observation_time_1, &tracking_mtd);
  fail_unless (ret == TRUE);

  gst_buffer_unref (buf1);

  buf2 = gst_buffer_new ();
  rmeta = gst_buffer_add_analytics_relation_meta_full (buf2, &init_params);
  tracking_id = 1;
  ret = gst_analytics_relation_meta_add_tracking_mtd (rmeta, tracking_id,
      tracking_observation_time_1, &tracking_mtd);
  fail_unless (ret == TRUE);

  gst_buffer_unref (buf2);
}

GST_END_TEST;

static Suite *
analyticmeta_suite (void)
{

  Suite *s;
  TCase *tc_chain_cls;
  TCase *tc_chain_relation;
  TCase *tc_chain_od;
  TCase *tc_chain_od_cls;
  TCase *tc_chain_tracking;

  s = suite_create ("Analytic Meta Library");

  tc_chain_cls = tcase_create ("Classification Mtd");
  suite_add_tcase (s, tc_chain_cls);
  tcase_add_test (tc_chain_cls, test_add_classification_meta);
  tcase_add_test (tc_chain_cls, test_classification_meta_classes);

  tcase_add_test (tc_chain_cls, test_meta_pooled);

  tc_chain_relation = tcase_create ("Relation Meta");
  suite_add_tcase (s, tc_chain_relation);
  tcase_add_test (tc_chain_relation, test_add_relation_meta);
  tcase_add_test (tc_chain_relation,
      test_add_relation_inefficiency_reporting_cases);
  tcase_add_test (tc_chain_relation, test_query_relation_meta_cases);
  tcase_add_test (tc_chain_relation, test_path_relation_meta);
  tcase_add_test (tc_chain_relation, test_cyclic_relation_meta);

  tc_chain_od = tcase_create ("Object Detection Mtd");
  suite_add_tcase (s, tc_chain_od);
  tcase_add_test (tc_chain_od, test_add_od_meta);
  tcase_add_test (tc_chain_od, test_od_meta_fields);

  tc_chain_od_cls = tcase_create ("Object Detection <-> Classification Mtd");
  suite_add_tcase (s, tc_chain_od_cls);
  tcase_add_test (tc_chain_od_cls, test_od_cls_relation);
  tcase_add_test (tc_chain_od_cls, test_multi_od_cls_relation);

  tc_chain_tracking = tcase_create ("Tracking Mtd");
  suite_add_tcase (s, tc_chain_tracking);
  tcase_add_test (tc_chain_tracking, test_add_tracking_meta);
  return s;
}

GST_CHECK_MAIN (analyticmeta);
