/* GStreamer
 *
 * unit test for state changes on all elements
 *
 * Copyright (C) <2012> Matthew Waters <ystreet00@gmail.com>
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

#include <gst/check/gstcheck.h>

#include <gst/gl/gl.h>

#include <stdio.h>

static GstGLDisplay *display;
static GstGLContext *context;

static void
setup (void)
{
  display = gst_gl_display_new ();
  context = gst_gl_context_new (display);
  gst_gl_context_create (context, 0, NULL);
  gst_gl_memory_init_once ();
}

static void
teardown (void)
{
  gst_object_unref (display);
  gst_object_unref (context);
}

/* one red rgba pixel */
static guint8 rgba_pixel[] = {
  0xff, 0x00, 0x00, 0xff,
};

static const struct
{
  GstVideoFormat format;
  guint width;
  guint height;
  guint plane;
  guint8 *data;
  gsize size;
} formats[] = {
  {
      GST_VIDEO_FORMAT_RGBA, 1, 1, 0, rgba_pixel, 4}, {
      GST_VIDEO_FORMAT_RGB, 1, 1, 0, rgba_pixel, 3}, {
      GST_VIDEO_FORMAT_YUY2, 1, 1, 0, rgba_pixel, 1}, {
      GST_VIDEO_FORMAT_I420, 1, 1, 0, rgba_pixel, 1},
};

GST_START_TEST (test_allocator_alloc)
{
  GstAllocator *gl_allocator;
  GstMemory *mem;

  gl_allocator = gst_allocator_find (GST_GL_MEMORY_ALLOCATOR_NAME);

  ASSERT_WARNING (mem = gst_allocator_alloc (gl_allocator, 0, NULL));
  fail_unless (mem == NULL);

  gst_object_unref (gl_allocator);
}

GST_END_TEST;

GST_START_TEST (test_allocator_pbo_alloc)
{
  GstAllocator *gl_allocator;
  GstMemory *mem;

  gl_allocator = gst_allocator_find (GST_GL_MEMORY_PBO_ALLOCATOR_NAME);

  ASSERT_WARNING (mem = gst_allocator_alloc (gl_allocator, 0, NULL));
  fail_unless (mem == NULL);

  gst_object_unref (gl_allocator);
}

GST_END_TEST;

static GstMemory *
create_memory (const gchar * allocator_name, const GstVideoInfo * v_info,
    guint plane)
{
  GstAllocator *gl_allocator;
  GstGLBaseMemoryAllocator *base_mem_alloc;
  GstGLVideoAllocationParams *params;
  GstGLMemory *gl_mem;
  GstMemory *mem;

  GST_DEBUG ("creating from %s texture for format %s, %ux%u plane %u",
      allocator_name, GST_VIDEO_INFO_NAME (v_info),
      GST_VIDEO_INFO_WIDTH (v_info), GST_VIDEO_INFO_HEIGHT (v_info), plane);

  gl_allocator = gst_allocator_find (allocator_name);
  fail_if (gl_allocator == NULL);
  base_mem_alloc = GST_GL_BASE_MEMORY_ALLOCATOR (gl_allocator);

  params = gst_gl_video_allocation_params_new (context, NULL, v_info, plane,
      NULL, GST_GL_TEXTURE_TARGET_2D, GST_GL_RGBA);

  /* texture creation */
  mem = (GstMemory *) gst_gl_base_memory_alloc (base_mem_alloc,
      (GstGLAllocationParams *) params);
  gl_mem = (GstGLMemory *) mem;
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  fail_unless_equals_int (TRUE, gst_video_info_is_equal (v_info,
          &gl_mem->info));
  fail_unless_equals_int (plane, gl_mem->plane);
  fail_if (gl_mem->mem.context != context);
  fail_if (gl_mem->tex_id == 0);

  gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
  gst_object_unref (gl_allocator);

  return mem;
}

GST_START_TEST (test_allocator_create)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    GstVideoInfo v_info;
    GstMemory *mem;

    gst_video_info_set_format (&v_info, formats[i].format, formats[i].width,
        formats[i].height);
    mem = create_memory (GST_GL_MEMORY_ALLOCATOR_NAME, &v_info,
        formats[i].plane);
    gst_memory_unref (mem);
    mem = create_memory (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane);
    gst_memory_unref (mem);
  }
}

