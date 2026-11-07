; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; The memset-plus-pointer-vector shape: the [0,16) partition of %local in
; @stash is covered by a memset AND by a typed <2 x ptr> load.  The partition
; has a typed access, so the partition-level memcpy-only machinery does not
; re-type it, and the slice type is the use's own <2 x ptr>.  An earlier
; version of the rewriter's memset guard only fired for slices that were not
; pointer-typed -- <2 x ptr> IS a pointer vector, so it did not fire -- and the
; memset then fell through to the VecTy/IntTy fast-path return in
; visitMemSetInst's CanContinue, which returns true without consulting
; canConvertValue.  The widening then built an integer splat for a
; vector-of-pointer slice, and convertValue's assertion fired: under the -ni:0
; datalayout an integer does not convert to a non-integral pointer, so this
; legal IR aborted the compiler with
;
;   clang-20: llvm/lib/Transforms/Scalar/SROA.cpp:2058: llvm::Value*
;     convertValue(const llvm::DataLayout&, {anonymous}::IRBuilderTy&,
;     llvm::Value*, llvm::Type*): Assertion `canConvertValue(DL, OldTy, NewTy)
;     && "Value not convertable to type"' failed.
;
; Under Fil-C a memset use is now ALWAYS kept whole (never widened into a typed
; store, for any slice type): GIMSO semantics make a memset zero both the
; payload and the shadow, and no typed store can express that -- an integer
; store leaves stale shadow behind, and a pointer/pointer-vector store cannot
; represent the integer splat under the -ni:0 datalayout at all
; (canConvertValue refuses it).  The pizlonator's filc_memset lowering zeroes
; payload and shadow together, which is exactly the fast path Fil-C always
; intended memsets to take.
;
; Post-fix the rewriter keeps the memset on the re-typed alloca, the (dead)
; <2 x ptr> load is dropped with the rest of the partition's typed uses, and a
; later SROA round re-types the now memset-only alloca as an integer.  The
; pizlonator therefore never sees a pointer vector -- which it could not lower
; anyway (toFlightTypeImpl refuses vectors of pointers), which is also why the
; vector load here is dead: a live pointer-vector access would abort
; pizlonation on legal IR regardless of SROA, so the kept-memset semantics are
; instead observed through @main's memset partition with typed i64 accesses
; (the bytes only the memset wrote must still read back as zero).
;
; .ll format requirements: `target datalayout` must be the pre-FilC datalayout
; including -ni:0 (what SROA and the rest of the pipeline see -- canConvertValue
; and the ptr-words machinery depend on pointers being non-integral), and
; `target datalayout_after_filc` is MANDATORY: clang asserts in
; MachineFunction::init() without it.
;
; Expected output: "ok 46".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 46
@.fmt = private unnamed_addr constant [7 x i8] c"ok %d\0A\00"
@.fmtbad = private unnamed_addr constant [13 x i8] c"bad %ld %ld\0A\00"

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare i32 @printf(ptr, ...)

; The regression shape: the [0,16) partition of %local is covered by a memset
; and by a typed <2 x ptr> load (the load is dead -- its value is never used --
; which is what lets the module survive pizlonation post-fix; see the header
; comment).
define void @stash(ptr %from) {
entry:
  %local = alloca [16 x i8], align 16
  call void @llvm.memset.p0.i64(ptr %local, i8 0, i64 16, i1 false)
  %v = load <2 x ptr>, ptr %local
  ret void
}

; Observable half: a memset partition with typed i64 accesses.  The memset
; must be kept whole on the re-typed alloca, so the bytes it zeroed (which no
; typed store overwrites) still read back as zero.
define i32 @main() {
entry:
  call void @stash(ptr @pointee)
  %buf = alloca [16 x i8], align 16
  call void @llvm.memset.p0.i64(ptr %buf, i8 0, i64 16, i1 false)
  %hi = getelementptr inbounds [16 x i8], ptr %buf, i64 0, i64 8
  store i64 46, ptr %hi
  %lo = load i64, ptr %buf
  %hv = load i64, ptr %hi
  %zeroed = icmp eq i64 %lo, 0
  br i1 %zeroed, label %good, label %bad

bad:
  %ignored0 = call i32 (ptr, ...) @printf(ptr @.fmtbad, i64 %lo, i64 %hv)
  ret i32 1

good:
  %hv32 = trunc i64 %hv to i32
  %ignored1 = call i32 (ptr, ...) @printf(ptr @.fmt, i32 %hv32)
  ret i32 0
}
