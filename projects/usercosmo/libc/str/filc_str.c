/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: string functions.                           │
│                                                                              │
│ cosmo's string functions are heavily vectorized: they routinely load         │
│ aligned 16/32-byte chunks around the scan position, i.e. they read *past*    │
│ the NUL terminator and *before* the start of the string.  Real memory        │
│ makes that harmless, but Fil-C pointers are capabilities with exact          │
│ bounds — a string literal "one" is a 4-byte object and a 16-byte vector      │
│ load near it traps.                                                          │
│                                                                              │
│ So under Fil-C this file provides simple byte-wise (and half-word-wise)      │
│ implementations of the scanning string functions, which never touch a byte   │
│ outside the object they were handed.  The cosmo originals replaced here are  │
│ excluded from the build (see build_usercosmo.sh).  mem* functions that stay  │
│ exactly within their `n` argument (memset/memcpy/memmove/memcmp) keep the    │
│ vectorized cosmo versions.                                                   │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/str/str.h"
#include "libc/limits.h"
#include "libc/ctype.h"
#include "libc/mem/mem.h"

/**
 * Returns length of NUL-terminated string.
 */
size_t strlen(const char *s) {
  const char *p = s;
  while (*p)
    ++p;
  return p - s;
}

/**
 * Returns length of NUL-terminated string, at most n.
 */
size_t strnlen(const char *s, size_t n) {
  const char *p = s;
  while (n-- && *p)
    ++p;
  return p - s;
}

/**
 * Copies string.
 */
char *strcpy(char *d, const char *s) {
  char *d0 = d;
  do {
    *d++ = *s;
  } while (*s++);
  return d0;
}

/**
 * Copies at most n bytes of string, padding with NUL.
 */
char *strncpy(char *d, const char *s, size_t n) {
  char *d0 = d;
  while (n--) {
    if (!(*d++ = *s++)) {
      while (n--)
        *d++ = 0;
      break;
    }
  }
  return d0;
}

/**
 * Appends string.
 */
char *strcat(char *d, const char *s) {
  char *d0 = d;
  while (*d)
    ++d;
  do {
    *d++ = *s;
  } while (*s++);
  return d0;
}

/**
 * Appends at most n bytes of string.
 */
char *strncat(char *d, const char *s, size_t n) {
  char *d0 = d;
  if (n) {
    while (*d)
      ++d;
    while (n--) {
      if (!(*d++ = *s++))
        return d0;
    }
    *d = 0;
  }
  return d0;
}

/**
 * Compares strings.
 */
int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return (*(unsigned char *)a - *(unsigned char *)b);
}

/**
 * Compares at most n bytes of strings.
 */
int strncmp(const char *a, const char *b, size_t n) {
  if (!n--)
    return 0;
  for (; n && *a && *a == *b; --n, ++a, ++b)
    ;
  return (*(unsigned char *)a - *(unsigned char *)b);
}

/**
 * Finds first occurrence of c in string.
 */
char *strchr(const char *s, int c) {
  char ch = c;
  for (;; ++s) {
    if (*s == ch)
      return (char *)s;
    if (!*s)
      return 0;
  }
}

/**
 * Finds last occurrence of c in string.
 */
char *strrchr(const char *s, int c) {
  char *res = 0;
  char ch = c;
  for (;; ++s) {
    if (*s == ch)
      res = (char *)s;
    if (!*s)
      return res;
  }
}

/**
 * Finds first occurrence of c in string, or the terminator.
 */
char *strchrnul(const char *s, int c) {
  char ch = c;
  for (;; ++s) {
    if (*s == ch || !*s)
      return (char *)s;
  }
}

/**
 * Finds first occurrence of needle in haystack.
 */
char *strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;
  for (; *haystack; ++haystack) {
    const char *h = haystack;
    const char *n = needle;
    while (*h && *n && *h == *n)
      ++h, ++n;
    if (!*n)
      return (char *)haystack;
  }
  return 0;
}

/**
 * Case-insensitive strstr.
 */
char *strcasestr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;
  for (; *haystack; ++haystack) {
    const char *h = haystack;
    const char *n = needle;
    while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n))
      ++h, ++n;
    if (!*n)
      return (char *)haystack;
  }
  return 0;
}

/**
 * Finds byte in memory.
 */
void *memchr(const void *s, int c, size_t n) {
  const unsigned char *p = s;
  unsigned char ch = c;
  while (n--) {
    if (*p == ch)
      return (void *)p;
    ++p;
  }
  return 0;
}

/**
 * Finds last byte in memory.
 */
void *memrchr(const void *s, int c, size_t n) {
  const unsigned char *p = s;
  unsigned char ch = c;
  const unsigned char *res = 0;
  while (n--) {
    if (*p == ch)
      res = p;
    ++p;
  }
  return (void *)res;
}

/**
 * Finds byte sequence in memory.
 */
