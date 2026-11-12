target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-ni:0-n8:16:32:64-S128"
target datalayout_after_filc = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Regression test for known-issue #13 (see build_rustc-notes.md): a pointer-
; maybe aggregate whose typed (all-pointer-struct) alloca receives a byte
; copy of a low-alignment buffer at a non-word-aligned offset, where the copy
; crosses the aggregate's pointer word.  This module mirrors the POST-FIX
; rustc IR: the Fil-C rustc now annotates every pointer-maybe aggregate
; alloca with a `zhas_union` call (clang's exact union-carrying-alloca escape,
; CodeGenFunction::CreateTempAlloca) and emits pointer-maybe copies as calls
; to the runtime's shadow-propagating `zmemmove_builtin` (clang's exact
; handling of memcpy/memmove builtins, CGBuiltin.cpp) instead of
; llvm.memcpy/memmove intrinsics.
;
; Why: with a plain llvm.memcpy on such an alloca, upstream SROA partitioned
; the typed alloca by its ptr fields and widened the crossing memcpy piece
; into `load ptr`/`store ptr` at the OTHER side's byte phase -- a 4-mod-8 or
; 2-mod-8 address.  That is legal IR for a generic target, but under GIMSO a
; pointer only rests in -- and is only accessed through -- a word-aligned
; slot, so the Fil-Pizlonator's inline word-alignment check died at run time
; ("alignment requirement of 8 bytes not met ... expected N writable bytes
; with ptr aligned to 8 bytes at offset 4") for perfectly valid source code
; (rustc's UnixListener::bind/TcpListener::accept at -O).  The zhas_union
; call is an opaque call use of the alloca pointer, so SROA (and every other
; pre-pizlonation optimization) leaves the alloca completely alone; the
; zmemmove_builtin calls are opaque to the optimizer and lowered by the
; pizlonator like any other memmove (payload + capability shadow together).
; The pizlonator erases zhas_union calls entirely and ignores them in its own
; escape analysis, so the annotation costs nothing at run time.
;
; The shape mirrors the rustc one: a 120-byte all-pointer-typed aggregate (the
; io::Result<(TcpStream, SocketAddr)> shape) whose Err variant's pointer word
; [8,16) is typed-accessed, plus a 110-byte align-2 sockaddr_un-shaped buffer
; copied into the payload at offset 4 -- a 4-mod-8 offset -- by a copy that
; crosses the [8,16) pointer word.

declare void @zmemmove_builtin(ptr, ptr, i64)
declare void @zhas_union(ptr)
declare i32 @printf(ptr, ...)

@ok_fmt = private unnamed_addr constant [4 x i8] c"ok\0A\00"
@bad_fmt = private unnamed_addr constant [8 x i8] c"bad %d\0A\00"

define void @make_result(ptr sret([120 x i8]) align 8 %out, i32 %want_err, ptr %err_val, ptr align 2 %sun) {
entry:
  %res = alloca <{ ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }>, align 8
  ; the Fil-C rustc's zhas_union annotation on the pointer-maybe aggregate's
  ; alloca (clang parity): makes the alloca an opaque escape for SROA.
  call void @zhas_union(ptr %res)
  %tagp = getelementptr i8, ptr %res, i64 0
  %payload = getelementptr i8, ptr %res, i64 4
  %errslot = getelementptr i8, ptr %res, i64 8
  store i32 0, ptr %tagp, align 4
  store ptr null, ptr %errslot, align 8
  %is_err = icmp ne i32 %want_err, 0
  br i1 %is_err, label %do_err, label %do_ok

do_err:
  store i32 1, ptr %tagp, align 4
  store ptr %err_val, ptr %errslot, align 8
  br label %copy_out

do_ok:
  ; 110 bytes of sockaddr_un (align 2) into the payload at offset 4 (4 mod 8);
  ; this copy crosses the ptr partition [8,16) of %res.  The Fil-C rustc
  ; emits pointer-maybe copies as zmemmove_builtin calls (clang parity), which
  ; no pre-pizlonation optimization can widen.
  call void @zmemmove_builtin(ptr align 4 %payload, ptr align 2 %sun, i64 110)
  br label %copy_out

copy_out:
  ; the by-value aggregate move of the pointer-maybe result is also a
  ; zmemmove_builtin call (payload + capability shadow move together).
  call void @zmemmove_builtin(ptr align 8 %out, ptr align 8 %res, i64 120)
  ret void
}

define i32 @main() {
entry:
  %outbuf = alloca [120 x i8], align 8
  %sun = alloca [110 x i8], align 2
  ; fill sun with byte values 1..110 (integer data - no real pointers)
  %fi = alloca i64, align 8
  store i64 0, ptr %fi, align 8
  br label %fill
fill:
  %fv = load i64, ptr %fi, align 8
  %flt = icmp ult i64 %fv, 110
  br i1 %flt, label %fbody, label %fdone
fbody:
  %fp = getelementptr i8, ptr %sun, i64 %fv
  %ftrunc = trunc i64 %fv to i8
  %fadd = add i8 %ftrunc, 1
  store i8 %fadd, ptr %fp, align 1
  %fv2 = add i64 %fv, 1
  store i64 %fv2, ptr %fi, align 8
  br label %fill
fdone:
  ; the ok path: the payload copy crosses the pointer word; before the fix
  ; (with a plain llvm.memcpy here) the crossing piece widened into a
  ; misaligned ptr access and panicked here.
  call void @make_result(ptr align 8 %outbuf, i32 0, ptr null, ptr align 2 %sun)
  ; the copied payload bytes [4,114) of outbuf must equal sun[0..110)
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 8
  br label %check
check:
  %v = load i64, ptr %i, align 8
  %vlt = icmp ult i64 %v, 110
  br i1 %vlt, label %cbody, label %cdone
cbody:
  %op4 = add i64 %v, 4
  %op = getelementptr i8, ptr %outbuf, i64 %op4
  %ob = load i8, ptr %op, align 1
  %sp = getelementptr i8, ptr %sun, i64 %v
  %sb = load i8, ptr %sp, align 1
  %eq = icmp eq i8 %ob, %sb
  br i1 %eq, label %cnext, label %cbad
cnext:
  %v2 = add i64 %v, 1
  store i64 %v2, ptr %i, align 8
  br label %check
cbad:
  %vz = trunc i64 %v to i32
  %badp = getelementptr inbounds [8 x i8], ptr @bad_fmt, i64 0, i64 0
  %pb = call i32 (ptr, ...) @printf(ptr %badp, i32 %vz)
  ret i32 1
cdone:
  ; also run the err path: the typed ptr store into the pointer word must
  ; keep working (the err pointer must come back out with its capability).
  %errslot2 = alloca ptr, align 8
  %marker = inttoptr i64 8899220416 to ptr
  call void @make_result(ptr align 8 %outbuf, i32 1, ptr %marker, ptr align 2 %sun)
  %ep = getelementptr i8, ptr %outbuf, i64 8
  %ev = load ptr, ptr %ep, align 8
  %ev_ok = icmp eq ptr %ev, %marker
  br i1 %ev_ok, label %print_ok, label %cbad2
cbad2:
  %badp2 = getelementptr inbounds [8 x i8], ptr @bad_fmt, i64 0, i64 0
  %pb2 = call i32 (ptr, ...) @printf(ptr %badp2, i32 111)
  ret i32 1
print_ok:
  %okp = getelementptr inbounds [4 x i8], ptr @ok_fmt, i64 0, i64 0
  %po = call i32 (ptr, ...) @printf(ptr %okp)
  ret i32 0
}
