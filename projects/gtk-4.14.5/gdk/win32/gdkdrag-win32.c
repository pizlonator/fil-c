/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Archaeopteryx Software Inc.
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

#include "config.h"

#include "gdkwin32dnd.h"

#include "gdkeventsprivate.h"
#include "gdkseatprivate.h"
#include "gdkprivate.h"

#include <fcntl.h>
#include <io.h>
#include <math.h>
#include <string.h>

/*
 * Support for OLE-2 drag and drop added at Archaeopteryx Software, 2001
 * For more information, do not contact Stephan R.A. Deibel (sdeibel@archaeopteryx.com),
 * because the code went through multiple modifications since then.
 *
 * Notes on the implementation:
 *
 * Source drag context, IDropSource and IDataObject for it are created
 * (almost) simultaneously, whereas target drag context and IDropTarget
 * are separated in time - IDropTarget is created when a window is made
 * to accept drops, while target drag context is created when a dragging
 * cursor enters the window and is destroyed when that cursor leaves
 * the window.
 *
 * There's a mismatch between data types supported by W32 (W32 formats)
 * and by GTK (GDK contentformats).
 * To account for it the data is transmuted back and forth. There are two
 * main points of transmutation:
 * * GdkWin32HDATAOutputStream: transmutes GTK data to W32 data
 * * GdkWin32Drop: transmutes W32 data to GTK data
 *
 * There are also two points where data formats are considered:
 * * When source drag context is created, it gets a list of GDK contentformats
 *   that it supports, these are matched to the W32 formats they
 *   correspond to (possibly with transmutation). New W32 formats for
 *   GTK-specific contentformats are also created here (see below).
 * * When target drop context is created, it queries the IDataObject
 *   for the list of W32 formats it supports and matches these to
 *   corresponding GDK contentformats that it will be able to provide
 *   (possibly with transmutation) later. Missing GDK contentformats for
 *   W32-specific formats are also created here (see below).
 *
 * W32 formats are integers (CLIPFORMAT), while GTK contentformats
 * are mime/type strings, and cannot be used interchangeably.
 *
 * To accommodate advanced GTK applications the code allows them to
 * register drop targets that accept W32 data formats, and to register
 * drag sources that provide W32 data formats. To do that they must
 * register with the mime/type "application/x.windows.ZZZ", where
 * ZZZ is the string name of the format in question
 * (for example, "Shell IDList Array") or, for unnamed pre-defined
 * formats, register with the stringified constant name of the format
 * in question (for example, "CF_UNICODETEXT").
 * If such contentformat is accepted/provided, GDK will not try to
 * transmute it to/from something else. Otherwise GDK will do the following
 * transmutation:
 * * If GTK application provides image/png, image/gif or image/jpeg,
 *   GDK will claim to also provide "PNG", "GIF" or "JFIF" respectively,
 *   and will pass these along verbatim.
 * * If GTK application provides any GdkPixbuf-compatible contentformat,
 *   GDK will also offer "PNG" and CF_DIB W32 formats.
 * * If GTK application provides text/plain;charset=utf8, GDK will also offer
 *   CF_UNICODETEXT (UTF-16-encoded) and CF_TEXT (encoded with thread-
 *   and locale-dependent codepage), and will do the conversion when such
 *   data is requested.
 * * If GTK application accepts image/png, image/gif or image/jpeg,
 *   GDK will claim to also accept "PNG", "GIF" or "JFIF" respectively,
 *   and will pass these along verbatim.
 * * If GTK application accepts image/bmp, GDK will
 *   claim to accept CF_DIB W32 format, and will convert
 *   it, changing the header, when such data is provided.
 * * If GTK application accepts text/plain;charset=utf8, GDK will
 *   claim to accept CF_UNICODETEXT and CF_TEXT, and will do
 *   the conversion when such data is provided.
 * * If GTK application accepts text/uri-list, GDK will
 *   claim to accept "Shell IDList Array", and will do the
 *   conversion when such data is provided.
 *
 * Currently the conversion from text/uri-list to "Shell IDList Array" is not
 * implemented, so it's not possible to drag & drop files from GTK
 * applications to non-GTK applications the same way one can drag files
 * from Windows Explorer.
 *
 * To increase inter-GTK compatibility, GDK will register GTK-specific
 * formats by their mime/types, as-is (i.e "text/plain;charset=utf-8", for example).
 * That way two GTK applications can exchange data in their native formats
 * (both well-known ones, such as text/plain;charset=utf8, and special,
 * known only to specific applications). This will work just
 * fine as long as both applications agree on what kind of data is stored
 * under such format exactly.
 *
 * Note that clipboard format space is limited, there can only be 16384
 * of them for a particular user session. Therefore it is highly inadvisable
 * to create and register such formats out of the whole cloth, dynamically.
 * If more flexibility is needed, register one format that has some
 * internal indicators of the kind of data it contains, then write the application
 * in such a way that it requests the data and inspects its header before deciding
 * whether to accept it or not. For details see GTK drag & drop documentation
 * on the "drag-motion" and "drag-data-received" signals.
 *
 * How managed DnD works:
 * GTK widget detects a drag gesture and calls
 * S: gdk_drag_begin_from_point() -> backend:drag_begin()
 * which creates the source drag context and the drag window,
 * and grabs the pointing device. GDK layer adds the context
 * to a list of currently-active contexts.
 *
 * From that point forward the context gets any events emitted
 * by GDK, and can prevent these events from going anywhere else.
 * They are all handled in
 * S: gdk_drag_handle_source_event() -> backend:handle_event()
 * (except for wayland backend - it doesn't have that function).
 *
 * That function catches the following events:
 * GDK_MOTION_NOTIFY
 * GDK_BUTTON_RELEASE
 * GDK_KEY_PRESS
 * GDK_KEY_RELEASE
 * GDK_GRAB_BROKEN
 *
 * GDK_MOTION_NOTIFY is emitted by the backend in response to
 * mouse movement.
 * Drag context handles it by calling a bunch of functions to
 * determine the state of the drag actions from the keys being
 * pressed, finding the drag window (another backend function
 * routed through GDK layer) and finally calls
 * S: gdk_drag_motion -> backend:drag_motion()
 * to notify the backend (i.e. itself) that the drag cursor
 * moved.
 * The response to that is to move the drag window and
 * do various bookkeeping.
 * W32: OLE2 protocol does nothing (other than moving the
 * drag window) in response to this, as all the functions
 * that GDK could perform here are already handled by the
 * OS driving the DnD process via DoDragDrop() call.
 *
 * GDK_BUTTON_RELEASE checks the
 * released button - if it's the button that was used to
 * initiate the drag, the "drop-performed" signal is emitted,
 * otherwise the drag is cancelled.
 *
 * GDK_KEY_PRESS and GDK_KEY_RELEASE handler does exactly the same thing as
 * GDK_MOTION_NOTIFY handler, but only after it checks the pressed
 * keys to emit "drop-performed" signal (on Space, Enter etc),
 * cancel the drag (on Escape) or move the drag cursor (arrow keys).
 *
 * GDK_GRAB_BROKEN handler cancels the drag for most broken grabs
 * (except for some special cases where the backend itself does
 *  temporary grabs as part of DnD, such as changing the cursor).
 *
 * GDK_DRAG_ENTER, GDK_DRAG_LEAVE, GDK_DRAG_MOTION and GDK_DROP_START
 * events are emitted when
 * the OS notifies the process about these things happening.
 * For X11 backend that is done in Xdnd event filters,
 * for W32 backend this is done in IDropSource/IDropTarget
 * object methods for the OLE2 protocol.
 * 
 */

/* The mingw.org compiler does not export GUIDS in it's import library. To work
 * around that, define INITGUID to have the GUIDS declared. */
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define INITGUID
#endif

/* For C-style COM wrapper macros */
#define COBJMACROS

