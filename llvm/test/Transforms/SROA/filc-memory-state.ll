; RUN: opt -passes='sroa,sroa' -S < %s | FileCheck %s
; RUN: opt -passes='sroa,instcombine,sroa,instcombine' -S < %s | FileCheck %s
;
; Fil-C memtransfers transport/clear shadow state. Numeric observations are
; not permission to replace them with numeric loads/stores. Conversely, dense
; aligned pointer-word copies should still promote without a union marker.
target datalayout = "e-m:e-ni:0-i64:64-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-i64:64-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)
declare void @observe(i64)

; CHECK-LABEL: define void @pointer_copy(
; CHECK-NOT: alloca
; CHECK-NOT: @llvm.memcpy
; CHECK: load ptr, ptr %src, align 8
; CHECK: store ptr {{.*}}, ptr %dest, align 8
define void @pointer_copy(ptr %dest, ptr %src) {
  %tmp = alloca { [1 x ptr] }, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %tmp, ptr align 8 %src, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %dest, ptr align 8 %tmp, i64 8, i1 false)
  ret void
}

; CHECK-LABEL: define void @observed_copy(
; CHECK: alloca { [1 x ptr] }, align 8
; CHECK: call void @llvm.memcpy
; CHECK: load i64
; CHECK: call void @observe
; CHECK: call void @llvm.memcpy
define void @observed_copy(ptr %dest, ptr %src) {
  %tmp = alloca { [1 x ptr] }, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %tmp, ptr align 8 %src, i64 8, i1 false)
  %bits = load i64, ptr %tmp, align 8
  call void @observe(i64 %bits)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %dest, ptr align 8 %tmp, i64 8, i1 false)
  ret void
}

; Byte/integer storage can carry capabilities despite having no pointer leaf.
; CHECK-LABEL: define void @byte_storage(
; CHECK: alloca [8 x i8], align 8
; CHECK: call void @llvm.memcpy
; CHECK: call void @observe
; CHECK: call void @llvm.memcpy
define void @byte_storage(ptr %dest, ptr %src) {
  %tmp = alloca [8 x i8], align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %tmp, ptr align 8 %src, i64 8, i1 false)
  %bits = load i64, ptr %tmp, align 8
  call void @observe(i64 %bits)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %dest, ptr align 8 %tmp, i64 8, i1 false)
  ret void
}

; Both allocations/worklist orders must preserve numeric observations and
; copied capabilities. Repeated SROA must not integerize a requeued allocation.
; CHECK-LABEL: define void @copy_chain(
; CHECK: load ptr, ptr %src, align 8
; CHECK: store ptr {{.*}}, ptr %second, align 8
; CHECK: call void @observe
; CHECK: call void @llvm.memcpy
define void @copy_chain(ptr %dest, ptr %src) {
  %second = alloca ptr, align 8
  %first = alloca ptr, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %first, ptr align 8 %src, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %second, ptr align 8 %first, i64 8, i1 false)
  %bits = load i64, ptr %second, align 8
  call void @observe(i64 %bits)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %dest, ptr align 8 %second, i64 8, i1 false)
  ret void
}

; A short transfer may clear the destination word's capability even though
; the source contains no capability. Neither SROA nor InstCombine may turn
; this into an i8 store, which leaves shadow state untouched in Fil-C.
; CHECK-LABEL: define void @short_copy(
; CHECK: alloca i8
; CHECK: call void @llvm.memcpy{{.*}}i64 1
define void @short_copy(ptr %dest, i8 %byte) {
  %tmp = alloca i8, align 1
  store i8 %byte, ptr %tmp
  call void @llvm.memcpy.p0.p0.i64(ptr %dest, ptr %tmp, i64 1, i1 false)
  ret void
}

; CHECK-LABEL: define ptr @clearing_memset(
; CHECK: alloca ptr, align 8
; CHECK: store ptr %value
; CHECK: call void @llvm.memset{{.*}}i64 1
; CHECK: load ptr
define ptr @clearing_memset(ptr %value) {
  %tmp = alloca ptr, align 8
  store ptr %value, ptr %tmp, align 8
  call void @llvm.memset.p0.i64(ptr align 8 %tmp, i8 0, i64 1, i1 false)
  %result = load ptr, ptr %tmp, align 8
  ret ptr %result
}

; Same source/destination is not permission to erase a partial transfer.
; CHECK-LABEL: define void @self_copy_byte(
; CHECK: call void @llvm.memmove{{.*}}ptr {{.*}}%slot, ptr {{.*}}%slot, i64 1
define void @self_copy_byte(ptr %slot) {
  call void @llvm.memmove.p0.p0.i64(ptr %slot, ptr %slot, i64 1, i1 false)
  ret void
}

; CHECK-LABEL: define void @unknown_length(
; CHECK: alloca ptr, align 8
; CHECK: call void @llvm.memcpy{{.*}}i64 %length
define void @unknown_length(ptr %dest, ptr %value, i64 %length) {
  %tmp = alloca ptr, align 8
  store ptr %value, ptr %tmp, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %dest, ptr align 8 %tmp, i64 %length, i1 false)
  ret void
}

; A weakly aligned external endpoint cannot be promoted into a pointer load.
; CHECK-LABEL: define ptr @weak_endpoint(
; CHECK: alloca ptr, align 8
; CHECK: call void @llvm.memcpy{{.*}}align 1 {{.*}}%src
define ptr @weak_endpoint(ptr %src) {
  %tmp = alloca ptr, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %tmp, ptr align 1 %src, i64 8, i1 false)
  %result = load ptr, ptr %tmp, align 8
  ret ptr %result
}
