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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#pragma once

#include <gdk/gdkcursor.h>
#include "gdksurface-broadway.h"
#include "gdkdisplay-broadway.h"
#include "gdkdrawcontext-broadway.h"

#include "gdkbroadwaycursor.h"
#include "gdkbroadwaysurface.h"

guint32 gdk_broadway_display_ensure_texture (GdkDisplay *display,
                                             GdkTexture *texture);

void gdk_broadway_display_flush_in_idle (GdkDisplay *display);

void gdk_broadway_surface_set_nodes (GdkSurface *surface,
                                     GArray *nodes,
                                     GPtrArray *node_textures);

GdkDrag * _gdk_broadway_surface_drag_begin        (GdkSurface          *surface,
                                                   GdkDevice          *device,
                                                   GdkContentProvider *content,
                                                   GdkDragAction       actions,
                                                   double              dx,
                                                   double              dy);
void     _gdk_broadway_surface_translate          (GdkSurface *surface,
                                                   cairo_region_t *area,
                                                   int        dx,
                                                   int        dy);
gboolean _gdk_broadway_moveresize_handle_event    (GdkDisplay *display,
                                                   BroadwayInputMsg *msg);
gboolean _gdk_broadway_moveresize_configure_done  (GdkDisplay *display,
                                                   GdkSurface  *surface);
void     _gdk_broadway_roundtrip_notify           (GdkSurface  *surface,
                                                   guint32 tag,
                                                   gboolean local_reply);
void     _gdk_broadway_surface_grab_check_destroy (GdkSurface *surface);
void     _gdk_broadway_surface_grab_check_unmap   (GdkSurface *surface,
                                                   gulong     serial);

void gdk_broadway_surface_move_resize (GdkSurface *surface,
                                       int         x,
                                       int         y,
                                       int         width,
                                       int         height);

void _gdk_keymap_keys_changed     (GdkDisplay      *display);
int  _gdk_broadway_get_group_for_state (GdkDisplay      *display,
                                        GdkModifierType  state);
void _gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
                                               GdkModifierType *modifiers);
gboolean _gdk_keymap_key_is_modifier   (GdkKeymap       *keymap,
                                        guint            keycode);

void _gdk_broadway_display_size_changed (GdkDisplay *display,
                                         BroadwayInputScreenResizeNotify *msg);

void _gdk_broadway_events_got_input      (GdkDisplay *display,
                                          BroadwayInputMsg *message);

void _gdk_broadway_display_init_root_window (GdkDisplay *display);
GdkDisplay * _gdk_broadway_display_open (const char *display_name);
void _gdk_broadway_display_queue_events (GdkDisplay *display);
GdkCursor*_gdk_broadway_display_get_cursor_for_name (GdkDisplay  *display,
                                                     const char *name);
GdkCursor *_gdk_broadway_display_get_cursor_for_texture (GdkDisplay *display,
                                                         GdkTexture *texture,
                                                         int         x,
                                                         int         y);
gboolean _gdk_broadway_display_supports_cursor_alpha (GdkDisplay *display);
gboolean _gdk_broadway_display_supports_cursor_color (GdkDisplay *display);
void _gdk_broadway_display_get_default_cursor_size (GdkDisplay *display,
                                                    guint       *width,
                                                    guint       *height);
void _gdk_broadway_display_get_maximal_cursor_size (GdkDisplay *display,
                                                    guint       *width,
                                                    guint       *height);
GdkKeymap* _gdk_broadway_display_get_keymap (GdkDisplay *display);
void _gdk_broadway_display_consume_all_input (GdkDisplay *display);
BroadwayInputMsg * _gdk_broadway_display_block_for_input (GdkDisplay *display,
                                                          char op,
                                                          guint32 serial,
                                                          gboolean remove);

/* Surface methods - testing */
void _gdk_broadway_surface_resize_surface        (GdkSurface *surface);

void _gdk_broadway_cursor_update_theme (GdkCursor *cursor);
void _gdk_broadway_cursor_display_finalize (GdkDisplay *display);

