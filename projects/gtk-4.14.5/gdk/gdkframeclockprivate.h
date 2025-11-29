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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Uninstalled header, internal to GDK */

#pragma once

#include <gdk/gdkframeclock.h>
#include <gdk/gdkprofilerprivate.h>

G_BEGIN_DECLS

/**
 * GdkFrameClock:
 * @parent_instance: The parent instance.
 */
struct _GdkFrameClock
{
  GObject parent_instance;

  /*< private >*/
  GdkFrameClockPrivate *priv;
};

/**
 * GdkFrameClockClass:
 * @parent_class: The parent class.

 * @get_frame_time: Gets the time that should currently be used for
 *    animations.
 * @request_phase: Asks the frame clock to run a particular phase.
 * @begin_updating: Starts updates for an animation.
 * @end_updating: Stops updates for an animation.
 * @freeze: 
 * @thaw: 
 */
struct _GdkFrameClockClass
{
  GObjectClass parent_class;

  /*< public >*/

  gint64   (* get_frame_time) (GdkFrameClock *clock);

  void     (* request_phase)  (GdkFrameClock      *clock,
                               GdkFrameClockPhase  phase);
  void     (* begin_updating) (GdkFrameClock      *clock);
  void     (* end_updating)   (GdkFrameClock      *clock);

  void     (* freeze)         (GdkFrameClock *clock);
  void     (* thaw)           (GdkFrameClock *clock);

  /* signals */
  /* void (* flush_events)       (GdkFrameClock *clock); */
  /* void (* before_paint)       (GdkFrameClock *clock); */
  /* void (* update)             (GdkFrameClock *clock); */
  /* void (* layout)             (GdkFrameClock *clock); */
  /* void (* paint)              (GdkFrameClock *clock); */
  /* void (* after_paint)        (GdkFrameClock *clock); */
  /* void (* resume_events)      (GdkFrameClock *clock); */
};

struct _GdkFrameTimings
{
  /*< private >*/
  guint ref_count;

  gint64 frame_counter;
  guint64 cookie;
  gint64 frame_time;
  gint64 smoothed_frame_time;
  gint64 drawn_time;
  gint64 presentation_time;
  gint64 refresh_interval;
  gint64 predicted_presentation_time;

  gint64 layout_start_time;
  gint64 paint_start_time;
  gint64 frame_end_time;

  guint complete : 1;
  guint slept_before : 1;
};

void _gdk_frame_clock_inhibit_freeze (GdkFrameClock *clock);
void _gdk_frame_clock_uninhibit_freeze (GdkFrameClock *clock);

void _gdk_frame_clock_begin_frame         (GdkFrameClock   *clock,
                                           gint64           monotonic_time);
void _gdk_frame_clock_debug_print_timings (GdkFrameClock   *clock,
                                           GdkFrameTimings *timings);
void _gdk_frame_clock_add_timings_to_profiler (GdkFrameClock *frame_clock,
                                               GdkFrameTimings *timings);

GdkFrameTimings *_gdk_frame_timings_new   (gint64           frame_counter);
gboolean         _gdk_frame_timings_steal (GdkFrameTimings *timings,
                                           gint64           frame_counter);

void _gdk_frame_clock_emit_flush_events  (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_before_paint  (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_update        (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_layout        (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_paint         (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_after_paint   (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_resume_events (GdkFrameClock *frame_clock);

G_END_DECLS

