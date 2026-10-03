#ifndef COSMOPOLITAN_LIBC_ISYSTEM_SYS_SYSCALL_H_
#define COSMOPOLITAN_LIBC_ISYSTEM_SYS_SYSCALL_H_
#include "libc/stdio/syscall.h"
#ifdef __FILC__
/* Fil-C port: the test suite (and portable Linux code) uses the Linux SYS_*
   numbers with syscall(2).  cosmo's sys/syscall.h defines only three
   cosmo-ism ordinals (SYS_gettid=1, SYS_getrandom=2, SYS_getcpu=3), which
   collide with the real Linux numbers (write=1, ...); replace them with the
   real Linux values, matching what the musl flavor exposes.
   The numbers are the NATIVE LINUX NUMBERS OF THE TARGET ARCHITECTURE,
   which is exactly the ABI libpizlo's zsys_syscall() implements (see
   filc/src/syscall.h): usercosmo's syscall() forwards verbatim to
   zsys_syscall(), so a number from this header always resolves below
   libpizlo, on every host OS.  The set below is the set of calls
   zsys_syscall() actually implements (plus the ones with typed zsys_*
   wrappers); calling syscall() with an unsupported number traps in
   libpizlo, exactly like the musl flavor. */
#undef SYS_gettid
#undef SYS_getrandom
#undef SYS_getcpu
#if defined(__aarch64__)
#define SYS_gettid           178
#define SYS_getrandom        278
#define SYS_getcpu           168
#define SYS_write            64
#define SYS_read             63
#define SYS_futex            98
#define SYS_getdents64       61
#define SYS_statx            291
#define SYS_copy_file_range  285
#define SYS_openat2          437
#define SYS_getpid           172
#define SYS_close            57
#define SYS_settimeofday     170
#define SYS_renameat2        276
#define SYS_pidfd_open       434
#define SYS_setreuid         145
#define SYS_setregid         143
#define SYS_memfd_create     279
#define SYS_sched_setaffinity 122
#define SYS_sched_getaffinity 123
#define SYS_set_mempolicy    237
#define SYS_get_mempolicy    236
#define SYS_perf_event_open  241
#define SYS_add_key          217
#define SYS_request_key      218
#define SYS_keyctl           219
#else
#define SYS_gettid           186
#define SYS_getrandom        318
#define SYS_getcpu           309
#define SYS_write            1
#define SYS_read             0
#define SYS_futex            202
#define SYS_getdents64       217
#define SYS_statx            332
#define SYS_copy_file_range  326
#define SYS_openat2          437
#define SYS_getpid           39
#define SYS_close            3
#define SYS_settimeofday     164
#define SYS_renameat2        316
#define SYS_pidfd_open       434
#define SYS_setreuid         113
#define SYS_setregid         114
#define SYS_memfd_create     319
#define SYS_sched_setaffinity 203
#define SYS_sched_getaffinity 204
#define SYS_set_mempolicy    238
#define SYS_get_mempolicy    239
#define SYS_perf_event_open  298
#define SYS_add_key          248
#define SYS_request_key      249
#define SYS_keyctl           250
#endif
#define SYS_fchmodat2        452
#define SYS_landlock_create_ruleset 444
#define SYS_landlock_add_rule 445
#define SYS_landlock_restrict_self  446
#endif
#endif /* COSMOPOLITAN_LIBC_ISYSTEM_SYS_SYSCALL_H_ */
