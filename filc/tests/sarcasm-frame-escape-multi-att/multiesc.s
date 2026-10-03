# D9 fixed-frame escape promotion, pinned for MULTIPLE escape clusters in ONE
# function. Two independent lea/copy clusters at different frame offsets
# (cluster A around region+16, cluster B around region+80) each hand a derived
# frame pointer to a different extern C helper (`call fillA ;! void(ptr)` /
# `call fillB ;! void(ptr)`), so the whole fixed frame is materialized as ONE
# GC allocation (filc_allocate in the sarcasm output — expected here) and BOTH
# clusters' traffic lands in that single home. Derived pointers from both
# clusters are live SIMULTANEOUSLY across both calls (parked in callee-saved
# registers, which is what lets them survive), each helper's writes are read
# back both directly and through the parked pointers, and everything is
# checksummed exactly (1375).
	.text
	.globl	multiesc
	.type	multiesc, @function
multiesc:                       #! long(long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	subq	$128, %rsp          # fixed frame [0,128): D0 = 160, region [0,128)

	# --- escape cluster A (offset 16) ---
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea (fillA below)
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$1, (%rdi)          # slot16 = 1
	movq	$2, (%rdx)          # slot24 = 2
	movq	%rdi, %r12          # park A's base derived pointer (call-safe)
	leaq	16(%r12), %r13      # r13 = region+32 (A derived)
	leaq	24(%r12), %rbx      # rbx = region+40 (A derived)

	# --- escape cluster B (offset 80) ---
	leaq	80(%rsp), %r8       # region+80 — the second ESCAPING lea
	movq	$3, (%r8)           # slot80 = 3
	movq	%r8, %r14           # park B's base derived pointer (call-safe)

	# --- hand A's pointer to fillA while B's pointer lives in r14 ---
	movq	%r12, %rdi
	call	fillA ;! void(ptr)  # slot16..slot72 = 100..107
	# --- hand B's pointer to fillB while A's pointers live in r12/r13/rbx ---
	movq	%r14, %rdi
	call	fillB ;! void(ptr)  # slot80..slot104 = 200..203

	# --- writes through the surviving derived pointers of BOTH clusters ---
	movq	$6, (%r12)          # slot16 = 6 (over the helper's 100)
	movq	$4, (%r13)          # slot32 = 4 (over the helper's 102)
	movq	$5, (%rbx)          # slot40 = 5 (over the helper's 103)
	movq	$7, 16(%r14)        # slot96 = 7 (over the helper's 202)

	# --- direct readback of every slot both clusters touched ---
	movq	16(%rsp), %rax      # 6
	addq	24(%rsp), %rax      # +101 = 107
	addq	32(%rsp), %rax      # +4 = 111
	addq	40(%rsp), %rax      # +5 = 116
	addq	48(%rsp), %rax      # +104 = 220
	addq	56(%rsp), %rax      # +105 = 325
	addq	64(%rsp), %rax      # +106 = 431
	addq	72(%rsp), %rax      # +107 = 538
	movq	80(%rsp), %rcx      # 200
	addq	88(%rsp), %rcx      # +201 = 401
	addq	96(%rsp), %rcx      # +7 = 408
	addq	104(%rsp), %rcx     # +203 = 611
	addq	%rcx, %rax          # 1149

	# --- one-home checks: the same slots through the parked pointers ---
	movq	(%r12), %rcx        # slot16 via A's base = 6
	addq	16(%r12), %rcx      # slot32 via A's base = 4
	addq	(%r13), %rcx        # slot32 via the copy = 4
	addq	(%rbx), %rcx        # slot40 via the offset copy = 5
	addq	(%r14), %rcx        # slot80 via B's base = 200
	addq	16(%r14), %rcx      # slot96 via B's base = 7
	addq	%rcx, %rax          # 1149 + 226 = 1375
	addq	$128, %rsp
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	multiesc, .-multiesc
	.section	.note.GNU-stack,"",@progbits
