# Prologue `and $-1024, %rsp` with live GC roots and an aligned cluster: the
# raw note max is 1024 but the widest materialized cluster needs only 32, so
# the EFFECTIVE alignment is 32 — and both the frameSlot root-area shift
# (transform.luau) and the layout pad (x86_64_render.computeLayout) must use
# that same effective width. Verified against the emitted .yolo.s: the
# prologue is `subq $736, %rsp; andq $-32, %rsp` with the vmovdqa cluster at
# 576..672(%rsp), inside the frame. With the old raw-width shift the same
# cluster landed at 1568..1664(%rsp) — ~992 bytes past the frame end, silently
# corrupting adjacent stack (self-consistent reads/writes, so no data check
# can catch it). Two entry pointer
# arguments keep GC roots live across the traffic; the heap round trip through
# those roots plus distinct per-slot patterns prove roots and aligned accesses
# both survived. Misaligned output would also raise #GP on the vmovdqa itself
# (an uncatchable fault — the test fails).
#
# Vacuous-proof against the raw-width bug at RUNTIME (not just by reading the
# .yolo.s): the C caller wraps every call in a swept stack-depth plus a
# canary wall (see and1024-main.c). Each of 128 C-recursion levels owns a
# 256-byte wall segment in its own frame, directly above the call's entry
# rsp — `volatile` with its address escaped through an asm barrier, so the
# optimizer cannot delete, reorder, or promote it past the call — walking the entry across ~16KB of stack depths (every 32-byte residue
# class mod 1024 many times over). A raw-1024 shift rounds with up to 1023
# bytes of slack instead of the effective 0..31: across the full period many
# levels necessarily land raw pads that throw the ~96-byte cluster ~800-1000
# bytes past the frame end, across their own segments (both layouts are
# 32-aligned and self-consistent, so no data check and no #GP can see the
# difference — the cluster is aligned, just outside the frame). Detection
# was validated empirically: a synthetic reproduction of the historical bug
# (raw root-area shift under an effective-32 frame) makes this test print
# BAD, while the fixed emission prints ok. (Reading the aligned rsp itself
# back into C is not an option: the frame pass rejects any rsp-as-value
# read as a frame-address escape.)
	.text
	.globl	and1024_test
	.type	and1024_test, @function
and1024_test:                   ;! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	subq	$256, %rsp
	and	$-1024, %rsp
	movq	%rdi, %rbx
	movq	%rsi, %r12
	movq	%rdx, %r13
	vmovdqu	(%rbx), %ymm0
	vmovdqa	%ymm0, 0(%rsp)
	vmovdqu	(%r12), %ymm0
	vmovdqa	%ymm0, 32(%rsp)
	leaq	-128(%rsp), %rax
	vmovdqa	0(%rsp), %ymm1
	vmovdqa	%ymm1, 192(%rax)
	vmovdqa	32(%rsp), %ymm2
	vmovdqa	%ymm2, 224(%rax)
	vmovdqa	192(%rax), %ymm3
	vmovdqu	%ymm3, (%r12)
	vmovdqa	224(%rax), %ymm4
	vmovdqu	%ymm4, (%rbx)
	xorl	%eax, %eax
	addq	$256, %rsp
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	and1024_test, .-and1024_test
	.section	.note.GNU-stack,"",@progbits
