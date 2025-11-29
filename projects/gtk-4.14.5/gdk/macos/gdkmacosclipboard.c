/*
 * Copyright © 2020 Red Hat, Inc.
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

#include "config.h"

#include <glib/gi18n.h>

#include "gdkmacosclipboard-private.h"
#include "gdkmacospasteboard-private.h"
#include "gdkmacosutils-private.h"
#include "gdkprivate.h"

struct _GdkMacosClipboard
{
  GdkClipboard  parent_instance;
  NSPasteboard *pasteboard;
  NSInteger     last_change_count;
};

G_DEFINE_TYPE (GdkMacosClipboard, _gdk_macos_clipboard, GDK_TYPE_CLIPBOARD)

static void
_gdk_macos_clipboard_load_contents (GdkMacosClipboard *self)
{
  GdkContentFormats *formats;
  NSInteger change_count;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));

  change_count = [self->pasteboard changeCount];

  formats = _gdk_macos_pasteboard_load_formats (self->pasteboard);
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (self), formats);
  gdk_content_formats_unref (formats);

  self->last_change_count = change_count;
}

static void
_gdk_macos_clipboard_read_async (GdkClipboard        *clipboard,
                                 GdkContentFormats   *formats,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  _gdk_macos_pasteboard_read_async (G_OBJECT (clipboard),
                                    GDK_MACOS_CLIPBOARD (clipboard)->pasteboard,
                                    formats,
                                    io_priority,
                                    cancellable,
                                    callback,
                                    user_data);
}

static GInputStream *
_gdk_macos_clipboard_read_finish (GdkClipboard  *clipboard,
                                  GAsyncResult  *result,
                                  const char   **out_mime_type,
                                  GError       **error)
{
  return _gdk_macos_pasteboard_read_finish (G_OBJECT (clipboard), result, out_mime_type, error);
}

static void
_gdk_macos_clipboard_send_to_pasteboard (GdkMacosClipboard  *self,
                                         GdkContentProvider *content)
{
  GdkMacosPasteboardItem *item;
  NSArray<NSPasteboardItem *> *items;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));
  g_assert (GDK_IS_CONTENT_PROVIDER (content));

  if (self->pasteboard == NULL)
    return;

  GDK_BEGIN_MACOS_ALLOC_POOL;

  item = [[GdkMacosPasteboardItem alloc] initForClipboard:GDK_CLIPBOARD (self) withContentProvider:content];
  items = [NSArray arrayWithObject:item];

  [self->pasteboard clearContents];
  if ([self->pasteboard writeObjects:items] == NO)
    g_warning ("Failed to send clipboard to pasteboard");

  self->last_change_count = [self->pasteboard changeCount];

  GDK_END_MACOS_ALLOC_POOL;
}

static gboolean
_gdk_macos_clipboard_claim (GdkClipboard       *clipboard,
                            GdkContentFormats  *formats,
                            gboolean            local,
                            GdkContentProvider *provider)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)clipboard;
  gboolean ret;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (formats != NULL);
  g_assert (!provider || GDK_IS_CONTENT_PROVIDER (provider));

  ret = GDK_CLIPBOARD_CLASS (_gdk_macos_clipboard_parent_class)->claim (clipboard, formats, local, provider);

  if (local)
    _gdk_macos_clipboard_send_to_pasteboard (self, provider);

  return ret;
}

static void
_gdk_macos_clipboard_constructed (GObject *object)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)object;

  if (self->pasteboard == nil)
    self->pasteboard = [[NSPasteboard generalPasteboard] retain];

  G_OBJECT_CLASS (_gdk_macos_clipboard_parent_class)->constructed (object);
}

static void
_gdk_macos_clipboard_finalize (GObject *object)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)object;

  if (self->pasteboard != nil)
    {
      [self->pasteboard release];
      self->pasteboard = nil;
    }

  G_OBJECT_CLASS (_gdk_macos_clipboard_parent_class)->finalize (object);
}

static void
_gdk_macos_clipboard_class_init (GdkMacosClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (klass);

  object_class->constructed = _gdk_macos_clipboard_constructed;
  object_class->finalize = _gdk_macos_clipboard_finalize;

  clipboard_class->claim = _gdk_macos_clipboard_claim;
  clipboard_class->read_async = _gdk_macos_clipboard_read_async;
  clipboard_class->read_finish = _gdk_macos_clipboard_read_finish;
}

static void
_gdk_macos_clipboard_init (GdkMacosClipboard *self)
{
}

GdkClipboard *
_gdk_macos_clipboard_new (GdkMacosDisplay *display)
{
  GdkMacosClipboard *self;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_MACOS_CLIPBOARD,
                       "display", display,
                       NULL);

  _gdk_macos_clipboard_load_contents (self);

  return GDK_CLIPBOARD (self);
}

void
_gdk_macos_clipboard_check_externally_modified (GdkMacosClipboard *self)
{
  g_return_if_fail (GDK_IS_MACOS_CLIPBOARD (self));

  if ([self->pasteboard changeCount] != self->last_change_count)
    _gdk_macos_clipboard_load_contents (self);
}
