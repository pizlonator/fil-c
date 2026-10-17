// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -emit-llvm -mllvm -filc-light-verbose -o %t %s 2>&1 | FileCheck %s --check-prefix=PRE --implicit-check-not=zhas_union
// RUN: FileCheck %s --check-prefix=POST < %t

// Hidden pointers become pointer-word storage; pointer-free unions keep their
// ordinary representation. Member access types and C/C++ sizes do not change.
union Hidden { unsigned long bits; void *pointers[1]; };
union Numeric { unsigned long bits; double number; };
Hidden hidden;
Numeric numeric;
// PRE: %union.Hidden = type { <{ [1 x ptr] }> }
// PRE: %union.Numeric = type { i64 }
static_assert(sizeof(Hidden) == 8 && sizeof(Numeric) == 8);
// PRE-DAG: %union.NestedMember = type { <{ [1 x ptr] }> }
// PRE-DAG: %union.BaseMember = type { <{ [1 x ptr] }> }
// PRE-DAG: %union.VptrMember = type { <{ [1 x ptr] }> }
// PRE-DAG: %union.FunctionMember = type { <{ [2 x ptr] }> }

struct Owner { int value; };
union Member { int Owner::*member; void *pointer; };
Member member;
thread_local Member tlsMember;
// Nonzero-null initialization uses the first member's existing null emitter,
// rather than manufacturing a pointer from the all-ones integer pattern.
// PRE: @member = {{.*}}global { i64 } { i64 -1 }, align 8
// PRE: @tlsMember = {{.*}}thread_local global { i64 } { i64 -1 }, align 8

// Nonzero null initialization must explicitly zero both nested padding and
// the union's extra word. Undef here lets optimization discard heap checks.
struct PaddedZero { unsigned char byte; unsigned int word; };
struct NullPayload { PaddedZero zero; int Owner::*member; };
union PaddedChoice { NullPayload payload; void *pointers[3]; };
PaddedChoice padded;
// PRE: @padded = {{.*}}global {{.*}}[8 x i8] zeroinitializer, i64 -1 }, [8 x i8] zeroinitializer }

// Detection scans every lowered alternative, even after nonzero-null storage
// selection and when the pointer-bearing alternative is nested or comes last.
union NestedMember { unsigned long bits; Member value; };
NestedMember nestedMember;

// Pointer leaves in lowered bases, vptrs and member-function representations
// must also be visible without a second walk over the source-level types.
struct PointerBase { void *pointer; };
struct Inherited : PointerBase {};
union BaseMember { unsigned long bits; Inherited value; };
BaseMember baseMember;

struct VirtualOwner { virtual ~VirtualOwner(); };
union VptrMember {
  unsigned long bits;
  VirtualOwner object;
  VptrMember() : bits(0) {}
  ~VptrMember() {}
};
VptrMember vptrMember;

union FunctionMember { unsigned long bits[2]; int (Owner::*method)(); };
FunctionMember functionMember;

// A plain pointer-only copy is still promotable. Normalized storage supplies
// the pointer type, even though this function never accesses a union member.
// PRE-LABEL: define {{.*}} @copy_hidden(
// PRE-NOT: alloca
// PRE-NOT: @llvm.memcpy
// PRE: load ptr, ptr %src, align 8
// PRE: store ptr {{.*}}, ptr %dest, align 8
extern "C" void copy_hidden(Hidden *dest, const Hidden *src) {
  Hidden temporary = *src;
  *dest = temporary;
}

int pointee = 42;
union PointerFirst { int *pointer; unsigned long integer; };
PointerFirst relocated = { &pointee };
// POST: @filc_constant_relocations = {{.*}}%filc_constant_relocation { i64 0, i32 0, ptr @pizlonated_pointee }

// Pre-created lifetime globals may receive differently typed null constants.
union Dynamic {
  int Owner::*member;
  void *pointer;
  explicit Dynamic(int);
};
extern int runtime_value();
const Dynamic &temporary = Dynamic(runtime_value());
