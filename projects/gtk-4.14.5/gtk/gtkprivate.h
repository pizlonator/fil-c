/* GTK - The GIMP Toolkit
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

#pragma once

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkdebugprivate.h>

#include "gtkcsstypesprivate.h"
#include "gtktexthandleprivate.h"
#include "gtkplacessidebarprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkwindowgroup.h"
#include "gtkdebug.h"

G_BEGIN_DECLS

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

#define OPPOSITE_ORIENTATION(_orientation) (1 - (_orientation))

#ifndef G_ENABLE_CONSISTENCY_CHECKS
/* This is true for buildtypes release and debugoptimized */
#define gtk_internal_return_if_fail(__expr) G_STMT_START{ (void)0; }G_STMT_END
#define gtk_internal_return_val_if_fail(__expr, __val) G_STMT_START{ (void)0; }G_STMT_END
#else
/* This is true for debug builds */
#define gtk_internal_return_if_fail(__expr) g_return_if_fail(__expr)
#define gtk_internal_return_val_if_fail(__expr, __val) g_return_val_if_fail(__expr, __val)
#endif

const char * _gtk_get_datadir            (void) G_GNUC_CONST;
const char * _gtk_get_libdir             (void) G_GNUC_CONST;
const char * _gtk_get_sysconfdir         (void) G_GNUC_CONST;
const char * _gtk_get_localedir          (void) G_GNUC_CONST;
const char * _gtk_get_data_prefix        (void) G_GNUC_CONST;

gboolean      _gtk_fnmatch                (const char *pattern,
                                           const char *string,
                                           gboolean    no_leading_period,
                                           gboolean    casefold);

char *        _gtk_make_ci_glob_pattern   (const char *pattern);


char        * _gtk_get_lc_ctype           (void) G_GNUC_MALLOC;

void          _gtk_ensure_resources       (void);

void          gtk_main_sync               (void);

GtkWidget *   gtk_window_group_get_current_grab (GtkWindowGroup *window_group);
void          gtk_grab_add                      (GtkWidget      *widget);
void          gtk_grab_remove                   (GtkWidget      *widget);

gboolean _gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

gboolean _gtk_single_string_accumulator   (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

gboolean         gtk_propagate_event_internal  (GtkWidget       *widget,
                                                GdkEvent        *event,
                                                GtkWidget       *topmost);
gboolean   gtk_propagate_event          (GtkWidget       *widget,
                                         GdkEvent        *event);
gboolean   gtk_main_do_event            (GdkEvent        *event);

GtkWidget *gtk_get_event_widget         (GdkEvent  *event);

guint32    gtk_get_current_event_time   (void);

void check_crossing_invariants (GtkWidget       *widget,
                                GtkCrossingData *crossing);

double _gtk_get_slowdown (void);
void    _gtk_set_slowdown (double slowdown_factor);

char *gtk_get_portal_request_path (GDBusConnection  *connection,
                                   char            **token) G_GNUC_MALLOC;
char *gtk_get_portal_session_path (GDBusConnection  *connection,
                                   char            **token) G_GNUC_MALLOC;
guint gtk_get_portal_interface_version (GDBusConnection *connection,
                                        const char      *interface_name);

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define PORTAL_SESSION_INTERFACE "org.freedesktop.portal.Session"
#define PORTAL_FILECHOOSER_INTERFACE "org.freedesktop.portal.FileChooser"
#define PORTAL_PRINT_INTERFACE "org.freedesktop.portal.Print"
#define PORTAL_SCREENSHOT_INTERFACE "org.freedesktop.portal.Screenshot"
#define PORTAL_INHIBIT_INTERFACE "org.freedesktop.portal.Inhibit"

#ifdef G_OS_WIN32
void _gtk_load_dll_with_libgtk3_manifest (const char *dllname);
#endif

void            gtk_set_display_debug_flags        (GdkDisplay    *display,
                                                    GtkDebugFlags  flags);
GtkDebugFlags   gtk_get_display_debug_flags        (GdkDisplay    *display);
gboolean        gtk_get_any_display_debug_flag_set (void);

GBytes *get_emoji_data (void);

#define GTK_DISPLAY_DEBUG_CHECK(display,type)                   \
  (gtk_get_any_display_debug_flag_set () &&                     \
   G_UNLIKELY (gtk_get_display_debug_flags (display) & GTK_DEBUG_##type))

#define GTK_DEBUG(type,...)                                     \
  G_STMT_START {                                                \
    if (GTK_DEBUG_CHECK (type))                                 \
      gdk_debug_message (__VA_ARGS__);                          \
  } G_STMT_END

#define GTK_DISPLAY_DEBUG(display,type,...)                     \
  G_STMT_START {                                                \
    if (GTK_DISPLAY_DEBUG_CHECK (display,type))                 \
      gdk_debug_message (__VA_ARGS__);                          \
  } G_STMT_END

char * _gtk_elide_underscores (const char *original);

void setlocale_initialization (void);

void gtk_synthesize_crossing_events (GtkRoot         *toplevel,
                                     GtkCrossingType  crossing_type,
                                     GtkWidget       *old_target,
                                     GtkWidget       *new_target,
                                     double           surface_x,
                                     double           surface_y,
                                     GdkCrossingMode  mode,
                                     GdkDrop         *drop);

G_END_DECLS

