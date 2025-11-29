/* GDK - The GIMP Drawing Kit
 *
 * gdkclipdrop-win32.h: Private Win32-specific clipboard/DnD object
 *
 * Copyright © 2017 LRN
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

G_BEGIN_DECLS

#define _gdk_win32_clipdrop_get() (_win32_clipdrop)
#define _gdk_atom_array_index(a, i) (g_array_index (a, const char *, i))
#define _gdk_win32_clipdrop_atom(i) (_gdk_atom_array_index (_gdk_win32_clipdrop_get ()->known_atoms, i))
#define _gdk_cf_array_index(a, i) (g_array_index (a, UINT, i))
#define _gdk_win32_clipdrop_cf(i) (_gdk_cf_array_index (_gdk_win32_clipdrop_get ()->known_clipboard_formats, i))

/* Maps GDK contentformats to w32formats or vice versa, depending on the
 * semantics of the array that holds these.
 * Also remembers whether the data needs to be transmuted.
 */
typedef struct {
  int w32format;
  /* This is assumed to be an interned string, it will be
   * compared by simply comparing the pointer.
   */
  const char *contentformat;
  gboolean transmute;
} GdkWin32ContentFormatPair;

/* OLE-based DND state */
typedef enum {
  GDK_WIN32_DND_NONE,
  GDK_WIN32_DND_PENDING,
  GDK_WIN32_DND_DROPPED,
  GDK_WIN32_DND_FAILED,
  GDK_WIN32_DND_DRAGGING,
} GdkWin32DndState;

enum _GdkWin32AtomIndex
{
/* atoms: properties, targets and types */
  GDK_WIN32_ATOM_INDEX_GDK_SELECTION = 0,
  GDK_WIN32_ATOM_INDEX_CLIPBOARD_MANAGER,
  GDK_WIN32_ATOM_INDEX_WM_TRANSIENT_FOR,
  GDK_WIN32_ATOM_INDEX_TARGETS,
  GDK_WIN32_ATOM_INDEX_DELETE,
  GDK_WIN32_ATOM_INDEX_SAVE_TARGETS,
  GDK_WIN32_ATOM_INDEX_TEXT_PLAIN_UTF8,
  GDK_WIN32_ATOM_INDEX_TEXT_PLAIN,
  GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST,
  GDK_WIN32_ATOM_INDEX_TEXT_HTML,
  GDK_WIN32_ATOM_INDEX_IMAGE_PNG,
  GDK_WIN32_ATOM_INDEX_IMAGE_JPEG,
  GDK_WIN32_ATOM_INDEX_IMAGE_BMP,
  GDK_WIN32_ATOM_INDEX_IMAGE_GIF,
/* DND selections */
  GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION,
  GDK_WIN32_ATOM_INDEX_DROPFILES_DND,
  GDK_WIN32_ATOM_INDEX_OLE2_DND,
/* Clipboard formats */
  GDK_WIN32_ATOM_INDEX_PNG,
  GDK_WIN32_ATOM_INDEX_JFIF,
  GDK_WIN32_ATOM_INDEX_GIF,
  GDK_WIN32_ATOM_INDEX_CF_DIB,
  GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST,
  GDK_WIN32_ATOM_INDEX_CF_TEXT,
  GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT,
  GDK_WIN32_ATOM_INDEX_LAST
};

typedef enum _GdkWin32AtomIndex GdkWin32AtomIndex;

enum _GdkWin32CFIndex
{
  GDK_WIN32_CF_INDEX_PNG = 0,
  GDK_WIN32_CF_INDEX_JFIF,
  GDK_WIN32_CF_INDEX_GIF,
  GDK_WIN32_CF_INDEX_UNIFORMRESOURCELOCATORW,
  GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST,
  GDK_WIN32_CF_INDEX_HTML_FORMAT,
  GDK_WIN32_CF_INDEX_TEXT_HTML,
  GDK_WIN32_CF_INDEX_IMAGE_PNG,
  GDK_WIN32_CF_INDEX_IMAGE_JPEG,
  GDK_WIN32_CF_INDEX_IMAGE_BMP,
  GDK_WIN32_CF_INDEX_IMAGE_GIF,
  GDK_WIN32_CF_INDEX_TEXT_URI_LIST,
  GDK_WIN32_CF_INDEX_TEXT_PLAIN_UTF8,
  GDK_WIN32_CF_INDEX_LAST
};

typedef enum _GdkWin32CFIndex GdkWin32CFIndex;

#define GDK_TYPE_WIN32_CLIPDROP         (gdk_win32_clipdrop_get_type ())
#define GDK_WIN32_CLIPDROP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WIN32_CLIPDROP, GdkWin32Clipdrop))
#define GDK_WIN32_CLIPDROP_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WIN32_CLIPDROP, GdkWin32ClipdropClass))
#define GDK_IS_WIN32_CLIPDROP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WIN32_CLIPDROP))
#define GDK_IS_WIN32_CLIPDROP_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WIN32_CLIPDROP))
#define GDK_WIN32_CLIPDROP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WIN32_CLIPDROP, GdkWin32ClipdropClass))

typedef struct _GdkWin32Clipdrop GdkWin32Clipdrop;
typedef struct _GdkWin32ClipdropClass GdkWin32ClipdropClass;

typedef BOOL (WINAPI * GetUpdatedClipboardFormatsFunc)(
  _Out_ PUINT lpuiFormats,
  _In_  UINT  cFormats,
  _Out_ PUINT pcFormatsOut
);

/* This object is just a sink to hold all the clipboard- and dnd-related data
 * that otherwise would be in global variables.
 */
