/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
GTK has two clipboards - normal clipboard and primary clipboard
Primary clipboard is only handled
internally by GTK (it's not portable to Windows).

("C:" means clipboard client (requester), "S:" means clipboard server (provider))
("transmute" here means "change the format of some data"; this term is used here
 instead of "convert" to avoid clashing with the old g(t|d)k_selection_convert() APIs,
 which are completely unrelated)

For Clipboard:
GTK calls one of the gdk_clipboard_set* () functions (either supplying
its own content provider, or giving a GTyped data for which GDK will
create a content provider automatically).
That function associates the content provider with the clipboard and calls
S: gdk_clipboard_claim(),
to claim ownership. GDK first calls the backend implementation of
that function, then the
S: gdk_clipboard_real_claim()
implementation.
The "real" function does some mundane bookkeeping, whereas the backend
implementation advertises the formats supported by the clipboard,
if the call says that the claim is local. Non-local (remote) claims
are there just to tell GDK that some other process owns the clipboard
and claims to provide data in particular formats.
No data is sent anywhere.

The content provider has a callback, which will be invoked every time
the data from this provider is needed.

GTK might also call gdk_clipboard_store_async(), which instructs
the backend to put the data into the OS clipboard manager (if
supported and available) so that it remains available for other
processes after the clipboard owner terminates.

When something needs to be obtained from clipboard, GTK calls
C: gdk_clipboard_read_async () -> gdk_clipboard_read_internal (),
providing it with a string-array of mime/types, which is internally
converted into a GdkContentFormats object.
That function creates a task.

Then, if the clipboard is local, it calls
C: gdk_clipboard_read_local_async(),
which matches given formats to the content provider formats and, if there's a match,
creates a pipe, calls
C: gdk_clipboard_write_async()
on the write end, and sets the read end as a return value of the task, which will
later be given to the caller-specified callback.

If the clipboard isn't local, it calls
C: read_async()
of the backend clipboard stream class.
The backend starts creating a stream (somehow) and sets up the callback to return that
stream via the task, once the stream is created.
Either way, the caller-specified callback is invoked, and it gets the read end
of a stream by calling
C: gdk_clipboard_read_finish(),
then reads the data from the stream and unrefs the stream once it is done.
IRL applications use wrappers, which create an extra task that gets the
stream, reads from it asynchronously and turns the bytes that it reads into
some kind of application-specific object type. GDK comes pre-equipped with
functions that read arbitrary GTypes (as long as they are serializable),
texts (strings) or textures (GdkPixbufs) this way.

On Windows:
Clipboard is opened by OpenClipboard(), emptied by EmptyClipboard() (which also
makes the window the clipboard owner), data is put into it by SetClipboardData().
Clipboard is closed with CloseClipboard().
If SetClipboardData() is given a NULL data value, the owner will later
receive WM_RENDERFORMAT message, in response to which it must call
SetClipboardData() with the provided handle and the actual data this time.
This way applications can avoid storing everything in the clipboard
all the time, only putting the data there as it is requested by other applications.
At some undefined points of time an application might get WM_RENDERALLFORMATS
message, it should respond by opening the clipboard and rendering
into it all the data that it offers, as if responding to multiple WM_RENDERFORMAT
messages.

On GDK-Win32:

Any operations that require OpenClipboard()/CloseClipboard() combo (i.e.
almost everything, except for WM_RENDERFORMAT handling) is offloaded into
separate thread, which tries to complete any operations in the queue.
Each operation routine usually starts with a timeout check (all operations
time out after 30 seconds), then a check for clipboard status (to abort
any operations that became obsolete due to clipboard status being changed - 
i.e. retrieving clipboard contents is aborted if clipboard contents change
before the operation can be completed), then an attempt to OpenClipboard().
Failure to OpenClipboard() leads to the queue processing stopping, and
resuming from the beginning after another 1-second delay.
A success in OpenClipboard() allows the operation to continue.
The thread remembers the fact that it has clipboard open, and does
not try to close & reopen it, unless that is strictly necessary.
The clipboard is closed after each queue processing run.

GTK calls one of the gdk_clipboard_set* () functions (either supplying
its own content provider, or giving a GTyped data for which GDK will
create a content provider automatically).
That function associates the content provider with the clipboard and calls
S: gdk_clipboard_claim(),
to claim ownership. GDK first calls the backend implementation of
that function,
S: gdk_win32_clipboard_claim(),
which maps the supported GDK contentformats to W32 data formats and
caches this mapping, then the
S: gdk_clipboard_real_claim()
implementation.
The "real" function does some mundane bookkeeping, whereas the backend
implementation advertises the formats supported by the clipboard,
if the call says that the claim is local. Non-local (remote) claims
are there just to tell GDK that some other process owns the clipboard
and claims to provide data in particular formats.
For the local claims gdk_win32_clipboard_claim() queues a clipboard
advertise operation (see above).
That operation will call EmptyClipboard() to claim the ownership,
then call SetClipboardData() with NULL value for each W32 data format
supported, advertising the W32 data formats to other processes.
No data is sent anywhere.

The content provider has a callback, which will be invoked every time
the data from this provider is needed.

GTK might also call gdk_clipboard_store_async(), which instructs
the W32 backend to put the data into the OS clipboard manager by
sending WM_RENDERALLFORMATS to itself and then handling it normally.

Every time W32 backend gets WM_CLIPBOARDUPDATE,
it calls GetUpdatedClipboardFormats() and GetClipboardSequenceNumber()
and caches the results of both. These calls do not require the clipboard
to be opened.
After that it would call
C: gdk_win32_clipboard_claim_remote()
to indicate that some other process owns the clipboard and supports
the formats from the cached list. If the process is the owner,
the remote claim is not performed (it's assumed that a local claim
was already made when a clipboard content provider is set, so no need
to do that either).
Note: clipboard sequence number changes with each SetClipboardData() call.
Specifically, a process that uses delayed rendering (like GDK does)
must call SetClipboardData() with NULL value every time the data changes,
even if its format remains the same.
The cached remote formats are then mapped into GDK contentformats.
This map is separate from the one that maps supported GDK contentformats
to W32 formats for locally-claimed clipboards.

When something needs to be obtained from clipboard, GTK calls
C: gdk_clipboard_read_async () -> gdk_clipboard_read_internal (),
providing it with a string-array of mime/types, which is internally
converted into a GdkContentFormats object.
That function creates a task.

Then, if the clipboard is local, it calls
C: gdk_clipboard_read_local_async(),
which matches given formats to the content provider formats and, if there's a match,
creates a pipe, calls
C: gdk_clipboard_write_async()
on the write end, and sets the read end as a return value of the task, which will
later be given to the caller-specified callback.

If the clipboard isn't local, it calls
C: read_async()
of the W32 backend clipboard stream class.
It then queues a retrieve operation (see above).
The retrieve operation goes over formats available on the clipboard,
and picks the first one that matches the list supplied with the retrieve
operation (that is, it gives priority to formats at the top of the clipboard
format list, even if such formats are at the bottom of the list of formats
supported by the application; this is due to the fact that formats at the
top of the clipboard format list are usually "raw" or "native" and assumed
to not to be transmuted by the clipboard owner from some other format,
and thus it is better to use these, if the requesting application can handle
them). It then calls GetClipboardData(), which either causes
a WM_RENDERFORMAT to be sent to the server (for delayed rendering),
or it just grabs the data from the OS.

Server-side GDK catches WM_RENDERFORMAT, figures out a contentformat
to request (it has an earlier advertisement cached in the thread, so
there's no need to ask the main thread for anything), and
creates a render request, then sends it to the main thread.
After that it keeps polling the queue until the request comes back.
The main thread render handler creates an output stream that
writes the data into a global memory buffer, then calls
S: gdk_clipboard_write_async()
to write the data.
The callback finishes that up with
S: gdk_clipboard_write_finish(),
which sends the render request back to the clipborad thread,
along with the data. The clipboard thread then calls
S: SetClipboardData()
with the clipboard handle provided by the OS on behalf of the client.

Once the data handle is available, the clipboard thread creates a stream
that reads from a copy of that data (after transmutation, if necessary),
and sends that stream back to the main thread. The data is kept in a client-side
memory buffer (owned by the stream), the HGLOBAL given by the OS is not held
around for this to happen.
The stream is then returned through the task to the caller.

Either way, the caller-specified callback is invoked, and it gets the read end
of a stream by calling
C: gdk_clipboard_read_finish(),
then reads the data from the stream and unrefs the stream once it is done.
The local buffer that backed the stream is freed with the stream.
IRL applications use wrappers, which create an extra task that gets the
stream, reads from it asynchronously and turns the bytes that it reads into
some kind of application-specific object type. GDK comes pre-equipped with
functions that read arbitrary GTypes (as long as they are serializable),
texts (strings) or textures (GdkPixbufs) this way.

If data must be stored on the clipboard, because the application is quitting,
GTK will call
S: gdk_clipboard_store_async()
on all the clipboards it owns. This creates multiple write stream (one for each
format being stored), each backed by a HGLOBAL memory object. Once all memory
objects are written, the backend queues a store operation, passing along
all these HGLOBAL objects. The clipboard thread processes that by sending
WM_RENDERALLFORMATS to the window, then signals the task that it's done.

When clipboard owner changes, the old owner receives WM_DESTROYCLIPBOARD message,
the clipboard thread schedules a call to gdk_clipboard_claim_remote()
in the main thread, with an empty list of formats,
to indicate that the clipboard is now owned by a remote process.
Later the OS will send WM_CLIPBOARDUPDATE to indicate
the new clipboard contents (see above).

DND:
GDK-Win32:
DnD server runs in a separate thread, and schedules calls to be
made in the main thread in response to the DnD thread being invoked
by the OS (using OLE2 mechanism).
The DnD thread normally just idles, until the main thread tells it
to call DoDragDrop(), at which point it enters the DoDragDrop() call
(which means that its OLE2 DnD callbacks get invoked repeatedly by the OS
 in response to user actions), and doesn't leave it until the DnD
operation is finished.

Otherwise it's similar to how the clipboard works. Only the DnD server
(drag source) works in a thread. DnD client (drop target) works normally.
*/

#include "config.h"
#include <string.h>
#include <stdlib.h>

/* For C-style COM wrapper macros */
#define COBJMACROS

/* for CIDA */
#include <shlobj.h>

#include "gdkdebugprivate.h"
#include "gdkdisplay.h"
#include "gdkprivate-win32.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-win32.h"
#include "gdkclipdrop-win32.h"
#include "gdkhdataoutputstream-win32.h"
#include "gdkwin32dnd.h"
#include "gdkwin32dnd-private.h"
#include "gdkwin32.h"

#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdragprivate.h"
#include <glib/gi18n-lib.h>
#include "gdk/gdkprivate.h"

#define HIDA_GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define HIDA_GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])

#define CLIPBOARD_OPERATION_TIMEOUT (G_USEC_PER_SEC * 30)

/* GetClipboardData() times out after 30 seconds.
 * Try to reply (even if it's a no-action reply due to a timeout)
 * before that happens.
 */
#define CLIPBOARD_RENDER_TIMEOUT (G_USEC_PER_SEC * 29)

gboolean _gdk_win32_transmute_windows_data (UINT          from_w32format,
                                            const char   *to_contentformat,
                                            HANDLE        hdata,
                                            guchar      **set_data,
                                            gsize        *set_data_length);

/* Just to avoid calling RegisterWindowMessage() every time */
static UINT thread_wakeup_message;

typedef enum _GdkWin32ClipboardThreadQueueItemType GdkWin32ClipboardThreadQueueItemType;

enum _GdkWin32ClipboardThreadQueueItemType
{
  GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_ADVERTISE = 1,
  GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_RETRIEVE = 2,
  GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_STORE = 3,
};

typedef struct _GdkWin32ClipboardThreadQueueItem GdkWin32ClipboardThreadQueueItem;

struct _GdkWin32ClipboardThreadQueueItem
{
  GdkWin32ClipboardThreadQueueItemType  item_type;
  gint64                                start_time;
  gint64                                end_time;
  gpointer                              opaque_task;
};

typedef struct _GdkWin32ClipboardThreadAdvertise GdkWin32ClipboardThreadAdvertise;

struct _GdkWin32ClipboardThreadAdvertise
{
  GdkWin32ClipboardThreadQueueItem  parent;
  GArray                      *pairs; /* of GdkWin32ContentFormatPair */
  gboolean                     unset;
};

typedef struct _GdkWin32ClipboardThreadRetrieve GdkWin32ClipboardThreadRetrieve;

struct _GdkWin32ClipboardThreadRetrieve
{
  GdkWin32ClipboardThreadQueueItem  parent;
  GArray                      *pairs; /* of GdkWin32ContentFormatPair */
  gint64                       sequence_number;
};

typedef struct _GdkWin32ClipboardStorePrepElement GdkWin32ClipboardStorePrepElement;

struct _GdkWin32ClipboardStorePrepElement
{
  UINT           w32format;
  const char    *contentformat;
  HANDLE         handle;
  GOutputStream *stream;
};

typedef struct _GdkWin32ClipboardStorePrep GdkWin32ClipboardStorePrep;

struct _GdkWin32ClipboardStorePrep
{
  GTask             *store_task;
  GArray            *elements; /* of GdkWin32ClipboardStorePrepElement */
};

typedef struct _GdkWin32ClipboardThreadStore GdkWin32ClipboardThreadStore;

struct _GdkWin32ClipboardThreadStore
{
  GdkWin32ClipboardThreadQueueItem  parent;
  GArray                      *elements; /* of GdkWin32ClipboardStorePrepElement */
};

typedef struct _GdkWin32ClipboardThreadRender GdkWin32ClipboardThreadRender;

struct _GdkWin32ClipboardThreadRender
{
  /* The handle that the main thread prepares for us.
   * We just give it to SetClipboardData ().
   * NULL means that the rendering failed.
   */
  HANDLE                                main_thread_data_handle;

  /* The format that is being requested of us */
  GdkWin32ContentFormatPair             pair;
};

typedef struct _GdkWin32ClipboardThread GdkWin32ClipboardThread;

struct _GdkWin32ClipboardThread
{
  /* A hidden window that owns our clipboard
   * and receives clipboard-related messages.
   */
  HWND         clipboard_window;

  /* We receive instructions from the main thread in this queue */
  GAsyncQueue *input_queue;

  /* Last observer owner of the clipboard, as reported by the OS.
   * This is compared to GetClipboardOwner() return value to see
   * whether the owner changed.
   */
  HWND         stored_hwnd_owner;

  /* The last time we saw an owner change event.
   * Any requests made before this time are invalid and
   * fail automatically.
   */
  gint64       owner_change_time;

  /* The handle that was given to OpenClipboard().
   * NULL is a valid handle,
   * INVALID_HANDLE_VALUE means that the clipboard is closed.
   */
  HWND         clipboard_opened_for;

  /* We can't peek the queue or "unpop" queue items,
   * so the items that we can't act upon (yet) got
   * to be stored *somewhere*.
   */
  GList       *dequeued_items;

  /* Wakeup timer id (1 if timer is set, 0 otherwise) */
  UINT         wakeup_timer;

  /* The formats that the main thread claims to provide */
  GArray      *cached_advertisement; /* of GdkWin32ContentFormatPair */

  /* We receive rendered clipboard data in this queue.
   * Contains GdkWin32ClipboardThreadRender structs.
   */
  GAsyncQueue *render_queue; 

  /* Set to TRUE when we're calling EmptyClipboard () */
  gboolean     ignore_destroy_clipboard;
};

/* The code is much more secure if we don't rely on the OS to keep
 * this around for us.
 */
static GdkWin32ClipboardThread *clipboard_thread_data = NULL;

typedef struct _GdkWin32ClipboardThreadResponse GdkWin32ClipboardThreadResponse;

struct _GdkWin32ClipboardThreadResponse
{
  GdkWin32ClipboardThreadQueueItemType  item_type;
  GError                               *error;
  gpointer                              opaque_task;
  GInputStream                         *input_stream;
};

gboolean
_gdk_win32_format_uses_hdata (UINT w32format)
{
  switch (w32format)
    {
    case CF_DIB:
    case CF_DIBV5:
    case CF_DIF:
    case CF_DSPBITMAP:
    case CF_DSPENHMETAFILE:
    case CF_DSPMETAFILEPICT:
    case CF_DSPTEXT:
    case CF_OEMTEXT:
    case CF_RIFF:
    case CF_SYLK:
    case CF_TEXT:
    case CF_TIFF:
    case CF_UNICODETEXT:
    case CF_WAVE:
      return TRUE;
    default:
      if (w32format >= 0xC000)
        return TRUE;
      else
        return FALSE;
    }
}


/* This function is called in the main thread */
static gboolean
clipboard_window_created (gpointer user_data)
{
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  clipdrop->clipboard_window = (HWND) user_data;

  return G_SOURCE_REMOVE;
}

/* This function is called in the main thread */
static gboolean
clipboard_owner_changed (gpointer user_data)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);
  gdk_win32_clipboard_claim_remote (GDK_WIN32_CLIPBOARD (clipboard));

  return G_SOURCE_REMOVE;
}

