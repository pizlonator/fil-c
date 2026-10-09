// Passes structs by value in memory (LLVM's byval: x86_64 for a struct over 16 bytes, aarch64 only
// for such a struct as a variadic argument; C++ copies to a temporary first) from misaligned
// addresses in a buffer of pointers, or with tail padding past the object. No argument: runs every
// case that must work. --list: one "name message" line per case that must die. A name: runs it.
#include <stdarg.h>
#include <stdfil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOINLINE __attribute__((noinline))

static int cases, failures;
static void check(int ok, const char *who, const char *what, int offset) {
  cases++;
  if (!ok) {
    failures++;
    printf("FAIL struct %s: %s (struct passed from byte %d of its object)\n", who, what, offset);
    fflush(stdout);  // a later case may crash
  }
}

struct words { long a, b, c; };                   // 8-aligned, no pointer member
struct ints { int a[8]; };                        // 4-aligned, no pointer member
struct long_then_ints { long x; struct ints s; }; // s is 8 bytes in: 8-aligned as far as the compiler knows
struct int_then_ints { int x; struct ints s; };   // s is 4 bytes in: a correct program passes it from there
struct bytes25 { char c[25]; };                   // 1-aligned, and its copy ends in the middle of a word
struct long_then_bytes25 { long x; struct bytes25 s; };
struct pointers { char *a, *b; long n; };         // has pointer members, unlike those above
struct padded { __int128 a; void *p; };           // 32 bytes, the last member ends at 24
// va_arg of a struct that declares no pointer loads no capabilities, so the variadic callees read
// their argument back as a struct of the same size that does, to see what capabilities travelled.
struct words_as_pointers { void *a, *b, *c; };
struct ints_as_pointers { void *w[4]; };

// What the callee found in its copy: each 8-byte word's bytes and the capability that came with it.
static uintptr_t got_bits[4], got_lower[4];
static void look(const void *copy, unsigned words) {
  for (unsigned i = 0; i < words; i++) {
    void *p;
    memcpy(&p, (const char *)copy + 8 * i, sizeof p);  // both 8-aligned, so the word's capability is copied too
    got_bits[i] = (uintptr_t)p;
    got_lower[i] = (uintptr_t)zgetlower(p);            // reads the capability, not the object
  }
}
static NOINLINE void take_words(struct words v) { look(&v, 3); }
static NOINLINE void take_ints(struct ints v) { look(&v, 4); }
static NOINLINE void take_padded(struct padded v) { look(&v, 4); }
static NOINLINE void take_bytes25(struct bytes25 v) { look(&v, 3); }
static NOINLINE void take_words_va(int n, ...) {
  va_list ap;
  va_start(ap, n);
  struct words_as_pointers v = va_arg(ap, struct words_as_pointers);
  va_end(ap);
  look(&v, 3);
}
static NOINLINE void take_ints_va(int n, ...) {
  va_list ap;
  va_start(ap, n);
  struct ints_as_pointers v = va_arg(ap, struct ints_as_pointers);
  va_end(ap);
  look(&v, 4);
}
// A struct that declares pointers: the callee uses them (DEREFERENCE) or only looks at what came.
static NOINLINE int take_pointers(struct pointers v, int dereference) {
  look(&v, 3);
  return dereference ? v.a[0] + v.b[0] + (int)v.n : 0;
}
static NOINLINE int take_pointers_va(int dereference, ...) {
  va_list ap;
  va_start(ap, dereference);
  struct pointers v = va_arg(ap, struct pointers);
  va_end(ap);
  look(&v, 3);
  return dereference ? v.a[0] + v.b[0] + (int)v.n : 0;
}

// A buffer of 8 pointers to 8 objects, so each of its words has a capability.
static void *objects[8];
static char *pointer_buffer(void) {
  void **buffer = (void **)malloc(8 * sizeof(void *));
  for (int i = 0; i < 8; i++)
    buffer[i] = objects[i] = malloc(16);
  return (char *)buffer;
}

// After a copy of WORDS words from byte OFFSET of the buffer: the bytes are the buffer's, and each
// word's capability is the one the buffer had for exactly that word (8-aligned source) or none.
static void check_copy(const char *buffer, int offset, unsigned words, const char *who) {
  int bytes_ok = 1, capabilities_ok = 1, kept = 1;
  for (unsigned i = 0; i < words; i++) {
    uintptr_t expected;
    memcpy(&expected, buffer + offset + 8 * i, sizeof expected);
    if (got_bits[i] != expected)
      bytes_ok = 0;
    uintptr_t real = offset % 8 ? 0 : (uintptr_t)objects[offset / 8 + i];
    if (got_lower[i] && got_lower[i] != real)
      capabilities_ok = 0;
    if (got_lower[i] != real)
      kept = 0;
  }
  check(bytes_ok, who, "the callee's bytes differ from the source's", offset);
  check(capabilities_ok, who, "a word arrived with a capability that is not its own", offset);
  if (offset % 8 == 0)
    check(kept, who, "an 8-aligned source lost a capability", offset);
}

// padding-*: struct padded's last 8 bytes (padding) lie past its object. The compiler's check at a
// byval call refuses that; where a temporary is filled first, the inlined memcpy's check does, with
// the same message. pointer-*: a misaligned struct's pointers arrive with no capability to use.
static const char *const must_die[][2] = {
  { "padding-out-of-bounds", "cannot read 32 bytes when upper - ptr = 24." },
  { "padding-out-of-bounds-variadic", "cannot read 32 bytes when upper - ptr = 24." },
  { "pointer-from-misaligned-struct", "cannot read pointer with null object." },
  { "pointer-from-misaligned-struct-variadic", "cannot read pointer with null object." },
};

static NOINLINE void take_padded_va(int n, ...) {
  va_list ap;
  va_start(ap, n);
  struct padded v = va_arg(ap, struct padded);
  va_end(ap);
  look(&v, 4);
}

static int die(void) {
  char *o = (char *)malloc(32);
  memset(o, 0x11, 32);
  struct padded *p = (struct padded *)(o + 8);  // its members end at o + 32, its padding at o + 40
  take_padded_va(0, *p);
  return 0;
}

int main(int argc, char **argv) {
    return die();
}
