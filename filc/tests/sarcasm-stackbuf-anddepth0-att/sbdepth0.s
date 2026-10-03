# `#! stack buffer (...)` in a DEPTH-0 `and $-32, %rsp` frame, GPR traffic: the
# rsaz-avx2/curve25519-donna prologue idiom `mov %rsp,%r11; and $-32,%rsp;
# subq $N,%rsp` — the and note's rsp depth is 0. The stack-buffer reach gate
# used `frameAndDepth ~= 0` as its "there is an and note" condition, and a
# depth-0 note (frameAndDepth==0) fell into the "no note" sentinel: the gate
# then demanded every buffer home to sit below the push pad, which is empty
# here, so ANY buffer declared over this frame's post-and traffic was
# compile-rejected ("stack buffer (...) extends to normalized offset ..., past
# the frame's usable top"). The gate now keys on frameAndSeen, so the buffer
# lowers into the synthesized frame like in any other and frame. The buffer's
# accesses here are all GPR (scalar integer loads/stores), which carry no
# alignment promise, so the buffer group is placed modulo 1 and the and's
# dynamic slack is irrelevant to it (see sarcasm-stackbuf-andframe-att); the
# and itself is dropped and the epilogue's `mov %r11, %rsp` restore is
# rewritten into a plain frame teardown.
#
# Vacuous-proof: the indexed buffer accesses carry runtime bounds checks (the
# emitted .yolo.s compares the index against 61 for the 4-byte byte-index form
# and 16 for the scale-4 form) that panic on any overflow, the four live
# constants read back byte-exact through two different indexings of the same
# lowered region, and the C caller sweeps 64 recursion levels of verified
# canary wall.
	.text
	.globl	sbdepth0_test
	.type	sbdepth0_test, @function
sbdepth0_test:                  ;! long(size_t, size_t)
	mov	%rsp, %r11
	andq	$-32, %rsp
	subq	$256, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
	movl	$0x33333333, 8(%rsp)
	movl	$0x44444444, 12(%rsp)
	movl	$0x55555555, 60(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (kh, %rsp, %rsp + 64)
	movl	(%rsp,%rsi,4), %ecx #! stack buffer (kh, %rsp, %rsp + 64)
	addl	%ecx, %eax
	movq	%r11, %rsp
	ret
	.size	sbdepth0_test, .-sbdepth0_test
	.section	.note.GNU-stack,"",@progbits
