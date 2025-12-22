/*
 * GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/cuda/gstcuda.h>
#include <gst/video/video.h>

static GstCudaContext *context = NULL;
static GQuark memory_tester_quark;

static void
setup_func (void)
{
  context = gst_cuda_context_new (0);
  fail_unless (GST_IS_CUDA_CONTEXT (context));

  memory_tester_quark = g_quark_from_static_string ("gst-cuda-memory-tester");
}

static void
teardown_func (void)
{
  gst_object_unref (context);
}

static void
allocator_finalize_cb (gboolean * alloc_finalized)
{
  *alloc_finalized = TRUE;
}

GST_START_TEST (test_free_active_allocator)
{
  GstCudaPoolAllocator *alloc;
  GstVideoInfo info;
  GstMemory *mem = NULL;
  gboolean ret;
  GstFlowReturn flow_ret;
  gboolean alloc_finalized = FALSE;

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 320, 240);

  alloc = gst_cuda_pool_allocator_new (context, NULL, &info);
  fail_unless (alloc);

  g_object_set_qdata_full (G_OBJECT (alloc), memory_tester_quark,
      &alloc_finalized, (GDestroyNotify) allocator_finalize_cb);

  /* inactive pool should return flusing */
  flow_ret = gst_cuda_pool_allocator_acquire_memory (alloc, &mem);
  fail_unless (flow_ret == GST_FLOW_FLUSHING);
  fail_if (mem);

  ret = gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (alloc), TRUE);
  fail_unless (ret);

  flow_ret = gst_cuda_pool_allocator_acquire_memory (alloc, &mem);
  fail_unless (flow_ret == GST_FLOW_OK);
  fail_unless (mem);

  gst_object_unref (alloc);
  /* Only memory should hold refcount at this moment */
  fail_unless (G_OBJECT (alloc)->ref_count == 1);
  fail_if (alloc_finalized);

  /* allocator should be finalized as well */
  gst_memory_unref (mem);
  fail_unless (alloc_finalized);
}

GST_END_TEST;

GST_START_TEST (test_free_buffer_after_deactivate)
{
  GstVideoInfo info;
  gboolean ret;
  GstFlowReturn flow_ret;
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  GstBuffer *buffers[2];
  gboolean alloc_finalized = FALSE;

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 320, 240);

  caps = gst_video_info_to_caps (&info);
  fail_unless (caps);

  pool = gst_cuda_buffer_pool_new (context);
  fail_unless (pool);

  g_object_set_qdata_full (G_OBJECT (pool), memory_tester_quark,
      &alloc_finalized, (GDestroyNotify) allocator_finalize_cb);

  config = gst_buffer_pool_get_config (pool);
  fail_unless (config);

  gst_buffer_pool_config_set_params (config, caps, info.size, 0, 0);
  gst_caps_unref (caps);

  ret = gst_buffer_pool_set_config (pool, config);
  fail_unless (ret);

  ret = gst_buffer_pool_set_active (pool, TRUE);
  fail_unless (ret);

  flow_ret = gst_buffer_pool_acquire_buffer (pool, &buffers[0], NULL);
  fail_unless (flow_ret == GST_FLOW_OK);

  flow_ret = gst_buffer_pool_acquire_buffer (pool, &buffers[1], NULL);
  fail_unless (flow_ret == GST_FLOW_OK);

  ret = gst_buffer_pool_set_active (pool, FALSE);
  fail_unless (ret);
  fail_if (alloc_finalized);

  gst_object_unref (pool);
  fail_if (alloc_finalized);

  gst_buffer_unref (buffers[0]);
  fail_if (alloc_finalized);

  gst_buffer_unref (buffers[1]);
  fail_unless (alloc_finalized);
}

GST_END_TEST;

static gboolean
check_cuda_device (void)
{
  GstCudaContext *context;

  if (!gst_cuda_load_library ())
    return FALSE;

  context = gst_cuda_context_new (0);
  if (context) {
    gst_object_unref (context);
    return TRUE;
  }

  return FALSE;
}

static Suite *
cudamemory_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  /* CUDA doesn't work well with fork  */
  g_setenv ("CK_FORK", "no", TRUE);

  s = suite_create ("cudamemory");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup_func, teardown_func);

  if (!check_cuda_device ())
    return s;

  tcase_add_test (tc_chain, test_free_active_allocator);
  tcase_add_test (tc_chain, test_free_buffer_after_deactivate);

  return s;
}

GST_CHECK_MAIN (cudamemory);
