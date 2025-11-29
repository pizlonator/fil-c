/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005-2007 Imendio AB
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "gdkdisplayprivate.h"

#include "gdkmacoseventsource-private.h"
#include "gdkmacosdisplay-private.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdkprivate.h"

/*
 * This file implementations integration between the GLib main loop and
 * the native system of the Core Foundation run loop and Cocoa event
 * handling. There are basically three different cases that we need to
 * handle:
 *
 * - the GLib main loop is in control. The application has called
 *   gtk_main(), or is otherwise iterating the main loop.
 * - CFRunLoop is in control. We are in a modal operation such as window
 *   resizing.
 * - CFRunLoop is running a nested loop. This happens when a drag-and-drop
 *   operation has been initiated.
 *
 * When the GLib main loop is in control we integrate in native event
 * handling in two ways: first we add a GSource that handles checking
 * whether there are native events available, translating native events
 * to GDK events, and dispatching GDK events. Second we replace the
 * "poll function" of the GLib main loop with our own version that knows
 * how to wait for both the file descriptors and timeouts that GLib is
 * interested in and also for incoming native events.
 *
 * When CFRunLoop is in control, we integrate in GLib main loop handling
 * by adding a "run loop observer" that gives us notification at various
 * points in the run loop cycle. We map these points onto the corresponding
 * stages of the GLib main loop (prepare, check, dispatch), and make the
 * appropriate calls into GLib.
 *
 * When initiating a drag operation, a nested CFRunLoop is executed.
 * The nested run loop is started when fetching a native event in our GLib
 * main loop. The application does not receive any events until the nested loop
 * is finished. We work around this by forwarding the
 * events that trigger the callbacks of the NSDraggingSource protocol.
 * The "run loop observer" is executing the GLib main loop stages as long as we're
 * in the nested run loop, as if CFRunLoop were in control.
 * See also GdkMacosWindow.
 *
 * All cases share a single problem: the macOS API’s don’t allow us to
 * wait simultaneously for file descriptors and for events. So when we
 * need to do a blocking wait that includes file descriptor activity, we
 * push the actual work of calling select() to a helper thread (the
 * "select thread") and wait for native events in the main thread.
 *
 * The main known limitation of this code is that if a callback is triggered
 * via the macOS run loop while we are "polling" (in either case described
 * above), iteration of the GLib main loop is not possible from within
 * that callback. If the programmer tries to do so explicitly, then they
 * will get a warning from GLib "main loop already active in another thread".
 */

/******* State for run loop iteration *******/

/* Count of number of times we've gotten an "Entry" notification for
 * our run loop observer.
 */
static int current_loop_level = 0;

/* Run loop level at which we acquired ownership of the GLib main
 * loop. See note in run_loop_entry(). -1 means that we don’t have
 * ownership
 */
static int acquired_loop_level = -1;

/* Between run_loop_before_waiting() and run_loop_after_waiting();
 * whether we we need to call select_thread_collect_poll()
 */
static gboolean run_loop_polling_async = FALSE;

/* Between run_loop_before_waiting() and run_loop_after_waiting();
 * max_prioritiy to pass to g_main_loop_check()
 */
static int run_loop_max_priority;

/* Timer that we've added to wake up the run loop when a GLib timeout
 */
static CFRunLoopTimerRef run_loop_timer = NULL;

/* These are the file descriptors that are we are polling out of
 * the run loop. (We keep the array around and reuse it to avoid
 * constant allocations.)
 */
#define RUN_LOOP_POLLFDS_INITIAL_SIZE 16
static GPollFD *run_loop_pollfds;
static guint run_loop_pollfds_size; /* Allocated size of the array */
static guint run_loop_n_pollfds;    /* Number of file descriptors in the array */

/******* Other global variables *******/

/* Since we count on replacing the GLib main loop poll function as our
 * method of integrating Cocoa event handling into the GLib main loop
 * we need to make sure that the poll function is always called even
 * when there are no file descriptors that need to be polled. To do
 * this, we add a dummy GPollFD to our event source with a file
 * descriptor of “-1”. Then any time that GLib is polling the event
 * source, it will call our poll function.
 */
static GPollFD event_poll_fd;