void *memmem(const void *haystack, size_t hl, const void *needle, size_t nl) {
  const unsigned char *h = haystack;
  const unsigned char *n = needle;
  if (!nl)
    return (void *)h;
  if (nl > hl)
    return 0;
  for (size_t i = 0; i + nl <= hl; ++i) {
    size_t j;
    for (j = 0; j < nl; ++j)
      if (h[i + j] != n[j])
        break;
    if (j == nl)
      return (void *)(h + i);
  }
  return 0;
}

/* ── half-width (char16_t) companions ───────────────────────────────────── */

size_t strlen16(const char16_t *s) {
  const char16_t *p = s;
  while (*p)
    ++p;
  return p - s;
}

char16_t *strchr16(const char16_t *s, char16_t c) {
  char16_t ch = c;
  for (;; ++s) {
    if (*s == ch)
      return (char16_t *)s;
    if (!*s)
      return 0;
  }
}

int strcmp16(const char16_t *a, const char16_t *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return (int)(*a) - (int)(*b);
}

char16_t *strcpy16(char16_t *d, const char16_t *s) {
  char16_t *d0 = d;
  do {
    *d++ = *s;
  } while (*s++);
  return d0;
}

char16_t *strcat16(char16_t *d, const char16_t *s) {
  char16_t *d0 = d;
  while (*d)
    ++d;
  do {
    *d++ = *s;
  } while (*s++);
  return d0;
}

/* ── wide (wchar_t) companions ──────────────────────────────────────────── */

size_t wcslen(const wchar_t *s) {
  const wchar_t *p = s;
  while (*p)
    ++p;
  return p - s;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c) {
  for (;; ++s) {
    if (*s == c)
      return (wchar_t *)s;
    if (!*s)
      return 0;
  }
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c) {
  wchar_t *res = 0;
  for (;; ++s) {
    if (*s == c)
      res = (wchar_t *)s;
    if (!*s)
      return res;
  }
}

int wcscmp(const wchar_t *a, const wchar_t *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return (int)(*a - *b);
}

wchar_t *wcscpy(wchar_t *d, const wchar_t *s) {
  wchar_t *d0 = d;
  do {
    *d++ = *s;
  } while (*s++);
  return d0;
}


char16_t *strchrnul16(const char16_t *s, char16_t c) {
  char16_t ch = c;
  for (;; ++s) {
    if (*s == ch || !*s)
      return (char16_t *)s;
  }
}

char16_t *strrchr16(const char16_t *s, int c) {
  char16_t ch = (char16_t)c;
  char16_t *res = 0;
  for (;; ++s) {
    if (*s == ch)
      res = (char16_t *)s;
    if (!*s)
      return res;
  }
}


size_t strnlen16(const char16_t *s, size_t n) {
  const char16_t *p = s;
  while (n-- && *p)
    ++p;
  return p - s;
}

int strncmp16(const char16_t *a, const char16_t *b, size_t n) {
  if (!n--)
    return 0;
  for (; n && *a && *a == *b; --n, ++a, ++b)
    ;
  return (int)(*a - *b);
}

char16_t *memchr16(const char16_t *s, char16_t c, size_t n) {
  while (n--) {
    if (*s == c)
      return (char16_t *)s;
    ++s;
  }
  return 0;
}

char16_t *rawmemchr16(const char16_t *s, char16_t c) {
  for (;; ++s) {
    if (*s == c)
      return (char16_t *)s;
  }
}

char16_t *strstr16(const char16_t *haystack, const char16_t *needle) {
  if (!*needle)
    return (char16_t *)haystack;
  for (; *haystack; ++haystack) {
    const char16_t *h = haystack;
    const char16_t *n = needle;
    while (*h && *n && *h == *n)
      ++h, ++n;
    if (!*n)
      return (char16_t *)haystack;
  }
  return 0;
}

char16_t *strncat16(char16_t *d, const char16_t *s, size_t n) {
  char16_t *d0 = d;
  if (n) {
    while (*d)
      ++d;
    while (n--) {
      if (!(*d++ = *s++))
        return d0;
    }
    *d = 0;
  }
  return d0;
}

char32_t *rawmemchr32(const char32_t *s, char32_t c) {
  for (;; ++s) {
    if (*s == c)
      return (char32_t *)s;
  }
}

int memcasecmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a;
  const unsigned char *y = b;
  for (; n--; ++x, ++y) {
    int d = tolower(*x) - tolower(*y);
    if (d)
      return d;
  }
  return 0;
}

void *memrchr16(const void *s, int c, size_t n) {
  const char16_t *p = s;
  char16_t ch = (char16_t)c;
  const char16_t *res = 0;
  while (n--) {
    if (*p == ch)
      res = p;
    ++p;
  }
  return (void *)res;
}

wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle) {
  if (!*needle)
    return (wchar_t *)haystack;
  for (; *haystack; ++haystack) {
    const wchar_t *h = haystack;
    const wchar_t *n = needle;
    while (*h && *n && *h == *n)
      ++h, ++n;
    if (!*n)
      return (wchar_t *)haystack;
  }
  return 0;
}
