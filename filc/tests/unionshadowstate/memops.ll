; Deliberately use LLVM intrinsics, not the already-opaque C memcpy builtin.
; These are the operations produced by aggregate copies and initialization.
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)

define void @copy_byte(ptr %dest, i8 %byte) {
  %tmp = alloca i8, align 1
  store i8 %byte, ptr %tmp
  call void @llvm.memcpy.p0.p0.i64(ptr %dest, ptr %tmp, i64 1, i1 false)
  ret void
}

define ptr @clear_byte(ptr %value) {
  %tmp = alloca ptr, align 8
  store ptr %value, ptr %tmp, align 8
  call void @llvm.memset.p0.i64(ptr align 8 %tmp, i8 0, i64 1, i1 false)
  %result = load ptr, ptr %tmp, align 8
  ret ptr %result
}

; The payload is unchanged, but a partial self-copy still clears shadow state.
define void @self_copy_byte(ptr %slot) {
  call void @llvm.memmove.p0.p0.i64(ptr %slot, ptr %slot, i64 1, i1 false)
  ret void
}