/* Current NSEvents that we've gotten from Cocoa but haven't yet converted
 * to GdkEvents. We wait until our dispatch() function to do the conversion
 * since the conversion can conceivably cause signals to be emitted
 * or other things that shouldn’t happen inside a poll function.
 */
static GQueue *current_events;

/* The default poll function for GLib; we replace this with our own
 * Cocoa-aware version and then call the old version to do actual
 * file descriptor polling. There’s no actual need to chain to the
 * old one; we could reimplement the same functionality from scratch,
 * but since the default implementation does the right thing, why
 * bother.
 */
static GPollFunc old_poll_func;

/* Reference to the run loop of the main thread. (There is a unique
 * CFRunLoop per thread.)
 */
static CFRunLoopRef main_thread_run_loop;

/* Normally the Cocoa main loop maintains an NSAutoReleasePool and frees
 * it on every iteration. Since we are replacing the main loop we have
 * to provide this functionality ourself. We free and replace the
 * auto-release pool in our sources prepare() function.
 */
static NSAutoreleasePool *autorelease_pool;

/* Flag when we've called nextEventMatchingMask ourself; this triggers
 * a run loop iteration, so we need to detect that and avoid triggering
 * our "run the GLib main loop while the run loop is active machinery.
 */
static int getting_events = 0;

/************************************************************
 *********              Select Thread               *********
 ************************************************************/

/* The states in our state machine, see comments in select_thread_func()
 * for descriptions of each state
 */
typedef enum {
  BEFORE_START,
  WAITING,
  POLLING_QUEUED,
  POLLING_RESTART,
  POLLING_DESCRIPTORS,
} SelectThreadState;

static const char *const state_names[]  = {
  "BEFORE_START",
  "WAITING",
  "POLLING_QUEUED",
  "POLLING_RESTART",
  "POLLING_DESCRIPTORS"
};

static SelectThreadState select_thread_state = BEFORE_START;

static pthread_t select_thread;
static pthread_mutex_t select_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t select_thread_cond = PTHREAD_COND_INITIALIZER;

#define SELECT_THREAD_LOCK() pthread_mutex_lock (&select_thread_mutex)
#define SELECT_THREAD_UNLOCK() pthread_mutex_unlock (&select_thread_mutex)
#define SELECT_THREAD_SIGNAL() pthread_cond_signal (&select_thread_cond)
#define SELECT_THREAD_WAIT() pthread_cond_wait (&select_thread_cond, &select_thread_mutex)

/* These are the file descriptors that the select thread is currently
 * polling.
 */
static GPollFD *current_pollfds;
static guint current_n_pollfds;

/* These are the file descriptors that the select thread should pick
 * up and start polling when it has a chance.
 */
static GPollFD *next_pollfds;
static guint next_n_pollfds;

/* Pipe used to wake up the select thread */
static int select_thread_wakeup_pipe[2];

/* Run loop source used to wake up the main thread */
static CFRunLoopSourceRef select_main_thread_source;

static void
select_thread_set_state (SelectThreadState new_state)
{
  gboolean old_state;

  if (select_thread_state == new_state)
    return;

  GDK_DEBUG (EVENTLOOP, "EventLoop: Select thread state: %s => %s", state_names[select_thread_state], state_names[new_state]);

  old_state = select_thread_state;
  select_thread_state = new_state;
  if (old_state == WAITING && new_state != WAITING)
    SELECT_THREAD_SIGNAL ();
}

static void
signal_main_thread (void)
{
  GDK_DEBUG (EVENTLOOP, "EventLoop: Waking up main thread");

  /* If we are in nextEventMatchingMask, then we need to make sure an
   * event gets queued, otherwise it's enough to simply wake up the
   * main thread run loop
   */
  if (!run_loop_polling_async)
    CFRunLoopSourceSignal (select_main_thread_source);

  /* Don't check for CFRunLoopIsWaiting() here because it causes a
   * race condition (the loop could go into waiting state right after
   * we checked).
   */
  CFRunLoopWakeUp (main_thread_run_loop);
}