typedef struct _GdkWin32ClipboardRenderAndStream GdkWin32ClipboardRenderAndStream;

struct _GdkWin32ClipboardRenderAndStream
{
  GdkWin32ClipboardThreadRender *render;
  GdkWin32HDataOutputStream *stream;
};

static void
clipboard_render_hdata_ready (GObject      *clipboard,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GError *error = NULL;
  GdkWin32ClipboardRenderAndStream render_and_stream = *(GdkWin32ClipboardRenderAndStream *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  g_free (user_data);

  if (!gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, &error))
    {
      HANDLE handle;
      gboolean is_hdata;
      GDK_NOTE(CLIPBOARD, g_printerr ("%p: failed to write HData-backed stream: %s\n", clipboard, error->message));
      g_error_free (error);
      g_output_stream_close (G_OUTPUT_STREAM (render_and_stream.stream), NULL, NULL);
      handle = gdk_win32_hdata_output_stream_get_handle (render_and_stream.stream, &is_hdata);

      if (is_hdata)
        API_CALL (GlobalFree, (handle));
      else
        API_CALL (CloseHandle, (handle));

      render_and_stream.render->main_thread_data_handle = NULL;
    }
  else
    {
      g_output_stream_close (G_OUTPUT_STREAM (render_and_stream.stream), NULL, NULL);
      render_and_stream.render->main_thread_data_handle = gdk_win32_hdata_output_stream_get_handle (render_and_stream.stream, NULL);
    }

  g_async_queue_push (clipdrop->clipboard_render_queue, render_and_stream.render);
  g_object_unref (render_and_stream.stream);
}

/* This function is called in the main thread */
static gboolean
clipboard_render (gpointer user_data)
{
  GdkWin32ClipboardThreadRender *render = (GdkWin32ClipboardThreadRender *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkDisplay *display = gdk_display_get_default ();
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);
  GError *error = NULL;
  GOutputStream *stream = gdk_win32_hdata_output_stream_new (&render->pair, &error);
  GdkWin32ClipboardRenderAndStream *render_and_stream;

  if (stream == NULL)
    {
      GDK_NOTE (SELECTION, g_printerr ("%p: failed create a HData-backed stream: %s\n", clipboard, error->message));
      g_error_free (error);
      render->main_thread_data_handle = NULL;
      g_async_queue_push (clipdrop->clipboard_render_queue, render);

      return G_SOURCE_REMOVE;
    }

  render_and_stream = g_new0 (GdkWin32ClipboardRenderAndStream, 1);
  render_and_stream->render = render;
  render_and_stream->stream = GDK_WIN32_HDATA_OUTPUT_STREAM (stream);

  gdk_clipboard_write_async (GDK_CLIPBOARD (clipboard),
                             render->pair.contentformat,
                             stream,
                             G_PRIORITY_DEFAULT,
                             NULL,
                             clipboard_render_hdata_ready,
                             render_and_stream);

  /* Keep our reference to the stream, don't unref it */

  return G_SOURCE_REMOVE;
}

/* This function is called in the main thread */
static gboolean
clipboard_thread_response (gpointer user_data)
{
  GdkWin32ClipboardThreadResponse *response = (GdkWin32ClipboardThreadResponse *) user_data;
  GTask *task = (GTask *) response->opaque_task;

  if (task != NULL)
    {
      if (response->error)
        g_task_return_error (task, response->error);
      else if (response->input_stream)
        g_task_return_pointer (task, response->input_stream, g_object_unref);
      else
        g_task_return_boolean (task, TRUE);

      g_object_unref (task);
    }

  g_free (response);

  return G_SOURCE_REMOVE;
}

static void
free_prep_element (GdkWin32ClipboardStorePrepElement *el)
{
  if (el->handle)
    {
      if (_gdk_win32_format_uses_hdata (el->w32format))
        GlobalFree (el->handle);
      else
        CloseHandle (el->handle);
    }

  if (el->stream)
    g_object_unref (el->stream);
}

static void
free_queue_item (GdkWin32ClipboardThreadQueueItem *item)
{
  GdkWin32ClipboardThreadAdvertise *adv;
  GdkWin32ClipboardThreadRetrieve *retr;
  GdkWin32ClipboardThreadStore    *store;
  int i;

  switch (item->item_type)
    {
    case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_ADVERTISE:
      adv = (GdkWin32ClipboardThreadAdvertise *) item;
      if (adv->pairs)
        g_array_free (adv->pairs, TRUE);
      break;
    case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_RETRIEVE:
      retr = (GdkWin32ClipboardThreadRetrieve *) item;
      if (retr->pairs)
        g_array_free (retr->pairs, TRUE);
      break;
    case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_STORE:
      store = (GdkWin32ClipboardThreadStore *) item;
      for (i = 0; i < store->elements->len; i++)
        {
          GdkWin32ClipboardStorePrepElement *el = &g_array_index (store->elements, GdkWin32ClipboardStorePrepElement, i);
          free_prep_element (el);
        }
      g_array_free (store->elements, TRUE);
      break;
    }

  g_free (item);
}

static void
send_response (GdkWin32ClipboardThreadQueueItemType  request_type,
               gpointer                              opaque_task,
               GError                               *error)
{
  GdkWin32ClipboardThreadResponse *response = g_new0 (GdkWin32ClipboardThreadResponse, 1);
  response->error = error;
  response->opaque_task = opaque_task;
  response->item_type = request_type;
  g_idle_add_full (G_PRIORITY_DEFAULT, clipboard_thread_response, response, NULL);
}

