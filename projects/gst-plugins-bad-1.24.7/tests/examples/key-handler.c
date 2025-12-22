/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "key-handler.h"

#ifdef G_OS_WIN32
#include <windows.h>

typedef struct _Win32KeyHandler
{
  GThread *thread;
  HANDLE cancellable;
  HANDLE console_handle;
  GMutex lock;
  gboolean closing;

  KeyInputCallback callback;
  gpointer user_data;
} Win32KeyHandler;

typedef struct
{
  KeyInputCallback callback;
  gpointer user_data;
  gchar value;
  gboolean is_ascii;
} KeyInputCallbackData;

static Win32KeyHandler *_handler = NULL;

static gboolean
handler_source_func (KeyInputCallbackData * data)
{
  data->callback (data->value, data->is_ascii, data->user_data);

  return G_SOURCE_REMOVE;
}

static gpointer
handler_thread_func (Win32KeyHandler * handler)
{
  HANDLE handles[2];

  handles[0] = handler->cancellable;
  handles[1] = handler->console_handle;

  while (TRUE) {
    DWORD ret = WaitForMultipleObjects (2, handles, FALSE, INFINITE);
    INPUT_RECORD buffer;
    DWORD num_read = 0;
    KeyInputCallbackData *data;

    if (ret == WAIT_FAILED) {
      gst_printerrln ("Wait failed");
      return NULL;
    }

    g_mutex_lock (&handler->lock);
    if (handler->closing) {
      g_mutex_unlock (&handler->lock);

      return NULL;
    }
    g_mutex_unlock (&handler->lock);

    if (!PeekConsoleInput (handler->console_handle, &buffer, 1, &num_read) ||
        num_read != 1)
      continue;

    ReadConsoleInput (handler->console_handle, &buffer, 1, &num_read);
    if (buffer.EventType != KEY_EVENT || !buffer.Event.KeyEvent.bKeyDown)
      continue;

    data = g_new0 (KeyInputCallbackData, 1);
    data->callback = handler->callback;
    data->user_data = handler->user_data;

    switch (buffer.Event.KeyEvent.wVirtualKeyCode) {
      case VK_UP:
        data->value = (gchar) KB_ARROW_UP;
        break;
      case VK_DOWN:
        data->value = (gchar) KB_ARROW_DOWN;
        break;
      case VK_LEFT:
        data->value = (gchar) KB_ARROW_LEFT;
        break;
      case VK_RIGHT:
        data->value = (gchar) KB_ARROW_RIGHT;
        break;
      default:
        data->value = buffer.Event.KeyEvent.uChar.AsciiChar;
        data->is_ascii = TRUE;
        break;
    }

    g_main_context_invoke_full (NULL,
        G_PRIORITY_DEFAULT,
        (GSourceFunc) handler_source_func, data, (GDestroyNotify) g_free);
  }
}

void
set_key_handler (KeyInputCallback callback, gpointer user_data)
{
  if (_handler || !callback)
    return;

  _handler = g_new0 (Win32KeyHandler, 1);

  SECURITY_ATTRIBUTES attr;
  attr.nLength = sizeof (SECURITY_ATTRIBUTES);
  attr.lpSecurityDescriptor = NULL;
  attr.bInheritHandle = FALSE;

  _handler->cancellable = CreateEvent (&attr, TRUE, FALSE, NULL);
  _handler->console_handle = GetStdHandle (STD_INPUT_HANDLE);
  _handler->callback = callback;
  _handler->user_data = user_data;
  g_mutex_init (&_handler->lock);
  _handler->thread =
      g_thread_new ("key-handler", (GThreadFunc) handler_thread_func, _handler);
}

void
unset_key_handler (void)
{
  if (!_handler)
    return;

  g_mutex_lock (&_handler->lock);
  _handler->closing = TRUE;
  g_mutex_unlock (&_handler->lock);

  SetEvent (_handler->cancellable);
  g_thread_join (_handler->thread);
  CloseHandle (_handler->cancellable);
  g_mutex_clear (&_handler->lock);

  g_clear_pointer (&_handler, g_free);
}
#else /* G_OS_WIN32 */

#include <termios.h>
#include <unistd.h>
#include <stdio.h>

typedef struct _LinuxKeyHandler
{
  gulong watch_id;
  struct termios term_settings;
  KeyInputCallback callback;
  GSource *source;
  gpointer user_data;
} LinuxKeyHandler;

static LinuxKeyHandler *_handler = NULL;

static gboolean
_handlerio_func (GIOChannel * channel,
    GIOCondition condition, LinuxKeyHandler * handler)
{
  if (condition & G_IO_IN) {
    GIOStatus status;
    gchar buf[16] = { 0, };
    gsize read;

    status =
        g_io_channel_read_chars (channel, buf, sizeof (buf) - 1, &read, NULL);
    if (status == G_IO_STATUS_ERROR) {
      return G_SOURCE_REMOVE;
    }

    if (status == G_IO_STATUS_NORMAL) {
      gchar value;
      gboolean is_ascii = FALSE;

      if (g_strcmp0 (buf, "\033[A") == 0) {
        value = (gchar) KB_ARROW_UP;
      } else if (g_strcmp0 (buf, "\033[B") == 0) {
        value = (gchar) KB_ARROW_DOWN;
      } else if (g_strcmp0 (buf, "\033[D") == 0) {
        value = (gchar) KB_ARROW_LEFT;
      } else if (g_strcmp0 (buf, "\033[C") == 0) {
        value = (gchar) KB_ARROW_RIGHT;
      } else {
        value = buf[0];
        is_ascii = TRUE;
      }

      handler->callback (value, is_ascii, handler->user_data);
    }
  }

  return G_SOURCE_CONTINUE;
}

void
set_key_handler (KeyInputCallback callback, gpointer user_data)
{
  struct termios new_settings;
  struct termios old_settings;
  GIOChannel *io_channel;

  if (_handler || !callback)
    return;

  if (tcgetattr (STDIN_FILENO, &old_settings) != 0)
    return;

  new_settings = old_settings;
  new_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  new_settings.c_cc[VMIN] = 0;
  new_settings.c_cc[VTIME] = 0;

  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &new_settings) != 0)
    return;

  setvbuf (stdin, NULL, _IONBF, 0);

  _handler = g_new0 (LinuxKeyHandler, 1);
  _handler->term_settings = old_settings;
  _handler->callback = callback;
  _handler->user_data = user_data;

  io_channel = g_io_channel_unix_new (STDIN_FILENO);

  _handler->source = g_io_create_watch (io_channel, G_IO_IN);
  g_io_channel_unref (io_channel);

  g_source_set_callback (_handler->source, (GSourceFunc) _handlerio_func,
      _handler, NULL);
  g_source_attach (_handler->source, NULL);
}

void
unset_key_handler (void)
{
  if (!_handler)
    return;

  if (_handler->source) {
    g_source_destroy (_handler->source);
    g_source_unref (_handler->source);
  }

  tcsetattr (STDIN_FILENO, TCSAFLUSH, &_handler->term_settings);
  setvbuf (stdin, NULL, _IOLBF, 0);

  g_clear_pointer (&_handler, g_free);
}
#endif
