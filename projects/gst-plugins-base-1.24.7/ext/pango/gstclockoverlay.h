/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2005> Tim-Philipp Müller <tim@centricular.net>
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


#ifndef __GST_CLOCK_OVERLAY_H__
#define __GST_CLOCK_OVERLAY_H__

#include "gstbasetextoverlay.h"

G_BEGIN_DECLS

#define GST_TYPE_CLOCK_OVERLAY \
  (gst_clock_overlay_get_type())
#define GST_CLOCK_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLOCK_OVERLAY,GstClockOverlay))
#define GST_CLOCK_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLOCK_OVERLAY,GstClockOverlayClass))
#define GST_IS_CLOCK_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLOCK_OVERLAY))
#define GST_IS_CLOCK_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLOCK_OVERLAY))

typedef struct _GstClockOverlay GstClockOverlay;
typedef struct _GstClockOverlayClass GstClockOverlayClass;

/**
 * GstClockOverlay:
 *
 * Opaque clockoverlay data structure.
 */
struct _GstClockOverlay {
  GstBaseTextOverlay textoverlay;
  gchar         *format; /* as in strftime () */

  /* for wcsftime */
  gunichar2     *wformat;

  gchar         *text;
};

struct _GstClockOverlayClass {
  GstBaseTextOverlayClass parent_class;
};

GType gst_clock_overlay_get_type (void);


/* This is a hack hat allows us to use nonliterals for strftime without
 * triggering a warning from -Wformat-nonliteral. We need to allow this
 * because we export the format string as a property of the element.
 * For the inspiration of this and a discussion of why this is necessary,
 * see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39438
 */
#ifdef __GNUC__
#pragma GCC system_header
static size_t my_strftime(char *s, size_t max, const char *format,
                          const struct tm *tm)
{
  return strftime (s, max, format, tm);
}
#define strftime my_strftime
#endif


G_END_DECLS

#endif /* __GST_CLOCK_OVERLAY_H__ */