static void
send_input_stream (GdkWin32ClipboardThreadQueueItemType  request_type,
                   gpointer                              opaque_task,
                   GInputStream                         *stream)
{
  GdkWin32ClipboardThreadResponse *response = g_new0 (GdkWin32ClipboardThreadResponse, 1);
  response->input_stream = stream;
  response->opaque_task = opaque_task;
  response->item_type = request_type;
  g_idle_add_full (G_PRIORITY_DEFAULT, clipboard_thread_response, response, NULL);
}

static DWORD
try_open_clipboard (HWND hwnd)
{
  if (clipboard_thread_data->clipboard_opened_for == hwnd)
    return NO_ERROR;

  if (clipboard_thread_data->clipboard_opened_for != INVALID_HANDLE_VALUE)
    {
      API_CALL (CloseClipboard, ());
      clipboard_thread_data->clipboard_opened_for = INVALID_HANDLE_VALUE;
    }

  if (!OpenClipboard (hwnd))
    return GetLastError ();

  clipboard_thread_data->clipboard_opened_for = hwnd;

  return NO_ERROR;
}

static gboolean
process_advertise (GdkWin32ClipboardThreadAdvertise *adv)
{
  DWORD error_code;
  int i;

  if (g_get_monotonic_time () > adv->parent.end_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("An advertise task timed out\n"));
      send_response (adv->parent.item_type,
                     adv->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot claim clipboard ownership. OpenClipboard() timed out.")));
      return FALSE;
    }

  if (clipboard_thread_data->owner_change_time > adv->parent.start_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("An advertise task timed out due to ownership change\n"));
      send_response (adv->parent.item_type,
                     adv->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot claim clipboard ownership. Another process claimed it before us.")));
      return FALSE;
    }

  error_code = try_open_clipboard (adv->unset ? NULL : clipboard_thread_data->clipboard_window);

  if (error_code == ERROR_ACCESS_DENIED)
    return TRUE;

  if (G_UNLIKELY (error_code != NO_ERROR))
    {
      send_response (adv->parent.item_type,
                     adv->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot claim clipboard ownership. OpenClipboard() failed: 0x%lx."), error_code));
      return FALSE;
    }

  clipboard_thread_data->ignore_destroy_clipboard = TRUE;
  if (!EmptyClipboard ())
    {
      clipboard_thread_data->ignore_destroy_clipboard = FALSE;
      error_code = GetLastError ();
      send_response (adv->parent.item_type,
                     adv->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot claim clipboard ownership. EmptyClipboard() failed: 0x%lx."), error_code));
      return FALSE;
    }

  clipboard_thread_data->ignore_destroy_clipboard = FALSE;

  if (adv->unset)
    return FALSE;

  for (i = 0; i < adv->pairs->len; i++)
    {
      GdkWin32ContentFormatPair *pair = &g_array_index (adv->pairs, GdkWin32ContentFormatPair, i);

      SetClipboardData (pair->w32format, NULL);
    }

  if (clipboard_thread_data->cached_advertisement)
    g_array_free (clipboard_thread_data->cached_advertisement, TRUE);

  clipboard_thread_data->cached_advertisement = adv->pairs;

  /* To enure that we don't free it later on */
  adv->pairs = NULL;

  send_response (adv->parent.item_type,
                 adv->parent.opaque_task,
                 NULL);

  return FALSE;
}

static gboolean
process_store (GdkWin32ClipboardThreadStore *store)
{
  DWORD error_code;
  int i;

  if (g_get_monotonic_time () > store->parent.end_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("A store task timed out\n"));
      send_response (store->parent.item_type,
                     store->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot set clipboard data. OpenClipboard() timed out.")));
      return FALSE;
    }

  if (clipboard_thread_data->owner_change_time > store->parent.start_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("A store task timed out due to ownership change\n"));
      send_response (store->parent.item_type,
                     store->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot set clipboard data. Another process claimed clipboard ownership.")));
      return FALSE;
    }

  error_code = try_open_clipboard (clipboard_thread_data->clipboard_window);

  if (error_code == ERROR_ACCESS_DENIED)
    return TRUE;

  if (G_UNLIKELY (error_code != NO_ERROR))
    {
      send_response (store->parent.item_type,
                     store->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot set clipboard data. OpenClipboard() failed: 0x%lx."), error_code));
      return FALSE;
    }

  /* It's possible for another process to claim ownership
   * between between us entering this function and us opening the clipboard.
   * So check the ownership one last time.
   * Unlike the advertisement routine above, here we don't want to
   * claim clipboard ownership - we want to store stuff in the clipboard
   * that we already own, otherwise we're just killing stuff that some other
   * process put in there, which is not nice.
   */
  if (GetClipboardOwner () != clipboard_thread_data->clipboard_window)
    {
      send_response (store->parent.item_type,
                     store->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot set clipboard data. Another process claimed clipboard ownership.")));
      return FALSE;
    }

  for (i = 0; i < store->elements->len; i++)
    {
      GdkWin32ClipboardStorePrepElement *el = &g_array_index (store->elements, GdkWin32ClipboardStorePrepElement, i);
      if (el->handle != NULL && el->w32format != 0)
        if (SetClipboardData (el->w32format, el->handle))
          el->handle = NULL; /* the OS now owns the handle, don't free it later on */
    }

  send_response (store->parent.item_type,
                 store->parent.opaque_task,
                 NULL);

  return FALSE;
}

static gpointer
grab_data_from_hdata (GdkWin32ClipboardThreadRetrieve *retr,
                      HANDLE                           hdata,
                      gsize                           *data_len)
{
  gpointer ptr;
  SIZE_T length;
  guchar *data;

  ptr = GlobalLock (hdata);
  if (ptr == NULL)
    {
      DWORD error_code = GetLastError ();
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. GlobalLock(0x%p) failed: 0x%lx."), hdata, error_code));
      return NULL;
    }

  length = GlobalSize (hdata);
  if (length == 0 && GetLastError () != NO_ERROR)
    {
      DWORD error_code = GetLastError ();
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. GlobalSize(0x%p) failed: 0x%lx."), hdata, error_code));
      GlobalUnlock (hdata);
      return NULL;
    }

  data = g_try_malloc (length);

  if (data == NULL)
    {
      char *length_str = g_strdup_printf ("%" G_GSIZE_FORMAT, length);
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. Failed to allocate %s bytes to store the data."), length_str));
      g_free (length_str);
      GlobalUnlock (hdata);
      return NULL;
    }

  memcpy (data, ptr, length);
  *data_len = length;

  GlobalUnlock (hdata);

  return data;
}

static gboolean
process_retrieve (GdkWin32ClipboardThreadRetrieve *retr)
{
  DWORD error_code;
  int i;
  UINT fmt, fmt_to_use;
  HANDLE hdata;
  GdkWin32ContentFormatPair *pair;
  guchar *data;
  gsize   data_len;
  GInputStream *stream;

  if (g_get_monotonic_time () > retr->parent.end_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("A retrieve task timed out\n"));
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. OpenClipboard() timed out.")));
      return FALSE;
    }

  if (clipboard_thread_data->owner_change_time > retr->parent.start_time)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("A retrieve task timed out due to ownership change\n"));
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. Clipboard ownership changed.")));
      return FALSE;
    }

  if (GetClipboardSequenceNumber () > retr->sequence_number)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("A retrieve task timed out due to data change\n"));
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. Clipboard data changed before we could get it.")));
      return FALSE;
    }

  if (clipboard_thread_data->clipboard_opened_for == INVALID_HANDLE_VALUE)
    error_code = try_open_clipboard (clipboard_thread_data->clipboard_window);
  else
    error_code = try_open_clipboard (clipboard_thread_data->clipboard_opened_for);

  if (error_code == ERROR_ACCESS_DENIED)
    return TRUE;

  if (G_UNLIKELY (error_code != NO_ERROR))
    {
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. OpenClipboard() failed: 0x%lx."), error_code));
      return FALSE;
    }

  for (fmt_to_use = 0, pair = NULL, fmt = 0;
       fmt_to_use == 0 && 0 != (fmt = EnumClipboardFormats (fmt));
      )
    {
      for (i = 0; i < retr->pairs->len; i++)
        {
          pair = &g_array_index (retr->pairs, GdkWin32ContentFormatPair, i);

          if (pair->w32format != fmt)
            continue;

          fmt_to_use = fmt;
          break;
        }
    }

  if (!fmt_to_use)
    {
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. No compatible transfer format found.")));
      return FALSE;
    }

  if ((hdata = GetClipboardData (fmt_to_use)) == NULL)
    {
      error_code = GetLastError ();
      send_response (retr->parent.item_type,
                     retr->parent.opaque_task,
                     g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  _("Cannot get clipboard data. GetClipboardData() failed: 0x%lx."), error_code));
      return FALSE;
    }

  if (!pair->transmute)
    {
      if (_gdk_win32_format_uses_hdata (pair->w32format))
        {
          data = grab_data_from_hdata (retr, hdata, &data_len);

          if (data == NULL)
            return FALSE;
        }
      else
        {
          data_len = sizeof (HANDLE);
          data = g_malloc (data_len);
          memcpy (data, &hdata, data_len);
        }
    }
  else
    {
      _gdk_win32_transmute_windows_data (pair->w32format, pair->contentformat, hdata, &data, &data_len);

      if (data == NULL)
        return FALSE;
    }

  stream = g_memory_input_stream_new_from_data (data, data_len, g_free);
  g_object_set_data (G_OBJECT (stream), "gdk-clipboard-stream-contenttype", (gpointer) pair->contentformat);

  GDK_NOTE (CLIPBOARD, g_printerr ("reading clipboard data from a %" G_GSIZE_FORMAT "-byte buffer\n",
                                   data_len));
  send_input_stream (retr->parent.item_type,
                     retr->parent.opaque_task,
                     stream);

  return FALSE;
}

static gboolean
process_clipboard_queue ()
{
  GdkWin32ClipboardThreadQueueItem *placeholder;
  GList *p;
  gboolean try_again;
  GList *p_next;

  for (p = clipboard_thread_data->dequeued_items, p_next = NULL; p; p = p_next)
    {
      placeholder = (GdkWin32ClipboardThreadQueueItem *) p->data;
      p_next = p->next;

      switch (placeholder->item_type)
        {
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_ADVERTISE:
          try_again = process_advertise ((GdkWin32ClipboardThreadAdvertise *) placeholder);
          break;
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_RETRIEVE:
          try_again = process_retrieve ((GdkWin32ClipboardThreadRetrieve *) placeholder);
          break;
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_STORE:
          try_again = process_store ((GdkWin32ClipboardThreadStore *) placeholder);
          break;
        }

      if (try_again)
        return FALSE;

      clipboard_thread_data->dequeued_items = g_list_delete_link (clipboard_thread_data->dequeued_items, p);
      free_queue_item (placeholder);
    }

  while ((placeholder = g_async_queue_try_pop (clipboard_thread_data->input_queue)) != NULL)
    {
      switch (placeholder->item_type)
        {
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_ADVERTISE:
          try_again = process_advertise ((GdkWin32ClipboardThreadAdvertise *) placeholder);
          break;
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_RETRIEVE:
          try_again = process_retrieve ((GdkWin32ClipboardThreadRetrieve *) placeholder);
          break;
        case GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_STORE:
          try_again = process_store ((GdkWin32ClipboardThreadStore *) placeholder);
          break;
        }

      if (!try_again)
        {
          free_queue_item (placeholder);
          continue;
        }

      clipboard_thread_data->dequeued_items = g_list_append (clipboard_thread_data->dequeued_items, placeholder);

      return FALSE;
    }

  return TRUE;
}

