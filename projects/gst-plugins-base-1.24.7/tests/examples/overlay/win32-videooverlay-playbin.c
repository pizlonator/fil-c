/*
 * GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
#include <gst/video/videooverlay.h>
#include <windows.h>

static GMainLoop *loop = NULL;
static gboolean visible = FALSE;
static HWND hwnd = NULL;
static gboolean set_handle_on_request = FALSE;
static gboolean test_reuse = FALSE;

static LRESULT CALLBACK
window_proc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_DESTROY:
      hwnd = NULL;

      if (loop) {
        g_main_loop_quit (loop);
      }
      return 0;
    default:
      break;
  }

  return DefWindowProc (hWnd, message, wParam, lParam);
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
bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstElement *playbin = GST_ELEMENT (user_data);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:
      /* make window visible when we have something to show */
      if (!visible && hwnd) {
        ShowWindow (hwnd, SW_SHOW);
        visible = TRUE;
      }

      gst_element_set_state (playbin, GST_STATE_PLAYING);
      break;
    case GST_MESSAGE_EOS:
      gst_println ("End of stream");
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }

  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  if (set_handle_on_request &&
      gst_is_video_overlay_prepare_window_handle_message (msg)) {
    GstVideoOverlay *overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (msg));

    gst_println ("Pipeline needs window handle");
    gst_video_overlay_set_window_handle (overlay, (guintptr) hwnd);
    gst_message_unref (msg);

    return GST_BUS_DROP;
  }

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s ", err->message);
      if (dbg != NULL)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      test_reuse = FALSE;

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return GST_BUS_PASS;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *playbin;
  WNDCLASSEX wc = { 0, };
  HINSTANCE hinstance = GetModuleHandle (NULL);
  GIOChannel *msg_io_channel;
  GOptionContext *option_ctx;
  GError *error = NULL;
  gchar *uri = NULL;
  RECT wr = { 0, 0, 320, 240 };
  gint exitcode = 0;
  gboolean ret;
  GOptionEntry options[] = {
    {"uri", 0, 0, G_OPTION_ARG_STRING, &uri,
        "URI to test playback with Win32 overlay", NULL}
    ,
    {"set-handle-on-request", 0, 0, G_OPTION_ARG_NONE, &set_handle_on_request,
        "Set window handle on \"prepare-window-handle\" message", NULL}
    ,
    {"repeat", 0, 0, G_OPTION_ARG_NONE, &test_reuse,
        "Repeat and reuse pipeline per EOS", NULL}
    ,
    {NULL}
  };

  option_ctx =
      g_option_context_new ("WIN32 video overlay with playbin example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", error->message);
    g_clear_error (&error);
    exit (1);
  }

  if (!uri) {
    gst_printerrln ("--uri is a required argument");
    g_clear_error (&error);
    exit (1);
  }

  /* prepare window */
  wc.cbSize = sizeof (WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = (WNDPROC) window_proc;
  wc.hInstance = hinstance;
  wc.hCursor = LoadCursor (NULL, IDC_ARROW);
  wc.lpszClassName = "GstWin32VideoOverlayPlaybin";
  RegisterClassEx (&wc);

  AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);
  hwnd = CreateWindowEx (0, wc.lpszClassName,
      "GstWin32VideoOverlayPlaybin",
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, (HWND) NULL, (HMENU) NULL,
      hinstance, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  msg_io_channel = g_io_channel_win32_new_messages (0);
  g_io_add_watch (msg_io_channel, G_IO_IN, msg_cb, NULL);

  /* prepare the pipeline */
  playbin = gst_element_factory_make ("playbin", NULL);

  if (!playbin) {
    gst_printerrln ("playbin is not available");

    exitcode = 1;
    goto terminate;
  }

  /* User can set window handle on playbin before starting
   * pipeline without watching "prepare-window-handle" message,
   * because playbin/playsink will pass the given handle to selected
   * video sink element later once video sink is prepared.
   *
   * But in case that an application wants to delay setting window handle
   * as much as possible for some reason, the application needs to check
   * "prepare-window-handle" message
   * (use gst_is_video_overlay_prepare_window_handle_message() API for check)
   * via a *sync* message handler and should set window handle on
   * the *sync* message handler immediately */
  if (!set_handle_on_request) {
    GstVideoOverlay *overlay = GST_VIDEO_OVERLAY (playbin);

    gst_println ("Setting window handle now");
    gst_video_overlay_set_window_handle (overlay, (guintptr) hwnd);
  } else {
    gst_println ("Will set window handle on \"prepare-window-handle\" message");
  }

  gst_bus_add_watch (GST_ELEMENT_BUS (playbin), bus_msg, playbin);
  gst_bus_set_sync_handler (GST_ELEMENT_BUS (playbin),
      bus_sync_handler, NULL, NULL);
  g_object_set (playbin, "uri", uri, NULL);

  do {
    if (gst_element_set_state (playbin,
            GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
      gst_printerrln ("Pipeline doesn't want to pause");
      gst_bus_remove_watch (GST_ELEMENT_BUS (playbin));

      exitcode = 1;
      goto terminate;
    }

    g_main_loop_run (loop);
    gst_element_set_state (playbin, GST_STATE_NULL);
  } while (test_reuse);

  gst_bus_remove_watch (GST_ELEMENT_BUS (playbin));

terminate:
  if (hwnd)
    DestroyWindow (hwnd);

  gst_object_unref (playbin);
  g_io_channel_unref (msg_io_channel);
  g_main_loop_unref (loop);
  g_free (uri);

  return exitcode;
}
