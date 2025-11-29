/*
 * Copyright © 2021 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <AppKit/AppKit.h>
#include <gio/gio.h>

#include "gdkclipboardprivate.h"

G_BEGIN_DECLS

#ifndef AVAILABLE_MAC_OS_X_VERSION_10_13_AND_LATER
typedef NSString *NSPasteboardType;
#endif

@interface GdkMacosPasteboardItemDataProvider : NSObject <NSPasteboardItemDataProvider>
{
  GdkContentProvider *_contentProvider;
  GdkClipboard *_clipboard;
  GdkDrag *_drag;
}

-(id)initForClipboard:(GdkClipboard *)clipboard withContentProvider:(GdkContentProvider *)contentProvider;
-(id)initForDrag:(GdkDrag *)drag withContentProvider:(GdkContentProvider *)contentProvider;

@end

@interface GdkMacosPasteboardItem : NSPasteboardItem
{
  GdkContentProvider *_contentProvider;
  GdkClipboard *_clipboard;
  GdkDrag *_drag;
  NSRect _draggingFrame;
}

-(id)initForClipboard:(GdkClipboard *)clipboard withContentProvider:(GdkContentProvider *)contentProvider;
-(id)initForDrag:(GdkDrag *)drag withContentProvider:(GdkContentProvider *)contentProvider;

@end

NSPasteboardType   _gdk_macos_pasteboard_to_ns_type          (const char                      *mime_type,
                                                              NSPasteboardType                *alternate);
const char        *_gdk_macos_pasteboard_from_ns_type        (NSPasteboardType                 type);
GdkContentFormats *_gdk_macos_pasteboard_load_formats        (NSPasteboard                    *pasteboard);
void               _gdk_macos_pasteboard_register_drag_types (NSWindow                        *window);
void               _gdk_macos_pasteboard_read_async          (GObject                         *object,
                                                              NSPasteboard                    *pasteboard,
                                                              GdkContentFormats               *formats,
                                                              int                              io_priority,
                                                              GCancellable                    *cancellable,
                                                              GAsyncReadyCallback              callback,
                                                              gpointer                         user_data);
GInputStream      *_gdk_macos_pasteboard_read_finish         (GObject                         *object,
                                                              GAsyncResult                    *result,
                                                              const char                     **out_mime_type,
                                                              GError                         **error);

G_END_DECLS

