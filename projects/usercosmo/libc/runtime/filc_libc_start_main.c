/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: program startup.                            │
│                                                                              │
│ Cosmo itself has no __libc_start_main(); its bootstrapping happens in        │
│ hand-written assembly (_start → cosmo() → _init chain).  When a cosmo-flavor │
│ program is built by Fil-C, the process boot looks like this:                 │
│                                                                              │
│   1. The yolo (un-pizlonated) cosmo boot runs first: cosmo's _start reads    │
│      argc/argv/envp/auxv off the initial stack and calls cosmo(), which      │
│      runs the _init chain (systemfive, pagesize, maps, TLS for the yolo      │
│      side, fds) and then calls main() — which is filc_crt.o's main,          │
│      calling filc_start_program().                                           │
│   2. filc_start_program() (libpizlo) calls THIS function — the user libc's   │
│      __libc_start_main() — passing the program's real main, exactly like     │
│      the musl flavor of Fil-C does.                                          │
│                                                                              │
│ This file mirrors projects/usermusl/src/env/__libc_start_main.c: it wires    │
│ up the pizlonated side of the world (Fil-C TLS-based TIB, errno handler,     │
│ cosmo globals like __hostos/__pagesize which exist in two copies — one       │
│ plain for the yolo side, one pizlonated for this side), runs the deferred    │
│ global constructors, and finally calls the user's main().                    │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/runtime/runtime.h"
#include "libc/str/locale.internal.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/tls.h"
#include <stdfil.h>
#include <pizlonated_runtime.h>
#include <pizlonated_syscalls.h>

/* Defined in libc/intrin/pagesize.c; cosmo only declares it internally. */
void __pagesize_init(unsigned long *auxv);

/* Fil-C land's own copy of the host os flag (the yolo copy lives in
   libc/sysv/hostos.S and is initialized by the yolo boot).  Declared non-const
   in libc/dce.h under __FILC__ so we can initialize it here. */
int __hostos = _HOSTLINUX;

/* cosmo defines these boot globals in assembly on x86_64
   (libc/nexgen32e/auxv.S, whose values are filled by the _init_auxv blob);
   the aarch64 C copies (auxv2.c/environ2.c) are excluded from this build.
   Define and fill them here instead. */
unsigned long *__auxv;
char **__envp;
char **environ;
char *program_invocation_name;

/* Implemented in libc/nexgen32e/filc_tables.c. */
void __filc_init_cpuids(void);

static void filc_errno_handler(int errno_value) {
  errno = errno_value;
}

static void filc_dlerror_handler(const char *str) {
  /* Cosmo doesn't have musl's __dl_seterr machinery; dropping the message is
     the best we can do for now. */
  (void)str;
}

/* The pizlonated copies of cosmo's boot globals (the yolo boot only fills the
   plain-symbol copies; libc code compiled by Fil-C sees these). */
int __argc;
char **__argv;

static void filc_setup_main_thread(void) {
  /* The main thread's PosixThread object: a static PT_STATIC instance, set up
     the same way __enable_tls() does it for the kernel TIB in the yolo world.
     __filc_init_tib() installs the TIB into the zthread cookie (see
     libc/thread/filc_tls.c) and returns it; it must run before anything in
     pizlonated land touches __get_tls(). */
  _pthread_static.pt_flags = PT_STATIC;
  _pthread_static.pt_locale = &__global_locale;
  dll_init(&_pthread_static.list);
  if (!_pthread_list)
    _pthread_list = &_pthread_static.list;
  _pthread_static.tib = __filc_init_tib(&_pthread_static);
  /* Wire up tib_ptid exactly like pthread_create.c's PosixThread() does for
     spawned threads: without this, _pthread_tid(&_pthread_static) spins
     forever waiting for tib_ptid to become non-zero (any libc call that
     resolves the current thread's tid - pthread_setschedparam(),
     pthread_getschedparam(), pthread_kill(), ... - goes through that loop),
     and gettid() falls off its tib_ptid fast path.  zthread_self_id() is the
     same kernel tid that gettid() must return for the main thread, matching
     the spawned-thread convention (and the fork.c child re-init). */
  atomic_init(&_pthread_static.tib->tib_ptid, zthread_self_id());
  /* tib_ptid participates in cosmo's recursive-mutex
     protocol: pthread_mutex_lock_recursive_*() treats
     MUTEX_OWNER(word) == tib_ptid as "recursive reentry" and skips the
     underlying nsync lock.  An unlocked recursive mutex has owner == 0, so
     a main thread with tib_ptid == 0 (zero-initialized zgc memory) would
     "reenter" every recursive lock - including stdout/stderr/stdin's FILE
     locks - without holding it, breaking mutual exclusion the moment any
     other thread touches the same lock.

     NB: tib_ctid is deliberately NOT set here.  _pthread_decimate() treats
     a zero tib_ctid as "this thread is not using its stack anymore"; the
     main thread's PosixThread lives for the whole process and cosmo's own
     __enable_tls() boot path (which this mirrors) leaves tib_ctid zero for
     the static main thread as well. */
}

static void filc_init_globals(char **argv, char **envp, size_t *auxv) {
  /* cpuid feature tables only exist on x86 (kCpuids feeds x86feature.h and
     the __cpu_model machinery; aarch64 has no cpuid instruction). */
#if defined(__x86_64__)
  __filc_init_cpuids();
#endif
  /* Cosmo's globals exist twice in the final binary: the plain copies that
     the yolo boot initialized (crt.S + _init blobs), and these pizlonated
     copies that only pizlonated code can see.  Re-derive the pizlonated ones
     from the arguments filc_start_program() passed us. */
  __auxv = auxv;
  __envp = envp;
  environ = envp;
  __pagesize_init(auxv);
  if (argv && argv[0]) {
    __program_executable_name = argv[0];
    program_invocation_name = argv[0];
    /* The pizlonated copies of __argc/__argv are referenced by e.g.
       program_invocation_short_name.c; the yolo boot only fills the yolo
       copies. */
    __argc = 0;
    while (argv[__argc])
      __argc++;
    __argv = argv;
  }
}

void __libc_start_main(int (*main)(int, char **, char **), int argc,
                       char **argv, char **envp, size_t *auxv) {
  static bool once;

  if (!once) {
    once = true;
    filc_setup_main_thread();
    filc_init_globals(argv, envp, auxv);
    zregister_sys_errno_handler(filc_errno_handler);
    zregister_sys_dlerror_handler(filc_dlerror_handler);
    /* Runs the Fil-C global constructors that were deferred while the yolo
       boot walked __init_array (they call filc_defer_or_run_global_ctor()
       which defers until this point). */
    zrun_deferred_global_ctors();
  }

  exit(main(argc, argv, envp));
}
