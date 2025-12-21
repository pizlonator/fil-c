/* GStreamer
 *
 * Copyright (C) 2014 Matthew Waters <matthew@centricular.com>
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
#include <gst/gl/gstglfuncs.h>

#include <stdio.h>

static GstGLDisplay *display;
static GstGLContext *context;
static GstGLWindow *window;
static GstGLColorConvert *convert;

typedef struct _TestFrame
{
  gint width;
  gint height;
  GstVideoFormat v_format;
  gchar *data[GST_VIDEO_MAX_PLANES];
} TestFrame;

#define IGNORE_MAGIC 0x05

static const gchar rgba_reorder_data[] = { 0x49, 0x24, 0x72, 0xff };
static const gchar rgbx_reorder_data[] = { 0x49, 0x24, 0x72, IGNORE_MAGIC };
static const gchar argb_reorder_data[] = { 0xff, 0x49, 0x24, 0x72 };
static const gchar xrgb_reorder_data[] = { IGNORE_MAGIC, 0x49, 0x24, 0x72 };
static const gchar rgb_reorder_data[] = { 0x49, 0x24, 0x72, IGNORE_MAGIC };
static const gchar bgr_reorder_data[] = { 0x72, 0x24, 0x49, IGNORE_MAGIC };
static const gchar bgra_reorder_data[] = { 0x72, 0x24, 0x49, 0xff };
static const gchar bgrx_reorder_data[] = { 0x72, 0x24, 0x49, IGNORE_MAGIC };
static const gchar abgr_reorder_data[] = { 0xff, 0x72, 0x24, 0x49 };
static const gchar xbgr_reorder_data[] = { IGNORE_MAGIC, 0x72, 0x24, 0x49 };

static TestFrame test_rgba_reorder[] = {
  {1, 1, GST_VIDEO_FORMAT_RGBA, {(gchar *) & rgba_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_RGBx, {(gchar *) & rgbx_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_ARGB, {(gchar *) & argb_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_xRGB, {(gchar *) & xrgb_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGRA, {(gchar *) & bgra_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGRx, {(gchar *) & bgrx_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_ABGR, {(gchar *) & abgr_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_xBGR, {(gchar *) & xbgr_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_RGB, {(gchar *) & rgb_reorder_data}},
  {1, 1, GST_VIDEO_FORMAT_BGR, {(gchar *) & bgr_reorder_data}},
};

#ifndef GST_CAPS_FEATURE_MEMORY_DMABUF
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"
#endif

static GstVideoFormat test_passthrough_formats[] = {
  GST_VIDEO_FORMAT_DMA_DRM,
};

static const gchar *test_passthrough_features[] = {
  GST_CAPS_FEATURE_MEMORY_DMABUF,
};

static void
setup (void)
{
  GError *error = NULL;

  display = gst_gl_display_new ();
  context = gst_gl_context_new (display);

  gst_gl_context_create (context, 0, &error);
  window = gst_gl_context_get_window (context);

  fail_if (error != NULL, "Error creating context: %s\n",
      error ? error->message : "Unknown Error");

  convert = gst_gl_color_convert_new (context);
}

static void
_check_gl_error (GstGLContext * context, gpointer data)
{
  GLuint error = context->gl_vtable->GetError ();
  fail_if (error != GL_NONE, "GL error 0x%x encountered during processing\n",
      error);
}

static void
teardown (void)
{
  gst_object_unref (convert);
  gst_object_unref (window);

  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _check_gl_error,
      NULL);
  gst_object_unref (context);
  gst_object_unref (display);
}

static gsize
_video_info_plane_size (GstVideoInfo * info, guint plane)
{
  if (GST_VIDEO_INFO_N_PLANES (info) == plane + 1)
    return info->offset[0] + info->size - info->offset[plane];

  return info->offset[plane + 1] - info->offset[plane];
}

static void
_frame_unref (gpointer user_data)
{
  gint *ref_count = user_data;
  g_atomic_int_add (ref_count, -1);
}

static void
check_conversion (TestFrame * frames, guint size)
{
  GstGLBaseMemoryAllocator *base_mem_alloc;
  gint i, j, k, l;
  gint ref_count = 0;

  base_mem_alloc =
      GST_GL_BASE_MEMORY_ALLOCATOR (gst_allocator_find
      (GST_GL_MEMORY_ALLOCATOR_NAME));

  for (i = 0; i < size; i++) {
    GstBuffer *inbuf;
    GstVideoInfo in_info;
    gint in_width = frames[i].width;
    gint in_height = frames[i].height;
    GstVideoFormat in_v_format = frames[i].v_format;
    GstVideoFrame in_frame;
    GstCaps *in_caps;

    gst_video_info_set_format (&in_info, in_v_format, in_width, in_height);
    in_caps = gst_video_info_to_caps (&in_info);
    gst_caps_set_features (in_caps, 0,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));

    /* create GL buffer */
    inbuf = gst_buffer_new ();
    for (j = 0; j < GST_VIDEO_INFO_N_PLANES (&in_info); j++) {
      GstGLFormat tex_format = gst_gl_format_from_video_info (context,
          &in_info, j);
      GstGLVideoAllocationParams *params;
      GstGLBaseMemory *mem;

      ref_count++;
      params = gst_gl_video_allocation_params_new_wrapped_data (context, NULL,
          &in_info, j, NULL, GST_GL_TEXTURE_TARGET_2D, tex_format,
          frames[i].data[j], &ref_count, _frame_unref);

      mem = gst_gl_base_memory_alloc (base_mem_alloc,
          (GstGLAllocationParams *) params);
      gst_buffer_append_memory (inbuf, GST_MEMORY_CAST (mem));

      gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
    }

    fail_unless (gst_video_frame_map (&in_frame, &in_info, inbuf,
            GST_MAP_READ));

    /* sanity check that the correct values were wrapped */
    for (j = 0; j < GST_VIDEO_INFO_N_PLANES (&in_info); j++) {
      int plane_size = _video_info_plane_size (&in_info, j);
      for (k = 0; k < plane_size; k++) {
        if (frames[i].data[j][k] != IGNORE_MAGIC)
          fail_unless (((gchar *) in_frame.data[j])[k] == frames[i].data[j][k]);
      }
    }

    for (j = 0; j < size; j++) {
      GstBuffer *outbuf;
      GstVideoInfo out_info;
      gint out_width = frames[j].width;
      gint out_height = frames[j].height;
      GstVideoFormat out_v_format = frames[j].v_format;
      gchar *out_data[GST_VIDEO_MAX_PLANES] = { 0 };
      GstVideoFrame out_frame;
      GstCaps *out_caps;

      gst_video_info_set_format (&out_info, out_v_format, out_width,
          out_height);
      out_caps = gst_video_info_to_caps (&out_info);
      gst_caps_set_features (out_caps, 0,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));

      for (k = 0; k < GST_VIDEO_INFO_N_PLANES (&out_info); k++) {
        out_data[k] = frames[j].data[k];
      }

      gst_gl_color_convert_set_caps (convert, in_caps, out_caps);

      /* convert the data */
      outbuf = gst_gl_color_convert_perform (convert, inbuf);
      if (outbuf == NULL) {
        const gchar *in_str = gst_video_format_to_string (in_v_format);
        const gchar *out_str = gst_video_format_to_string (out_v_format);
        GST_WARNING ("failed to convert from %s to %s", in_str, out_str);
      }

      fail_unless (gst_video_frame_map (&out_frame, &out_info, outbuf,
              GST_MAP_READ));

      /* check that the converted values are correct */
      for (k = 0; k < GST_VIDEO_INFO_N_PLANES (&out_info); k++) {
        int plane_size = _video_info_plane_size (&out_info, k);
        GST_MEMDUMP ("expected plane", (guint8 *) out_data[k], plane_size);
        GST_MEMDUMP ("produced plane", out_frame.data[k], plane_size);
        for (l = 0; l < plane_size; l++) {
          gchar out_pixel = ((gchar *) out_frame.data[k])[l];
          if (out_data[k][l] != IGNORE_MAGIC && out_pixel != IGNORE_MAGIC)
            fail_unless (out_pixel == out_data[k][l]);
          /* FIXME: check alpha clobbering */
        }
      }

      gst_caps_unref (out_caps);
      gst_video_frame_unmap (&out_frame);
      gst_buffer_unref (outbuf);
    }

    gst_caps_unref (in_caps);
    gst_video_frame_unmap (&in_frame);
    gst_buffer_unref (inbuf);

    fail_unless_equals_int (ref_count, 0);
  }

  gst_object_unref (base_mem_alloc);
}

