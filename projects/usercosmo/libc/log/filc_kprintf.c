/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: kprintf family.                             │
│                                                                              │
│ Cosmo's own kprintf.greg.c is excluded from this build (raw syscall inline   │
│ asm), but a growing number of programs reference the kprintf family:         │
│ cosmo's swprintf() stub is a kprintf caller, and the crash-report helpers     │
│ (die.c, crash.c, ubsan.c, ...) all report through kprintf.  Without these    │
│ shims every program that pulls one of those objects fails to link with       │
│ undefined __kprintf/__ksnprintf references.                                  │
│                                                                              │
│ This is a plain C reimplementation: format with the ordinary (already         │
│ Fil-C-clean) printf machinery and write the result to stderr, which is       │
│ what kprintf amounts to on a Linux host.                                     │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/intrin/kprintf.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"

size_t __kvsnprintf(char *buf, size_t size, const char *format, va_list va) {
  return vsnprintf(buf, size, format, va);
}

void __kvprintf(const char *format, va_list va) {
  char buf[1024];
  size_t n = __kvsnprintf(buf, sizeof(buf), format, va);
  if (n >= sizeof(buf))
    n = sizeof(buf) - 1;
  if (n)
    write(2, buf, n);
}

void __kprintf(const char *format, ...) {
  va_list va;
  va_start(va, format);
  __kvprintf(format, va);
  va_end(va);
}

size_t __ksnprintf(char *buf, size_t size, const char *format, ...) {
  va_list va;
  va_start(va, format);
  size_t n = __kvsnprintf(buf, size, format, va);
  va_end(va);
  return n;
}

void __klog(const char *data, size_t size) {
  if (size)
    write(2, data, size);
}