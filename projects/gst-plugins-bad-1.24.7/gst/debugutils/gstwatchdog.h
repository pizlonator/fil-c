/* GStreamer
 * Copyright (C) 2013 Rdio <ingestions@rdio.com>
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_WATCHDOG_H_
#define _GST_WATCHDOG_H_

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_WATCHDOG   (gst_watchdog_get_type())
#define GST_WATCHDOG(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WATCHDOG,GstWatchdog))
#define GST_WATCHDOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WATCHDOG,GstWatchdogClass))
#define GST_IS_WATCHDOG(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WATCHDOG))
#define GST_IS_WATCHDOG_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WATCHDOG))

typedef struct _GstWatchdog GstWatchdog;
typedef struct _GstWatchdogClass GstWatchdogClass;

struct _GstWatchdog
{
  GstBaseTransform base_watchdog;

  /* properties */
  int timeout;

  GMainContext *main_context;
  GMainLoop *main_loop;
  GThread *thread;
  GSource *source;

  gboolean waiting_for_a_buffer;
  gboolean waiting_for_flush_start;
  gboolean waiting_for_flush_stop;
};

struct _GstWatchdogClass
{
  GstBaseTransformClass base_watchdog_class;
};

GType gst_watchdog_get_type (void);

G_END_DECLS

#endif