static void
discard_render (GdkWin32ClipboardThreadRender *render,
                gboolean                       dont_touch_the_handle)
{
  GdkWin32ClipboardThreadRender render_copy = *render;

  g_free (render);

  if (dont_touch_the_handle || render_copy.main_thread_data_handle == NULL)
    return;

  if (_gdk_win32_format_uses_hdata (render_copy.pair.w32format))
    API_CALL (GlobalFree, (render_copy.main_thread_data_handle));
  else
    API_CALL (CloseHandle, (render_copy.main_thread_data_handle));
}

static LRESULT
inner_clipboard_window_procedure (HWND   hwnd,
                                  UINT   message,
                                  WPARAM wparam,
                                  LPARAM lparam)
{
  if (message == thread_wakeup_message ||
      message == WM_TIMER)
    {
      gboolean queue_is_empty = FALSE;

      if (clipboard_thread_data == NULL)
        {
          g_warning ("Clipboard thread got an actionable message with no thread data");
          return DefWindowProcW (hwnd, message, wparam, lparam);
        }

      queue_is_empty = process_clipboard_queue ();

      if (queue_is_empty && clipboard_thread_data->wakeup_timer)
        {
          API_CALL (KillTimer, (clipboard_thread_data->clipboard_window, clipboard_thread_data->wakeup_timer));
          clipboard_thread_data->wakeup_timer = 0;
        }

      /* Close the clipboard after each queue run, if it's open.
       * It would be wrong to keep it open, even if we would
       * need it again a second later.
       * queue_is_empty == FALSE implies that the clipboard
       * is closed already, but it's better to be sure.
       */
      if (clipboard_thread_data->clipboard_opened_for != INVALID_HANDLE_VALUE)
        {
          API_CALL (CloseClipboard, ());
          clipboard_thread_data->clipboard_opened_for = INVALID_HANDLE_VALUE;
        }

      if (queue_is_empty ||
          clipboard_thread_data->wakeup_timer != 0)
        return 0;

      if (SetTimer (clipboard_thread_data->clipboard_window, 1, 1000, NULL))
        clipboard_thread_data->wakeup_timer = 1;
      else
        g_critical ("Failed to set a timer for the clipboard window 0x%p: %lu",
                    clipboard_thread_data->clipboard_window,
                    GetLastError ());
    }

  switch (message)
    {
    case WM_DESTROY: /* unregister the clipboard listener */
      {
        if (clipboard_thread_data == NULL)
          {
            g_warning ("Clipboard thread got an actionable message with no thread data");
            return DefWindowProcW (hwnd, message, wparam, lparam);
          }

        RemoveClipboardFormatListener (hwnd);
        PostQuitMessage (0);
        return 0;
      }
    case WM_DESTROYCLIPBOARD:
      {
        return 0;
      }
    case WM_CLIPBOARDUPDATE:
      {
        HWND hwnd_owner;
        HWND hwnd_opener;
/*
        GdkEvent *event;
*/
        if (clipboard_thread_data == NULL)
          {
            g_warning ("Clipboard thread got an actionable message with no thread data");
            return DefWindowProcW (hwnd, message, wparam, lparam);
          }

        SetLastError (0);
        hwnd_owner = GetClipboardOwner ();
        if (hwnd_owner == NULL && GetLastError () != 0)
          WIN32_API_FAILED ("GetClipboardOwner");

        hwnd_opener = GetOpenClipboardWindow ();

        GDK_NOTE (DND, g_print (" drawclipboard owner: %p; opener %p ", hwnd_owner, hwnd_opener));

        if (GDK_DEBUG_CHECK (DND))
          {
            /* FIXME: grab and print clipboard formats without opening the clipboard
            if (clipboard_thread_data->clipboard_opened_for != INVALID_HANDLE_VALUE ||
                OpenClipboard (hwnd))
              {
                UINT nFormat = 0;

                while ((nFormat = EnumClipboardFormats (nFormat)) != 0)
                  g_print ("%s ", _gdk_win32_cf_to_string (nFormat));

                if (clipboard_thread_data->clipboard_opened_for == INVALID_HANDLE_VALUE)
                  CloseClipboard ();
              }
            else
              {
                WIN32_API_FAILED ("OpenClipboard");
              }
             */
          }

        GDK_NOTE (DND, g_print (" \n"));

        if (clipboard_thread_data->stored_hwnd_owner != hwnd_owner)
          {
            clipboard_thread_data->stored_hwnd_owner = hwnd_owner;
            clipboard_thread_data->owner_change_time = g_get_monotonic_time ();

            if (hwnd_owner != clipboard_thread_data->clipboard_window)
              {
                if (clipboard_thread_data->cached_advertisement)
                  g_array_free (clipboard_thread_data->cached_advertisement, TRUE);

                clipboard_thread_data->cached_advertisement = NULL;
              }

            API_CALL (PostMessage, (clipboard_thread_data->clipboard_window, thread_wakeup_message, 0, 0));

            if (hwnd_owner != clipboard_thread_data->clipboard_window)
              g_idle_add_full (G_PRIORITY_DEFAULT, clipboard_owner_changed, NULL, NULL);
          }

        /* clear error to avoid confusing SetClipboardViewer() return */
        SetLastError (0);

        return 0;
      }
    case WM_RENDERALLFORMATS:
      {
        if (clipboard_thread_data == NULL)
          {
            g_warning ("Clipboard thread got an actionable message with no thread data");
            return DefWindowProcW (hwnd, message, wparam, lparam);
          }

        if (clipboard_thread_data->cached_advertisement == NULL)
          return DefWindowProcW (hwnd, message, wparam, lparam);

        if (API_CALL (OpenClipboard, (hwnd)))
          {
            int i;
            GdkWin32ContentFormatPair *pair;

            for (pair = NULL, i = 0;
                 i < clipboard_thread_data->cached_advertisement->len;
                 i++)
              {
                pair = &g_array_index (clipboard_thread_data->cached_advertisement, GdkWin32ContentFormatPair, i);

                if (pair->w32format != 0)
                  SendMessage (hwnd, WM_RENDERFORMAT, pair->w32format, 0);
              }

            API_CALL (CloseClipboard, ());
          }

        return 0;
      }
    case WM_RENDERFORMAT:
      {
        int i;
        GdkWin32ClipboardThreadRender *render;
        GdkWin32ClipboardThreadRender *returned_render;
        GdkWin32ContentFormatPair *pair;


        GDK_NOTE (EVENTS, g_print (" %s", _gdk_win32_cf_to_string (wparam)));

        if (clipboard_thread_data == NULL)
          {
            g_warning ("Clipboard thread got an actionable message with no thread data");
            return DefWindowProcW (hwnd, message, wparam, lparam);
          }

        if (clipboard_thread_data->cached_advertisement == NULL)
          return DefWindowProcW (hwnd, message, wparam, lparam);

        for (pair = NULL, i = 0;
             i < clipboard_thread_data->cached_advertisement->len;
             i++)
          {
            pair = &g_array_index (clipboard_thread_data->cached_advertisement, GdkWin32ContentFormatPair, i);

            if (pair->w32format == wparam)
              break;

            pair = NULL;
          }

        if (pair == NULL)
          {
            GDK_NOTE (EVENTS, g_print (" (contentformat not found)"));
            return DefWindowProcW (hwnd, message, wparam, lparam);
          }

        /* Clear the queue */
        while ((render = g_async_queue_try_pop (clipboard_thread_data->render_queue)) != NULL)
          discard_render (render, FALSE);

        render = g_new0 (GdkWin32ClipboardThreadRender, 1);
        render->pair = *pair;
        g_idle_add_full (G_PRIORITY_DEFAULT, clipboard_render, render, NULL);
        returned_render = g_async_queue_timeout_pop (clipboard_thread_data->render_queue, CLIPBOARD_RENDER_TIMEOUT);

        /* We should get back the same pointer, ignore everything else. */
        while (returned_render != NULL && returned_render != render)
        {
          discard_render (returned_render, FALSE);
          /* Technically, we should use timed pop here as well,
           * as it's *possible* for a late render struct
           * to come down the queue just after we cleared
           * the queue, but before our idle function
           * triggered the actual render to be pushed.
           * If you get many "Clipboard rendering timed out" warnings,
           * this is probably why.
           */
          returned_render = g_async_queue_try_pop (clipboard_thread_data->render_queue);
        }

        /* Just in case */
        render = NULL;

        if (returned_render == NULL)
          {
            g_warning ("Clipboard rendering timed out");
          }
        else if (returned_render->main_thread_data_handle)
          {
            BOOL set_data_succeeded;
            /* The requester is holding the clipboard, no
             * OpenClipboard() is required/possible
             */
            GDK_NOTE (DND,
                      g_print (" SetClipboardData (%s, %p)\n",
                               _gdk_win32_cf_to_string (wparam),
                               returned_render->main_thread_data_handle));

            SetLastError (0);
            set_data_succeeded = (SetClipboardData (wparam, returned_render->main_thread_data_handle) != NULL);

            if (!set_data_succeeded)
              WIN32_API_FAILED ("SetClipboardData");

            discard_render (returned_render, set_data_succeeded);
          }

        return 0;
      }
    default:
      /* Otherwise call DefWindowProcW(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProcW"));
      return DefWindowProcW (hwnd, message, wparam, lparam);
    }
}

LRESULT CALLBACK
_clipboard_window_procedure (HWND   hwnd,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  LRESULT retval;

  GDK_NOTE (EVENTS, g_print ("clipboard thread %s %p",
			     _gdk_win32_message_to_string (message), hwnd));
  retval = inner_clipboard_window_procedure (hwnd, message, wparam, lparam);

  GDK_NOTE (EVENTS, g_print (" => %" G_GINT64_FORMAT "\n", (gint64) retval));

  return retval;
}

/*
 * Creates a hidden window and add a clipboard listener
 */
static gboolean
register_clipboard_notification ()
{
  WNDCLASS wclass = { 0, };
  ATOM klass;

  wclass.lpszClassName = "GdkClipboardNotification";
  wclass.lpfnWndProc = _clipboard_window_procedure;
  wclass.hInstance = this_module ();
  wclass.cbWndExtra = sizeof (GdkWin32ClipboardThread *);

  klass = RegisterClass (&wclass);
  if (!klass)
    return FALSE;

  clipboard_thread_data->clipboard_window = CreateWindow (MAKEINTRESOURCE (klass),
                                                NULL, WS_POPUP,
                                                0, 0, 0, 0, NULL, NULL,
                                                this_module (), NULL);

  if (clipboard_thread_data->clipboard_window == NULL)
    goto failed;

  SetLastError (0);

  if (AddClipboardFormatListener (clipboard_thread_data->clipboard_window) == FALSE)
    {
      DestroyWindow (clipboard_thread_data->clipboard_window);
      goto failed;
    }

  g_idle_add_full (G_PRIORITY_DEFAULT, clipboard_window_created, (gpointer) clipboard_thread_data->clipboard_window, NULL);

  return TRUE;

failed:
  g_critical ("Failed to install clipboard viewer");
  UnregisterClass (MAKEINTRESOURCE (klass), this_module ());
  return FALSE;
}

static gpointer
_gdk_win32_clipboard_thread_main (gpointer data)
{
  MSG msg;
  GAsyncQueue *queue = (GAsyncQueue *) data;
  GAsyncQueue *render_queue = (GAsyncQueue *) g_async_queue_pop (queue);

  g_assert (clipboard_thread_data == NULL);

  clipboard_thread_data = g_new0 (GdkWin32ClipboardThread, 1);
  clipboard_thread_data->input_queue = queue;
  clipboard_thread_data->render_queue = render_queue;
  clipboard_thread_data->clipboard_opened_for = INVALID_HANDLE_VALUE;

  if (!register_clipboard_notification ())
    {
      g_async_queue_unref (queue);
      g_clear_pointer (&clipboard_thread_data, g_free);

      return NULL;
    }

  while (GetMessage (&msg, NULL, 0, 0))
    {
      TranslateMessage (&msg); 
      DispatchMessage (&msg); 
    }

  /* Just in case, as this should only happen when we shut down */
  DestroyWindow (clipboard_thread_data->clipboard_window);
  CloseHandle (clipboard_thread_data->clipboard_window);
  g_async_queue_unref (queue);
  g_clear_pointer (&clipboard_thread_data, g_free);

  return NULL;
}

G_DEFINE_TYPE (GdkWin32Clipdrop, gdk_win32_clipdrop, G_TYPE_OBJECT)

static void
gdk_win32_clipdrop_class_init (GdkWin32ClipdropClass *klass)
{
}

void
_gdk_win32_clipdrop_init (void)
{
  _win32_main_thread = g_thread_self ();
  _win32_clipdrop = GDK_WIN32_CLIPDROP (g_object_new (GDK_TYPE_WIN32_CLIPDROP, NULL));
}

static void
gdk_win32_clipdrop_init (GdkWin32Clipdrop *win32_clipdrop)
{
  GArray             *atoms;
  GArray             *cfs;
  GSList             *pixbuf_formats;
  GSList             *rover;
  int                 i;
  GArray             *comp;
  GdkWin32ContentFormatPair fmt;
  HMODULE                   user32;

  thread_wakeup_message = RegisterWindowMessage ("GDK_WORKER_THREAD_WEAKEUP");

  user32 = LoadLibrary ("user32.dll");
  win32_clipdrop->GetUpdatedClipboardFormats = (GetUpdatedClipboardFormatsFunc) GetProcAddress (user32, "GetUpdatedClipboardFormats");
  FreeLibrary (user32);

  atoms = g_array_sized_new (FALSE, TRUE, sizeof (const char *), GDK_WIN32_ATOM_INDEX_LAST);
  g_array_set_size (atoms, GDK_WIN32_ATOM_INDEX_LAST);
  cfs = g_array_sized_new (FALSE, TRUE, sizeof (UINT), GDK_WIN32_CF_INDEX_LAST);
  g_array_set_size (cfs, GDK_WIN32_CF_INDEX_LAST);

  win32_clipdrop->known_atoms = atoms;
  win32_clipdrop->known_clipboard_formats = cfs;

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GDK_SELECTION) = g_intern_static_string ("GDK_SELECTION");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CLIPBOARD_MANAGER) = g_intern_static_string ("CLIPBOARD_MANAGER");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_WM_TRANSIENT_FOR) = g_intern_static_string ("WM_TRANSIENT_FOR");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TARGETS) = g_intern_static_string ("TARGETS");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_DELETE) = g_intern_static_string ("DELETE");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_SAVE_TARGETS) = g_intern_static_string ("SAVE_TARGETS");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8) = g_intern_static_string ("text/plain;charset=utf-8");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_PLAIN) = g_intern_static_string ("text/plain");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) = g_intern_static_string ("text/uri-list");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_HTML) = g_intern_static_string ("text/html");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG) = g_intern_static_string ("image/png");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) = g_intern_static_string ("image/jpeg");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP) = g_intern_static_string ("image/bmp");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF) = g_intern_static_string ("image/gif");

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION) = g_intern_static_string ("LocalDndSelection");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_DROPFILES_DND) = g_intern_static_string ("DROPFILES_DND");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_OLE2_DND) = g_intern_static_string ("OLE2_DND");

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_PNG)= g_intern_static_string ("PNG");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_JFIF) = g_intern_static_string ("JFIF");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GIF) = g_intern_static_string ("GIF");

  /* These are a bit unusual. It's here to allow GTK applications
   * to actually support CF_DIB and Shell ID List clipboard formats on their own,
   * instead of allowing GDK to use them internally for interoperability.
   */
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_DIB) = g_intern_static_string ("application/x.windows.CF_DIB");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST) = g_intern_static_string ("application/x.windows.Shell IDList Array");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT) = g_intern_static_string ("application/x.windows.CF_UNICODETEXT");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_TEXT) = g_intern_static_string ("application/x.windows.CF_TEXT");

  /* MS Office 2007, at least, offers images in common file formats
   * using clipboard format names like "PNG" and "JFIF". So we follow
   * the lead and map the GDK contentformat "image/png" to the clipboard
   * format name "PNG" etc.
   */
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG) = RegisterClipboardFormatA ("PNG");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF) = RegisterClipboardFormatA ("JFIF");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF) = RegisterClipboardFormatA ("GIF");

  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_UNIFORMRESOURCELOCATORW) = RegisterClipboardFormatA ("UniformResourceLocatorW");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST) = RegisterClipboardFormatA (CFSTR_SHELLIDLIST);
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_HTML_FORMAT) = RegisterClipboardFormatA ("HTML Format");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_HTML) = RegisterClipboardFormatA ("text/html");

  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_PNG) = RegisterClipboardFormatA ("image/png");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_JPEG) = RegisterClipboardFormatA ("image/jpeg");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_BMP) = RegisterClipboardFormatA ("image/bmp");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_GIF) = RegisterClipboardFormatA ("image/gif");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_URI_LIST) = RegisterClipboardFormatA ("text/uri-list");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_PLAIN_UTF8) = RegisterClipboardFormatA ("text/plain;charset=utf-8");

  win32_clipdrop->active_source_drags = g_hash_table_new_full (NULL, NULL, (GDestroyNotify) g_object_unref, NULL);

  pixbuf_formats = gdk_pixbuf_get_formats ();

  win32_clipdrop->n_known_pixbuf_formats = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      char **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);
      char **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	win32_clipdrop->n_known_pixbuf_formats++;
    }

  win32_clipdrop->known_pixbuf_formats = g_new (const char *, win32_clipdrop->n_known_pixbuf_formats);

  i = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      char **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);
      char **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	win32_clipdrop->known_pixbuf_formats[i++] = g_intern_string (*mime_type);
    }

  g_slist_free (pixbuf_formats);

  win32_clipdrop->compatibility_w32formats = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_array_unref);

  /* GTK actually has more text formats, but it's unlikely that we'd
   * get anything other than UTF8_STRING these days.
   * GTKTEXTBUFFERCONTENTS format can potentially be converted to
   * W32-compatible rich text format, but that's too complex to address right now.
   */
  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 3);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_PLAIN_UTF8);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = CF_UNICODETEXT;
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  fmt.w32format = CF_TEXT;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, (gpointer) fmt.contentformat, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_PNG);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, (gpointer) fmt.contentformat, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_JPEG);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, (gpointer) fmt.contentformat, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_GIF);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, (gpointer) fmt.contentformat, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_BMP);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = CF_DIB;
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, (gpointer) fmt.contentformat, comp);


