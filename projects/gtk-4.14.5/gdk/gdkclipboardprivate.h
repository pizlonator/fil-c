/*
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

#pragma once

#include <gdk/gdkclipboard.h>

G_BEGIN_DECLS

#define GDK_CLIPBOARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CLIPBOARD, GdkClipboardClass))
#define GDK_IS_CLIPBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CLIPBOARD))
#define GDK_CLIPBOARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CLIPBOARD, GdkClipboardClass))

typedef struct _GdkClipboardClass GdkClipboardClass;

struct _GdkClipboard
{
  GObject parent;
};

struct _GdkClipboardClass
{
  GObjectClass parent_class;

  /* signals */
  void                  (* changed)                             (GdkClipboard           *clipboard);

  /* vfuncs */
  gboolean              (* claim)                               (GdkClipboard           *clipboard,
                                                                 GdkContentFormats      *formats,
                                                                 gboolean                local,
                                                                 GdkContentProvider     *content);
  void                  (* store_async)                         (GdkClipboard           *clipboard,
                                                                 int                     io_priority,
                                                                 GCancellable           *cancellable,
                                                                 GAsyncReadyCallback     callback,
                                                                 gpointer                user_data);
  gboolean              (* store_finish)                        (GdkClipboard           *clipboard,
                                                                 GAsyncResult           *result,
                                                                 GError                **error);
  void                  (* read_async)                          (GdkClipboard           *clipboard,
                                                                 GdkContentFormats      *formats,
                                                                 int                      io_priority,
                                                                 GCancellable           *cancellable,
                                                                 GAsyncReadyCallback     callback,
                                                                 gpointer                user_data);
  GInputStream *        (* read_finish)                         (GdkClipboard           *clipboard,
                                                                 GAsyncResult           *result,
                                                                 const char            **out_mime_type,
                                                                 GError                **error);
};

GdkClipboard *          gdk_clipboard_new                       (GdkDisplay             *display);

void                    gdk_clipboard_claim_remote              (GdkClipboard           *clipboard,
                                                                 GdkContentFormats      *formats);
void                    gdk_clipboard_write_async               (GdkClipboard           *clipboard,
                                                                 const char             *mime_type,
                                                                 GOutputStream          *stream,
                                                                 int                     io_priority,
                                                                 GCancellable           *cancellable,
                                                                 GAsyncReadyCallback     callback,
                                                                 gpointer                user_data);
gboolean                gdk_clipboard_write_finish              (GdkClipboard           *clipboard,
                                                                 GAsyncResult           *result,
                                                                 GError                **error);

G_END_DECLS

