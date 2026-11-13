/* Pizlonated exit() for the usercosmo flavor.
 *
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cosmo's exit() (libc/runtime/exit.c) runs the C++ thread destructors, the
 * __cxa_atexit handlers, and the .fini_array, and then terminates the
 * process.  It does not flush stdio itself: the flush is registered by a
 * __constructor__(60) inside libc/stdio/fflush.c (fflush_init -> __cxa_atexit
 * ((void *)fflush, 0, 0)).  That works in a normal cosmo build because the
 * link always includes fflush.c.o there; in the Fil-C cosmo flavor the libc
 * is a static archive that is scanned like any other archive, so fflush.c.o
 * is only pulled in when the program itself references fflush/fclose.  A
 * program that merely does fopen() + fprintf() and exits (e.g. bash's
 * mksignames build tool) therefore loses everything still sitting in the
 * stdio buffer - the process exits 0 and leaves a zero-length file.  (musl
 * never had this problem: its exit() calls __stdio_exit() directly, and the
 * Fil-C musl flavor inherited that.)
 *
 * This file replaces libc/runtime/exit.c (excluded in filc.mk) with the same
 * function plus an unconditional fflush(NULL) after the destructors, which
 * both guarantees the flush and matches POSIX's exit-time stdio semantics.
 * Referencing fflush here also makes the archive scan pull fflush.c.o into
 * every link, so cosmo's own flush registration rides along too.
 *
 * Pulling fflush.c.o into every link has a subtle consequence that bit the
 * filc/run-tests suite: any pizlonated touch of cosmo's __stdio state (the
 * FILE list that fflush(NULL) walks) goes through the Fil-C lazy-global-
 * initialization machinery, and for a program that never used stdio that
 * first touch happens INSIDE exit() - from the fflush constructor registered
 * with __cxa_atexit and from our own fflush(NULL) below.  Running the
 * global-initialization stub that late races the concurrent garbage
 * collector: the stub's frame and the exit path around it hand the collector
 * tracked pointers into stack regions that get reused while the GC is still
 * marking, and the collector then pops a mark-stack entry whose "object"
 * header is really saved thread state, dereferences the garbage aux, and
 * segfaults (observed as filc/tests/iswctype failing under FUGC rage mode,
 * with the collector crashing in filc_object_mark_outgoing_ptrs while the
 * main thread sat in pizlonated___stdio <- fflush <- __cxa_finalize <-
 * exit).  Programs that touch stdio before exiting never hit this, because
 * the stub runs at the first printf instead.
 *
 * So the constructor below eagerly touches stdio during startup, making the
 * exit-time flushes (ours and cosmo's registered one) always find an
 * already-initialized __stdio.  The cost is that every cosmo-flavor program
 * initializes its (empty) stdio state at boot, which is what statically
 * initialized stdio in the musl flavor effectively did anyway.
 */

#include "libc/cxxabi.h"
#include "libc/intrin/cxaatexit.h"
#include "libc/intrin/strace.h"
#include "libc/intrin/weaken.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"

/* Initialize the stdio state during startup; see the block comment above for
 * why this must not be left to the first touch inside exit().  The flush
 * itself is a no-op this early: nothing is open and nothing is buffered. */
__attribute__((__constructor__(160))) static textstartup void filc_stdio_early_init(void) {
  fflush(NULL);
}

wontreturn void exit(int exitcode) {
  STRACE("exit(%d)", exitcode);

  // call thread local c++ object destructors
  __cxa_thread_finalize();

  // call atexit() and __cxa_atexit() destructors
  __cxa_finalize(NULL);

  // call __destructor__ and finiarray destructors
  const uintptr_t *p;
  for (p = __fini_array_end; p > __fini_array_start;) {
    ((void (*)(void))(*--p))();
  }

  // flush every open stdio stream (cosmo would do this via fflush.c.o's
  // constructor; see the comment above)
  fflush(NULL);

  // terminate process
  _Exit(exitcode);
}
