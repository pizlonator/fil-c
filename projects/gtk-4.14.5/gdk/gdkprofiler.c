/* GDK - The GIMP Drawing Kit
 *
 * gdkprofiler.c: A simple profiler
 *
 * Copyright © 2018 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkprofilerprivate.h"

#include <sys/types.h>
#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "version/gdkversionmacros.h"
#include "gdkframeclockprivate.h"

#define CATEGORY "GTK"

gboolean
gdk_profiler_is_running (void)
{
#ifdef HAVE_SYSPROF
  return sysprof_collector_is_active ();
#else
  return FALSE;
#endif
}

void
(gdk_profiler_add_mark) (gint64      begin_time,
                         gint64      duration,
                         const char *name,
                         const char *message)
{
#ifdef HAVE_SYSPROF
  sysprof_collector_mark (begin_time, duration, CATEGORY, name, message);
#endif
}

void
(gdk_profiler_end_mark) (gint64      begin_time,
                         const char *name,
                         const char *message)
{
#ifdef HAVE_SYSPROF
  sysprof_collector_mark (begin_time, GDK_PROFILER_CURRENT_TIME - begin_time, CATEGORY, name, message);
#endif
}

void
(gdk_profiler_add_markf) (gint64       begin_time,
                          gint64       duration,
                          const gchar *name,
                          const gchar *message_format,
                          ...)
{
#ifdef HAVE_SYSPROF
  va_list args;
  va_start (args, message_format);
  sysprof_collector_mark_vprintf (begin_time, duration, CATEGORY, name, message_format, args);
  va_end (args);
#endif  /* HAVE_SYSPROF */
}

void
(gdk_profiler_end_markf) (gint64       begin_time,
                          const gchar *name,
                          const gchar *message_format,
                          ...)
{
#ifdef HAVE_SYSPROF
  va_list args;
  va_start (args, message_format);
  sysprof_collector_mark_vprintf (begin_time, GDK_PROFILER_CURRENT_TIME - begin_time, CATEGORY, name, message_format, args);
  va_end (args);
#endif  /* HAVE_SYSPROF */
}

guint
(gdk_profiler_define_counter) (const char *name,
                               const char *description)
{
#ifdef HAVE_SYSPROF
  SysprofCaptureCounter counter;

  counter.id = sysprof_collector_request_counters (1);
  counter.type = SYSPROF_CAPTURE_COUNTER_DOUBLE;
  counter.value.vdbl = 0.0;
  g_strlcpy (counter.category, CATEGORY, sizeof counter.category);
  g_strlcpy (counter.name, name, sizeof counter.name);
  g_strlcpy (counter.description, description, sizeof counter.name);

  sysprof_collector_define_counters (&counter, 1);

  return counter.id;
#else
  return 0;
#endif
}

guint
(gdk_profiler_define_int_counter) (const char *name,
                                   const char *description)
{
#ifdef HAVE_SYSPROF
  SysprofCaptureCounter counter;

  counter.id = sysprof_collector_request_counters (1);
  counter.type = SYSPROF_CAPTURE_COUNTER_INT64;
  counter.value.v64 = 0;
  g_strlcpy (counter.category, CATEGORY, sizeof counter.category);
  g_strlcpy (counter.name, name, sizeof counter.name);
  g_strlcpy (counter.description, description, sizeof counter.name);

  sysprof_collector_define_counters (&counter, 1);

  return counter.id;
#else
  return 0;
#endif
}

void
(gdk_profiler_set_counter) (guint  id,
                            double val)
{
#ifdef HAVE_SYSPROF
  SysprofCaptureCounterValue value;

  value.vdbl = val;
  sysprof_collector_set_counters (&id, &value, 1);
#endif
}

void
(gdk_profiler_set_int_counter) (guint  id,
                                gint64 val)
{
#ifdef HAVE_SYSPROF
  SysprofCaptureCounterValue value;

  value.v64 = val;
  sysprof_collector_set_counters (&id, &value, 1);
#endif
}