/* Not implemented, but definitely possible
  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_URI_LIST);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_w32formats, fmt.contentformat, comp);
*/

  win32_clipdrop->compatibility_contentformats = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_array_unref);

  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = CF_TEXT;
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_TEXT);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (CF_TEXT), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = CF_UNICODETEXT;
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (CF_UNICODETEXT), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG);
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_PNG);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF);
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_JFIF);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF);
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GIF);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = CF_DIB;
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_DIB);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (CF_DIB), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair), 2);
  fmt.w32format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST);
  fmt.transmute = FALSE;

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST);
  g_array_append_val (comp, fmt);

  fmt.contentformat = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_clipdrop->compatibility_contentformats, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST)), comp);

  win32_clipdrop->clipboard_open_thread_queue = g_async_queue_new ();
  win32_clipdrop->clipboard_render_queue = g_async_queue_new ();
  /* Out of sheer laziness, we just push the extra queue through the
   * main queue, instead of allocating a struct with two queue
   * pointers and then passing *that* to the thread.
   */
  g_async_queue_push (win32_clipdrop->clipboard_open_thread_queue, g_async_queue_ref (win32_clipdrop->clipboard_render_queue));
  win32_clipdrop->clipboard_open_thread = g_thread_new ("GDK Win32 Clipboard Thread",
                                                        _gdk_win32_clipboard_thread_main,
                                                        g_async_queue_ref (win32_clipdrop->clipboard_open_thread_queue));

  win32_clipdrop->dnd_queue = g_async_queue_new ();
  win32_clipdrop->dnd_thread = g_thread_new ("GDK Win32 DnD Thread",
                                             _gdk_win32_dnd_thread_main,
                                             g_async_queue_ref (win32_clipdrop->dnd_queue));
  win32_clipdrop->dnd_thread_id = GPOINTER_TO_UINT (g_async_queue_pop (win32_clipdrop->dnd_queue));
}

#define CLIPBOARD_IDLE_ABORT_TIME 30

static const char *
predefined_name (UINT fmt)
{
#define CASE(fmt) case fmt: return #fmt
  switch (fmt)
    {
    CASE (CF_TEXT);
    CASE (CF_BITMAP);
    CASE (CF_METAFILEPICT);
    CASE (CF_SYLK);
    CASE (CF_DIF);
    CASE (CF_TIFF);
    CASE (CF_OEMTEXT);
    CASE (CF_DIB);
    CASE (CF_PALETTE);
    CASE (CF_PENDATA);
    CASE (CF_RIFF);
    CASE (CF_WAVE);
    CASE (CF_UNICODETEXT);
    CASE (CF_ENHMETAFILE);
    CASE (CF_HDROP);
    CASE (CF_LOCALE);
    CASE (CF_DIBV5);
    CASE (CF_MAX);

    CASE (CF_OWNERDISPLAY);
    CASE (CF_DSPTEXT);
    CASE (CF_DSPBITMAP);
    CASE (CF_DSPMETAFILEPICT);
    CASE (CF_DSPENHMETAFILE);
#undef CASE
    default:
      return NULL;
    }
}

