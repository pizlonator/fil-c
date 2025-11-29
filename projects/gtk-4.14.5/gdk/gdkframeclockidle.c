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

#include "config.h"

#include "gdkframeclockidleprivate.h"

#include "gdkdebugprivate.h"
#include "gdkframeclockprivate.h"
#include "gdkprivate.h"
#include "gdkprofilerprivate.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define FRAME_INTERVAL 16667 /* microseconds */

typedef enum {
  SMOOTH_PHASE_STATE_VALID = 0,    /* explicit, since we count on zero-init */
  SMOOTH_PHASE_STATE_AWAIT_FIRST,
  SMOOTH_PHASE_STATE_AWAIT_DRAWN,
} SmoothDeltaState;

struct _GdkFrameClockIdlePrivate
{
  gint64 frame_time;                   /* The exact time we last ran the clock cycle, or 0 if never */
  gint64 smoothed_frame_time_base;     /* A grid-aligned version of frame_time (grid size == refresh period), never more than half a grid from frame_time */
  gint64 smoothed_frame_time_period;   /* The grid size that smoothed_frame_time_base is aligned to */
  gint64 smoothed_frame_time_reported; /* Ensures we are always monotonic */
  gint64 smoothed_frame_time_phase;    /* The offset of the first reported frame time, in the current animation sequence, from the preceding vsync */
  gint64 min_next_frame_time;          /* We're not synced to vblank, so wait at least until this before next cycle to avoid busy looping */
  SmoothDeltaState smooth_phase_state; /* The state of smoothed_frame_time_phase - is it valid, awaiting vsync etc. Thanks to zero-init, the initial value
                                          of smoothed_frame_time_phase is `0`. This is valid, since we didn't get a "frame drawn" event yet. Accordingly,
                                          the initial value of smooth_phase_state is SMOOTH_PHASE_STATE_VALID. See the comment in gdk_frame_clock_paint_idle()
                                          for details. */

  gint64 sleep_serial;
  gint64 freeze_time; /* in microseconds */

  guint flush_idle_id;
  guint paint_idle_id;
  guint freeze_count;
  guint updating_count;

  GdkFrameClockPhase requested;
  GdkFrameClockPhase phase;

  guint in_paint_idle : 1;
  guint paint_is_thaw : 1;
#ifdef G_OS_WIN32
  guint begin_period : 1;
#endif
};

static gboolean gdk_frame_clock_flush_idle (void *data);
static gboolean gdk_frame_clock_paint_idle (void *data);

G_DEFINE_TYPE_WITH_PRIVATE (GdkFrameClockIdle, gdk_frame_clock_idle, GDK_TYPE_FRAME_CLOCK)

static gint64 sleep_serial;
static gint64 sleep_source_prepare_time;
static GSource *sleep_source;

static gboolean
sleep_source_prepare (GSource *source,
                      int     *timeout)
{
  sleep_source_prepare_time = g_source_get_time (source);
  *timeout = -1;
  return FALSE;
}

static gboolean
sleep_source_check (GSource *source)
{
  if (g_source_get_time (source) != sleep_source_prepare_time)
    sleep_serial++;

  return FALSE;
}

static gboolean
sleep_source_dispatch (GSource     *source,
                       GSourceFunc  callback,
                       gpointer     user_data)
{
  return TRUE;
}

static GSourceFuncs sleep_source_funcs = {
  sleep_source_prepare,
  sleep_source_check,
  sleep_source_dispatch,
  NULL /* finalize */
};

static gint64
get_sleep_serial (void)
{
  if (sleep_source == NULL)
    {
      sleep_source = g_source_new (&sleep_source_funcs, sizeof (GSource));

      g_source_set_static_name (sleep_source, "[gtk] sleep serial");
      g_source_set_priority (sleep_source, G_PRIORITY_HIGH);
      g_source_attach (sleep_source, NULL);
      g_source_unref (sleep_source);
    }

  return sleep_serial;
}

