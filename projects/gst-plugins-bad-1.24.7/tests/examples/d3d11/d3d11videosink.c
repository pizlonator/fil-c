/*
 * GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <gst/video/video.h>

#include <windows.h>
#include <string.h>
#include "../key-handler.h"

static GMainLoop *loop = NULL;
static gboolean visible = FALSE;
static gboolean test_reuse = FALSE;
static HWND hwnd = NULL;

typedef struct
{
  GstElement *pipeline;
  GstElement *sink;
  gboolean fullscreen;
  gboolean force_aspect_ratio;
} CallbackData;

static LRESULT CALLBACK
window_proc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_DESTROY:
      hwnd = NULL;
      g_print ("destroy\n");
      if (loop) {
        g_main_loop_quit (loop);
      }
      return 0;
    default:
      break;
  }

  return DefWindowProc (hWnd, message, wParam, lParam);
}

static void
keyboard_cb (gchar input, gboolean is_ascii, CallbackData * data)
{
  if (!is_ascii)
    return;

  switch (input) {
    case 'q':
    case 'Q':
      gst_element_send_event (data->pipeline, gst_event_new_eos ());
      if (hwnd)
        PostMessage (hwnd, WM_CLOSE, 0, 0);
      else
        g_main_loop_quit (loop);
      break;
    case 27:                   /* ESC */
      gst_element_send_event (data->pipeline, gst_event_new_eos ());
      if (hwnd)
        PostMessage (hwnd, WM_CLOSE, 0, 0);
      else
        g_main_loop_quit (loop);
      break;
    case ' ':
      data->fullscreen = !data->fullscreen;
      gst_print ("change to %s mode\n", data->fullscreen ?
          "fullscreen" : "windowed");
      g_object_set (data->sink, "fullscreen", data->fullscreen, NULL);
      break;
    case 'f':
      data->force_aspect_ratio = !data->force_aspect_ratio;
      g_object_set (data->sink,
          "force-aspect-ratio", data->force_aspect_ratio, NULL);
      break;
    default:
      break;
  }
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, CallbackData * data)
{
  GstElement *pipeline = data->pipeline;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:
      /* make window visible when we have something to show */
      if (!visible && hwnd) {
        ShowWindow (hwnd, SW_SHOW);
        visible = TRUE;
      }

      gst_element_set_state (pipeline, GST_STATE_PLAYING);
      break;
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("ERROR %s \n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);
      test_reuse = FALSE;

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_ELEMENT:
    {
      GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);
      if (mtype == GST_NAVIGATION_MESSAGE_EVENT) {
        GstEvent *ev = NULL;

        if (gst_navigation_message_parse_event (msg, &ev)) {
          GstNavigationEventType e_type = gst_navigation_event_get_type (ev);
          if (e_type == GST_NAVIGATION_EVENT_KEY_PRESS) {
            const gchar *key;

            if (gst_navigation_event_parse_key_event (ev, &key)) {
              if (strcmp (key, "space") == 0 || strcmp (key, "Space") == 0) {
                keyboard_cb (' ', TRUE, data);
              } else {
                keyboard_cb (key[0], TRUE, data);
              }
            }
          }
        }
        if (ev)
          gst_event_unref (ev);
      }
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

static gboolean
timeout_cb (gpointer user_data)
{
  g_main_loop_quit ((GMainLoop *) user_data);

  return G_SOURCE_REMOVE;
}

static void
print_keyboard_help (void)
{
  static struct
  {
    const gchar *key_desc;
    const gchar *key_help;
  } key_controls[] = {
    {
        "q or ESC", "Quit"}, {
        "SPACE", "Toggle fullscreen mode"}, {
        "f", "Toggle force-aspect-ratio"}
  };
  guint i, chars_to_pad, desc_len, max_desc_len = 0;

  gst_print ("\n%s\n", "Keyboard controls:");

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    desc_len = g_utf8_strlen (key_controls[i].key_desc, -1);
    max_desc_len = MAX (max_desc_len, desc_len);
  }
  ++max_desc_len;

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    chars_to_pad = max_desc_len - g_utf8_strlen (key_controls[i].key_desc, -1);
    gst_print ("\t%s", key_controls[i].key_desc);
    gst_print ("%-*s: ", chars_to_pad, "");
    gst_print ("%s\n", key_controls[i].key_help);
  }
  gst_print ("\n");
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline, *src, *sink;
  GstStateChangeReturn sret;
  WNDCLASSEX wc = { 0, };
  HINSTANCE hinstance = GetModuleHandle (NULL);
  GIOChannel *msg_io_channel = NULL;
  GOptionContext *option_ctx;
  GError *error = NULL;
  RECT wr = { 0, 0, 320, 240 };
  gint exitcode = 0;
  gboolean ret;
  gboolean use_overlay = FALSE;
  gboolean start_fullscreen = FALSE;
  GOptionEntry options[] = {
    {"use-overlay", 0, 0, G_OPTION_ARG_NONE, &use_overlay,
        "Test reuse video sink element", NULL}
    ,
    {"repeat", 0, 0, G_OPTION_ARG_NONE, &test_reuse,
        "Test reuse video sink element", NULL}
    ,
    {"start-fullscreen", 0, 0, G_OPTION_ARG_NONE, &start_fullscreen,
        "Run pipeline in fullscreen mode", NULL}
    ,
    {NULL}
  };
  gint num_repeat = 0;
  CallbackData cb_data = { 0, };

  option_ctx = g_option_context_new ("WIN32 video overlay example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  print_keyboard_help ();

  loop = g_main_loop_new (NULL, FALSE);

  if (use_overlay) {
    /* prepare window */
    wc.cbSize = sizeof (WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC) window_proc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.lpszClassName = "GstD3D11VideoSinkExample";
    RegisterClassEx (&wc);

    AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd = CreateWindowEx (0, wc.lpszClassName, "GstD3D11VideoSinkExample",
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top, (HWND) NULL, (HMENU) NULL,
        hinstance, NULL);

    msg_io_channel = g_io_channel_win32_new_messages (0);
    g_io_add_watch (msg_io_channel, G_IO_IN, msg_cb, NULL);
  }

  /* prepare the pipeline */
  pipeline = gst_pipeline_new ("d3d11videosink-pipeline");
  src = gst_element_factory_make ("videotestsrc", NULL);
  sink = gst_element_factory_make ("d3d11videosink", NULL);

  cb_data.fullscreen = start_fullscreen;
  cb_data.force_aspect_ratio = TRUE;
  cb_data.pipeline = pipeline;
  cb_data.sink = sink;

  g_object_set (sink, "fullscreen-toggle-mode", 0x2 | 0x4, NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link (src, sink);

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), (GstBusFunc) bus_msg,
      &cb_data);

  set_key_handler ((KeyInputCallback) keyboard_cb, &cb_data);

  if (start_fullscreen)
    g_object_set (sink, "fullscreen", TRUE, NULL);

  do {
    gst_print ("Running loop %d\n", num_repeat++);

    if (use_overlay) {
      gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink),
          (guintptr) hwnd);
    }

    sret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
    if (sret == GST_STATE_CHANGE_FAILURE) {
      g_printerr ("Pipeline doesn't want to pause\n");
      break;
    } else {
      /* add timer to repeat and reuse pipeline  */
      if (test_reuse) {
        GSource *timeout_source = g_timeout_source_new_seconds (3);

        g_source_set_callback (timeout_source,
            (GSourceFunc) timeout_cb, loop, NULL);
        g_source_attach (timeout_source, NULL);
        g_source_unref (timeout_source);
      }

      g_main_loop_run (loop);
    }
    gst_element_set_state (pipeline, GST_STATE_NULL);

    visible = FALSE;
  } while (test_reuse);

  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

terminate:
  unset_key_handler ();

  if (hwnd)
    DestroyWindow (hwnd);

  gst_object_unref (pipeline);
  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);
  g_main_loop_unref (loop);

  return exitcode;
}
