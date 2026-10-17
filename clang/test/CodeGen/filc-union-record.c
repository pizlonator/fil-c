// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -emit-llvm -o - %s | FileCheck %s

// A scalar union occupying one pointer word keeps its capability-bearing direct
// signature. In particular, it must not acquire a byval argument, which would
// force a generic Fil-C call and heap promotion even for this tiny record.
union Scalar { long integer; double number; void *pointer; };
struct First { union Scalar value; long tag; };
struct Last { long tag; union Scalar value; };

// CHECK-LABEL: define {{.*}} @pizlonatedFIP{{[1-9][0-9]*}}_first(
// CHECK-NOT: @filc_promote_already_checked_stack_to_heap_without_exiting(
// CHECK: ret
struct First first(struct First value) { return value; }

// CHECK-LABEL: define {{.*}} @pizlonatedFIP{{[1-9][0-9]*}}_last(
// CHECK-NOT: @filc_promote_already_checked_stack_to_heap_without_exiting(
// CHECK: ret
struct Last last(struct Last value) { return value; }

// Aggregate alternatives can hide pointers in LLVM storage. Normalization
// exposes every full word in a pointer-bearing union to the ABI.
struct Hidden { union { long integer; void *pointers[1]; } value; long tag; };
struct Wide { union { void *pointer; void *pointers[2]; } value; };
struct Large { union { void *pointer; __int128 integer; } value; };

// CHECK-LABEL: define { i1, %filc_flight_ptr, i64 } @pizlonatedFIP{{[1-9][0-9]*}}_hidden(
// CHECK-SAME: %filc_flight_ptr {{.*}}, i64
// CHECK-NOT: @filc_promote_already_checked_stack_to_heap_without_exiting(
// CHECK: ret
struct Hidden hidden(struct Hidden value) { return value; }

// CHECK-LABEL: define { i1, %filc_flight_ptr, %filc_flight_ptr } @pizlonatedFIP{{[1-9][0-9]*}}_wide(
// CHECK-SAME: %filc_flight_ptr {{.*}}, %filc_flight_ptr
// CHECK-NOT: @filc_promote_already_checked_stack_to_heap_without_exiting(
// CHECK: ret
struct Wide wide(struct Wide value) { return value; }

// CHECK-LABEL: define { i1, %filc_flight_ptr, %filc_flight_ptr } @pizlonatedFIP{{[1-9][0-9]*}}_large(
// CHECK-NOT: @filc_promote_already_checked_stack_to_heap_without_exiting(
// CHECK: ret
struct Large large(struct Large value) { return value; }

union LastWord { long bits[2]; struct { long tag; void *pointer; } value; };
// CHECK-LABEL: define { i1, %filc_flight_ptr, %filc_flight_ptr } @pizlonatedFIP{{[1-9][0-9]*}}_last_word(
// CHECK-SAME: %filc_flight_ptr {{.*}}, %filc_flight_ptr
union LastWord last_word(union LastWord value) { return value; }

union Mixed { struct { void *pointer; double number; } value; double numbers[2]; };
// CHECK-LABEL: define { i1, %filc_flight_ptr, double } @pizlonatedFIP{{[1-9][0-9]*}}_mixed(
// CHECK-SAME: %filc_flight_ptr {{.*}}, double
union Mixed mixed(union Mixed value) { return value; }

union Numeric { long integer; double number; };
// CHECK-LABEL: define { i1, i64 } @pizlonatedFIP{{[1-9][0-9]*}}_numeric(
// CHECK-SAME: i64
union Numeric numeric(union Numeric value) { return value; }

union Oversized { void *pointer; long words[3]; };
// CHECK-LABEL: define {{.*}} @pizlonatedFIP0_oversized(
// CHECK: call {{.*}} @filc_promote_already_checked_stack_to_heap_without_exiting(
union Oversized oversized(union Oversized value) { return value; }