static void *
select_thread_func (void *arg)
{
  char c;

  SELECT_THREAD_LOCK ();

  while (TRUE)
    {
      switch (select_thread_state)
        {
        case BEFORE_START:
          /* The select thread has not been started yet
           */
          g_assert_not_reached ();

        case WAITING:
          /* Waiting for a set of file descriptors to be submitted by the main thread
           *
           *  => POLLING_QUEUED: main thread thread submits a set of file descriptors
           */
          SELECT_THREAD_WAIT ();
          break;

        case POLLING_QUEUED:
          /* Waiting for a set of file descriptors to be submitted by the main thread
           *
           *  => POLLING_DESCRIPTORS: select thread picks up the file descriptors to begin polling
           */
          g_free (current_pollfds);

          current_pollfds = next_pollfds;
          current_n_pollfds = next_n_pollfds;

          next_pollfds = NULL;
          next_n_pollfds = 0;

          select_thread_set_state (POLLING_DESCRIPTORS);
          break;

        case POLLING_RESTART:
          /* Select thread is currently polling a set of file descriptors, main thread has
           * began a new iteration with the same set of file descriptors. We don't want to
           * wake the select thread up and wait for it to restart immediately, but to avoid
           * a race (described below in select_thread_start_polling()) we need to recheck after
           * polling completes.
           *
           * => POLLING_DESCRIPTORS: select completes, main thread rechecks by polling again
           * => POLLING_QUEUED: main thread submits a new set of file descriptors to be polled
           */
          select_thread_set_state (POLLING_DESCRIPTORS);
          break;

        case POLLING_DESCRIPTORS:
          /* In the process of polling the file descriptors
           *
           *  => WAITING: polling completes when a file descriptor becomes active
           *  => POLLING_QUEUED: main thread submits a new set of file descriptors to be polled
           *  => POLLING_RESTART: main thread begins a new iteration with the same set file descriptors
           */
          SELECT_THREAD_UNLOCK ();
          old_poll_func (current_pollfds, current_n_pollfds, -1);
          SELECT_THREAD_LOCK ();

          read (select_thread_wakeup_pipe[0], &c, 1);

          if (select_thread_state == POLLING_DESCRIPTORS)
            {
              signal_main_thread ();
              select_thread_set_state (WAITING);
            }
          break;
        }
    }
}

static void
got_fd_activity (void *info)
{
  NSEvent *event;

  /* Post a message so we'll break out of the message loop */
  event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                             location: NSZeroPoint
                        modifierFlags: 0
                            timestamp: 0
                         windowNumber: 0
                              context: nil
                              subtype: GDK_MACOS_EVENT_SUBTYPE_EVENTLOOP
                                data1: 0
                                data2: 0];

  [NSApp postEvent:event atStart:YES];
}

static void
select_thread_start (void)
{
  g_return_if_fail (select_thread_state == BEFORE_START);

  pipe (select_thread_wakeup_pipe);
  fcntl (select_thread_wakeup_pipe[0], F_SETFL, O_NONBLOCK);

  CFRunLoopSourceContext source_context = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, got_fd_activity };
  select_main_thread_source = CFRunLoopSourceCreate (NULL, 0, &source_context);

  CFRunLoopAddSource (main_thread_run_loop, select_main_thread_source, kCFRunLoopCommonModes);

  select_thread_state = WAITING;

  while (TRUE)
    {
      if (pthread_create (&select_thread, NULL, select_thread_func, NULL) == 0)
        break;

      g_warning ("Failed to create select thread, sleeping and trying again");
      sleep (1);
    }
}

static void
dump_poll_result (GPollFD *ufds,
                  guint    nfds)
{
  GString *s;
  int i;

  s = g_string_new ("");
  for (i = 0; i < nfds; i++)
    {
      if (ufds[i].fd >= 0 && ufds[i].revents)
        {
          g_string_append_printf (s, " %d:", ufds[i].fd);
          if (ufds[i].revents & G_IO_IN)
            g_string_append (s, " in");
          if (ufds[i].revents & G_IO_OUT)
            g_string_append (s, " out");
          if (ufds[i].revents & G_IO_PRI)
            g_string_append (s, " pri");
          g_string_append (s, "\n");
        }
    }
  gdk_debug_message ("%s", s->str);
  g_string_free (s, TRUE);
}

static gboolean
pollfds_equal (GPollFD *old_pollfds,
               guint    old_n_pollfds,
               GPollFD *new_pollfds,
               guint    new_n_pollfds)
{
  int i;

  if (old_n_pollfds != new_n_pollfds)
    return FALSE;

  for (i = 0; i < old_n_pollfds; i++)
    {
      if (old_pollfds[i].fd != new_pollfds[i].fd ||
          old_pollfds[i].events != new_pollfds[i].events)
        return FALSE;
    }

  return TRUE;
}