static void
gdk_frame_clock_idle_init (GdkFrameClockIdle *frame_clock_idle)
{
  GdkFrameClockIdlePrivate *priv;

  frame_clock_idle->priv = priv =
    gdk_frame_clock_idle_get_instance_private (frame_clock_idle);

  priv->freeze_count = 0;
  priv->smoothed_frame_time_period = FRAME_INTERVAL;
}

static void
gdk_frame_clock_idle_dispose (GObject *object)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (object)->priv;

  if (priv->flush_idle_id != 0)
    {
      g_source_remove (priv->flush_idle_id);
      priv->flush_idle_id = 0;
    }

  if (priv->paint_idle_id != 0)
    {
      g_source_remove (priv->paint_idle_id);
      priv->paint_idle_id = 0;
    }

#ifdef G_OS_WIN32
  if (priv->begin_period) 
    {
      timeEndPeriod(1);
      priv->begin_period = FALSE;
    }
#endif

  G_OBJECT_CLASS (gdk_frame_clock_idle_parent_class)->dispose (object);
}

/* Note: This is never called on first frame, so
 * smoothed_frame_time_base != 0 and we have a valid frame_interval. */
static gint64
compute_smooth_frame_time (GdkFrameClock *clock,
                           gint64 new_frame_time,
                           gboolean new_frame_time_is_vsync_related,
                           gint64 smoothed_frame_time_base,
                           gint64 frame_interval)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;
  int frames_passed;
  gint64 new_smoothed_time;
  gint64 current_error;
  gint64 correction_magnitude;

  /* Consecutive frame, assume it is an integer number of frames later, so round to nearest such */
  /* NOTE:  This is >= 0, because smoothed_frame_time_base is < frame_interval/2 from old_frame_time
   *        and new_frame_time >= old_frame_time. */
  frames_passed = (new_frame_time - smoothed_frame_time_base + frame_interval / 2) / frame_interval;

  /* We use an approximately whole number of frames in the future from
   * last smoothed frame time. This way we avoid minor jitter in the
   * frame times making the animation speed uneven, but still animate
   * evenly in case of whole frame skips. */
  new_smoothed_time = smoothed_frame_time_base + frames_passed * frame_interval;

  /* However, sometimes the smoothed time is too much off from the
   * real time. For example, if the first frame clock cycle happened
   * not due to a frame rendering but an input event, then
   * new_frame_time could happen to be near the middle between two
   * frames. If that happens and we then start regularly animating at
   * the refresh_rate, then the jitter in the real time may cause us
   * to randomly sometimes round up, and sometimes down.
   *
   * To combat this we converge the smooth time towards the real time
   * in a way that is slow when they are near and fast when they are
   * far from each other.
   *
   * This is done by using the square of the error as the correction
   * magnitude. I.e. if the error is 0.5 frame, we correct by
   * 0.5*0.5=0.25 frame, if the error is 0.25 we correct by 0.125, if
   * the error is 0.1, frame we correct by 0.01 frame, etc.
   *
   * The actual computation is:
   *   (current_error/frame_interval)*(current_error/frame_interval)*frame_interval
   * But this can be simplified as below.
   *
   * Note: We only do this correction if the new frame is caused by a
   * thaw of the frame clock, so that we know the time is actually
   * related to the physical vblank. For frameclock cycles triggered
   * by other events we always step up in whole frames from the last
   * reported time.
   */
  if (new_frame_time_is_vsync_related)
    {
      current_error = new_smoothed_time - new_frame_time;
      correction_magnitude = current_error * current_error / frame_interval; /* Note, this is always > 0 due to the square */
      if (current_error > 0)
        new_smoothed_time -= correction_magnitude;
      else
        new_smoothed_time += correction_magnitude;
    }

  /* Ensure we're always monotonic  */
  if (new_smoothed_time <= priv->smoothed_frame_time_reported)
    new_smoothed_time = priv->smoothed_frame_time_reported;

  return new_smoothed_time;
}

