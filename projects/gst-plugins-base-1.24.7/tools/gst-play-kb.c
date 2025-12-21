/* GStreamer command line playback testing utility - keyboard handling helpers
 *
 * Copyright (C) 2013 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2013 Centricular Ltd
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

#include "gst-play-kb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include <termios.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#endif

#include <gst/gst.h>

/* This is all not thread-safe, but doesn't have to be really */
static GstPlayKbFunc kb_callback;
static gpointer kb_callback_data;

#ifdef G_OS_UNIX
static struct termios term_settings;
static gboolean term_settings_saved = FALSE;
static gulong io_watch_id;

static gboolean
gst_play_kb_io_cb (GIOChannel * ioc, GIOCondition cond, gpointer user_data)
{
  GIOStatus status;

  if (cond & G_IO_IN) {
    gchar buf[16] = { 0, };
    gsize read;

    status = g_io_channel_read_chars (ioc, buf, sizeof (buf) - 1, &read, NULL);
    if (status == G_IO_STATUS_ERROR)
      return FALSE;
    if (status == G_IO_STATUS_NORMAL) {
      if (kb_callback)
        kb_callback (buf, kb_callback_data);
    }
  }

  return TRUE;                  /* call us again */
}

gboolean
gst_play_kb_set_key_handler (GstPlayKbFunc kb_func, gpointer user_data)
{
  GIOChannel *ioc;

  if (!isatty (STDIN_FILENO)) {
    GST_INFO ("stdin is not connected to a terminal");
    return FALSE;
  }

  if (io_watch_id > 0) {
    g_source_remove (io_watch_id);
    io_watch_id = 0;
  }

  if (kb_func == NULL && term_settings_saved) {
    /* restore terminal settings */
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_settings) == 0)
      term_settings_saved = FALSE;
    else
      g_warning ("could not restore terminal attributes");

    setvbuf (stdin, NULL, _IOLBF, 0);
  }

  if (kb_func != NULL) {
    struct termios new_settings;

    if (!term_settings_saved) {
      if (tcgetattr (STDIN_FILENO, &term_settings) != 0) {
        g_warning ("could not save terminal attributes");
        return FALSE;
      }
      term_settings_saved = TRUE;

      /* Echo off, canonical mode off, extended input processing off  */
      new_settings = term_settings;
      new_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN);
      new_settings.c_cc[VMIN] = 0;
      new_settings.c_cc[VTIME] = 0;

      if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &new_settings) != 0) {
        g_warning ("Could not set terminal state");
        return FALSE;
      }
      setvbuf (stdin, NULL, _IONBF, 0);
    }
  }

  ioc = g_io_channel_unix_new (STDIN_FILENO);

  io_watch_id = g_io_add_watch_full (ioc, G_PRIORITY_DEFAULT, G_IO_IN,
      (GIOFunc) gst_play_kb_io_cb, user_data, NULL);
  g_io_channel_unref (ioc);

  kb_callback = kb_func;
  kb_callback_data = user_data;

  return TRUE;
}

#elif defined(G_OS_WIN32)

typedef struct
{
  GThread *thread;
  HANDLE event_handle;
  HANDLE console_handle;
  gboolean closing;
  GMutex lock;
} Win32KeyHandler;

static Win32KeyHandler *win32_handler = NULL;

typedef struct
{
  gboolean is_ascii;
  WORD vkcode;
  gchar key_val[2];
} KbSourceData;

static gboolean
gst_play_kb_source_cb (KbSourceData * data)
{
  if (!kb_callback)
    return G_SOURCE_REMOVE;

  if (data->is_ascii) {
    kb_callback (data->key_val, kb_callback_data);
  } else {
    switch (data->vkcode) {
      case VK_RIGHT:
        kb_callback (GST_PLAY_KB_ARROW_RIGHT, kb_callback_data);
        break;
      case VK_LEFT:
        kb_callback (GST_PLAY_KB_ARROW_LEFT, kb_callback_data);
        break;
      case VK_UP:
        kb_callback (GST_PLAY_KB_ARROW_UP, kb_callback_data);
        break;
      case VK_DOWN:
        kb_callback (GST_PLAY_KB_ARROW_DOWN, kb_callback_data);
        break;
      default:
        break;
    }
  }

  return G_SOURCE_REMOVE;
}