GST_END_TEST;

GST_START_TEST (test_memory_copy)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    GstVideoInfo v_info;
    GstMemory *mem, *mem2;
    GstGLMemory *gl_mem, *gl_mem2;

    gst_video_info_set_format (&v_info, formats[i].format, formats[i].width,
        formats[i].height);
    mem = create_memory (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane);
    gl_mem = (GstGLMemory *) mem;
    mem2 = gst_memory_copy (mem, 0, -1);
    gl_mem2 = (GstGLMemory *) mem;

    fail_unless (gl_mem->mem.context == context);
    fail_unless_equals_int (gl_mem->tex_id, gl_mem2->tex_id);
    fail_unless_equals_int (gl_mem->tex_target, gl_mem2->tex_target);
    fail_unless_equals_int (gl_mem->tex_format, gl_mem2->tex_format);
    fail_unless_equals_int (TRUE, gst_video_info_is_equal (&gl_mem2->info,
            &gl_mem->info));
    fail_unless_equals_int (gl_mem->plane, gl_mem2->plane);

    gst_memory_unref (mem);
    gst_memory_unref (mem2);
  }
}

GST_END_TEST;

static GstMemory *
wrap_raw_data (const gchar * allocator_name, const GstVideoInfo * v_info,
    guint plane, guint8 * data)
{
  GstAllocator *gl_allocator;
  GstGLBaseMemoryAllocator *base_mem_alloc;
  GstGLVideoAllocationParams *params;
  GstMemory *mem;
  GstGLMemory *gl_mem;
  GstGLFormat gl_format;

  GST_DEBUG ("wrapping from %s data pointer %p for format %s, %ux%u plane %u",
      allocator_name, data, GST_VIDEO_INFO_NAME (v_info),
      GST_VIDEO_INFO_WIDTH (v_info), GST_VIDEO_INFO_HEIGHT (v_info), plane);

  gl_allocator = gst_allocator_find (allocator_name);
  fail_if (gl_allocator == NULL);
  base_mem_alloc = GST_GL_BASE_MEMORY_ALLOCATOR (gl_allocator);

  gl_format = gst_gl_format_from_video_info (context, v_info, plane);
  params = gst_gl_video_allocation_params_new_wrapped_data (context, NULL,
      v_info, plane, NULL, GST_GL_TEXTURE_TARGET_2D,
      gl_format, data, NULL, NULL);
  mem =
      (GstMemory *) gst_gl_base_memory_alloc (base_mem_alloc,
      (GstGLAllocationParams *) params);
  gl_mem = (GstGLMemory *) mem;
  fail_if (mem == NULL);

  fail_unless (GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  fail_unless_equals_int (TRUE, gst_video_info_is_equal (v_info,
          &gl_mem->info));
  fail_unless_equals_int (gl_mem->plane, plane);

  gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
  gst_object_unref (gl_allocator);

  return mem;
}

GST_START_TEST (test_wrap_raw)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    GstVideoInfo v_info;
    GstMemory *mem;
    GstGLMemory *gl_mem;
    GstMapInfo map_info;

    gst_video_info_set_format (&v_info, formats[i].format, formats[i].width,
        formats[i].height);
    mem = wrap_raw_data (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane, formats[i].data);
    gl_mem = (GstGLMemory *) mem;

    fail_unless (gl_mem->mem.context == context);

    fail_unless (gst_memory_map (mem, &map_info, GST_MAP_READ));
    fail_unless (memcmp (map_info.data, formats[i].data, formats[i].size) == 0);
    gst_memory_unmap (mem, &map_info);

    gst_memory_unref (mem);
  }
}

GST_END_TEST;

