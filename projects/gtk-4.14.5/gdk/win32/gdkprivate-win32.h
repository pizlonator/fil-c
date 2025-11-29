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

#pragma once

#include "config.h"

#include <gdk/gdkcursorprivate.h>
#include <gdk/gdkdebugprivate.h>
#include <gdk/win32/gdksurface-win32.h>
#include <gdk/win32/gdkwin32display.h>
#include <gdk/win32/gdkwin32screen.h>
#include <gdk/win32/gdkwin32keys.h>
#include <gdk/win32/gdkdevicemanager-win32.h>
#include <gdk/win32/gdkclipdrop-win32.h>


/* Old debug macros */

#define GDK_NOTE(type,action)                             \
    G_STMT_START {                                        \
      if (GDK_DEBUG_CHECK (type))                         \
         { action; };                                     \
    } G_STMT_END

/* According to
 * http://blog.airesoft.co.uk/2009/11/wm_messages/
 * this is the actual internal name MS uses for this undocumented message.
 * According to
 * https://bugs.winehq.org/show_bug.cgi?id=15055
 * wParam is 0
 * lParam is a pair of virtual desktop coordinates for the popup
 */
#ifndef WM_SYSMENU
#define WM_SYSMENU 0x313
#endif

/* Define some combinations of GdkDebugFlags */
#define GDK_DEBUG_EVENTS_OR_INPUT (GDK_DEBUG_EVENTS|GDK_DEBUG_INPUT)
#define GDK_DEBUG_MISC_OR_EVENTS (GDK_DEBUG_MISC|GDK_DEBUG_EVENTS)

GdkWin32Screen *GDK_SURFACE_SCREEN(GObject *win);

/* Use this for hWndInsertAfter (2nd argument to SetWindowPos()) if
 * SWP_NOZORDER flag is used. Otherwise it's unobvious why a particular
 * argument is used. Using NULL is misleading, because
 * NULL is equivalent to HWND_TOP.
 */
#define SWP_NOZORDER_SPECIFIED HWND_TOP

typedef enum
{
  GDK_DRAG_PROTO_NONE = 0,
  GDK_DRAG_PROTO_WIN32_DROPFILES,
  GDK_DRAG_PROTO_OLE2,
} GdkDragProtocol;

gulong _gdk_win32_get_next_tick (gulong suggested_tick);
BOOL _gdk_win32_get_cursor_pos (LPPOINT lpPoint);

gboolean _gdk_win32_surface_enable_transparency (GdkSurface *window);

void _gdk_win32_dnd_exit (void);

void     gdk_win32_handle_table_insert  (HANDLE   *handle,
                                         gpointer data);
void     gdk_win32_handle_table_remove  (HANDLE handle);

cairo_region_t *_gdk_win32_hrgn_to_region    (HRGN  hrgn,
                                              guint scale);

void    _gdk_win32_adjust_client_rect   (GdkSurface *window,
                                         RECT      *RECT);

void       _gdk_push_modal_window   (GdkSurface *window);
void       _gdk_remove_modal_window (GdkSurface *window);
GdkSurface *_gdk_modal_current       (void);
gboolean   _gdk_modal_blocked       (GdkSurface *window);

gboolean gdk_win32_ensure_com (void);
gboolean gdk_win32_ensure_ole (void);

void   _gdk_win32_print_dc             (HDC          hdc);

char *_gdk_win32_surface_state_to_string (GdkToplevelState state);
char *_gdk_win32_surface_style_to_string (LONG style);
char *_gdk_win32_surface_exstyle_to_string (LONG style);
char *_gdk_win32_surface_pos_bits_to_string (UINT flags);
char *_gdk_win32_drag_action_to_string (GdkDragAction actions);

char *_gdk_win32_message_to_string    (UINT         msg);
char *_gdk_win32_key_to_string        (LONG         lParam);
char *_gdk_win32_cf_to_string         (UINT         format);
char *_gdk_win32_rect_to_string       (const RECT  *rect);

void   _gdk_win32_print_event            (GdkEvent     *event);

void    _gdk_win32_api_failed        (const char *where,
                                      const char *api);
void    _gdk_other_api_failed        (const char *where,
                                      const char *api);

