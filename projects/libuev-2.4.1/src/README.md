API Documentation
=================

* [Introduction](#introduction)
* [Overview](#overview)
  * [Create an Event Context](#create-an-event-context)
  * [Register an Event Watcher](#register-an-event-watcher)
  * [Start Event Loop](#start-event-loop)
  * [Summary](#summary)
* [Using -luev](#using--luev)
* [Joystick Example](#joystick-example)


> “Event driven software improves concurrency” -- [Dave Zarzycki, Apple][]

Introduction
------------

The C API to [libuEv][], listed in `uev/uev.h`, handles three different
types of events: I/O (pipes, sockets, message queues, etc.), timers, and
signals.  The [Summary](#summary) details a slight caveat on signals.

Notice the *lack of support for regular files* and directories.  This is
a limitation of the underlying Linux `epoll` interface which will return
`EPERM` for regular files.  Except for the special case when `stdin` is
redirected from the command line.  See `examples/redirect.c` for more on
this particular case.

Timers can be either relative, timeout in milliseconds, or absolute with
a time given in `time_t`, see `mktime()` et al.  Absolute timers are
called cron timers and their callbacks get an `UEV_HUP` error event if
the wall clock changes, either via NTP or user input.

**NOTE:** On some systems, embedded in particular, `time_t` is a 32-bit
  integer that wraps around in the year 2038.  A [GLIBC workaround][]
  (`-D_TIME_BITS=64`) protects those systems, but users of other C
  libraries have no known workarounds.  It is strongly recommended to
  use relative timers as often as possible.


Overview
--------

```C
/*
 * Callback example, arg comes from the watcher's *_init() function,
 * w->fd holds the file descriptor or socket, and events is set by
 * libuEv to indicate status: UEV_READ and/or UEV_WRITE with any of
 * the optional UEV_HUP, UEV_RDHUP, or UEV_PRI for urgent read data.
 *
 * Note: UEV_ERROR may be returned for any watcher and must be checked
 *       by all callbacks.  I/O watchers may also need to check UEV_HUP.
 *       Appropriate action, e.g. restart the watcher, is up to the
 *       application and is thus delegated to the callback.
 */
void callback       (uev_t *w, void *arg, int events);

/* Event loop:      Notice the use of flags! */
int uev_init        (uev_ctx_t *ctx);
int uev_init1       (uev_ctx_t *ctx, int maxevents);
int uev_exit        (uev_ctx_t *ctx);
int uev_run         (uev_ctx_t *ctx, int flags);         /* UEV_NONE, UEV_ONCE, and/or UEV_NONBLOCK */

/* I/O watcher:     fd      *MUST* be non-blocking!
 *                  events  combination of the main flags:  UEV_READ, UEV_WRITE,
 *                                                          UEV_EDGE, UEV_ONESHOT
 */
int uev_io_init     (uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, void *arg, int fd, int events);
int uev_io_set      (uev_t *w, int fd, int events);
int uev_io_start    (uev_t *w);
int uev_io_stop     (uev_t *w);

/* Timer watcher:   schedule a relative timer, timeout (must be non-zero) and period in milliseconds */
int uev_timer_init  (uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, void *arg, int timeout, int period);
int uev_timer_set   (uev_t *w, int timeout, int period); /* Change timeout or period */
int uev_timer_start (uev_t *w);                          /* Restart a stopped timer */
int uev_timer_stop  (uev_t *w);                          /* Stop a timer */

/* Cron watcher:    schedule an absolute timer, when and period in time_t seconds */
int uev_cron_init   (uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, void *arg, time_t when, time_t period);
int uev_cron_set    (uev_t *w, int when, time_t period); /* Change when or period */
int uev_cron_start  (uev_t *w);                          /* Restart a stopped cron */
int uev_cron_stop   (uev_t *w);                          /* Stop a cron */

/* Signal watcher:  signo is the signal to wait for, e.g., SIGTERM */
int uev_signal_init (uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, void *arg, int signo);
int uev_signal_set  (uev_t *w, int signo);               /* Change signal to wait for */
int uev_signal_start(uev_t *w);                          /* Restart a stopped signal watcher */
int uev_signal_stop (uev_t *w);                          /* Stop signal watcher */

/* Generic event watcher, post events for later processing, or from forked child */
int uev_event_init  (uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, void *arg);
int uev_event_post  (uev_t *w);
int uev_event_stop  (uev_t *w);
```


### Create an Event Context

To monitor events the developer first creates an *event context*, this
is achieved by calling `uev_init()` with a pointer to a (thread) local
`uev_ctx_t` variable.

```C
uev_ctx_t ctx;

uev_init(&ctx);
```


### Register an Event Watcher

For each event to monitor, be it a signal, cron/timer or a file/network
descriptor, a *watcher* must be registered with the event context.  The
watcher, an `uev_t`, is registered by calling the event type's `_init()`
function with the `uev_ctx_t` context, the callback, and an optional
argument.

Here is a signal example:

```C
void cleanup_exit(uev_t *w, void *arg, int events)
{
    if (UEV_ERROR == events)
        puts("Ignoring signal watcher error ...");
    else
        printf("Got signal (signo %d) from PID %d\n",
               w->siginfo.ssi_signo, w->siginfo.ssi_pid);

    /* Graceful exit, with optional cleanup ... */
    uev_exit(w->ctx);
}

int main(void)
{
    uev_t sigterm_watcher;

    .
    .
    uev_signal_init(&ctx, &sigterm_watcher, cleanup_exit, NULL, SIGTERM);
    .
    .
}
```

Notice that the callback must be prepared to handle `UEV_ERROR`.  I/O
watchers in particular, but also timer watchers, must be restarted if
required by the application.  libuEv automatically tries to restart a
signal watcher, but should that fail the callback will return error as
well.

I/O watchers should also check for `UEV_HUP`, preferably when handling
any short `read()` or `write()` system calls.  A short read on a socket
may be due to the remote end having performed a `shutdown()`.  This is
signaled to the callback using `UEV_HUP` in the `events` mask.


### Start Event Loop

When all watchers are registered, call the *event loop* with `uev_run()`
and the argument to the event context.  The `flags` parameter can be
used to integrate [libuEv][] into another event loop.

In this example we set `flags` to none:

```C
result = uev_run(&ctx, UEV_NONE);
```

With `flags` set to `UEV_ONCE` the event loop returns as soon as it has
served the first event.  If `flags` is set to `UEV_ONCE | UEV_NONBLOCK`
the event loop returns immediately if no event is available.

```
if (result < 0)
    errx(result, "Unrecoverable event loop error, error %d", result);
```

If the call to `uev_run()` fails you should notify the user somehow.
libuEv fails if there is an invalid pointer, if `uev_init()` was not
called, or recurring `epoll()` errors thare are impossible to recover
from should occur.  This is true for individual watchers as well, in
particular signal and timer watchers which can fail in miserable ways.

**Note:** libuEv handles many types of errors, stream close, or peer
shutdowns internally, but also lets the callback run.  This is useful
for stateful connections to be able to detect EOF.

### Summary

1. Set up an event context with `uev_init()`
2. Register event callbacks with the event context using
   `uev_io_init()`, `uev_signal_init()` or `uev_timer_init()`
3. Make sure callbacks checks their `events` mask and handles:

   - `UEV_ERROR`, e.g. I/O watchers must be restarted
   - `UEV_HUP`, reading any remaining data on the descriptor

   In both of these cases the watcher is stopped by libuEv.  On HUP the
   descriptor/connection must be reopened and the watcher reinitialized
   with `uev_io_set()`, if required by the application.
4. Start the event loop with `uev_run()`
5. Exit the event loop with `uev_exit()`, possibly from a callback

**Note 1:** Make sure to use non-blocking stream I/O!  Most hard to find
  bugs in event driven applications are due to sockets and files being
  opened in blocking mode.  Be careful out there!

**Note 2:** When closing a descriptor or socket, make sure to first stop
  your watcher, if possible.  This will help prevent any nasty side
  effects on your program.

**Note 3:** a certain amount of care is needed when dealing with APIs
  that employ signalfd.  If your application use `system()` you replace
  that with `fork()`, and then in the child, unblock all signals blocked
  by your parent process, before you run `exec()`.  This because Linux
  does not unblock signals for your children, and neither does most
  (all?)  C-libraries.  See the [finit][6] project's implementation of
  `run()` for an example of this.  For more details on this issue, see
  [this article][4] at [lwn.net](http://lwn.net).


Using -luev
-----------

libuEv is by default installed as a library with a few header files, you
should only ever need to include one:

```C
#include <uev/uev.h>
```

The output from the `pkg-config` tool holds no surprises:

```sh
$ pkg-config --libs --static --cflags libuev
-I/usr/local/include -L/usr/local/lib -luev
```

The prefix path `/usr/local/` shown here is only the default.  Use the
`configure` script to select a different prefix when installing libuEv.

For GNU autotools based projects, use the following in `configure.ac`:

```sh
# Check for required libraries
PKG_CHECK_MODULES([uev], [libuev >= 1.4.0])
```

and in your `Makefile.am`:

```sh
proggy_CFLAGS = $(uev_CFLAGS)
proggy_LDADD  = $(uev_LIBS)
```


Joystick Example
----------------

Here follows a very brief example to illustrate how one can use libuEv
to act upon joystick input.

```C
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <uev/uev.h>

struct js_event {
    uint32_t time;      /* event timestamp in milliseconds */
    int16_t  value;     /* value */
    uint8_t  type;      /* event type */
    uint8_t  number;    /* axis/button number */
} e;

/*
 * Called on any joystick event
 *
 * Note: We handle errors here by exiting, for a background daemon with
 *       multiple watchers you may want another approach.
 */
static void joystick_cb(uev_t *w, void *arg, int events)
{
    ssize_t cnt;

    if (UEV_ERROR == events) {
        /* Possibly joystick was unplugged */
        warnx("Spurious problem with the joystick watcher, restarting.");
        uev_io_start(w);
        return;
    }

    cnt = read(w->fd, &e, sizeof(e));
    if (cnt < 0) {
        warn("Failed reading joystick event");
        return;
    }

    if (cnt == 0 || UEV_HUP == events) {
        warn("Joystick disconnected");
        return;
    }

    switch (e.type) {
    case 1:
        printf("Button %d %s\n", e.number, e.value ? "pressed" : "released");
        break;

    case 2:
        printf("Joystick axis %d moved, value %d!\n", e.number, e.value);
        break;
    }
}

int main(void)
{
    uev_ctx_t ctx;
    uev_t js;
    int fd;

    fd = open("/dev/input/js0", O_RDONLY, O_NONBLOCK);
    if (fd < 0)
        errx(errno, "Cannot find a joystick attached.");

    uev_init(&ctx);
    uev_io_init(&ctx, &js, joystick_cb, NULL, fd, UEV_READ);

    puts("Starting, press Ctrl-C to exit.");

    return uev_run(&ctx, 0);
}
```

To build the example, follow installation instructions below, then save
the code as `joystick.c` and call GCC

```sh
$ gcc `pkg-config --libs --static --cflags libuev` -o joystick joystick.c
```

Alternatively, call the `Makefile` with <kbd>make joystick</kbd> from
the unpacked [libuEv][] distribution.

More complete and relevant example uses of [libuEv][] is the FTP/TFTP
server [uftpd][5], and the Linux `/sbin/init` replacement [finit][6].
Both successfully employ [libuEv][].

Also see the `bench.c` program (<kbd>make bench</kbd> from within the
library) for [reference benchmarks][7] against [libevent][1] and
[libev][2].

[1]:      http://libevent.org
[2]:      http://software.schmorp.de/pkg/libev.html
[4]:      http://lwn.net/Articles/415684/
[5]:      https://github.com/troglobit/uftpd
[6]:      https://github.com/troglobit/finit
[7]:      http://libev.schmorp.de/bench.html
[libuEv]: https://github.com/troglobit/libuev
[GLIBC workaround]: https://sourceware.org/glibc/wiki/Y2038ProofnessDesign
[Dave Zarzycki, Apple]: http://www.youtube.com/watch?v=cD_s6Fjdri8
