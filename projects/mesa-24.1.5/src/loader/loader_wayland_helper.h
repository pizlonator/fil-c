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

#ifndef LOADER_WAYLAND_HELPER_HEADER_H
#define LOADER_WAYLAND_HELPER_HEADER_H

#include <wayland-client.h>

#ifndef HAVE_WL_DISPATCH_QUEUE_TIMEOUT

#include <util/timespec.h>
int
wl_display_dispatch_queue_timeout(struct wl_display *display,
                                  struct wl_event_queue *queue,
                                  const struct timespec *deadline);
#endif

#ifndef HAVE_WL_CREATE_QUEUE_WITH_NAME
struct wl_event_queue *
wl_display_create_queue_with_name(struct wl_display *display,
                                  const char *name);
#endif

#endif
