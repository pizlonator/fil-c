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
#include <gst/d3d11/gstd3d11.h>
#include <gst/video/video.h>
#include <string.h>

static GstD3D11Device *device = NULL;
static GQuark memory_tester_quark;

static void
setup_func (void)
{
  device = gst_d3d11_device_new (0, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
  fail_unless (GST_IS_D3D11_DEVICE (device));

  memory_tester_quark = g_quark_from_static_string ("gst-d3d11-memory-tester");
}

static void
teardown_func (void)
{
  gst_object_unref (device);
}

static void
allocator_finalize_cb (gboolean * alloc_finalized)
{
  *alloc_finalized = TRUE;
}

GST_START_TEST (test_free_active_allocator)
{
  GstD3D11PoolAllocator *alloc;
  GstMemory *mem = NULL;
  gboolean ret;
  GstFlowReturn flow_ret;
  gboolean alloc_finalized = FALSE;
  D3D11_TEXTURE2D_DESC desc;

  memset (&desc, 0, sizeof (desc));

  desc.Width = 16;
  desc.Height = 16;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;

  alloc = gst_d3d11_pool_allocator_new (device, &desc);
  fail_unless (alloc);

  g_object_set_qdata_full (G_OBJECT (alloc), memory_tester_quark,
      &alloc_finalized, (GDestroyNotify) allocator_finalize_cb);

  /* inactive pool should return flusing */
  flow_ret = gst_d3d11_pool_allocator_acquire_memory (alloc, &mem);
  fail_unless (flow_ret == GST_FLOW_FLUSHING);
  fail_if (mem);

  ret = gst_d3d11_allocator_set_active (GST_D3D11_ALLOCATOR (alloc), TRUE);
  fail_unless (ret);

  flow_ret = gst_d3d11_pool_allocator_acquire_memory (alloc, &mem);
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

  pool = gst_d3d11_buffer_pool_new (device);
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

typedef struct
{
  GMutex lock;
  GCond cond;
  gboolean blocked;
  GstBufferPool *pool;
} UnblockTestData;

static gpointer
alloc_thread (UnblockTestData * data)
{
  GstBuffer *buffers[2];
  GstBuffer *flush_buf = NULL;
  GstFlowReturn ret;

  g_mutex_lock (&data->lock);
  ret = gst_buffer_pool_acquire_buffer (data->pool, &buffers[0], NULL);
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_buffer_pool_acquire_buffer (data->pool, &buffers[1], NULL);
  fail_unless (ret == GST_FLOW_OK);

  /* below call will be blocked by buffer pool */
  data->blocked = TRUE;
  g_cond_signal (&data->cond);
  g_mutex_unlock (&data->lock);

  ret = gst_buffer_pool_acquire_buffer (data->pool, &flush_buf, NULL);
  fail_unless (ret == GST_FLOW_FLUSHING);

  gst_buffer_unref (buffers[0]);
  gst_buffer_unref (buffers[1]);

  return NULL;
}

GST_START_TEST (test_unblock_on_stop)
{
  GstStructure *config;
  GstVideoInfo info;
  GstCaps *caps;
  GstD3D11AllocationParams *params;
  UnblockTestData data;
  GThread *thread;

  data.blocked = FALSE;
  g_mutex_init (&data.lock);
  g_cond_init (&data.cond);

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 16, 16);
  caps = gst_video_info_to_caps (&info);
  fail_unless (caps);

  data.pool = gst_d3d11_buffer_pool_new (device);
  fail_unless (data.pool);

  config = gst_buffer_pool_get_config (data.pool);
  fail_unless (config);

  params = gst_d3d11_allocation_params_new (device,
      &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);
  fail_unless (params);

  gst_d3d11_allocation_params_set_array_size (params, 2);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, caps, info.size, 0, 2);
  gst_caps_unref (caps);

  fail_unless (gst_buffer_pool_set_config (data.pool, config));
  fail_unless (gst_buffer_pool_set_active (data.pool, TRUE));

  thread = g_thread_new (NULL, (GThreadFunc) alloc_thread, &data);

  g_mutex_lock (&data.lock);
  while (!data.blocked)
    g_cond_wait (&data.cond, &data.lock);
  g_mutex_unlock (&data.lock);

  /* Wait 1 second for the alloc thread to be actually blocked */
  Sleep (1000);

  fail_unless (gst_buffer_pool_set_active (data.pool, FALSE));
  g_thread_join (thread);

  gst_object_unref (data.pool);
  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);
}

GST_END_TEST;

static gboolean
check_d3d11_device (void)
{
  GstD3D11Device *device;

  device = gst_d3d11_device_new (0, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
  if (device) {
    gst_object_unref (device);
    return TRUE;
  }

  return FALSE;
}

static Suite *
d3d11memory_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  s = suite_create ("d3d11memory");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup_func, teardown_func);

  if (!check_d3d11_device ())
    return s;

  tcase_add_test (tc_chain, test_free_active_allocator);
  tcase_add_test (tc_chain, test_free_buffer_after_deactivate);
  tcase_add_test (tc_chain, test_unblock_on_stop);

  return s;
}

GST_CHECK_MAIN (d3d11memory);
