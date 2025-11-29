/* gdkcursor-macos.c
 *
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2020 Red Hat, Inc.
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

#include "config.h"

#include <string.h>

#include "gdkmacoscursor-private.h"

@interface NSCursor()
-(long long)_coreCursorType;
@end

/* OS X only exports a number of cursor types in its public NSCursor interface.
 * By overriding the private _coreCursorType method, we can tell OS X to load
 * one of its internal cursors instead (since cursor images are loaded on demand
 * instead of in advance). WebKit does this too.
 */

@interface gdkCoreCursor : NSCursor {
@private
        int type;
        BOOL override;
}
@end

@implementation gdkCoreCursor

- (long long)_coreCursorType
{
  if (self->override)
    return self->type;
  return [super _coreCursorType];
}

#define CUSTOM_CURSOR_CTOR(name, id) \
        + (gdkCoreCursor *)name \
        { \
                gdkCoreCursor *obj; \
                obj = [self new]; \
                if (obj) { \
                        obj->override = YES; \
                        obj->type = id; \
                } \
                return obj; \
        }
CUSTOM_CURSOR_CTOR(gdkHelpCursor, 40)
CUSTOM_CURSOR_CTOR(gdkProgressCursor, 4)
/* TODO OS X doesn't seem to have a way to get this. There is an undocumented
 * method +[NSCursor _waitCursor], but it doesn't actually return this cursor,
 * but rather some odd low-quality non-animating version of this cursor. Use
 * the progress cursor instead for now.
 */
CUSTOM_CURSOR_CTOR(gdkWaitCursor, 4)
CUSTOM_CURSOR_CTOR(gdkAliasCursor, 2)
CUSTOM_CURSOR_CTOR(gdkMoveCursor, 39)
/* TODO OS X doesn't seem to provide one; copy the move cursor for now
 *  since it looks similar to what we want. */
CUSTOM_CURSOR_CTOR(gdkAllScrollCursor, 39)
CUSTOM_CURSOR_CTOR(gdkNEResizeCursor, 29)
CUSTOM_CURSOR_CTOR(gdkNWResizeCursor, 33)
CUSTOM_CURSOR_CTOR(gdkSEResizeCursor, 35)
CUSTOM_CURSOR_CTOR(gdkSWResizeCursor, 37)
CUSTOM_CURSOR_CTOR(gdkEWResizeCursor, 28)
CUSTOM_CURSOR_CTOR(gdkNSResizeCursor, 32)
CUSTOM_CURSOR_CTOR(gdkNESWResizeCursor, 30)
CUSTOM_CURSOR_CTOR(gdkNWSEResizeCursor, 34)
CUSTOM_CURSOR_CTOR(gdkZoomInCursor, 42)
CUSTOM_CURSOR_CTOR(gdkZoomOutCursor, 43)
#undef CUSTOM_CURSOR_CTOR

@end

struct CursorsByName {
  const char *name;
  NSString *selector;
};

static const struct CursorsByName cursors_by_name[] = {
  /* Link & Status */
  { "context-menu", @"contextualMenuCursor" },
  { "help", @"gdkHelpCursor" },
  { "pointer", @"pointingHandCursor" },
  { "progress", @"gdkProgressCursor" },
  { "wait", @"gdkWaitCursor" },
  /* Selection */
  { "cell", @"crosshairCursor" },
  { "crosshair", @"crosshairCursor" },
  { "text", @"IBeamCursor" },
  { "vertical-text", @"IBeamCursorForVerticalLayout" },
  /* Drag & Drop */
  { "alias", @"gdkAliasCursor" },
  { "copy", @"dragCopyCursor" },
  { "move", @"gdkMoveCursor" },
  { "no-drop", @"operationNotAllowedCursor" },
  { "not-allowed", @"operationNotAllowedCursor" },
  { "grab", @"openHandCursor" },
  { "grabbing", @"closedHandCursor" },
  /* Resize & Scrolling */
  { "all-scroll", @"gdkAllScrollCursor" },
  { "col-resize", @"resizeLeftRightCursor" },
  { "row-resize", @"resizeUpDownCursor" },

  /* Undocumented cursors to match native resizing */
  { "e-resize", @"_windowResizeEastWestCursor" },
  { "w-resize", @"_windowResizeEastWestCursor" },
  { "n-resize", @"_windowResizeNorthSouthCursor" },
  { "s-resize", @"_windowResizeNorthSouthCursor" },

  { "ne-resize", @"gdkNEResizeCursor" },
  { "nw-resize", @"gdkNWResizeCursor" },
  { "se-resize", @"gdkSEResizeCursor" },
  { "sw-resize", @"gdkSWResizeCursor" },
  { "ew-resize", @"gdkEWResizeCursor" },
  { "ns-resize", @"gdkNSResizeCursor" },
  { "nesw-resize", @"gdkNESWResizeCursor" },
  { "nwse-resize", @"gdkNWSEResizeCursor" },
  /* Zoom */
  { "zoom-in", @"gdkZoomInCursor" },
  { "zoom-out", @"gdkZoomOutCursor" },
};

