/* GStreamer
 *
 * Unit test for d3d11videosink element
 * Copyright (C) 2023 Alexander Slobodeniuk <aslobodeniuk@fluendo.com>
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
#include <gst/video/video.h>
#include <windows.h>

#define WM_FIXTURE_RESTART (WM_USER + 123)
#define WM_FIXTURE_TRICKY_RESTART (WM_FIXTURE_RESTART + 1)

static struct
{
  HWND hwnd;
  GstElement *pipeline;
  gboolean ever_started;
  volatile gboolean stopping;
} fixture;

static void
fixture_sleep_approx_ms (guint ms)
{
  Sleep (g_random_int_range (0, ms));
}

static gpointer
fixture_restart_playback (gpointer data)
{
  GST_DEBUG ("Restarting the pipeline");
  gst_element_set_state (fixture.pipeline, GST_STATE_NULL);
  GST_DEBUG ("Stopped");
  gst_element_set_state (fixture.pipeline, GST_STATE_PLAYING);
  GST_DEBUG ("Started");
  return NULL;
}


static LRESULT CALLBACK
test_win32window_window_proc (HWND hwnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  switch (message) {
    case WM_FIXTURE_RESTART:
      fixture_restart_playback (NULL);
      break;
    case WM_FIXTURE_TRICKY_RESTART:
      /* Synchronous stop, but the sink will have to
       * release it's internal window asynchronously */
      g_thread_join (g_thread_new (NULL, fixture_restart_playback, NULL));
      break;
    case WM_DESTROY:
      PostQuitMessage (0);
      break;
    default:
      /* Simulate application load */
      fixture_sleep_approx_ms (30);
      return DefWindowProc (hwnd, message, wParam, lParam);
  }
  return 0;
}

static void
test_win32window_create_window (void)
{
  HINSTANCE hinstance = GetModuleHandleA (NULL);
  WNDCLASSEXA wc;
  ATOM atom = 0;
  const gchar *class_name = "d3d11videosink_test";

  GST_DEBUG ("Creating a win32 window");
  atom = GetClassInfoExA (hinstance, class_name, &wc);
  if (atom == 0) {
    ZeroMemory (&wc, sizeof (WNDCLASSEXA));
    wc.cbSize = sizeof (WNDCLASSEX);

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = test_win32window_window_proc;
    wc.hInstance = hinstance;
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszClassName = class_name;

    fail_unless (RegisterClassExA (&wc));
  }

  fixture.hwnd =
      CreateWindowA (class_name, "d3d11videosink test", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hinstance, NULL);
  fail_unless (fixture.hwnd);
}

static void
test_win32window_msg_loop (void)
{
  MSG msg;
  while (GetMessage (&msg, NULL, 0, 0)) {
    TranslateMessage (&msg);
    DispatchMessage (&msg);
  }
}

static gpointer
test_win32window_resize_thr2 (gpointer data)
{
  gint16 length = 0;
  while (!fixture.stopping) {
    if ((length += 1) > 164) {
      length = 0;
    }

    MoveWindow (fixture.hwnd, 0, 0, length, length, 1);
    fixture_sleep_approx_ms (10);
  }
  return NULL;
}

static gpointer
test_win32window_resize_thr1 (gpointer data)
{
  gint i;
  const gint NUM_REPETITIONS = 150;

  GThread *thr = g_thread_new (NULL,
      test_win32window_resize_thr2, NULL);

  for (i = 0; i < NUM_REPETITIONS; i++) {
    gst_element_set_state (fixture.pipeline, GST_STATE_PLAYING);
    /* Pause in playback */
    fixture_sleep_approx_ms (500);


    if (0 == i % 4) {
      GST_DEBUG ("Scheduling pipeline restart from the window thread");
      PostMessageA (fixture.hwnd, WM_FIXTURE_RESTART, 0, 0);
      fixture_sleep_approx_ms (500);
    }

    if (0 == i % 5) {
      GST_DEBUG ("Scheduling pipeline restart blocking the window thread");
      PostMessageA (fixture.hwnd, WM_FIXTURE_TRICKY_RESTART, 0, 0);
      fixture_sleep_approx_ms (500);
    }

    gst_element_set_state (fixture.pipeline, GST_STATE_NULL);
    /* Pause without playback */
    fixture_sleep_approx_ms (100);
  }

  fixture.stopping = TRUE;
  g_thread_join (thr);

  GST_INFO ("Closing the window");
  PostMessageA (fixture.hwnd, WM_CLOSE, 0, 0);
  return NULL;
}

static GstBusSyncReply
test_win32window_bus_sync_handle (GstBus * bus,
    GstMessage * message, gpointer data)
{
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT ||
      !gst_is_video_overlay_prepare_window_handle_message (message)) {
    return GST_BUS_PASS;
  }

  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY
      (GST_MESSAGE_SRC (message)), (guintptr) fixture.hwnd);
  fixture.ever_started = TRUE;
  return GST_BUS_DROP;
}

GST_START_TEST (test_win32window_resize)
{
  GstBus *bus;
  GThread *thr;

  GST_INFO ("Start resising test");
  test_win32window_create_window ();

  fixture.pipeline =
      gst_parse_launch
      ("videotestsrc ! video/x-raw, width=128, height=128 ! d3d11videosink sync=false",
      NULL);
  bus = gst_element_get_bus (fixture.pipeline);
  gst_bus_set_sync_handler (bus, test_win32window_bus_sync_handle, NULL, NULL);
  gst_object_unref (bus);
  bus = NULL;

  ShowWindow (fixture.hwnd, SW_SHOW);
  UpdateWindow (fixture.hwnd);

  thr = g_thread_new (NULL, test_win32window_resize_thr1, NULL);

  test_win32window_msg_loop ();

  g_thread_join (thr);
  gst_object_unref (fixture.pipeline);
  fixture.hwnd = NULL;

  fail_unless (fixture.ever_started, "videosink didn't request window handle,"
      "probably it couldn't start");
  GST_INFO ("resising test ok");
}

GST_END_TEST;

static Suite *
d3d11videosink_suite (void)
{
  Suite *s = suite_create ("d3d11videosink");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_win32window_resize);

  return s;
}

GST_CHECK_MAIN (d3d11videosink);
