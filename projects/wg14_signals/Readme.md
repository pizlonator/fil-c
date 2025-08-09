# Reference implementation for proposed extensions to standard C signals handling

(C) 2024 - 2025 Niall Douglas [http://www.nedproductions.biz/](http://www.nedproductions.biz/)

CI: [![CI](https://github.com/ned14/wg14_signals/actions/workflows/ci.yml/badge.svg)](https://github.com/ned14/wg14_signals/actions/workflows/ci.yml)

Can be configured to be a standard library implementation for your
standard C library runtime. Licensed permissively.

## Example of use

```c
/* Invoke `func` passing it `user_value`. If `SIGILL` is raised during the
execution of `func`, call `decider_func` as a filter to decide what to do.
If `decider_func` chooses to initiate recovery, perform as-if a `longjmp()`
back to before `func` was called, and invoke `recovery_func` to recover.

`thrd_signal_invoke()` can be stacked i.e. `func` can invoke subfunctions
with their own signal raise filter functions.
*/
sigset_t guarded;
sigemptyset(&guarded);
sigaddset(&guarded, SIGILL);
thrd_signal_invoke(&guarded, func, recovery_func, decider_func, user_value);
```

## Supported targets

This library should work well on any POSIX implementation, as well as
Microsoft Windows. You will need a minimum of C 11 in your toolchain.
Every architecture supporting C 11 atomics should work.

Current CI test targets:

- Ubuntu Linux, x64.
- Mac OS, AArch64.
- Microsoft Windows, x64.

Current compilers:

- GCC
- clang
- MSVC

## Configuration

You can find a number of user definable macros to override in `config.h`.
They have sensible defaults on the major platforms and toolchains.

The only one to be especially aware of is `WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL`.
If your platform and toolchain has async signal safe thread local storage,
it can be used instead of a hash table which is much, much faster. Current
platform status for this support can be read all about at
https://maskray.me/blog/2021-02-14-all-about-thread-local-storage, but to
summarise:

- Most ELF platforms require the `initial-exec` TLS attribute, which we
turn on for all GNU toolchains except on Apple ones. **IF** your libc
reserves static TLS space for runtime loaded shared libraries (e.g. glibc),
you can incorporate this library into runtime loaded shared libraries
without issue. If it does not, runtime loading a shared library will fail.
In this case, either place this library in a process bootstrap shared
library or the program library, or force disable off use of async safe
thread locals.

- PE platforms (Microsoft Windows) use async thread safe thread locals
in all situations. They are initialised when their shared library is
loaded.

- Mach O platforms (Apple) do not provide async thread safe thread locals
and so the fallback hash table is always used on those platforms, which
is unfortunate.

## Performance

### On my Threadripper 5975WX which is a 3.6Ghz processor bursting to 4.5Ghz on Linux:

- `tss_async_signal_safe_get()` which implements an async signal safe
thread local storage using a hash table costs about 8 nanoseconds, so
maybe 29 clock cycles.


With `WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL=1` (the default on Linux,
Windows, and other ELF based platforms):

    - `thrd_signal_invoke()` which invokes a function which thread locally
handles any signals raised costs about 16 nanoseconds (31 clock cycles)
for the happy case (most of this is the cost of `_setjmp()` on this platform
and glibc).

    - A globally installed signal decider takes about 8 nanoseconds (29
clock cycles) to reach (there is a CAS lock-unlock sequence needed).


With `WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL=0`:

    - `thrd_signal_invoke()` which invokes a function which thread locally
handles any signals raised costs about 25 nanoseconds for the happy case.

    - A globally installed signal decider takes about 10 nanoseconds to reach.

### On a MacBook Pro M3 running ARM64

- `tss_async_signal_safe_get()` which implements an async signal safe
thread local storage using a hash table costs about 16 nanoseconds.

    - `thrd_signal_invoke()` which invokes a function which thread locally
handles any signals raised costs about 20 nanoseconds for the happy case
(most of this is the cost of `_setjmp()` on this platform and libc).

    - A globally installed signal decider takes about 20 nanoseconds to reach
(there is a CAS lock-unlock sequence needed, and a hash table lookup).

### On a MacBook Pro M3 running ARM64 Windows within a VM

- `tss_async_signal_safe_get()` which implements an async signal safe
thread local storage using a hash table costs about 22 nanoseconds.

- `thrd_signal_invoke()` which invokes a function which thread locally
handles any signals raised costs about 17 nanoseconds (this is Windows
Structured Exception Handling, not our library code).

- A globally installed signal decider takes about 7,372 nanoseconds to reach
(this is also Windows code, not our library code, shame it is so slow).

# Todo

- Global signal deciders are still racy with respect to modification during
invocation. Given that they execute in an async signal unsafe situation,
not sure how much I care.