#include "gdkdrag.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"
#include "gdkwin32dnd.h"
#include "gdkdisplayprivate.h"
#include "gdk/gdkdragprivate.h"
#include "gdkwin32dnd-private.h"
#include "gdkdisplay-win32.h"
#include "gdkdevice-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkhdataoutputstream-win32.h"

#include <ole2.h>

#include <shlobj.h>
#include <shlguid.h>
#include <objidl.h>
#include <glib/gi18n-lib.h>

#include <gdk/gdk.h>
#include <glib/gstdio.h>

/* Just to avoid calling RegisterWindowMessage() every time */
static UINT thread_wakeup_message;

typedef struct
{
  IDropSource                     ids;
  IDropSourceNotify               idsn;
  int                             ref_count;
  GdkDrag                        *drag;

  /* These are thread-local
   * copies of the similar fields from GdkWin32Drag
   */
  GdkWin32DragUtilityData  util_data;

  /* Cached here, so that we don't have to look in
   * the context every time.
   */
  HWND                            source_window_handle;
  guint                           scale;

  /* We get this from the OS via IDropSourceNotify and pass it to the
   * main thread.
   * Will be INVALID_HANDLE_VALUE (not NULL!) when unset.
   */
  HWND                            dest_window_handle;
} source_drag_context;

typedef struct {
  IDataObject                     ido;
  int                             ref_count;
  GdkDrag                        *drag;
  GArray                         *formats;
} data_object;

typedef struct {
  IEnumFORMATETC                  ief;
  int                             ref_count;
  int                             ix;
  GArray                         *formats;
} enum_formats;

typedef enum _GdkWin32DnDThreadQueueItemType GdkWin32DnDThreadQueueItemType;

enum _GdkWin32DnDThreadQueueItemType
{
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK = 1,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO = 2,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP = 3,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA = 4,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE = 5,
};

typedef struct _GdkWin32DnDThreadQueueItem GdkWin32DnDThreadQueueItem;

struct _GdkWin32DnDThreadQueueItem
{
  GdkWin32DnDThreadQueueItemType  item_type;

  /* This is used by the DnD thread to communicate the identity
   * of the drag context to the main thread. This is thread-safe
   * because DnD thread holds a reference to the context.
   */
  gpointer                        opaque_context;
};

typedef struct _GdkWin32DnDThreadDoDragDrop GdkWin32DnDThreadDoDragDrop;

/* This is used both to signal the DnD thread that it needs
 * to call DoDragDrop(), *and* to signal the main thread
 * that the DoDragDrop() call returned.
 */
struct _GdkWin32DnDThreadDoDragDrop
{
  GdkWin32DnDThreadQueueItem  base;

  source_drag_context        *src_context;
  data_object                *src_object;
  DWORD                       allowed_drop_effects;

  DWORD                       received_drop_effect;
  HRESULT                     received_result;
};

typedef struct _GdkWin32DnDThreadGetData GdkWin32DnDThreadGetData;

/* This is used both to signal the main thread that the DnD thread
 * needs DnD data, and to give that data to the DnD thread.
 */
struct _GdkWin32DnDThreadGetData
{
  GdkWin32DnDThreadQueueItem  base;

  GdkWin32ContentFormatPair   pair;
  GdkWin32HDataOutputStream  *stream;

  STGMEDIUM                   produced_data_medium;
};

typedef struct _GdkWin32DnDThreadGiveFeedback GdkWin32DnDThreadGiveFeedback;

struct _GdkWin32DnDThreadGiveFeedback
{
  GdkWin32DnDThreadQueueItem base;

  DWORD                      received_drop_effect;
};

typedef struct _GdkWin32DnDThreadDragInfo GdkWin32DnDThreadDragInfo;

struct _GdkWin32DnDThreadDragInfo
{
  GdkWin32DnDThreadQueueItem base;

  BOOL                       received_escape_pressed;
  DWORD                      received_keyboard_mods;
};

typedef struct _GdkWin32DnDThreadUpdateDragState GdkWin32DnDThreadUpdateDragState;

struct _GdkWin32DnDThreadUpdateDragState
{
  GdkWin32DnDThreadQueueItem base;

  gpointer                opaque_ddd;
  GdkWin32DragUtilityData produced_util_data;
};

typedef struct _GdkWin32DnDThread GdkWin32DnDThread;

struct _GdkWin32DnDThread
{
  /* We receive instructions from the main thread in this queue */
  GAsyncQueue *input_queue;

  /* We can't peek the queue or "unpop" queue items,
   * so the items that we can't act upon (yet) got
   * to be stored *somewhere*.
   */
  GList       *dequeued_items;

  source_drag_context *src_context;
  data_object         *src_object;
};

/* The code is much more secure if we don't rely on the OS to keep
 * this around for us.
 */
static GdkWin32DnDThread *dnd_thread_data = NULL;

static gboolean
dnd_queue_is_empty ()
{
  return g_atomic_int_get (&_win32_clipdrop->dnd_queue_counter) == 0;
}

static void
decrement_dnd_queue_counter ()
{
  g_atomic_int_dec_and_test (&_win32_clipdrop->dnd_queue_counter);
}

static void
increment_dnd_queue_counter ()
{
  g_atomic_int_inc (&_win32_clipdrop->dnd_queue_counter);
}

static void
free_queue_item (GdkWin32DnDThreadQueueItem *item)
{
  GdkWin32DnDThreadGetData *getdata;

  switch (item->item_type)
    {
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP:
      /* Don't unref anything, it's all done in the main thread,
       * when it receives a DoDragDrop reply.
       */
      break;
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE:
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK:
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO:
      /* These have no data to clean up */
      break;
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA:
      getdata = (GdkWin32DnDThreadGetData *) item;

      switch (getdata->produced_data_medium.tymed)
        {
        case TYMED_FILE:
        case TYMED_ISTREAM:
        case TYMED_ISTORAGE:
        case TYMED_GDI:
        case TYMED_MFPICT:
        case TYMED_ENHMF:
          g_critical ("Unsupported STGMEDIUM type");
          break;
        case TYMED_NULL:
          break;
        case TYMED_HGLOBAL:
          GlobalFree (getdata->produced_data_medium.hGlobal);
          break;
        }
    }

  g_free (item);
}

static gboolean
process_dnd_queue (gboolean                   timed,
                   guint64                    end_time,
                   GdkWin32DnDThreadGetData  *getdata_check)
{
  GdkWin32DnDThreadQueueItem *item;
  GdkWin32DnDThreadUpdateDragState *updatestate;
  GdkWin32DnDThreadDoDragDrop *ddd;

  while (TRUE)
    {
      if (timed)
        {
          guint64 current_time = g_get_monotonic_time ();

          if (current_time >= end_time)
            break;

          item = g_async_queue_timeout_pop (dnd_thread_data->input_queue, end_time - current_time);
        }
      else
        {
          item = g_async_queue_try_pop (dnd_thread_data->input_queue);
        }

      if (item == NULL)
        break;

      decrement_dnd_queue_counter ();

      switch (item->item_type)
        {
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP:
          /* We don't support more than one DnD at a time */
          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE:
          updatestate = (GdkWin32DnDThreadUpdateDragState *) item;
          ddd = (GdkWin32DnDThreadDoDragDrop *) updatestate->opaque_ddd;
          ddd->src_context->util_data = updatestate->produced_util_data;
          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA:
          if (item == (GdkWin32DnDThreadQueueItem *) getdata_check)
            return TRUE;

          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK:
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO:
          g_assert_not_reached ();
        }
    }

  return FALSE;
}