/* Begins a polling operation with the specified GPollFD array; the
 * timeout is used only to tell if the polling operation is blocking
 * or non-blocking.
 *
 * Returns:
 *  -1: No file descriptors ready, began asynchronous poll
 *   0: No file descriptors ready, asynchronous poll not needed
 * > 0: Number of file descriptors ready
 */
static int
select_thread_start_poll (GPollFD *ufds,
                          guint    nfds,
                          int      timeout)
{
  int n_ready;
  gboolean have_new_pollfds = FALSE;
  int poll_fd_index = -1;
  int i;

  for (i = 0; i < nfds; i++)
    if (ufds[i].fd == -1)
      {
        poll_fd_index = i;
        break;
      }

  if (nfds == 0 ||
      (nfds == 1 && poll_fd_index >= 0))
    {
      GDK_DEBUG (EVENTLOOP, "EventLoop: Nothing to poll");
      return 0;
    }

  /* If we went immediately to an async poll, then we might decide to
   * dispatch idle functions when higher priority file descriptor sources
   * are ready to be dispatched. So we always need to first check
   * check synchronously with a timeout of zero, and only when no
   * sources are immediately ready, go to the asynchronous poll.
   *
   * Of course, if the timeout passed in is 0, then the synchronous
   * check is sufficient and we never need to do the asynchronous poll.
   */
  n_ready = old_poll_func (ufds, nfds, 0);
  if (n_ready > 0 || timeout == 0)
    {
      if (GDK_DEBUG_CHECK (EVENTLOOP) && n_ready > 0)
        {
          gdk_debug_message ("EventLoop: Found ready file descriptors before waiting");
          dump_poll_result (ufds, nfds);
        }

      return n_ready;
    }

  SELECT_THREAD_LOCK ();

  if (select_thread_state == BEFORE_START)
    {
      select_thread_start ();
    }

  if (select_thread_state == POLLING_QUEUED)
    {
      /* If the select thread hasn't picked up the set of file descriptors yet
       * then we can simply replace an old stale set with a new set.
       */
      if (!pollfds_equal (ufds, nfds, next_pollfds, next_n_pollfds - 1))
        {
          g_free (next_pollfds);
          next_pollfds = NULL;
          next_n_pollfds = 0;

          have_new_pollfds = TRUE;
        }
    }
  else if (select_thread_state == POLLING_RESTART || select_thread_state == POLLING_DESCRIPTORS)
    {
      /* If we are already in the process of polling the right set of file descriptors,
       * there's no need for us to immediately force the select thread to stop polling
       * and then restart again. And avoiding doing so increases the efficiency considerably
       * in the common case where we have a set of basically inactive file descriptors that
       * stay unchanged present as we process many events.
       *
       * However, we have to be careful that we don't hit the following race condition
       *  Select Thread              Main Thread
       *  -----------------          ---------------
       *  Polling Completes
       *                             Reads data or otherwise changes file descriptor state
       *                             Checks if polling is current
       *                             Does nothing (*)
       *                             Releases lock
       *  Acquires lock
       *  Marks polling as complete
       *  Wakes main thread
       *                             Receives old stale file descriptor state
       *
       * To avoid this, when the new set of poll descriptors is the same as the current
       * one, we transition to the POLLING_RESTART stage at the point marked (*). When
       * the select thread wakes up from the poll because a file descriptor is active, if
       * the state is POLLING_RESTART it immediately begins polling same the file descriptor
       * set again. This normally will just return the same set of active file descriptors
       * as the first time, but in sequence described above will properly update the
       * file descriptor state.
       *
       * Special case: this RESTART logic is not needed if the only FD is the internal GLib
       * "wakeup pipe" that is presented when threads are initialized.
       *
       * P.S.: The harm in the above sequence is mostly that sources can be signalled
       *   as ready when they are no longer ready. This may prompt a blocking read
       *   from a file descriptor that hangs.
       */
      if (!pollfds_equal (ufds, nfds, current_pollfds, current_n_pollfds - 1))
        have_new_pollfds = TRUE;
      else
        {
          if (!((nfds == 1 && poll_fd_index < 0) ||
                (nfds == 2 && poll_fd_index >= 0)))
            select_thread_set_state (POLLING_RESTART);
        }
    }
  else
    have_new_pollfds = TRUE;

  if (have_new_pollfds)
    {
      GDK_DEBUG (EVENTLOOP, "EventLoop: Submitting a new set of file descriptor to the select thread");

      g_assert (next_pollfds == NULL);

      next_n_pollfds = nfds + 1;
      next_pollfds = g_new (GPollFD, nfds + 1);
      memcpy (next_pollfds, ufds, nfds * sizeof (GPollFD));

      next_pollfds[nfds].fd = select_thread_wakeup_pipe[0];
      next_pollfds[nfds].events = G_IO_IN;

      if (select_thread_state != POLLING_QUEUED && select_thread_state != WAITING)
        {
          if (select_thread_wakeup_pipe[1])
            {
              char c = 'A';
              write (select_thread_wakeup_pipe[1], &c, 1);
            }
        }

      select_thread_set_state (POLLING_QUEUED);
    }

  SELECT_THREAD_UNLOCK ();

  return -1;
}