#define WIN32_API_FAILED(api) _gdk_win32_api_failed (G_STRLOC , api)
#define WIN32_GDI_FAILED(api) WIN32_API_FAILED (api)
#define OTHER_API_FAILED(api) _gdk_other_api_failed (G_STRLOC, api)

#define WIN32_API_FAILED_LOG_ONCE(api) G_STMT_START { static gboolean logged = 0; if (!logged) { _gdk_win32_api_failed (G_STRLOC , api); logged = 1; }} G_STMT_END

/* These two macros call a GDI or other Win32 API and if the return
 * value is zero or NULL, print a warning message. The majority of GDI
 * calls return zero or NULL on failure. The value of the macros is nonzero
 * if the call succeeded, zero otherwise.
 */

#define GDI_CALL(api, arglist) (api arglist ? 1 : (WIN32_GDI_FAILED (#api), 0))
#define API_CALL(api, arglist) (api arglist ? 1 : (WIN32_API_FAILED (#api), 0))

#define HR_LOG(hr)

#define HR_CHECK_RETURN(hr) { if G_UNLIKELY (FAILED (hr)) return; }
#define HR_CHECK_RETURN_VAL(hr, val) { if G_UNLIKELY (FAILED (hr)) return val; }
#define HR_CHECK_GOTO(hr, label) { if G_UNLIKELY (FAILED (hr)) goto label; }

extern LRESULT CALLBACK _gdk_win32_surface_procedure (HWND, UINT, WPARAM, LPARAM);

extern GdkDisplay       *_gdk_display;

extern GdkDeviceManagerWin32 *_gdk_device_manager;

extern int               _gdk_input_ignore_core;

/* These are thread specific, but GDK/win32 works OK only when invoked
 * from a single thread anyway.
 */
extern HKL               _gdk_input_locale;
extern gboolean          _gdk_input_locale_is_ime;

extern guint             _gdk_keymap_serial;

/* The singleton clipdrop object pointer */
extern GdkWin32Clipdrop *_win32_clipdrop;

/* Used to identify the main thread */
extern GThread          *_win32_main_thread;

typedef enum {
  GDK_WIN32_MODAL_OP_NONE = 0x0,
  GDK_WIN32_MODAL_OP_SIZE = 0x1 << 0,
  GDK_WIN32_MODAL_OP_MOVE = 0x1 << 1,
  GDK_WIN32_MODAL_OP_MENU = 0x1 << 2,
  GDK_WIN32_MODAL_OP_DND  = 0x1 << 3
} GdkWin32ModalOpKind;

#define GDK_WIN32_MODAL_OP_SIZEMOVE_MASK (GDK_WIN32_MODAL_OP_SIZE | GDK_WIN32_MODAL_OP_MOVE)

/* Non-zero while a modal sizing, moving, or dnd operation is in progress */
extern GdkWin32ModalOpKind _modal_operation_in_progress;

extern HWND             _modal_move_resize_window;

void  _gdk_win32_begin_modal_call (GdkWin32ModalOpKind kind);
void  _gdk_win32_end_modal_call (GdkWin32ModalOpKind kind);

void _gdk_win32_display_init_cursors (GdkWin32Display     *display);
void _gdk_win32_display_finalize_cursors (GdkWin32Display *display);
void _gdk_win32_display_update_cursors (GdkWin32Display   *display);

typedef struct _Win32CursorTheme Win32CursorTheme;

struct _Win32CursorTheme {
  GHashTable *named_cursors;
};

typedef enum GdkWin32CursorLoadType {
  GDK_WIN32_CURSOR_LOAD_FROM_FILE = 0,
  GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL = 1,
  GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_THIS = 2,
  GDK_WIN32_CURSOR_CREATE = 3,
  GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_GTK = 4,
} GdkWin32CursorLoadType;

typedef struct _Win32Cursor Win32Cursor;

struct _Win32Cursor {
  GdkWin32CursorLoadType load_type;
  gunichar2 *resource_name;
  int width;
  int height;
  guint load_flags;
  int xcursor_number;
};

Win32CursorTheme *win32_cursor_theme_load             (const char       *name,
                                                       int               size);