static NSCursor *
create_blank_cursor (void)
{
  NSCursor *nscursor;
  NSImage *nsimage;
  NSSize size = { 1.0, 1.0 };

  nsimage = [[NSImage alloc] initWithSize:size];
  nscursor = [[NSCursor alloc] initWithImage:nsimage
                               hotSpot:NSMakePoint(0.0, 0.0)];
  [nsimage release];

  return nscursor;
}

static NSCursor *
create_cursor_from_texture (GdkTexture *texture,
                            int         x,
                            int         y)
{
  guint32 width;
  guint32 height;
  guchar *pixels;
  gsize stride;
  GdkTextureDownloader *downloader;
  NSCursor *nscursor;
  NSBitmapImageRep *nsbitmap;
  NSImage *nsimage;

  if (texture == NULL)
    return create_blank_cursor ();

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  nsbitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                       pixelsWide:width
                                       pixelsHigh:height
                                       bitsPerSample:8
                                       samplesPerPixel:4
                                       hasAlpha:YES
                                       isPlanar:NO
                                       colorSpaceName:NSDeviceRGBColorSpace
                                       bytesPerRow:0
                                       bitsPerPixel:0];
  pixels = [nsbitmap bitmapData];
  stride = [nsbitmap bytesPerRow];

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
  gdk_texture_downloader_download_into (downloader,
                                        pixels,
                                        stride);
  gdk_texture_downloader_free (downloader);

  nsimage = [[NSImage alloc] init];
  [nsimage addRepresentation:nsbitmap];
  [nsimage setSize:NSMakeSize(width, height)];
  [nsbitmap release];

  nscursor = [[NSCursor alloc] initWithImage:nsimage
                               hotSpot:NSMakePoint(x, y)];
  [nsimage release];
  return nscursor;
}

NSCursor *
_gdk_macos_cursor_get_ns_cursor (GdkCursor *cursor)
{
  const char *name = NULL;
  NSCursor *nscursor;
  SEL selector = @selector(arrowCursor);

  g_return_val_if_fail (!cursor || GDK_IS_CURSOR (cursor), NULL);

  if (cursor != NULL)
    {
      name = gdk_cursor_get_name (cursor);

      if (name == NULL)
        {
          nscursor = create_cursor_from_texture (gdk_cursor_get_texture (cursor),
                                                 gdk_cursor_get_hotspot_x (cursor),
                                                 gdk_cursor_get_hotspot_y (cursor));
          return nscursor;
        }
    }

  if (name == NULL)
    goto load_cursor;

  if (strcmp (name, "none") == 0)
    return create_blank_cursor ();

  for (guint i = 0; i < G_N_ELEMENTS (cursors_by_name); i++)
    {
      if (strcmp (cursors_by_name[i].name, name) == 0)
        {
          selector = NSSelectorFromString (cursors_by_name[i].selector);
          break;
        }
    }

load_cursor:
  nscursor = [[gdkCoreCursor class] performSelector:selector];
  [nscursor retain];
  return nscursor;
}