struct _GdkWin32Clipdrop
{
  GObject parent_instance;

  /* interned strings for well-known image formats */
  const char **known_pixbuf_formats;
  int n_known_pixbuf_formats;

  /* GArray of GdkAtoms for various known Selection and DnD strings.
   * Size is guaranteed to be at least GDK_WIN32_ATOM_INDEX_LAST
   */
  GArray *known_atoms;

  /* GArray of UINTs for various known clipboard formats.
   * Size is guaranteed to be at least GDK_WIN32_CF_INDEX_LAST.
   */
  GArray *known_clipboard_formats;

  GdkWin32DndState  dnd_target_state;

  /* A target-keyed hash table of GArrays of GdkWin32ContentFormatPairs describing compatibility w32formats for a contentformat */
  GHashTable       *compatibility_w32formats;
  /* A format-keyed hash table of GArrays of GdkAtoms describing compatibility contentformats for a w32format */
  GHashTable       *compatibility_contentformats;

  /* By all rights we should be able to just use this function
   * normally, as our target platform is Vista-or-later.
   * This pointer is manually retrieved only to allow
   * GTK to be compiled with old MinGW versions, which
   * don't have GetUpdatedClipboardFormats in the import libs.
   */
  GetUpdatedClipboardFormatsFunc GetUpdatedClipboardFormats;

  /* The thread that tries to open the clipboard and then
   * do stuff with it. Since clipboard opening can
   * fail, we split the code into a thread, and let
   * it try to open the clipboard repeatedly until
   * the operation times out.
   */
  GThread          *clipboard_open_thread;

  /* Our primary means of communicating with the thread.
   * The communication is one-way only - the thread replies
   * by just queueing functions to be called in the main
   * thread by using g_idle_add().
   */
  GAsyncQueue      *clipboard_open_thread_queue;

  /* We reply to clipboard render requests via this thing.
   * We can't use messages, since the clipboard thread will
   * stop spinning the message loop while it waits for us
   * to render the data.
   */
  GAsyncQueue      *clipboard_render_queue;

  /* Window handle for the clipboard window that we
   * receive from the clipboard thread. We use that
   * to wake up the clipboard window main loop by
   * posting a message to it.
   */
  HWND              clipboard_window;

  /* The thread that calls DoDragDrop (), which would
   * normally block our main thread, as it runs its own
   * Windows message loop.
   */
  GThread          *dnd_thread;
  DWORD             dnd_thread_id;

  /* We reply to the various dnd thread requests via this thing.
   * We can't use messages, since the dnd thread will
   * stop spinning the message loop while it waits for us
   * to come up with a reply.
   */
  GAsyncQueue      *dnd_queue;

  /* This counter is atomically incremented every time
   * the main thread pushes something into the queue,
   * and atomically decremented every time the DnD thread
   * pops something out of it.
   * It can be cheaply atomically checked to see if there's
   * anything in the queue. If there is, then the queue
   * processing (which requires expensice locks) can happen.
   */
  int               dnd_queue_counter;

  /* We don't actually support multiple simultaneous drags,
   * for obvious reasons (though this might change with
   * the advent of multitouch support?), but there may be
   * circumstances where we have two drag contexts at
   * the same time (one of them will grab the cursor
   * and thus cancel the other drag operation, but
   * there will be a point of time when both contexts
   * are out there). Thus we keep them around in this hash table.
   * Key is the context object (which is safe, because the main
   * thread keeps a reference on each one of those), value
   * is a pointer to a GdkWin32DnDThreadDoDragDrop struct,
   * which we can only examine when we're sure that the
   * dnd thread is not active.
   */
  GHashTable       *active_source_drags;
};

struct _GdkWin32ClipdropClass
{
  GObjectClass parent_class;
};

GType    gdk_win32_clipdrop_get_type                               (void) G_GNUC_CONST;

void     _gdk_win32_clipdrop_init                                  (void);

gboolean _gdk_win32_format_uses_hdata                              (UINT                         w32format);

char   * _gdk_win32_get_clipboard_format_name                      (UINT                         fmt,
                                                                    gboolean                    *is_predefined);
void     _gdk_win32_add_w32format_to_pairs                         (UINT                         format,
                                                                    GArray                      *array,
                                                                    GdkContentFormatsBuilder    *builder);
int      _gdk_win32_add_contentformat_to_pairs                     (const char                  *target,
                                                                    GArray                      *array);

void     _gdk_win32_clipboard_default_output_done                  (GObject                     *clipboard,
                                                                    GAsyncResult                *result,
                                                                    gpointer                     user_data);
gboolean _gdk_win32_transmute_contentformat                        (const char                  *from_contentformat,
                                                                    UINT                         to_w32format,
                                                                    const guchar                *data,
                                                                    gsize                        length,
                                                                    guchar                     **set_data,
                                                                    gsize                       *set_data_length);

gboolean _gdk_win32_transmute_windows_data                         (UINT                         from_w32format,
                                                                    const char                  *to_contentformat,
                                                                    HANDLE                       hdata,
                                                                    guchar                     **set_data,
                                                                    gsize                       *set_data_length);


gboolean _gdk_win32_store_clipboard_contentformats                 (GdkClipboard                *cb,
                                                                    GTask                       *task,
                                                                    GdkContentFormats           *contentformats);

void     _gdk_win32_retrieve_clipboard_contentformats              (GTask                       *task,
                                                                    GdkContentFormats           *contentformats);

void     _gdk_win32_advertise_clipboard_contentformats             (GTask                       *task,
                                                                    GdkContentFormats           *contentformats);