/* End an asynchronous polling operation started with
 * select_thread_collect_poll(). This must be called if and only if
 * select_thread_start_poll() return -1. The GPollFD array passed
 * in must be identical to the one passed to select_thread_start_poll().
 *
 * The results of the poll are written into the GPollFD array passed in.
 *
 * Returns: number of file descriptors ready
 */
static int
select_thread_collect_poll (GPollFD *ufds, guint nfds)
{
  int i;
  int n_ready = 0;

  SELECT_THREAD_LOCK ();

  if (select_thread_state == WAITING) /* The poll completed */
    {
      for (i = 0; i < nfds; i++)
        {
          if (ufds[i].fd == -1)
            continue;

          g_assert (ufds[i].fd == current_pollfds[i].fd);
          g_assert (ufds[i].events == current_pollfds[i].events);

          if (current_pollfds[i].revents)
            {
              ufds[i].revents = current_pollfds[i].revents;
              n_ready++;
            }
        }

      if (GDK_DEBUG_CHECK (EVENTLOOP))
        {
          gdk_debug_message ("EventLoop: Found ready file descriptors after waiting");
          dump_poll_result (ufds, nfds);
        }
    }

  SELECT_THREAD_UNLOCK ();

  return n_ready;
}

/************************************************************
 *********             Main Loop Source             *********
 ************************************************************/

typedef struct _GdkMacosEventSource
{
  GSource     source;
  GdkDisplay *display;
} GdkMacosEventSource;

gboolean
_gdk_macos_event_source_check_pending (void)
{
  return current_events && current_events->head;
}

NSEvent *
_gdk_macos_event_source_get_pending (void)
{
  NSEvent *event = NULL;

  if (current_events)
    event = g_queue_pop_tail (current_events);

  return event;
}

static void
_gdk_macos_event_source_queue_event (NSEvent *event)
{
  /* Just used to wake us up; if an event and a FD arrived at the same
    * time; could have come from a previous iteration in some cases,
    * but the spurious wake up is harmless if a little inefficient.
    */
  if (!event ||
      ([event type] == NSEventTypeApplicationDefined &&
       [event subtype] == GDK_MACOS_EVENT_SUBTYPE_EVENTLOOP))
    return;

  if (!current_events)
    current_events = g_queue_new ();
  g_queue_push_head (current_events, [event retain]);
}

static gboolean
gdk_macos_event_source_prepare (GSource *source,
                                int     *timeout)
{
  GdkMacosEventSource *event_source = (GdkMacosEventSource *)source;
  gboolean retval;

  /* The prepare stage is the stage before the main loop starts polling
   * and dispatching events. The autorelease poll is drained here for
   * the preceding main loop iteration or, in case of the first iteration,
   * for the operations carried out between event loop initialization and
   * this first iteration.
   *
   * The autorelease poll must only be drained when the following conditions
   * apply:
   *  - We are at the base CFRunLoop level (indicated by current_loop_level),
   *  - We are at the base g_main_loop level (indicated by
   *    g_main_depth())
   *  - We are at the base poll_func level (indicated by getting events).
   *
   * Messing with the autorelease pool at any level of nesting can cause access
   * to deallocated memory because autorelease_pool is static and releasing a
   * pool will cause all pools allocated inside of it to be released as well.
   */
  if (current_loop_level == 0 && g_main_depth() == 0 && getting_events == 0)
    {
      if (autorelease_pool)
        [autorelease_pool drain];

      autorelease_pool = [[NSAutoreleasePool alloc] init];
    }

  *timeout = -1;

  if (event_source->display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (event_source->display) != NULL;
  else
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              _gdk_macos_event_source_check_pending ());

  return retval;
}

