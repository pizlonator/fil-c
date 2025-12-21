/* GStreamer
 *
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
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
#  include "config.h"
#endif

#include <gst/gl/gl.h>
#undef GST_CAT_DEFAULT
#include <gst/check/gstcheck.h>

#define VEC4_FORMAT "10.4f %10.4f %10.4f %10.4f"
#define VEC4_ARGS(v) (v)[0], (v)[1], (v)[2], (v)[3]
#define EPSILON 0.0001f
#define FEQ(a,b) (fabs(a-b) < EPSILON)

static void
debug_matrix (const float *m)
{
  int i;
  for (i = 0; i < 4; i++) {
    GST_DEBUG ("%" VEC4_FORMAT, VEC4_ARGS (&m[i * 4]));
  }
}

GST_START_TEST (test_matrix_multiply)
{
  /* A * B == C */
  const float A[] = {
    1., 1., 2., 5.,
    0., 3., 0., 1.,
    2., 0., 3., 1.,
    3., 2., 1., 0.,
  };

  const float B[] = {
    3., 1., 0., 2.,
    1., 0., 3., 2.,
    0., 1., 2., 3.,
    3., 2., 1., 0.,
  };

  const float C[] = {
    19., 13., 12., 10.,
    6., 2., 10., 6.,
    9., 7., 7., 13.,
    11., 4., 8., 13.,
  };

  float res[16];
  int i;

  gst_gl_multiply_matrix4 (A, B, res);
  GST_DEBUG ("Matrix A:");
  debug_matrix (A);
  GST_DEBUG ("Matrix B:");
  debug_matrix (B);
  GST_DEBUG ("Matrix C:");
  debug_matrix (C);
  GST_DEBUG ("Multiplication Result == C == A * B:");
  debug_matrix (res);

  for (i = 0; i < G_N_ELEMENTS (res); i++) {
    fail_unless (FEQ (res[i], C[i]), "value %f at index %u does not match "
        "expected value %f", res[i], i, C[i]);
  }
}

GST_END_TEST;

GST_START_TEST (test_matrix_ndc)
{
  GstBuffer *buffer = gst_buffer_new ();
  GstVideoAffineTransformationMeta *aff_meta;
  float res[16];
  int i;

  const float m[] = {
    1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.,
  };

  const float n[] = {
    4., 6., 4., 9.,
    1., 5., 8., 2.,
    9., 3., 5., 8.,
    3., 7., 9., 1.,
  };

  aff_meta = gst_buffer_add_video_affine_transformation_meta (buffer);

  /* test default identity matrix */
  gst_gl_get_affine_transformation_meta_as_ndc (aff_meta, res);
  GST_DEBUG ("Default matrix in the affine meta:");
  debug_matrix (res);

  for (i = 0; i < G_N_ELEMENTS (res); i++) {
    fail_unless (FEQ (res[i], m[i]), "value %f at index %u does not match "
        "expected value %f", res[i], i, m[i]);
  }

  /* test setting and receiving the same values */
  GST_DEBUG ("Set matrix on the affine transformation meta:");
  debug_matrix (n);

  gst_gl_set_affine_transformation_meta_from_ndc (aff_meta, n);
  gst_gl_get_affine_transformation_meta_as_ndc (aff_meta, res);

  GST_DEBUG ("Retrieve the matrix set on the affine meta:");
  debug_matrix (res);

  for (i = 0; i < G_N_ELEMENTS (res); i++) {
    fail_unless (FEQ (res[i], n[i]), "value %f at index %u does not match "
        "expected value %f", res[i], i, n[i]);
  }

  gst_buffer_unref (buffer);
}

GST_END_TEST;

static void
transpose_matrix4 (float *m, float *res)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      int idx = i + (j * 4);
      int swapped_idx = j + (i * 4);

      GST_ERROR ("swapping %i %f into %i", idx, m[idx], swapped_idx);

      if (i == j)
        fail_unless (idx == swapped_idx);

      res[swapped_idx] = m[idx];
    }
  }
}

static float
dot4 (float *v1, float *v2)
{
  GST_TRACE ("%.4f * %.4f + %.4f * %.4f + %.4f * %.4f + %.4f * %.4f",
      v1[0], v2[0], v1[1], v2[1], v1[2], v2[2], v1[3], v2[3]);
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2] + v1[3] * v2[3];
}

/* m * v */
static void
_matrix_mult_vertex4 (float *m, float *v, float *res)
{
  res[0] = dot4 (&m[0], v);
  res[1] = dot4 (&m[4], v);
  res[2] = dot4 (&m[8], v);
  res[3] = dot4 (&m[12], v);
}

/* v * m */
/* Used because the default is for OpenGL to read matrices transposed on
 * uploading */
static void
_vertex_mult_matrix4 (float *m, float *v, float *res)
{
  float tmp[16] = { 0., };

  GST_TRACE ("original matrix");
  debug_matrix (m);
  transpose_matrix4 (m, tmp);
  GST_TRACE ("transposed matrix");
  debug_matrix (tmp);
  _matrix_mult_vertex4 (tmp, v, res);
}

