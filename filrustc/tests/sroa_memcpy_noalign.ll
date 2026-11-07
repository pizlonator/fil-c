; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; The pointer word at offset 8 of %local is an 8-byte memcpy-only SROA
; partition: the typed i64 store at [0,8) splits the buffer so that [8,16) is
; its own partition, and only the two memcpys touch [8,16).  The memcpys carry
; NO alignment annotations, and none can be inferred (%local has align 1 and
; %from has unknown alignment), so the pieces that cover the partition have no
; alignment to speak of.  The copies end at a global (@slot), so no typed
; pointer access exists anywhere along the chain.
;
; SROA must not re-type that partition as an integer: the integer load/store
; pairs it would emit drop the pointer's capability under GIMSO semantics
; (gimso_semantics.md:61), and main's `load ptr` from @slot would then trip
; "filc safety error: cannot read pointer with null object" (exit 133).  With
; the fix, a Fil-C memcpy-only partition that fails the pointer-word gates
; (here: the memcpys are unaligned) is typed as an i8 array instead, which is
; not a single value type, so the rewriter leaves the memcpys whole and the
; pizlonator's memcpy lowering preserves the capability.
;
; Compiling this module with an SROA that falls back to the integer re-typing
; fails at -O2 with exit 133; the -O build of the same IR with `align 8` on
; both memcpys passes (the aligned pointer-word re-typing path handles it).
;
; Expected output: "ok 42".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 42
@slot = global {i64, ptr} zeroinitializer
@.fmt = private unnamed_addr constant [7 x i8] c"ok %d\0A\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare i32 @printf(ptr, ...)

define void @stash(ptr %from) {
entry:
  %local = alloca [16 x i8], align 1
  call void @llvm.memcpy.p0.p0.i64(ptr %local, ptr %from, i64 16, i1 false)
  store i64 0, ptr %local
  call void @llvm.memcpy.p0.p0.i64(ptr @slot, ptr %local, i64 16, i1 false)
  ret void
}

define i32 @main() {
entry:
  %buf = alloca [16 x i8], align 8
  %pg = getelementptr inbounds [16 x i8], ptr %buf, i64 0, i64 8
  store ptr @pointee, ptr %pg
  call void @stash(ptr %buf)
  %slotptr = getelementptr inbounds {i64, ptr}, ptr @slot, i64 0, i32 1
  %p = load ptr, ptr %slotptr
  %v = load i32, ptr %p
  %ignored = call i32 (ptr, ...) @printf(ptr @.fmt, i32 %v)
  ret i32 0
}
