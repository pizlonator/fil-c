/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdk/gdkframeclock.h"
#include "gdk/gdkdisplay.h"

/* Ensure we included config.h as needed for the below HAVE_SYSPROF_CAPTURE check */
#ifndef GETTEXT_PACKAGE
#error "config.h was not included before gdkprofilerprivate.h."
#endif

#ifdef HAVE_SYSPROF
#include <sysprof-capture.h>
#endif

G_BEGIN_DECLS

#ifdef HAVE_SYSPROF
#define GDK_PROFILER_IS_RUNNING (gdk_profiler_is_running ())
#define GDK_PROFILER_CURRENT_TIME SYSPROF_CAPTURE_CURRENT_TIME
#else
#define GDK_PROFILER_IS_RUNNING 0
#define GDK_PROFILER_CURRENT_TIME 0
#endif

gboolean gdk_profiler_is_running (void);

/* Note: Times and durations are in nanoseconds;
 * g_get_monotonic_time(), and GdkFrameClock times
 * are in microseconds, so multiply by 1000.
 */
void   gdk_profiler_add_mark  (gint64       begin_time,
                               gint64       duration,
                               const gchar *name,
                               const gchar *message);
void   gdk_profiler_add_markf (gint64       begin_time,
                               gint64       duration,
                               const gchar *name,
                               const gchar *message_format,
                               ...) G_GNUC_PRINTF (4, 5);
void   gdk_profiler_end_mark  (gint64       begin_time,
                               const gchar *name,
                               const gchar *message);
void   gdk_profiler_end_markf (gint64       begin_time,
                               const gchar *name,
                               const gchar *message_format,
                               ...) G_GNUC_PRINTF (3, 4);

guint   gdk_profiler_define_counter     (const char *name,
                                         const char *description);
guint   gdk_profiler_define_int_counter (const char *name,
                                         const char *description);
void    gdk_profiler_set_counter        (guint  id,
                                         double value);
void    gdk_profiler_set_int_counter    (guint  id,
                                         gint64 value);

#ifndef HAVE_SYSPROF
#define gdk_profiler_add_mark(b, d, n, m) G_STMT_START {} G_STMT_END
#define gdk_profiler_end_mark(b, n, m) G_STMT_START {} G_STMT_END
/* Optimise the whole call out */
#if defined(G_HAVE_ISO_VARARGS)
#define gdk_profiler_add_markf(b, d, n, m, ...) G_STMT_START {} G_STMT_END
#define gdk_profiler_end_markf(b, n, m, ...) G_STMT_START {} G_STMT_END
#elif defined(G_HAVE_GNUC_VARARGS)
#define gdk_profiler_add_markf(b, d, n, m...) G_STMT_START {} G_STMT_END
#define gdk_profiler_end_markf(b, n, m...) G_STMT_START {} G_STMT_END
#else
/* no varargs macro support; the call will have to be optimised out by the compiler */
#endif

#define gdk_profiler_define_counter(n, d) 0
#define gdk_profiler_define_int_counter(n, d) 0
#define gdk_profiler_set_counter(i, v) G_STMT_START {} G_STMT_END
#define gdk_profiler_set_int_counter(i, v) G_STMT_START {} G_STMT_END
#endif

G_END_DECLS

