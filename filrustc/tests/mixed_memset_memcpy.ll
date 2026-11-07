; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; The memset variant of mixed_typed_memcpy.ll: the typed access that splits and
; disqualifies the [0,8) partition is a `llvm.memset` instead of a `store i64`.
; A memset-mixed partition used to fall back to the integer re-typing (the
; memset is not a memtransfer, so the old "memcpy-only" predicate missed it)
; and then widened BOTH the memcpys and the memset into integer accesses --
; the widened memcpy dropped the pointer capability flowing through it, and
; main's dereference of the pointer loaded from @slot tripped "filc safety
; error: cannot read pointer with null object" (exit 133).  The notes used to
; claim that memset-mixed partitions keeping the integer path was "benign";
; that was FALSE whenever pointer bytes flow through the memcpys.  With the
; rewriter-level guard, a Fil-C memset (or memtransfer) use whose emitted
; access type is not pointer-typed stays a real memory intrinsic on the
; re-typed alloca (which is never promoted while it has memory-intrinsic
; uses), so the capability survives.
;
; The -O0 build of the same IR passes (no SROA); only -O2 miscompiled before
; the fix.
;
; .ll format requirements: `target datalayout` must be the pre-FilC datalayout
; including -ni:0 (what SROA and the rest of the pipeline see -- canConvertValue
; and the ptr-words machinery depend on pointers being non-integral), and
; `target datalayout_after_filc` is MANDATORY: clang asserts in
; MachineFunction::init() without it.
;
; Expected output: "ok 44".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 44
@slot = global ptr zeroinitializer
@.fmt = private unnamed_addr constant [7 x i8] c"ok %d\0A\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare i32 @printf(ptr, ...)

define void @stash(ptr %from) {
entry:
  %local = alloca [16 x i8], align 8
  call void @llvm.memset.p0.i64(ptr %local, i8 0, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr %local, ptr %from, i64 16, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr @slot, ptr %local, i64 8, i1 false)
  ret void
}

define i32 @main() {
entry:
  %buf = alloca [16 x i8], align 8
  store ptr @pointee, ptr %buf
  call void @stash(ptr %buf)
  %p = load ptr, ptr @slot
  %v = load i32, ptr %p
  %ignored = call i32 (ptr, ...) @printf(ptr @.fmt, i32 %v)
  ret i32 0
}
