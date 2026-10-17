// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=1 -verify=weak -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=2 -verify=odd -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=3 -verify=one -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=4 -verify=seven -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=5 -verify=internal -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=6 -verify=mixed -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -DCASE=0 -verify=allowed -o /dev/null %s

union Natural { void *pointer; unsigned long bits; };

// We deliberately do not support synthetic pointer words at unaligned offsets.
// Reject with a diagnostic, rather than inventing a byte-only byval descriptor.
#if CASE == 1
union __attribute__((packed)) Weak { // weak-error {{unaligned pointer-bearing union storage}}
  void *pointer; // weak-error {{unaligned pointer-bearing union}}
  unsigned long bits;
};
Weak weak;
#elif CASE == 2
union __attribute__((packed)) Odd { // odd-error {{unaligned pointer-bearing union storage}}
  void *pointer; // odd-error {{unaligned pointer-bearing union}}
  char bytes[9];
};
Odd array[2];
#elif CASE == 3
struct __attribute__((packed, aligned(8))) OffsetOne { // one-error {{unaligned pointer-bearing union storage}}
  char prefix;
  Natural choice;
};
OffsetOne atOne;
#elif CASE == 4
struct __attribute__((packed, aligned(8))) OffsetSeven { // seven-error {{unaligned pointer-bearing union storage}}
  char prefix[7];
  Natural choice;
};
OffsetSeven atSeven;
#elif CASE == 5
// Outer alignment does not repair a pointer at byte 1 in an alternative.
struct __attribute__((packed)) Payload { char prefix; void *pointer; };
union __attribute__((aligned(8))) Internal {
  Payload payload; // internal-error {{unaligned pointer-bearing union}}
  void *pointer;
};
Internal internal;
#elif CASE == 6
// Our deliberately conservative boundary covers every pointer in a record
// containing a union, even if that particular union is pointer-free.
union Numeric { unsigned long bits; double number; };
struct __attribute__((packed)) Mixed { // mixed-error {{unaligned pointer-bearing union storage}}
  char prefix;
  void *pointer;
  Numeric choice;
};
Mixed mixed;
#else
// allowed-no-diagnostics
// Packed does not itself mean unsafe: these offsets and strides are aligned.
struct __attribute__((packed, aligned(8))) Allowed {
  Natural choice;
  unsigned long sentinel;
};
Allowed allowed[2];

// Pointer-free packed unions retain their ordinary layout and remain allowed.
union __attribute__((packed)) Numeric { long bits; double number; char bytes[9]; };
Numeric numeric;
#endif