static gboolean
gdk_macos_event_source_check (GSource *source)
{
  GdkMacosEventSource *event_source = (GdkMacosEventSource *)source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (event_source->display) != NULL;
  else
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              _gdk_macos_event_source_check_pending ());

  return retval;
}

static gboolean
gdk_macos_event_source_dispatch (GSource     *source,
                                 GSourceFunc  callback,
                                 gpointer     user_data)
{
  GdkMacosEventSource *event_source = (GdkMacosEventSource *)source;
  GdkEvent *event;

  _gdk_macos_display_queue_events (GDK_MACOS_DISPLAY (event_source->display));

  event = _gdk_event_unqueue (event_source->display);

  if (event)
    {
      gboolean handled = _gdk_event_emit (event);

      if (!handled)
        {
          NSEvent *nsevent = _gdk_macos_display_get_exact_nsevent (event);
          if (nsevent != NULL)
            [NSApp sendEvent: nsevent];
        }

      gdk_event_unref (event);
    }

  return TRUE;
}

static void
gdk_macos_event_source_finalize (GSource *source)
{
  GdkMacosEventSource *event_source = (GdkMacosEventSource *)source;

  g_clear_object (&event_source->display);
}

static GSourceFuncs event_funcs = {
  gdk_macos_event_source_prepare,
  gdk_macos_event_source_check,
  gdk_macos_event_source_dispatch,
  gdk_macos_event_source_finalize,
};

/************************************************************
 *********             Our Poll Function            *********
 ************************************************************/

static int
poll_func (GPollFD *ufds,
           guint    nfds,
           int      timeout_)
{
  NSEvent *event;
  NSDate *limit_date;
  int n_ready;

  static GPollFD *last_ufds;

  last_ufds = ufds;

  n_ready = select_thread_start_poll (ufds, nfds, timeout_);
  if (n_ready > 0)
    timeout_ = 0;

  if (timeout_ == -1)
    limit_date = [NSDate distantFuture];
  else if (timeout_ == 0)
    limit_date = [NSDate distantPast];
  else
    limit_date = [NSDate dateWithTimeIntervalSinceNow:timeout_/1000.0];

  getting_events++;
  event = [NSApp nextEventMatchingMask: NSEventMaskAny
                             untilDate: limit_date
                                inMode: NSDefaultRunLoopMode
                               dequeue: YES];
  getting_events--;

  /* We check if last_ufds did not change since the time this function was
   * called. It is possible that a recursive main loop (and thus recursive
   * invocation of this poll function) is triggered while in
   * nextEventMatchingMask:. If during that time new fds are added,
   * the cached fds array might be replaced in g_main_context_iterate().
   * So, we should avoid accessing the old fd array (still pointed at by
   * ufds) here in that case, since it might have been freed. We avoid this
   * by not calling the collect stage.
   */
  if (last_ufds == ufds && n_ready < 0)
    n_ready = select_thread_collect_poll (ufds, nfds);

  _gdk_macos_event_source_queue_event (event);

  return n_ready;
}

/************************************************************
 *********  Running the main loop out of CFRunLoop  *********
 ************************************************************/

/* Wrapper around g_main_context_query() that handles reallocating
 * run_loop_pollfds up to the proper size
 */
static int
query_main_context (GMainContext *context,
                    int           max_priority,
                    int          *timeout)
{
  int nfds;

  if (!run_loop_pollfds)
    {
      run_loop_pollfds_size = RUN_LOOP_POLLFDS_INITIAL_SIZE;
      run_loop_pollfds = g_new (GPollFD, run_loop_pollfds_size);
    }

  while ((nfds = g_main_context_query (context, max_priority, timeout,
                                       run_loop_pollfds,
                                       run_loop_pollfds_size)) > run_loop_pollfds_size)
    {
      g_free (run_loop_pollfds);
      run_loop_pollfds_size = nfds;
      run_loop_pollfds = g_new (GPollFD, nfds);
    }

  return nfds;
}