static gboolean
do_drag_drop_response (gpointer user_data)
{
  GdkWin32DnDThreadDoDragDrop *ddd = (GdkWin32DnDThreadDoDragDrop *) user_data;
  HRESULT hr = ddd->received_result;
  GdkDrag *drag = GDK_DRAG (ddd->base.opaque_context);
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gpointer table_value = g_hash_table_lookup (clipdrop->active_source_drags, drag);

  if (ddd == table_value)
    {
      GDK_NOTE (DND, g_print ("DoDragDrop returned %s with effect %lu\n",
                              (hr == DRAGDROP_S_DROP ? "DRAGDROP_S_DROP" :
                               (hr == DRAGDROP_S_CANCEL ? "DRAGDROP_S_CANCEL" :
                                (hr == E_UNEXPECTED ? "E_UNEXPECTED" :
                                 g_strdup_printf ("%#.8lx", hr)))), ddd->received_drop_effect));

      drag_win32->drop_failed = !(SUCCEEDED (hr) || hr == DRAGDROP_S_DROP);

      /* We used to delete the selection here,
       * now GTK does that automatically in response to 
       * the "dnd-finished" signal,
       * if the operation was successful and was a move.
       */
      GDK_NOTE (DND, g_print ("gdk_dnd_handle_drop_finished: 0x%p\n",
                              drag));

      g_signal_emit_by_name (drag, "dnd-finished");
      gdk_drag_drop_done (drag, !drag_win32->drop_failed);
    }
  else
    {
      if (!table_value)
        g_critical ("Did not find drag 0x%p in the active drags table", drag);
      else
        g_critical ("Found drag 0x%p in the active drags table, but the record doesn't match (0x%p != 0x%p)", drag, ddd, table_value);
    }

  /* 3rd parties could keep a reference to this object,
   * but we won't keep the drag alive that long.
   * Neutralize it (attempts to get its data will fail)
   * by nullifying the drag pointer (it doesn't hold
   * a reference, so no unreffing).
   */
  g_clear_object (&ddd->src_object->drag);

  IDropSource_Release (&ddd->src_context->ids);
  IDataObject_Release (&ddd->src_object->ido);

  g_hash_table_remove (clipdrop->active_source_drags, drag);
  free_queue_item (&ddd->base);

  return G_SOURCE_REMOVE;
}

static void
received_drag_context_data (GObject      *drag,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GError *error = NULL;
  GdkWin32DnDThreadGetData *getdata = (GdkWin32DnDThreadGetData *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  if (!gdk_drag_write_finish (GDK_DRAG (drag), result, &error))
    {
      HANDLE handle;
      gboolean is_hdata;

      GDK_NOTE (DND, g_printerr ("%p: failed to write HData-backed stream: %s\n", drag, error->message));
      g_error_free (error);
      g_output_stream_close (G_OUTPUT_STREAM (getdata->stream), NULL, NULL);
      handle = gdk_win32_hdata_output_stream_get_handle (getdata->stream, &is_hdata);

      if (is_hdata)
        API_CALL (GlobalFree, (handle));
      else
        API_CALL (CloseHandle, (handle));
    }
  else
    {
      g_output_stream_close (G_OUTPUT_STREAM (getdata->stream), NULL, NULL);
      getdata->produced_data_medium.tymed = TYMED_HGLOBAL;
      getdata->produced_data_medium.hGlobal = gdk_win32_hdata_output_stream_get_handle (getdata->stream, NULL);
    }

  g_clear_object (&getdata->stream);
  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, getdata);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));
}

static gboolean
get_data_response (gpointer user_data)
{
  GdkWin32DnDThreadGetData *getdata = (GdkWin32DnDThreadGetData *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkDrag *drag = GDK_DRAG (getdata->base.opaque_context);
  gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, drag);

  GDK_NOTE (DND, g_print ("idataobject_getdata will request target 0x%p (%s)",
                          getdata->pair.contentformat, getdata->pair.contentformat));

  /* This just verifies that we got the right drag,
   * we don't need the ddd struct itself.
   */
  if (ddd)
    {
      GError *error = NULL;
      GOutputStream *stream = gdk_win32_hdata_output_stream_new (&getdata->pair, &error);

      if (stream)
        {
          getdata->stream = GDK_WIN32_HDATA_OUTPUT_STREAM (stream);
          gdk_drag_write_async (drag,
                                        getdata->pair.contentformat,
                                        stream,
                                        G_PRIORITY_DEFAULT,
                                        NULL,
                                        received_drag_context_data,
                                        getdata);

          return G_SOURCE_REMOVE;
        }
    }

  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, getdata);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));

  return G_SOURCE_REMOVE;
}

static void
do_drag_drop (GdkWin32DnDThreadDoDragDrop *ddd)
{
  HRESULT hr;

  dnd_thread_data->src_object = ddd->src_object;
  dnd_thread_data->src_context = ddd->src_context;

  hr = DoDragDrop (&dnd_thread_data->src_object->ido,
                   &dnd_thread_data->src_context->ids,
                   ddd->allowed_drop_effects,
                   &ddd->received_drop_effect);

  ddd->received_result = hr;

  g_idle_add_full (G_PRIORITY_DEFAULT, do_drag_drop_response, ddd, NULL);
}

gpointer
_gdk_win32_dnd_thread_main (gpointer data)
{
  GAsyncQueue *queue = (GAsyncQueue *) data;
  GdkWin32DnDThreadQueueItem *item;
  MSG msg;
  HRESULT hr;

  g_assert (dnd_thread_data == NULL);

  dnd_thread_data = g_new0 (GdkWin32DnDThread, 1);
  dnd_thread_data->input_queue = queue;

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  hr = OleInitialize (NULL);

  if (!SUCCEEDED (hr))
    g_error ("OleInitialize failed");

  /* Create a message queue */
  PeekMessage (&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

  thread_wakeup_message = RegisterWindowMessage ("GDK_WORKER_THREAD_WEAKEUP");

  /* Signal the main thread that we're ready.
   * This is the only time the queue works in reverse.
   */
  g_async_queue_push (queue, GUINT_TO_POINTER (GetCurrentThreadId ()));

  while (GetMessage (&msg, NULL, 0, 0))
    {
      if (!dnd_queue_is_empty ())
        {
          while ((item = g_async_queue_try_pop (queue)) != NULL)
            {
              decrement_dnd_queue_counter ();

              if (item->item_type != GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP)
                {
                  free_queue_item (item);
                  continue;
                }

              do_drag_drop ((GdkWin32DnDThreadDoDragDrop *) item);
              API_CALL (PostThreadMessage, (GetCurrentThreadId (), thread_wakeup_message, 0, 0));
              break;
            }
        }

      /* Just to be safe, although this mostly does nothing */
      TranslateMessage (&msg); 
      DispatchMessage (&msg); 
    }

  g_async_queue_unref (queue);
  g_clear_pointer (&dnd_thread_data, g_free);

  OleUninitialize ();
  CoUninitialize ();

  return NULL;
}

static gboolean drag_context_grab (GdkDrag *drag);

G_DEFINE_TYPE (GdkWin32Drag, gdk_win32_drag, GDK_TYPE_DRAG)

static void
move_drag_surface (GdkDrag *drag,
                   guint    x_root,
                   guint    y_root)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  gdk_win32_surface_move (drag_win32->drag_surface,
                          x_root - drag_win32->hot_x,
                          y_root - drag_win32->hot_y);
  gdk_win32_surface_raise (drag_win32->drag_surface);
}

static void
gdk_win32_drag_init (GdkWin32Drag *drag)
{
  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  drag->handle_events = TRUE;
  drag->dest_window = INVALID_HANDLE_VALUE;

  GDK_NOTE (DND, g_print ("gdk_win32_drag_init %p\n", drag));
}

