; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; This is the MIXED typed/memcpy shape: one SROA partition contains BOTH a
; typed access (the `store i64 0`) AND memtransfer uses that move pointer
; bytes.  The typed store disqualifies the [0,8) partition from the
; memcpy-only pointer-word/i8-array re-typing, so (before the rewriter-level
; guard) SROA fell back to `alloca i64` and widened BOTH memcpys into integer
; load/store pairs.  Under GIMSO semantics integer accesses move the payload
; but not the capability shadow, so the pointer capability flowing through the
; memcpys was dropped: main's `load ptr` from @slot yielded a null-capability
; pointer and the dereference tripped "filc safety error: cannot read pointer
; with null object" (exit 133).  With the guard, a Fil-C memtransfer use whose
; emitted access type is not pointer-typed stays a real memcpy on the re-typed
; alloca (which is never promoted while it has memory-intrinsic uses), so the
; capability survives.  This is reachable by rustc, which feeds memtransfer
; intrinsics into SROA (unlike Fil-C clang, whose .ll tests this is).
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
; Expected output: "ok 42".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 42
@slot = global ptr zeroinitializer
@.fmt = private unnamed_addr constant [7 x i8] c"ok %d\0A\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare i32 @printf(ptr, ...)

define void @stash(ptr %from) {
entry:
  %local = alloca [16 x i8], align 8
  store i64 0, ptr %local
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
