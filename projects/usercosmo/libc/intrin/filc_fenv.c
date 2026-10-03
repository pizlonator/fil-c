/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: software floating-point environment.        │
│                                                                              │
│ cosmo implements fenv for x86_64 in assembly (fnstcw/ldmxcsr/...).  Some of  │
│ those mnemonics are not on the Fil-Pizlonator's safe-inline-asm allowlist,   │
│ and the exception state is per-thread state anyway, so this is a plain       │
│ software implementation: flags and the rounding mode live in ordinary        │
│ variables.  The rounding mode is fixed to FE_TONEAREST, which is what the    │
│ hardware defaults to and what all of cosmo's own code assumes.               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/runtime/fenv.h"

static uint16_t __filc_fenv_flags;
static int __filc_fenv_round = FE_TONEAREST;

int feclearexcept(int excepts) {
  __filc_fenv_flags &= ~(uint16_t)excepts;
  return 0;
}

int fetestexcept(int excepts) {
  return __filc_fenv_flags & (uint16_t)excepts;
}

int feraiseexcept(int excepts) {
  __filc_fenv_flags |= (uint16_t)excepts;
  return 0;
}

int fegetenv(fenv_t *envp) {
  (void)envp;
  return 0;
}

int fesetenv(const fenv_t *envp) {
  /* FE_DFL_ENV (-1) means "restore the default environment"; any saved
     environment is equivalent to the default one in our software model. */
  (void)envp;
  __filc_fenv_flags = 0;
  __filc_fenv_round = FE_TONEAREST;
  return 0;
}



int fegetround(void) {
  return __filc_fenv_round;
}


