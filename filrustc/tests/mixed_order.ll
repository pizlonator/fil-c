; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; The order-dependence shape: BOTH allocas below have the exact same use-set
; shape {memset [0,8), memcpy [0,8), memcpy [0,16)} -- only the order in which
; the uses appear in the IR differs, which changes the order in which SROA's
; partition slices get classified.  The old classification used a
; last-writer-wins "MemTransferOnly" boolean (every memtransfer set it true,
; every disqualifier -- including the memset -- set it false), so same-shaped
; partitions could take different paths depending on compile order -- and once
; the miscompiled variants exist, the widened integer memcpys drop the pointer
; capability flowing through them and main's dereference of the pointer loaded
; from @slotB trips "filc safety error: cannot read pointer with null object"
; (exit 133).  The gate is now a conjunction of monotone flags (SawMemTransfer
; && !SawDisqualifier && ...), so both partitions classify identically no
; matter the order, and the rewriter-level guard keeps every memintrinsic use
; whole on the re-typed alloca, so the capability survives either way.
;
; Scope note (what this test does and does not discriminate): at -O2 stash is
; inlined into main, and the DECISIVE SROA runs (on the inlined allocas) see
; the memsets already converted to `store i64 0` -- the surviving uses take the
; typed-mixed path ("Fil-C: keeping memtransfer; the access type i64" in the
; -mllvm -debug-only=sroa trace).  That path is closed by the rewriter-level
; guard alone, so the "ok 45 45" result does NOT by itself discriminate the old
; last-writer-wins predicate from the new conjunction: with the rewriter guard
; in place both classifications produce correct code for this shape.  The
; memset-in-the-classifier variant of the shape is covered by
; mixed_memset_memcpy.ll, and the conjunction's order-independence on this
; exact use-set shape was verified via -mllvm -debug-only=sroa traces (both IR
; orders classify their partitions identically in every SROA iteration).
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
; Expected output: "ok 45 45".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 45
@slotA = global ptr zeroinitializer
@slotB = global ptr zeroinitializer
@.fmt = private unnamed_addr constant [10 x i8] c"ok %d %d\0A\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare i32 @printf(ptr, ...)

define void @stash(ptr %from) {
entry:
  %a = alloca [16 x i8], align 8
  %b = alloca [16 x i8], align 8
  ; order A: memset first in IR (the order that made the old last-writer-wins
  ; predicate end up TRUE -- "keeping memcpys" -- despite the memset)
  call void @llvm.memset.p0.i64(ptr %a, i8 0, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr %a, ptr %from, i64 16, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr @slotA, ptr %a, i64 8, i1 false)
  ; order B: memset last in IR (the order that made it end up FALSE)
  call void @llvm.memcpy.p0.p0.i64(ptr %b, ptr %from, i64 16, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr @slotB, ptr %b, i64 8, i1 false)
  call void @llvm.memset.p0.i64(ptr %b, i8 0, i64 8, i1 false)
  ret void
}

define i32 @main() {
entry:
  %buf = alloca [16 x i8], align 8
  store ptr @pointee, ptr %buf
  call void @stash(ptr %buf)
  %pA = load ptr, ptr @slotA
  %vA = load i32, ptr %pA
  %pB = load ptr, ptr @slotB
  %vB = load i32, ptr %pB
  %ignored = call i32 (ptr, ...) @printf(ptr @.fmt, i32 %vA, i32 %vB)
  ret i32 0
}
