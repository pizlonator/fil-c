; SROA/GIMSO regression test (Fil-C .ll-level; see filrustc/run-tests for how
; these are compiled and why they must be .ll).
;
; Variant of sroa_memcpy_noalign.ll that exercises the alignment gate with
; annotations that are present but too weak: the memcpys DO carry alignment
; annotations, but sub-word ones (align 4).  An annotation only qualifies a
; memcpy piece for the pointer-word re-typing if it guarantees a whole Fil-C
; word (8 bytes) at the piece offset on both sides; align 4 does not.  Since
; alignment annotations are optional and may be any power of two in LLVM IR,
; SROA cannot assume that annotated means word-aligned.
;
; The shape is otherwise the same as sroa_memcpy_noalign.ll: the pointer word
; at offset 8 of %local is an 8-byte memcpy-only partition (the typed i64
; store at [0,8) splits it off; only the two memcpys touch it; the copies end
; at a global, so no typed pointer access exists along the chain).
;
; With an SROA that falls back to the integer re-typing for such partitions,
; this fails at -O2 with "filc safety error: cannot read pointer with null
; object" (exit 133).  With the fix, the partition is typed as an i8 array and
; the memcpys stay whole, so the pizlonator's memcpy lowering preserves the
; capability.
;
; Expected output: "ok 43".

target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

@pointee = global i32 43
@slot = global {i64, ptr} zeroinitializer
@.fmt = private unnamed_addr constant [7 x i8] c"ok %d\0A\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare i32 @printf(ptr, ...)

define void @stash(ptr %from) {
entry:
  %local = alloca [16 x i8], align 1
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %local, ptr align 4 %from, i64 16, i1 false)
  store i64 0, ptr %local
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 @slot, ptr align 4 %local, i64 16, i1 false)
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