GST_START_TEST (test_matrix_vertex_identity)
{
  float identity[] = {
    1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.,
  };

  float v[] = { 1., 1., 1., 1. };
  float res[4] = { 0., };
  int i;

  _vertex_mult_matrix4 (identity, v, res);
  GST_DEBUG ("vertex: %" VEC4_FORMAT, VEC4_ARGS (v));
  GST_DEBUG ("result: %" VEC4_FORMAT, VEC4_ARGS (res));

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], v[i]), "value %f at index %u does not match "
        "expected value %f", res[i], i, v[i]);
  }
}

GST_END_TEST;

GST_START_TEST (test_matrix_vertex_scale)
{
  float scale[] = {
    1.5, 0., 0., 0.,
    0., 2.5, 0., 0.,
    0., 0., 3., 0.,
    0., 0., 0., 1.,
  };

  float v[] = { 1., 1., 1., 1. };
  float expected[] = { 1.5, 2.5, 3., 1. };
  float res[4] = { 0., };
  int i;

  _vertex_mult_matrix4 (scale, v, res);
  GST_DEBUG ("vertex: %" VEC4_FORMAT, VEC4_ARGS (v));
  GST_DEBUG ("result: %" VEC4_FORMAT, VEC4_ARGS (res));

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], expected[i]),
        "value %f at index %u does not match " "expected value %f", res[i], i,
        expected[i]);
  }
}

GST_END_TEST;

GST_START_TEST (test_matrix_vertex_translate)
{
  float translate_1[] = {
    1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    1., 2., 3., 1.,
  };

  float v[] = { 1., 1., 1., 1. };
  float expected[] = { 2., 3., 4., 1. };
  float res[4] = { 0., };
  int i;

  _vertex_mult_matrix4 (translate_1, v, res);

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], expected[i]),
        "value %f at index %u does not match " "expected value %f", res[i], i,
        expected[i]);
  }
}

GST_END_TEST;

GST_START_TEST (test_matrix_vertex_y_invert)
{
  GstBuffer *buffer = gst_buffer_new ();
  GstVideoAffineTransformationMeta *aff_meta;

  float y_invert[] = {
    1., 0., 0., 0.,
    0., -1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.,
  };

  float v[] = { 1., 1., 1., 1. };
  float expected[] = { 1., -1., 1., 1. };
  float res[4] = { 0., };
  int i;

  /* The y_invert matrix but with a coordinate space of [0, 1]^3 instead
   * of [-1, 1]^3 */

  aff_meta = gst_buffer_add_video_affine_transformation_meta (buffer);

  GST_DEBUG ("y-invert");
  debug_matrix (y_invert);

  _vertex_mult_matrix4 (y_invert, v, res);
  GST_DEBUG ("vertex: %" VEC4_FORMAT, VEC4_ARGS (v));
  GST_DEBUG ("result: %" VEC4_FORMAT, VEC4_ARGS (res));

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], expected[i]),
        "value %f at index %u does not match " "expected value %f", res[i], i,
        expected[i]);
  }

  /* now test the [0, 1]^3 matrix and update the test values accordingly */
  gst_gl_set_affine_transformation_meta_from_ndc (aff_meta, y_invert);
  expected[1] = 0.;

  GST_DEBUG ("y-invert from ndc [-1,1]^3 to [0,1]^3");
  debug_matrix (aff_meta->matrix);

  _vertex_mult_matrix4 (aff_meta->matrix, v, res);
  GST_DEBUG ("vertex: %" VEC4_FORMAT, VEC4_ARGS (v));
  GST_DEBUG ("result: %" VEC4_FORMAT, VEC4_ARGS (res));

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], expected[i]),
        "value %f at index %u does not match " "expected value %f", res[i], i,
        expected[i]);
  }

  /* test vec4(1,0,1,1) -> vec4(1,1,1,1) */
  v[1] = 0.;
  expected[1] = 1.;
  _vertex_mult_matrix4 (aff_meta->matrix, v, res);
  GST_DEBUG ("vertex: %" VEC4_FORMAT, VEC4_ARGS (v));
  GST_DEBUG ("result: %" VEC4_FORMAT, VEC4_ARGS (res));

  for (i = 0; i < 4; i++) {
    fail_unless (FEQ (res[i], expected[i]),
        "value %f at index %u does not match " "expected value %f", res[i], i,
        expected[i]);
  }

  gst_buffer_unref (buffer);
}

GST_END_TEST;

static Suite *
gst_gl_matrix_suite (void)
{
  Suite *s = suite_create ("GstGLMatrix");
  TCase *tc_chain = tcase_create ("matrix");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_matrix_multiply);
  tcase_add_test (tc_chain, test_matrix_ndc);
  tcase_add_test (tc_chain, test_matrix_vertex_identity);
  tcase_add_test (tc_chain, test_matrix_vertex_scale);
  tcase_add_test (tc_chain, test_matrix_vertex_translate);
  tcase_add_test (tc_chain, test_matrix_vertex_y_invert);

  return s;
}

GST_CHECK_MAIN (gst_gl_matrix);
