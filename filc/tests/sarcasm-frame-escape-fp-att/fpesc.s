# D9 fixed-frame escape promotion, pinned for FP/SSE traffic into the
# promoted frame. The `leaq 96(%rsp), %rdi` handed to `call fpfill
# ;! void(ptr)` escapes, so the whole fixed frame is materialized as ONE GC
# allocation (filc_allocate in the sarcasm output — expected here) and BOTH
# the direct rsp-relative FP slots and the derived-pointer FP accesses ride
# that one home:
#   (a) direct slot write `movdqa %xmm0, 80(%rsp)` + direct readback;
#   (b) ONE-HOME check: direct 5s written at region+48, then 6s/9s written
#       over the same home THROUGH a derived pointer — every later read (via
#       the pointer and via the direct slots) must agree on the derived
#       values;
#   (c) partial-width `movsd` into the middle of that home must not disturb
#       the next slot (which an aligned movaps parked at region+64);
#   (d) aligned `movaps` through a derived pointer at a 16-aligned offset.
# Only SSE2 is used; lanes reach the checksum through movq/punpckhqdq and
# cvttsd2si. The checksum is exact (112) and mirrored in the C driver.
	.text
	.globl	fpesc
	.type	fpesc, @function
fpesc:                          #! long(long)
	pushq	%rbx
	pushq	%r12                # r12 = accumulator (rbx parks the derived ptr)
	subq	$128, %rsp          # fixed frame [0,128): D0 = 144, region [0,128)

	# --- escape cluster: the lea escapes to the helper, which promotes the
	# whole frame to one GC region ---
	leaq	96(%rsp), %rdi      # region+96 — the ESCAPING lea
	call	fpfill ;! void(ptr) # slot96 = bits(1.5), slot104 = bits(2.5)
	leaq	48(%rsp), %rbx      # region+48 derived pointer (call-safe park)
	xorl	%r12d, %r12d        # accumulator

	# --- (b) direct 5s, then derived 6s over the same home ---
	movq	$5, %rax
	movq	%rax, %xmm0
	movq	%rax, %xmm1
	punpcklqdq	%xmm1, %xmm0    # xmm0 = {5, 5}
	movdqa	%xmm0, 48(%rsp)     # direct: slot48 = 5, slot56 = 5
	movq	$6, %rax
	movq	%rax, %xmm1
	movq	$9, %rax
	movq	%rax, %xmm2
	punpcklqdq	%xmm2, %xmm1    # xmm1 = {6, 9}
	movaps	%xmm1, (%rbx)       # derived: slot48 = 6, slot56 = 9

	# --- (d) aligned movaps through the derived pointer (region+64) ---
	movq	$10, %rax
	movq	%rax, %xmm3
	movq	$11, %rax
	movq	%rax, %xmm4
	punpcklqdq	%xmm4, %xmm3    # xmm3 = {10, 11}
	movaps	%xmm3, 16(%rbx)     # slot64 = 10, slot72 = 11

	# --- (c) partial-width movsd into the home's second lane: slot56 = 9,
	# and slot64 (parked above) must survive untouched ---
	movsd	%xmm2, 8(%rbx)      # low 64 of xmm2 = 9 -> slot56 only

	# --- (a) direct slot write elsewhere in the region ---
	movdqa	%xmm0, 80(%rsp)     # slot80 = 5, slot88 = 5

	# --- readbacks; everything into r12 ---
	movq	48(%rsp), %rax      # direct GPR read: 6
	addq	56(%rsp), %rax      # +9 = 15
	addq	%rax, %r12
	movdqa	48(%rsp), %xmm5     # direct vector read of the derived home
	movq	%xmm5, %rax         # low lane: 6
	addq	%rax, %r12
	punpckhqdq	%xmm5, %xmm5    # high lane: 9
	movq	%xmm5, %rax
	addq	%rax, %r12          # 15 -> 30
	movaps	(%rbx), %xmm6       # the same home through the derived pointer
	movq	%xmm6, %rax         # 6
	addq	%rax, %r12
	punpckhqdq	%xmm6, %xmm6    # 9
	movq	%xmm6, %rax
	addq	%rax, %r12          # 15 -> 45
	movsd	8(%rbx), %xmm7      # partial-width read through the pointer: 9
	movq	%xmm7, %rax
	addq	%rax, %r12          # -> 54
	movaps	16(%rbx), %xmm6     # the movaps home through the pointer
	movq	%xmm6, %rax         # 10
	addq	%rax, %r12
	punpckhqdq	%xmm6, %xmm6    # 11
	movq	%xmm6, %rax
	addq	%rax, %r12          # 21 -> 75
	movq	64(%rsp), %rax      # slot64 direct: 10 (the movsd left it alone)
	addq	72(%rsp), %rax      # slot72 direct: 11
	addq	%rax, %r12          # 21 -> 96
	movdqa	80(%rsp), %xmm5     # (a) readback
	movq	%xmm5, %rax         # 5
	addq	%rax, %r12
	punpckhqdq	%xmm5, %xmm5    # 5
	movq	%xmm5, %rax
	addq	%rax, %r12          # 10 -> 106
	movsd	96(%rsp), %xmm0     # the helper's doubles, direct slots
	cvttsd2siq	%xmm0, %rax     # 1
	addq	%rax, %r12
	movsd	104(%rsp), %xmm1
	cvttsd2siq	%xmm1, %rax     # +2
	addq	%rax, %r12          # -> 109
	leaq	96(%rsp), %rdx      # the same doubles through a derived pointer
	movsd	(%rdx), %xmm0
	cvttsd2siq	%xmm0, %rax     # 1
	addq	%rax, %r12
	movsd	8(%rdx), %xmm1
	cvttsd2siq	%xmm1, %rax     # +2
	addq	%rax, %r12          # -> 112
	movq	%r12, %rax
	addq	$128, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	fpesc, .-fpesc
	.section	.note.GNU-stack,"",@progbits