static gint64
gdk_frame_clock_idle_get_frame_time (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;
  gint64 now;
  gint64 new_smoothed_time;

  /* can't change frame time during a paint */
  if (priv->phase != GDK_FRAME_CLOCK_PHASE_NONE &&
      priv->phase != GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS &&
      (priv->phase != GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT || priv->in_paint_idle))
    return priv->smoothed_frame_time_base;

  /* Outside a paint, pick something smoothed close to now */
  now = g_get_monotonic_time ();

  /* First time frame, just return something */
  if (priv->smoothed_frame_time_base == 0)
    {
      priv->smoothed_frame_time_reported = now;
      return now;
    }

  /* Since time is monotonic this is <= what we will pick for the next cycle, but
     more likely than not it will be equal if we're doing a constant animation. */
  new_smoothed_time = compute_smooth_frame_time (clock, now, FALSE,
                                                 priv->smoothed_frame_time_base,
                                                 priv->smoothed_frame_time_period);

  priv->smoothed_frame_time_reported = new_smoothed_time;
  return new_smoothed_time;
}

static inline gboolean
gdk_frame_clock_idle_is_frozen (GdkFrameClockIdle *self)
{
  GdkFrameClockIdlePrivate *priv = self->priv;

  if (GDK_DEBUG_CHECK (NO_VSYNC))
    return FALSE;

  return priv->freeze_count > 0;
}

