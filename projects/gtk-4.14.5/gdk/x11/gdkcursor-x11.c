/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/* needs to be first because any header might include gdk-pixbuf.h otherwise */
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <string.h>

static void
gdk_x11_cursor_remove_from_cache (gpointer data, GObject *cursor)
{
  GdkDisplay *display = data;
  Cursor xcursor;

  xcursor = GDK_POINTER_TO_XID (g_hash_table_lookup (GDK_X11_DISPLAY (display)->cursors, cursor));
  XFreeCursor (GDK_DISPLAY_XDISPLAY (display), xcursor);
  g_hash_table_remove (GDK_X11_DISPLAY (display)->cursors, cursor);
}

void
_gdk_x11_cursor_display_finalize (GdkDisplay *display)
{
  GHashTableIter iter;
  gpointer cursor;

  if (GDK_X11_DISPLAY (display)->cursors)
    {
      g_hash_table_iter_init (&iter, GDK_X11_DISPLAY (display)->cursors);
      while (g_hash_table_iter_next (&iter, &cursor, NULL))
        g_object_weak_unref (G_OBJECT (cursor), gdk_x11_cursor_remove_from_cache, display);
      g_hash_table_unref (GDK_X11_DISPLAY (display)->cursors);
    }
}

static Cursor
get_blank_cursor (GdkDisplay *display)
{
  Pixmap pixmap;
  XColor color;
  Cursor cursor;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = _gdk_x11_display_create_bitmap_surface (display, 1, 1);
  /* Clear surface */
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_destroy (cr);
 
  pixmap = cairo_xlib_surface_get_drawable (surface);

  color.pixel = 0; 
  color.red = color.blue = color.green = 0;

  if (gdk_display_is_closed (display))
    cursor = None;
  else
    cursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
                                  pixmap, pixmap,
                                  &color, &color, 1, 1);
  cairo_surface_destroy (surface);

  return cursor;
}

static const struct {
  const char *css_name;
  const char *traditional_name;
  int cursor_glyph;
} name_map[] = {
  { "default",      "left_ptr",            XC_left_ptr, },
  { "help",         "question_arrow",      XC_question_arrow },
  { "context-menu", "left_ptr",            XC_left_ptr },
  { "pointer",      "hand",                XC_hand1 },
  { "progress",     "left_ptr_watch",      XC_watch },
  { "wait",         "watch",               XC_watch },
  { "cell",         "crosshair",           XC_plus },
  { "crosshair",    "cross",               XC_crosshair },
  { "text",         "xterm",               XC_xterm },
  { "vertical-text","xterm",               XC_xterm },
  { "alias",        "dnd-link",            XC_target },
  { "copy",         "dnd-copy",            XC_target },
  { "move",         "dnd-move",            XC_target },
  { "no-drop",      "dnd-none",            XC_pirate },
  { "dnd-ask",      "dnd-copy",            XC_target }, /* not CSS, but we want to guarantee it anyway */
  { "not-allowed",  "crossed_circle",      XC_pirate },
  { "grab",         "hand2",               XC_hand2 },
  { "grabbing",     "hand2",               XC_hand2 },
  { "all-scroll",   "left_ptr",            XC_left_ptr },
  { "col-resize",   "h_double_arrow",      XC_sb_h_double_arrow },
  { "row-resize",   "v_double_arrow",      XC_sb_v_double_arrow },
  { "n-resize",     "top_side",            XC_top_side },
  { "e-resize",     "right_side",          XC_right_side },
  { "s-resize",     "bottom_side",         XC_bottom_side },
  { "w-resize",     "left_side",           XC_left_side },
  { "ne-resize",    "top_right_corner",    XC_top_right_corner },
  { "nw-resize",    "top_left_corner",     XC_top_left_corner },
  { "se-resize",    "bottom_right_corner", XC_bottom_right_corner },
  { "sw-resize",    "bottom_left_corner",  XC_bottom_left_corner },
  { "ew-resize",    "h_double_arrow",      XC_sb_h_double_arrow },
  { "ns-resize",    "v_double_arrow",      XC_sb_v_double_arrow },
  { "nesw-resize",  "fd_double_arrow",     XC_X_cursor },
  { "nwse-resize",  "bd_double_arrow",     XC_X_cursor },
  { "zoom-in",      "left_ptr",            XC_draped_box },
  { "zoom-out",     "left_ptr",            XC_draped_box }
};

#ifdef HAVE_XCURSOR

static XcursorImage*
create_cursor_image (GdkTexture *texture,
                     int         x,
                     int         y,
		     int         scale)
{
  XcursorImage *xcimage;

  xcimage = XcursorImageCreate (gdk_texture_get_width (texture), gdk_texture_get_height (texture));

  xcimage->xhot = x;
  xcimage->yhot = y;

  gdk_texture_download (texture,
                        (guchar *) xcimage->pixels,
                        gdk_texture_get_width (texture) * 4);

  return xcimage;
}

static Cursor
gdk_x11_cursor_create_for_texture (GdkDisplay *display,
                                   GdkTexture *texture,
                                   int         x,
                                   int         y)
{
  XcursorImage *xcimage;
  Cursor xcursor;
  int target_scale;

  target_scale =
    gdk_monitor_get_scale_factor (gdk_x11_display_get_primary_monitor (display));
  xcimage = create_cursor_image (texture, x, y, target_scale);
  xcursor = XcursorImageLoadCursor (GDK_DISPLAY_XDISPLAY (display), xcimage);
  XcursorImageDestroy (xcimage);

  return xcursor;
}

static const char *
name_fallback (const char *name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (name_map); i++)
    {
      if (g_str_equal (name_map[i].css_name, name))
        return name_map[i].traditional_name;
    }

  return NULL;
}