GST_START_TEST (test_reorder_buffer)
{
  guint size = G_N_ELEMENTS (test_rgba_reorder);

  /* gles can't download rgb24 textures */
  if (gst_gl_context_get_gl_api (context) & GST_GL_API_GLES2)
    size -= 2;

  check_conversion (test_rgba_reorder, size);
}

GST_END_TEST;

GST_START_TEST (test_passthrough)
{
  guint formats_size = G_N_ELEMENTS (test_passthrough_formats);
  guint features_size = G_N_ELEMENTS (test_passthrough_features);
  gint i, j, k, l;

  for (i = 0; i < formats_size; i++) {
    GstVideoFormat in_format = test_passthrough_formats[i];

    for (j = 0; j < formats_size; j++) {
      GstVideoFormat out_format = test_passthrough_formats[j];

      for (k = 0; k < features_size; k++) {
        const gchar *in_feature = test_passthrough_features[k];
        GstCaps *in_caps;

        in_caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING,
            gst_video_format_to_string (in_format), NULL);
        gst_caps_set_features_simple (in_caps,
            gst_caps_features_from_string (in_feature));


        for (l = 0; l < features_size; l++) {
          const gchar *out_feature = test_passthrough_features[l];
          GstCaps *out_caps;

          out_caps = gst_caps_new_simple ("video/x-raw", "format",
              G_TYPE_STRING, gst_video_format_to_string (out_format), NULL);
          gst_caps_set_features_simple (out_caps,
              gst_caps_features_from_string (out_feature));

          if (gst_caps_is_equal (in_caps, out_caps)) {
            GstCaps *tmp_caps, *tmp_caps2, *tmp_caps3;

            tmp_caps = gst_gl_color_convert_transform_caps (context,
                GST_PAD_SINK, in_caps, NULL);
            tmp_caps2 = gst_gl_color_convert_transform_caps (context,
                GST_PAD_SRC, out_caps, NULL);

            tmp_caps3 = gst_caps_intersect (tmp_caps, tmp_caps2);

            fail_unless (!gst_caps_is_empty (tmp_caps3));

            gst_caps_unref (tmp_caps);
            gst_caps_unref (tmp_caps2);
            gst_caps_unref (tmp_caps3);
          }

          gst_caps_unref (out_caps);
        }

        gst_caps_unref (in_caps);
      }
    }
  }
}

GST_END_TEST;

static Suite *
gst_gl_color_convert_suite (void)
{
  Suite *s = suite_create ("GstGLColorConvert");
  TCase *tc_chain = tcase_create ("upload");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_reorder_buffer);
  tcase_add_test (tc_chain, test_passthrough);
  /* FIXME add YUV <--> RGB conversion tests */

  return s;
}

GST_CHECK_MAIN (gst_gl_color_convert);
