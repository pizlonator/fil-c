/* GStreamer unit test for the viewfinderbin element
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
# include "config.h"
#endif

#include <stdio.h>
#include <gst/check/gstcheck.h>

typedef struct
{
  GstElement *pipe;
  GstElement *src;
  GstElement *vfbin;
} GstViewFinderBinTestContext;

static void
gstviewfinderbin_init_test_context (GstViewFinderBinTestContext * ctx,
    gint num_buffers)
{
  GstElement *sink;
  fail_unless (ctx != NULL);

  ctx->pipe = gst_pipeline_new ("pipeline");
  fail_unless (ctx->pipe != NULL);
  ctx->src = gst_element_factory_make ("videotestsrc", "src");
  fail_unless (ctx->src != NULL, "Failed to create videotestsrc element");
  sink = gst_element_factory_make ("fakesink", NULL);
  ctx->vfbin = gst_element_factory_make ("viewfinderbin", "vfbin");
  fail_unless (ctx->vfbin != NULL, "Failed to create viewfinderbin element");
  g_object_set (ctx->vfbin, "video-sink", sink, NULL);
  gst_object_unref (sink);

  if (num_buffers > 0)
    g_object_set (ctx->src, "num-buffers", num_buffers, NULL);

  fail_unless (gst_bin_add (GST_BIN (ctx->pipe), ctx->src));
  fail_unless (gst_bin_add (GST_BIN (ctx->pipe), ctx->vfbin));
  fail_unless (gst_element_link (ctx->src, ctx->vfbin));
}

static void
gstviewfinderbin_unset_test_context (GstViewFinderBinTestContext * ctx)
{
  gst_element_set_state (ctx->pipe, GST_STATE_NULL);
  gst_object_unref (ctx->pipe);
  memset (ctx, 0, sizeof (GstViewFinderBinTestContext));
}

GST_START_TEST (test_simple_run)
{
  GstViewFinderBinTestContext ctx;
  GstBus *bus;
  GstMessage *msg;

  gstviewfinderbin_init_test_context (&ctx, 10);
  bus = gst_element_get_bus (ctx.pipe);

  fail_if (gst_element_set_state (ctx.pipe, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE);

  msg = gst_bus_timed_pop_filtered (bus, GST_SECOND * 30,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  gstviewfinderbin_unset_test_context (&ctx);
  gst_object_unref (bus);
}

GST_END_TEST;

static Suite *
viewfinderbin_suite (void)
{
  Suite *s = suite_create ("viewfinderbin");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_simple_run);

  return s;
}

GST_CHECK_MAIN (viewfinderbin);
