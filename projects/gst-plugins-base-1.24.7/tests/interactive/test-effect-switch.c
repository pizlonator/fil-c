/* GStreamer dynamic effect change test app
 * Copyright (C) 2012 Tim-Philipp Müller <tim centricular net>

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
 *
 * Based on python script by Thiago Sousa Santos
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

static gchar *opt_effects = NULL;

#define DEFAULT_EFFECTS "identity,exclusion,navigationtest," \
    "agingtv,videoflip,vertigotv,gaussianblur,shagadelictv,edgetv"

static GstPad *blockpad;
static GstElement *conv_before;
static GstElement *conv_after;
static GstElement *cur_effect;
static GstElement *pipeline;

static GQueue effects = G_QUEUE_INIT;

static GstPadProbeReturn
event_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GMainLoop *loop = user_data;
  GstElement *next;

  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) != GST_EVENT_EOS)
    return GST_PAD_PROBE_OK;

  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  /* take next effect from the queue */
  next = g_queue_pop_head (&effects);
  if (next == NULL) {
    GST_DEBUG_OBJECT (pad, "no more effects");
    g_main_loop_quit (loop);
    return GST_PAD_PROBE_DROP;
  }

  g_print ("Switching from '%s' to '%s'..\n", GST_OBJECT_NAME (cur_effect),
      GST_OBJECT_NAME (next));

  gst_element_set_state (cur_effect, GST_STATE_NULL);

  /* remove unlinks automatically */
  GST_DEBUG_OBJECT (pipeline, "removing %" GST_PTR_FORMAT, cur_effect);
  gst_bin_remove (GST_BIN (pipeline), cur_effect);

  /* push current effect back into the queue */
  g_queue_push_tail (&effects, g_steal_pointer (&cur_effect));

  /* add, link and start the new effect */
  GST_DEBUG_OBJECT (pipeline, "adding   %" GST_PTR_FORMAT, next);
  gst_bin_add (GST_BIN (pipeline), next);

  GST_DEBUG_OBJECT (pipeline, "linking..");
  gst_element_link_many (conv_before, next, conv_after, NULL);

  gst_element_set_state (next, GST_STATE_PLAYING);

  cur_effect = next;
  GST_DEBUG_OBJECT (pipeline, "done");

  return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn
pad_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstPad *srcpad, *sinkpad;

  GST_DEBUG_OBJECT (pad, "pad is blocked now");

  /* remove the probe first */
  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  /* install new probe for EOS */
  srcpad = gst_element_get_static_pad (cur_effect, "src");
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_BLOCK |
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe_cb, user_data, NULL);
  gst_object_unref (srcpad);

  /* push EOS into the element, the probe will be fired when the
   * EOS leaves the effect and it has thus drained all of its data */
  sinkpad = gst_element_get_static_pad (cur_effect, "sink");
  gst_pad_send_event (sinkpad, gst_event_new_eos ());
  gst_object_unref (sinkpad);

  return GST_PAD_PROBE_OK;
}

static gboolean
timeout_cb (gpointer user_data)
{
  gst_pad_add_probe (blockpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      pad_probe_cb, user_data, NULL);

  return TRUE;
}

static gboolean
bus_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GMainLoop *loop = user_data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_object_default_error (msg->src, err, dbg);
      g_clear_error (&err);
      g_free (dbg);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

int
main (int argc, char **argv)
{
  GOptionEntry options[] = {
    {"effects", 'e', 0, G_OPTION_ARG_STRING, &opt_effects,
        "Effects to use (comma-separated list of element names)", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;
  GMainLoop *loop;
  GstElement *src, *q1, *q2, *effect, *filter, *sink;
  gchar **effect_names, **e;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_error ("Error initializing: %s\n", err->message);
    return 1;
  }
  g_option_context_free (ctx);

  if (opt_effects != NULL)
    effect_names = g_strsplit (opt_effects, ",", -1);
  else
    effect_names = g_strsplit (DEFAULT_EFFECTS, ",", -1);

  for (e = effect_names; e != NULL && *e != NULL; ++e) {
    GstElement *el;

    el = gst_element_factory_make (*e, NULL);
    if (el) {
      g_print ("Adding effect '%s'\n", *e);
      g_queue_push_tail (&effects, gst_object_ref_sink (el));
    }
  }

  pipeline = gst_pipeline_new ("pipeline");

  src = gst_element_factory_make ("videotestsrc", NULL);
  g_object_set (src, "is-live", TRUE, NULL);

  filter = gst_element_factory_make ("capsfilter", NULL);
  gst_util_set_object_arg (G_OBJECT (filter), "caps",
      "video/x-raw, width=320, height=240, "
      "format={ I420, YV12, YUY2, UYVY, AYUV, Y41B, Y42B, "
      "YVYU, Y444, v210, v216, NV12, NV21, UYVP, A420, YUV9, YVU9, IYU1 }");

  q1 = gst_element_factory_make ("queue", NULL);

  blockpad = gst_element_get_static_pad (q1, "src");

  conv_before = gst_element_factory_make ("videoconvert", NULL);

  effect = g_queue_pop_head (&effects);
  cur_effect = effect;

  conv_after = gst_element_factory_make ("videoconvert", NULL);

  q2 = gst_element_factory_make ("queue", NULL);

  sink = gst_element_factory_make ("autovideosink", NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, filter, q1, conv_before, effect,
      conv_after, q2, sink, NULL);

  gst_element_link_many (src, filter, q1, conv_before, effect, conv_after,
      q2, sink, NULL);

  if (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Error starting pipeline");
    return 1;
  }

  loop = g_main_loop_new (NULL, FALSE);

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_cb, loop);

  g_timeout_add_seconds (1, timeout_cb, loop);

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (blockpad);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);

  g_queue_clear_full (&effects, (GDestroyNotify) gst_object_unref);
  gst_object_unref (cur_effect);
  g_strfreev (effect_names);

  return 0;
}