static GstMemory *
wrap_gl_memory (GstGLMemory * gl_mem)
{
  GstGLBaseMemoryAllocator *base_mem_alloc;
  GstGLVideoAllocationParams *params;
  GstMemory *mem, *mem2;
  GstGLMemory *gl_mem2;

  mem = (GstMemory *) gl_mem;
  base_mem_alloc = GST_GL_BASE_MEMORY_ALLOCATOR (mem->allocator);

  GST_DEBUG ("wrapping from %s %" GST_PTR_FORMAT " for format %s, %ux%u "
      "plane %u", mem->allocator->mem_type, gl_mem,
      GST_VIDEO_INFO_NAME (&gl_mem->info), GST_VIDEO_INFO_WIDTH (&gl_mem->info),
      GST_VIDEO_INFO_HEIGHT (&gl_mem->info), gl_mem->plane);

  params = gst_gl_video_allocation_params_new_wrapped_texture (context, NULL,
      &gl_mem->info, gl_mem->plane, NULL, gl_mem->tex_target,
      gl_mem->tex_format, gl_mem->tex_id, NULL, NULL);
  mem2 =
      (GstMemory *) gst_gl_base_memory_alloc (base_mem_alloc,
      (GstGLAllocationParams *) params);
  gl_mem2 = (GstGLMemory *) mem2;
  fail_if (mem == NULL);

  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem2,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (GST_MEMORY_FLAG_IS_SET (mem2,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  fail_unless (gl_mem->mem.context == context);
  fail_unless_equals_int (gl_mem->tex_id, gl_mem2->tex_id);
  fail_unless_equals_int (gl_mem->tex_target, gl_mem2->tex_target);
  fail_unless_equals_int (gl_mem->tex_format, gl_mem2->tex_format);
  fail_unless_equals_int (TRUE, gst_video_info_is_equal (&gl_mem2->info,
          &gl_mem->info));
  fail_unless_equals_int (gl_mem->plane, gl_mem2->plane);

  gst_gl_allocation_params_free ((GstGLAllocationParams *) params);

  return mem2;
}

GST_START_TEST (test_wrap_gl_memory)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    GstVideoInfo v_info;
    GstMemory *mem, *mem2;
    GstGLMemory *gl_mem;

    gst_video_info_set_format (&v_info, formats[i].format, formats[i].width,
        formats[i].height);
    mem = create_memory (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane);
    gl_mem = (GstGLMemory *) mem;
    mem2 = wrap_gl_memory (gl_mem);

    gst_memory_unref (mem);
    gst_memory_unref (mem2);
  }
}

GST_END_TEST;

GST_START_TEST (test_wrap_data_copy_into)
{
  int i;

  /* FIXME: in GLES2 only supported with RGBA */
  for (i = 0; i < 1 /*G_N_ELEMENTS (formats) */ ; i++) {
    GstVideoInfo v_info;
    GstMemory *mem, *mem2;
    GstGLMemory *gl_mem, *gl_mem2;
    GstMapInfo map_info;

    gst_video_info_set_format (&v_info, formats[i].format, formats[i].width,
        formats[i].height);
    /* wrap some data */
    mem = wrap_raw_data (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane, formats[i].data);
    gl_mem = (GstGLMemory *) mem;
    mem2 = create_memory (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info,
        formats[i].plane);
    gl_mem2 = (GstGLMemory *) mem2;

    fail_unless (gst_memory_map (mem, &map_info, GST_MAP_READ | GST_MAP_GL));

    /* copy wrapped data into another texture */
    fail_unless (gst_gl_memory_copy_into (gl_mem,
            gl_mem2->tex_id, GST_GL_TEXTURE_TARGET_2D, gl_mem2->tex_format,
            formats[i].width, formats[i].height));
    GST_MINI_OBJECT_FLAG_SET (mem2, GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);

    fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
            GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
    fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
            GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));
    fail_unless (!GST_MEMORY_FLAG_IS_SET (mem2,
            GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
    fail_unless (GST_MEMORY_FLAG_IS_SET (mem2,
            GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

    gst_memory_unmap (mem, &map_info);

    /* check copied texture is the same as the wrapped data */
    fail_unless (gst_memory_map (mem2, &map_info, GST_MAP_READ));
    fail_unless (memcmp (map_info.data, formats[i].data, formats[i].size) == 0);
    gst_memory_unmap (mem2, &map_info);

    gst_memory_unref (mem);
    gst_memory_unref (mem2);
  }
}

GST_END_TEST;

GST_START_TEST (test_transfer_state)
{
  GstVideoInfo v_info;
  GstMapInfo map_info;
  GstMemory *mem;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 1, 1);
  mem = create_memory (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info, 0);

  /* initial state is no transfer needed */
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  GST_DEBUG ("read-only map");
  gst_memory_map (mem, &map_info, GST_MAP_READ);
  gst_memory_unmap (mem, &map_info);
  /* read map does not change transfer state */
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  GST_DEBUG ("read/GL-only map");
  gst_memory_map (mem, &map_info, GST_MAP_READ | GST_MAP_GL);
  gst_memory_unmap (mem, &map_info);
  /* read | GL map does not change transfer state */
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  GST_DEBUG ("write-only map");
  gst_memory_map (mem, &map_info, GST_MAP_WRITE);
  gst_memory_unmap (mem, &map_info);
  /* write map causes need-upload */
  fail_unless (GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  GST_DEBUG ("write/GL-only map");
  gst_memory_map (mem, &map_info, GST_MAP_WRITE | GST_MAP_GL);
  gst_memory_unmap (mem, &map_info);
  /* write | GL map from need-upload causes only need-download */
  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD));
  fail_unless (GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_separate_upload_transfer)
{
  GstVideoInfo v_info;
  GstMemory *mem;
  GstMapInfo info;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 1, 1);
  mem =
      wrap_raw_data (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info, 0, rgba_pixel);
  gst_gl_memory_pbo_upload_transfer ((GstGLMemoryPBO *) mem);

  fail_unless (!GST_MEMORY_FLAG_IS_SET (mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD));

  /* complete the upload */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ | GST_MAP_GL));
  gst_memory_unmap (mem, &info);

  /* force a download */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_WRITE | GST_MAP_GL));
  gst_memory_unmap (mem, &info);

  /* check the downloaded data is the same */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (memcmp (info.data, rgba_pixel, G_N_ELEMENTS (rgba_pixel)) == 0,
      "0x%02x%02x%02x%02x != 0x%02x%02x%02x%02x", (guint8) info.data[0],
      (guint8) info.data[1], (guint8) info.data[2],
      (guint8) info.data[3], (guint8) rgba_pixel[0], (guint8) rgba_pixel[1],
      (guint8) rgba_pixel[2], (guint8) rgba_pixel[3]);
  gst_memory_unmap (mem, &info);

  gst_memory_unref (mem);
}

