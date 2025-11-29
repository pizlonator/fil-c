/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <poll.h>
#include <errno.h>

#include "loader_wayland_helper.h"

#ifndef HAVE_WL_DISPATCH_QUEUE_TIMEOUT
static int
wl_display_poll(struct wl_display *display,
                short int events,
                const struct timespec *timeout)
{
   int ret;
   struct pollfd pfd[1];
   struct timespec now;
   struct timespec deadline = {0};
   struct timespec result;
   struct timespec *remaining_timeout = NULL;

   if (timeout) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      timespec_add(&deadline, &now, timeout);
   }

   pfd[0].fd = wl_display_get_fd(display);
   pfd[0].events = events;
   do {
      if (timeout) {
         clock_gettime(CLOCK_MONOTONIC, &now);
         timespec_sub_saturate(&result, &deadline, &now);
         remaining_timeout = &result;
      }
      ret = ppoll(pfd, 1, remaining_timeout, NULL);
   } while (ret == -1 && errno == EINTR);

   return ret;
}

int
wl_display_dispatch_queue_timeout(struct wl_display *display,
                                  struct wl_event_queue *queue,
                                  const struct timespec *timeout)
{
   int ret;
   struct timespec now;
   struct timespec deadline = {0};
   struct timespec result;
   struct timespec *remaining_timeout = NULL;

   if (timeout) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      timespec_add(&deadline, &now, timeout);
   }

   if (wl_display_prepare_read_queue(display, queue) == -1)
      return wl_display_dispatch_queue_pending(display, queue);

   while (true) {
      ret = wl_display_flush(display);

      if (ret != -1 || errno != EAGAIN)
         break;

      if (timeout) {
         clock_gettime(CLOCK_MONOTONIC, &now);
         timespec_sub_saturate(&result, &deadline, &now);
         remaining_timeout = &result;
      }
      ret = wl_display_poll(display, POLLOUT, remaining_timeout);

      if (ret <= 0) {
         wl_display_cancel_read(display);
         return ret;
      }
   }

   /* Don't stop if flushing hits an EPIPE; continue so we can read any
    * protocol error that may have triggered it. */
   if (ret < 0 && errno != EPIPE) {
      wl_display_cancel_read(display);
      return -1;
   }

   while (true) {
      if (timeout) {
         clock_gettime(CLOCK_MONOTONIC, &now);
         timespec_sub_saturate(&result, &deadline, &now);
         remaining_timeout = &result;
      }

      ret = wl_display_poll(display, POLLIN, remaining_timeout);
      if (ret <= 0) {
         wl_display_cancel_read(display);
         break;
      }

      ret = wl_display_read_events(display);
      if (ret == -1)
         break;

      ret = wl_display_dispatch_queue_pending(display, queue);
      if (ret != 0)
         break;

      /* wl_display_dispatch_queue_pending can return 0 if we ended up reading
       * from WL fd, but there was no complete event to dispatch yet.
       * Try reading again. */
      if (wl_display_prepare_read_queue(display, queue) == -1)
         return wl_display_dispatch_queue_pending(display, queue);
   }

   return ret;
}
#endif

#ifndef HAVE_WL_CREATE_QUEUE_WITH_NAME
struct wl_event_queue *
wl_display_create_queue_with_name(struct wl_display *display, const char *name)
{
   return wl_display_create_queue(display);
}
#endif