static void
run_loop_entry (void)
{
  if (acquired_loop_level == -1)
    {
      if (g_main_context_acquire (NULL))
        {
          GDK_DEBUG (EVENTLOOP, "EventLoop: Beginning tracking run loop activity");
          acquired_loop_level = current_loop_level;
        }
      else
        {
          /* If we fail to acquire the main context, that means someone is iterating
           * the main context in a different thread; we simply wait until this loop
           * exits and then try again at next entry. In general, iterating the loop
           * from a different thread is rare: it is only possible when GDK threading
           * is initialized and is not frequently used even then. So, we hope that
           * having GLib main loop iteration blocked in the combination of that and
           * a native modal operation is a minimal problem. We could imagine using a
           * thread that does g_main_context_wait() and then wakes us back up, but
           * the gain doesn't seem worth the complexity.
           */
          GDK_DEBUG (EVENTLOOP, "EventLoop: Can't acquire main loop; skipping tracking run loop activity");
        }
    }
}

static void
run_loop_before_timers (void)
{
}

static void
run_loop_before_sources (void)
{
  GMainContext *context = g_main_context_default ();
  int max_priority;
  int nfds;

  /* Before we let the CFRunLoop process sources, we want to check if there
   * are any pending GLib main loop sources more urgent than
   * G_PRIORITY_DEFAULT that need to be dispatched. (We consider all activity
   * from the CFRunLoop to have a priority of G_PRIORITY_DEFAULT.) If no
   * sources are processed by the CFRunLoop, then processing will continue
   * on to the BeforeWaiting stage where we check for lower priority sources.
   */

  g_main_context_prepare (context, &max_priority);
  max_priority = MIN (max_priority, G_PRIORITY_DEFAULT);

  /* We ignore the timeout that query_main_context () returns since we'll
   * always query again before waiting.
   */
  nfds = query_main_context (context, max_priority, NULL);

  if (nfds)
    old_poll_func (run_loop_pollfds, nfds, 0);

  if (g_main_context_check (context, max_priority, run_loop_pollfds, nfds))
    {
      GDK_DEBUG (EVENTLOOP, "EventLoop: Dispatching high priority sources");
      g_main_context_dispatch (context);
    }
}

static void
dummy_timer_callback (CFRunLoopTimerRef  timer,
                      void              *info)
{
  /* Nothing; won't normally even be called */
}

static void
run_loop_before_waiting (void)
{
  GMainContext *context = g_main_context_default ();
  int timeout;
  int n_ready;

  /* At this point, the CFRunLoop is ready to wait. We start a GMain loop
   * iteration by calling the check() and query() stages. We start a
   * poll, and if it doesn't complete immediately we let the run loop
   * go ahead and sleep. Before doing that, if there was a timeout from
   * GLib, we set up a CFRunLoopTimer to wake us up.
   */

  g_main_context_prepare (context, &run_loop_max_priority);

  run_loop_n_pollfds = query_main_context (context, run_loop_max_priority, &timeout);

  n_ready = select_thread_start_poll (run_loop_pollfds, run_loop_n_pollfds, timeout);

  if (n_ready > 0 || timeout == 0)
    {
      /* We have stuff to do, no sleeping allowed! */
      CFRunLoopWakeUp (main_thread_run_loop);
    }
  else if (timeout > 0)
    {
      /* We need to get the run loop to break out of its wait when our timeout
       * expires. We do this by adding a dummy timer that we'll remove immediately
       * after the wait wakes up.
       */
      GDK_DEBUG (EVENTLOOP, "EventLoop: Adding timer to wake us up in %d milliseconds", timeout);

      run_loop_timer = CFRunLoopTimerCreate (NULL, /* allocator */
                                             CFAbsoluteTimeGetCurrent () + timeout / 1000.,
                                             0, /* interval (0=does not repeat) */
                                             0, /* flags */
                                             0, /* order (priority) */
                                             dummy_timer_callback,
                                             NULL);

      CFRunLoopAddTimer (main_thread_run_loop, run_loop_timer, kCFRunLoopCommonModes);
    }

  run_loop_polling_async = n_ready < 0;
}