static inline gboolean
should_run_flush_idle (GdkFrameClockIdle *self)
{
  GdkFrameClockIdlePrivate *priv = self->priv;

  return !gdk_frame_clock_idle_is_frozen (self) &&
         (priv->requested & GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0;
}

/* The reason why we track updating_count separately here and don't
 * just add GDK_FRAME_CLOCK_PHASE_UPDATE into ->request on every frame
 * is so that we can avoid doing one more frame when an animation
 * is cancelled.
 */
static inline gboolean
should_run_paint_idle (GdkFrameClockIdle *self)
{
  GdkFrameClockIdlePrivate *priv = self->priv;

  return !gdk_frame_clock_idle_is_frozen (self) &&
         ((priv->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0 ||
          priv->updating_count > 0);
}

static void
maybe_start_idle (GdkFrameClockIdle *self,
                  gboolean           caused_by_thaw)
{
  GdkFrameClockIdlePrivate *priv = self->priv;

  if (should_run_flush_idle (self) || should_run_paint_idle (self))
    {
      guint min_interval = 0;

      if (priv->min_next_frame_time != 0 &&
          !GDK_DEBUG_CHECK (NO_VSYNC))
        {
          gint64 now = g_get_monotonic_time ();
          gint64 min_interval_us = MAX (priv->min_next_frame_time, now) - now;
          min_interval = (min_interval_us + 500) / 1000;
        }

      if (priv->flush_idle_id == 0 && should_run_flush_idle (self))
        {
          GSource *source;

          priv->flush_idle_id = g_timeout_add_full (GDK_PRIORITY_EVENTS + 1,
                                                    min_interval,
                                                    gdk_frame_clock_flush_idle,
                                                    g_object_ref (self),
                                                    (GDestroyNotify) g_object_unref);
          source = g_main_context_find_source_by_id (NULL, priv->flush_idle_id);
          g_source_set_static_name (source, "[gtk] gdk_frame_clock_flush_idle");
        }

      if (!priv->in_paint_idle &&
	  priv->paint_idle_id == 0 && should_run_paint_idle (self))
        {
          priv->paint_is_thaw = caused_by_thaw;
          priv->paint_idle_id = g_timeout_add_full (GDK_PRIORITY_REDRAW,
                                                    min_interval,
                                                    gdk_frame_clock_paint_idle,
                                                    g_object_ref (self),
                                                    (GDestroyNotify) g_object_unref);
          gdk_source_set_static_name_by_id (priv->paint_idle_id, "[gtk] gdk_frame_clock_paint_idle");
        }
    }
}

static void
maybe_stop_idle (GdkFrameClockIdle *self)
{
  GdkFrameClockIdlePrivate *priv = self->priv;

  if (priv->flush_idle_id != 0 && !should_run_flush_idle (self))
    {
      g_source_remove (priv->flush_idle_id);
      priv->flush_idle_id = 0;
    }

  if (priv->paint_idle_id != 0 && !should_run_paint_idle (self))
    {
      g_source_remove (priv->paint_idle_id);
      priv->paint_idle_id = 0;
    }
}

static gboolean
gdk_frame_clock_flush_idle (void *data)
{
  GdkFrameClock *clock = GDK_FRAME_CLOCK (data);
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->flush_idle_id = 0;

  if (priv->phase != GDK_FRAME_CLOCK_PHASE_NONE)
    return FALSE;

  priv->phase = GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS;
  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS;

  _gdk_frame_clock_emit_flush_events (clock);

  if ((priv->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0 ||
      priv->updating_count > 0)
    priv->phase = GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;
  else
    priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

  g_clear_handle_id (&priv->paint_idle_id, g_source_remove);
  gdk_frame_clock_paint_idle (data);

  return FALSE;
}

/*
 * Returns the positive remainder.
 *
 * As an example, lets consider (-5) % 16:
 *
 *   (-5) % 16 = (0 * 16) + (-5) = -5
 *
 * If we only want positive remainders, we can instead calculate
 *
 *   (-5) % 16 = (1 * 16) + (-5) = 11
 *
 * The built-in `%` operator returns the former, positive_modulo() returns the latter.
 */
static int
positive_modulo (int i, int n)
{
  return (i % n + n) % n;
}

static gboolean
gdk_frame_clock_paint_idle (void *data)
{
  GdkFrameClock *clock = GDK_FRAME_CLOCK (data);
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;
  gboolean skip_to_resume_events;
  GdkFrameTimings *timings = NULL;
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  priv->paint_idle_id = 0;
  priv->in_paint_idle = TRUE;
  priv->min_next_frame_time = 0;

  skip_to_resume_events =
    (priv->requested & ~(GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS | GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS)) == 0 &&
    priv->updating_count == 0;

  if (priv->phase > GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT)
    {
      timings = gdk_frame_clock_get_current_timings (clock);
    }

  if (!skip_to_resume_events)
    {
      switch (priv->phase)
        {
        case GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS:
          break;
        case GDK_FRAME_CLOCK_PHASE_NONE:
        case GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT:
          if (!gdk_frame_clock_idle_is_frozen (clock_idle))
            {
              gint64 frame_interval = FRAME_INTERVAL;
              GdkFrameTimings *prev_timings = gdk_frame_clock_get_current_timings (clock);

              if (prev_timings && prev_timings->refresh_interval)
                frame_interval = prev_timings->refresh_interval;

              priv->frame_time = g_get_monotonic_time ();

              /*
               * The first clock cycle of an animation might have been triggered by some external event. An external
               * event can be an input event, an expired timer, data arriving over the network etc. This can happen at
               * any time, so the cycle could have been scheduled at some random time rather then immediately after a
               * frame completion. The offset between the start of the first animation cycle and the preceding vsync is
               * called the "phase" of the clock cycle start time (not to be confused with the phase of the frame
               * clock).
               *
               * In this first clock cycle, the "smooth" frame time is simply the time when the cycle was started. This
               * could be followed by several cycles which are not vsync-related. As long as we don't get a "frame
               * drawn" signal from the compositor, the clock cycles will occur every about frame_interval. Once we do
               * get a "frame drawn" signal, from this point on the frame clock cycles will start shortly after the
               * corresponding vsync signals, again every about frame_interval. The first vsync-related clock cycle
               * might occur less than a refresh interval away from the last non-vsync-related cycle. See the diagram
               * below for details. So while the cadence stays the same - a frame clock cycle every about frame_interval
               * - the phase of the cycles start time has changed.
               *
               * Since we might have already reported the frame time to the application in the previous clock cycles, we
               * have to adjust future reported frame times. We want the first vsync-related smooth time to be separated
               * by exactly 1 frame_interval from the previous one, in order to maintain the regularity of the reported
               * frame times. To achieve that, from this point on we add the phase of the first clock cycle start time to
               * the smooth time. In order to compute that phase, accounting for possible skipped frames (e.g. due to
               * compositor stalls), we want the following to be true:
               *
               *   first_vsync_smooth_time = last_non_vsync_smooth_time + frame_interval * (1 + frames_skipped)
               *
               * We can assign the following known/desired values to the above equation:
               *
               *   last_non_vsync_smooth_time = smoothed_frame_time_base
               *   first_vsync_smooth_time = frame_time + smoothed_frame_time_phase
               *
               * That leads us to the following, from which we can extract smoothed_frame_time_phase:
               *
               *   frame_time + smoothed_frame_time_phase = smoothed_frame_time_base +
               *                                            frame_interval * (1 + frames_skipped)
               *
               * In the following diagram, '|' mark a vsync, '*' mark the start of a clock cycle, '+' is the adjusted
               * frame time, '!' marks the reception of "frame drawn" events from the compositor. Note that the clock
               * cycle cadence changed after the first vsync-related cycle. This cadence is kept even if we don't
               * receive a 'frame drawn' signal in a subsequent frame, since then we schedule the clock at intervals of
               * refresh_interval.
               *
               * vsync             |           |           |           |           |           |...
               * frame drawn       |           |           |!          |!          |           |...
               * cycle start       |       *   |       *   |*          |*          |*          |...
               * adjusted times    |       *   |       *   |       +   |       +   |       +   |...
               * phase                                      ^------^
               */
              if (priv->smooth_phase_state == SMOOTH_PHASE_STATE_AWAIT_FIRST)
                {
                  /* First animation cycle - usually unrelated to vsync */
                  priv->smoothed_frame_time_base = 0;
                  priv->smoothed_frame_time_phase = 0;
                  priv->smooth_phase_state = SMOOTH_PHASE_STATE_AWAIT_DRAWN;
                }
              else if (priv->smooth_phase_state == SMOOTH_PHASE_STATE_AWAIT_DRAWN &&
                       priv->paint_is_thaw)
                {
                  /* First vsync-related animation cycle, we can now compute the phase. We want the phase to satisfy
                     0 <= phase < frame_interval */
                  priv->smoothed_frame_time_phase =
                      positive_modulo (priv->smoothed_frame_time_base - priv->frame_time,
                                       frame_interval);
                  priv->smooth_phase_state = SMOOTH_PHASE_STATE_VALID;
                }

              if (priv->smoothed_frame_time_base == 0)
                {
                  /* First frame ever, or first cycle in a new animation sequence. Ensure monotonicity */
                  priv->smoothed_frame_time_base = MAX (priv->frame_time, priv->smoothed_frame_time_reported);
                }
              else
                {
                  /* compute_smooth_frame_time() ensures monotonicity */
                  priv->smoothed_frame_time_base =
                      compute_smooth_frame_time (clock, priv->frame_time + priv->smoothed_frame_time_phase,
                                                 priv->paint_is_thaw,
                                                 priv->smoothed_frame_time_base,
                                                 priv->smoothed_frame_time_period);
                }

              priv->smoothed_frame_time_period = frame_interval;
              priv->smoothed_frame_time_reported = priv->smoothed_frame_time_base;

              _gdk_frame_clock_begin_frame (clock, priv->frame_time);
              /* Note "current" is different now so timings != prev_timings */
              timings = gdk_frame_clock_get_current_timings (clock);

              timings->frame_time = priv->frame_time;
              timings->smoothed_frame_time = priv->smoothed_frame_time_base;
              timings->slept_before = priv->sleep_serial != get_sleep_serial ();

              priv->phase = GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;

              /* We always emit ::before-paint and ::after-paint if
               * any of the intermediate phases are requested and
               * they don't get repeated if you freeze/thaw while
               * in them.
               */
              priv->requested &= ~GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;
              _gdk_frame_clock_emit_before_paint (clock);
              priv->phase = GDK_FRAME_CLOCK_PHASE_UPDATE;
            }
          G_GNUC_FALLTHROUGH;

        case GDK_FRAME_CLOCK_PHASE_UPDATE:
          if (!gdk_frame_clock_idle_is_frozen (clock_idle))
            {
              if ((priv->requested & GDK_FRAME_CLOCK_PHASE_UPDATE) != 0 ||
                  priv->updating_count > 0)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_UPDATE;
                  _gdk_frame_clock_emit_update (clock);
                }
            }
          G_GNUC_FALLTHROUGH;

        case GDK_FRAME_CLOCK_PHASE_LAYOUT:
          if (!gdk_frame_clock_idle_is_frozen (clock_idle))
            {
	      int iter;
              if (GDK_DEBUG_CHECK (FRAMES))
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_LAYOUT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT))
                    {
                      if (timings)
                        timings->layout_start_time = g_get_monotonic_time ();
                    }
                }

              priv->phase = GDK_FRAME_CLOCK_PHASE_LAYOUT;
	      /* We loop in the layout phase, because we don't want to progress
	       * into the paint phase with invalid size allocations. This may
	       * happen in some situation like races between user window
	       * resizes and natural size changes.
	       */
	      iter = 0;
              while ((priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT) &&
                     !gdk_frame_clock_idle_is_frozen (clock_idle) &&
		     iter++ < 4)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_LAYOUT;
                  _gdk_frame_clock_emit_layout (clock);
                }
	      if (iter == 5)
		g_warning ("gdk-frame-clock: layout continuously requested, giving up after 4 tries");
            }
          G_GNUC_FALLTHROUGH;

        case GDK_FRAME_CLOCK_PHASE_PAINT:
          if (!gdk_frame_clock_idle_is_frozen (clock_idle))
            {
              if (GDK_DEBUG_CHECK (FRAMES))
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_PAINT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT))
                    {
                      if (timings)
                        timings->paint_start_time = g_get_monotonic_time ();
                    }
                }

              priv->phase = GDK_FRAME_CLOCK_PHASE_PAINT;
              if (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_PAINT;
                  _gdk_frame_clock_emit_paint (clock);
                }
            }
          G_GNUC_FALLTHROUGH;

        case GDK_FRAME_CLOCK_PHASE_AFTER_PAINT:
          if (!gdk_frame_clock_idle_is_frozen (clock_idle))
            {
              priv->requested &= ~GDK_FRAME_CLOCK_PHASE_AFTER_PAINT;
              _gdk_frame_clock_emit_after_paint (clock);
              /* the ::after-paint phase doesn't get repeated on freeze/thaw,
               */
              priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;
            }
          if (GDK_DEBUG_CHECK (FRAMES))
            {
              if (timings)
                timings->frame_end_time = g_get_monotonic_time ();
            }
          G_GNUC_FALLTHROUGH;

        case GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS:
        default:
          ;
        }
    }

  if (priv->requested & GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS)
    {
      priv->requested &= ~GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS;
      _gdk_frame_clock_emit_resume_events (clock);
    }

  if (!gdk_frame_clock_idle_is_frozen (clock_idle))
    priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

  priv->in_paint_idle = FALSE;

  /* If there is throttling in the backend layer, then we'll do another
   * update as soon as the backend unthrottles (if there is work to do),
   * otherwise we need to figure when the next frame should be.
   */
  if (!gdk_frame_clock_idle_is_frozen (clock_idle))
    {
      /*
       * If we don't receive "frame drawn" events, smooth_cycle_start will simply be advanced in constant increments of
       * the refresh interval. That way we get absolute target times for the next cycles, which should prevent skewing
       * in the scheduling of the frame clock.
       *
       * Once we do receive "frame drawn" events, smooth_cycle_start will track the vsync, and do so in a more stable
       * way compared to frame_time. If we then no longer receive "frame drawn" events, smooth_cycle_start will again be
       * simply advanced in increments of the refresh interval, but this time we are in sync with the vsync. If we start
       * receiving "frame drawn" events shortly after losing them, then we should still be in sync.
       */
      gint64 smooth_cycle_start = priv->smoothed_frame_time_base - priv->smoothed_frame_time_phase;
      priv->min_next_frame_time = smooth_cycle_start + priv->smoothed_frame_time_period;

      maybe_start_idle (clock_idle, FALSE);
    }

  if (!gdk_frame_clock_idle_is_frozen (clock_idle))
    priv->sleep_serial = get_sleep_serial ();

  gdk_profiler_end_mark (before, "Frameclock cycle", NULL);

  return FALSE;
}