static void
gdk_win32_drag_finalize (GObject *object)
{
  GdkDrag *drag;
  GdkWin32Drag *drag_win32;
  GdkSurface *drag_surface;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  GDK_NOTE (DND, g_print ("gdk_win32_drag_finalize %p\n", object));

  g_return_if_fail (GDK_IS_WIN32_DRAG (object));

  drag = GDK_DRAG (object);
  drag_win32 = GDK_WIN32_DRAG (drag);

  gdk_drag_set_cursor (drag, NULL);

  g_set_object (&drag_win32->grab_surface, NULL);
  drag_surface = drag_win32->drag_surface;

  G_OBJECT_CLASS (gdk_win32_drag_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (drag_surface);
}

/* Drag Contexts */

static GdkDrag *
gdk_drag_new (GdkDisplay         *display,
              GdkSurface         *surface,
              GdkContentProvider *content,
              GdkDragAction       actions,
              GdkDevice          *device)
{
  GdkWin32Drag *drag_win32;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkDrag *drag;

  drag_win32 = g_object_new (GDK_TYPE_WIN32_DRAG,
                             "device", device,
                             "content", content,
                             "surface", surface,
                             "actions", actions,
                             NULL);

  drag = GDK_DRAG (drag_win32);

  if (display_win32->has_fixed_scale)
    drag_win32->scale = display_win32->surface_scale;
  else
    drag_win32->scale = gdk_win32_display_get_monitor_scale_factor (display_win32, NULL, NULL);

  return drag;
}

#define PRINT_GUID(guid) \
  g_print ("%.08lx-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x", \
           ((gulong *)  guid)[0], \
           ((gushort *) guid)[2], \
           ((gushort *) guid)[3], \
           ((guchar *)  guid)[8], \
           ((guchar *)  guid)[9], \
           ((guchar *)  guid)[10], \
           ((guchar *)  guid)[11], \
           ((guchar *)  guid)[12], \
           ((guchar *)  guid)[13], \
           ((guchar *)  guid)[14], \
           ((guchar *)  guid)[15]);

static enum_formats *enum_formats_new (GArray *formats);

/* Finds a GdkDrag object that corresponds to a DnD operation
 * which is currently targeting the dest_window
 * Does not give a reference.
 */
GdkDrag *
_gdk_win32_find_drag_for_dest_window (HWND dest_window)
{
  GHashTableIter               iter;
  GdkWin32Drag                *drag_win32;
  GdkWin32DnDThreadDoDragDrop *ddd;
  GdkWin32Clipdrop            *clipdrop = _gdk_win32_clipdrop_get ();

  g_hash_table_iter_init (&iter, clipdrop->active_source_drags);

  while (g_hash_table_iter_next (&iter, (gpointer *) &drag_win32, (gpointer *) &ddd))
    if (ddd->src_context->dest_window_handle == dest_window)
      return GDK_DRAG (drag_win32);

  return NULL;
}

static GdkDragAction
action_for_drop_effect (DWORD effect)
{
  GdkDragAction action = 0;

  if (effect & DROPEFFECT_MOVE)
    action |= GDK_ACTION_MOVE;
  if (effect & DROPEFFECT_LINK)
    action |= GDK_ACTION_LINK;
  if (effect & DROPEFFECT_COPY)
    action |= GDK_ACTION_COPY;

  return action;
}

static ULONG STDMETHODCALLTYPE
idropsource_addref (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;

  int ref_count = ++ctx->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_addref %p %d\n", This, ref_count));

  return ref_count;
}

typedef struct _GdkWin32DnDEnterLeaveNotify GdkWin32DnDEnterLeaveNotify;

struct _GdkWin32DnDEnterLeaveNotify
{
  gpointer opaque_context;
  HWND     target_window_handle;
};

static gboolean
notify_dnd_enter (gpointer user_data)
{
  GdkWin32DnDEnterLeaveNotify *notify = (GdkWin32DnDEnterLeaveNotify *) user_data;
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (notify->opaque_context);

  drag_win32->dest_window = notify->target_window_handle;

  g_free (notify);

  return G_SOURCE_REMOVE;
}

static gboolean
notify_dnd_leave (gpointer user_data)
{
  GdkWin32DnDEnterLeaveNotify *notify = (GdkWin32DnDEnterLeaveNotify *) user_data;
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (notify->opaque_context);

  if (notify->target_window_handle != drag_win32->dest_window)
    g_warning ("DnD leave says that the window handle is 0x%p, but drag has 0x%p", notify->target_window_handle, drag_win32->dest_window);

  drag_win32->dest_window = INVALID_HANDLE_VALUE;

  g_free (notify);

  return G_SOURCE_REMOVE;
}

static HRESULT STDMETHODCALLTYPE
idropsourcenotify_dragentertarget (IDropSourceNotify *This,
                                   HWND               hwndTarget)
{
  source_drag_context *ctx = (source_drag_context *) (((char *) This) - G_STRUCT_OFFSET (source_drag_context, idsn));
  GdkWin32DnDEnterLeaveNotify *notify;

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  GDK_NOTE (DND, g_print ("idropsourcenotify_dragentertarget %p (SDC %p) 0x%p\n", This, ctx, hwndTarget));

  ctx->dest_window_handle = hwndTarget;

  notify = g_new0 (GdkWin32DnDEnterLeaveNotify, 1);
  notify->target_window_handle = hwndTarget;
  notify->opaque_context = ctx->drag;
  g_idle_add_full (G_PRIORITY_DEFAULT, notify_dnd_enter, notify, NULL);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idropsourcenotify_dragleavetarget (IDropSourceNotify *This)
{
  source_drag_context *ctx = (source_drag_context *) (((char *) This) - G_STRUCT_OFFSET (source_drag_context, idsn));
  GdkWin32DnDEnterLeaveNotify *notify;

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  GDK_NOTE (DND, g_print ("idropsourcenotify_dragleavetarget %p (SDC %p) 0x%p\n", This, ctx, ctx->dest_window_handle));

  notify = g_new0 (GdkWin32DnDEnterLeaveNotify, 1);
  notify->target_window_handle = ctx->dest_window_handle;
  ctx->dest_window_handle = INVALID_HANDLE_VALUE;
  notify->opaque_context = ctx->drag;
  g_idle_add_full (G_PRIORITY_DEFAULT, notify_dnd_leave, notify, NULL);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idropsource_queryinterface (LPDROPSOURCE This,
                            REFIID       riid,
                            LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idropsource_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      GDK_NOTE (DND, g_print ("...IDropSource S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSourceNotify))
    {
      GDK_NOTE (DND, g_print ("...IDropSourceNotify S_OK\n"));
      idropsource_addref (This);
      *ppvObject = &((source_drag_context *) This)->idsn;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static gboolean
unref_context_in_main_thread (gpointer opaque_context)
{
  GdkDrag *drag = GDK_DRAG (opaque_context);

  g_clear_object (&drag);

  return G_SOURCE_REMOVE;
}

static ULONG STDMETHODCALLTYPE
idropsource_release (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;

  int ref_count = --ctx->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_release %p %d\n", This, ref_count));

  if (ref_count == 0)
  {
    g_idle_add (unref_context_in_main_thread, ctx->drag);
    g_free (This);
  }

  return ref_count;
}

/* NOTE: This method is called continuously, even if nothing is
 * happening, as long as the drag operation is in progress.
 * It is OK to return a "safe" value (S_OK, to keep the drag
 * operation going) even if something notable happens, because
 * we will have another opportunity to return the "right" value
 * (once we know what it is, after GTK processes the events we
 *  send out) very soon.
 * Note that keyboard-related state in this function is nonsense,
 * as DoDragDrop doesn't get precise information about the keyboard,
 * especially the fEscapePressed argument.
 */
static HRESULT STDMETHODCALLTYPE
idropsource_querycontinuedrag (LPDROPSOURCE This,
                               BOOL         fEscapePressed,
                               DWORD        grfKeyState)
{
  source_drag_context *ctx = (source_drag_context *) This;

  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag %p esc=%d keystate=0x%lx with state %d\n", This, fEscapePressed, grfKeyState, ctx->util_data.state));

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag state %d\n", ctx->util_data.state));

  if (ctx->util_data.state == GDK_WIN32_DND_DROPPED)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_DROP\n"));
      return DRAGDROP_S_DROP;
    }
  else if (ctx->util_data.state == GDK_WIN32_DND_NONE)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_CANCEL\n"));
      return DRAGDROP_S_CANCEL;
    }
  else
    {
      GDK_NOTE (DND, g_print ("S_OK\n"));
      return S_OK;
    }
}

static void
maybe_emit_action_changed (GdkWin32Drag        *drag_win32,
                           GdkDragAction        actions)
{
  if (actions != drag_win32->current_action)
    {
      drag_win32->current_action = actions;
      gdk_drag_set_selected_action (GDK_DRAG (drag_win32), actions);
    }
}

static gboolean
give_feedback (gpointer user_data)
{
  GdkWin32DnDThreadGiveFeedback *feedback = (GdkWin32DnDThreadGiveFeedback *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, feedback->base.opaque_context);

  if (ddd)
    {
      GdkDrag *drag = GDK_DRAG (feedback->base.opaque_context);
      GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

      GDK_NOTE (DND, g_print ("gdk_dnd_handle_drag_status: 0x%p\n",
                              drag));

      maybe_emit_action_changed (drag_win32, action_for_drop_effect (feedback->received_drop_effect));
    }

  free_queue_item (&feedback->base);

  return G_SOURCE_REMOVE;
}

static HRESULT STDMETHODCALLTYPE
idropsource_givefeedback (LPDROPSOURCE This,
                          DWORD        dwEffect)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkWin32DnDThreadGiveFeedback *feedback;

  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p with drop effect %lu S_OK\n", This, dwEffect));

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  feedback = g_new0 (GdkWin32DnDThreadGiveFeedback, 1);
  feedback->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK;
  feedback->base.opaque_context = ctx->drag;
  feedback->received_drop_effect = dwEffect;

  g_idle_add_full (G_PRIORITY_DEFAULT, give_feedback, feedback, NULL);

  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p returns\n", This));

  return S_OK;
}

static ULONG STDMETHODCALLTYPE
idataobject_addref (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = ++dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_queryinterface (LPDATAOBJECT This,
                            REFIID       riid,
                            LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idataobject_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDataObject))
    {
      GDK_NOTE (DND, g_print ("...IDataObject S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
idataobject_release (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = --dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_array_free (dobj->formats, TRUE);
      g_free (This);
    }

  return ref_count;
}

static HRESULT
query (LPDATAOBJECT                This,
       LPFORMATETC                 pFormatEtc,
       GdkWin32ContentFormatPair **pair)
{
  data_object *ctx = (data_object *) This;
  int i;

  if (pair)
    *pair = NULL;

  if (!pFormatEtc)
    return DV_E_FORMATETC;

  if (pFormatEtc->lindex != -1)
    return DV_E_LINDEX;

  if ((pFormatEtc->tymed & TYMED_HGLOBAL) == 0)
    return DV_E_TYMED;

  if ((pFormatEtc->dwAspect & DVASPECT_CONTENT) == 0)
    return DV_E_DVASPECT;

  for (i = 0; i < ctx->formats->len; i++)
    {
      GdkWin32ContentFormatPair *p = &g_array_index (ctx->formats, GdkWin32ContentFormatPair, i);
      if (pFormatEtc->cfFormat == p->w32format)
        {
          if (pair)
            *pair = p;

          return S_OK;
        }
    }

  return DV_E_FORMATETC;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdata (LPDATAOBJECT This,
                     LPFORMATETC  pFormatEtc,
                     LPSTGMEDIUM  pMedium)
{
  data_object *ctx = (data_object *) This;
  HRESULT hr;
  GdkWin32DnDThreadGetData *getdata;
  GdkWin32ContentFormatPair *pair;

  if (ctx->drag == NULL)
    return E_FAIL;

  GDK_NOTE (DND, g_print ("idataobject_getdata %p %s ",
                          This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  /* Check whether we can provide requested format */
  hr = query (This, pFormatEtc, &pair);

  if (hr != S_OK)
    {
      GDK_NOTE (DND, g_print ("Unsupported format, returning 0x%lx\n", hr));
      return hr;
    }

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  getdata = g_new0 (GdkWin32DnDThreadGetData, 1);
  getdata->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA;
  getdata->base.opaque_context = (gpointer) ctx->drag;
  getdata->pair = *pair;
  g_idle_add_full (G_PRIORITY_DEFAULT, get_data_response, getdata, NULL);

  if (!process_dnd_queue (TRUE, g_get_monotonic_time () + G_USEC_PER_SEC * 30, getdata))
    return E_FAIL;

  if (getdata->produced_data_medium.tymed == TYMED_NULL)
    {
      free_queue_item (&getdata->base);

      return E_FAIL;
    }

  memcpy (pMedium, &getdata->produced_data_medium, sizeof (*pMedium));

  /* To ensure that the data isn't freed */
  getdata->produced_data_medium.tymed = TYMED_NULL;

  free_queue_item (&getdata->base);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdatahere (LPDATAOBJECT This,
                         LPFORMATETC  pFormatEtc,
                         LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdatahere %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_querygetdata (LPDATAOBJECT This,
                          LPFORMATETC  pFormatEtc)
{
  HRESULT hr;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread != g_thread_self ());

  hr = query (This, pFormatEtc, NULL);

  GDK_NOTE (DND,
      g_print ("idataobject_querygetdata %p 0x%08x fmt, %p ptd, %lu aspect, %ld lindex, %0lx tymed - %s, return %#lx (%s)\n",
               This, pFormatEtc->cfFormat, pFormatEtc->ptd, pFormatEtc->dwAspect, pFormatEtc->lindex, pFormatEtc->tymed, _gdk_win32_cf_to_string (pFormatEtc->cfFormat),
               hr, (hr == S_OK) ? "S_OK" : (hr == DV_E_FORMATETC) ? "DV_E_FORMATETC" : (hr == DV_E_LINDEX) ? "DV_E_LINDEX" : (hr == DV_E_TYMED) ? "DV_E_TYMED" : (hr == DV_E_DVASPECT) ? "DV_E_DVASPECT" : "unknown meaning"));

  return hr;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getcanonicalformatetc (LPDATAOBJECT This,
                                   LPFORMATETC  pFormatEtcIn,
                                   LPFORMATETC  pFormatEtcOut)
{
  GDK_NOTE (DND, g_print ("idataobject_getcanonicalformatetc %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_setdata (LPDATAOBJECT This,
                     LPFORMATETC  pFormatEtc,
                     LPSTGMEDIUM  pMedium,
                     BOOL         fRelease)
{
  GDK_NOTE (DND, g_print ("idataobject_setdata %p %s E_NOTIMPL\n",
                          This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumformatetc (LPDATAOBJECT     This,
                           DWORD            dwDirection,
                           LPENUMFORMATETC *ppEnumFormatEtc)
{
  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread != g_thread_self ());

  if (dwDirection != DATADIR_GET)
    {
      GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p E_NOTIMPL", This));

      return E_NOTIMPL;
    }

  *ppEnumFormatEtc = &enum_formats_new (((data_object *) This)->formats)->ief;

  GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p -> %p S_OK", This, *ppEnumFormatEtc));

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dadvise (LPDATAOBJECT This,
                     LPFORMATETC  pFormatetc,
                     DWORD        advf,
                     LPADVISESINK pAdvSink,
                     DWORD       *pdwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dunadvise (LPDATAOBJECT This,
                       DWORD         dwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dunadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumdadvise (LPDATAOBJECT    This,
                         LPENUMSTATDATA *ppenumAdvise)
{
  GDK_NOTE (DND, g_print ("idataobject_enumdadvise %p OLE_E_ADVISENOTSUPPORTED\n", This));

  return OLE_E_ADVISENOTSUPPORTED;
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_addref (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = ++en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_queryinterface (LPENUMFORMATETC This,
                               REFIID          riid,
                               LPVOID         *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("ienumformatetc_queryinterface %p", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IEnumFORMATETC))
    {
      GDK_NOTE (DND, g_print ("...IEnumFORMATETC S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_release (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = --en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_array_unref (en->formats);
      g_free (This);
    }

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_next (LPENUMFORMATETC This,
                     ULONG             celt,
                     LPFORMATETC     elts,
                     ULONG            *nelt)
{
  enum_formats *en = (enum_formats *) This;
  ULONG i, n;
  ULONG formats_to_get = celt;

  GDK_NOTE (DND, g_print ("ienumformatetc_next %p %d %ld ", This, en->ix, celt));

  n = 0;
  for (i = 0; i < formats_to_get; i++)
    {
      UINT fmt;
      if (en->ix >= en->formats->len)
        break;
      fmt = g_array_index (en->formats, GdkWin32ContentFormatPair, en->ix++).w32format;
      /* skip internals */
      if (fmt == 0 || fmt > 0xFFFF)
        {
          formats_to_get += 1;
          continue;
        }
      elts[n].cfFormat = fmt;
      elts[n].ptd = NULL;
      elts[n].dwAspect = DVASPECT_CONTENT;
      elts[n].lindex = -1;
      elts[n].tymed = TYMED_HGLOBAL;

      n++;
    }

  if (nelt != NULL)
    *nelt = n;

  GDK_NOTE (DND, g_print ("%s\n", (n == celt) ? "S_OK" : "S_FALSE"));

  if (n == celt)
    return S_OK;
  else
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_skip (LPENUMFORMATETC This,
                     ULONG             celt)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_skip %p %d %ld S_OK\n", This, en->ix, celt));

  en->ix += celt;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_reset (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_reset %p S_OK\n", This));

  en->ix = 0;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_clone (LPENUMFORMATETC  This,
                      LPENUMFORMATETC *ppEnumFormatEtc)
{
  enum_formats *en = (enum_formats *) This;
  enum_formats *new;

  GDK_NOTE (DND, g_print ("ienumformatetc_clone %p S_OK\n", This));

  new = enum_formats_new (en->formats);

  new->ix = en->ix;

  *ppEnumFormatEtc = &new->ief;

  return S_OK;
}

static IDropSourceVtbl ids_vtbl = {
  idropsource_queryinterface,
  idropsource_addref,
  idropsource_release,
  idropsource_querycontinuedrag,
  idropsource_givefeedback
};

static IDropSourceNotifyVtbl idsn_vtbl = {
  (HRESULT (STDMETHODCALLTYPE *) (IDropSourceNotify *, REFIID , LPVOID *)) idropsource_queryinterface,
  (ULONG (STDMETHODCALLTYPE *) (IDropSourceNotify *)) idropsource_addref,
  (ULONG (STDMETHODCALLTYPE *) (IDropSourceNotify *)) idropsource_release,
  idropsourcenotify_dragentertarget,
  idropsourcenotify_dragleavetarget
};

static IDataObjectVtbl ido_vtbl = {
  idataobject_queryinterface,
  idataobject_addref,
  idataobject_release,
  idataobject_getdata,
  idataobject_getdatahere,
  idataobject_querygetdata,
  idataobject_getcanonicalformatetc,
  idataobject_setdata,
  idataobject_enumformatetc,
  idataobject_dadvise,
  idataobject_dunadvise,
  idataobject_enumdadvise
};

static IEnumFORMATETCVtbl ief_vtbl = {
  ienumformatetc_queryinterface,
  ienumformatetc_addref,
  ienumformatetc_release,
  ienumformatetc_next,
  ienumformatetc_skip,
  ienumformatetc_reset,
  ienumformatetc_clone
};

static source_drag_context *
source_context_new (GdkDrag           *drag,
                    GdkContentFormats *formats)
{
  GdkWin32Drag *drag_win32;
  source_drag_context *result;
  GdkSurface *surface;

  drag_win32 = GDK_WIN32_DRAG (drag);

  g_object_get (drag, "surface", &surface, NULL);

  result = g_new0 (source_drag_context, 1);
  result->drag = g_object_ref (drag);
  result->ids.lpVtbl = &ids_vtbl;
  result->idsn.lpVtbl = &idsn_vtbl;
  result->ref_count = 1;
  result->source_window_handle = GDK_SURFACE_HWND (surface);
  result->scale = drag_win32->scale;
  result->util_data.state = GDK_WIN32_DND_PENDING; /* Implicit */
  result->dest_window_handle = INVALID_HANDLE_VALUE;

  g_object_unref (surface);

  GDK_NOTE (DND, g_print ("source_context_new: %p (drag %p)\n", result, result->drag));

  return result;
}

static data_object *
data_object_new (GdkDrag *drag)
{
  data_object *result;
  const char * const *mime_types;
  gsize n_mime_types, i;

  result = g_new0 (data_object, 1);

  result->ido.lpVtbl = &ido_vtbl;
  result->ref_count = 1;
  result->drag = drag;
  result->formats = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));

  mime_types = gdk_content_formats_get_mime_types (gdk_drag_get_formats (drag), &n_mime_types);

  for (i = 0; i < n_mime_types; i++)
    {
      int added_count = 0;
      int j;

      GDK_NOTE (DND, g_print ("DataObject supports contentformat 0x%p (%s)\n", mime_types[i], mime_types[i]));

      added_count = _gdk_win32_add_contentformat_to_pairs (mime_types[i], result->formats);

      for (j = 0; j < added_count && result->formats->len - 1 - j >= 0; j++)
        GDK_NOTE (DND, g_print ("DataObject will support w32format 0x%x\n", g_array_index (result->formats, GdkWin32ContentFormatPair, j).w32format));
    }

  GDK_NOTE (DND, g_print ("data_object_new: %p\n", result));

  return result;
}

static enum_formats *
enum_formats_new (GArray *formats)
{
  enum_formats *result;

  result = g_new0 (enum_formats, 1);

  result->ief.lpVtbl = &ief_vtbl;
  result->ref_count = 1;
  result->ix = 0;
  result->formats = g_array_ref (formats);

  return result;
}

void
_gdk_drag_init (void)
{
  HRESULT hr;

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  hr = OleInitialize (NULL);

  if (! SUCCEEDED (hr))
    g_error ("OleInitialize failed");
}

void
_gdk_win32_dnd_exit (void)
{
  OleUninitialize ();
  CoUninitialize ();
}

GdkDrag *
_gdk_win32_surface_drag_begin (GdkSurface         *surface,
                               GdkDevice          *device,
                               GdkContentProvider *content,
                               GdkDragAction       actions,
                               double              dx,
                               double              dy)
{
  GdkDrag *drag;
  GdkWin32Drag *drag_win32;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  double px, py;
  int x_root, y_root;
  GdkWin32DnDThreadDoDragDrop *ddd;
  source_drag_context *source_ctx;
  data_object         *data_obj;

  g_return_val_if_fail (surface != NULL, NULL);

  drag = gdk_drag_new (gdk_surface_get_display (surface),
                       surface,
                       content,
                       actions,
                       device);
  drag_win32 = GDK_WIN32_DRAG (drag);

  GDK_NOTE (DND, g_print ("_gdk_win32_surface_drag_begin\n"));

  _gdk_device_win32_query_state (device, NULL, NULL, &px, &py, NULL);
  x_root = round (px + dx);
  y_root = round (py + dy);

  drag_win32->start_x = x_root;
  drag_win32->start_y = y_root;
  drag_win32->util_data.last_x = drag_win32->start_x;
  drag_win32->util_data.last_y = drag_win32->start_y;

  g_set_object (&drag_win32->grab_surface, surface);

  drag_win32->drag_surface = gdk_win32_drag_surface_new (gdk_surface_get_display (surface));

  if (!drag_context_grab (drag))
    {
      g_object_unref (drag);
      return FALSE;
    }

  ddd = g_new0 (GdkWin32DnDThreadDoDragDrop, 1);

  source_ctx = source_context_new (drag, gdk_drag_get_formats (drag));
  data_obj = data_object_new (drag);

  ddd->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP;
  ddd->base.opaque_context = drag_win32;
  ddd->src_context = source_ctx;
  ddd->src_object = data_obj;
  ddd->allowed_drop_effects = 0;
  if (actions & GDK_ACTION_COPY)
    ddd->allowed_drop_effects |= DROPEFFECT_COPY;
  if (actions & GDK_ACTION_MOVE)
    ddd->allowed_drop_effects |= DROPEFFECT_MOVE;
  if (actions & GDK_ACTION_LINK)
    ddd->allowed_drop_effects |= DROPEFFECT_LINK;

  g_hash_table_replace (clipdrop->active_source_drags, g_object_ref (drag), ddd);
  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, ddd);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));

  drag_win32->util_data.state = GDK_WIN32_DND_PENDING;

  move_drag_surface (drag, x_root, y_root);

  return drag;
}

static DWORD
manufacture_keystate_from_GMT (GdkModifierType state)
{
  DWORD key_state = 0;

  if (state & GDK_ALT_MASK)
    key_state |= MK_ALT;
  if (state & GDK_CONTROL_MASK)
    key_state |= MK_CONTROL;
  if (state & GDK_SHIFT_MASK)
    key_state |= MK_SHIFT;
  if (state & GDK_BUTTON1_MASK)
    key_state |= MK_LBUTTON;
  if (state & GDK_BUTTON2_MASK)
    key_state |= MK_MBUTTON;
  if (state & GDK_BUTTON3_MASK)
    key_state |= MK_RBUTTON;

  return key_state;
}

static void
send_source_state_update (GdkWin32Clipdrop    *clipdrop,
                          GdkWin32Drag        *drag_win32,
                          gpointer            *ddd)
{
  GdkWin32DnDThreadUpdateDragState *status = g_new0 (GdkWin32DnDThreadUpdateDragState, 1);
  status->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE;
  status->opaque_ddd = ddd;
  status->produced_util_data = drag_win32->util_data;
  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, status);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));
}

static void
gdk_win32_drag_drop (GdkDrag *drag,
                     guint32  time_)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gpointer ddd;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  g_return_if_fail (drag != NULL);

  GDK_NOTE (DND, g_print ("gdk_win32_drag_drop\n"));

  ddd = g_hash_table_lookup (clipdrop->active_source_drags, drag);

  drag_win32->util_data.state = GDK_WIN32_DND_DROPPED;

  if (ddd)
    send_source_state_update (clipdrop, drag_win32, ddd);
}

static void
gdk_win32_drag_set_cursor (GdkDrag *drag,
                           GdkCursor      *cursor)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  GDK_NOTE (DND, g_print ("gdk_win32_drag_set_cursor: 0x%p 0x%p\n", drag, cursor));

  if (!g_set_object (&drag_win32->cursor, cursor))
    return;

  if (drag_win32->grab_seat)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_device_grab (gdk_seat_get_pointer (drag_win32->grab_seat),
                       drag_win32->grab_surface,
                       FALSE,
                       GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                       cursor, GDK_CURRENT_TIME);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

#define ANIM_TIME 500000 /* half a second */

typedef struct _GdkDragAnim GdkDragAnim;
struct _GdkDragAnim {
  GdkWin32Drag *drag;
  GdkFrameClock *frame_clock;
  gint64 start_time;
};

static void
gdk_drag_anim_destroy (GdkDragAnim *anim)
{
  g_object_unref (anim->drag);
  g_free (anim);
}

static gboolean
gdk_drag_anim_timeout (gpointer data)
{
  GdkDragAnim *anim = data;
  GdkWin32Drag *drag = anim->drag;
  GdkFrameClock *frame_clock = anim->frame_clock;
  gint64 current_time;
  double f;
  double t;
  int x, y;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);

  f = (current_time - anim->start_time) / (double) ANIM_TIME;

  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  gdk_win32_surface_show (drag->drag_surface, FALSE);
  x = (drag->util_data.last_x +
       (drag->start_x - drag->util_data.last_x) * t -
       drag->hot_x);
  y = (drag->util_data.last_y +
       (drag->start_y - drag->util_data.last_y) * t -
       drag->hot_y);
  gdk_win32_surface_move (drag->drag_surface, x, y);

  return G_SOURCE_CONTINUE;
}

static void
gdk_win32_drag_drop_done (GdkDrag  *drag,
                          gboolean  success)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  GdkDragAnim *anim;
  GdkWin32Clipdrop *clipdrop;
  gpointer ddd;
  guint id;

  GDK_NOTE (DND, g_print ("gdk_win32_drag_drop_done: 0x%p %s\n",
                          drag,
                          success ? "dropped successfully" : "dropped unsuccessfully"));

  /* FIXME: This is temporary, until the code is fixed to ensure that
   * gdk_drag_finish () is called by GTK.
   */
  clipdrop = _gdk_win32_clipdrop_get ();
  ddd = g_hash_table_lookup (clipdrop->active_source_drags, drag);

  if (success)
    drag_win32->util_data.state = GDK_WIN32_DND_DROPPED;
  else
    drag_win32->util_data.state = GDK_WIN32_DND_NONE;

  if (ddd)
    send_source_state_update (clipdrop, drag_win32, ddd);

  drag_win32->handle_events = FALSE;

  if (success)
    {
      gdk_surface_hide (drag_win32->drag_surface);

      return;
    }

  anim = g_new0 (GdkDragAnim, 1);
  g_set_object (&anim->drag, drag_win32);
  anim->frame_clock = gdk_surface_get_frame_clock (drag_win32->drag_surface);
  anim->start_time = gdk_frame_clock_get_frame_time (anim->frame_clock);

  GDK_NOTE (DND, g_print ("gdk_win32_drag_drop_done: animate the drag window from %d : %d to %d : %d\n",
                          drag_win32->util_data.last_x, drag_win32->util_data.last_y,
                          drag_win32->start_x, drag_win32->start_y));

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_drag_anim_timeout, anim,
                           (GDestroyNotify) gdk_drag_anim_destroy);
  gdk_source_set_static_name_by_id (id, "[gtk] gdk_drag_anim_timeout");
}

static gboolean
drag_context_grab (GdkDrag *drag)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  GdkSeatCapabilities capabilities;
  GdkSeat *seat;
  GdkCursor *cursor;

  GDK_NOTE (DND, g_print ("drag_context_grab: 0x%p with grab surface 0x%p\n",
                          drag,
                          drag_win32->grab_surface));

  if (!drag_win32->grab_surface)
    return FALSE;

  seat = gdk_device_get_seat (gdk_drag_get_device (drag));

  capabilities = GDK_SEAT_CAPABILITY_ALL;

  cursor = gdk_drag_get_cursor (drag, gdk_drag_get_selected_action (drag));
  g_set_object (&drag_win32->cursor, cursor);

  if (gdk_seat_grab (seat, drag_win32->grab_surface,
                     capabilities, FALSE,
                     drag_win32->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&drag_win32->grab_seat, seat);

  /* TODO: Should be grabbing keys here, to support keynav. SetWindowsHookEx()? */

  return TRUE;
}

static void
drag_context_ungrab (GdkDrag *drag)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  GDK_NOTE (DND, g_print ("drag_context_ungrab: 0x%p 0x%p\n",
                          drag,
                          drag_win32->grab_seat));

  if (!drag_win32->grab_seat)
    return;

  gdk_seat_ungrab (drag_win32->grab_seat);

  g_clear_object (&drag_win32->grab_seat);

  /* TODO: Should be ungrabbing keys here */
}

static void
gdk_win32_drag_cancel (GdkDrag             *drag,
                       GdkDragCancelReason  reason)
{
  const char *reason_str = NULL;
  switch (reason)
    {
    case GDK_DRAG_CANCEL_NO_TARGET:
      reason_str = "no target";
      break;
    case GDK_DRAG_CANCEL_USER_CANCELLED:
      reason_str = "user cancelled";
      break;
    case GDK_DRAG_CANCEL_ERROR:
      reason_str = "error";
      break;
    default:
      reason_str = "<unknown>";
      break;
    }

  GDK_NOTE (DND, g_print ("gdk_win32_drag_cancel: 0x%p %s\n",
                          drag,
                          reason_str));

  gdk_drag_set_cursor (drag, NULL);
  drag_context_ungrab (drag);
  gdk_drag_drop_done (drag, FALSE);
}

static void
gdk_win32_drag_drop_performed (GdkDrag *drag,
                               guint32  time_)
{
  GDK_NOTE (DND, g_print ("gdk_win32_drag_drop_performed: 0x%p %u\n",
                          drag,
                          time_));

  gdk_win32_drag_drop (drag, time_);
  gdk_drag_set_cursor (drag, NULL);
  drag_context_ungrab (drag);
}

#define BIG_STEP 20
#define SMALL_STEP 1

static gboolean
gdk_dnd_handle_motion_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  GdkModifierType state;
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  DWORD key_state;
  double x, y;
  double x_root, y_root;
  GdkWin32Clipdrop *clipdrop;

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_motion_event: 0x%p\n", drag));

  state = gdk_event_get_modifier_state (event);
  gdk_event_get_position (event, &x, &y);

  x_root = event->surface->x + x;
  y_root = event->surface->y + y;

  if (drag_win32->drag_surface)
    move_drag_surface (drag, x_root, y_root);

  key_state = manufacture_keystate_from_GMT (state);

  clipdrop = _gdk_win32_clipdrop_get ();

  GDK_NOTE (DND, g_print ("Post WM_MOUSEMOVE keystate=%lu\n", key_state));

  drag_win32->util_data.last_x = x_root;
  drag_win32->util_data.last_y = y_root;

  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id,
                                WM_MOUSEMOVE,
                                key_state,
                                MAKELPARAM (x_root * drag_win32->scale,
                                            y_root * drag_win32->scale)));

  return TRUE;
}

static gboolean
gdk_dnd_handle_key_event (GdkDrag  *drag,
                          GdkEvent *event)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);
  GdkModifierType state;
  GdkDevice *pointer;
  GdkSeat *seat;
  int dx, dy;

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_key_event: 0x%p\n", drag));

  state = gdk_event_get_modifier_state (event);

  dx = dy = 0;
  seat = gdk_event_get_seat (event);
  pointer = gdk_seat_get_pointer (seat);

  if (gdk_event_get_event_type (event) == GDK_KEY_PRESS)
    {
      switch (gdk_key_event_get_keyval (event))
        {
        case GDK_KEY_Escape:
          gdk_drag_cancel (drag, GDK_DRAG_CANCEL_USER_CANCELLED);
          return TRUE;

        case GDK_KEY_space:
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_KP_Space:
          if ((gdk_drag_get_selected_action (drag) != 0) &&
              (drag_win32->dest_window != INVALID_HANDLE_VALUE))
            {
              g_signal_emit_by_name (drag, "drop-performed");
            }
          else
            gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

          return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          dy = (state & GDK_ALT_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          dy = (state & GDK_ALT_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          dx = (state & GDK_ALT_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          dx = (state & GDK_ALT_MASK) ? BIG_STEP : SMALL_STEP;
          break;
        }
    }

  /* The state is not yet updated in the event, so we need
   * to query it here.
   */
  _gdk_device_win32_query_state (pointer, NULL, NULL, NULL, NULL, &state);

  if (dx != 0 || dy != 0)
    {
      drag_win32->util_data.last_x += dx;
      drag_win32->util_data.last_y += dy;
    }

  if (drag_win32->drag_surface)
    move_drag_surface (drag, drag_win32->util_data.last_x, drag_win32->util_data.last_y);

  return TRUE;
}

static gboolean
gdk_dnd_handle_grab_broken_event (GdkDrag  *drag,
                                  GdkEvent *event)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_grab_broken_event: 0x%p\n", drag));

  /* Don't cancel if we break the implicit grab from the initial button_press.
   * Also, don't cancel if we re-grab on the widget or on our grab window, for
   * example, when changing the drag cursor.
   */
  if (/* FIXME: event->implicit || */
      gdk_grab_broken_event_get_grab_surface (event) == drag_win32->drag_surface ||
      gdk_grab_broken_event_get_grab_surface (event) == drag_win32->grab_surface)
    return FALSE;

  if (gdk_event_get_device (event) != gdk_drag_get_device (drag))
    return FALSE;

  gdk_drag_cancel (drag, GDK_DRAG_CANCEL_ERROR);
  return TRUE;
}

static gboolean
gdk_dnd_handle_button_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  GDK_NOTE (DND, g_print ("gdk_dnd_handle_button_event: 0x%p\n", drag));

#if 0
  /* FIXME: Check the button matches */
  if (event->button != drag_win32->button)
    return FALSE;
#endif

  if ((gdk_drag_get_selected_action (drag) != 0))
    {
      g_signal_emit_by_name (drag, "drop-performed");
    }
  else
    gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

  /* Make sure GTK gets mouse release button event */
  return FALSE;
}

gboolean
gdk_win32_drag_handle_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  if (!drag_win32->grab_seat)
    return FALSE;
  if (!drag_win32->handle_events)
    {
      /* FIXME: remove this functionality once gtk no longer calls DnD after drag_done() */
      g_warning ("Got an event %d for drag context %p, even though it's done!",
                 event->event_type, drag);
      return FALSE;
    }

  switch (gdk_event_get_event_type (event))
    {
    case GDK_MOTION_NOTIFY:
      return gdk_dnd_handle_motion_event (drag, event);
    case GDK_BUTTON_RELEASE:
      return gdk_dnd_handle_button_event (drag, event);
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return gdk_dnd_handle_key_event (drag, event);
    case GDK_GRAB_BROKEN:
      return gdk_dnd_handle_grab_broken_event (drag, event);
    default:
      break;
    }

  return FALSE;
}

static GdkSurface *
gdk_win32_drag_get_drag_surface (GdkDrag *drag)
{
  return GDK_WIN32_DRAG (drag)->drag_surface;
}

static void
gdk_win32_drag_set_hotspot (GdkDrag *drag,
                            int      hot_x,
                            int      hot_y)
{
  GdkWin32Drag *drag_win32 = GDK_WIN32_DRAG (drag);

  GDK_NOTE (DND, g_print ("gdk_drag_set_hotspot: 0x%p %d:%d\n",
                          drag,
                          hot_x, hot_y));

  drag_win32->hot_x = hot_x;
  drag_win32->hot_y = hot_y;

  if (drag_win32->grab_seat)
    {
      /* DnD is managed, update current position */
      move_drag_surface (drag, drag_win32->util_data.last_x, drag_win32->util_data.last_y);
    }
}

static void
gdk_win32_drag_class_init (GdkWin32DragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_win32_drag_finalize;

  drag_class->get_drag_surface = gdk_win32_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_win32_drag_set_hotspot;
  drag_class->drop_done = gdk_win32_drag_drop_done;
  drag_class->set_cursor = gdk_win32_drag_set_cursor;
  drag_class->cancel = gdk_win32_drag_cancel;
  drag_class->drop_performed = gdk_win32_drag_drop_performed;
  drag_class->handle_event = gdk_win32_drag_handle_event;
}