static void
run_loop_after_waiting (void)
{
  GMainContext *context = g_main_context_default ();

  /* After sleeping, we finish of the GMain loop iteratin started in before_waiting()
   * by doing the check() and dispatch() stages.
   */

  if (run_loop_timer)
    {
      CFRunLoopRemoveTimer (main_thread_run_loop, run_loop_timer, kCFRunLoopCommonModes);
      CFRelease (run_loop_timer);
      run_loop_timer = NULL;
    }

  if (run_loop_polling_async)
    {
      select_thread_collect_poll (run_loop_pollfds, run_loop_n_pollfds);
      run_loop_polling_async = FALSE;
    }

  if (g_main_context_check (context, run_loop_max_priority, run_loop_pollfds, run_loop_n_pollfds))
    {
      GDK_DEBUG (EVENTLOOP, "EventLoop: Dispatching after waiting");
      g_main_context_dispatch (context);
    }
}

static void
run_loop_exit (void)
{
  /* + 1 because we decrement current_loop_level separately in observer_callback() */
  if ((current_loop_level + 1) == acquired_loop_level)
    {
      g_main_context_release (NULL);
      acquired_loop_level = -1;
      GDK_DEBUG (EVENTLOOP, "EventLoop: Ended tracking run loop activity");
    }
}

static void
run_loop_observer_callback (CFRunLoopObserverRef observer,
                            CFRunLoopActivity    activity,
                            void                *info)
{
  switch (activity)
    {
    case kCFRunLoopEntry:
      current_loop_level++;
      break;
    case kCFRunLoopExit:
      g_return_if_fail (current_loop_level > 0);
      current_loop_level--;
      break;
    case kCFRunLoopBeforeTimers:
    case kCFRunLoopBeforeSources:
    case kCFRunLoopBeforeWaiting:
    case kCFRunLoopAfterWaiting:
    case kCFRunLoopAllActivities:
    default:
      break;
    }

  /* DnD starts a nested runloop, or so it seems.
     If we have such a loop, we still want to run
     our idle handlers. */
  if (getting_events > 0 && current_loop_level < 2)
    return;

  switch (activity)
    {
    case kCFRunLoopEntry:
      run_loop_entry ();
      break;
    case kCFRunLoopBeforeTimers:
      run_loop_before_timers ();
      break;
    case kCFRunLoopBeforeSources:
      run_loop_before_sources ();
      break;
    case kCFRunLoopBeforeWaiting:
      run_loop_before_waiting ();
      break;
    case kCFRunLoopAfterWaiting:
      run_loop_after_waiting ();
      break;
    case kCFRunLoopExit:
      run_loop_exit ();
      break;
    case kCFRunLoopAllActivities:
    default:
      break;
    }
}

/************************************************************/

GSource *
_gdk_macos_event_source_new (GdkMacosDisplay *display)
{
  CFRunLoopObserverRef observer;
  GdkMacosEventSource *event_source;
  GSource *source;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  /* Hook into the GLib main loop */

  event_poll_fd.events = G_IO_IN;
  event_poll_fd.fd = -1;

  source = g_source_new (&event_funcs, sizeof (GdkMacosEventSource));
  g_source_set_static_name (source, "GDK Quartz event source");
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);

  old_poll_func = g_main_context_get_poll_func (NULL);
  g_main_context_set_poll_func (NULL, poll_func);

  event_source = (GdkMacosEventSource *)source;
  event_source->display = g_object_ref (GDK_DISPLAY (display));

  /* Hook into the CFRunLoop for the main thread */

  main_thread_run_loop = CFRunLoopGetCurrent ();

  observer = CFRunLoopObserverCreate (NULL, /* default allocator */
                                      kCFRunLoopAllActivities,
                                      true, /* repeats: not one-shot */
                                      0, /* order (priority) */
                                      run_loop_observer_callback,
                                      NULL);

  CFRunLoopAddObserver (main_thread_run_loop, observer, kCFRunLoopCommonModes);

  /* Initialize our autorelease pool */
  autorelease_pool = [[NSAutoreleasePool alloc] init];

  return source;
}