static void
gdk_frame_clock_idle_request_phase (GdkFrameClock      *clock,
                                    GdkFrameClockPhase  phase)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->requested |= phase;
  maybe_start_idle (clock_idle, FALSE);
}

static void
gdk_frame_clock_idle_begin_updating (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

#ifdef G_OS_WIN32
  /* We need a higher resolution timer while doing animations */
  if (priv->updating_count == 0 && !priv->begin_period)
    {
      timeBeginPeriod(1);
      priv->begin_period = TRUE;
    }
#endif

  if (priv->updating_count == 0)
    {
      priv->smooth_phase_state = SMOOTH_PHASE_STATE_AWAIT_FIRST;
    }

  priv->updating_count++;
  maybe_start_idle (clock_idle, FALSE);
}

static void
gdk_frame_clock_idle_end_updating (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  g_return_if_fail (priv->updating_count > 0);

  priv->updating_count--;
  maybe_stop_idle (clock_idle);

  if (priv->updating_count == 0)
    {
      priv->smooth_phase_state = SMOOTH_PHASE_STATE_VALID;
    }

#ifdef G_OS_WIN32
  if (priv->updating_count == 0 && priv->begin_period)
    {
      timeEndPeriod(1);
      priv->begin_period = FALSE;
    }
#endif
}