char *
_gdk_win32_get_clipboard_format_name (UINT      fmt,
                                      gboolean *is_predefined)
{
  int registered_name_w_len = 1024;
  wchar_t *registered_name_w = g_malloc (registered_name_w_len);
  char *registered_name = NULL;
  int gcfn_result;
  const char *predef = predefined_name (fmt);

  /* TODO: cache the result in a hash table */

  do
    {
      gcfn_result = GetClipboardFormatNameW (fmt, registered_name_w, registered_name_w_len);

      if (gcfn_result > 0 && gcfn_result < registered_name_w_len)
        {
          registered_name = g_utf16_to_utf8 (registered_name_w, -1, NULL, NULL, NULL);
          g_clear_pointer (&registered_name_w, g_free);
          if (!registered_name)
            gcfn_result = 0;
          else
            *is_predefined = FALSE;
          break;
        }

      /* If GetClipboardFormatNameW() used up all the space, it means that
       * we probably need a bigger buffer, but cap this at 1 megabyte.
       */
      if (gcfn_result == 0 || registered_name_w_len > 1024 * 1024)
        {
          gcfn_result = 0;
          g_clear_pointer (&registered_name_w, g_free);
          break;
        }

      registered_name_w_len *= 2;
      registered_name_w = g_realloc (registered_name_w, registered_name_w_len);
      gcfn_result = registered_name_w_len;
    } while (gcfn_result == registered_name_w_len);

  if (registered_name == NULL &&
      predef != NULL)
    {
      registered_name = g_strdup (predef);
      *is_predefined = TRUE;
    }

  return registered_name;
}

/* This turns an arbitrary string into a string that
 * *looks* like a mime/type, such as:
 * "application/x.windows.FOO_BAR" from "FOO_BAR".
 * Does nothing for strings that already look like a mime/type
 * (no spaces, one slash, with at least one char on each side of the slash).
 */
const char *
_gdk_win32_get_clipboard_format_name_as_interned_mimetype (char *w32format_name)
{
  char *mime_type;
  const char *result;
  char *space = strchr (w32format_name, ' ');
  char *slash = strchr (w32format_name, '/');

  if (space == NULL &&
      slash > w32format_name &&
      slash[1] != '\0' &&
      strchr (&slash[1], '/') == NULL)
    return g_intern_string (w32format_name);

  mime_type = g_strdup_printf ("application/x.windows.%s", w32format_name);
  result = g_intern_string (mime_type);
  g_free (mime_type);

  return result;
}

static GArray *
get_compatibility_w32formats_for_contentformat (const char *contentformat)
{
  GArray *result = NULL;
  int i;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  result = g_hash_table_lookup (clipdrop->compatibility_w32formats, contentformat);

  if (result != NULL)
    return result;

  for (i = 0; i < clipdrop->n_known_pixbuf_formats; i++)
    {
      if (contentformat != clipdrop->known_pixbuf_formats[i])
        continue;

      /* Any format known to gdk-pixbuf can be presented as PNG or BMP */
      result = g_hash_table_lookup (clipdrop->compatibility_w32formats,
                                    _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG));
      break;
    }

  return result;
}

static GArray *
_gdk_win32_get_compatibility_contentformats_for_w32format (UINT w32format)
{
  GArray *result = NULL;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  result = g_hash_table_lookup (clipdrop->compatibility_contentformats, GINT_TO_POINTER (w32format));

  if (result != NULL)
    return result;

  /* TODO: reverse gdk-pixbuf conversion? We have to somehow
   * match gdk-pixbuf format names to the corresponding clipboard
   * format names. The former are known only at runtime,
   * the latter are presently unknown...
   * Maybe try to get the data and then just feed it to gdk-pixbuf,
   * see if it knows what it is?
   */

  return result;
}

/* Turn W32 format into a GDK content format and add it
 * to the array of W32 format <-> GDK content format pairs
 * and/or to a GDK contentformat builder.
 * Also add compatibility GDK content formats for that W32 format.
 * The added content format string is always interned.
 * Ensures that duplicates are not added to the pairs array
 * (builder already takes care of that for itself).
 */
void
_gdk_win32_add_w32format_to_pairs (UINT                      w32format,
                                   GArray                   *pairs,
                                   GdkContentFormatsBuilder *builder)
{
  gboolean predef;
  char *w32format_name = _gdk_win32_get_clipboard_format_name (w32format, &predef);
  const char *interned_w32format_name;
  GdkWin32ContentFormatPair pair;
  GArray *comp_pairs;
  int i, j;

  if (w32format_name != NULL)
    {
      interned_w32format_name = _gdk_win32_get_clipboard_format_name_as_interned_mimetype (w32format_name);
      GDK_NOTE (DND, g_print ("Maybe add as-is format %s (%s) (0x%p)\n", w32format_name, interned_w32format_name, interned_w32format_name));
      g_free (w32format_name);

      if (pairs && interned_w32format_name != 0)
        {
          for (j = 0; j < pairs->len; j++)
            if (g_array_index (pairs, GdkWin32ContentFormatPair, j).contentformat == interned_w32format_name)
              break;
          if (j == pairs->len)
            {
              pair.w32format = w32format;
              pair.contentformat = interned_w32format_name;
              pair.transmute = FALSE;
              g_array_append_val (pairs, pair);
            }
        }
      if (builder != NULL && interned_w32format_name != 0)
        gdk_content_formats_builder_add_mime_type (builder, interned_w32format_name);
    }

  comp_pairs = _gdk_win32_get_compatibility_contentformats_for_w32format (w32format);

 if (pairs != NULL && comp_pairs != NULL)
   for (i = 0; i < comp_pairs->len; i++)
     {
       pair = g_array_index (comp_pairs, GdkWin32ContentFormatPair, i);

       for (j = 0; j < pairs->len; j++)
         if (g_array_index (pairs, GdkWin32ContentFormatPair, j).contentformat == pair.contentformat &&
             g_array_index (pairs, GdkWin32ContentFormatPair, j).w32format == pair.w32format)
           break;

       if (j == pairs->len)
         g_array_append_val (pairs, pair);
     }

 if (builder != NULL && comp_pairs != NULL)
   for (i = 0; i < comp_pairs->len; i++)
     {
       pair = g_array_index (comp_pairs, GdkWin32ContentFormatPair, i);

       gdk_content_formats_builder_add_mime_type (builder, pair.contentformat);
     }
}

static void
transmute_cf_unicodetext_to_utf8_string (const guchar    *data,
                                         gsize            length,
                                         guchar         **set_data,
                                         gsize           *set_data_length,
                                         GDestroyNotify  *set_data_destroy)
{
  wchar_t *ptr, *p, *q, *endp;
  char *result;
  glong wclen, u8_len;

  /* Replace CR and CR+LF with LF */
  ptr = (wchar_t *) data;
  p = ptr;
  q = ptr;
  endp = ptr + length / 2;
  wclen = 0;

  while (p < endp)
    {
      if (*p != L'\r')
        {
          *q++ = *p;
          wclen++;
        }
      else if (p + 1 >= endp || *(p + 1) != L'\n')
        {
          *q++ = L'\n';
          wclen++;
        }

      p++;
    }

  result = g_utf16_to_utf8 (ptr, wclen, NULL, &u8_len, NULL);

  if (result)
    {
      *set_data = (guchar *) result;

      if (set_data_length)
        *set_data_length = u8_len + 1;
      if (set_data_destroy)
        *set_data_destroy = (GDestroyNotify) g_free;
    }
}

static void
transmute_utf8_string_to_cf_unicodetext (const guchar    *data,
                                         gsize            length,
                                         guchar         **set_data,
                                         gsize           *set_data_length,
                                         GDestroyNotify  *set_data_destroy)
{
  glong wclen;
  GError *err = NULL;
  glong size;
  int i;
  wchar_t *wcptr, *p;

  wcptr = g_utf8_to_utf16 ((char *) data, length, NULL, &wclen, &err);
  if (err != NULL)
    {
      g_warning ("Failed to convert utf8: %s", err->message);
      g_clear_error (&err);
      return;
    }

  wclen++; /* Terminating 0 */
  size = wclen * 2;
  for (i = 0; i < wclen; i++)
    if (wcptr[i] == L'\n' && (i == 0 || wcptr[i - 1] != L'\r'))
      size += 2;

  *set_data = g_malloc0 (size);
  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = (GDestroyNotify) g_free;

  p = (wchar_t *) *set_data;

  for (i = 0; i < wclen; i++)
    {
      if (wcptr[i] == L'\n' && (i == 0 || wcptr[i - 1] != L'\r'))
	*p++ = L'\r';
      *p++ = wcptr[i];
    }

  g_free (wcptr);
}

static int
wchar_to_str (const wchar_t  *wstr,
              char         **retstr,
              UINT            cp)
{
  char *str;
  int   len;
  int   lenc;

  len = WideCharToMultiByte (cp, 0, wstr, -1, NULL, 0, NULL, NULL);

  if (len <= 0)
    return -1;

  str = g_malloc (sizeof (char) * len);

  lenc = WideCharToMultiByte (cp, 0, wstr, -1, str, len, NULL, NULL);

  if (lenc != len)
    {
      g_free (str);

      return -3;
    }

  *retstr = str;

  return 0;
}

static void
transmute_utf8_string_to_cf_text (const guchar    *data,
                                  gsize            length,
                                  guchar         **set_data,
                                  gsize           *set_data_length,
                                  GDestroyNotify  *set_data_destroy)
{
  glong rlen;
  GError *err = NULL;
  gsize size;
  int i;
  char *strptr, *p;
  wchar_t *wcptr;

  wcptr = g_utf8_to_utf16 ((char *) data, length, NULL, NULL, &err);
  if (err != NULL)
    {
      g_warning ("Failed to convert utf8: %s", err->message);
      g_clear_error (&err);
      return;
    }

  if (wchar_to_str (wcptr, &strptr, CP_ACP) != 0)
    {
      g_warning ("Failed to convert utf-16 %S to ACP", wcptr);
      g_free (wcptr);
      return;
    }

  rlen = strlen (strptr);
  g_free (wcptr);

  rlen++; /* Terminating 0 */
  size = rlen * sizeof (char);
  for (i = 0; i < rlen; i++)
    if (strptr[i] == '\n' && (i == 0 || strptr[i - 1] != '\r'))
      size += sizeof (char);

  *set_data = g_malloc0 (size);
  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = (GDestroyNotify) g_free;

  p = (char *) *set_data;

  for (i = 0; i < rlen; i++)
    {
      if (strptr[i] == '\n' && (i == 0 || strptr[i - 1] != '\r'))
	*p++ = '\r';
      *p++ = strptr[i];
    }

  g_free (strptr);
}

static int
str_to_wchar (const char  *str,
              wchar_t    **wretstr,
              UINT         cp)
{
  wchar_t *wstr;
  int      len;
  int      lenc;

  len = MultiByteToWideChar (cp, 0, str, -1, NULL, 0);

  if (len <= 0)
    return -1;

  wstr = g_malloc (sizeof (wchar_t) * len);

  lenc = MultiByteToWideChar (cp, 0, str, -1, wstr, len);

  if (lenc != len)
    {
      g_free (wstr);

      return -3;
    }

  *wretstr = wstr;

  return 0;
}

static void
transmute_cf_text_to_utf8_string (const guchar    *data,
                                  gsize            length,
                                  guchar         **set_data,
                                  gsize           *set_data_length,
                                  GDestroyNotify  *set_data_destroy)
{
  char *ptr, *p, *q, *endp;
  char *result;
  glong wclen, u8_len;
  wchar_t *wstr;

  /* Strip out \r */
  ptr = (char *) data;
  p = ptr;
  q = ptr;
  endp = ptr + length / 2;
  wclen = 0;

  while (p < endp)
    {
      if (*p != '\r')
        {
          *q++ = *p;
          wclen++;
        }
      else if (p + 1 > endp || *(p + 1) != '\n')
        {
          *q++ = '\n';
          wclen++;
        }

      p++;
    }

  if (str_to_wchar (ptr, &wstr, CP_ACP) < 0)
    return;

  result = g_utf16_to_utf8 (wstr, -1, NULL, &u8_len, NULL);

  if (result)
    {
      *set_data = (guchar *) result;

      if (set_data_length)
        *set_data_length = u8_len + 1;
      if (set_data_destroy)
        *set_data_destroy = (GDestroyNotify) g_free;
    }

  g_free (wstr);
}