Win32Cursor *     win32_cursor_theme_get_cursor       (Win32CursorTheme *theme,
                                                       const char       *name);
void              win32_cursor_theme_destroy          (Win32CursorTheme *theme);
Win32CursorTheme *_gdk_win32_display_get_cursor_theme (GdkWin32Display  *win32_display);

HICON _gdk_win32_create_hicon_for_texture (GdkTexture *texture,
                                           gboolean    is_icon,
                                           int         x,
                                           int         y);

gboolean _gdk_win32_display_has_pending (GdkDisplay *display);
void _gdk_win32_display_queue_events (GdkDisplay *display);

guint8     _gdk_win32_keymap_get_active_group    (GdkWin32Keymap *keymap);
guint8     _gdk_win32_keymap_get_rshift_scancode (GdkWin32Keymap *keymap);
void       _gdk_win32_keymap_set_active_layout   (GdkWin32Keymap *keymap,
                                                  HKL             hkl);
GdkModifierType _gdk_win32_keymap_get_mod_mask   (GdkWin32Keymap *keymap);

GdkKeymap *_gdk_win32_display_get_keymap (GdkDisplay *display);

/* stray GdkSurfaceImplWin32 members */
void _gdk_win32_surface_register_dnd (GdkSurface *window);
void _gdk_win32_surface_unregister_dnd (GdkSurface *window);

GdkDrag *_gdk_win32_surface_drag_begin (GdkSurface         *window,
                                        GdkDevice          *device,
                                        GdkContentProvider *content,
                                        GdkDragAction       actions,
                                        double              x_root,
                                        double              y_root);

/* Stray GdkWin32Screen members */
gboolean _gdk_win32_get_setting (const char *name, GValue *value);
void _gdk_win32_screen_on_displaychange_event (GdkWin32Screen *screen);

/* Distributed display manager implementation */
GdkDisplay *_gdk_win32_display_open (const char *display_name);
void _gdk_win32_append_event (GdkEvent *event);

void     _gdk_win32_surface_handle_aerosnap      (GdkSurface            *window,
                                                  GdkWin32AeroSnapCombo combo);

gboolean _gdk_win32_get_window_rect             (GdkSurface  *window,
                                                 RECT       *rect);
void     _gdk_win32_do_emit_configure_event     (GdkSurface  *window,
                                                 RECT        rect);
void      gdk_win32_surface_do_move_resize_drag  (GdkSurface  *window,
                                                  int         x,
                                                  int         y);
void      gdk_win32_surface_end_move_resize_drag (GdkSurface  *window);
gboolean _gdk_win32_surface_fill_min_max_info    (GdkSurface  *window,
                                                  MINMAXINFO *mmi);

gboolean _gdk_win32_surface_lacks_wm_decorations (GdkSurface *window);

void gdk_win32_surface_show (GdkSurface *surface,
                             gboolean    already_mapped);
void gdk_win32_surface_raise (GdkSurface *surface);
void gdk_win32_surface_set_opacity (GdkSurface *surface,
                                    double      opacity);
void gdk_win32_surface_resize (GdkSurface *surface,
                               int         width,
                               int         height);

BOOL WINAPI GtkShowWindow (GdkSurface *window,
                           int        cmd_show);

/* Initialization */
void _gdk_win32_surfaceing_init (void);
void _gdk_drag_init    (void);
void _gdk_events_init (GdkDisplay *display);

typedef enum _GdkWin32ProcessorCheckType
{
  GDK_WIN32_ARM64,
  GDK_WIN32_WOW64,
} GdkWin32ProcessorCheckType;

gboolean _gdk_win32_check_processor (GdkWin32ProcessorCheckType check_type);

GdkPixbuf    *gdk_win32_icon_to_pixbuf_libgtk_only (HICON hicon,
                                                    double *x_hot,
                                                    double *y_hot);
void          gdk_win32_set_modal_dialog_libgtk_only (HWND window);

gpointer      gdk_win32_handle_table_lookup_       (HWND handle);

extern IMAGE_DOS_HEADER __ImageBase;

static inline HMODULE
this_module (void)
{
  return (HMODULE) &__ImageBase;
}