static void
gdk_frame_clock_idle_freeze (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  if (!gdk_frame_clock_idle_is_frozen (clock_idle))
    {
      if (GDK_PROFILER_IS_RUNNING)
        priv->freeze_time = g_get_monotonic_time ();
    }

  priv->freeze_count++;
  maybe_stop_idle (clock_idle);
}

static void
gdk_frame_clock_idle_thaw (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  g_return_if_fail (priv->freeze_count > 0);

  priv->freeze_count--;
  if (!gdk_frame_clock_idle_is_frozen (clock_idle))
    {
      maybe_start_idle (clock_idle, TRUE);
      /* If nothing is requested so we didn't start an idle, we need
       * to skip to the end of the state chain, since the idle won't
       * run and do it for us.
       */
      if (priv->paint_idle_id == 0)
        priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

      priv->sleep_serial = get_sleep_serial ();

      if (GDK_PROFILER_IS_RUNNING)
        {
          if (gdk_frame_clock_idle_is_frozen (clock_idle))
            {
              gdk_profiler_end_mark (priv->freeze_time * 1000, "frameclock frozen", NULL);
              priv->freeze_time = 0;
            }
        }
    }
}

static void
gdk_frame_clock_idle_class_init (GdkFrameClockIdleClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GdkFrameClockClass *frame_clock_class = (GdkFrameClockClass *)klass;

  gobject_class->dispose = gdk_frame_clock_idle_dispose;

  frame_clock_class->get_frame_time = gdk_frame_clock_idle_get_frame_time;
  frame_clock_class->request_phase = gdk_frame_clock_idle_request_phase;
  frame_clock_class->begin_updating = gdk_frame_clock_idle_begin_updating;
  frame_clock_class->end_updating = gdk_frame_clock_idle_end_updating;
  frame_clock_class->freeze = gdk_frame_clock_idle_freeze;
  frame_clock_class->thaw = gdk_frame_clock_idle_thaw;
}

GdkFrameClock *
_gdk_frame_clock_idle_new (void)
{
  GdkFrameClockIdle *clock;

  clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);

  return GDK_FRAME_CLOCK (clock);
}