GST_END_TEST;

GST_START_TEST (test_separate_download_transfer)
{
  GstVideoInfo v_info;
  GstMemory *mem;
  GstMapInfo info;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 1, 1);
  mem =
      wrap_raw_data (GST_GL_MEMORY_PBO_ALLOCATOR_NAME, &v_info, 0, rgba_pixel);

  /* complete the upload */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ | GST_MAP_GL));
  gst_memory_unmap (mem, &info);

  /* force a download */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_WRITE | GST_MAP_GL));
  gst_memory_unmap (mem, &info);

  gst_gl_memory_pbo_download_transfer ((GstGLMemoryPBO *) mem);

  /* check the downloaded data is the same */
  fail_unless (gst_memory_map (mem, &info, GST_MAP_READ));
  fail_unless (memcmp (info.data, rgba_pixel, G_N_ELEMENTS (rgba_pixel)) == 0,
      "0x%02x%02x%02x%02x != 0x%02x%02x%02x%02x", (guint8) info.data[0],
      (guint8) info.data[1], (guint8) info.data[2],
      (guint8) info.data[3], (guint8) rgba_pixel[0], (guint8) rgba_pixel[1],
      (guint8) rgba_pixel[2], (guint8) rgba_pixel[3]);
  gst_memory_unmap (mem, &info);

  gst_memory_unref (mem);
}

GST_END_TEST;

static Suite *
gst_gl_memory_suite (void)
{
  Suite *s = suite_create ("GstGLMemory");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_allocator_alloc);
  tcase_add_test (tc_chain, test_allocator_pbo_alloc);
  tcase_add_test (tc_chain, test_allocator_create);
  tcase_add_test (tc_chain, test_memory_copy);
  tcase_add_test (tc_chain, test_wrap_raw);
  tcase_add_test (tc_chain, test_wrap_gl_memory);
  tcase_add_test (tc_chain, test_wrap_data_copy_into);
  tcase_add_test (tc_chain, test_transfer_state);
  tcase_add_test (tc_chain, test_separate_upload_transfer);
  tcase_add_test (tc_chain, test_separate_download_transfer);

  return s;
}

GST_CHECK_MAIN (gst_gl_memory);
