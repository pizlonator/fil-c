// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -emit-llvm -mllvm -filc-light-verbose -o %t %s 2>&1 | FileCheck %s --check-prefix=PRE

union Choice { void *pointer; unsigned long words[3]; };
extern int take(union Choice);
extern int take_unnamed(int tag, ...);

// Byval uses the ordinary pointer-bearing object type, not a byte descriptor.
// Its typed checks must enforce actual pointer alignment even when a caller's
// C cast lies. There is no extra alignment-staging transport mode.
// PRE-LABEL: define {{.*}} @forward(
// PRE: call i32 @take(ptr {{.*}}byval(%union.Choice) align 8 %value)
int forward(const union Choice *value) { return take(*value); }

// PRE-LABEL: define {{.*}} @forward_unnamed(
// PRE: call i32 (i32, ...) @take_unnamed(i32 0, ptr {{.*}}byval(%union.Choice) align 8 %value)
int forward_unnamed(const union Choice *value) { return take_unnamed(0, *value); }