static void
transmute_cf_dib_to_image_bmp (const guchar    *data,
                               gsize            length,
                               guchar         **set_data,
                               gsize           *set_data_length,
                               GDestroyNotify  *set_data_destroy)
{
  /* Need to add a BMP file header so gdk-pixbuf can load
   * it.
   *
   * If the data is from Mozilla Firefox or IE7, and
   * starts with an "old fashioned" BITMAPINFOHEADER,
   * i.e. with biSize==40, and biCompression == BI_RGB and
   * biBitCount==32, we assume that the "extra" byte in
   * each pixel in fact is alpha.
   *
   * The gdk-pixbuf bmp loader doesn't trust 32-bit BI_RGB
   * bitmaps to in fact have alpha, so we have to convince
   * it by changing the bitmap header to a version 5
   * BI_BITFIELDS one with explicit alpha mask indicated.
   *
   * The RGB bytes that are in bitmaps on the clipboard
   * originating from Firefox or IE7 seem to be
   * premultiplied with alpha. The gdk-pixbuf bmp loader
   * of course doesn't expect that, so we have to undo the
   * premultiplication before feeding the bitmap to the
   * bmp loader.
   *
   * Note that for some reason the bmp loader used to want
   * the alpha bytes in its input to actually be
   * 255-alpha, but here we assume that this has been
   * fixed before this is committed.
   */
  BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *) data;
  BITMAPFILEHEADER *bf;
  gpointer result;
  gsize data_length = length;
  gsize new_length;
  gboolean make_dibv5 = FALSE;
  BITMAPV5HEADER *bV5;
  guchar *p;
  guint i;

  if (bi->biSize == sizeof (BITMAPINFOHEADER) &&
      bi->biPlanes == 1 &&
      bi->biBitCount == 32 &&
      bi->biCompression == BI_RGB &&
#if 0
      /* Maybe check explicitly for Mozilla or IE7?
       *
       * If the clipboard format
       * application/x-moz-nativeimage is present, that is
       * a reliable indicator that the data is offered by
       * Mozilla one would think. For IE7,
       * UniformResourceLocatorW is presumably not that
       * uniqie, so probably need to do some
       * GetClipboardOwner(), GetWindowThreadProcessId(),
       * OpenProcess(), GetModuleFileNameEx() dance to
       * check?
       */
      (IsClipboardFormatAvailable
       (RegisterClipboardFormatA ("application/x-moz-nativeimage")) ||
       IsClipboardFormatAvailable
       (RegisterClipboardFormatA ("UniformResourceLocatorW"))) &&
#endif
      TRUE)
    {
      /* We turn the BITMAPINFOHEADER into a
       * BITMAPV5HEADER before feeding it to gdk-pixbuf.
       */
      new_length = (data_length +
		    sizeof (BITMAPFILEHEADER) +
		    (sizeof (BITMAPV5HEADER) - sizeof (BITMAPINFOHEADER)));
      make_dibv5 = TRUE;
    }
  else
    {
      new_length = data_length + sizeof (BITMAPFILEHEADER);
    }

  result = g_try_malloc (new_length);

  if (result == NULL)
    return;

  bf = (BITMAPFILEHEADER *) result;
  bf->bfType = 0x4d42; /* "BM" */
  bf->bfSize = new_length;
  bf->bfReserved1 = 0;
  bf->bfReserved2 = 0;

  *set_data = result;

  if (set_data_length)
    *set_data_length = new_length;
  if (set_data_destroy)
    *set_data_destroy = g_free;

  if (!make_dibv5)
    {
      bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
		       bi->biSize +
		       bi->biClrUsed * sizeof (RGBQUAD));

      if (bi->biCompression == BI_BITFIELDS && bi->biBitCount >= 16)
        {
          /* Screenshots taken with PrintScreen or
           * Alt + PrintScreen are found on the clipboard in
           * this format. In this case the BITMAPINFOHEADER is
           * followed by three DWORD specifying the masks of the
           * red green and blue components, so adjust the offset
           * accordingly. */
          bf->bfOffBits += (3 * sizeof (DWORD));
        }

      memcpy ((char *) result + sizeof (BITMAPFILEHEADER),
	      bi,
	      data_length);

      return;
    }

  bV5 = (BITMAPV5HEADER *) ((char *) result + sizeof (BITMAPFILEHEADER));

  bV5->bV5Size = sizeof (BITMAPV5HEADER);
  bV5->bV5Width = bi->biWidth;
  bV5->bV5Height = bi->biHeight;
  bV5->bV5Planes = 1;
  bV5->bV5BitCount = 32;
  bV5->bV5Compression = BI_BITFIELDS;
  bV5->bV5SizeImage = 4 * bV5->bV5Width * ABS (bV5->bV5Height);
  bV5->bV5XPelsPerMeter = bi->biXPelsPerMeter;
  bV5->bV5YPelsPerMeter = bi->biYPelsPerMeter;
  bV5->bV5ClrUsed = 0;
  bV5->bV5ClrImportant = 0;
  /* Now the added mask fields */
  bV5->bV5RedMask   = 0x00ff0000;
  bV5->bV5GreenMask = 0x0000ff00;
  bV5->bV5BlueMask  = 0x000000ff;
  bV5->bV5AlphaMask = 0xff000000;
  ((char *) &bV5->bV5CSType)[3] = 's';
  ((char *) &bV5->bV5CSType)[2] = 'R';
  ((char *) &bV5->bV5CSType)[1] = 'G';
  ((char *) &bV5->bV5CSType)[0] = 'B';
  /* Ignore colorspace and profile fields */
  bV5->bV5Intent = LCS_GM_GRAPHICS;
  bV5->bV5Reserved = 0;

  bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
		   bV5->bV5Size);

  p = ((guchar *) result) + sizeof (BITMAPFILEHEADER) + sizeof (BITMAPV5HEADER);
  memcpy (p, ((char *) bi) + bi->biSize,
          data_length - sizeof (BITMAPINFOHEADER));

  for (i = 0; i < bV5->bV5SizeImage / 4; i++)
    {
      if (p[3] != 0)
        {
          double inverse_alpha = 255. / p[3];

          p[0] = p[0] * inverse_alpha + 0.5;
          p[1] = p[1] * inverse_alpha + 0.5;
          p[2] = p[2] * inverse_alpha + 0.5;
        }

      p += 4;
    }
}

static void
transmute_cf_shell_id_list_to_text_uri_list (const guchar    *data,
                                             gsize            length,
                                             guchar         **set_data,
                                             gsize           *set_data_length,
                                             GDestroyNotify  *set_data_destroy)
{
  guint i;
  CIDA *cida = (CIDA *) data;
  guint number_of_ids = cida->cidl;
  GString *result = g_string_new (NULL);
  PCIDLIST_ABSOLUTE folder_id = HIDA_GetPIDLFolder (cida);
  wchar_t path_w[MAX_PATH + 1];

  for (i = 0; i < number_of_ids; i++)
    {
      PCUIDLIST_RELATIVE file_id = HIDA_GetPIDLItem (cida, i);
      PIDLIST_ABSOLUTE file_id_full = ILCombine (folder_id, file_id);

      if (SHGetPathFromIDListW (file_id_full, path_w))
        {
          char *filename = g_utf16_to_utf8 (path_w, -1, NULL, NULL, NULL);

          if (filename)
            {
              char *uri = g_filename_to_uri (filename, NULL, NULL);

              if (uri)
                {
                  g_string_append (result, uri);
                  g_string_append (result, "\r\n");
                  g_free (uri);
                }

              g_free (filename);
            }
        }

      ILFree (file_id_full);
    }

  if (set_data_length)
    *set_data_length = result->len;
  *set_data = (guchar *) g_string_free (result, FALSE);
  if (set_data_destroy)
    *set_data_destroy = g_free;
}

void
transmute_image_bmp_to_cf_dib (const guchar    *data,
                               gsize            length,
                               guchar         **set_data,
                               gsize           *set_data_length,
                               GDestroyNotify  *set_data_destroy)
{
  gsize size;
  guchar *ptr;

  g_return_if_fail (length >= sizeof (BITMAPFILEHEADER));

  /* No conversion is needed, just strip the BITMAPFILEHEADER */
  size = length - sizeof (BITMAPFILEHEADER);
  ptr = g_malloc (size);

  memcpy (ptr, data + sizeof (BITMAPFILEHEADER), size);

  *set_data = ptr;

  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = g_free;
}

gboolean
_gdk_win32_transmute_windows_data (UINT          from_w32format,
                                   const char   *to_contentformat,
                                   HANDLE        hdata,
                                   guchar      **set_data,
                                   gsize        *set_data_length)
{
  const guchar *data;
  SIZE_T hdata_length;
  gsize length;
  gboolean res = FALSE;

  /* FIXME: error reporting */

  if ((data = GlobalLock (hdata)) == NULL)
    {
      return FALSE;
    }

  hdata_length = GlobalSize (hdata);
  if (hdata_length > G_MAXSIZE)
    goto out;

  length = (gsize) hdata_length;

  if ((to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG) &&
       from_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_PNG)) ||
      (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) &&
       from_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_JFIF)) ||
      (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_GIF) &&
       from_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_GIF)))
    {
      /* No transmutation needed */
      *set_data = g_memdup2 (data, length);
      *set_data_length = length;
    }
  else if (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8) &&
           from_w32format == CF_UNICODETEXT)
    {
      transmute_cf_unicodetext_to_utf8_string (data, length, set_data, set_data_length, NULL);
      res = TRUE;
    }
  else if (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8) &&
           from_w32format == CF_TEXT)
    {
      transmute_cf_text_to_utf8_string (data, length, set_data, set_data_length, NULL);
      res = TRUE;
    }
  else if (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           (from_w32format == CF_DIB || from_w32format == CF_DIBV5))
    {
      transmute_cf_dib_to_image_bmp (data, length, set_data, set_data_length, NULL);
      res = TRUE;
    }
  else if (to_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) &&
           from_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST))
    {
      transmute_cf_shell_id_list_to_text_uri_list (data, length, set_data, set_data_length, NULL);
      res = TRUE;
    }
  else
    {
      g_warning ("Don't know how to transmute W32 format 0x%x to content format 0x%p (%s)",
                 from_w32format, to_contentformat, to_contentformat);
      goto out;
    }

out:
  GlobalUnlock (hdata);

  return res;
}