static gpointer
gst_play_kb_win32_thread (gpointer user_data)
{
  Win32KeyHandler *handler = (Win32KeyHandler *) user_data;
  HANDLE handles[2];
  INPUT_RECORD buffer;
  DWORD n;

  handles[0] = handler->event_handle;
  handles[1] = handler->console_handle;

  if (!kb_callback)
    return NULL;

  while (TRUE) {
    DWORD ret = WaitForMultipleObjects (2, handles, FALSE, INFINITE);

    if (ret == WAIT_FAILED) {
      GST_WARNING ("WaitForMultipleObject Failed");
      return NULL;
    }

    g_mutex_lock (&handler->lock);
    if (handler->closing) {
      g_mutex_unlock (&handler->lock);

      return NULL;
    }
    g_mutex_unlock (&handler->lock);

    if (PeekConsoleInput (handler->console_handle, &buffer, 1, &n) && n == 1) {
      if (ReadConsoleInput (handler->console_handle, &buffer, 1, &n) &&
          buffer.EventType == KEY_EVENT && buffer.Event.KeyEvent.bKeyDown) {
        KbSourceData *data = g_new0 (KbSourceData, 1);

        switch (buffer.Event.KeyEvent.wVirtualKeyCode) {
          case VK_RIGHT:
          case VK_LEFT:
          case VK_UP:
          case VK_DOWN:
            data->is_ascii = FALSE;
            data->vkcode = buffer.Event.KeyEvent.wVirtualKeyCode;
            break;
          default:
            data->is_ascii = TRUE;
            data->key_val[0] = buffer.Event.KeyEvent.uChar.AsciiChar;
            data->key_val[1] = '\0';
            break;
        }

        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
            (GSourceFunc) gst_play_kb_source_cb, data, g_free);
      }
    }
  }

  return NULL;
}

gboolean
gst_play_kb_set_key_handler (GstPlayKbFunc kb_func, gpointer user_data)
{
  gint fd = _fileno (stdin);

  if (!_isatty (fd)) {
    GST_INFO ("stdin is not connected to a terminal");
    return FALSE;
  }

  if (win32_handler) {
    g_mutex_lock (&win32_handler->lock);
    win32_handler->closing = TRUE;
    g_mutex_unlock (&win32_handler->lock);

    SetEvent (win32_handler->event_handle);
    g_thread_join (win32_handler->thread);
    CloseHandle (win32_handler->event_handle);

    g_mutex_clear (&win32_handler->lock);
    g_free (win32_handler);
    win32_handler = NULL;
  }

  if (kb_func) {
    SECURITY_ATTRIBUTES sec_attrs;

    sec_attrs.nLength = sizeof (SECURITY_ATTRIBUTES);
    sec_attrs.lpSecurityDescriptor = NULL;
    sec_attrs.bInheritHandle = FALSE;

    win32_handler = g_new0 (Win32KeyHandler, 1);

    /* create cancellable event handle */
    win32_handler->event_handle = CreateEvent (&sec_attrs, TRUE, FALSE, NULL);

    if (!win32_handler->event_handle) {
      GST_WARNING ("Couldn't create event handle");
      g_free (win32_handler);
      win32_handler = NULL;

      return FALSE;
    }

    win32_handler->console_handle = GetStdHandle (STD_INPUT_HANDLE);
    if (!win32_handler->console_handle) {
      GST_WARNING ("Couldn't get console handle");
      CloseHandle (win32_handler->event_handle);
      g_free (win32_handler);
      win32_handler = NULL;

      return FALSE;
    }

    g_mutex_init (&win32_handler->lock);
    win32_handler->thread =
        g_thread_new ("gst-play-kb", gst_play_kb_win32_thread, win32_handler);
  }

  kb_callback = kb_func;
  kb_callback_data = user_data;

  return TRUE;
}

#else

gboolean
gst_play_kb_set_key_handler (GstPlayKbFunc key_func, gpointer user_data)
{
  GST_FIXME ("Keyboard handling for this OS needs to be implemented");
  return FALSE;
}

#endif /* !G_OS_UNIX */