static Cursor
gdk_x11_cursor_create_for_name (GdkDisplay *display,
                                const char *name)
{
  Cursor xcursor;
  Display *xdisplay;

  if (strcmp (name, "none") == 0)
    {
      xcursor = get_blank_cursor (display);
    }
  else
    {
      xdisplay = GDK_DISPLAY_XDISPLAY (display);
      xcursor = XcursorLibraryLoadCursor (xdisplay, name);
      if (xcursor == None)
        {
          const char *fallback;

          fallback = name_fallback (name);
          if (fallback)
            xcursor = XcursorLibraryLoadCursor (xdisplay, fallback);
        }
    }

  return xcursor;
}

#else

static Cursor
gdk_x11_cursor_create_for_texture (GdkDisplay *display,
                                   GdkTexture *texture,
                                   int         x,
                                   int         y)
{
  return None;
}

static Cursor
gdk_x11_cursor_create_for_name (GdkDisplay  *display,
                                const char *name)
{
  int i;

  if (g_str_equal (name, "none"))
    return get_blank_cursor (display);

  for (i = 0; i < G_N_ELEMENTS (name_map); i++)
    {
      if (g_str_equal (name_map[i].css_name, name) ||
          g_str_equal (name_map[i].traditional_name, name))
        return XCreateFontCursor (GDK_DISPLAY_XDISPLAY (display), name_map[i].cursor_glyph);
    }

  return None;
}

#endif

/**
 * gdk_x11_display_set_cursor_theme:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @theme: (nullable): the name of the cursor theme to use, or %NULL
 *   to unset a previously set value
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken.
 *
 * If the windowing system supports it, existing cursors created
 * with [ctor@Gdk.Cursor.new_from_name] are updated to reflect the theme
 * change. Custom cursors constructed with [ctor@Gdk.Cursor.new_from_texture]
 * will have to be handled by the application (GTK applications can learn
 * about cursor theme changes by listening for change notification
 * for the corresponding `GtkSetting`).
 */
void
gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
                                  const char *theme,
                                  const int    size)
{
#if defined(HAVE_XCURSOR) && defined(HAVE_XFIXES) && XFIXES_MAJOR >= 2
  GdkX11Screen *x11_screen;
  Display *xdisplay;
  char *old_theme;
  int real_size;
  int old_size;
  gpointer cursor, xcursor;
  GHashTableIter iter;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  x11_screen = gdk_x11_display_get_screen (display);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  real_size = size * x11_screen->surface_scale;

  old_theme = XcursorGetTheme (xdisplay);
  old_size = XcursorGetDefaultSize (xdisplay);

  if (old_size == real_size &&
      (old_theme == theme ||
       (old_theme && theme && strcmp (old_theme, theme) == 0)))
    return;

  XcursorSetTheme (xdisplay, theme);
  if (real_size > 0)
    XcursorSetDefaultSize (xdisplay, real_size);

  if (GDK_X11_DISPLAY (display)->cursors == NULL)
    return;

  g_hash_table_iter_init (&iter, GDK_X11_DISPLAY (display)->cursors);
  while (g_hash_table_iter_next (&iter, &cursor, &xcursor))
    {
      const char *name = gdk_cursor_get_name (cursor);
      
      if (name)
        {
          Cursor new_cursor = gdk_x11_cursor_create_for_name (display, name);

          if (new_cursor != None)
            {
              XFixesChangeCursor (xdisplay, new_cursor, GDK_POINTER_TO_XID (xcursor));
              g_hash_table_iter_replace (&iter, GDK_XID_TO_POINTER (new_cursor));
            }
          else
            {
              g_hash_table_iter_remove (&iter);
            }
        }
    }
#endif
}

/**
 * gdk_x11_display_get_xcursor:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @cursor: a `GdkCursor`
 *
 * Returns the X cursor belonging to a `GdkCursor`, potentially
 * creating the cursor.
 *
 * Be aware that the returned cursor may not be unique to @cursor.
 * It may for example be shared with its fallback cursor. On old
 * X servers that don't support the XCursor extension, all cursors
 * may even fall back to a few default cursors.
 *
 * Returns: an Xlib Cursor.
 */
Cursor
gdk_x11_display_get_xcursor (GdkDisplay *display,
                             GdkCursor  *cursor)
{
  GdkX11Display *x11_display = GDK_X11_DISPLAY (display);
  Cursor xcursor;

  g_return_val_if_fail (cursor != NULL, None);

  if (gdk_display_is_closed (display))
    return None;

  if (x11_display->cursors == NULL)
    x11_display->cursors = g_hash_table_new (gdk_cursor_hash, gdk_cursor_equal);

  xcursor = GDK_POINTER_TO_XID (g_hash_table_lookup (x11_display->cursors, cursor));
  if (xcursor)
    return xcursor;

  if (gdk_cursor_get_name (cursor))
    xcursor = gdk_x11_cursor_create_for_name (display, gdk_cursor_get_name (cursor));
  else
    xcursor = gdk_x11_cursor_create_for_texture (display,
                                                 gdk_cursor_get_texture (cursor),
                                                 gdk_cursor_get_hotspot_x (cursor),
                                                 gdk_cursor_get_hotspot_y (cursor));

  if (xcursor != None)
    {
      g_object_weak_ref (G_OBJECT (cursor), gdk_x11_cursor_remove_from_cache, display);
      g_hash_table_insert (x11_display->cursors, cursor, GDK_XID_TO_POINTER (xcursor));
      return xcursor;
    }
      
  if (gdk_cursor_get_fallback (cursor))
    return gdk_x11_display_get_xcursor (display, gdk_cursor_get_fallback (cursor));

  return None;
}