gboolean
_gdk_win32_transmute_contentformat (const char    *from_contentformat,
                                    UINT           to_w32format,
                                    const guchar  *data,
                                    gsize          length,
                                    guchar       **set_data,
                                    gsize         *set_data_length)
{
  if ((from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG) &&
       to_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_PNG)) ||
      (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) &&
       to_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_JFIF)) ||
      (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_GIF) &&
       to_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_GIF)))
    {
      /* No conversion needed */
      *set_data = g_memdup2 (data, length);
      *set_data_length = length;
    }
  else if (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8) &&
           to_w32format == CF_UNICODETEXT)
    {
      transmute_utf8_string_to_cf_unicodetext (data, length, set_data, set_data_length, NULL);
    }
  else if (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8) &&
           to_w32format == CF_TEXT)
    {
      transmute_utf8_string_to_cf_text (data, length, set_data, set_data_length, NULL);
    }
  else if (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           to_w32format == CF_DIB)
    {
      transmute_image_bmp_to_cf_dib (data, length, set_data, set_data_length, NULL);
    }
  else if (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           to_w32format == CF_DIBV5)
    {
      transmute_image_bmp_to_cf_dib (data, length, set_data, set_data_length, NULL);
    }
/*
  else if (from_contentformat == _gdk_win32_clipdrop_atom (GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) &&
           to_w32format == _gdk_win32_clipdrop_cf (GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST))
    {
      transmute_text_uri_list_to_shell_id_list (data, length, set_data, set_data_length, NULL);
    }
*/
  else
    {
      g_warning ("Don't know how to transmute from target 0x%p to format 0x%x", from_contentformat, to_w32format);

      return FALSE;
    }

  return TRUE;
}

int
_gdk_win32_add_contentformat_to_pairs (const char *contentformat,
                                       GArray     *array)
{
  int added_count = 0;
  wchar_t *contentformat_w;
  GdkWin32ContentFormatPair fmt;
  int i;
  GArray *comp_pairs;
  int starting_point;
  const wchar_t *prefix = L"application/x.windows.";
  size_t prefix_len = wcslen (prefix);
  size_t offset = 0;

  for (i = 0; i < array->len; i++)
    if (g_array_index (array, GdkWin32ContentFormatPair, i).contentformat == contentformat)
      break;

  /* Don't put duplicates into the array */
  if (i < array->len)
    return added_count;

  /* Only check the newly-added pairs for duplicates,
   * all the ones that exist right now have different targets.
   */
  starting_point = array->len;

  contentformat_w = g_utf8_to_utf16 (contentformat, -1, NULL, NULL, NULL);

  if (contentformat_w == NULL)
    return added_count;

  if (wcsnicmp (contentformat_w, prefix, prefix_len) == 0)
    offset = prefix_len;
  else
    offset = 0;

  fmt.w32format = RegisterClipboardFormatW (&contentformat_w[offset]);
  GDK_NOTE (DND, g_print ("Registered clipboard format %S as 0x%x\n", &contentformat_w[offset], fmt.w32format));
  g_free (contentformat_w);
  fmt.contentformat = contentformat;
  fmt.transmute = FALSE;

  /* Add the "as-is" pair */
  g_array_append_val (array, fmt);
  added_count += 1;

  comp_pairs = get_compatibility_w32formats_for_contentformat (contentformat);
  for (i = 0; comp_pairs != NULL && i < comp_pairs->len; i++)
    {
      int j;

      fmt = g_array_index (comp_pairs, GdkWin32ContentFormatPair, i);

      /* Don't put duplicates into the array */
      for (j = starting_point; j < array->len; j++)
        if (g_array_index (array, GdkWin32ContentFormatPair, j).w32format == fmt.w32format)
          break;

      if (j < array->len)
        continue;

      /* Add a compatibility pair */
      g_array_append_val (array, fmt);
      added_count += 1;
    }

  return added_count;
}

void
_gdk_win32_advertise_clipboard_contentformats (GTask             *task,
                                               GdkContentFormats *contentformats)
{
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkWin32ClipboardThreadAdvertise *adv = g_new0 (GdkWin32ClipboardThreadAdvertise, 1);
  const char * const *mime_types;
  gsize mime_types_len;
  gsize i;

  g_assert (clipdrop->clipboard_window != NULL);

  adv->parent.item_type = GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_ADVERTISE;
  adv->parent.start_time = g_get_monotonic_time ();
  adv->parent.end_time = adv->parent.start_time + CLIPBOARD_OPERATION_TIMEOUT;
  adv->parent.opaque_task = task;

  if (contentformats == NULL)
    {
      adv->unset = TRUE;
    }
  else
    {
      adv->unset = FALSE;

      adv->pairs = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));
      mime_types = gdk_content_formats_get_mime_types (contentformats, &mime_types_len);

      for (i = 0; i < mime_types_len; i++)
        _gdk_win32_add_contentformat_to_pairs (mime_types[i], adv->pairs);
    }

  g_async_queue_push (clipdrop->clipboard_open_thread_queue, adv);
  API_CALL (PostMessage, (clipdrop->clipboard_window, thread_wakeup_message, 0, 0));

  return;
}

void
_gdk_win32_retrieve_clipboard_contentformats (GTask             *task,
                                              GdkContentFormats *contentformats)
{
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkWin32ClipboardThreadRetrieve *retr = g_new0 (GdkWin32ClipboardThreadRetrieve, 1);
  const char * const *mime_types;
  gsize mime_types_len;
  gsize i;

  g_assert (clipdrop->clipboard_window != NULL);

  retr->parent.item_type = GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_RETRIEVE;
  retr->parent.start_time = g_get_monotonic_time ();
  retr->parent.end_time = retr->parent.start_time + CLIPBOARD_OPERATION_TIMEOUT;
  retr->parent.opaque_task = task;
  retr->pairs = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));
  retr->sequence_number = GetClipboardSequenceNumber ();

  mime_types = gdk_content_formats_get_mime_types (contentformats, &mime_types_len);

  for (i = 0; i < mime_types_len; i++)
    _gdk_win32_add_contentformat_to_pairs (mime_types[i], retr->pairs);

  g_async_queue_push (clipdrop->clipboard_open_thread_queue, retr);
  API_CALL (PostMessage, (clipdrop->clipboard_window, thread_wakeup_message, 0, 0));

  return;
}

typedef struct _GdkWin32ClipboardHDataPrepAndStream GdkWin32ClipboardHDataPrepAndStream;

struct _GdkWin32ClipboardHDataPrepAndStream
{
  GdkWin32ClipboardStorePrep *prep;
  GdkWin32HDataOutputStream  *stream;
};

static void
clipboard_store_hdata_ready (GObject      *clipboard,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GError *error = NULL;
  int i;
  gboolean no_other_streams;
  GdkWin32ClipboardHDataPrepAndStream *prep_and_stream = (GdkWin32ClipboardHDataPrepAndStream *) user_data;
  GdkWin32ClipboardStorePrep *prep = prep_and_stream->prep;
  GdkWin32HDataOutputStream  *stream = prep_and_stream->stream;
  GdkWin32ClipboardThreadStore *store;
  GdkWin32Clipdrop *clipdrop;

  g_clear_pointer (&prep_and_stream, g_free);

  if (!gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, &error))
    {
      HANDLE handle;
      gboolean is_hdata;

      GDK_NOTE(CLIPBOARD, g_printerr ("Failed to write stream: %s\n", error->message));
      g_error_free (error);
      for (i = 0; i < prep->elements->len; i++)
        free_prep_element (&g_array_index (prep->elements, GdkWin32ClipboardStorePrepElement, i));
      g_free (prep);
      g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
      handle = gdk_win32_hdata_output_stream_get_handle (stream, &is_hdata);

      if (is_hdata)
        API_CALL (GlobalFree, (handle));
      else
        API_CALL (CloseHandle, (handle));

      g_object_unref (stream);

      return;
    }

  for (i = 0, no_other_streams = TRUE; i < prep->elements->len; i++)
    {
      GdkWin32ClipboardStorePrepElement *el = &g_array_index (prep->elements, GdkWin32ClipboardStorePrepElement, i);

      if (el->stream == G_OUTPUT_STREAM (stream))
        {
          g_output_stream_close (el->stream, NULL, NULL);
          el->handle = gdk_win32_hdata_output_stream_get_handle (GDK_WIN32_HDATA_OUTPUT_STREAM (el->stream), NULL);
          g_object_unref (el->stream);
          el->stream = NULL;
        }
      else if (el->stream != NULL)
        no_other_streams = FALSE;
    }

  if (!no_other_streams)
    return;

  clipdrop = _gdk_win32_clipdrop_get ();

  store = g_new0 (GdkWin32ClipboardThreadStore, 1);

  store->parent.start_time = g_get_monotonic_time ();
  store->parent.end_time = store->parent.start_time + CLIPBOARD_OPERATION_TIMEOUT;
  store->parent.item_type = GDK_WIN32_CLIPBOARD_THREAD_QUEUE_ITEM_STORE;
  store->parent.opaque_task = prep->store_task;
  store->elements = prep->elements;

  g_async_queue_push (clipdrop->clipboard_open_thread_queue, store);
  API_CALL (PostMessage, (clipdrop->clipboard_window, thread_wakeup_message, 0, 0));

  g_free (prep);
}

gboolean
_gdk_win32_store_clipboard_contentformats (GdkClipboard      *cb,
                                           GTask             *task,
                                           GdkContentFormats *contentformats)
{
  GArray *pairs; /* of GdkWin32ContentFormatPair */
  const char * const *mime_types;
  gsize n_mime_types;
  gsize i;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkWin32ClipboardStorePrep *prep;

  g_assert (clipdrop->clipboard_window != NULL);

  mime_types = gdk_content_formats_get_mime_types (contentformats, &n_mime_types);

  pairs = g_array_sized_new (FALSE,
                             FALSE,
                             sizeof (GdkWin32ContentFormatPair),
                             n_mime_types);

  for (i = 0; i < n_mime_types; i++)
    _gdk_win32_add_contentformat_to_pairs (mime_types[i], pairs);

  if (pairs->len <= 0)
    {
      g_array_free (pairs, TRUE);

      return FALSE;
    }

  prep = g_new0 (GdkWin32ClipboardStorePrep, 1);
  prep->elements = g_array_sized_new (FALSE, TRUE, sizeof (GdkWin32ClipboardStorePrepElement), pairs->len);
  prep->store_task = task;

  for (i = 0; i < pairs->len; i++)
    {
      GdkWin32ClipboardStorePrepElement el;
      GdkWin32ContentFormatPair *pair = &g_array_index (pairs, GdkWin32ContentFormatPair, i);

      el.stream = gdk_win32_hdata_output_stream_new (pair, NULL);

      if (!el.stream)
        continue;

      el.w32format = pair->w32format;
      el.contentformat = pair->contentformat;
      el.handle = NULL;
      g_array_append_val (prep->elements, el);
    }

  for (i = 0; i < prep->elements->len; i++)
    {
      GdkWin32ClipboardStorePrepElement *el = &g_array_index (prep->elements, GdkWin32ClipboardStorePrepElement, i);
      GdkWin32ClipboardHDataPrepAndStream *prep_and_stream = g_new0 (GdkWin32ClipboardHDataPrepAndStream, 1);
      prep_and_stream->prep = prep;
      prep_and_stream->stream = GDK_WIN32_HDATA_OUTPUT_STREAM (el->stream);

      gdk_clipboard_write_async (GDK_CLIPBOARD (cb),
                                 el->contentformat,
                                 el->stream,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 clipboard_store_hdata_ready,
                                 prep_and_stream);
    }

  g_array_free (pairs, TRUE);

  return TRUE;
}
